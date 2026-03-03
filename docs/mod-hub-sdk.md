# Mod Hub SDK (v1)

This is the consumer-facing integration reference for the `mod_hub_client` helper.

## Phase 8 Single-Table Registration (SSOT)

The canonical table schema is defined in `include/emc/mod_hub_client.h`:

- `emc::ModHubClientSettingKind`
- `emc::ModHubClientSettingRowV1`
- `emc::ModHubClientTableRegistrationV1`
- `emc::RegisterSettingsTableV1(...)`

### Row Kinds

- `MOD_HUB_CLIENT_SETTING_KIND_BOOL`
- `MOD_HUB_CLIENT_SETTING_KIND_KEYBIND`
- `MOD_HUB_CLIENT_SETTING_KIND_INT`
- `MOD_HUB_CLIENT_SETTING_KIND_FLOAT`
- `MOD_HUB_CLIENT_SETTING_KIND_ACTION`

### Deterministic Behavior

`RegisterSettingsTableV1` performs:

1. `register_mod` first.
2. Row registration in table order.
3. Fail-fast on first non-`EMC_OK` result.

Reference implementation: `src/mod_hub_client.cpp`.

## Helper Integration

Use `ModHubClient::Config.table_registration` for table-only registration.

```cpp
emc::ModHubClient::Config config;
config.table_registration = ModHubConsumerAdapter_GetTableRegistration();
config.should_force_attach_failure_fn = &ShouldForceAttachFailureForClient;
client.SetConfig(config);
```

`ModHubClient` entrypoints remain:

- `OnStartup()`
- `OnOptionsWindowInit()`
- `UseHubUi()`

## Phase 9 Scaffold Command

Run either scaffold entrypoint:

```bash
./scripts/init-mod-template.sh --with-hub
```

```powershell
./scripts/init-mod-template.ps1 -WithHub
```

Generated adapter files:

- `src/mod_hub_consumer_adapter.h`
- `src/mod_hub_consumer_adapter.cpp`

The scaffold includes:

1. A static settings-table schema for all v1 row kinds.
2. `ModHubClient` wiring for `OnStartup` and `OnOptionsWindowInit`.
3. `UseHubUi`/fallback helpers for duplicate-safe local-tab suppression.

Template sources:

- `scripts/templates/mod_hub_consumer_adapter.h.template`
- `scripts/templates/mod_hub_consumer_adapter.cpp.template`

## Validation Script

Phase 8 harness:

```powershell
./scripts/phase8_mod_hub_client_table_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll>
```

The harness validates all v1 row kinds, order, descriptor correctness, and failure propagation.

Phase 9 scaffold harness:

```powershell
./scripts/phase9_init_mod_template_scaffold_test.ps1
```

The harness validates scaffold generation, helper/fallback wiring presence, and performs a compile smoke check when `cl.exe` is available.

## Phase 10 Versioned SDK Package

Create the standalone SDK release asset:

```powershell
./scripts/package-sdk.ps1
```

Or produce mod + SDK assets in one pass:

```powershell
./scripts/build-and-package.ps1
```

Default SDK asset output:

- `dist/Emkejs-Mod-Core-SDK-<VERSION>.zip`

SDK bundle contents:

- `include/emc/mod_hub_api.h`
- `include/emc/mod_hub_client.h`
- `src/mod_hub_client.cpp`
- `samples/minimal/mod_hub_consumer_adapter.h`
- `samples/minimal/mod_hub_consumer_adapter.cpp`
- `docs/mod-hub-sdk.md`
- `sdk-metadata.json`

Compatibility metadata (`sdk-metadata.json`) fields:

- `sdk_package_version` (matches package/version file)
- `supported_hub_api_versions` (explicit supported API version list)
- `default_hub_api_version` (recommended request version)

Phase 10 harness:

```powershell
./scripts/phase10_sdk_packaging_test.ps1
```
