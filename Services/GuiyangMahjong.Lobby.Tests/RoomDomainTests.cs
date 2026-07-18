using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Options;
using GuiyangMahjong.Lobby.Realtime;
using GuiyangMahjong.Lobby.Security;
using GuiyangMahjong.Lobby.Services;
using GuiyangMahjong.Lobby.Storage;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Lobby.Tests;

public sealed class RoomDomainTests
{
    [Fact]
    public async Task ConcurrentRoomCreation_AlwaysProducesUniqueRoomCodes()
    {
        var store = new InMemoryLobbyStore();
        var service = CreateService(store);
        var createTasks = Enumerable.Range(0, 200).Select(index => service.CreateRoomAsync(
            Guid.NewGuid().ToString(),
            new PlayerIdentity($"player-{index}", $"玩家{index}", "Guest"),
            NewCreateRequest(),
            CancellationToken.None));

        var rooms = await Task.WhenAll(createTasks);

        Assert.Equal(200, rooms.Select(room => room.RoomCode).Distinct(StringComparer.Ordinal).Count());
        Assert.All(rooms, room => Assert.Matches("^[0-9]{6}$", room.RoomCode));
    }

    [Fact]
    public async Task SharedPersistentStore_RestoresRoomForNewServiceInstance()
    {
        var durableStoreContract = new InMemoryLobbyStore();
        var firstService = CreateService(durableStoreContract);
        var created = await firstService.CreateRoomAsync(
            Guid.NewGuid().ToString(),
            new PlayerIdentity("owner", "房主", "Guest"),
            NewCreateRequest(),
            CancellationToken.None);

        var restartedService = CreateService(durableStoreContract);
        var directory = await restartedService.ListRoomsAsync(CancellationToken.None);

        Assert.Contains(directory, room => room.RoomCode == created.RoomCode);
    }

    [Fact]
    public void StateMachine_RejectsSkippingFromAllocatingToPlaying()
    {
        Assert.False(RoomStateMachine.CanTransition(RoomLifecycle.Allocating, RoomLifecycle.Playing));
        Assert.True(RoomStateMachine.CanTransition(RoomLifecycle.Allocating, RoomLifecycle.Waiting));
    }

    private static LobbyService CreateService(ILobbyStore store)
    {
        var options = Microsoft.Extensions.Options.Options.Create(new LobbyOptions
        {
            TokenSigningKey = LobbyWebApplicationFactory.SigningKey,
            RoomCodeRetryLimit = 500,
            MaximumPlayersPerRoom = 4
        });
        return new LobbyService(
            store,
            new RoomPasswordService(options, TimeProvider.System),
            new NoOpEventPublisher(),
            new DisabledAllocatorClient(),
            new HmacJoinTicketIssuer(options, TimeProvider.System),
            options,
            TimeProvider.System,
            NullLogger<LobbyService>.Instance);
    }

    private static CreateRoomRequest NewCreateRequest() => new(
        4,
        true,
        true,
        false,
        null,
        new Dictionary<string, object?> { ["ruleId"] = "GuiyangMainstreamV1" });

    private sealed class NoOpEventPublisher : ILobbyEventPublisher
    {
        public Task PublishAsync(string type, object data, CancellationToken cancellationToken) => Task.CompletedTask;
    }
}
