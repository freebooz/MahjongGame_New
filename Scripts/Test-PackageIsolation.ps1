[CmdletBinding()]
param(
    [string]$Root = '',
    [string]$ClientReceipt = '',
    [string]$ServerReceipt = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Root)) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = [IO.Path]::GetFullPath($Root)
if (!$ClientReceipt) { $ClientReceipt = Join-Path $Root 'Binaries\Win64\GuiyangMahjongClient.target' }
if (!$ServerReceipt) { $ServerReceipt = Join-Path $Root 'Binaries\Win64\GuiyangMahjongServer.target' }

function Assert-ReceiptClean {
    param([string]$Path, [string]$Role, [string[]]$ForbiddenPatterns, [string[]]$RequiredPlugins)
    if (!(Test-Path -LiteralPath $Path)) {
        Write-Warning "$Role receipt not present yet: $Path"
        return
    }
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

Assert-ReceiptClean -Path $ClientReceipt -Role 'Client' `
    -ForbiddenPatterns @('GuiyangMahjongServer', 'Agones') -RequiredPlugins @()
Assert-ReceiptClean -Path $ServerReceipt -Role 'Server' `
    -ForbiddenPatterns @('GuiyangMahjongClient', 'GuiyangMahjongOnline', 'NNERuntimeORT', 'NNEDenoiser',
        'MsQuic', '[\\/]Content[\\/]UI[\\/]', '[\\/]Content[\\/]Art[\\/]', '[\\/]Content[\\/]Slate[\\/]') `
    -RequiredPlugins @('Agones')

Write-Host 'PACKAGE_ISOLATION_OK'

