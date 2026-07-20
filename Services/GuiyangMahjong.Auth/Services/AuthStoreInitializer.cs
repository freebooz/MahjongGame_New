using GuiyangMahjong.Auth.Storage;

namespace GuiyangMahjong.Auth.Services;

public sealed class AuthStoreInitializer(IAuthStore store) : IHostedService
{
    public Task StartAsync(CancellationToken cancellationToken) => store.InitializeAsync(cancellationToken);
    public Task StopAsync(CancellationToken cancellationToken) => Task.CompletedTask;
}
