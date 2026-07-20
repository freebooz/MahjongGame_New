using GuiyangMahjong.Auth.Domain;

namespace GuiyangMahjong.Auth.Storage;

public interface IAuthStore
{
    Task InitializeAsync(CancellationToken cancellationToken);
    Task<bool> CheckHealthAsync(CancellationToken cancellationToken);
    Task<AuthIdentity> GetOrCreateGuestAsync(
        string installationHash,
        AuthIdentity proposedIdentity,
        CancellationToken cancellationToken);
    Task CreateRefreshSessionAsync(RefreshSession session, CancellationToken cancellationToken);
    Task<RefreshRotationResult> RotateRefreshSessionAsync(
        string currentSessionId,
        byte[] currentTokenHash,
        RefreshSession replacement,
        DateTimeOffset now,
        CancellationToken cancellationToken);
    Task<bool> RevokeRefreshSessionAsync(
        string sessionId,
        byte[] tokenHash,
        DateTimeOffset now,
        CancellationToken cancellationToken);
}
