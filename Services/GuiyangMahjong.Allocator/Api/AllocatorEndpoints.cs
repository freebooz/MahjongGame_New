using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Services;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Api;

public static class AllocatorEndpoints
{
    public static void MapAllocatorEndpoints(this WebApplication app)
    {
        app.MapGet("/health/live", () => Results.Ok(new { status = "live" }));
        app.MapGet("/health/ready", (
            GameServerInstanceManager manager,
            IOptions<AllocatorOptions> options) =>
        {
            var executablePath = Path.IsPathRooted(options.Value.GameServerExecutablePath)
                ? options.Value.GameServerExecutablePath
                : Path.Combine(AppContext.BaseDirectory, options.Value.GameServerExecutablePath);
            var executableReady = File.Exists(executablePath);
            return manager.IsInitialized && executableReady
                ? Results.Ok(new { status = "ready", stateReconciled = true, gameServerExecutable = "ready" })
                : Results.Json(
                    new
                    {
                        status = "not-ready",
                        stateReconciled = manager.IsInitialized,
                        gameServerExecutable = executableReady ? "ready" : "missing"
                    },
                    statusCode: StatusCodes.Status503ServiceUnavailable);
        });
        app.MapGet("/openapi/v1.yaml", async (HttpContext context) =>
        {
            var path = Path.Combine(AppContext.BaseDirectory, "OpenAPI", "allocator-v1.openapi.yaml");
            context.Response.ContentType = "application/yaml; charset=utf-8";
            await context.Response.SendFileAsync(path, context.RequestAborted);
        });

        var internalApi = app.MapGroup("/internal");
        internalApi.MapPost("/allocations", async (
            HttpContext context,
            AllocationRequest request,
            GameServerInstanceManager manager,
            CancellationToken cancellationToken) => Results.Accepted(value: await manager.AllocateAsync(
                GetRequestId(context), request, cancellationToken)));

        internalApi.MapGet("/instances", (GameServerInstanceManager manager) => Results.Ok(manager.List()));
        internalApi.MapGet("/instances/{serverInstanceId}", (
            string serverInstanceId,
            GameServerInstanceManager manager) => manager.Get(serverInstanceId) is { } instance
                ? Results.Ok(instance)
                : Results.NotFound());

        internalApi.MapPost("/instances/{serverInstanceId}/register", async (
            string serverInstanceId,
            HttpContext context,
            ConfirmRegistrationRequest request,
            GameServerInstanceManager manager,
            CancellationToken cancellationToken) => Results.Ok(await manager.ConfirmRegistrationAsync(
                GetRequestId(context), serverInstanceId, request, cancellationToken)));

        internalApi.MapPost("/instances/{serverInstanceId}/heartbeat", async (
            string serverInstanceId,
            InstanceHeartbeatRequest request,
            GameServerInstanceManager manager,
            CancellationToken cancellationToken) =>
        {
            await manager.RecordHeartbeatAsync(serverInstanceId, request, cancellationToken);
            return Results.NoContent();
        });

        internalApi.MapPost("/instances/{serverInstanceId}/drain", async (
            string serverInstanceId,
            GameServerInstanceManager manager,
            CancellationToken cancellationToken) => Results.Ok(await manager.DrainAsync(
                serverInstanceId, cancellationToken)));
    }

    private static string GetRequestId(HttpContext context)
    {
        var supplied = context.Request.Headers["X-Request-Id"].ToString();
        return Guid.TryParse(supplied, out var id) ? id.ToString() : Guid.NewGuid().ToString();
    }
}
