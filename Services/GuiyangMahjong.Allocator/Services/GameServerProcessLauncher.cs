using System.Diagnostics;
using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

/// <summary>Launches a server without shell parsing and never logs credentials.</summary>
public sealed class GameServerProcessLauncher(
    IOptions<AllocatorOptions> options,
    ILogger<GameServerProcessLauncher> logger) : IGameServerProcessLauncher
{
    private readonly AllocatorOptions options = options.Value;

    public Task<IManagedGameServerProcess> LaunchAsync(
        GameServerLaunchSpec spec,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (string.IsNullOrWhiteSpace(options.GameServerExecutablePath))
        {
            throw new InvalidOperationException("GameServerExecutablePath is not configured.");
        }
        Directory.CreateDirectory(Path.GetDirectoryName(spec.MatchResultOutboxPath)
            ?? throw new InvalidOperationException("MatchResultOutboxPath has no parent directory."));

        var startInfo = new ProcessStartInfo
        {
            FileName = options.GameServerExecutablePath,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = AppContext.BaseDirectory
        };
        foreach (var argument in options.GameServerPrefixArguments) startInfo.ArgumentList.Add(argument);
        startInfo.ArgumentList.Add("-MahjongManagedGameServer");
        startInfo.ArgumentList.Add($"-RoomId={spec.RoomId}");
        startInfo.ArgumentList.Add($"-MatchId={spec.MatchId}");
        startInfo.ArgumentList.Add($"-ServerInstanceId={spec.ServerInstanceId}");
        startInfo.ArgumentList.Add($"-Port={spec.Port}");
        startInfo.ArgumentList.Add($"-LobbyInternalUrl={spec.LobbyInternalUrl}");
        startInfo.ArgumentList.Add($"-BuildVersion={spec.BuildVersion}");
        startInfo.ArgumentList.Add($"-AdvertisedIp={spec.AdvertisedIp}");
        startInfo.Environment["MAHJONG_REGISTRATION_CREDENTIAL"] = spec.RegistrationCredential;
        startInfo.Environment["MAHJONG_JOIN_TICKET_SIGNING_KEY"] = spec.JoinTicketSigningKey;
        startInfo.Environment["MAHJONG_MATCH_RESULT_OUTBOX_PATH"] = spec.MatchResultOutboxPath;

        var process = Process.Start(startInfo)
            ?? throw new InvalidOperationException("GameServer process failed to start.");
        logger.LogInformation(
            "GameServer started InstanceId={InstanceId} RoomId={RoomId} Port={Port} ProcessId={ProcessId}",
            spec.ServerInstanceId,
            spec.RoomId,
            spec.Port,
            process.Id);
        return Task.FromResult<IManagedGameServerProcess>(new ManagedGameServerProcess(process));
    }

    private sealed class ManagedGameServerProcess(Process process) : IManagedGameServerProcess
    {
        public int ProcessId => process.Id;

        public bool HasExited
        {
            get
            {
                try { return process.HasExited; }
                catch (InvalidOperationException) { return true; }
            }
        }

        public async ValueTask StopAsync(TimeSpan gracePeriod, CancellationToken cancellationToken)
        {
            if (HasExited) return;
            if (gracePeriod > TimeSpan.Zero)
            {
                try
                {
                    using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
                    timeout.CancelAfter(gracePeriod);
                    await process.WaitForExitAsync(timeout.Token);
                    return;
                }
                catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
                {
                }
            }

            if (!HasExited) process.Kill(entireProcessTree: true);
            await process.WaitForExitAsync(cancellationToken);
            process.Dispose();
        }
    }
}
