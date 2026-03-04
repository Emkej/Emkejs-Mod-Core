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
$quickstartDocsPath = Join-Path $RepoRoot "docs\mod-hub-sdk-quickstart.md"
Assert-Condition -Condition (Test-Path $quickstartDocsPath) -Message "Missing quick-start docs file: $quickstartDocsPath"
$quickstartDocsText = Get-Content -Path $quickstartDocsPath -Raw

$requiredSymbols = @(
    "EMC_ModHub_GetApi",
    "EMC_ModHub_GetApi_v1_compat",
    "EMC_HUB_API_VERSION_1",
    "EMC_HUB_API_V1_MIN_SIZE",
    "EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE",
    "EMC_MOD_HUB_GET_API_EXPORT_NAME",
    "EMC_MOD_HUB_GET_API_COMPAT_EXPORT_NAME",
    "EMC_MOD_HUB_GET_API_COMPAT_REMOVAL_TARGET",
    "EMC_Result",
    "EMC_OK",
    "EMC_ERR_INVALID_ARGUMENT",
    "EMC_ERR_UNSUPPORTED_VERSION",
    "EMC_ERR_API_SIZE_MISMATCH",
    "EMC_ERR_CONFLICT",
    "EMC_ERR_NOT_FOUND",
    "EMC_ERR_CALLBACK_FAILED",
    "EMC_ERR_INTERNAL",
    "EMC_OptionsWindowInitObserverFn",
    "register_options_window_init_observer",
    "unregister_options_window_init_observer",
    "EMC_KeybindValueV1",
    "EMC_KEY_UNBOUND",
    "EMC_ACTION_FORCE_REFRESH",
    "EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT",
    "RegisterSettingsTableV1",
    "MOD_HUB_CLIENT_SETTING_KIND_BOOL",
    "MOD_HUB_CLIENT_SETTING_KIND_KEYBIND",
    "MOD_HUB_CLIENT_SETTING_KIND_INT",
    "MOD_HUB_CLIENT_SETTING_KIND_FLOAT",
    "MOD_HUB_CLIENT_SETTING_KIND_ACTION",
    "OnStartup()",
    "OnOptionsWindowInit()",
    "UseHubUi()",
    "IsAttachRetryPending()",
    "HasAttachRetryAttempted()",
    "LastAttemptFailureResult()"
)

foreach ($symbol in $requiredSymbols) {
    Assert-Condition -Condition ($docsText.Contains($symbol)) -Message "docs/mod-hub-sdk.md missing symbol: $symbol"
}

Assert-Condition -Condition ($docsText.Contains("mod-hub-sdk-quickstart.md")) -Message "docs/mod-hub-sdk.md should reference the quick-start guide."
Assert-Condition -Condition ($docsText.Contains("legacy compatibility fallback")) -Message "docs/mod-hub-sdk.md should describe the legacy options-init fallback."
Assert-Condition -Condition ($docsText.Contains("Callbacks run on the main thread")) -Message "docs/mod-hub-sdk.md should document the observer callback thread contract."

$requiredLogEvents = @(
    "event=hub_commit_failure",
    "event=hub_commit_summary",
    "event=hub_ui_get_failure",
    "event=hub_registration_warning",
    "event=hub_registration_rejected",
    "event=hub_setting_registration_conflict",
    "event=hub_action_failure",
    "event=hub_action_refresh_get_failure",
    "event=hub_commit_get_failure",
    "event=hub_get_api_alias_deprecated"
)

foreach ($eventName in $requiredLogEvents) {
    Assert-Condition -Condition ($docsText.Contains($eventName)) -Message "docs/mod-hub-sdk.md missing log event: $eventName"
}

$requiredQuickstartSymbols = @(
    "init-mod-template.ps1 -WithHub",
    "init-mod-template.sh --with-hub",
    "HubNamespaceId",
    "HubModId",
    "ModHubConsumerAdapter_OnStartup()",
    "ModHubConsumerAdapter_OnOptionsWindowInit()",
    "ModHubConsumerAdapter_ShouldCreateLocalTab()",
    "no per-mod options-init RVA hook is required"
)

foreach ($symbol in $requiredQuickstartSymbols) {
    Assert-Condition -Condition ($quickstartDocsText.Contains($symbol)) -Message "docs/mod-hub-sdk-quickstart.md missing symbol: $symbol"
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
Assert-Condition -Condition ($headerCode.Contains("ModHubConsumerAdapter_IsAttachRetryPending")) -Message "Phase11 sample header missing retry-pending accessor."
Assert-Condition -Condition ($headerCode.Contains("ModHubConsumerAdapter_HasAttachRetryAttempted")) -Message "Phase11 sample header missing retry-attempted accessor."
Assert-Condition -Condition ($headerCode.Contains("ModHubConsumerAdapter_LastAttachFailureResult")) -Message "Phase11 sample header missing last-failure accessor."
Assert-Condition -Condition ($headerCode.Contains("older hub builds that do not expose observer registration")) -Message "Phase11 sample header should describe legacy observer fallback wiring."
Assert-Condition -Condition ($sourceCode.Contains("return !ModHubConsumerAdapter_UseHubUi();")) -Message "Phase11 sample should derive local-tab suppression from UseHubUi."

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
