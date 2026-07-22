using System.Net.Http.Headers;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

public sealed record AgonesAllocationSpec(
    string RoomId,
    string MatchId,
    string ServerInstanceId,
    string RegistrationCredential,
    string LobbyInternalUrl,
    string BuildVersion);

public sealed record AgonesAllocationResult(string GameServerName, string Address, int Port);

public interface IAgonesAllocationClient
{
    Task<AgonesAllocationResult> AllocateAsync(AgonesAllocationSpec spec, CancellationToken cancellationToken);
    Task<string?> GetGameServerStateAsync(string gameServerName, CancellationToken cancellationToken);
    Task ShutdownAsync(string gameServerName, CancellationToken cancellationToken);
    Task<bool> CheckReadyAsync(CancellationToken cancellationToken);
}

public sealed class KubernetesAgonesAllocationClient : IAgonesAllocationClient, IDisposable
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web);
    private readonly AllocatorOptions options;
    private readonly HttpClient client;

    public KubernetesAgonesAllocationClient(IOptions<AllocatorOptions> options)
    {
        this.options = options.Value;
        var agones = this.options.Agones;
        var handler = new HttpClientHandler();
        if (File.Exists(agones.ServiceAccountCaPath))
        {
            // Kubernetes projects a CA certificate only; CreateFromPemFile also attempts to
            // locate a private key and rejects this valid service-account trust bundle.
            var root = X509Certificate2.CreateFromPem(
                File.ReadAllText(agones.ServiceAccountCaPath));
            handler.ServerCertificateCustomValidationCallback = (_, certificate, chain, errors) =>
            {
                if (certificate is null || chain is null) return false;
                chain.ChainPolicy.TrustMode = X509ChainTrustMode.CustomRootTrust;
                chain.ChainPolicy.CustomTrustStore.Add(root);
                chain.ChainPolicy.RevocationMode = X509RevocationMode.NoCheck;
                return chain.Build(new X509Certificate2(certificate))
                       && errors is SslPolicyErrors.None or SslPolicyErrors.RemoteCertificateChainErrors;
            };
        }
        client = new HttpClient(handler)
        {
            BaseAddress = new Uri(agones.ApiServer.TrimEnd('/') + "/", UriKind.Absolute),
            Timeout = TimeSpan.FromSeconds(agones.RequestTimeoutSeconds)
        };
    }

    public async Task<AgonesAllocationResult> AllocateAsync(
        AgonesAllocationSpec spec, CancellationToken cancellationToken)
    {
        var annotations = new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["mahjong.freebooz/room-id"] = spec.RoomId,
            ["mahjong.freebooz/match-id"] = spec.MatchId,
            ["mahjong.freebooz/server-instance-id"] = spec.ServerInstanceId,
            ["mahjong.freebooz/registration-credential"] = spec.RegistrationCredential,
            ["mahjong.freebooz/lobby-internal-url"] = spec.LobbyInternalUrl,
            ["mahjong.freebooz/build-version"] = spec.BuildVersion
        };
        var body = new
        {
            apiVersion = "allocation.agones.dev/v1",
            kind = "GameServerAllocation",
            metadata = new { generateName = "guiyang-mahjong-", @namespace = options.Agones.Namespace },
            spec = new
            {
                scheduling = "Packed",
                selectors = new[] { new { matchLabels = new Dictionary<string, string>
                {
                    ["agones.dev/fleet"] = options.Agones.FleetName,
                    ["mahjong.freebooz/game"] = "guiyang-zhua-ji"
                } } },
                metadata = new { labels = new Dictionary<string, string>
                {
                    ["mahjong.freebooz/allocation-source"] = "lobby"
                }, annotations }
            }
        };
        using var request = await CreateRequestAsync(
            HttpMethod.Post,
            $"apis/allocation.agones.dev/v1/namespaces/{Uri.EscapeDataString(options.Agones.Namespace)}/gameserverallocations",
            cancellationToken);
        request.Content = new StringContent(JsonSerializer.Serialize(body, JsonOptions), Encoding.UTF8, "application/json");
        using var response = await client.SendAsync(request, cancellationToken);
        var payload = await response.Content.ReadAsStringAsync(cancellationToken);
        if (!response.IsSuccessStatusCode)
            throw new HttpRequestException($"Agones allocation failed with HTTP {(int)response.StatusCode}.");
        using var document = JsonDocument.Parse(payload);
        var status = document.RootElement.GetProperty("status");
        if (!string.Equals(status.GetProperty("state").GetString(), "Allocated", StringComparison.Ordinal))
            throw new InvalidOperationException("Agones did not allocate a GameServer.");
        var name = status.GetProperty("gameServerName").GetString();
        var address = status.GetProperty("address").GetString();
        var port = status.GetProperty("ports").EnumerateArray()
            .OrderByDescending(item => string.Equals(item.GetProperty("name").GetString(), "game", StringComparison.Ordinal))
            .Select(item => item.GetProperty("port").GetInt32())
            .FirstOrDefault();
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(address) || port is < 1 or > 65535)
            throw new InvalidDataException("Agones returned an incomplete allocation response.");
        return new AgonesAllocationResult(name, address, port);
    }

    public async Task ShutdownAsync(string gameServerName, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(gameServerName)) return;
        using var request = await CreateRequestAsync(
            HttpMethod.Delete,
            $"apis/agones.dev/v1/namespaces/{Uri.EscapeDataString(options.Agones.Namespace)}/gameservers/{Uri.EscapeDataString(gameServerName)}",
            cancellationToken);
        using var response = await client.SendAsync(request, cancellationToken);
        if (!response.IsSuccessStatusCode && response.StatusCode != System.Net.HttpStatusCode.NotFound)
            throw new HttpRequestException($"Agones shutdown failed with HTTP {(int)response.StatusCode}.");
    }

    public async Task<string?> GetGameServerStateAsync(
        string gameServerName, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(gameServerName)) return null;
        using var request = await CreateRequestAsync(
            HttpMethod.Get,
            $"apis/agones.dev/v1/namespaces/{Uri.EscapeDataString(options.Agones.Namespace)}/gameservers/{Uri.EscapeDataString(gameServerName)}",
            cancellationToken);
        using var response = await client.SendAsync(request, cancellationToken);
        if (response.StatusCode == System.Net.HttpStatusCode.NotFound) return null;
        if (!response.IsSuccessStatusCode)
            throw new HttpRequestException($"Agones reconciliation failed with HTTP {(int)response.StatusCode}.");
        await using var payload = await response.Content.ReadAsStreamAsync(cancellationToken);
        using var document = await JsonDocument.ParseAsync(payload, cancellationToken: cancellationToken);
        return document.RootElement.GetProperty("status").GetProperty("state").GetString();
    }

    public async Task<bool> CheckReadyAsync(CancellationToken cancellationToken)
    {
        try
        {
            using var request = await CreateRequestAsync(
                HttpMethod.Get,
                $"apis/agones.dev/v1/namespaces/{Uri.EscapeDataString(options.Agones.Namespace)}/fleets/{Uri.EscapeDataString(options.Agones.FleetName)}",
                cancellationToken);
            using var response = await client.SendAsync(request, cancellationToken);
            return response.IsSuccessStatusCode;
        }
        catch (Exception exception) when (exception is IOException
                                           or UnauthorizedAccessException
                                           or InvalidDataException
                                           or HttpRequestException
                                           or TaskCanceledException)
        {
            return false;
        }
    }

    private async Task<HttpRequestMessage> CreateRequestAsync(
        HttpMethod method, string relativeUrl, CancellationToken cancellationToken)
    {
        var token = (await File.ReadAllTextAsync(
            options.Agones.ServiceAccountTokenPath, cancellationToken)).Trim();
        if (token.Length < 16) throw new InvalidDataException("Kubernetes service account token is unavailable.");
        var request = new HttpRequestMessage(method, relativeUrl);
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", token);
        request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
        return request;
    }

    public void Dispose() => client.Dispose();
}

public sealed class DisabledAgonesAllocationClient : IAgonesAllocationClient
{
    public Task<AgonesAllocationResult> AllocateAsync(AgonesAllocationSpec spec, CancellationToken cancellationToken) =>
        throw new InvalidOperationException("Agones allocator backend is disabled.");
    public Task<string?> GetGameServerStateAsync(string gameServerName, CancellationToken cancellationToken) =>
        Task.FromResult<string?>(null);
    public Task ShutdownAsync(string gameServerName, CancellationToken cancellationToken) => Task.CompletedTask;
    public Task<bool> CheckReadyAsync(CancellationToken cancellationToken) => Task.FromResult(false);
}
