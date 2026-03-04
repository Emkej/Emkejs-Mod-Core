param(
    [string]$RepoRoot = "",
    [string]$TempRoot = ""
)

$ErrorActionPreference = "Stop"

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Extract-CodeBlockBetweenMarkers {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$StartMarker,
        [Parameter(Mandatory = $true)][string]$EndMarker,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $startIndex = $Text.IndexOf($StartMarker, [StringComparison]::Ordinal)
    Assert-Condition -Condition ($startIndex -ge 0) -Message "Missing marker: $StartMarker"

    $contentStart = $startIndex + $StartMarker.Length
    $endIndex = $Text.IndexOf($EndMarker, $contentStart, [StringComparison]::Ordinal)
    Assert-Condition -Condition ($endIndex -ge 0) -Message "Missing marker: $EndMarker"

    $section = $Text.Substring($contentStart, $endIndex - $contentStart)
    $match = [Regex]::Match($section, '```cpp\s*(?<code>[\s\S]*?)```')
    Assert-Condition -Condition $match.Success -Message "Missing cpp fenced block for $Label."

    return $match.Groups["code"].Value.Trim()
}

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
    $RepoRoot = Split-Path -Parent $scriptDir
}

if (-not (Test-Path $RepoRoot)) {
    throw "RepoRoot not found: $RepoRoot"
}

$docsPath = Join-Path $RepoRoot "docs\mod-hub-sdk.md"
Assert-Condition -Condition (Test-Path $docsPath) -Message "Missing docs file: $docsPath"
$docsText = Get-Content -Path $docsPath -Raw

$requiredSymbols = @(
    "EMC_HUB_API_VERSION_1",
    "EMC_HUB_API_V1_MIN_SIZE",
    "EMC_Result",
    "EMC_KeybindValueV1",
    "EMC_ACTION_FORCE_REFRESH",
    "EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT",
    "MOD_HUB_CLIENT_SETTING_KIND_BOOL",
    "MOD_HUB_CLIENT_SETTING_KIND_KEYBIND",
    "MOD_HUB_CLIENT_SETTING_KIND_INT",
    "MOD_HUB_CLIENT_SETTING_KIND_FLOAT",
    "MOD_HUB_CLIENT_SETTING_KIND_ACTION",
    "OnStartup()",
    "OnOptionsWindowInit()",
    "UseHubUi()"
)

foreach ($symbol in $requiredSymbols) {
    Assert-Condition -Condition ($docsText.Contains($symbol)) -Message "docs/mod-hub-sdk.md missing symbol: $symbol"
}

$requiredLogEvents = @(
    "event=hub_commit_failure",
    "event=hub_commit_summary",
    "event=hub_ui_get_failure",
    "event=hub_registration_warning",
    "event=hub_registration_rejected",
    "event=hub_setting_registration_conflict",
    "event=hub_action_failure",
    "event=hub_action_refresh_get_failure",
    "event=hub_commit_get_failure"
)

foreach ($eventName in $requiredLogEvents) {
    Assert-Condition -Condition ($docsText.Contains($eventName)) -Message "docs/mod-hub-sdk.md missing log event: $eventName"
}

$headerCode = Extract-CodeBlockBetweenMarkers `
    -Text $docsText `
    -StartMarker "<!-- PHASE11_SAMPLE_HEADER_BEGIN -->" `
    -EndMarker "<!-- PHASE11_SAMPLE_HEADER_END -->" `
    -Label "sample header"

$sourceCode = Extract-CodeBlockBetweenMarkers `
    -Text $docsText `
    -StartMarker "<!-- PHASE11_SAMPLE_SOURCE_BEGIN -->" `
    -EndMarker "<!-- PHASE11_SAMPLE_SOURCE_END -->" `
    -Label "sample source"

Assert-Condition -Condition (-not $sourceCode.Contains("src/hub_")) -Message "Phase11 sample should not reference src/hub_* internals."
Assert-Condition -Condition (-not $sourceCode.Contains('#include "hub_')) -Message "Phase11 sample should not include hub_* headers."

if (-not $TempRoot) {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "emc_phase11_sdk_docs"
}
if (-not (Test-Path $TempRoot)) {
    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null
}

$runRoot = Join-Path $TempRoot ([Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

try {
    $sampleHeaderPath = Join-Path $runRoot "mod_hub_consumer_adapter.h"
    $sampleSourcePath = Join-Path $runRoot "mod_hub_consumer_adapter.cpp"
    $sampleHeaderCode = $headerCode.Trim() + [Environment]::NewLine
    $sampleSourceCode = $sourceCode.Trim() + [Environment]::NewLine
    Set-Content -Path $sampleHeaderPath -Value $sampleHeaderCode -NoNewline
    Set-Content -Path $sampleSourcePath -Value $sampleSourceCode -NoNewline

    $includePath = Join-Path $RepoRoot "include"
    $helperSourcePath = Join-Path $RepoRoot "src\mod_hub_client.cpp"
    Assert-Condition -Condition (Test-Path $helperSourcePath) -Message "Missing helper source: $helperSourcePath"

    $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
    if ($null -ne $cl) {
        $sampleObj = Join-Path $runRoot "sample.obj"
        $helperObj = Join-Path $runRoot "mod_hub_client.obj"

        & $cl.Source /nologo /c /EHsc /I"$includePath" $sampleSourcePath /Fo"$sampleObj" | Out-Null
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile phase11 sample source."
        Assert-Condition -Condition (Test-Path $sampleObj) -Message "cl.exe did not produce phase11 sample object."

        & $cl.Source /nologo /c /EHsc /I"$includePath" $helperSourcePath /Fo"$helperObj" | Out-Null
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile mod_hub_client.cpp for phase11."
        Assert-Condition -Condition (Test-Path $helperObj) -Message "cl.exe did not produce mod_hub_client.cpp object."
    }
    else {
        $gpp = Get-Command "g++" -ErrorAction SilentlyContinue
        Assert-Condition -Condition ($null -ne $gpp) -Message "No compiler found (neither cl.exe nor g++)."

        $sampleObj = Join-Path $runRoot "sample.o"
        $helperObj = Join-Path $runRoot "mod_hub_client.o"

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $sampleSourcePath "-o" $sampleObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile phase11 sample source."
        Assert-Condition -Condition (Test-Path $sampleObj) -Message "g++ did not produce phase11 sample object."

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $helperSourcePath "-o" $helperObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile mod_hub_client.cpp for phase11."
        Assert-Condition -Condition (Test-Path $helperObj) -Message "g++ did not produce mod_hub_client.cpp object."
    }

    Write-Host "PASS"
}
finally {
    if (Test-Path $runRoot) {
        Remove-Item -Path $runRoot -Recurse -Force
    }
}
