using System.Collections.Concurrent;
using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using GuiyangMahjong.Allocator.Security;
using GuiyangMahjong.Allocator.Services;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Tests;

public sealed class GameServerInstanceManagerTests
{
    [Fact]
    public async Task ConcurrentAllocations_UseUniquePortsAndProcesses()
    {
        var fixture = CreateFixture(19000, 19009);
        var allocations = await Task.WhenAll(Enumerable.Range(0, 10).Select(index =>
            fixture.Manager.AllocateAsync(
                Guid.NewGuid().ToString(),
                new AllocationRequest($"room-{index}", $"match-{index}", "test"),
                CancellationToken.None)));

        Assert.Equal(10, allocations.Select(x => x.Port).Distinct().Count());
        Assert.Equal(10, allocations.Select(x => x.ServerInstanceId).Distinct().Count());
        Assert.Equal(0, fixture.Ports.AvailableCount);
    }

    [Fact]
    public async Task RegistrationCredential_IsOneTimeAndInvalidCredentialIsRejected()
    {
        var fixture = CreateFixture();
        var allocation = await AllocateAsync(fixture, "room-registration");
        var launch = fixture.Launcher.Specs[allocation.ServerInstanceId];

        await Assert.ThrowsAsync<AllocatorOperationException>(() => fixture.Manager.ConfirmRegistrationAsync(
            Guid.NewGuid().ToString(),
            allocation.ServerInstanceId,
            NewRegistration(launch, "invalid"),
            CancellationToken.None));

        var acknowledged = await fixture.Manager.ConfirmRegistrationAsync(
            Guid.NewGuid().ToString(),
            allocation.ServerInstanceId,
            NewRegistration(launch, launch.RegistrationCredential),
            CancellationToken.None);

        Assert.True(acknowledged.Accepted);
        Assert.NotEmpty(acknowledged.HeartbeatCredential);
        await Assert.ThrowsAsync<AllocatorOperationException>(() => fixture.Manager.ConfirmRegistrationAsync(
            Guid.NewGuid().ToString(),
            allocation.ServerInstanceId,
            NewRegistration(launch, launch.RegistrationCredential),
            CancellationToken.None));
    }

    [Fact]
    public async Task HeartbeatTimeout_StopsProcessBeforePortIsReclaimed()
    {
        var fixture = CreateFixture(19100, 19100);
        var allocation = await AllocateAsync(fixture, "room-timeout");
        var launch = fixture.Launcher.Specs[allocation.ServerInstanceId];
        await fixture.Manager.ConfirmRegistrationAsync(
            Guid.NewGuid().ToString(),
            allocation.ServerInstanceId,
            NewRegistration(launch, launch.RegistrationCredential),
            CancellationToken.None);

        fixture.Time.Advance(TimeSpan.FromSeconds(16));
        await fixture.Manager.MonitorAsync(CancellationToken.None);

        var process = fixture.Launcher.Processes[allocation.ServerInstanceId];
        var snapshot = fixture.Manager.Get(allocation.ServerInstanceId);
        Assert.True(process.HasExited);
        Assert.Equal(GameServerInstanceState.Failed, snapshot?.State);
        Assert.Equal(1, fixture.Ports.AvailableCount);
        Assert.Single(fixture.Notifier.Failures);
    }

    [Fact]
    public async Task Drain_StopsInstanceAndMakesPortReusable()
    {
        var fixture = CreateFixture(19200, 19200);
        var first = await AllocateAsync(fixture, "room-drain");
        var launch = fixture.Launcher.Specs[first.ServerInstanceId];
        await fixture.Manager.ConfirmRegistrationAsync(
            Guid.NewGuid().ToString(),
            first.ServerInstanceId,
            NewRegistration(launch, launch.RegistrationCredential),
            CancellationToken.None);

        var stopped = await fixture.Manager.DrainAsync(first.ServerInstanceId, CancellationToken.None);
        var second = await AllocateAsync(fixture, "room-after-drain");

        Assert.Equal(GameServerInstanceState.Stopped, stopped.State);
        Assert.Equal(first.Port, second.Port);
        Assert.True(fixture.Launcher.Processes[first.ServerInstanceId].HasExited);
    }

    private static Task<AllocationResponse> AllocateAsync(Fixture fixture, string roomId) =>
        fixture.Manager.AllocateAsync(
            Guid.NewGuid().ToString(),
            new AllocationRequest(roomId, $"match-{roomId}", "test"),
            CancellationToken.None);

    private static ConfirmRegistrationRequest NewRegistration(GameServerLaunchSpec spec, string credential) => new(
        spec.RoomId,
        spec.AdvertisedIp,
        spec.Port,
        spec.BuildVersion,
        credential);

    private static Fixture CreateFixture(int portStart = 19000, int portEnd = 19010)
    {
        var allocatorOptions = Microsoft.Extensions.Options.Options.Create(new AllocatorOptions
        {
            PortStart = portStart,
            PortEnd = portEnd,
            RegistrationTimeoutSeconds = 30,
            HeartbeatTimeoutSeconds = 15,
            HeartbeatIntervalSeconds = 3,
            AdvertisedIp = "127.0.0.1",
            LobbyInternalUrl = "http://127.0.0.1:18080",
            ServiceToken = "test-only-allocator-service-token-long-enough",
            LobbyCallbackToken = "test-only-lobby-callback-token-long-enough"
        });
        var ports = new PortLeasePool(allocatorOptions);
        var launcher = new FakeProcessLauncher();
        var notifier = new RecordingFailureNotifier();
        var time = new MutableTimeProvider(DateTimeOffset.Parse("2026-07-18T00:00:00Z"));
        var manager = new GameServerInstanceManager(
            ports,
            new InstanceCredentialService(),
            launcher,
            notifier,
            allocatorOptions,
            time,
            NullLogger<GameServerInstanceManager>.Instance);
        return new Fixture(manager, ports, launcher, notifier, time);
    }

    private sealed record Fixture(
        GameServerInstanceManager Manager,
        PortLeasePool Ports,
        FakeProcessLauncher Launcher,
        RecordingFailureNotifier Notifier,
        MutableTimeProvider Time);

    private sealed class FakeProcessLauncher : IGameServerProcessLauncher
    {
        public ConcurrentDictionary<string, GameServerLaunchSpec> Specs { get; } = new();
        public ConcurrentDictionary<string, FakeManagedProcess> Processes { get; } = new();

        public Task<IManagedGameServerProcess> LaunchAsync(
            GameServerLaunchSpec spec,
            CancellationToken cancellationToken)
        {
            Specs[spec.ServerInstanceId] = spec;
            var process = new FakeManagedProcess(Processes.Count + 1000);
            Processes[spec.ServerInstanceId] = process;
            return Task.FromResult<IManagedGameServerProcess>(process);
        }
    }

    private sealed class FakeManagedProcess(int processId) : IManagedGameServerProcess
    {
        public int ProcessId { get; } = processId;
        public bool HasExited { get; private set; }

        public ValueTask StopAsync(TimeSpan gracePeriod, CancellationToken cancellationToken)
        {
            HasExited = true;
            return ValueTask.CompletedTask;
        }
    }

    private sealed class RecordingFailureNotifier : IInstanceFailureNotifier
    {
        public List<InstanceFailureNotification> Failures { get; } = [];
        public Task NotifyAsync(InstanceFailureNotification notification, CancellationToken cancellationToken)
        {
            Failures.Add(notification);
            return Task.CompletedTask;
        }
    }

    private sealed class MutableTimeProvider(DateTimeOffset current) : TimeProvider
    {
        public override DateTimeOffset GetUtcNow() => current;
        public void Advance(TimeSpan duration) => current += duration;
    }
}
