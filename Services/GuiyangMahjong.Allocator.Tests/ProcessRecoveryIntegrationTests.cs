using System.Diagnostics;
using System.Net;
using System.Net.Http.Json;
using System.Net.Sockets;
using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using GuiyangMahjong.Allocator.Security;
using GuiyangMahjong.Allocator.Services;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Tests;

public sealed class ProcessRecoveryIntegrationTests
{
    [Fact]
    [Trait("Category", "ProcessIntegration")]
    public async Task Restart_ReattachesRealProcessAndKeepsPortLeased()
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(45));
        var gamePort = FindFreePort();
        var lobbyPort = FindFreePort();
        while (lobbyPort == gamePort) lobbyPort = FindFreePort();
        var temporaryDirectory = Path.Combine(
            Path.GetTempPath(), $"guiyang-allocator-process-{Guid.NewGuid():N}");
        var statePath = Path.Combine(temporaryDirectory, "allocator-state.json");
        var options = CreateOptions(gamePort, lobbyPort, statePath, FindFakeGameServerExecutable());
        var app = CreateLobbyStub(lobbyPort);
        GameServerInstanceManager? activeManager = null;
        GameServerInstanceManager? originalManager = null;
        GameServerInstanceManager? restartedManager = null;
        AllocationResponse? allocation = null;
        int? launchedProcessId = null;

        app.MapPost("/internal/gameservers/register", async (HttpContext context) =>
        {
            var registration = await context.Request.ReadFromJsonAsync<FakeRegistration>(
                cancellationToken: context.RequestAborted)
                ?? throw new InvalidDataException("Fake registration body is empty.");
            var manager = Volatile.Read(ref activeManager)
                ?? throw new InvalidOperationException("Allocator manager is not available.");
            var acknowledgement = await manager.ConfirmRegistrationAsync(
                Guid.NewGuid().ToString("N"),
                registration.ServerInstanceId,
                new ConfirmRegistrationRequest(
                    registration.RoomId,
                    registration.ListenIp,
                    registration.ListenPort,
                    registration.BuildVersion,
                    registration.RegistrationCredential),
                context.RequestAborted);
            return Results.Ok(new
            {
                requestId = Guid.NewGuid().ToString("N"),
                accepted = acknowledgement.Accepted,
                heartbeatIntervalSeconds = acknowledgement.HeartbeatIntervalSeconds,
                heartbeatCredential = acknowledgement.HeartbeatCredential,
                roomBootstrap = new
                {
                    roomId = registration.RoomId,
                    roomCode = "810005",
                    matchId = registration.MatchId,
                    ownerPlayerId = "process-owner",
                    roundCount = 4,
                    maximumPlayers = 4,
                    publicRoom = true,
                    autoStart = true,
                    passwordProtected = false,
                    ruleSnapshot = new Dictionary<string, object?>
                    {
                        ["ruleId"] = "GuiyangMainstreamV1"
                    }
                }
            });
        });
        app.MapPost("/internal/gameservers/{serverInstanceId}/heartbeat", async (
            string serverInstanceId,
            HttpContext context) =>
        {
            var heartbeat = await context.Request.ReadFromJsonAsync<FakeHeartbeat>(
                cancellationToken: context.RequestAborted)
                ?? throw new InvalidDataException("Fake heartbeat body is empty.");
            var manager = Volatile.Read(ref activeManager);
            if (manager is not null)
            {
                await manager.RecordHeartbeatAsync(
                    serverInstanceId,
                    new InstanceHeartbeatRequest(
                        heartbeat.RoomId,
                        heartbeat.HeartbeatCredential,
                        heartbeat.ConnectedPlayers,
                        heartbeat.RoomLifecycle,
                        heartbeat.RoundId,
                        heartbeat.BuildVersion,
                        heartbeat.SentAtUtc),
                    context.RequestAborted);
            }
            return Results.NoContent();
        });

        try
        {
            await app.StartAsync(timeout.Token);
            var originalPorts = new PortLeasePool(options);
            var originalState = CreateStateStore(options);
            originalManager = CreateManager(originalPorts, originalState, options);
            activeManager = originalManager;
            await originalManager.InitializeAsync(timeout.Token);
            allocation = await originalManager.AllocateAsync(
                Guid.NewGuid().ToString("N"),
                new AllocationRequest("process-room", "process-match", "fake-process-integration"),
                timeout.Token);
            await WaitForStateAsync(
                originalManager,
                allocation.ServerInstanceId,
                GameServerInstanceState.Allocated,
                timeout.Token);
            var durableState = await originalState.LoadAsync(timeout.Token);
            Assert.Contains(durableState.Instances, instance =>
                instance.ServerInstanceId == allocation.ServerInstanceId
                && instance.State == GameServerInstanceState.Allocated);
            var originalSnapshot = originalManager.Get(allocation.ServerInstanceId);
            Assert.NotNull(originalSnapshot?.ProcessId);
            launchedProcessId = originalSnapshot!.ProcessId;

            Volatile.Write(ref activeManager, null);
            var restartedPorts = new PortLeasePool(options);
            var restartedState = CreateStateStore(options);
            restartedManager = CreateManager(restartedPorts, restartedState, options);
            await restartedManager.InitializeAsync(timeout.Token);
            Volatile.Write(ref activeManager, restartedManager);

            var recovered = restartedManager.Get(allocation.ServerInstanceId);
            Assert.True(restartedManager.IsInitialized);
            Assert.Equal(GameServerInstanceState.Allocated, recovered?.State);
            Assert.Equal(originalSnapshot!.ProcessId, recovered?.ProcessId);
            Assert.Equal(0, restartedPorts.AvailableCount);
            using (var process = Process.GetProcessById(recovered!.ProcessId!.Value))
                Assert.False(process.HasExited);
            await Assert.ThrowsAsync<AllocatorOperationException>(() => restartedManager.AllocateAsync(
                Guid.NewGuid().ToString("N"),
                new AllocationRequest("second-room", "second-match", "fake-process-integration"),
                timeout.Token));

            var stopped = await restartedManager.DrainAsync(allocation.ServerInstanceId, timeout.Token);
            Assert.Equal(GameServerInstanceState.Stopped, stopped.State);
            Assert.Equal(1, restartedPorts.AvailableCount);
            await AssertProcessExitedAsync(originalSnapshot.ProcessId.Value, timeout.Token);
        }
        finally
        {
            Volatile.Write(ref activeManager, null);
            var cleanupManager = restartedManager ?? originalManager;
            try
            {
                if (allocation is not null && cleanupManager?.Get(allocation.ServerInstanceId) is
                    { State: not GameServerInstanceState.Stopped and not GameServerInstanceState.Failed })
                    await cleanupManager.DrainAsync(allocation.ServerInstanceId, CancellationToken.None);
            }
            catch (Exception) { }
            if (launchedProcessId is not null)
            {
                try
                {
                    using var process = Process.GetProcessById(launchedProcessId.Value);
                    if (!process.HasExited) process.Kill(entireProcessTree: true);
                    await process.WaitForExitAsync(CancellationToken.None);
                }
                catch (Exception exception) when (exception is ArgumentException or InvalidOperationException) { }
            }
            await app.StopAsync(CancellationToken.None);
            await app.DisposeAsync();
            if (Directory.Exists(temporaryDirectory)) Directory.Delete(temporaryDirectory, true);
        }
    }

    private static WebApplication CreateLobbyStub(int port)
    {
        var builder = WebApplication.CreateBuilder(new WebApplicationOptions
        {
            EnvironmentName = "Development"
        });
        builder.Logging.ClearProviders();
        builder.WebHost.UseUrls($"http://127.0.0.1:{port}");
        return builder.Build();
    }

    private static IOptions<AllocatorOptions> CreateOptions(
        int gamePort,
        int lobbyPort,
        string statePath,
        string executablePath) => Microsoft.Extensions.Options.Options.Create(new AllocatorOptions
        {
            PortStart = gamePort,
            PortEnd = gamePort,
            RegistrationTimeoutSeconds = 30,
            HeartbeatTimeoutSeconds = 10,
            HeartbeatIntervalSeconds = 1,
            DrainGraceSeconds = 0,
            AdvertisedIp = "127.0.0.1",
            GameServerExecutablePath = executablePath,
            MatchResultOutboxDirectory = Path.Combine(Path.GetDirectoryName(statePath)!, "outbox"),
            StateFilePath = statePath,
            StateCheckpointSeconds = 1,
            LobbyInternalUrl = $"http://127.0.0.1:{lobbyPort}",
            ServiceToken = "process-test-allocator-service-token-long-enough",
            LobbyCallbackToken = "process-test-lobby-callback-token-long-enough",
            JoinTicketSigningKey = "process-test-join-ticket-signing-key-long-enough"
        });

    private static JsonAllocatorStateStore CreateStateStore(IOptions<AllocatorOptions> options) => new(
        options,
        TimeProvider.System,
        NullLogger<JsonAllocatorStateStore>.Instance);

    private static GameServerInstanceManager CreateManager(
        PortLeasePool ports,
        IAllocatorStateStore stateStore,
        IOptions<AllocatorOptions> options) => new(
            ports,
            new InstanceCredentialService(),
            new GameServerProcessLauncher(options, NullLogger<GameServerProcessLauncher>.Instance),
            new NoOpFailureNotifier(),
            stateStore,
            options,
            TimeProvider.System,
            NullLogger<GameServerInstanceManager>.Instance);

    private static async Task WaitForStateAsync(
        GameServerInstanceManager manager,
        string serverInstanceId,
        GameServerInstanceState expected,
        CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            if (manager.Get(serverInstanceId)?.State == expected) return;
            await Task.Delay(50, cancellationToken);
        }
        throw new TimeoutException($"Instance {serverInstanceId} did not reach {expected}.");
    }

    private static async Task AssertProcessExitedAsync(int processId, CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            try
            {
                using var process = Process.GetProcessById(processId);
                if (process.HasExited) return;
            }
            catch (ArgumentException)
            {
                return;
            }
            await Task.Delay(50, cancellationToken);
        }
        throw new TimeoutException($"Process {processId} did not exit.");
    }

    private static string FindFakeGameServerExecutable()
    {
        var configuredPath = Environment.GetEnvironmentVariable("FAKE_GAME_SERVER_EXECUTABLE");
        if (!string.IsNullOrWhiteSpace(configuredPath))
        {
            var resolvedPath = Path.GetFullPath(configuredPath);
            if (!File.Exists(resolvedPath))
                throw new FileNotFoundException("Configured FakeGameServer apphost was not built.", resolvedPath);
            return resolvedPath;
        }

        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        var configuration = directory.Parent?.Name
            ?? throw new InvalidOperationException("Could not resolve test build configuration.");
        while (directory is not null
               && !File.Exists(Path.Combine(directory.FullName, "GuiyangMahjong.Services.slnx")))
            directory = directory.Parent;
        if (directory is null)
            throw new FileNotFoundException("Could not locate Services solution root.");
        var executableName = OperatingSystem.IsWindows()
            ? "GuiyangMahjong.FakeGameServer.exe"
            : "GuiyangMahjong.FakeGameServer";
        var path = Path.Combine(
            directory.FullName,
            "GuiyangMahjong.FakeGameServer",
            "bin",
            configuration,
            "net10.0",
            executableName);
        if (!File.Exists(path))
            throw new FileNotFoundException("FakeGameServer apphost was not built.", path);
        return path;
    }

    private static int FindFreePort()
    {
        using var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        return ((IPEndPoint)listener.LocalEndpoint).Port;
    }

    private sealed class NoOpFailureNotifier : IInstanceFailureNotifier
    {
        public Task NotifyAsync(
            InstanceFailureNotification notification,
            CancellationToken cancellationToken) => Task.CompletedTask;
    }

    private sealed record FakeRegistration(
        string ServerInstanceId,
        string RoomId,
        string MatchId,
        string ListenIp,
        int ListenPort,
        string BuildVersion,
        string RegistrationCredential);

    private sealed record FakeHeartbeat(
        string RoomId,
        string HeartbeatCredential,
        int ConnectedPlayers,
        string RoomLifecycle,
        int RoundId,
        string BuildVersion,
        DateTimeOffset SentAtUtc);
}
