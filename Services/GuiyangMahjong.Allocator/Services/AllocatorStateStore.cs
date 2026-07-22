using System.Text.Json;
using System.Text.Json.Serialization;
using System.ComponentModel;
using System.Runtime.InteropServices;
using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

public sealed record PersistedGameServerInstance(
    string ServerInstanceId,
    string RoomId,
    string MatchId,
    int Port,
    string AdvertisedIp,
    string RegistrationCredentialHash,
    string? HeartbeatCredentialHash,
    DateTimeOffset RegistrationExpireAtUtc,
    DateTimeOffset StartedAtUtc,
    DateTimeOffset? ProcessStartedAtUtc,
    int? ProcessId,
    DateTimeOffset? RegisteredAtUtc,
    DateTimeOffset? LastHeartbeatAtUtc,
    string BuildVersion,
    GameServerInstanceState State,
    string? FailureReason,
    bool FailureNotified,
    DateTimeOffset? FailureNotificationAttemptedAtUtc,
    bool PortReleased,
    string? OrchestratorResourceName = null);

public sealed record AllocatorStateDocument(
    int SchemaVersion,
    DateTimeOffset UpdatedAtUtc,
    PersistedGameServerInstance[] Instances);

public interface IAllocatorStateStore
{
    Task<AllocatorStateDocument> LoadAsync(CancellationToken cancellationToken);
    Task SaveAsync(AllocatorStateDocument state, CancellationToken cancellationToken);
}

public sealed class JsonAllocatorStateStore(
    IOptions<AllocatorOptions> options,
    TimeProvider timeProvider,
    ILogger<JsonAllocatorStateStore> logger) : IAllocatorStateStore
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter() }
    };
    private readonly string statePath = Path.GetFullPath(
        Path.IsPathRooted(options.Value.StateFilePath)
            ? options.Value.StateFilePath
            : Path.Combine(AppContext.BaseDirectory, options.Value.StateFilePath));
    private readonly SemaphoreSlim gate = new(1, 1);

    public async Task<AllocatorStateDocument> LoadAsync(CancellationToken cancellationToken)
    {
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!File.Exists(statePath))
                return new AllocatorStateDocument(1, timeProvider.GetUtcNow(), []);
            await using var stream = new FileStream(
                statePath, FileMode.Open, FileAccess.Read, FileShare.Read, 16 * 1024, true);
            return await JsonSerializer.DeserializeAsync<AllocatorStateDocument>(
                       stream, JsonOptions, cancellationToken)
                   ?? throw new InvalidDataException("Allocator state document is empty.");
        }
        catch (JsonException exception)
        {
            logger.LogCritical(exception, "Allocator state file is corrupt Path={StatePath}", statePath);
            throw new InvalidDataException("Allocator state file is corrupt; refusing unsafe port reuse.", exception);
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task SaveAsync(AllocatorStateDocument state, CancellationToken cancellationToken)
    {
        await gate.WaitAsync(cancellationToken);
        try
        {
            var directory = Path.GetDirectoryName(statePath)
                ?? throw new InvalidOperationException("Allocator state path has no parent directory.");
            Directory.CreateDirectory(directory);
            var temporaryPath = $"{statePath}.{Guid.NewGuid():N}.tmp";
            try
            {
                await using (var stream = new FileStream(
                                 temporaryPath, FileMode.CreateNew, FileAccess.Write, FileShare.None,
                                 16 * 1024, FileOptions.Asynchronous | FileOptions.WriteThrough))
                {
                    await JsonSerializer.SerializeAsync(stream, state, JsonOptions, cancellationToken);
                    await stream.FlushAsync(cancellationToken);
                    stream.Flush(flushToDisk: true);
                }
                File.Move(temporaryPath, statePath, true);
                FlushDirectory(directory);
            }
            finally
            {
                if (File.Exists(temporaryPath)) File.Delete(temporaryPath);
            }
        }
        finally
        {
            gate.Release();
        }
    }

    private static void FlushDirectory(string directory)
    {
        if (!OperatingSystem.IsLinux()) return;
        const int openReadOnly = 0;
        const int openDirectory = 65536;
        var descriptor = open(directory, openReadOnly | openDirectory);
        if (descriptor < 0)
            throw new Win32Exception(Marshal.GetLastPInvokeError(),
                "Could not open allocator state directory for fsync.");
        try
        {
            if (fsync(descriptor) != 0)
                throw new Win32Exception(Marshal.GetLastPInvokeError(),
                    "Could not fsync allocator state directory.");
        }
        finally
        {
            close(descriptor);
        }
    }

    [DllImport("libc", SetLastError = true)]
    private static extern int open(string path, int flags);

    [DllImport("libc", SetLastError = true)]
    private static extern int fsync(int fileDescriptor);

    [DllImport("libc")]
    private static extern int close(int fileDescriptor);
}
