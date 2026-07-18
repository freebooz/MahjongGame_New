using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Options;
using GuiyangMahjong.Lobby.Security;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Lobby.Tests;

public sealed class SecurityTests
{
    private static readonly DateTimeOffset Now = new(2026, 7, 18, 0, 0, 0, TimeSpan.Zero);

    [Fact]
    public void Token_ValidSignature_AcceptsTrustedIdentity()
    {
        var time = new FixedTimeProvider(Now);
        var validator = CreateValidator(time);
        var token = HmacPlayerTokenValidator.CreateSignedToken(
            LobbyWebApplicationFactory.SigningKey,
            new PlayerIdentity("guest-001", "玩家甲", "Guest"),
            Now.AddMinutes(5));

        var result = validator.Validate(token);

        Assert.True(result.IsValid);
        Assert.Equal("guest-001", result.Player?.PlayerId);
    }

    [Fact]
    public void Token_Expired_IsRejected()
    {
        var time = new FixedTimeProvider(Now);
        var validator = CreateValidator(time);
        var token = HmacPlayerTokenValidator.CreateSignedToken(
            LobbyWebApplicationFactory.SigningKey,
            new PlayerIdentity("guest-expired", "过期玩家", "Guest"),
            Now.AddSeconds(-1));

        var result = validator.Validate(token);

        Assert.False(result.IsValid);
        Assert.Contains("过期", result.ChineseReason, StringComparison.Ordinal);
    }

    [Fact]
    public void Token_ClientTampering_IsRejected()
    {
        var time = new FixedTimeProvider(Now);
        var validator = CreateValidator(time);
        var token = HmacPlayerTokenValidator.CreateSignedToken(
            LobbyWebApplicationFactory.SigningKey,
            new PlayerIdentity("guest-001", "玩家甲", "Guest"),
            Now.AddMinutes(5));
        var tampered = $"A{token[1..]}";

        Assert.False(validator.Validate(tampered).IsValid);
    }

    [Fact]
    public void Password_WrongAttempts_AreRateLimitedAndRecoverAfterWindow()
    {
        var time = new FixedTimeProvider(Now);
        var options = Microsoft.Extensions.Options.Options.Create(CreateOptions());
        var service = new RoomPasswordService(options, time);
        var protectedPassword = service.Protect("654321");

        for (var index = 0; index < 5; index++)
        {
            Assert.Equal(
                PasswordVerificationStatus.Wrong,
                service.Verify("player", "room", protectedPassword, "wrong00").Status);
        }
        Assert.Equal(
            PasswordVerificationStatus.RateLimited,
            service.Verify("player", "room", protectedPassword, "654321").Status);

        time.Advance(TimeSpan.FromMinutes(6));
        Assert.Equal(
            PasswordVerificationStatus.Success,
            service.Verify("player", "room", protectedPassword, "654321").Status);
    }

    private static HmacPlayerTokenValidator CreateValidator(TimeProvider timeProvider) =>
        new(Microsoft.Extensions.Options.Options.Create(CreateOptions()), timeProvider);

    private static LobbyOptions CreateOptions() => new()
    {
        TokenSigningKey = LobbyWebApplicationFactory.SigningKey,
        PasswordFailureLimit = 5,
        PasswordFailureWindowSeconds = 300
    };
}
