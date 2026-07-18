using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Storage;

public enum AddPlayerStatus
{
    Added,
    AlreadyMember,
    RoomNotFound,
    RoomClosed,
    RoomFull
}

public sealed record AddPlayerResult(AddPlayerStatus Status, LobbyRoom? Room);

public enum FinalizeMatchStatus
{
    Accepted,
    Duplicate,
    Conflict
}

public interface ILobbyStore
{
    Task InitializeAsync(CancellationToken cancellationToken);
    Task<bool> TryCreateRoomAsync(LobbyRoom room, CancellationToken cancellationToken);
    Task<LobbyRoom?> GetRoomByCodeAsync(string roomCode, CancellationToken cancellationToken);
    Task<LobbyRoom?> GetRoomByIdAsync(string roomId, CancellationToken cancellationToken);
    Task<LobbyRoom?> GetActiveRoomByPlayerAsync(string playerId, CancellationToken cancellationToken);
    Task<IReadOnlyList<LobbyRoom>> ListPublicRoomsAsync(CancellationToken cancellationToken);
    Task<AddPlayerResult> TryAddPlayerAsync(
        string roomCode, string playerId, CancellationToken cancellationToken);
    Task<bool> UpdateRoomAsync(LobbyRoom room, CancellationToken cancellationToken);
    Task<FinalizeMatchStatus> FinalizeMatchAsync(
        LobbyRoom closedRoom, MatchResultReport report, CancellationToken cancellationToken);
}
