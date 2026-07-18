using System.Text.Json;
using System.Text.Json.Serialization;
using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Options;
using Microsoft.Extensions.Options;
using Npgsql;
using StackExchange.Redis;

namespace GuiyangMahjong.Lobby.Storage;

/// <summary>
/// Redis 保存热房间快照，PostgreSQL 通过唯一约束提供房间号原子性和重启恢复。
/// 任何密码字段均已是盐化摘要，原始密码不会传入本类型或日志。
/// </summary>
public sealed class RedisPostgresLobbyStore : ILobbyStore, IAsyncDisposable
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        Converters = { new JsonStringEnumConverter() }
    };

    private readonly LobbyPersistenceOptions options;
    private readonly IConnectionMultiplexer redis;
    private readonly NpgsqlDataSource postgres;
    private readonly ILogger<RedisPostgresLobbyStore> logger;

    public RedisPostgresLobbyStore(
        IOptions<LobbyOptions> options,
        IConnectionMultiplexer redis,
        NpgsqlDataSource postgres,
        ILogger<RedisPostgresLobbyStore> logger)
    {
        this.options = options.Value.Persistence;
        this.redis = redis;
        this.postgres = postgres;
        this.logger = logger;
    }

    public async Task InitializeAsync(CancellationToken cancellationToken)
    {
        var schemaPath = Path.Combine(AppContext.BaseDirectory, "Storage", "schema.sql");
        var sql = await File.ReadAllTextAsync(schemaPath, cancellationToken);
        await using var command = postgres.CreateCommand(sql);
        await command.ExecuteNonQueryAsync(cancellationToken);
        logger.LogInformation("PostgreSQL 大厅表结构已验证，Redis 前缀={RedisKeyPrefix}", options.RedisKeyPrefix);
    }

    public async Task<bool> TryCreateRoomAsync(LobbyRoom room, CancellationToken cancellationToken)
    {
        var payload = JsonSerializer.Serialize(room, JsonOptions);
        await using var command = postgres.CreateCommand(
            """
            INSERT INTO lobby_rooms(room_id, room_code, lifecycle, state_sequence, payload, updated_at_utc)
            VALUES ($1, $2, $3, $4, $5::jsonb, $6)
            ON CONFLICT (room_code) DO NOTHING
            RETURNING room_id
            """);
        command.Parameters.AddWithValue(room.RoomId);
        command.Parameters.AddWithValue(room.RoomCode);
        command.Parameters.AddWithValue(room.Lifecycle.ToString());
        command.Parameters.AddWithValue(room.StateSequence);
        command.Parameters.AddWithValue(payload);
        command.Parameters.AddWithValue(room.UpdatedAtUtc);
        var inserted = await command.ExecuteScalarAsync(cancellationToken);
        if (inserted is null)
        {
            return false;
        }

        await CacheRoomAsync(room);
        return true;
    }

    public Task<LobbyRoom?> GetRoomByCodeAsync(string roomCode, CancellationToken cancellationToken) =>
        GetRoomAsync("room_code", roomCode, cancellationToken);

    public Task<LobbyRoom?> GetRoomByIdAsync(string roomId, CancellationToken cancellationToken) =>
        GetRoomAsync("room_id", roomId, cancellationToken);

    public async Task<IReadOnlyList<LobbyRoom>> ListPublicRoomsAsync(CancellationToken cancellationToken)
    {
        await using var command = postgres.CreateCommand(
            """
            SELECT payload::text FROM lobby_rooms
            WHERE lifecycle IN ('Allocating', 'Waiting')
            ORDER BY updated_at_utc DESC LIMIT 100
            """);
        var result = new List<LobbyRoom>();
        await using var reader = await command.ExecuteReaderAsync(cancellationToken);
        while (await reader.ReadAsync(cancellationToken))
        {
            var room = JsonSerializer.Deserialize<LobbyRoom>(reader.GetString(0), JsonOptions);
            if (room is { PublicRoom: true }) result.Add(room);
        }
        return result;
    }

    public async Task<AddPlayerResult> TryAddPlayerAsync(
        string roomCode, string playerId, CancellationToken cancellationToken)
    {
        await using var connection = await postgres.OpenConnectionAsync(cancellationToken);
        await using var transaction = await connection.BeginTransactionAsync(cancellationToken);
        await using var select = new NpgsqlCommand(
            "SELECT payload::text FROM lobby_rooms WHERE room_code = $1 FOR UPDATE", connection, transaction);
        select.Parameters.AddWithValue(roomCode);
        var payload = await select.ExecuteScalarAsync(cancellationToken) as string;
        if (payload is null)
        {
            await transaction.RollbackAsync(cancellationToken);
            return new AddPlayerResult(AddPlayerStatus.RoomNotFound, null);
        }

        var room = JsonSerializer.Deserialize<LobbyRoom>(payload, JsonOptions)
            ?? throw new InvalidDataException("数据库房间快照无法解析");
        if (room.PlayerIds.Contains(playerId, StringComparer.Ordinal))
        {
            await transaction.CommitAsync(cancellationToken);
            return new AddPlayerResult(AddPlayerStatus.AlreadyMember, room);
        }
        if (room.Lifecycle is RoomLifecycle.Closed or RoomLifecycle.Failed or RoomLifecycle.Playing or RoomLifecycle.Settling)
        {
            await transaction.CommitAsync(cancellationToken);
            return new AddPlayerResult(AddPlayerStatus.RoomClosed, room);
        }
        if (room.PlayerIds.Length >= room.MaximumPlayers)
        {
            await transaction.CommitAsync(cancellationToken);
            return new AddPlayerResult(AddPlayerStatus.RoomFull, room);
        }

        var updated = room with
        {
            PlayerIds = [.. room.PlayerIds, playerId],
            StateSequence = room.StateSequence + 1,
            UpdatedAtUtc = DateTimeOffset.UtcNow
        };
        await UpdateInTransactionAsync(connection, transaction, updated, cancellationToken);
        await transaction.CommitAsync(cancellationToken);
        await CacheRoomAsync(updated);
        return new AddPlayerResult(AddPlayerStatus.Added, updated);
    }

    public async Task<bool> UpdateRoomAsync(LobbyRoom room, CancellationToken cancellationToken)
    {
        var payload = JsonSerializer.Serialize(room, JsonOptions);
        await using var command = postgres.CreateCommand(
            """
            UPDATE lobby_rooms
            SET lifecycle=$1, state_sequence=$2, payload=$3::jsonb, updated_at_utc=$4
            WHERE room_id=$5 AND state_sequence <= $2
            """);
        command.Parameters.AddWithValue(room.Lifecycle.ToString());
        command.Parameters.AddWithValue(room.StateSequence);
        command.Parameters.AddWithValue(payload);
        command.Parameters.AddWithValue(room.UpdatedAtUtc);
        command.Parameters.AddWithValue(room.RoomId);
        if (await command.ExecuteNonQueryAsync(cancellationToken) == 0) return false;
        await CacheRoomAsync(room);
        return true;
    }

    private async Task<LobbyRoom?> GetRoomAsync(string column, string value, CancellationToken cancellationToken)
    {
        var database = redis.GetDatabase();
        var cacheKey = column == "room_code" ? RoomCodeKey(value) : RoomIdKey(value);
        var cached = await database.StringGetAsync(cacheKey);
        if (cached.HasValue)
        {
            return JsonSerializer.Deserialize<LobbyRoom>((string)cached!, JsonOptions);
        }

        var sql = $"SELECT payload::text FROM lobby_rooms WHERE {column} = $1";
        await using var command = postgres.CreateCommand(sql);
        command.Parameters.AddWithValue(value);
        var payload = await command.ExecuteScalarAsync(cancellationToken) as string;
        if (payload is null) return null;
        var room = JsonSerializer.Deserialize<LobbyRoom>(payload, JsonOptions);
        if (room is not null) await CacheRoomAsync(room);
        return room;
    }

    private async Task CacheRoomAsync(LobbyRoom room)
    {
        var payload = JsonSerializer.Serialize(room, JsonOptions);
        var database = redis.GetDatabase();
        var expiry = TimeSpan.FromHours(24);
        await Task.WhenAll(
            database.StringSetAsync(RoomCodeKey(room.RoomCode), payload, expiry),
            database.StringSetAsync(RoomIdKey(room.RoomId), payload, expiry));
    }

    private static async Task UpdateInTransactionAsync(
        NpgsqlConnection connection,
        NpgsqlTransaction transaction,
        LobbyRoom room,
        CancellationToken cancellationToken)
    {
        await using var update = new NpgsqlCommand(
            """
            UPDATE lobby_rooms SET lifecycle=$1, state_sequence=$2, payload=$3::jsonb, updated_at_utc=$4
            WHERE room_id=$5
            """, connection, transaction);
        update.Parameters.AddWithValue(room.Lifecycle.ToString());
        update.Parameters.AddWithValue(room.StateSequence);
        update.Parameters.AddWithValue(JsonSerializer.Serialize(room, JsonOptions));
        update.Parameters.AddWithValue(room.UpdatedAtUtc);
        update.Parameters.AddWithValue(room.RoomId);
        await update.ExecuteNonQueryAsync(cancellationToken);
    }

    private string RoomCodeKey(string roomCode) => $"{options.RedisKeyPrefix}:room:code:{roomCode}";
    private string RoomIdKey(string roomId) => $"{options.RedisKeyPrefix}:room:id:{roomId}";

    public async ValueTask DisposeAsync()
    {
        await postgres.DisposeAsync();
        await redis.CloseAsync();
        redis.Dispose();
    }
}

