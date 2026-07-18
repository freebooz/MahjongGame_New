using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

public sealed class InstanceMonitorService(
    GameServerInstanceManager manager,
    IOptions<AllocatorOptions> options,
    ILogger<InstanceMonitorService> logger) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var timer = new PeriodicTimer(
            TimeSpan.FromMilliseconds(options.Value.MonitorIntervalMilliseconds));
        while (await timer.WaitForNextTickAsync(stoppingToken))
        {
            try { await manager.MonitorAsync(stoppingToken); }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested) { }
            catch (Exception exception) { logger.LogError(exception, "GameServer 监控轮询失败"); }
        }
    }
}

