using System.Net.Http.Json;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Realtime;
using GuiyangMahjong.Lobby.Security;

namespace GuiyangMahjong.Lobby.Tests;

public sealed class WebSocketEventTests(LobbyWebApplicationFactory factory)
    : IClassFixture<LobbyWebApplicationFactory>
{
    [Fact]
    public async Task ConnectionAndRoomCreation_PublishRequiredEvents()
    {
        const string playerId = "websocket-player";
        var token = HmacPlayerTokenValidator.CreateSignedToken(
            LobbyWebApplicationFactory.SigningKey,
            new PlayerIdentity(playerId, "事件玩家", "Guest"),
            DateTimeOffset.UtcNow.AddMinutes(10));
        var webSocketClient = factory.Server.CreateWebSocketClient();
        webSocketClient.ConfigureRequest = request =>
        {
            request.Headers.Authorization = $"Bearer {token}";
            request.Headers["X-Request-Id"] = Guid.NewGuid().ToString();
        };

        using var socket = await webSocketClient.ConnectAsync(
            new Uri("ws://localhost/v1/events"), CancellationToken.None);
        var connectedEvent = await ReceiveEventAsync(socket);
        Assert.Equal(LobbyEventTypes.LobbyUpdated, connectedEvent.GetProperty("type").GetString());

        using var http = factory.CreateAuthenticatedClient(playerId, "事件玩家");
        using var create = new HttpRequestMessage(HttpMethod.Post, "/v1/rooms")
        {
            Content = JsonContent.Create(new CreateRoomRequest(
                4,
                true,
                true,
                false,
                null,
                new Dictionary<string, object?> { ["ruleId"] = "GuiyangMainstreamV1" }))
        };
        LobbyWebApplicationFactory.AddRequestHeaders(create, "websocket-create-room-key");
        var response = await http.SendAsync(create);
        response.EnsureSuccessStatusCode();

        var roomEvent = await ReceiveEventAsync(socket);
        Assert.Equal(LobbyEventTypes.RoomUpdated, roomEvent.GetProperty("type").GetString());
        Assert.Contains(LobbyEventTypes.ServerAssigned, LobbyEventTypes.All);
        Assert.Contains(LobbyEventTypes.RoomClosed, LobbyEventTypes.All);

        await socket.CloseAsync(WebSocketCloseStatus.NormalClosure, "测试结束", CancellationToken.None);
    }

    private static async Task<JsonElement> ReceiveEventAsync(WebSocket socket)
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        var buffer = new byte[4096];
        var result = await socket.ReceiveAsync(buffer, timeout.Token);
        Assert.Equal(WebSocketMessageType.Text, result.MessageType);
        using var document = JsonDocument.Parse(Encoding.UTF8.GetString(buffer, 0, result.Count));
        return document.RootElement.Clone();
    }
}
