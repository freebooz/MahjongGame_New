using System.ComponentModel.DataAnnotations;

namespace GuiyangMahjong.Allocator.Options;

public sealed class AllocatorOptions
{
    public const string SectionName = "Allocator";

    [Range(1024, 65535)] public int PortStart { get; init; } = 19000;
    [Range(1024, 65535)] public int PortEnd { get; init; } = 19099;
    [Range(5, 300)] public int RegistrationTimeoutSeconds { get; init; } = 30;
    [Range(3, 300)] public int HeartbeatTimeoutSeconds { get; init; } = 15;
    [Range(1, 60)] public int HeartbeatIntervalSeconds { get; init; } = 3;
    [Range(100, 60_000)] public int MonitorIntervalMilliseconds { get; init; } = 500;
    [Range(0, 60)] public int DrainGraceSeconds { get; init; } = 3;
    [Required] public string AdvertisedIp { get; init; } = "127.0.0.1";
    public string GameServerExecutablePath { get; init; } = string.Empty;
    public string GameServerWorkingDirectory { get; init; } = string.Empty;
    public string[] GameServerPrefixArguments { get; init; } = [];
    [Required] public string MatchResultOutboxDirectory { get; init; } = "match-result-outbox";
    [Required] public string StateFilePath { get; init; } = "allocator-state/instances.json";
    [Range(1, 60)] public int StateCheckpointSeconds { get; init; } = 5;
    [Range(1, 300)] public int FailureNotificationRetrySeconds { get; init; } = 5;
    [Range(1, 300)] public int MatchResultRecoveryDelaySeconds { get; init; } = 15;
    [Required, Url] public string LobbyInternalUrl { get; init; } = "http://127.0.0.1:18080";
    [MinLength(32)] public string ServiceToken { get; init; } = string.Empty;
    [MinLength(32)] public string LobbyCallbackToken { get; init; } = string.Empty;
    [MinLength(32)] public string JoinTicketSigningKey { get; init; } = string.Empty;
}
