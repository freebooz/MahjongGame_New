using System.Collections.Concurrent;

namespace GuiyangMahjong.Lobby.Services;

public sealed class InMemoryIdempotencyStore
{
    private readonly ConcurrentDictionary<string, Lazy<Task<object>>> operations = new(StringComparer.Ordinal);

    public async Task<T> ExecuteAsync<T>(string key, Func<Task<T>> operation)
        where T : notnull
    {
        var lazy = operations.GetOrAdd(
            key,
            _ => new Lazy<Task<object>>(
                async () => (object)await operation(),
                LazyThreadSafetyMode.ExecutionAndPublication));
        try
        {
            return (T)await lazy.Value;
        }
        catch
        {
            operations.TryRemove(new KeyValuePair<string, Lazy<Task<object>>>(key, lazy));
            throw;
        }
    }
}

