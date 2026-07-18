using GuiyangMahjong.Lobby.Storage;

namespace GuiyangMahjong.Lobby.Services;

public sealed class LobbyStoreInitializer(
    ILobbyStore store,
    ILogger<LobbyStoreInitializer> logger) : IHostedService
{
    public async Task StartAsync(CancellationToken cancellationToken)
    {
        await store.InitializeAsync(cancellationToken);
        logger.LogInformation("大厅存储初始化完成 Store={StoreType}", store.GetType().Name);
    }

    public Task StopAsync(CancellationToken cancellationToken) => Task.CompletedTask;
}

