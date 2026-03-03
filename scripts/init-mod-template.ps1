# Local wrapper: delegates to shared scripts submodule.
param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$KenshiPath = "",
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
foreach ($k in @('RepoDir','ModName','DllName','ModFileName','ConfigFileName','KenshiPath')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

if ($PSBoundParameters.ContainsKey('KenshiPath')) {
    $hadKenshiPath = Test-Path Env:KENSHI_PATH
    $hadKenshiDefaultPath = Test-Path Env:KENSHI_DEFAULT_PATH
    $previousKenshiPath = $env:KENSHI_PATH
    $previousKenshiDefaultPath = $env:KENSHI_DEFAULT_PATH

    $env:KENSHI_PATH = $KenshiPath
    $env:KENSHI_DEFAULT_PATH = $KenshiPath

    try {
        & $SharedScript @Forward
    }
    finally {
        if ($hadKenshiPath) {
            $env:KENSHI_PATH = $previousKenshiPath
        }
        else {
            Remove-Item -Path Env:KENSHI_PATH -ErrorAction SilentlyContinue
        }

        if ($hadKenshiDefaultPath) {
            $env:KENSHI_DEFAULT_PATH = $previousKenshiDefaultPath
        }
        else {
            Remove-Item -Path Env:KENSHI_DEFAULT_PATH -ErrorAction SilentlyContinue
        }
    }
}
else {
    & $SharedScript @Forward
}

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
$resolved = Resolve-KenshiBuildContext `
    -BoundParameters $PSBoundParameters `
    -RepoDir $RepoDir `
    -ModName $ModName `
    -DllName $DllName `
    -ModFileName $ModFileName `
    -ConfigFileName $ConfigFileName `
    -KenshiPath $KenshiPath

$safeModToken = ($resolved.ModName.ToLowerInvariant() -replace '[^a-z0-9]+', '_').Trim('_')
if (-not $safeModToken) { $safeModToken = "mod" }

if (-not $HubNamespaceId) { $HubNamespaceId = "example.$safeModToken" }
if (-not $HubNamespaceDisplayName) { $HubNamespaceDisplayName = "$($resolved.ModName) Namespace" }
if (-not $HubModId) { $HubModId = $safeModToken }
if (-not $HubModDisplayName) { $HubModDisplayName = $resolved.ModName }

$srcDir = Join-Path $RepoDir "src"
if (-not (Test-Path $srcDir)) {
    New-Item -ItemType Directory -Path $srcDir -Force | Out-Null
}

$templateSpecs = @(
    @{
        TemplatePath = Join-Path $LocalRepoDir "scripts\templates\mod_hub_consumer_adapter.h.template"
        DestinationPath = Join-Path $srcDir "mod_hub_consumer_adapter.h"
    },
    @{
        TemplatePath = Join-Path $LocalRepoDir "scripts\templates\mod_hub_consumer_adapter.cpp.template"
        DestinationPath = Join-Path $srcDir "mod_hub_consumer_adapter.cpp"
    }
)

$createdCount = 0
foreach ($spec in $templateSpecs) {
    if (-not (Test-Path $spec.TemplatePath)) {
        Write-Host "ERROR: Hub scaffold template not found: $($spec.TemplatePath)" -ForegroundColor Red
        exit 1
    }

    if (Test-Path $spec.DestinationPath) {
        Write-Host "Hub scaffold exists, skipping: $($spec.DestinationPath)" -ForegroundColor Yellow
        continue
    }

    $template = Get-Content -Path $spec.TemplatePath -Raw
    $content = $template.Replace("__NAMESPACE_ID__", $HubNamespaceId)
    $content = $content.Replace("__NAMESPACE_DISPLAY_NAME__", $HubNamespaceDisplayName)
    $content = $content.Replace("__MOD_ID__", $HubModId)
    $content = $content.Replace("__MOD_DISPLAY_NAME__", $HubModDisplayName)
    $content | Set-Content -Path $spec.DestinationPath -NoNewline
    Write-Host "Created Hub scaffold: $($spec.DestinationPath)" -ForegroundColor Gray
    $createdCount += 1
}

if ($createdCount -eq 0) {
    Write-Host "Hub scaffold unchanged: all target files already exist." -ForegroundColor Yellow
}

exit 0
