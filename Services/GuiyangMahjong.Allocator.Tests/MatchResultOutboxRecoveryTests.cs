using System.Net;
using System.Net.Http.Json;
using System.Text.Json;
using GuiyangMahjong.Allocator.Options;
using GuiyangMahjong.Allocator.Services;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Tests;

public sealed class MatchResultOutboxRecoveryTests
{
    [Fact]
    public async Task ValidOutbox_IsAcknowledgedAndDeletedWithoutPersistingCredentials()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"mahjong-outbox-test-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            var matchId = Guid.NewGuid().ToString();
            var instanceId = Guid.NewGuid().ToString();
            var envelope = new MatchResultOutboxEnvelope(
                1,
                matchId,
                new MatchResultOutboxReport(
                    Guid.NewGuid().ToString(), instanceId, 9, 4,
                    [new MatchResultOutboxPlayer("player-1", 0, 1, 16)]));
            var path = Path.Combine(directory, $"{instanceId}.json");
            await File.WriteAllTextAsync(path, JsonSerializer.Serialize(
                envelope, new JsonSerializerOptions(JsonSerializerDefaults.Web)));
            File.SetLastWriteTimeUtc(path, DateTime.UtcNow.AddMinutes(-1));

            var handler = new RecordingHandler(async request =>
            {
                Assert.Equal(
                    $"http://127.0.0.1:18080/internal/matches/{matchId}/result/recovery",
                    request.RequestUri?.ToString());
                Assert.Equal("Bearer", request.Headers.Authorization?.Scheme);
                Assert.Equal("test-only-lobby-callback-token-long-enough",
                    request.Headers.Authorization?.Parameter);
                var payload = await request.Content!.ReadFromJsonAsync<MatchResultOutboxReport>();
                Assert.Equal(instanceId, payload?.ServerInstanceId);
                return new HttpResponseMessage(HttpStatusCode.OK)
                {
                    Content = JsonContent.Create(new MatchResultRecoveryAck(
                        Guid.NewGuid().ToString(), matchId, 9, true, false))
                };
            });
            var recovery = CreateRecovery(directory, handler);

            await recovery.RecoverAvailableAsync(CancellationToken.None);

            Assert.False(File.Exists(path));
            Assert.Equal(1, handler.RequestCount);
            var persisted = JsonSerializer.Serialize(envelope);
            Assert.DoesNotContain("credential", persisted, StringComparison.OrdinalIgnoreCase);
            Assert.DoesNotContain("token", persisted, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    [Fact]
    public async Task MismatchedFilename_IsRejectedLocallyAndRetained()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"mahjong-outbox-test-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            var envelope = new MatchResultOutboxEnvelope(
                1,
                Guid.NewGuid().ToString(),
                new MatchResultOutboxReport(
                    Guid.NewGuid().ToString(), Guid.NewGuid().ToString(), 1, 1,
                    [new MatchResultOutboxPlayer("player-1", 0, 1, 0)]));
            var path = Path.Combine(directory, $"{Guid.NewGuid()}.json");
            await File.WriteAllTextAsync(path, JsonSerializer.Serialize(
                envelope, new JsonSerializerOptions(JsonSerializerDefaults.Web)));
            File.SetLastWriteTimeUtc(path, DateTime.UtcNow.AddMinutes(-1));
            var handler = new RecordingHandler(_ => throw new InvalidOperationException("不应发送请求"));

            await CreateRecovery(directory, handler).RecoverAvailableAsync(CancellationToken.None);

            Assert.True(File.Exists(path));
            Assert.Equal(0, handler.RequestCount);
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static MatchResultOutboxRecovery CreateRecovery(string directory, RecordingHandler handler)
    {
        var options = Microsoft.Extensions.Options.Options.Create(new AllocatorOptions
        {
            MatchResultOutboxDirectory = directory,
            MatchResultRecoveryDelaySeconds = 1,
            LobbyInternalUrl = "http://127.0.0.1:18080",
            LobbyCallbackToken = "test-only-lobby-callback-token-long-enough"
        });
        return new MatchResultOutboxRecovery(
            new SingleClientFactory(handler), options, TimeProvider.System,
            NullLogger<MatchResultOutboxRecovery>.Instance);
    }

    private sealed class SingleClientFactory(HttpMessageHandler handler) : IHttpClientFactory
    {
        public HttpClient CreateClient(string name) => new(handler, disposeHandler: false);
    }

    private sealed class RecordingHandler(
        Func<HttpRequestMessage, Task<HttpResponseMessage>> responder) : HttpMessageHandler
    {
        public int RequestCount { get; private set; }

        protected override Task<HttpResponseMessage> SendAsync(
            HttpRequestMessage request, CancellationToken cancellationToken)
        {
            RequestCount++;
            return responder(request);
        }
    }
}
