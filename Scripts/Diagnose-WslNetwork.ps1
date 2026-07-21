[CmdletBinding()]
param(
    [string]$Distribution = 'Ubuntu-22.04',
    [string]$RepositoryPath = '/home/freebooz/src/MahjongGame'
)

$ErrorActionPreference = 'Stop'
Write-Host '== WSL distributions =='
wsl.exe --list --verbose

Write-Host '== Windows LAN addresses =='
Get-NetIPAddress -AddressFamily IPv4 |
    Where-Object { $_.AddressState -eq 'Preferred' -and $_.IPAddress -notlike '127.*' } |
    Select-Object InterfaceAlias, IPAddress, PrefixLength |
    Format-Table -AutoSize

Write-Host '== Relevant Windows listeners =='
Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue |
    Where-Object LocalPort -in 18080, 18081, 18082 |
    Select-Object LocalAddress, LocalPort, OwningProcess |
    Format-Table -AutoSize

Write-Host '== WSL Hyper-V firewall =='
$wslCreatorId = '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}'
if (Get-Command Get-NetFirewallHyperVVMSetting -ErrorAction SilentlyContinue) {
    Get-NetFirewallHyperVVMSetting -PolicyStore ActiveStore -Name $wslCreatorId |
        Select-Object Enabled, DefaultInboundAction, DefaultOutboundAction, LoopbackEnabled |
        Format-List
    Get-NetFirewallHyperVRule -PolicyStore ActiveStore -ErrorAction SilentlyContinue |
        Where-Object Name -Like 'GuiyangMahjong-*' |
        Select-Object Name, Direction, Action, Protocol, LocalPorts, RemoteAddresses, Enabled |
        Format-Table -AutoSize
}

Write-Host '== Linux network and service diagnostics =='
$linuxCommand = "cd '$RepositoryPath' && sudo ./Scripts/Linux/diagnose-network.sh"
wsl.exe -d $Distribution -- bash -lc $linuxCommand
if ($LASTEXITCODE -ne 0) {
    throw "WSL network diagnostics failed with exit code $LASTEXITCODE"
}
