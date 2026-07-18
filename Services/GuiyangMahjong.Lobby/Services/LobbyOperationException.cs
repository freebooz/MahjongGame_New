using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Services;

public sealed class LobbyOperationException : Exception
{
    public LobbyOperationException(
        LobbyErrorCode errorCode,
        string chineseMessage,
        int statusCode,
        int? retryAfterMilliseconds = null)
        : base(chineseMessage)
    {
        ErrorCode = errorCode;
        StatusCode = statusCode;
        RetryAfterMilliseconds = retryAfterMilliseconds;
    }

    public LobbyErrorCode ErrorCode { get; }
    public int StatusCode { get; }
    public int? RetryAfterMilliseconds { get; }

    public string StableCode => ErrorCode switch
    {
        LobbyErrorCode.InvalidRequest => "INVALID_REQUEST",
        LobbyErrorCode.SessionExpired => "SESSION_EXPIRED",
        LobbyErrorCode.RequestInProgress => "REQUEST_IN_PROGRESS",
        LobbyErrorCode.RoomNotFound => "ROOM_NOT_FOUND",
        LobbyErrorCode.RoomFull => "ROOM_FULL",
        LobbyErrorCode.RoomClosed => "ROOM_CLOSED",
        LobbyErrorCode.PasswordRequired => "PASSWORD_REQUIRED",
        LobbyErrorCode.WrongPassword => "WRONG_PASSWORD",
        LobbyErrorCode.RateLimited => "RATE_LIMITED",
        LobbyErrorCode.ServerUnavailable => "SERVER_UNAVAILABLE",
        LobbyErrorCode.TicketExpired => "TICKET_EXPIRED",
        LobbyErrorCode.VersionMismatch => "VERSION_MISMATCH",
        LobbyErrorCode.Timeout => "TIMEOUT",
        LobbyErrorCode.Cancelled => "CANCELLED",
        LobbyErrorCode.BackendNotConfigured => "BACKEND_NOT_CONFIGURED",
        _ => "INTERNAL_ERROR"
    };
}

