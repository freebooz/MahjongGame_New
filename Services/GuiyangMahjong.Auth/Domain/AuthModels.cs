namespace GuiyangMahjong.Auth.Domain;

public sealed record GuestLoginRequest(string InstallationId, string? DisplayName);
public sealed record RefreshSessionRequest(string RefreshToken);
public sealed record LogoutRequest(string RefreshToken);

public sealed record AuthSessionResponse(
    string PlayerId,
    string DisplayName,
    string Provider,
    string AccessToken,
    DateTimeOffset AccessTokenExpiresAtUtc,
    string RefreshToken,
    DateTimeOffset RefreshTokenExpiresAtUtc);

public sealed record AuthIdentity(
    string PlayerId,
    string DisplayName,
    string Provider,
    DateTimeOffset CreatedAtUtc,
    DateTimeOffset UpdatedAtUtc);

public sealed record RefreshSession(
    string SessionId,
    string PlayerId,
    byte[] TokenHash,
    DateTimeOffset ExpiresAtUtc,
    DateTimeOffset CreatedAtUtc,
    DateTimeOffset? RevokedAtUtc);

public enum RefreshRotationStatus { Rotated, NotFound, Invalid, Expired, Revoked }
public sealed record RefreshRotationResult(RefreshRotationStatus Status, AuthIdentity? Identity);
