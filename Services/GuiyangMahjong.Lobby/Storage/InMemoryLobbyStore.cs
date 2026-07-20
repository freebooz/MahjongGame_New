using System.Collections.Concurrent;
using System.Text.Json;
using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Storage;

/// <summary>本地开发与自动化测试存储；生产环境不得使用。</summary>
public sealed class InMemoryLobbyStore : ILobbyStore
{
    private readonly ConcurrentDictionary<string, LobbyRoom> roomsByCode = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, string> codeById = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, string> matchResults = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string> activeRoomByPlayer = new(StringComparer.Ordinal);
    private readonly object mutationGate = new();

    public Task InitializeAsync(CancellationToken cancellationToken) => Task.CompletedTask;
    public Task<bool> CheckHealthAsync(CancellationToken cancellationToken) => Task.FromResult(true);

    public Task<CreateRoomResult> TryCreateRoomAsync(LobbyRoom room, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        lock (mutationGate)
        {
            if (roomsByCode.ContainsKey(room.RoomCode) || codeById.ContainsKey(room.RoomId))
            {
                return Task.FromResult(new CreateRoomResult(CreateRoomStatus.RoomCodeConflict));
            }

            if (IsActive(room.Lifecycle))
            {
                var conflictingPlayer = room.PlayerIds.FirstOrDefault(activeRoomByPlayer.ContainsKey);
                if (conflictingPlayer is not null)
                {
                    return Task.FromResult(new CreateRoomResult(
                        CreateRoomStatus.PlayerAlreadyActive,
                        GetRoomByIdUnsafe(activeRoomByPlayer[conflictingPlayer])));
                }
            }

            roomsByCode[room.RoomCode] = room;
            codeById[room.RoomId] = room.RoomCode;
            SynchronizeActivePlayersUnsafe(room);
            return Task.FromResult(new CreateRoomResult(CreateRoomStatus.Created));
        }
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
        lock (mutationGate)
        {
            return Task.FromResult(activeRoomByPlayer.TryGetValue(playerId, out var roomId)
                ? GetRoomByIdUnsafe(roomId)
                : null);
        }
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
        lock (mutationGate)
        {
            if (!roomsByCode.TryGetValue(roomCode, out var room))
            {
                return Task.FromResult(new AddPlayerResult(AddPlayerStatus.RoomNotFound, null));
            }
            if (room.PlayerIds.Contains(playerId, StringComparer.Ordinal))
            {
                return Task.FromResult(new AddPlayerResult(AddPlayerStatus.AlreadyMember, room));
            }
            if (activeRoomByPlayer.TryGetValue(playerId, out var activeRoomId)
                && activeRoomId != room.RoomId)
            {
                return Task.FromResult(new AddPlayerResult(
                    AddPlayerStatus.AlreadyInAnotherRoom,
                    GetRoomByIdUnsafe(activeRoomId)));
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
            activeRoomByPlayer[playerId] = updated.RoomId;
            return Task.FromResult(new AddPlayerResult(AddPlayerStatus.Added, updated));
        }
    }

    public Task<bool> UpdateRoomAsync(LobbyRoom room, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        lock (mutationGate)
        {
            if (!roomsByCode.TryGetValue(room.RoomCode, out var current)
                || room.StateSequence != current.StateSequence + 1
                || HasActivePlayerConflictUnsafe(room))
            {
                return Task.FromResult(false);
            }
            roomsByCode[room.RoomCode] = room;
            codeById[room.RoomId] = room.RoomCode;
            SynchronizeActivePlayersUnsafe(room);
            return Task.FromResult(true);
        }
    }

    public Task<FinalizeMatchStatus> FinalizeMatchAsync(
        LobbyRoom closedRoom, MatchResultReport report, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var resultKey = $"{closedRoom.MatchId}:{report.ResultSequence}";
        var payload = JsonSerializer.Serialize(report);
        lock (mutationGate)
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
            SynchronizeActivePlayersUnsafe(closedRoom);
            return Task.FromResult(FinalizeMatchStatus.Accepted);
        }
    }

    private LobbyRoom? GetRoomByIdUnsafe(string roomId) =>
        codeById.TryGetValue(roomId, out var code) && roomsByCode.TryGetValue(code, out var room)
            ? room
            : null;

    private bool HasActivePlayerConflictUnsafe(LobbyRoom room) =>
        IsActive(room.Lifecycle) && room.PlayerIds.Any(playerId =>
            activeRoomByPlayer.TryGetValue(playerId, out var activeRoomId)
            && activeRoomId != room.RoomId);

    private void SynchronizeActivePlayersUnsafe(LobbyRoom room)
    {
        foreach (var playerId in activeRoomByPlayer
                     .Where(pair => pair.Value == room.RoomId && !room.PlayerIds.Contains(pair.Key, StringComparer.Ordinal))
                     .Select(pair => pair.Key)
                     .ToArray())
        {
            activeRoomByPlayer.Remove(playerId);
        }

        if (!IsActive(room.Lifecycle))
        {
            foreach (var playerId in activeRoomByPlayer
                         .Where(pair => pair.Value == room.RoomId)
                         .Select(pair => pair.Key)
                         .ToArray())
            {
                activeRoomByPlayer.Remove(playerId);
            }
            return;
        }

        foreach (var playerId in room.PlayerIds)
        {
            activeRoomByPlayer[playerId] = room.RoomId;
        }
    }

    private static bool IsActive(RoomLifecycle lifecycle) => lifecycle is
        RoomLifecycle.Allocating or RoomLifecycle.Waiting or RoomLifecycle.Playing or RoomLifecycle.Settling;
}
