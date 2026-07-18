using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Options;
using GuiyangMahjong.Lobby.Realtime;
using GuiyangMahjong.Lobby.Security;
using GuiyangMahjong.Lobby.Services;
using GuiyangMahjong.Lobby.Storage;
using Microsoft.Extensions.Logging.Abstractions;

namespace GuiyangMahjong.Lobby.Tests;

public sealed class AllocatorIntegrationDomainTests
{
    [Fact]
    public async Task Route_IsUnavailableUntilRegistrationThenGetsShortLivedTicket()
    {
        var fixture = CreateFixture();
        var owner = new PlayerIdentity("owner-route", "Owner", "Guest");
        var created = await fixture.Service.CreateRoomAsync(
            Guid.NewGuid().ToString(), owner, NewCreateRequest(), CancellationToken.None);

        await Assert.ThrowsAsync<LobbyOperationException>(() => fixture.Service.GetRouteAsync(
            Guid.NewGuid().ToString(), owner, created.RoomCode, CancellationToken.None));

        var room = await fixture.Store.GetRoomByIdAsync(created.RoomId, CancellationToken.None);
        Assert.NotNull(room);
        var registration = new GameServerRegistration(
            fixture.Allocator.ServerInstanceId,
            room.RoomId,
            room.MatchId,
            "127.0.0.1",
            19000,
            "test",
            "one-time-registration-credential");
        await fixture.Service.RegisterGameServerAsync(
            Guid.NewGuid().ToString(), registration, CancellationToken.None);

        var route = await fixture.Service.GetRouteAsync(
            Guid.NewGuid().ToString(), owner, created.RoomCode, CancellationToken.None);
        Assert.Equal(fixture.Allocator.ServerInstanceId, route.ServerInstanceId);
        Assert.NotEmpty(route.JoinTicket);
        Assert.True(route.TicketExpireAtUtc > fixture.Time.GetUtcNow());
    }

    [Fact]
    public async Task AllocatorFailure_RemovesRouteAndFailsRoom()
    {
        var fixture = CreateFixture();
        var owner = new PlayerIdentity("owner-failure", "Owner", "Guest");
        var created = await fixture.Service.CreateRoomAsync(
            Guid.NewGuid().ToString(), owner, NewCreateRequest(), CancellationToken.None);
        var room = await fixture.Store.GetRoomByIdAsync(created.RoomId, CancellationToken.None);
        Assert.NotNull(room);
        await fixture.Service.RegisterGameServerAsync(
            Guid.NewGuid().ToString(),
            new GameServerRegistration(
                fixture.Allocator.ServerInstanceId,
                room.RoomId,
                room.MatchId,
                "127.0.0.1",
                19000,
                "test",
                "credential"),
            CancellationToken.None);

        await fixture.Service.MarkGameServerFailedAsync(new GameServerFailure(
            fixture.Allocator.ServerInstanceId,
            room.RoomId,
            "test crash"), CancellationToken.None);

        var failed = await fixture.Store.GetRoomByIdAsync(room.RoomId, CancellationToken.None);
        Assert.Equal(RoomLifecycle.Failed, failed?.Lifecycle);
        Assert.Null(failed?.Route);
    }

    [Fact]
    public async Task FailureBeforeRegistration_FailsAllocatingRoom()
    {
        var fixture = CreateFixture();
        var created = await fixture.Service.CreateRoomAsync(
            Guid.NewGuid().ToString(),
            new PlayerIdentity("owner-startup-failure", "Owner", "Guest"),
            NewCreateRequest(),
            CancellationToken.None);

        await fixture.Service.MarkGameServerFailedAsync(new GameServerFailure(
            fixture.Allocator.ServerInstanceId,
            created.RoomId,
            "registration timeout"), CancellationToken.None);

        var failed = await fixture.Store.GetRoomByIdAsync(created.RoomId, CancellationToken.None);
        Assert.Equal(RoomLifecycle.Failed, failed?.Lifecycle);
        Assert.Null(failed?.PendingServerInstanceId);
    }

    private static Fixture CreateFixture()
    {
        var options = Microsoft.Extensions.Options.Options.Create(new LobbyOptions
        {
            TokenSigningKey = LobbyWebApplicationFactory.SigningKey,
            JoinTicketSigningKey = "test-only-join-ticket-signing-key-which-is-long-enough",
            InternalServiceToken = "test-only-internal-service-token-which-is-long-enough",
            RoomCodeRetryLimit = 100,
            MaximumPlayersPerRoom = 4,
            Allocator = new AllocatorClientOptions { Enabled = true, GameServerBuildVersion = "test" }
        });
        var store = new InMemoryLobbyStore();
        var allocator = new RecordingAllocatorClient();
        var time = new FixedTimeProvider(DateTimeOffset.Parse("2026-07-18T00:00:00Z"));
        var service = new LobbyService(
            store,
            new RoomPasswordService(options, time),
            new NoOpEventPublisher(),
            allocator,
            new HmacJoinTicketIssuer(options, time),
            options,
            time,
            NullLogger<LobbyService>.Instance);
        return new Fixture(service, store, allocator, time);
    }

    private static CreateRoomRequest NewCreateRequest() => new(
        4,
        true,
        true,
        false,
        null,
        new Dictionary<string, object?> { ["ruleId"] = "GuiyangMainstreamV1" });

    private sealed record Fixture(
        LobbyService Service,
        InMemoryLobbyStore Store,
        RecordingAllocatorClient Allocator,
        FixedTimeProvider Time);

    private sealed class RecordingAllocatorClient : IAllocatorClient
    {
        public string ServerInstanceId { get; } = Guid.NewGuid().ToString();
        public bool Enabled => true;

        public Task<AllocatorAllocation> AllocateAsync(
            string requestId, string roomId, string matchId, CancellationToken cancellationToken) =>
            Task.FromResult(new AllocatorAllocation(
                requestId, roomId, ServerInstanceId, 19000, "Starting"));

        public Task<AllocatorRegistrationAck> ConfirmRegistrationAsync(
            string requestId, GameServerRegistration request, CancellationToken cancellationToken) =>
            Task.FromResult(new AllocatorRegistrationAck(
                requestId, request.ServerInstanceId, true, 3, "heartbeat-credential"));

        public Task RecordHeartbeatAsync(
            string requestId,
            string serverInstanceId,
            GameServerHeartbeat request,
            CancellationToken cancellationToken) => Task.CompletedTask;
    }

    private sealed class NoOpEventPublisher : ILobbyEventPublisher
    {
        public Task PublishAsync(string type, object data, CancellationToken cancellationToken) => Task.CompletedTask;
    }
}
