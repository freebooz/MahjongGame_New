namespace GuiyangMahjong.Allocator.Services;

public sealed class AllocatorStateInitializer(GameServerInstanceManager manager) : IHostedService
{
    public Task StartAsync(CancellationToken cancellationToken) => manager.InitializeAsync(cancellationToken);
    public Task StopAsync(CancellationToken cancellationToken) => Task.CompletedTask;
}
