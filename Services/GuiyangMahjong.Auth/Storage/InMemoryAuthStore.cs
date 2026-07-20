using System.Security.Cryptography;
using GuiyangMahjong.Auth.Domain;

namespace GuiyangMahjong.Auth.Storage;

public sealed class InMemoryAuthStore : IAuthStore
{
    private readonly Dictionary<string, AuthIdentity> identitiesByInstallation = new(StringComparer.Ordinal);
    private readonly Dictionary<string, AuthIdentity> identitiesByPlayer = new(StringComparer.Ordinal);
    private readonly Dictionary<string, RefreshSession> sessions = new(StringComparer.Ordinal);
    private readonly object gate = new();

    public Task InitializeAsync(CancellationToken cancellationToken) => Task.CompletedTask;
    public Task<bool> CheckHealthAsync(CancellationToken cancellationToken) => Task.FromResult(true);

    public Task<AuthIdentity> GetOrCreateGuestAsync(
        string installationHash,
        AuthIdentity proposedIdentity,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        lock (gate)
        {
            if (identitiesByInstallation.TryGetValue(installationHash, out var existing))
                return Task.FromResult(existing);
            identitiesByInstallation[installationHash] = proposedIdentity;
            identitiesByPlayer[proposedIdentity.PlayerId] = proposedIdentity;
            return Task.FromResult(proposedIdentity);
        }
    }

    public Task CreateRefreshSessionAsync(RefreshSession session, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        lock (gate) sessions.Add(session.SessionId, session);
        return Task.CompletedTask;
    }

    public Task<RefreshRotationResult> RotateRefreshSessionAsync(
        string currentSessionId,
        byte[] currentTokenHash,
        RefreshSession replacement,
        DateTimeOffset now,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        lock (gate)
        {
            if (!sessions.TryGetValue(currentSessionId, out var current))
                return Task.FromResult(new RefreshRotationResult(RefreshRotationStatus.NotFound, null));
            if (!FixedTimeEquals(current.TokenHash, currentTokenHash))
                return Task.FromResult(new RefreshRotationResult(RefreshRotationStatus.Invalid, null));
            if (current.RevokedAtUtc is not null)
                return Task.FromResult(new RefreshRotationResult(RefreshRotationStatus.Revoked, null));
            if (current.ExpiresAtUtc <= now)
                return Task.FromResult(new RefreshRotationResult(RefreshRotationStatus.Expired, null));
            if (!identitiesByPlayer.TryGetValue(current.PlayerId, out var identity))
                return Task.FromResult(new RefreshRotationResult(RefreshRotationStatus.NotFound, null));

            sessions[currentSessionId] = current with { RevokedAtUtc = now };
            sessions.Add(replacement.SessionId, replacement with { PlayerId = current.PlayerId });
            return Task.FromResult(new RefreshRotationResult(RefreshRotationStatus.Rotated, identity));
        }
    }

    public Task<bool> RevokeRefreshSessionAsync(
        string sessionId,
        byte[] tokenHash,
        DateTimeOffset now,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        lock (gate)
        {
            if (!sessions.TryGetValue(sessionId, out var session)
                || session.RevokedAtUtc is not null
                || !FixedTimeEquals(session.TokenHash, tokenHash)) return Task.FromResult(false);
            sessions[sessionId] = session with { RevokedAtUtc = now };
            return Task.FromResult(true);
        }
    }

    private static bool FixedTimeEquals(byte[] left, byte[] right) =>
        left.Length == right.Length && CryptographicOperations.FixedTimeEquals(left, right);
}
