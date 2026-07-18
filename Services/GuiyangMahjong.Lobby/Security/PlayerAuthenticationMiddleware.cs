using GuiyangMahjong.Lobby.Domain;
using GuiyangMahjong.Lobby.Services;

namespace GuiyangMahjong.Lobby.Security;

public sealed class PlayerAuthenticationMiddleware(RequestDelegate next)
{
    public const string PlayerItemKey = "GuiyangLobby.Player";

    public async Task InvokeAsync(
        HttpContext context,
        IPlayerTokenValidator tokenValidator,
        OnlinePresenceService presence)
    {
        if (!context.Request.Path.StartsWithSegments("/v1"))
        {
            await next(context);
            return;
        }

        var authorization = context.Request.Headers.Authorization.ToString();
        if (!authorization.StartsWith("Bearer ", StringComparison.OrdinalIgnoreCase))
        {
            await WriteUnauthorized(context, "缺少玩家登录凭据");
            return;
        }

        var result = tokenValidator.Validate(authorization["Bearer ".Length..].Trim());
        if (!result.IsValid || result.Player is null)
        {
            await WriteUnauthorized(context, result.ChineseReason);
            return;
        }

        context.Items[PlayerItemKey] = result.Player;
        presence.Touch(result.Player.PlayerId);
        await next(context);
    }

    public static PlayerIdentity GetPlayer(HttpContext context) =>
        context.Items[PlayerItemKey] as PlayerIdentity
        ?? throw new InvalidOperationException("玩家身份中间件尚未执行");

    private static async Task WriteUnauthorized(HttpContext context, string message)
    {
        context.Response.StatusCode = StatusCodes.Status401Unauthorized;
        context.Response.ContentType = "application/problem+json";
        var requestId = RequestIdMiddleware.GetRequestId(context);
        await context.Response.WriteAsJsonAsync(
            new ApiError(requestId, "SESSION_EXPIRED", message),
            cancellationToken: context.RequestAborted);
    }
}
