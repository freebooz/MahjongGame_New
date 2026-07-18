param(
    [int]$LobbyPort = 28080,
    [int]$AllocatorPort = 28081,
    [int]$GameServerPortStart = 29000
)

$ErrorActionPreference = 'Stop'
$lobby = $null
$allocator = $null
$serviceToken = 'integration-allocator-service-token-which-is-long-enough'
$lobbyToken = 'integration-lobby-callback-token-which-is-long-enough'
$playerKey = 'integration-player-signing-key-which-is-long-enough'
$joinKey = 'integration-join-ticket-key-which-is-long-enough'
$root = Split-Path -Parent $PSScriptRoot
$lobbyDll = Join-Path $root 'Services/GuiyangMahjong.Lobby/bin/Release/net10.0/GuiyangMahjong.Lobby.dll'
$allocatorDll = Join-Path $root 'Services/GuiyangMahjong.Allocator/bin/Release/net10.0/GuiyangMahjong.Allocator.dll'
$fakeDll = Join-Path $root 'Services/GuiyangMahjong.FakeGameServer/bin/Release/net10.0/GuiyangMahjong.FakeGameServer.dll'

function Wait-Health([string]$Url) {
    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        try {
            $null = Invoke-RestMethod -Uri $Url -TimeoutSec 1
            return
        }
        catch {
            Start-Sleep -Milliseconds 250
        }
    }
    throw "Health check failed: $Url"
}

function ConvertTo-Base64Url([byte[]]$Bytes) {
    [Convert]::ToBase64String($Bytes).TrimEnd('=').Replace('+', '-').Replace('/', '_')
}

function New-Headers([string]$Bearer, [string]$Idempotency = '') {
    $headers = @{
        'X-Request-Id' = [Guid]::NewGuid().ToString()
        Authorization = "Bearer $Bearer"
    }
    if ($Idempotency) { $headers['Idempotency-Key'] = $Idempotency }
    $headers
}

function ConvertTo-Items([object]$Response) {
    if ($null -ne $Response.PSObject.Properties['value']) {
        @($Response.value)
        return
    }
    @($Response)
}

function Wait-Route([object]$Room, [string]$PlayerToken) {
    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        try {
            return Invoke-RestMethod `
                -Uri "http://127.0.0.1:$LobbyPort/v1/rooms/$($Room.roomCode)/route" `
                -Headers (New-Headers $PlayerToken)
        }
        catch {
            Start-Sleep -Milliseconds 250
        }
    }
    throw "Route was not published for room $($Room.roomId)"
}

try {
    $env:ASPNETCORE_ENVIRONMENT = 'Development'
    $lobbyArguments = @(
        $lobbyDll,
        "--urls=http://127.0.0.1:$LobbyPort",
        "--Lobby:TokenSigningKey=$playerKey",
        "--Lobby:JoinTicketSigningKey=$joinKey",
        "--Lobby:InternalServiceToken=$lobbyToken",
        '--Lobby:Allocator:Enabled=true',
        "--Lobby:Allocator:BaseUrl=http://127.0.0.1:$AllocatorPort",
        "--Lobby:Allocator:ServiceToken=$serviceToken",
        '--Lobby:Persistence:Mode=InMemory'
    )
    $lobby = Start-Process -FilePath 'dotnet' -ArgumentList $lobbyArguments -WindowStyle Hidden -PassThru
    Wait-Health "http://127.0.0.1:$LobbyPort/health/ready"

    $allocatorArguments = @(
        $allocatorDll,
        "--urls=http://127.0.0.1:$AllocatorPort",
        "--Allocator:PortStart=$GameServerPortStart",
        "--Allocator:PortEnd=$($GameServerPortStart + 2)",
        '--Allocator:HeartbeatTimeoutSeconds=3',
        '--Allocator:HeartbeatIntervalSeconds=1',
        '--Allocator:MonitorIntervalMilliseconds=200',
        '--Allocator:RegistrationTimeoutSeconds=10',
        '--Allocator:DrainGraceSeconds=0',
        '--Allocator:AdvertisedIp=127.0.0.1',
        '--Allocator:GameServerExecutablePath=dotnet',
        "--Allocator:GameServerPrefixArguments:0=$fakeDll",
        "--Allocator:LobbyInternalUrl=http://127.0.0.1:$LobbyPort",
        "--Allocator:ServiceToken=$serviceToken",
        "--Allocator:LobbyCallbackToken=$lobbyToken"
    )
    $allocator = Start-Process -FilePath 'dotnet' -ArgumentList $allocatorArguments -WindowStyle Hidden -PassThru
    Wait-Health "http://127.0.0.1:$AllocatorPort/health/ready"

    $tokenPayloadJson = @{
        Sub = 'integration-owner'
        Name = 'IntegrationOwner'
        Provider = 'Test'
        Exp = [DateTimeOffset]::UtcNow.AddMinutes(5).ToUnixTimeSeconds()
    } | ConvertTo-Json -Compress
    $tokenPayload = ConvertTo-Base64Url ([Text.Encoding]::UTF8.GetBytes($tokenPayloadJson))
    $hmac = [Security.Cryptography.HMACSHA256]::new([Text.Encoding]::UTF8.GetBytes($playerKey))
    try {
        $tokenSignature = ConvertTo-Base64Url (
            $hmac.ComputeHash([Text.Encoding]::ASCII.GetBytes($tokenPayload)))
    }
    finally {
        $hmac.Dispose()
    }
    $playerToken = "$tokenPayload.$tokenSignature"
    $createBody = @{
        roundCount = 4
        publicRoom = $true
        autoStart = $true
        passwordProtected = $false
        password = $null
        ruleSnapshot = @{ ruleId = 'GuiyangMainstreamV1' }
    } | ConvertTo-Json -Depth 5

    $rooms = @()
    for ($index = 0; $index -lt 2; $index++) {
        $rooms += Invoke-RestMethod `
            -Method Post `
            -Uri "http://127.0.0.1:$LobbyPort/v1/rooms" `
            -Headers (New-Headers $playerToken "integration-create-room-$index-0001") `
            -ContentType 'application/json' `
            -Body $createBody
    }
    $routes = @($rooms | ForEach-Object { Wait-Route $_ $playerToken })

    $instancesResponse = Invoke-RestMethod `
        -Uri "http://127.0.0.1:$AllocatorPort/internal/instances" `
        -Headers (New-Headers $serviceToken)
    $instancesBefore = @(ConvertTo-Items $instancesResponse)
    if (@($instancesBefore | Where-Object state -eq 'Allocated').Count -ne 2) {
        throw "Expected two allocated instances: $($instancesBefore | ConvertTo-Json -Compress)"
    }
    if (@($instancesBefore.port | Select-Object -Unique).Count -ne 2) {
        throw 'Expected unique ports.'
    }
    if (@($routes.joinTicket | Where-Object { -not $_ }).Count -ne 0) {
        throw 'Expected non-empty join tickets.'
    }

    $victim = $instancesBefore | Sort-Object port | Select-Object -First 1
    Stop-Process -Id $victim.processId -Force
    $failed = $null
    for ($attempt = 0; $attempt -lt 40 -and $null -eq $failed; $attempt++) {
        Start-Sleep -Milliseconds 250
        $currentResponse = Invoke-RestMethod `
            -Uri "http://127.0.0.1:$AllocatorPort/internal/instances" `
            -Headers (New-Headers $serviceToken)
        $current = @(ConvertTo-Items $currentResponse)
        $failed = $current | Where-Object {
            $_.serverInstanceId -eq $victim.serverInstanceId -and $_.state -eq 'Failed'
        }
    }
    if ($null -eq $failed) { throw 'Crashed instance was not marked failed.' }

    $thirdRoom = Invoke-RestMethod `
        -Method Post `
        -Uri "http://127.0.0.1:$LobbyPort/v1/rooms" `
        -Headers (New-Headers $playerToken 'integration-create-room-third-0001') `
        -ContentType 'application/json' `
        -Body $createBody
    $thirdRoute = Wait-Route $thirdRoom $playerToken
    if ($thirdRoute.serverPort -ne $victim.port) {
        throw "Expected reclaimed port $($victim.port), got $($thirdRoute.serverPort)."
    }

    [pscustomobject]@{
        Status = 'INTEGRATION_OK'
        RoomsCreated = 3
        InitialInstances = 2
        UniqueInitialPorts = 2
        FailedInstance = $victim.serverInstanceId
        ReclaimedPort = $thirdRoute.serverPort
    } | ConvertTo-Json -Compress
}
finally {
    if ($allocator -and -not $allocator.HasExited) {
        try {
            $remainingResponse = Invoke-RestMethod `
                -Uri "http://127.0.0.1:$AllocatorPort/internal/instances" `
                -Headers (New-Headers $serviceToken) `
                -TimeoutSec 2
            $remaining = @(ConvertTo-Items $remainingResponse)
            foreach ($instance in $remaining) {
                if ($instance.processId) {
                    Stop-Process -Id $instance.processId -Force -ErrorAction SilentlyContinue
                }
            }
        }
        catch {
        }
        Stop-Process -Id $allocator.Id -Force -ErrorAction SilentlyContinue
    }
    if ($lobby -and -not $lobby.HasExited) {
        Stop-Process -Id $lobby.Id -Force -ErrorAction SilentlyContinue
    }
}
