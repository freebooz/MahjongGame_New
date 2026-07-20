using GuiyangMahjong.Lobby.Options;
using Microsoft.Extensions.Options;
using Npgsql;
using StackExchange.Redis;

namespace GuiyangMahjong.Lobby.Storage;

public sealed class LobbyPersistenceConnections : IAsyncDisposable
{
    private readonly Lazy<IConnectionMultiplexer> redis;
    private readonly Lazy<NpgsqlDataSource> postgres;

    public LobbyPersistenceConnections(IOptions<LobbyOptions> options)
    {
        var persistence = options.Value.Persistence;
        redis = new Lazy<IConnectionMultiplexer>(
            () => ConnectionMultiplexer.Connect(persistence.RedisConnectionString),
            LazyThreadSafetyMode.ExecutionAndPublication);
        postgres = new Lazy<NpgsqlDataSource>(
            () => NpgsqlDataSource.Create(persistence.PostgresConnectionString),
            LazyThreadSafetyMode.ExecutionAndPublication);
    }

    public IConnectionMultiplexer Redis => redis.Value;
    public NpgsqlDataSource Postgres => postgres.Value;

    public async ValueTask DisposeAsync()
    {
        if (postgres.IsValueCreated) await postgres.Value.DisposeAsync();
        if (redis.IsValueCreated)
        {
            await redis.Value.CloseAsync();
            redis.Value.Dispose();
        }
    }
}
