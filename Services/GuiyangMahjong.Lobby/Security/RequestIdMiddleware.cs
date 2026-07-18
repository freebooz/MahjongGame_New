using System.Collections.Frozen;
using GuiyangMahjong.Lobby.Domain;

namespace GuiyangMahjong.Lobby.Security;

public sealed class RequestIdMiddleware(RequestDelegate next)
{
    private const string ItemKey = "GuiyangLobby.RequestId";
    private static readonly FrozenSet<string> ExemptPaths =
        new[] { "/health/live", "/health/ready", "/openapi/v1.yaml" }
        .ToFrozenSet(StringComparer.OrdinalIgnoreCase);

    public async Task InvokeAsync(HttpContext context)
    {
        var supplied = context.Request.Headers["X-Request-Id"].ToString();
        if (!Guid.TryParse(supplied, out var requestId))
        {
            if (!ExemptPaths.Contains(context.Request.Path.Value ?? string.Empty))
            {
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                context.Response.ContentType = "application/problem+json";
                await context.Response.WriteAsJsonAsync(
                    new ApiError(Guid.NewGuid().ToString(), "INVALID_REQUEST", "X-Request-Id 必须为有效 UUID"),
                    cancellationToken: context.RequestAborted);
                return;
            }
            requestId = Guid.NewGuid();
        }

        var normalized = requestId.ToString();
        context.Items[ItemKey] = normalized;
        context.Response.Headers["X-Request-Id"] = normalized;
        await next(context);
    }

    public static string GetRequestId(HttpContext context) =>
        context.Items.TryGetValue(ItemKey, out var value) && value is string requestId
            ? requestId
            : context.TraceIdentifier;
}

