using System.Collections.Concurrent;
using System.Text.Json;
using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Storage;

/// <summary>本地开发与自动化测试存储；生产环境不得使用。</summary>
public sealed class InMemoryLobbyStore : ILobbyStore
{
    private readonly ConcurrentDictionary<string, LobbyRoom> roomsByCode = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, string> codeById = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, object> roomLocks = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, string> matchResults = new(StringComparer.Ordinal);

    public Task InitializeAsync(CancellationToken cancellationToken) => Task.CompletedTask;

    public Task<bool> TryCreateRoomAsync(LobbyRoom room, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (!roomsByCode.TryAdd(room.RoomCode, room))
        {
            return Task.FromResult(false);
        }

        if (!codeById.TryAdd(room.RoomId, room.RoomCode))
        {
            roomsByCode.TryRemove(room.RoomCode, out _);
            return Task.FromResult(false);
        }

        return Task.FromResult(true);
    }

    public Task<LobbyRoom?> GetRoomByCodeAsync(string roomCode, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        roomsByCode.TryGetValue(roomCode, out var room);
        return Task.FromResult(room);
    }

    public Task<LobbyRoom?> GetRoomByIdAsync(string roomId, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (codeById.TryGetValue(roomId, out var code) && roomsByCode.TryGetValue(code, out var room))
        {
            return Task.FromResult<LobbyRoom?>(room);
        }
        return Task.FromResult<LobbyRoom?>(null);
    }

    public Task<LobbyRoom?> GetActiveRoomByPlayerAsync(string playerId, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var room = roomsByCode.Values
            .Where(candidate => candidate.Lifecycle is RoomLifecycle.Allocating or RoomLifecycle.Waiting
                or RoomLifecycle.Playing or RoomLifecycle.Settling)
            .Where(candidate => candidate.PlayerIds.Contains(playerId, StringComparer.Ordinal))
            .OrderByDescending(candidate => candidate.UpdatedAtUtc)
            .FirstOrDefault();
        return Task.FromResult(room);
    }

    public Task<IReadOnlyList<LobbyRoom>> ListPublicRoomsAsync(CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        IReadOnlyList<LobbyRoom> rooms = roomsByCode.Values
            .Where(room => room.PublicRoom && room.Lifecycle is RoomLifecycle.Allocating or RoomLifecycle.Waiting)
            .OrderByDescending(room => room.CreatedAtUtc)
            .Take(100)
            .ToArray();
        return Task.FromResult(rooms);
    }

    public Task<AddPlayerResult> TryAddPlayerAsync(
        string roomCode, string playerId, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var gate = roomLocks.GetOrAdd(roomCode, _ => new object());
        lock (gate)
        {
            if (!roomsByCode.TryGetValue(roomCode, out var room))
            {
                return Task.FromResult(new AddPlayerResult(AddPlayerStatus.RoomNotFound, null));
            }
            if (room.PlayerIds.Contains(playerId, StringComparer.Ordinal))
            {
                return Task.FromResult(new AddPlayerResult(AddPlayerStatus.AlreadyMember, room));
            }
            if (room.Lifecycle is RoomLifecycle.Closed or RoomLifecycle.Failed or RoomLifecycle.Playing or RoomLifecycle.Settling)
            {
                return Task.FromResult(new AddPlayerResult(AddPlayerStatus.RoomClosed, room));
            }
            if (room.PlayerIds.Length >= room.MaximumPlayers)
            {
                return Task.FromResult(new AddPlayerResult(AddPlayerStatus.RoomFull, room));
            }

            var updated = room with
            {
                PlayerIds = [.. room.PlayerIds, playerId],
                StateSequence = room.StateSequence + 1,
                UpdatedAtUtc = DateTimeOffset.UtcNow
            };
            roomsByCode[roomCode] = updated;
            return Task.FromResult(new AddPlayerResult(AddPlayerStatus.Added, updated));
        }
    }

    public Task<bool> UpdateRoomAsync(LobbyRoom room, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var gate = roomLocks.GetOrAdd(room.RoomCode, _ => new object());
        lock (gate)
        {
            if (!roomsByCode.TryGetValue(room.RoomCode, out var current) || room.StateSequence < current.StateSequence)
            {
                return Task.FromResult(false);
            }
            roomsByCode[room.RoomCode] = room;
            codeById[room.RoomId] = room.RoomCode;
            return Task.FromResult(true);
        }
    }

    public Task<FinalizeMatchStatus> FinalizeMatchAsync(
        LobbyRoom closedRoom, MatchResultReport report, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var resultKey = $"{closedRoom.MatchId}:{report.ResultSequence}";
        var payload = JsonSerializer.Serialize(report);
        var gate = roomLocks.GetOrAdd(closedRoom.RoomCode, _ => new object());
        lock (gate)
        {
            if (matchResults.TryGetValue(resultKey, out var existing))
            {
                return Task.FromResult(existing == payload
                    ? FinalizeMatchStatus.Duplicate
                    : FinalizeMatchStatus.Conflict);
            }
            if (!roomsByCode.TryGetValue(closedRoom.RoomCode, out var current)
                || current.MatchId != closedRoom.MatchId
                || current.StateSequence >= closedRoom.StateSequence)
            {
                return Task.FromResult(FinalizeMatchStatus.Conflict);
            }
            matchResults[resultKey] = payload;
            roomsByCode[closedRoom.RoomCode] = closedRoom;
            codeById[closedRoom.RoomId] = closedRoom.RoomCode;
            return Task.FromResult(FinalizeMatchStatus.Accepted);
        }
    }
}
