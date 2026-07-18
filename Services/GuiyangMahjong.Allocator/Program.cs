using System.Text;
using GuiyangMahjong.Allocator.Api;
using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using GuiyangMahjong.Allocator.Security;
using GuiyangMahjong.Allocator.Services;

Console.OutputEncoding = Encoding.UTF8;

var builder = WebApplication.CreateBuilder(new WebApplicationOptions
{
    Args = args,
    ContentRootPath = AppContext.BaseDirectory
});

builder.Services
    .AddOptions<AllocatorOptions>()
    .Bind(builder.Configuration.GetSection(AllocatorOptions.SectionName))
    .ValidateDataAnnotations()
    .Validate(options => options.PortEnd >= options.PortStart, "Allocator port range is invalid.")
    .Validate(options => !string.IsNullOrWhiteSpace(options.GameServerExecutablePath),
        "Allocator GameServerExecutablePath is required.")
    .ValidateOnStart();

builder.Services.AddSingleton(TimeProvider.System);
builder.Services.AddHttpClient();
builder.Services.AddSingleton<PortLeasePool>();
builder.Services.AddSingleton<InstanceCredentialService>();
builder.Services.AddSingleton<IGameServerProcessLauncher, GameServerProcessLauncher>();
builder.Services.AddSingleton<IInstanceFailureNotifier, LobbyInstanceFailureNotifier>();
builder.Services.AddSingleton<MatchResultOutboxRecovery>();
builder.Services.AddSingleton<GameServerInstanceManager>();
builder.Services.AddHostedService<InstanceMonitorService>();
builder.Services.AddHostedService<MatchResultOutboxRecoveryService>();

var app = builder.Build();
app.UseMiddleware<AllocatorExceptionMiddleware>();
app.UseMiddleware<AllocatorServiceAuthenticationMiddleware>();
app.MapAllocatorEndpoints();
app.Run();

public partial class Program;
