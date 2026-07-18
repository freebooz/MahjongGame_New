using System.Collections.Concurrent;
using System.Security.Cryptography;
using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Lobby.Security;

public enum PasswordVerificationStatus
{
    Success,
    Required,
    Wrong,
    RateLimited
}

public sealed record PasswordVerificationResult(
    PasswordVerificationStatus Status,
    int RetryAfterMilliseconds = 0);

public interface IRoomPasswordService
{
    ProtectedPassword Protect(string password);
    PasswordVerificationResult Verify(
        string playerId, string roomId, ProtectedPassword? protectedPassword, string? candidate);
}

/// <summary>密码只保留 PBKDF2-SHA256 盐化摘要；审计日志不得传入候选密码。</summary>
public sealed class RoomPasswordService : IRoomPasswordService
{
    private const int Iterations = 120_000;
    private const int SaltSize = 16;
    private const int HashSize = 32;

    private readonly ConcurrentDictionary<string, FailureWindow> failures = new(StringComparer.Ordinal);
    private readonly LobbyOptions options;
    private readonly TimeProvider timeProvider;

    public RoomPasswordService(IOptions<LobbyOptions> options, TimeProvider timeProvider)
    {
        this.options = options.Value;
        this.timeProvider = timeProvider;
    }

    public ProtectedPassword Protect(string password)
    {
        ValidatePassword(password);
        var salt = RandomNumberGenerator.GetBytes(SaltSize);
        var hash = Rfc2898DeriveBytes.Pbkdf2(
            password, salt, Iterations, HashAlgorithmName.SHA256, HashSize);
        return new ProtectedPassword(Convert.ToBase64String(salt), Convert.ToBase64String(hash), Iterations);
    }

    public PasswordVerificationResult Verify(
        string playerId, string roomId, ProtectedPassword? protectedPassword, string? candidate)
    {
        if (protectedPassword is null)
        {
            return new PasswordVerificationResult(PasswordVerificationStatus.Success);
        }

        if (string.IsNullOrEmpty(candidate))
        {
            return new PasswordVerificationResult(PasswordVerificationStatus.Required);
        }

        var key = $"{playerId}:{roomId}";
        var now = timeProvider.GetUtcNow();
        var window = failures.GetOrAdd(key, _ => new FailureWindow());
        lock (window)
        {
            if (window.WindowStartedUtc == default ||
                now - window.WindowStartedUtc >= TimeSpan.FromSeconds(options.PasswordFailureWindowSeconds))
            {
                window.WindowStartedUtc = now;
                window.Count = 0;
            }

            if (window.Count >= options.PasswordFailureLimit)
            {
                var retry = TimeSpan.FromSeconds(options.PasswordFailureWindowSeconds) - (now - window.WindowStartedUtc);
                return new PasswordVerificationResult(
                    PasswordVerificationStatus.RateLimited,
                    Math.Max(1, (int)retry.TotalMilliseconds));
            }

            byte[] actual;
            byte[] expected;
            byte[] salt;
            try
            {
                salt = Convert.FromBase64String(protectedPassword.SaltBase64);
                expected = Convert.FromBase64String(protectedPassword.HashBase64);
                actual = Rfc2898DeriveBytes.Pbkdf2(
                    candidate, salt, protectedPassword.Iterations, HashAlgorithmName.SHA256, expected.Length);
            }
            catch (FormatException)
            {
                window.Count++;
                return new PasswordVerificationResult(PasswordVerificationStatus.Wrong);
            }

            if (!CryptographicOperations.FixedTimeEquals(actual, expected))
            {
                window.Count++;
                return new PasswordVerificationResult(PasswordVerificationStatus.Wrong);
            }

            failures.TryRemove(key, out _);
            return new PasswordVerificationResult(PasswordVerificationStatus.Success);
        }
    }

    private static void ValidatePassword(string password)
    {
        if (password.Length is < 6 or > 12)
        {
            throw new ArgumentException("房间密码必须为 6 到 12 个字符", nameof(password));
        }
    }

    private sealed class FailureWindow
    {
        public DateTimeOffset WindowStartedUtc { get; set; }
        public int Count { get; set; }
    }
}

