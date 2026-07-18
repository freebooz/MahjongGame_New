using System.Collections.Concurrent;
using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using GuiyangMahjong.Allocator.Security;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

public sealed class GameServerInstanceManager
{
    private readonly ConcurrentDictionary<string, GameServerInstance> instances = new(StringComparer.Ordinal);
    private readonly SemaphoreSlim gate = new(1, 1);
    private readonly PortLeasePool ports;
    private readonly InstanceCredentialService credentials;
    private readonly IGameServerProcessLauncher launcher;
    private readonly IInstanceFailureNotifier failureNotifier;
    private readonly AllocatorOptions options;
    private readonly TimeProvider timeProvider;
    private readonly ILogger<GameServerInstanceManager> logger;

    public GameServerInstanceManager(
        PortLeasePool ports,
        InstanceCredentialService credentials,
        IGameServerProcessLauncher launcher,
        IInstanceFailureNotifier failureNotifier,
        IOptions<AllocatorOptions> options,
        TimeProvider timeProvider,
        ILogger<GameServerInstanceManager> logger)
    {
        this.ports = ports;
        this.credentials = credentials;
        this.launcher = launcher;
        this.failureNotifier = failureNotifier;
        this.options = options.Value;
        this.timeProvider = timeProvider;
        this.logger = logger;
    }

    public async Task<AllocationResponse> AllocateAsync(
        string requestId,
        AllocationRequest request,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(request.RoomId) || string.IsNullOrWhiteSpace(request.MatchId))
        {
            throw new AllocatorOperationException("RoomId and MatchId are required.", 400);
        }

        await gate.WaitAsync(cancellationToken);
        try
        {
            var existing = instances.Values.FirstOrDefault(instance =>
                instance.RoomId == request.RoomId
                && instance.State is not GameServerInstanceState.Stopped and not GameServerInstanceState.Failed);
            if (existing is not null) return ToAllocationResponse(requestId, existing);

            var port = ports.Acquire();
            var registration = credentials.Generate();
            var now = timeProvider.GetUtcNow();
            var instance = new GameServerInstance
            {
                ServerInstanceId = Guid.NewGuid().ToString(),
                RoomId = request.RoomId,
                MatchId = request.MatchId,
                Port = port,
                AdvertisedIp = options.AdvertisedIp,
                RegistrationCredentialHash = registration.Hash,
                RegistrationExpireAtUtc = now.AddSeconds(options.RegistrationTimeoutSeconds),
                StartedAtUtc = now,
                BuildVersion = request.BuildVersion
            };
            instances[instance.ServerInstanceId] = instance;

            try
            {
                instance.Process = await launcher.LaunchAsync(new GameServerLaunchSpec(
                    instance.RoomId,
                    instance.MatchId,
                    instance.ServerInstanceId,
                    instance.Port,
                    options.LobbyInternalUrl,
                    registration.Plaintext,
                    options.JoinTicketSigningKey,
                    request.BuildVersion,
                    instance.AdvertisedIp), cancellationToken);
            }
            catch
            {
                await MarkFailedAsync(instance, "Process launch failed", cancellationToken);
                throw;
            }

            return ToAllocationResponse(requestId, instance);
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task<ConfirmRegistrationResponse> ConfirmRegistrationAsync(
        string requestId,
        string serverInstanceId,
        ConfirmRegistrationRequest request,
        CancellationToken cancellationToken)
    {
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!instances.TryGetValue(serverInstanceId, out var instance))
                throw new AllocatorOperationException("GameServer instance was not found.", 404);
            if (instance.State != GameServerInstanceState.Starting)
                throw new AllocatorOperationException("GameServer cannot register in its current state.", 409);
            if (timeProvider.GetUtcNow() >= instance.RegistrationExpireAtUtc)
                throw new AllocatorOperationException("GameServer registration credential expired.", 401);
            if (instance.RoomId != request.RoomId
                || instance.Port != request.ListenPort
                || instance.AdvertisedIp != request.ListenIp
                || instance.BuildVersion != request.BuildVersion)
                throw new AllocatorOperationException("Registration does not match the allocation.", 400);
            if (!credentials.Verify(request.RegistrationCredential, instance.RegistrationCredentialHash))
                throw new AllocatorOperationException("GameServer registration credential is invalid.", 401);

            instance.RegistrationCredentialHash = [];
            InstanceStateMachine.Transition(instance, GameServerInstanceState.Ready);
            var heartbeat = credentials.Generate();
            instance.HeartbeatCredentialHash = heartbeat.Hash;
            instance.RegisteredAtUtc = timeProvider.GetUtcNow();
            instance.LastHeartbeatAtUtc = instance.RegisteredAtUtc;
            InstanceStateMachine.Transition(instance, GameServerInstanceState.Allocated);
            logger.LogInformation(
                "GameServer registered InstanceId={InstanceId} RoomId={RoomId} Port={Port}",
                instance.ServerInstanceId,
                instance.RoomId,
                instance.Port);
            return new ConfirmRegistrationResponse(
                requestId,
                instance.ServerInstanceId,
                true,
                options.HeartbeatIntervalSeconds,
                heartbeat.Plaintext);
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task RecordHeartbeatAsync(
        string serverInstanceId,
        InstanceHeartbeatRequest request,
        CancellationToken cancellationToken)
    {
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!instances.TryGetValue(serverInstanceId, out var instance))
                throw new AllocatorOperationException("GameServer instance was not found.", 404);
            if (instance.State != GameServerInstanceState.Allocated || instance.RoomId != request.RoomId)
                throw new AllocatorOperationException("GameServer cannot heartbeat in its current state.", 409);
            if (instance.HeartbeatCredentialHash is null
                || !credentials.Verify(request.HeartbeatCredential, instance.HeartbeatCredentialHash))
                throw new AllocatorOperationException("GameServer heartbeat credential is invalid.", 401);
            instance.LastHeartbeatAtUtc = timeProvider.GetUtcNow();
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task<GameServerInstanceSnapshot> DrainAsync(
        string serverInstanceId,
        CancellationToken cancellationToken)
    {
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!instances.TryGetValue(serverInstanceId, out var instance))
                throw new AllocatorOperationException("GameServer instance was not found.", 404);
            if (instance.State == GameServerInstanceState.Stopped) return instance.Snapshot();
            if (instance.State == GameServerInstanceState.Failed)
            {
                InstanceStateMachine.Transition(instance, GameServerInstanceState.Stopped);
                ReleasePort(instance);
                return instance.Snapshot();
            }
            if (instance.State != GameServerInstanceState.Allocated)
                throw new AllocatorOperationException("Only allocated instances can drain.", 409);

            InstanceStateMachine.Transition(instance, GameServerInstanceState.Draining);
            if (instance.Process is not null)
            {
                await instance.Process.StopAsync(
                    TimeSpan.FromSeconds(options.DrainGraceSeconds), cancellationToken);
            }
            InstanceStateMachine.Transition(instance, GameServerInstanceState.Stopped);
            ReleasePort(instance);
            logger.LogInformation(
                "GameServer stopped and port returned InstanceId={InstanceId} Port={Port}",
                instance.ServerInstanceId,
                instance.Port);
            return instance.Snapshot();
        }
        finally
        {
            gate.Release();
        }
    }

    public GameServerInstanceSnapshot? Get(string serverInstanceId) =>
        instances.TryGetValue(serverInstanceId, out var instance) ? instance.Snapshot() : null;

    public IReadOnlyList<GameServerInstanceSnapshot> List() =>
        instances.Values.Select(instance => instance.Snapshot()).OrderBy(x => x.Port).ToArray();

    public async Task MonitorAsync(CancellationToken cancellationToken)
    {
        var failures = new List<InstanceFailureNotification>();
        await gate.WaitAsync(cancellationToken);
        try
        {
            var now = timeProvider.GetUtcNow();
            foreach (var instance in instances.Values)
            {
                string? reason = null;
                if (instance.State == GameServerInstanceState.Starting
                    && (instance.Process?.HasExited == true || now >= instance.RegistrationExpireAtUtc))
                {
                    reason = instance.Process?.HasExited == true
                        ? "Process exited before registration"
                        : "Registration timed out";
                }
                else if (instance.State == GameServerInstanceState.Allocated
                    && (instance.Process?.HasExited == true
                        || instance.LastHeartbeatAtUtc is null
                        || now - instance.LastHeartbeatAtUtc.Value
                            >= TimeSpan.FromSeconds(options.HeartbeatTimeoutSeconds)))
                {
                    reason = instance.Process?.HasExited == true
                        ? "GameServer process exited"
                        : "Heartbeat timed out";
                }

                if (reason is null) continue;
                await MarkFailedAsync(instance, reason, cancellationToken);
                failures.Add(new InstanceFailureNotification(
                    instance.ServerInstanceId,
                    instance.RoomId,
                    reason));
            }
        }
        finally
        {
            gate.Release();
        }

        foreach (var failure in failures)
        {
            await failureNotifier.NotifyAsync(failure, cancellationToken);
        }
    }

    private async Task MarkFailedAsync(
        GameServerInstance instance,
        string reason,
        CancellationToken cancellationToken)
    {
        if (instance.State is GameServerInstanceState.Failed or GameServerInstanceState.Stopped) return;
        if (instance.Process is { HasExited: false })
        {
            await instance.Process.StopAsync(TimeSpan.Zero, cancellationToken);
        }
        InstanceStateMachine.Transition(instance, GameServerInstanceState.Failed);
        instance.FailureReason = reason;
        ReleasePort(instance);
        logger.LogWarning(
            "GameServer failed InstanceId={InstanceId} RoomId={RoomId} Reason={Reason}",
            instance.ServerInstanceId,
            instance.RoomId,
            reason);
    }

    private void ReleasePort(GameServerInstance instance)
    {
        if (instance.PortReleased) return;
        ports.Release(instance.Port);
        instance.PortReleased = true;
    }

    private static AllocationResponse ToAllocationResponse(string requestId, GameServerInstance instance) => new(
        requestId,
        instance.RoomId,
        instance.ServerInstanceId,
        instance.Port,
        instance.State);
}
