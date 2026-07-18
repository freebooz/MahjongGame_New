using System.Collections.Concurrent;
using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Storage;

/// <summary>本地开发与自动化测试存储；生产环境不得使用。</summary>
public sealed class InMemoryLobbyStore : ILobbyStore
{
    private readonly ConcurrentDictionary<string, LobbyRoom> roomsByCode = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, string> codeById = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, object> roomLocks = new(StringComparer.Ordinal);

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
}

