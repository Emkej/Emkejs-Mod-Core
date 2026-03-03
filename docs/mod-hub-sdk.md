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
config.table_registration = GetModHubTableRegistration();
config.should_force_attach_failure_fn = &ShouldForceAttachFailureForClient;
client.SetConfig(config);
```

`ModHubClient` entrypoints remain:

- `OnStartup()`
- `OnOptionsWindowInit()`
- `UseHubUi()`

## Scaffold Reuse

Run the local scaffold command with `-WithHub`:

```powershell
./scripts/init-mod-template.ps1 -WithHub
```

This creates a reusable table-based skeleton at:

- `src/mod_hub_consumer_adapter.cpp`

Template source (single schema example):

- `scripts/templates/mod_hub_consumer_adapter.cpp.template`

## Validation Script

Phase 8 harness:

```powershell
./scripts/phase8_mod_hub_client_table_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll>
```

The harness validates all v1 row kinds, order, descriptor correctness, and failure propagation.
