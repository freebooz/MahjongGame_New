using GuiyangMahjong.Allocator.Domain;

namespace GuiyangMahjong.Allocator.Api;

public sealed class AllocatorExceptionMiddleware(
    RequestDelegate next,
    ILogger<AllocatorExceptionMiddleware> logger)
{
    public async Task InvokeAsync(HttpContext context)
    {
        try
        {
            await next(context);
        }
        catch (AllocatorOperationException exception)
        {
            if (context.Response.HasStarted) throw;
            context.Response.StatusCode = exception.StatusCode;
            await WriteProblemAsync(context, "ALLOCATOR_OPERATION_REJECTED", exception.Message);
        }
        catch (InvalidOperationException exception)
        {
            if (context.Response.HasStarted) throw;
            logger.LogWarning(exception, "Allocator request rejected RequestId={RequestId}", GetRequestId(context));
            context.Response.StatusCode = StatusCodes.Status503ServiceUnavailable;
            await WriteProblemAsync(context, "ALLOCATOR_UNAVAILABLE", exception.Message);
        }
        catch (Exception exception)
        {
            if (context.Response.HasStarted) throw;
            logger.LogError(exception, "Allocator request failed RequestId={RequestId}", GetRequestId(context));
            context.Response.StatusCode = StatusCodes.Status500InternalServerError;
            await WriteProblemAsync(context, "INTERNAL_ERROR", "Allocator is temporarily unavailable.");
        }
    }

    private static Task WriteProblemAsync(HttpContext context, string code, string message)
    {
        context.Response.ContentType = "application/problem+json";
        return context.Response.WriteAsJsonAsync(new { requestId = GetRequestId(context), code, message });
    }

    private static string GetRequestId(HttpContext context) =>
        context.Request.Headers["X-Request-Id"].FirstOrDefault() ?? context.TraceIdentifier;
}
