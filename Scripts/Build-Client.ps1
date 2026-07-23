[CmdletBinding()]
param(
    [string]$EngineRoot = 'F:\UnrealEngine-5.8.0-release',
    [ValidateSet('Win64', 'Android')]
    [string]$Platform = 'Win64',
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',
    [string]$StagingDirectory = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'GuiyangMahjong.uproject'
$uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if ([string]::IsNullOrWhiteSpace($StagingDirectory)) {
    $StagingDirectory = Join-Path $root "Saved\StagedBuilds\${Platform}Client"
}
foreach ($required in @($project, $uat)) {
    if (!(Test-Path -LiteralPath $required)) { throw "Required path not found: $required" }
}

& $uat BuildCookRun `
    "-project=$project" `
    -noP4 `
    -utf8output `
    -client `
    -clienttarget=GuiyangMahjongClient `
    "-clientplatform=$Platform" `
    "-clientconfig=$Configuration" `
    -build `
    -cook `
    -stage `
    -pak `
    -SkipCookingEditorContent `
    -nodebuginfo `
    "-stagingdirectory=$StagingDirectory"
if ($LASTEXITCODE -ne 0) { throw "Client package failed with exit code $LASTEXITCODE" }

& (Join-Path $PSScriptRoot 'Test-PackageIsolation.ps1') -Root $root
Write-Host "CLIENT_PACKAGE_OK target=GuiyangMahjongClient platform=$Platform stage=$StagingDirectory"

