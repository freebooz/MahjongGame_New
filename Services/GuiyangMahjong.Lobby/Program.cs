using GuiyangMahjong.Lobby.Api;
using GuiyangMahjong.Lobby.Options;
using GuiyangMahjong.Lobby.Realtime;
using GuiyangMahjong.Lobby.Security;
using GuiyangMahjong.Lobby.Services;
using GuiyangMahjong.Lobby.Storage;
using Microsoft.Extensions.Options;
using Npgsql;
using StackExchange.Redis;
using System.Text;

Console.OutputEncoding = Encoding.UTF8;

var builder = WebApplication.CreateBuilder(new WebApplicationOptions
{
    Args = args,
    ContentRootPath = AppContext.BaseDirectory
});

builder.Services
    .AddOptions<LobbyOptions>()
    .Bind(builder.Configuration.GetSection(LobbyOptions.SectionName))
    .ValidateDataAnnotations()
    .Validate(options => options.JoinTicketSigningKey.Length >= 32,
        "Lobby:JoinTicketSigningKey must contain at least 32 characters.")
    .Validate(options => options.InternalServiceToken.Length >= 32,
        "Lobby:InternalServiceToken must contain at least 32 characters.")
    .Validate(options => !options.Allocator.Enabled || options.Allocator.ServiceToken.Length >= 32,
        "Lobby:Allocator:ServiceToken must contain at least 32 characters when enabled.")
    .Validate(options => options.TokenSigningKey.Length >= 32, "Lobby:TokenSigningKey 至少需要 32 个字符")
    .Validate(options =>
        !builder.Environment.IsProduction()
        || !options.TokenSigningKey.StartsWith("development-only", StringComparison.OrdinalIgnoreCase),
        "生产环境禁止使用 development-only 签名密钥")
    .Validate(options =>
        !options.Persistence.Mode.Equals("RedisPostgres", StringComparison.OrdinalIgnoreCase)
        || (!string.IsNullOrWhiteSpace(options.Persistence.RedisConnectionString)
            && !string.IsNullOrWhiteSpace(options.Persistence.PostgresConnectionString)),
        "RedisPostgres 模式必须配置 Redis 与 PostgreSQL 连接字符串")
    .ValidateOnStart();

builder.Services.AddSingleton(TimeProvider.System);
builder.Services.AddSingleton<IPlayerTokenValidator, HmacPlayerTokenValidator>();
builder.Services.AddSingleton<IRoomPasswordService, RoomPasswordService>();
builder.Services.AddSingleton<IJoinTicketIssuer, HmacJoinTicketIssuer>();
builder.Services.AddHttpClient();
builder.Services.AddSingleton<IAllocatorClient>(provider =>
    provider.GetRequiredService<IOptions<LobbyOptions>>().Value.Allocator.Enabled
        ? ActivatorUtilities.CreateInstance<HttpAllocatorClient>(provider)
        : new DisabledAllocatorClient());
builder.Services.AddSingleton<LobbyEventHub>();
builder.Services.AddSingleton<ILobbyEventPublisher>(provider => provider.GetRequiredService<LobbyEventHub>());
builder.Services.AddSingleton<OnlinePresenceService>();
builder.Services.AddSingleton<InMemoryIdempotencyStore>();
builder.Services.AddSingleton<LobbyService>();
builder.Services.AddSingleton<ILobbyStore>(provider =>
{
    var lobbyOptions = provider.GetRequiredService<IOptions<LobbyOptions>>();
    var persistence = lobbyOptions.Value.Persistence;
    if (!persistence.Mode.Equals("RedisPostgres", StringComparison.OrdinalIgnoreCase))
    {
        return new InMemoryLobbyStore();
    }

    var redis = ConnectionMultiplexer.Connect(persistence.RedisConnectionString);
    var postgres = NpgsqlDataSource.Create(persistence.PostgresConnectionString);
    return new RedisPostgresLobbyStore(
        lobbyOptions,
        redis,
        postgres,
        provider.GetRequiredService<ILogger<RedisPostgresLobbyStore>>());
});
builder.Services.AddHostedService<LobbyStoreInitializer>();

var app = builder.Build();

if (!app.Environment.IsDevelopment()) app.UseHttpsRedirection();
app.UseWebSockets(new WebSocketOptions { KeepAliveInterval = TimeSpan.FromSeconds(20) });
app.UseMiddleware<RequestIdMiddleware>();
app.UseMiddleware<LobbyExceptionMiddleware>();
app.UseMiddleware<PlayerAuthenticationMiddleware>();
app.MapLobbyEndpoints();

app.Run();

public partial class Program;
