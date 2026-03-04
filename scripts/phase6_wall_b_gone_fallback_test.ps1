param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = "Stop"

Write-Host "SKIPPED: phase6_wall_b_gone_fallback_test moved to Wall-B-Gone repo (consumer-owned bridge)."
Write-Host "PASS"
