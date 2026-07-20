using System.Collections.Concurrent;
using System.Text.Json;
using GuiyangMahjong.Lobby.Options;
using GuiyangMahjong.Lobby.Storage;
using Microsoft.Extensions.Options;
using StackExchange.Redis;

namespace GuiyangMahjong.Lobby.Services;

public sealed record IdempotentHttpResponse(int StatusCode, JsonElement Body);

public interface IIdempotencyStore
{
    Task<IdempotentHttpResponse> ExecuteAsync(
        string key,
        Func<Task<IdempotentHttpResponse>> operation,
        CancellationToken cancellationToken);
}

public sealed class InMemoryIdempotencyStore(
    IOptions<LobbyOptions> options,
    TimeProvider timeProvider) : IIdempotencyStore
{
    private readonly ConcurrentDictionary<string, Entry> operations = new(StringComparer.Ordinal);
    private readonly TimeSpan ttl = TimeSpan.FromSeconds(options.Value.IdempotencyTtlSeconds);

    public async Task<IdempotentHttpResponse> ExecuteAsync(
        string key,
        Func<Task<IdempotentHttpResponse>> operation,
        CancellationToken cancellationToken)
    {
        var now = timeProvider.GetUtcNow();
        if (operations.TryGetValue(key, out var expired) && now - expired.CreatedAtUtc >= ttl)
            operations.TryRemove(new KeyValuePair<string, Entry>(key, expired));
        var entry = operations.GetOrAdd(
            key,
            _ => new Entry(
                now,
                new Lazy<Task<IdempotentHttpResponse>>(
                    operation, LazyThreadSafetyMode.ExecutionAndPublication)));
        try
        {
            return await entry.Operation.Value.WaitAsync(cancellationToken);
        }
        catch
        {
            operations.TryRemove(new KeyValuePair<string, Entry>(key, entry));
            throw;
        }
    }

    private sealed record Entry(
        DateTimeOffset CreatedAtUtc,
        Lazy<Task<IdempotentHttpResponse>> Operation);
}

public sealed class RedisIdempotencyStore : IIdempotencyStore
{
    private const string ReleaseLockScript =
        "if redis.call('get', KEYS[1]) == ARGV[1] then return redis.call('del', KEYS[1]) else return 0 end";
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web);
    private readonly IDatabase database;
    private readonly string prefix;
    private readonly TimeSpan resultTtl;
    private readonly TimeSpan lockTtl;

    public RedisIdempotencyStore(
        LobbyPersistenceConnections connections,
        IOptions<LobbyOptions> options)
    {
        database = connections.Redis.GetDatabase();
        prefix = options.Value.Persistence.RedisKeyPrefix;
        resultTtl = TimeSpan.FromSeconds(options.Value.IdempotencyTtlSeconds);
        lockTtl = TimeSpan.FromSeconds(options.Value.IdempotencyLockSeconds);
    }

    public async Task<IdempotentHttpResponse> ExecuteAsync(
        string key,
        Func<Task<IdempotentHttpResponse>> operation,
        CancellationToken cancellationToken)
    {
        var resultKey = $"{prefix}:idempotency:result:{key}";
        var lockKey = $"{prefix}:idempotency:lock:{key}";
        while (true)
        {
            var cached = await database.StringGetAsync(resultKey).WaitAsync(cancellationToken);
            if (cached.HasValue)
                return Deserialize(cached!);

            var owner = Guid.NewGuid().ToString("N");
            if (!await database.StringSetAsync(
                    lockKey, owner, lockTtl, When.NotExists).WaitAsync(cancellationToken))
            {
                await Task.Delay(TimeSpan.FromMilliseconds(40), cancellationToken);
                continue;
            }

            try
            {
                cached = await database.StringGetAsync(resultKey).WaitAsync(cancellationToken);
                if (cached.HasValue) return Deserialize(cached!);
                var response = await operation();
                var payload = JsonSerializer.Serialize(response, JsonOptions);
                await database.StringSetAsync(resultKey, payload, resultTtl).WaitAsync(cancellationToken);
                return response;
            }
            finally
            {
                await database.ScriptEvaluateAsync(
                    ReleaseLockScript,
                    [new RedisKey(lockKey)],
                    [new RedisValue(owner)]);
            }
        }
    }

    private static IdempotentHttpResponse Deserialize(RedisValue value) =>
        JsonSerializer.Deserialize<IdempotentHttpResponse>((string)value!, JsonOptions)
        ?? throw new InvalidDataException("Redis idempotency response is invalid.");
}
