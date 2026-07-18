using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

public static class MatchResultOutboxPaths
{
    public static string GetDirectory(AllocatorOptions options)
    {
        var configured = options.MatchResultOutboxDirectory.Trim();
        var path = Path.IsPathRooted(configured)
            ? configured
            : Path.Combine(AppContext.BaseDirectory, configured);
        return Path.GetFullPath(path);
    }

    public static string GetInstancePath(AllocatorOptions options, string serverInstanceId) =>
        Path.Combine(GetDirectory(options), $"{serverInstanceId}.json");
}

public sealed record MatchResultOutboxPlayer(
    string PlayerId,
    int SeatIndex,
    int Rank,
    int TotalScore);

public sealed record MatchResultOutboxReport(
    string RoomId,
    string ServerInstanceId,
    long ResultSequence,
    int CompletedRounds,
    MatchResultOutboxPlayer[] Players);

public sealed record MatchResultOutboxEnvelope(
    int Version,
    string MatchId,
    MatchResultOutboxReport Report);

public sealed record MatchResultRecoveryAck(
    string RequestId,
    string MatchId,
    long ResultSequence,
    bool Accepted,
    bool Duplicate);

public sealed class MatchResultOutboxRecovery(
    IHttpClientFactory httpClientFactory,
    IOptions<AllocatorOptions> options,
    TimeProvider timeProvider,
    ILogger<MatchResultOutboxRecovery> logger)
{
    private const long MaximumOutboxBytes = 64 * 1024;
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web);
    private readonly AllocatorOptions options = options.Value;
    private readonly string outboxDirectory = MatchResultOutboxPaths.GetDirectory(options.Value);

    public async Task RecoverAvailableAsync(CancellationToken cancellationToken)
    {
        Directory.CreateDirectory(outboxDirectory);
        foreach (var path in Directory.EnumerateFiles(outboxDirectory, "*.json", SearchOption.TopDirectoryOnly))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (timeProvider.GetUtcNow() - File.GetLastWriteTimeUtc(path)
                < TimeSpan.FromSeconds(options.MatchResultRecoveryDelaySeconds))
            {
                continue;
            }

            await TryRecoverAsync(path, cancellationToken);
        }
    }

    private async Task TryRecoverAsync(string path, CancellationToken cancellationToken)
    {
        MatchResultOutboxEnvelope envelope;
        try
        {
            var file = new FileInfo(path);
            if (file.Length is <= 0 or > MaximumOutboxBytes)
            {
                logger.LogError("结算 outbox 文件大小非法 Path={Path} Bytes={Bytes}", path, file.Length);
                return;
            }
            await using var stream = new FileStream(
                path, FileMode.Open, FileAccess.Read, FileShare.Read, 16 * 1024, FileOptions.Asynchronous);
            envelope = await JsonSerializer.DeserializeAsync<MatchResultOutboxEnvelope>(
                    stream, JsonOptions, cancellationToken)
                ?? throw new JsonException("结算 outbox 内容为空");
            Validate(path, envelope);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException
            or InvalidDataException)
        {
            logger.LogError(exception, "结算 outbox 无法读取或校验 Path={Path}", path);
            return;
        }

        try
        {
            using var request = new HttpRequestMessage(
                HttpMethod.Post,
                $"{options.LobbyInternalUrl.TrimEnd('/')}/internal/matches/{envelope.MatchId}/result/recovery")
            {
                Content = JsonContent.Create(envelope.Report, options: JsonOptions)
            };
            request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", options.LobbyCallbackToken);
            request.Headers.Add("X-Request-Id", Guid.NewGuid().ToString());
            request.Headers.Add("Idempotency-Key", $"recovery:{envelope.MatchId}:{envelope.Report.ResultSequence}");
            using var response = await httpClientFactory.CreateClient(nameof(MatchResultOutboxRecovery))
                .SendAsync(request, cancellationToken);
            if (!response.IsSuccessStatusCode)
            {
                logger.LogWarning(
                    "大厅暂未接受结算 outbox MatchId={MatchId} InstanceId={InstanceId} Status={Status}",
                    envelope.MatchId, envelope.Report.ServerInstanceId, (int)response.StatusCode);
                return;
            }

            var acknowledgement = await response.Content.ReadFromJsonAsync<MatchResultRecoveryAck>(
                JsonOptions, cancellationToken);
            if (acknowledgement is not { Accepted: true }
                || acknowledgement.MatchId != envelope.MatchId
                || acknowledgement.ResultSequence != envelope.Report.ResultSequence)
            {
                logger.LogWarning(
                    "大厅返回的结算恢复确认不匹配 MatchId={MatchId} InstanceId={InstanceId}",
                    envelope.MatchId, envelope.Report.ServerInstanceId);
                return;
            }

            File.Delete(path);
            logger.LogInformation(
                "结算 outbox 已恢复 MatchId={MatchId} InstanceId={InstanceId} Sequence={Sequence} Duplicate={Duplicate}",
                envelope.MatchId, envelope.Report.ServerInstanceId,
                envelope.Report.ResultSequence, acknowledgement.Duplicate);
        }
        catch (Exception exception) when (exception is HttpRequestException or IOException
            or UnauthorizedAccessException or TaskCanceledException)
        {
            if (exception is TaskCanceledException && cancellationToken.IsCancellationRequested) throw;
            logger.LogWarning(exception,
                "结算 outbox 恢复暂时失败 MatchId={MatchId} InstanceId={InstanceId}",
                envelope.MatchId, envelope.Report.ServerInstanceId);
        }
    }

    private static void Validate(string path, MatchResultOutboxEnvelope envelope)
    {
        if (envelope.Version != 1
            || !Guid.TryParse(envelope.MatchId, out _)
            || !Guid.TryParse(envelope.Report.RoomId, out _)
            || !Guid.TryParse(envelope.Report.ServerInstanceId, out _)
            || !string.Equals(Path.GetFileNameWithoutExtension(path), envelope.Report.ServerInstanceId,
                StringComparison.OrdinalIgnoreCase)
            || envelope.Report.ResultSequence < 1
            || envelope.Report.CompletedRounds is < 1 or > 16
            || envelope.Report.Players is null
            || envelope.Report.Players.Length is < 1 or > 4
            || envelope.Report.Players.Any(player => string.IsNullOrWhiteSpace(player.PlayerId)
                || player.PlayerId.Length > 80
                || player.SeatIndex is < 0 or > 3
                || player.Rank is < 1 or > 4)
            || envelope.Report.Players.Select(player => player.PlayerId)
                .Distinct(StringComparer.Ordinal).Count() != envelope.Report.Players.Length)
        {
            throw new InvalidDataException("结算 outbox 的版本、作用域或结果数据非法");
        }
    }
}

public sealed class MatchResultOutboxRecoveryService(
    MatchResultOutboxRecovery recovery,
    IOptions<AllocatorOptions> options,
    ILogger<MatchResultOutboxRecoveryService> logger) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var timer = new PeriodicTimer(
            TimeSpan.FromMilliseconds(options.Value.MonitorIntervalMilliseconds));
        do
        {
            try { await recovery.RecoverAvailableAsync(stoppingToken); }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested) { return; }
            catch (Exception exception) { logger.LogError(exception, "结算 outbox 恢复轮询失败"); }
        }
        while (await timer.WaitForNextTickAsync(stoppingToken));
    }
}
