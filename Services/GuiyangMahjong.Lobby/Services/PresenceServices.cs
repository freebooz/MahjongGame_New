using System.Collections.Concurrent;
using GuiyangMahjong.Lobby.Options;
using GuiyangMahjong.Lobby.Storage;
using Microsoft.Extensions.Options;
using StackExchange.Redis;

namespace GuiyangMahjong.Lobby.Services;

public interface IOnlinePresenceService
{
    Task TouchAsync(string playerId, CancellationToken cancellationToken);
    Task<long> GetOnlineCountAsync(CancellationToken cancellationToken);
}

public sealed class InMemoryOnlinePresenceService(
    IOptions<LobbyOptions> options,
    TimeProvider timeProvider) : IOnlinePresenceService
{
    private readonly ConcurrentDictionary<string, DateTimeOffset> lastSeen = new(StringComparer.Ordinal);
    private readonly TimeSpan timeout = TimeSpan.FromSeconds(options.Value.PresenceTimeoutSeconds);

    public Task TouchAsync(string playerId, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        lastSeen[playerId] = timeProvider.GetUtcNow();
        return Task.CompletedTask;
    }

    public Task<long> GetOnlineCountAsync(CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var cutoff = timeProvider.GetUtcNow() - timeout;
        foreach (var pair in lastSeen)
            if (pair.Value < cutoff) lastSeen.TryRemove(pair.Key, out _);
        return Task.FromResult((long)lastSeen.Count);
    }
}

public sealed class RedisOnlinePresenceService : IOnlinePresenceService
{
    private readonly IDatabase database;
    private readonly RedisKey presenceKey;
    private readonly TimeSpan timeout;
    private readonly TimeProvider timeProvider;

    public RedisOnlinePresenceService(
        LobbyPersistenceConnections connections,
        IOptions<LobbyOptions> options,
        TimeProvider timeProvider)
    {
        database = connections.Redis.GetDatabase();
        presenceKey = $"{options.Value.Persistence.RedisKeyPrefix}:presence";
        timeout = TimeSpan.FromSeconds(options.Value.PresenceTimeoutSeconds);
        this.timeProvider = timeProvider;
    }

    public async Task TouchAsync(string playerId, CancellationToken cancellationToken)
    {
        var score = timeProvider.GetUtcNow().ToUnixTimeMilliseconds();
        await database.SortedSetAddAsync(presenceKey, playerId, score).WaitAsync(cancellationToken);
    }

    public async Task<long> GetOnlineCountAsync(CancellationToken cancellationToken)
    {
        var cutoff = (timeProvider.GetUtcNow() - timeout).ToUnixTimeMilliseconds();
        await database.SortedSetRemoveRangeByScoreAsync(
            presenceKey, double.NegativeInfinity, cutoff).WaitAsync(cancellationToken);
        return await database.SortedSetLengthAsync(presenceKey).WaitAsync(cancellationToken);
    }
}
