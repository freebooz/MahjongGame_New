using System.Net;
using System.Net.Http.Json;
using System.Text.Json;
using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Tests;

public sealed class LobbyApiTests(LobbyWebApplicationFactory factory)
    : IClassFixture<LobbyWebApplicationFactory>
{
    [Fact]
    public async Task Bootstrap_ReturnsAuthenticatedPlayerAndProtocol()
    {
        using var client = factory.CreateAuthenticatedClient("bootstrap-player");
        using var request = new HttpRequestMessage(HttpMethod.Get, "/v1/lobby/bootstrap");
        LobbyWebApplicationFactory.AddRequestHeaders(request);

        var response = await client.SendAsync(request);
        var body = await response.Content.ReadFromJsonAsync<LobbyBootstrapResponse>();

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Equal("bootstrap-player", body?.PlayerId);
        Assert.Equal(1, body?.ProtocolVersion);
    }

    [Fact]
    public async Task ExpiredToken_ReturnsSessionExpired()
    {
        using var client = factory.CreateAuthenticatedClient(
            "expired-player", expiresAtUtc: DateTimeOffset.UtcNow.AddMinutes(-1));
        using var request = new HttpRequestMessage(HttpMethod.Get, "/v1/lobby/bootstrap");
        LobbyWebApplicationFactory.AddRequestHeaders(request);

        var response = await client.SendAsync(request);
        var text = await response.Content.ReadAsStringAsync();

        Assert.Equal(HttpStatusCode.Unauthorized, response.StatusCode);
        Assert.Contains("SESSION_EXPIRED", text, StringComparison.Ordinal);
    }

    [Fact]
    public async Task ConcurrentHttpCreation_ReturnsUniqueRoomCodes()
    {
        var tasks = Enumerable.Range(0, 50).Select(async index =>
        {
            using var client = factory.CreateAuthenticatedClient($"http-player-{index}");
            using var request = new HttpRequestMessage(HttpMethod.Post, "/v1/rooms")
            {
                Content = JsonContent.Create(NewCreateRequest(false, null))
            };
            LobbyWebApplicationFactory.AddRequestHeaders(request, $"create-room-key-{index:D4}");
            var response = await client.SendAsync(request);
            Assert.Equal(HttpStatusCode.Accepted, response.StatusCode);
            return await response.Content.ReadFromJsonAsync<RoomOperation>();
        });

        var rooms = await Task.WhenAll(tasks);

        Assert.Equal(50, rooms.Select(room => room!.RoomCode).Distinct(StringComparer.Ordinal).Count());
    }

    [Fact]
    public async Task PasswordFailure_IsLimitedAndResponseNeverEchoesPassword()
    {
        const string secret = "857421";
        using var owner = factory.CreateAuthenticatedClient("password-owner");
        using var create = new HttpRequestMessage(HttpMethod.Post, "/v1/rooms")
        {
            Content = JsonContent.Create(NewCreateRequest(true, secret))
        };
        LobbyWebApplicationFactory.AddRequestHeaders(create, "password-room-create-key");
        var createResponse = await owner.SendAsync(create);
        var room = await createResponse.Content.ReadFromJsonAsync<RoomOperation>();
        Assert.NotNull(room);

        using var joiner = factory.CreateAuthenticatedClient("password-joiner");
        for (var attempt = 0; attempt < 5; attempt++)
        {
            using var join = NewJoinRequest(room.RoomCode, "wrong00", $"wrong-password-{attempt:D2}");
            var response = await joiner.SendAsync(join);
            var body = await response.Content.ReadAsStringAsync();
            Assert.Equal(HttpStatusCode.Forbidden, response.StatusCode);
            Assert.DoesNotContain("wrong00", body, StringComparison.Ordinal);
            Assert.DoesNotContain(secret, body, StringComparison.Ordinal);
        }

        using var limitedJoin = NewJoinRequest(room.RoomCode, secret, "password-limit-final");
        var limited = await joiner.SendAsync(limitedJoin);
        var limitedBody = await limited.Content.ReadAsStringAsync();
        Assert.Equal(HttpStatusCode.TooManyRequests, limited.StatusCode);
        Assert.Contains("RATE_LIMITED", limitedBody, StringComparison.Ordinal);
        Assert.DoesNotContain(secret, limitedBody, StringComparison.Ordinal);
    }

    [Fact]
    public async Task OpenApi_IsServedByIndependentApplication()
    {
        using var client = factory.CreateClient();
        var response = await client.GetAsync("/openapi/v1.yaml");
        var text = await response.Content.ReadAsStringAsync();

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Contains("openapi: 3.1.0", text, StringComparison.Ordinal);
    }

    private static CreateRoomRequest NewCreateRequest(bool protectedRoom, string? password) => new(
        4,
        true,
        true,
        protectedRoom,
        password,
        new Dictionary<string, object?> { ["ruleId"] = "GuiyangMainstreamV1" });

    private static HttpRequestMessage NewJoinRequest(string roomCode, string password, string idempotencyKey)
    {
        var request = new HttpRequestMessage(HttpMethod.Post, $"/v1/rooms/{roomCode}/join")
        {
            Content = JsonContent.Create(new JoinRoomRequest(password, 1))
        };
        LobbyWebApplicationFactory.AddRequestHeaders(request, idempotencyKey);
        return request;
    }
}

