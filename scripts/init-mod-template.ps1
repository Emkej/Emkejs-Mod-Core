# Local wrapper: delegates to shared scripts submodule.
param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [switch]$WithHub,
    [string]$HubNamespaceId = "",
    [string]$HubNamespaceDisplayName = "",
    [string]$HubModId = "",
    [string]$HubModDisplayName = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$LocalRepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
if (-not $RepoDir) { $RepoDir = $LocalRepoDir }
$env:KENSHI_REPO_DIR = $RepoDir
$SharedScript = Join-Path $LocalRepoDir "tools\build-scripts\init-mod-template.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Run: git submodule update --init --recursive" -ForegroundColor Yellow
    exit 1
}

$Forward = @{}
foreach ($k in @('RepoDir','ModName','DllName','ModFileName','ConfigFileName')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

& $SharedScript @Forward
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $WithHub) {
    exit 0
}

$CommonScript = Join-Path $LocalRepoDir "tools\build-scripts\kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Shared helper not found: $CommonScript" -ForegroundColor Red
    exit 1
}

. $CommonScript
$resolved = Resolve-KenshiBuildContext -BoundParameters $PSBoundParameters -RepoDir $RepoDir -ModName $ModName -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName

$safeModToken = ($resolved.ModName.ToLowerInvariant() -replace '[^a-z0-9]+', '_').Trim('_')
if (-not $safeModToken) { $safeModToken = "mod" }

if (-not $HubNamespaceId) { $HubNamespaceId = "example.$safeModToken" }
if (-not $HubNamespaceDisplayName) { $HubNamespaceDisplayName = "$($resolved.ModName) Namespace" }
if (-not $HubModId) { $HubModId = $safeModToken }
if (-not $HubModDisplayName) { $HubModDisplayName = $resolved.ModName }

$templatePath = Join-Path $LocalRepoDir "scripts\templates\mod_hub_consumer_adapter.cpp.template"
if (-not (Test-Path $templatePath)) {
    Write-Host "ERROR: Hub scaffold template not found: $templatePath" -ForegroundColor Red
    exit 1
}

$srcDir = Join-Path $RepoDir "src"
if (-not (Test-Path $srcDir)) {
    New-Item -ItemType Directory -Path $srcDir -Force | Out-Null
}

$destPath = Join-Path $srcDir "mod_hub_consumer_adapter.cpp"
if (Test-Path $destPath) {
    Write-Host "Hub scaffold exists, skipping: $destPath" -ForegroundColor Yellow
    exit 0
}

$template = Get-Content -Path $templatePath -Raw
$content = $template.Replace("__NAMESPACE_ID__", $HubNamespaceId)
$content = $content.Replace("__NAMESPACE_DISPLAY_NAME__", $HubNamespaceDisplayName)
$content = $content.Replace("__MOD_ID__", $HubModId)
$content = $content.Replace("__MOD_DISPLAY_NAME__", $HubModDisplayName)
$content | Set-Content -Path $destPath -NoNewline

Write-Host "Created Hub scaffold: $destPath" -ForegroundColor Gray
exit 0
