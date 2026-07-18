using System.Security.Cryptography;
using System.Text;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Security;

public sealed class AllocatorServiceAuthenticationMiddleware(
    RequestDelegate next,
    IOptions<AllocatorOptions> options)
{
    private readonly byte[] expected = Encoding.UTF8.GetBytes(options.Value.ServiceToken);

    public async Task InvokeAsync(HttpContext context)
    {
        if (!context.Request.Path.StartsWithSegments("/internal"))
        {
            await next(context);
            return;
        }

        var header = context.Request.Headers.Authorization.ToString();
        var supplied = header.StartsWith("Bearer ", StringComparison.OrdinalIgnoreCase)
            ? Encoding.UTF8.GetBytes(header[7..].Trim())
            : [];
        var authenticated = supplied.Length == expected.Length
            && CryptographicOperations.FixedTimeEquals(supplied, expected);
        CryptographicOperations.ZeroMemory(supplied);
        if (!authenticated)
        {
            context.Response.StatusCode = StatusCodes.Status401Unauthorized;
            await context.Response.WriteAsJsonAsync(new
            {
                requestId = GetRequestId(context),
                code = "UNAUTHORIZED",
                message = "A valid allocator service credential is required."
            });
            return;
        }

        await next(context);
    }

    private static string GetRequestId(HttpContext context) =>
        context.Request.Headers["X-Request-Id"].FirstOrDefault() ?? context.TraceIdentifier;
}
