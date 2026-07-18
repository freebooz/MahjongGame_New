using System.Collections.Concurrent;
using GuiyangMahjong.Lobby.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Lobby.Services;

public sealed class OnlinePresenceService
{
    private readonly ConcurrentDictionary<string, DateTimeOffset> lastSeen = new(StringComparer.Ordinal);
    private readonly LobbyOptions options;
    private readonly TimeProvider timeProvider;

    public OnlinePresenceService(IOptions<LobbyOptions> options, TimeProvider timeProvider)
    {
        this.options = options.Value;
        this.timeProvider = timeProvider;
    }

    public void Touch(string playerId) => lastSeen[playerId] = timeProvider.GetUtcNow();

    public int GetOnlineCount()
    {
        var now = timeProvider.GetUtcNow();
        var cutoff = now - TimeSpan.FromSeconds(options.PresenceTimeoutSeconds);
        foreach (var pair in lastSeen)
        {
            if (pair.Value < cutoff) lastSeen.TryRemove(pair.Key, out _);
        }
        return lastSeen.Count;
    }
}

