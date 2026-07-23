[CmdletBinding()]
param(
    [string]$EngineRoot = 'F:\UnrealEngine-5.8.0-release',
    [string]$ProjectPath = '',
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',
    [string]$ArtifactDirectory = '',
    [switch]$SkipCook,
    [switch]$PostProcessOnly,
    [switch]$IncludeDebugSymbols
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $root 'GuiyangMahjong.uproject'
}
if ([string]::IsNullOrWhiteSpace($ArtifactDirectory)) {
    $ArtifactDirectory = Join-Path $root 'Artifacts\LinuxServer'
}

$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$engine = (Resolve-Path -LiteralPath $EngineRoot).Path
$artifact = [IO.Path]::GetFullPath($ArtifactDirectory)
$artifactRoot = [IO.Path]::GetFullPath((Join-Path $root 'Artifacts'))
if (!$artifact.StartsWith($artifactRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ArtifactDirectory must stay below $artifactRoot"
}

$toolchain = $env:LINUX_MULTIARCH_ROOT
if ([string]::IsNullOrWhiteSpace($toolchain)) {
    $toolchain = [Environment]::GetEnvironmentVariable('LINUX_MULTIARCH_ROOT', 'Machine')
}
if ([string]::IsNullOrWhiteSpace($toolchain)) {
    $toolchain = [Environment]::GetEnvironmentVariable('LINUX_MULTIARCH_ROOT', 'User')
}
if ([string]::IsNullOrWhiteSpace($toolchain) -or !(Test-Path -LiteralPath $toolchain)) {
    throw 'UE 5.8 Linux v26 toolchain is not installed or LINUX_MULTIARCH_ROOT is missing.'
}
$toolchain = [IO.Path]::GetFullPath($toolchain)
$versionFile = Join-Path $toolchain 'ToolchainVersion.txt'
$toolchainVersion = (Get-Content -LiteralPath $versionFile -Raw).Trim()
if ($toolchainVersion -ne 'v26_clang-20.1.8-rockylinux8') {
    throw "Unexpected Linux toolchain: $toolchainVersion"
}
$env:LINUX_MULTIARCH_ROOT = $toolchain

$scratch = Join-Path $root 'Saved\StagedBuilds\LinuxServerArchive'
if (!$PostProcessOnly) {
    $runUat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'
    if (!(Test-Path -LiteralPath $runUat)) {
        throw "RunUAT.bat not found below $engine"
    }

    foreach ($path in @($scratch, $artifact)) {
        $resolved = [IO.Path]::GetFullPath($path)
        if ($resolved.StartsWith([IO.Path]::GetFullPath($root), [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $resolved)) {
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
    New-Item -ItemType Directory -Path $scratch -Force | Out-Null

    $arguments = @(
        'BuildCookRun',
        "-project=$project",
        '-noP4',
        '-utf8output',
        '-server',
        '-noclient',
        '-serverplatform=Linux',
        "-serverconfig=$Configuration",
        '-map=/Game/Maps/MahjongServerMap',
        '-build',
        '-SkipCookingEditorContent',
        '-nodebuginfo',
        '-stage',
        '-pak',
        '-archive',
        "-archivedirectory=$scratch"
    )
    if ($SkipCook) { $arguments += '-skipcook' } else { $arguments += '-cook' }

    & $runUat @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "LinuxServer BuildCookRun failed with exit code $LASTEXITCODE"
    }
} elseif (!(Test-Path -LiteralPath $scratch)) {
    throw "PostProcessOnly requires an existing archive below $scratch"
}

$packageRoot = Join-Path $scratch 'LinuxServer'
if (!(Test-Path -LiteralPath $packageRoot)) { $packageRoot = $scratch }
$serverBinary = Get-ChildItem -LiteralPath $packageRoot -Recurse -File -Filter 'GuiyangMahjongServer' |
    Where-Object { $_.FullName -match '[\\/]Binaries[\\/]Linux[\\/]GuiyangMahjongServer$' } |
    Select-Object -First 1
if ($null -eq $serverBinary) {
    throw "Packaged GuiyangMahjongServer ELF was not found below $packageRoot"
}

if (Test-Path -LiteralPath $artifact) {
    Remove-Item -LiteralPath $artifact -Recurse -Force
}
New-Item -ItemType Directory -Path $artifact -Force | Out-Null
if ($IncludeDebugSymbols) {
    Copy-Item -Path (Join-Path $packageRoot '*') -Destination $artifact -Recurse -Force
} else {
    & robocopy.exe $packageRoot $artifact /E /XF '*.debug' '*.sym' 'Manifest_DebugFiles_*.txt' /NFL /NDL /NJH /NJS /NP
    if ($LASTEXITCODE -gt 7) {
        throw "Could not copy the runtime-only LinuxServer artifact (robocopy exit code $LASTEXITCODE)."
    }
}
$packagePrefix = $packageRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
$relativeBinary = $serverBinary.FullName.Substring($packagePrefix.Length).Replace('\', '/')
$artifactBinary = Join-Path $artifact ($relativeBinary.Replace('/', [IO.Path]::DirectorySeparatorChar))
$binaryHash = (Get-FileHash -LiteralPath $artifactBinary -Algorithm SHA256).Hash.ToLowerInvariant()
$checksumLine = "$binaryHash  $relativeBinary"
$checksumLine | Set-Content -LiteralPath (Join-Path $artifact 'SHA256SUMS') -Encoding ascii
$commit = (git -C $root rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0) { $commit = 'unknown' }
$manifest = [ordered]@{
    schemaVersion = 1
    target = 'GuiyangMahjongServer'
    platform = 'Linux'
    configuration = $Configuration
    toolchain = $toolchainVersion
    gitCommit = $commit
    executable = $relativeBinary
    executableSha256 = $binaryHash
    generatedAtUtc = [DateTimeOffset]::UtcNow.ToString('O')
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $artifact 'build-manifest.json') -Encoding utf8

Write-Host "LINUX_SERVER_BUILD_OK artifact=$artifact executable=$relativeBinary toolchain=$toolchainVersion"
