using System.Security.Cryptography;
using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Options;
using GuiyangMahjong.Lobby.Realtime;
using GuiyangMahjong.Lobby.Security;
using GuiyangMahjong.Lobby.Storage;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Lobby.Services;

public sealed class LobbyService
{
    private readonly ILobbyStore store;
    private readonly IRoomPasswordService passwordService;
    private readonly ILobbyEventPublisher events;
    private readonly IAllocatorClient allocator;
    private readonly IJoinTicketIssuer joinTicketIssuer;
    private readonly LobbyOptions options;
    private readonly TimeProvider timeProvider;
    private readonly ILogger<LobbyService> logger;

    public LobbyService(
        ILobbyStore store,
        IRoomPasswordService passwordService,
        ILobbyEventPublisher events,
        IAllocatorClient allocator,
        IJoinTicketIssuer joinTicketIssuer,
        IOptions<LobbyOptions> options,
        TimeProvider timeProvider,
        ILogger<LobbyService> logger)
    {
        this.store = store;
        this.passwordService = passwordService;
        this.events = events;
        this.allocator = allocator;
        this.joinTicketIssuer = joinTicketIssuer;
        this.options = options.Value;
        this.timeProvider = timeProvider;
        this.logger = logger;
    }

    public async Task<RoomOperation> CreateRoomAsync(
        string requestId,
        PlayerIdentity player,
        CreateRoomRequest request,
        CancellationToken cancellationToken)
    {
        ValidateCreateRequest(request);
        var protectedPassword = request.PasswordProtected
            ? passwordService.Protect(request.Password!)
            : null;

        for (var attempt = 0; attempt < options.RoomCodeRetryLimit; attempt++)
        {
            var now = timeProvider.GetUtcNow();
            var room = new LobbyRoom
            {
                RoomId = Guid.NewGuid().ToString(),
                RoomCode = RandomNumberGenerator.GetInt32(0, 1_000_000).ToString("D6"),
                OwnerPlayerId = player.PlayerId,
                RoundCount = request.RoundCount,
                PublicRoom = request.PublicRoom,
                AutoStart = request.AutoStart,
                MaximumPlayers = options.MaximumPlayersPerRoom,
                RuleSnapshot = request.RuleSnapshot,
                Lifecycle = RoomLifecycle.Allocating,
                PlayerIds = [player.PlayerId],
                Password = protectedPassword,
                MatchId = Guid.NewGuid().ToString(),
                StateSequence = 1,
                CreatedAtUtc = now,
                UpdatedAtUtc = now
            };

            if (!await store.TryCreateRoomAsync(room, cancellationToken)) continue;

            logger.LogInformation(
                "房间创建请求已接受 RequestId={RequestId} RoomId={RoomId} PlayerId={PlayerId} PasswordProtected={PasswordProtected}",
                requestId, room.RoomId, player.PlayerId, room.Password is not null);
            await events.PublishAsync(LobbyEventTypes.RoomUpdated, ToDirectoryItem(room), cancellationToken);
            if (allocator.Enabled)
            {
                try
                {
                    var allocation = await allocator.AllocateAsync(
                        requestId,
                        room.RoomId,
                        room.MatchId,
                        cancellationToken);
                    room = room with
                    {
                        PendingServerInstanceId = allocation.ServerInstanceId,
                        StateSequence = room.StateSequence + 1,
                        UpdatedAtUtc = timeProvider.GetUtcNow()
                    };
                    if (!await store.UpdateRoomAsync(room, cancellationToken))
                    {
                        throw new HttpRequestException("Allocated room binding could not be persisted.");
                    }
                }
                catch (Exception exception) when (exception is HttpRequestException or TaskCanceledException)
                {
                    room = RoomStateMachine.Transition(room, RoomLifecycle.Failed, timeProvider);
                    await store.UpdateRoomAsync(room, cancellationToken);
                    await events.PublishAsync(LobbyEventTypes.RoomClosed, ToDirectoryItem(room), cancellationToken);
                    throw new LobbyOperationException(
                        LobbyErrorCode.ServerUnavailable,
                        "GameServer allocator is temporarily unavailable.",
                        StatusCodes.Status503ServiceUnavailable,
                        1000);
                }
            }
            return new RoomOperation(requestId, room.RoomId, room.RoomCode, room.Lifecycle);
        }

        throw new LobbyOperationException(
            LobbyErrorCode.InternalError,
            "暂时无法生成唯一房间号，请稍后重试",
            StatusCodes.Status503ServiceUnavailable,
            1000);
    }

    public async Task<object> JoinRoomAsync(
        string requestId,
        PlayerIdentity player,
        string roomCode,
        JoinRoomRequest request,
        CancellationToken cancellationToken)
    {
        if (roomCode.Length != 6 || !roomCode.All(char.IsAsciiDigit))
        {
            throw Invalid("房间号必须为 6 位数字");
        }
        if (request.ClientProtocolVersion != options.ProtocolVersion)
        {
            throw new LobbyOperationException(
                LobbyErrorCode.VersionMismatch, "客户端协议版本不兼容", StatusCodes.Status409Conflict);
        }

        var room = await store.GetRoomByCodeAsync(roomCode, cancellationToken)
            ?? throw new LobbyOperationException(
                LobbyErrorCode.RoomNotFound, "房间不存在", StatusCodes.Status404NotFound);

        var passwordResult = passwordService.Verify(player.PlayerId, room.RoomId, room.Password, request.Password);
        switch (passwordResult.Status)
        {
            case PasswordVerificationStatus.Required:
                throw new LobbyOperationException(
                    LobbyErrorCode.PasswordRequired, "请输入房间密码", StatusCodes.Status400BadRequest);
            case PasswordVerificationStatus.Wrong:
                logger.LogWarning(
                    "房间密码验证失败 RequestId={RequestId} RoomId={RoomId} PlayerId={PlayerId}",
                    requestId, room.RoomId, player.PlayerId);
                throw new LobbyOperationException(
                    LobbyErrorCode.WrongPassword, "房间密码错误", StatusCodes.Status403Forbidden);
            case PasswordVerificationStatus.RateLimited:
                logger.LogWarning(
                    "房间密码尝试被限流 RequestId={RequestId} RoomId={RoomId} PlayerId={PlayerId}",
                    requestId, room.RoomId, player.PlayerId);
                throw new LobbyOperationException(
                    LobbyErrorCode.RateLimited,
                    "密码尝试次数过多，请稍后重试",
                    StatusCodes.Status429TooManyRequests,
                    passwordResult.RetryAfterMilliseconds);
        }

        var added = await store.TryAddPlayerAsync(roomCode, player.PlayerId, cancellationToken);
        room = added.Room ?? room;
        switch (added.Status)
        {
            case AddPlayerStatus.RoomNotFound:
                throw new LobbyOperationException(
                    LobbyErrorCode.RoomNotFound, "房间不存在", StatusCodes.Status404NotFound);
            case AddPlayerStatus.RoomClosed:
                throw new LobbyOperationException(
                    LobbyErrorCode.RoomClosed, "房间已关闭或牌局已经开始", StatusCodes.Status409Conflict);
            case AddPlayerStatus.RoomFull:
                throw new LobbyOperationException(
                    LobbyErrorCode.RoomFull, "房间人数已满", StatusCodes.Status409Conflict);
        }

        logger.LogInformation(
            "玩家加入房间 RequestId={RequestId} RoomId={RoomId} PlayerId={PlayerId}",
            requestId, room.RoomId, player.PlayerId);
        await events.PublishAsync(LobbyEventTypes.RoomUpdated, ToDirectoryItem(room), cancellationToken);
        return room.Route is not null
            ? GetAuthorizedRoute(requestId, player, room)
            : new RoomOperation(requestId, room.RoomId, room.RoomCode, room.Lifecycle);
    }

    public async Task<GameServerRoute> GetRouteAsync(
        string requestId,
        PlayerIdentity player,
        string roomCode,
        CancellationToken cancellationToken)
    {
        var room = await store.GetRoomByCodeAsync(roomCode, cancellationToken)
            ?? throw new LobbyOperationException(
                LobbyErrorCode.RoomNotFound, "房间不存在", StatusCodes.Status404NotFound);
        return GetAuthorizedRoute(requestId, player, room);
    }

    public async Task<GameServerRoute> GetReconnectRouteAsync(
        string requestId,
        PlayerIdentity player,
        ReconnectRouteRequest request,
        CancellationToken cancellationToken)
    {
        var room = await store.GetRoomByIdAsync(request.RoomId, cancellationToken)
            ?? throw new LobbyOperationException(
                LobbyErrorCode.RoomNotFound, "原房间不存在", StatusCodes.Status404NotFound);
        if (!string.Equals(room.MatchId, request.MatchId, StringComparison.Ordinal))
        {
            throw Invalid("牌局标识与房间不匹配");
        }
        return GetAuthorizedRoute(requestId, player, room);
    }

    public async Task<IReadOnlyList<RoomDirectoryItem>> ListRoomsAsync(CancellationToken cancellationToken) =>
        (await store.ListPublicRoomsAsync(cancellationToken)).Select(ToDirectoryItem).ToArray();

    public async Task<GameServerRegistrationAck> RegisterGameServerAsync(
        string requestId,
        GameServerRegistration registration,
        CancellationToken cancellationToken)
    {
        if (!allocator.Enabled)
        {
            throw new LobbyOperationException(
                LobbyErrorCode.BackendNotConfigured,
                "Allocator integration is disabled.",
                StatusCodes.Status503ServiceUnavailable);
        }
        var room = await WaitForPendingAllocationAsync(
                registration.RoomId,
                registration.ServerInstanceId,
                cancellationToken)
            ?? throw new LobbyOperationException(
                LobbyErrorCode.RoomNotFound, "Room was not found.", StatusCodes.Status404NotFound);
        if (room.MatchId != registration.MatchId
            || room.Lifecycle != RoomLifecycle.Allocating
            || room.PendingServerInstanceId != registration.ServerInstanceId)
        {
            throw Invalid("GameServer registration does not match the room allocation.");
        }

        var acknowledgement = await allocator.ConfirmRegistrationAsync(
            requestId, registration, cancellationToken);
        var now = timeProvider.GetUtcNow();
        room = RoomStateMachine.Transition(room, RoomLifecycle.Waiting, timeProvider) with
        {
            PendingServerInstanceId = null,
            Route = new GameServerRoute(
                requestId,
                string.Empty,
                room.RoomId,
                registration.ServerInstanceId,
                room.MatchId,
                registration.ListenIp,
                registration.ListenPort,
                string.Empty,
                now)
        };
        if (!await store.UpdateRoomAsync(room, cancellationToken))
        {
            throw new LobbyOperationException(
                LobbyErrorCode.InternalError,
                "Room route could not be persisted.",
                StatusCodes.Status500InternalServerError);
        }
        await events.PublishAsync(LobbyEventTypes.ServerAssigned, ToDirectoryItem(room), cancellationToken);
        await events.PublishAsync(LobbyEventTypes.RoomUpdated, ToDirectoryItem(room), cancellationToken);
        return new GameServerRegistrationAck(
            requestId,
            acknowledgement.Accepted,
            acknowledgement.HeartbeatIntervalSeconds,
            acknowledgement.HeartbeatCredential);
    }

    public Task RecordGameServerHeartbeatAsync(
        string requestId,
        string serverInstanceId,
        GameServerHeartbeat heartbeat,
        CancellationToken cancellationToken) => allocator.RecordHeartbeatAsync(
            requestId, serverInstanceId, heartbeat, cancellationToken);

    public async Task MarkGameServerFailedAsync(
        GameServerFailure failure,
        CancellationToken cancellationToken)
    {
        var room = await store.GetRoomByIdAsync(failure.RoomId, cancellationToken);
        if (room is null
            || (room.Route?.ServerInstanceId != failure.ServerInstanceId
                && room.PendingServerInstanceId != failure.ServerInstanceId)
            || room.Lifecycle is RoomLifecycle.Closed or RoomLifecycle.Failed)
        {
            return;
        }

        room = RoomStateMachine.Transition(room, RoomLifecycle.Failed, timeProvider) with
        {
            Route = null,
            PendingServerInstanceId = null
        };
        await store.UpdateRoomAsync(room, cancellationToken);
        logger.LogWarning(
            "GameServer failure closed room RoomId={RoomId} InstanceId={InstanceId} Reason={Reason}",
            failure.RoomId,
            failure.ServerInstanceId,
            failure.Reason);
        await events.PublishAsync(LobbyEventTypes.RoomClosed, ToDirectoryItem(room), cancellationToken);
    }

    private GameServerRoute GetAuthorizedRoute(string requestId, PlayerIdentity player, LobbyRoom room)
    {
        if (!room.PlayerIds.Contains(player.PlayerId, StringComparer.Ordinal))
        {
            throw new LobbyOperationException(
                LobbyErrorCode.InvalidRequest, "玩家尚未加入该房间", StatusCodes.Status403Forbidden);
        }
        if (room.Route is null)
        {
            throw new LobbyOperationException(
                LobbyErrorCode.ServerUnavailable,
                "牌桌服务器仍在分配中",
                StatusCodes.Status503ServiceUnavailable,
                1000);
        }
        var issued = joinTicketIssuer.Issue(player.PlayerId, room, room.Route.ServerInstanceId);
        return room.Route with
        {
            RequestId = requestId,
            PlayerId = player.PlayerId,
            JoinTicket = issued.Ticket,
            TicketExpireAtUtc = issued.ExpiresAtUtc
        };
    }

    private async Task<LobbyRoom?> WaitForPendingAllocationAsync(
        string roomId,
        string serverInstanceId,
        CancellationToken cancellationToken)
    {
        for (var attempt = 0; attempt < 40; attempt++)
        {
            var room = await store.GetRoomByIdAsync(roomId, cancellationToken);
            if (room is null
                || room.PendingServerInstanceId == serverInstanceId
                || room.Lifecycle != RoomLifecycle.Allocating)
            {
                return room;
            }
            await Task.Delay(25, cancellationToken);
        }
        return await store.GetRoomByIdAsync(roomId, cancellationToken);
    }

    private static RoomDirectoryItem ToDirectoryItem(LobbyRoom room) => new(
        room.RoomCode,
        room.Lifecycle,
        room.PlayerIds.Length,
        room.MaximumPlayers,
        room.Password is not null,
        room.RoundCount);

    private static void ValidateCreateRequest(CreateRoomRequest request)
    {
        if (request.RoundCount is < 1 or > 16) throw Invalid("局数必须为 1 到 16");
        if (request.RuleSnapshot is null) throw Invalid("缺少规则快照");
        if (request.PasswordProtected && (request.Password is null || request.Password.Length is < 6 or > 12))
        {
            throw Invalid("房间密码必须为 6 到 12 个字符");
        }
        if (!request.PasswordProtected && !string.IsNullOrEmpty(request.Password))
        {
            throw Invalid("非密码房不得提交密码");
        }
    }

    private static LobbyOperationException Invalid(string message) => new(
        LobbyErrorCode.InvalidRequest, message, StatusCodes.Status400BadRequest);
}
