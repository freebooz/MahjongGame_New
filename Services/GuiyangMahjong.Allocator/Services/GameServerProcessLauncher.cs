using System.Diagnostics;
using System.ComponentModel;
using System.Runtime.InteropServices;
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
        var executablePath = ResolveExecutablePath(options.GameServerExecutablePath);
        EnsureExecutable(executablePath);
        var workingDirectory = ResolveWorkingDirectory(executablePath);
        Directory.CreateDirectory(Path.GetDirectoryName(spec.MatchResultOutboxPath)
            ?? throw new InvalidOperationException("MatchResultOutboxPath has no parent directory."));

        var startInfo = new ProcessStartInfo
        {
            FileName = OperatingSystem.IsLinux() ? ResolveSetSidPath() : executablePath,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = workingDirectory
        };
        if (OperatingSystem.IsLinux()) startInfo.ArgumentList.Add(executablePath);
        foreach (var argument in options.GameServerPrefixArguments)
        {
            if (!string.IsNullOrWhiteSpace(argument)) startInfo.ArgumentList.Add(argument.Trim());
        }
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

    public Task<IManagedGameServerProcess?> TryAttachAsync(
        int processId,
        DateTimeOffset expectedStartedAtUtc,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        try
        {
            var process = Process.GetProcessById(processId);
            var managed = new ManagedGameServerProcess(process);
            if (managed.HasExited
                || Math.Abs((managed.StartedAtUtc - expectedStartedAtUtc).TotalSeconds) > 2)
            {
                process.Dispose();
                return Task.FromResult<IManagedGameServerProcess?>(null);
            }

            try
            {
                var configured = CanonicalizeExistingPath(ResolveExecutablePath(options.GameServerExecutablePath));
                var actual = process.MainModule?.FileName;
                if (string.IsNullOrWhiteSpace(actual)
                    || !string.Equals(
                        configured,
                        CanonicalizeExistingPath(actual),
                        OperatingSystem.IsWindows()
                            ? StringComparison.OrdinalIgnoreCase
                            : StringComparison.Ordinal))
                {
                    process.Dispose();
                    return Task.FromResult<IManagedGameServerProcess?>(null);
                }
            }
            catch (Exception exception) when (exception is System.ComponentModel.Win32Exception
                                               or InvalidOperationException
                                               or NotSupportedException
                                               or IOException
                                               or UnauthorizedAccessException)
            {
                logger.LogWarning(
                    exception,
                    "Refusing to recover GameServer because its executable path could not be verified ProcessId={ProcessId}",
                    processId);
                process.Dispose();
                return Task.FromResult<IManagedGameServerProcess?>(null);
            }

            return Task.FromResult<IManagedGameServerProcess?>(managed);
        }
        catch (Exception exception) when (exception is ArgumentException
                                           or InvalidOperationException
                                           or System.ComponentModel.Win32Exception)
        {
            return Task.FromResult<IManagedGameServerProcess?>(null);
        }
    }

    public string GetResolvedExecutablePath() => ResolveExecutablePath(options.GameServerExecutablePath);

    public string GetResolvedWorkingDirectory() => ResolveWorkingDirectory(GetResolvedExecutablePath());

    public static bool IsExecutable(string path)
    {
        if (!File.Exists(path)) return false;
        if (!OperatingSystem.IsLinux()) return true;
        var mode = File.GetUnixFileMode(path);
        const UnixFileMode executeBits = UnixFileMode.UserExecute
                                         | UnixFileMode.GroupExecute
                                         | UnixFileMode.OtherExecute;
        return (mode & executeBits) != 0;
    }

    private string ResolveWorkingDirectory(string executablePath)
    {
        var configured = options.GameServerWorkingDirectory.Trim();
        var path = string.IsNullOrEmpty(configured)
            ? Path.GetDirectoryName(executablePath)
            : Path.IsPathRooted(configured)
                ? configured
                : Path.Combine(AppContext.BaseDirectory, configured);
        path = Path.GetFullPath(path
            ?? throw new InvalidOperationException("GameServer executable has no parent directory."));
        if (!Directory.Exists(path))
            throw new DirectoryNotFoundException($"GameServer working directory does not exist: {path}");
        return path;
    }

    private static void EnsureExecutable(string path)
    {
        if (!File.Exists(path)) throw new FileNotFoundException("GameServer executable does not exist.", path);
        if (!IsExecutable(path))
            throw new InvalidOperationException($"GameServer file is not executable: {path}");
    }

    private static string ResolveExecutablePath(string configured)
    {
        configured = configured.Trim();
        if (string.IsNullOrEmpty(configured))
            throw new InvalidOperationException("GameServerExecutablePath is not configured.");
        if (Path.IsPathRooted(configured)) return Path.GetFullPath(configured);
        if (configured.Contains(Path.DirectorySeparatorChar)
            || configured.Contains(Path.AltDirectorySeparatorChar))
            return Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, configured));

        var pathValue = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        foreach (var directory in pathValue.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            var candidate = Path.Combine(directory, configured);
            if (File.Exists(candidate)) return Path.GetFullPath(candidate);
            if (OperatingSystem.IsWindows() && Path.GetExtension(candidate).Length == 0)
            {
                foreach (var extension in new[] { ".exe", ".cmd", ".bat" })
                {
                    if (File.Exists(candidate + extension)) return Path.GetFullPath(candidate + extension);
                }
            }
        }
        throw new FileNotFoundException($"GameServer executable was not found on PATH: {configured}");
    }

    private static string CanonicalizeExistingPath(string path)
    {
        var fullPath = Path.GetFullPath(path);
        var target = new FileInfo(fullPath).ResolveLinkTarget(returnFinalTarget: true);
        return target is null ? fullPath : Path.GetFullPath(target.FullName);
    }

    private static string ResolveSetSidPath()
    {
        foreach (var candidate in new[] { "/usr/bin/setsid", "/bin/setsid" })
            if (File.Exists(candidate)) return candidate;
        throw new FileNotFoundException("Linux GameServer launch requires util-linux setsid.");
    }

    private sealed class ManagedGameServerProcess : IManagedGameServerProcess
    {
        private readonly Process process;

        public ManagedGameServerProcess(Process process)
        {
            this.process = process;
            ProcessId = process.Id;
            StartedAtUtc = process.StartTime.ToUniversalTime();
        }

        public int ProcessId { get; }
        public DateTimeOffset StartedAtUtc { get; }

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
            if (HasExited)
            {
                process.Dispose();
                return;
            }
            if (OperatingSystem.IsLinux()) SendLinuxProcessGroupSignal(ProcessId, 15);
            if (gracePeriod > TimeSpan.Zero)
            {
                try
                {
                    using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
                    timeout.CancelAfter(gracePeriod);
                    await process.WaitForExitAsync(timeout.Token);
                    process.Dispose();
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

    private static void SendLinuxProcessGroupSignal(int processId, int signal)
    {
        if (kill(-processId, signal) == 0) return;
        var error = Marshal.GetLastPInvokeError();
        if (error == 3) return; // ESRCH: the process already exited.
        throw new Win32Exception(error, $"Could not signal GameServer process group {processId}.");
    }

    [DllImport("libc", SetLastError = true)]
    private static extern int kill(int processId, int signal);
}
