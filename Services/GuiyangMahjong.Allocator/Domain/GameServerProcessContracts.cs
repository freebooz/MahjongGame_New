namespace GuiyangMahjong.Allocator.Domain;

public sealed record GameServerLaunchSpec(
    string RoomId,
    string MatchId,
    string ServerInstanceId,
    int Port,
    string LobbyInternalUrl,
    string RegistrationCredential,
    string JoinTicketSigningKey,
    string BuildVersion,
    string AdvertisedIp);

public interface IManagedGameServerProcess
{
    int ProcessId { get; }
    bool HasExited { get; }
    ValueTask StopAsync(TimeSpan gracePeriod, CancellationToken cancellationToken);
}

public interface IGameServerProcessLauncher
{
    Task<IManagedGameServerProcess> LaunchAsync(
        GameServerLaunchSpec spec,
        CancellationToken cancellationToken);
}
