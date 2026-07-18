using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text.Json;
using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Realtime;

public static class LobbyEventTypes
{
    public const string LobbyUpdated = "lobby.updated";
    public const string RoomUpdated = "room.updated";
    public const string ServerAssigned = "server.assigned";
    public const string RoomClosed = "room.closed";

    public static readonly IReadOnlySet<string> All = new HashSet<string>(StringComparer.Ordinal)
    {
        LobbyUpdated,
        RoomUpdated,
        ServerAssigned,
        RoomClosed
    };
}

public interface ILobbyEventPublisher
{
    Task PublishAsync(string type, object data, CancellationToken cancellationToken);
}

public sealed class LobbyEventHub : ILobbyEventPublisher
{
    private readonly ConcurrentDictionary<Guid, ClientConnection> clients = new();
    private long sequence;

    public int ConnectedClientCount => clients.Count;

    public async Task HandleClientAsync(
        PlayerIdentity player, WebSocket socket, CancellationToken cancellationToken)
    {
        var id = Guid.NewGuid();
        clients[id] = new ClientConnection(player.PlayerId, socket);
        try
        {
            await SendAsync(socket, new LobbyEventEnvelope(
                LobbyEventTypes.LobbyUpdated,
                Interlocked.Increment(ref sequence),
                DateTimeOffset.UtcNow,
                new { onlinePlayerCount = ConnectedClientCount }), cancellationToken);

            var buffer = new byte[512];
            while (socket.State == WebSocketState.Open && !cancellationToken.IsCancellationRequested)
            {
                var result = await socket.ReceiveAsync(buffer, cancellationToken);
                if (result.MessageType == WebSocketMessageType.Close) break;
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
        catch (WebSocketException)
        {
        }
        finally
        {
            clients.TryRemove(id, out _);
            if (socket.State is WebSocketState.Open or WebSocketState.CloseReceived)
            {
                await socket.CloseAsync(
                    WebSocketCloseStatus.NormalClosure, "连接结束", CancellationToken.None);
            }
            socket.Dispose();
        }
    }

    public async Task PublishAsync(string type, object data, CancellationToken cancellationToken)
    {
        var envelope = new LobbyEventEnvelope(
            type,
            Interlocked.Increment(ref sequence),
            DateTimeOffset.UtcNow,
            data);
        var tasks = clients.Values
            .Where(client => client.Socket.State == WebSocketState.Open)
            .Select(client => SendAsync(client.Socket, envelope, cancellationToken));
        await Task.WhenAll(tasks);
    }

    private static Task SendAsync(WebSocket socket, LobbyEventEnvelope envelope, CancellationToken cancellationToken)
    {
        var payload = JsonSerializer.SerializeToUtf8Bytes(envelope, new JsonSerializerOptions(JsonSerializerDefaults.Web));
        return socket.SendAsync(payload, WebSocketMessageType.Text, true, cancellationToken);
    }

    private sealed record ClientConnection(string PlayerId, WebSocket Socket);
}
