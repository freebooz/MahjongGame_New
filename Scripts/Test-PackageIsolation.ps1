[CmdletBinding()]
param(
    [string]$Root = '',
    [string]$ClientReceipt = '',
    [string]$ServerReceipt = '',
    [ValidateSet('Client', 'Server', 'Both')]
    [string]$Role = 'Both'
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Root)) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = [IO.Path]::GetFullPath($Root)
if (!$ClientReceipt) { $ClientReceipt = Join-Path $Root 'Binaries\Win64\GuiyangMahjongClient.target' }
if (!$ServerReceipt) { $ServerReceipt = Join-Path $Root 'Binaries\Win64\GuiyangMahjongServer.target' }

$projectDescriptor = Get-Content -LiteralPath (Join-Path $Root 'GuiyangMahjong.uproject') -Raw | ConvertFrom-Json
if (!$projectDescriptor.DisableEnginePluginsByDefault) {
    throw 'Engine plugins must be disabled by default and explicitly allow-listed.'
}
$moduleTypes = @{}
foreach ($module in $projectDescriptor.Modules) { $moduleTypes[$module.Name] = $module.Type }
if ($moduleTypes.GuiyangMahjongClient -ne 'ClientOnly' -or
    $moduleTypes.GuiyangMahjongOnline -ne 'ClientOnly' -or
    $moduleTypes.GuiyangMahjongServer -ne 'ServerOnly') {
    throw 'Client, Online, and Server modules are not target-isolated in GuiyangMahjong.uproject.'
}
$agonesReference = $projectDescriptor.Plugins | Where-Object Name -eq 'Agones'
if (!$agonesReference.Enabled -or $agonesReference.TargetAllowList -contains 'Client') {
    throw 'Agones must be enabled only for Server/Editor targets.'
}
$defaultGame = Get-Content -LiteralPath (Join-Path $Root 'Config\DefaultGame.ini') -Raw
if ($defaultGame -match 'DirectoriesToAlwaysCook|MapsToCook') {
    throw 'DefaultGame.ini contains global Cook roots; move them to target-platform configs.'
}

if ($Role -in @('Client', 'Both')) {
    foreach ($config in @('Config\Windows\WindowsGame.ini', 'Config\Android\AndroidGame.ini')) {
        $text = Get-Content -LiteralPath (Join-Path $Root $config) -Raw
        if ($text -notmatch '/Game/UI' -or $text -notmatch '/Game/Art/Mahjong' -or
            $text -notmatch '/Game/Maps/MahjongRoomMap') {
            throw "Client Cook allow-list is incomplete: $config"
        }
    }
}
if ($Role -in @('Server', 'Both')) {
    foreach ($config in @('Config\WindowsServer\WindowsServerGame.ini',
        'Config\LinuxServer\LinuxServerGame.ini')) {
        $text = Get-Content -LiteralPath (Join-Path $Root $config) -Raw
        if ($text -notmatch '/Game/Maps/MahjongServerMap' -or $text -notmatch '/Game/UI' -or
            $text -notmatch '/Game/Art') {
            throw "Server Cook isolation is incomplete: $config"
        }
    }
}

function Assert-ReceiptClean {
    param([string]$Path, [string]$Role, [string[]]$ForbiddenPatterns, [string[]]$RequiredPlugins)
    if (!(Test-Path -LiteralPath $Path)) { throw "$Role receipt is missing: $Path" }
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $searchable = @($receipt.BuildPlugins) + @($receipt.BuildProducts.Path) + @($receipt.RuntimeDependencies.Path)
    foreach ($pattern in $ForbiddenPatterns) {
        $matches = @($searchable | Where-Object { $_ -match $pattern })
        if ($matches.Count -gt 0) {
            throw "$Role isolation failed: forbidden '$pattern' found: $($matches[0])"
        }
    }
    foreach ($plugin in $RequiredPlugins) {
        if ($receipt.BuildPlugins -notcontains $plugin) {
            throw "$Role isolation failed: required plugin '$plugin' is absent"
        }
    }
}

if ($Role -in @('Client', 'Both')) {
    Assert-ReceiptClean -Path $ClientReceipt -Role 'Client' `
        -ForbiddenPatterns @('GuiyangMahjongServer', 'Agones') -RequiredPlugins @()
}
if ($Role -in @('Server', 'Both')) {
    Assert-ReceiptClean -Path $ServerReceipt -Role 'Server' `
        -ForbiddenPatterns @('GuiyangMahjongClient', 'GuiyangMahjongOnline', 'NNERuntimeORT', 'NNEDenoiser',
            'MsQuic', '[\\/]Content[\\/]UI[\\/]', '[\\/]Content[\\/]Art[\\/]', '[\\/]Content[\\/]Slate[\\/]') `
        -RequiredPlugins @('Agones')
}

Write-Host 'PACKAGE_ISOLATION_OK'
