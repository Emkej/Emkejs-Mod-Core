param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

function Get-SupportedHubApiVersions {
    param(
        [Parameter(Mandatory = $true)][string]$ApiHeaderPath
    )

    $versions = @()
    foreach ($line in Get-Content -Path $ApiHeaderPath) {
        if ($line -match '^\s*#define\s+EMC_HUB_API_VERSION_[0-9]+\s+\(\(uint32_t\)([0-9]+)u\)') {
            $versions += [int]$Matches[1]
        }
    }

    $versions = @($versions | Sort-Object -Unique)
    if ($versions.Count -eq 0) {
        throw "No EMC_HUB_API_VERSION_* defines found in: $ApiHeaderPath"
    }

    return $versions
}

function Get-ApiHeaderStringDefine {
    param(
        [Parameter(Mandatory = $true)][string]$ApiHeaderPath,
        [Parameter(Mandatory = $true)][string]$DefineName
    )

    foreach ($line in Get-Content -Path $ApiHeaderPath) {
        if ($line -match ('^\s*#define\s+' + [Regex]::Escape($DefineName) + '\s+"([^"]+)"')) {
            return $Matches[1]
        }
    }

    throw "Required $DefineName define not found in: $ApiHeaderPath"
}

$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
if (-not $RepoDir) {
    $RepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
}
$env:KENSHI_REPO_DIR = $RepoDir

if (-not $ModName) {
    if ($env:KENSHI_MOD_NAME) {
        $ModName = $env:KENSHI_MOD_NAME
    }
    elseif ($env:MOD_NAME) {
        $ModName = $env:MOD_NAME
    }
    else {
        $ModName = Split-Path -Leaf $RepoDir
    }
}

$ctx = [pscustomobject]@{
    RepoDir = $RepoDir
}

$apiHeaderPath = Join-Path $ctx.RepoDir "include\emc\mod_hub_api.h"
$clientHeaderPath = Join-Path $ctx.RepoDir "include\emc\mod_hub_client.h"
$clientSourcePath = Join-Path $ctx.RepoDir "src\mod_hub_client.cpp"
$sdkDocPath = Join-Path $ctx.RepoDir "docs\mod-hub-sdk.md"
$sdkQuickstartDocPath = Join-Path $ctx.RepoDir "docs\mod-hub-sdk-quickstart.md"
$sampleHeaderTemplatePath = Join-Path $ctx.RepoDir "scripts\templates\mod_hub_consumer_adapter.h.template"
$sampleSourceTemplatePath = Join-Path $ctx.RepoDir "scripts\templates\mod_hub_consumer_adapter.cpp.template"

foreach ($requiredPath in @(
        $apiHeaderPath,
        $clientHeaderPath,
        $clientSourcePath,
        $sdkDocPath,
        $sdkQuickstartDocPath,
        $sampleHeaderTemplatePath,
        $sampleSourceTemplatePath)) {
    if (-not (Test-Path $requiredPath)) {
        Write-Host "ERROR: Required SDK asset missing: $requiredPath" -ForegroundColor Red
        exit 1
    }
}

$versionFile = Join-Path $ctx.RepoDir "VERSION"
if (-not $Version -and (Test-Path $versionFile)) {
    $Version = (Get-Content -Path $versionFile | Select-Object -First 1).Trim()
}
if (-not $Version) {
    $Version = "0.0.0"
}

if (-not $OutDir) {
    $OutDir = Join-Path $ctx.RepoDir "dist"
}
if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

if (-not $ZipName) {
    $ZipName = "$ModName-SDK-$Version.zip"
}
$zipPath = Join-Path $OutDir $ZipName

$stagingRoot = Join-Path $ctx.RepoDir ".packaging\$ModName-sdk"
if (Test-Path $stagingRoot) {
    Remove-Item -Path $stagingRoot -Recurse -Force
}

$bundleRootName = "$ModName-sdk-$Version"
$bundleRoot = Join-Path $stagingRoot $bundleRootName
$bundleIncludeDir = Join-Path $bundleRoot "include\emc"
$bundleSrcDir = Join-Path $bundleRoot "src"
$bundleDocsDir = Join-Path $bundleRoot "docs"
$bundleSampleDir = Join-Path $bundleRoot "samples\minimal"

foreach ($dir in @($bundleIncludeDir, $bundleSrcDir, $bundleDocsDir, $bundleSampleDir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

Copy-Item -Path $apiHeaderPath -Destination (Join-Path $bundleIncludeDir "mod_hub_api.h") -Force
Copy-Item -Path $clientHeaderPath -Destination (Join-Path $bundleIncludeDir "mod_hub_client.h") -Force
Copy-Item -Path $clientSourcePath -Destination (Join-Path $bundleSrcDir "mod_hub_client.cpp") -Force
Copy-Item -Path $sdkDocPath -Destination (Join-Path $bundleDocsDir "mod-hub-sdk.md") -Force
Copy-Item -Path $sdkQuickstartDocPath -Destination (Join-Path $bundleDocsDir "mod-hub-sdk-quickstart.md") -Force

$sampleTokens = [ordered]@{
    "__NAMESPACE_ID__" = "example.mod_hub"
    "__NAMESPACE_DISPLAY_NAME__" = "Example Mod Hub"
    "__MOD_ID__" = "example_consumer"
    "__MOD_DISPLAY_NAME__" = "Example Consumer"
}

$sampleHeaderContent = Get-Content -Path $sampleHeaderTemplatePath -Raw
$sampleSourceContent = Get-Content -Path $sampleSourceTemplatePath -Raw
foreach ($token in $sampleTokens.Keys) {
    $sampleHeaderContent = $sampleHeaderContent.Replace($token, $sampleTokens[$token])
    $sampleSourceContent = $sampleSourceContent.Replace($token, $sampleTokens[$token])
}

Set-Content -Path (Join-Path $bundleSampleDir "mod_hub_consumer_adapter.h") -Value $sampleHeaderContent -NoNewline
Set-Content -Path (Join-Path $bundleSampleDir "mod_hub_consumer_adapter.cpp") -Value $sampleSourceContent -NoNewline

$supportedHubApiVersions = @(Get-SupportedHubApiVersions -ApiHeaderPath $apiHeaderPath)
$canonicalGetApiExport = Get-ApiHeaderStringDefine -ApiHeaderPath $apiHeaderPath -DefineName "EMC_MOD_HUB_GET_API_EXPORT_NAME"
$compatGetApiExport = Get-ApiHeaderStringDefine -ApiHeaderPath $apiHeaderPath -DefineName "EMC_MOD_HUB_GET_API_COMPAT_EXPORT_NAME"
$compatRemovalTarget = Get-ApiHeaderStringDefine -ApiHeaderPath $apiHeaderPath -DefineName "EMC_MOD_HUB_GET_API_COMPAT_REMOVAL_TARGET"
$sdkMetadata = [ordered]@{
    sdk_package_version = $Version
    supported_hub_api_versions = $supportedHubApiVersions
    default_hub_api_version = [int]$supportedHubApiVersions[0]
    export_contract = [ordered]@{
        canonical_get_api_export = $canonicalGetApiExport
        compatibility_get_api_exports = @($compatGetApiExport)
        compatibility_alias_removal_target = $compatRemovalTarget
    }
    assets = [ordered]@{
        api_header = "include/emc/mod_hub_api.h"
        client_header = "include/emc/mod_hub_client.h"
        client_source = "src/mod_hub_client.cpp"
        minimal_sample_header = "samples/minimal/mod_hub_consumer_adapter.h"
        minimal_sample_source = "samples/minimal/mod_hub_consumer_adapter.cpp"
        integration_doc = "docs/mod-hub-sdk.md"
        quickstart_doc = "docs/mod-hub-sdk-quickstart.md"
    }
}
$sdkMetadata | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $bundleRoot "sdk-metadata.json")
Set-Content -Path (Join-Path $bundleRoot "VERSION") -Value $Version -NoNewline

if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}

Write-Host "Packaging SDK bundle: $bundleRoot" -ForegroundColor Yellow
Write-Host "Output:               $zipPath" -ForegroundColor Gray

Compress-Archive -Path $bundleRoot -DestinationPath $zipPath

Write-Host "SDK package created: $zipPath" -ForegroundColor Green
