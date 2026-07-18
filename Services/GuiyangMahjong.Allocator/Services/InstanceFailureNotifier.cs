using System.Net.Http.Headers;
using System.Net.Http.Json;
using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

public interface IInstanceFailureNotifier
{
    Task NotifyAsync(InstanceFailureNotification notification, CancellationToken cancellationToken);
}

public sealed class LobbyInstanceFailureNotifier(
    IHttpClientFactory httpClientFactory,
    IOptions<AllocatorOptions> options,
    ILogger<LobbyInstanceFailureNotifier> logger) : IInstanceFailureNotifier
{
    private readonly AllocatorOptions options = options.Value;

    public async Task NotifyAsync(InstanceFailureNotification notification, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(options.LobbyInternalUrl)) return;
        try
        {
            using var request = new HttpRequestMessage(
                HttpMethod.Post,
                $"{options.LobbyInternalUrl.TrimEnd('/')}/internal/gameservers/failure")
            {
                Content = JsonContent.Create(notification)
            };
            request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", options.LobbyCallbackToken);
            request.Headers.Add("X-Request-Id", Guid.NewGuid().ToString());
            using var response = await httpClientFactory.CreateClient(nameof(LobbyInstanceFailureNotifier))
                .SendAsync(request, cancellationToken);
            if (!response.IsSuccessStatusCode)
            {
                logger.LogWarning(
                    "大厅拒绝实例失败回调 InstanceId={InstanceId} Status={Status}",
                    notification.ServerInstanceId, (int)response.StatusCode);
            }
        }
        catch (HttpRequestException exception)
        {
            logger.LogWarning(exception,
                "实例失败回调暂时无法送达 InstanceId={InstanceId}", notification.ServerInstanceId);
        }
    }
}

