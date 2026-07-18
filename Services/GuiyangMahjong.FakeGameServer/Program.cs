using System.Net;
using System.Net.Http.Json;
using System.Net.Sockets;

var arguments = args
    .Where(argument => argument.StartsWith('-') && argument.Contains('='))
    .Select(argument => argument[1..].Split('=', 2))
    .ToDictionary(parts => parts[0], parts => parts[1], StringComparer.OrdinalIgnoreCase);

string Required(string name) => arguments.TryGetValue(name, out var value) && !string.IsNullOrWhiteSpace(value)
    ? value
    : throw new ArgumentException($"Missing -{name}=... argument.");

var roomId = Required("RoomId");
var matchId = Required("MatchId");
var serverInstanceId = Required("ServerInstanceId");
var port = int.Parse(Required("Port"));
var lobbyInternalUrl = Required("LobbyInternalUrl").TrimEnd('/');
var registrationCredential = Environment.GetEnvironmentVariable("MAHJONG_REGISTRATION_CREDENTIAL");
if (string.IsNullOrWhiteSpace(registrationCredential))
{
    throw new InvalidOperationException("MAHJONG_REGISTRATION_CREDENTIAL is required.");
}
Environment.SetEnvironmentVariable("MAHJONG_REGISTRATION_CREDENTIAL", null);
var buildVersion = Required("BuildVersion");
var advertisedIp = Required("AdvertisedIp");

using var shutdown = new CancellationTokenSource();
Console.CancelKeyPress += (_, eventArgs) =>
{
    eventArgs.Cancel = true;
    shutdown.Cancel();
};

var listener = new TcpListener(IPAddress.Any, port);
listener.Start();
Console.WriteLine($"FakeGameServer listening InstanceId={serverInstanceId} RoomId={roomId} Port={port}");

using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(5) };
using var registrationRequest = new HttpRequestMessage(
    HttpMethod.Post,
    $"{lobbyInternalUrl}/internal/gameservers/register")
{
    Content = JsonContent.Create(new GameServerRegistration(
        serverInstanceId,
        roomId,
        matchId,
        advertisedIp,
        port,
        buildVersion,
        registrationCredential))
};
registrationRequest.Headers.Add("X-Request-Id", Guid.NewGuid().ToString());
using var registrationResponse = await http.SendAsync(registrationRequest, shutdown.Token);
registrationResponse.EnsureSuccessStatusCode();
var registration = await registrationResponse.Content.ReadFromJsonAsync<GameServerRegistrationAck>(
    cancellationToken: shutdown.Token)
    ?? throw new InvalidOperationException("Lobby returned an empty registration response.");
if (registration.RoomBootstrap is null
    || registration.RoomBootstrap.RoomId != roomId
    || registration.RoomBootstrap.MatchId != matchId
    || registration.RoomBootstrap.RoomCode.Length != 6)
{
    throw new InvalidOperationException("Lobby returned an invalid authoritative room bootstrap.");
}
registrationCredential = string.Empty;

Console.WriteLine($"FakeGameServer registered InstanceId={serverInstanceId}");
using var heartbeatTimer = new PeriodicTimer(TimeSpan.FromSeconds(registration.HeartbeatIntervalSeconds));
try
{
    while (await heartbeatTimer.WaitForNextTickAsync(shutdown.Token))
    {
        using var heartbeatRequest = new HttpRequestMessage(
            HttpMethod.Post,
            $"{lobbyInternalUrl}/internal/gameservers/{serverInstanceId}/heartbeat")
        {
            Content = JsonContent.Create(new GameServerHeartbeat(
                roomId,
                registration.HeartbeatCredential,
                0,
                "Waiting",
                0,
                buildVersion,
                DateTimeOffset.UtcNow))
        };
        heartbeatRequest.Headers.Add("X-Request-Id", Guid.NewGuid().ToString());
        using var heartbeatResponse = await http.SendAsync(heartbeatRequest, shutdown.Token);
        heartbeatResponse.EnsureSuccessStatusCode();
    }
}
catch (OperationCanceledException) when (shutdown.IsCancellationRequested)
{
}
finally
{
    listener.Stop();
}

internal sealed record GameServerRegistration(
    string ServerInstanceId,
    string RoomId,
    string MatchId,
    string ListenIp,
    int ListenPort,
    string BuildVersion,
    string RegistrationCredential);

internal sealed record GameServerRegistrationAck(
    string RequestId,
    bool Accepted,
    int HeartbeatIntervalSeconds,
    string HeartbeatCredential,
    ManagedRoomBootstrap? RoomBootstrap);

internal sealed record ManagedRoomBootstrap(
    string RoomId,
    string RoomCode,
    string MatchId,
    string OwnerPlayerId,
    int RoundCount,
    int MaximumPlayers,
    bool PublicRoom,
    bool AutoStart,
    bool PasswordProtected,
    Dictionary<string, object?> RuleSnapshot);

internal sealed record GameServerHeartbeat(
    string RoomId,
    string HeartbeatCredential,
    int ConnectedPlayers,
    string RoomLifecycle,
    int RoundId,
    string BuildVersion,
    DateTimeOffset SentAtUtc);
