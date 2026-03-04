# Mod Hub SDK Quick Start (Mod Authors)

Fast path for integrating a mod with `Emkejs-Mod-Core` Mod Hub using the public SDK/helper.

## 1) Scaffold the adapter

From your mod repo root (where `scripts/init-mod-template.*` is available):

```bash
./scripts/init-mod-template.sh --with-hub
```

```powershell
./scripts/init-mod-template.ps1 -WithHub
```

This generates:

- `src/mod_hub_consumer_adapter.h`
- `src/mod_hub_consumer_adapter.cpp`

## 2) Set your namespace and mod IDs

Use stable identity values for your mod:

- `namespace_id`: shared bucket for related mods (for example `myteam.qol`)
- `mod_id`: unique within the namespace (for example `faster_looting`)

You can set these during scaffold:

```powershell
./scripts/init-mod-template.ps1 -WithHub `
  -HubNamespaceId "myteam.qol" `
  -HubNamespaceDisplayName "My Team QoL" `
  -HubModId "faster_looting" `
  -HubModDisplayName "Faster Looting"
```

```bash
./scripts/init-mod-template.sh --with-hub \
  --hub-namespace-id "myteam.qol" \
  --hub-namespace-display-name "My Team QoL" \
  --hub-mod-id "faster_looting" \
  --hub-mod-display-name "Faster Looting"
```

Or edit the generated `EMC_ModDescriptorV1` in `src/mod_hub_consumer_adapter.cpp`.

## 3) Register settings rows in one table

In the generated adapter, keep one static table using:

- `emc::ModHubClientSettingRowV1`
- `emc::ModHubClientTableRegistrationV1`

Supported row kinds:

- `MOD_HUB_CLIENT_SETTING_KIND_BOOL`
- `MOD_HUB_CLIENT_SETTING_KIND_KEYBIND`
- `MOD_HUB_CLIENT_SETTING_KIND_INT`
- `MOD_HUB_CLIENT_SETTING_KIND_FLOAT`
- `MOD_HUB_CLIENT_SETTING_KIND_ACTION`

Use your existing get/set callbacks in row definitions; the helper performs deterministic registration and commit ordering.

## 4) Wire lifecycle calls

Call into the adapter from your plugin lifecycle:

- Startup: `ModHubConsumerAdapter_OnStartup()`
- Options window init (legacy hub compatibility only): `ModHubConsumerAdapter_OnOptionsWindowInit()`
- Local UI fallback gate: `ModHubConsumerAdapter_ShouldCreateLocalTab()` (or `!ModHubConsumerAdapter_UseHubUi()`)

Current hub builds register the retry observer from `ModHubConsumerAdapter_OnStartup()`, so no per-mod options-init RVA hook is required unless you need compatibility with older hubs.

If hub attach/registration fails, helper fallback keeps your local UI path active.

## 5) Build and verify

Recommended checks:

```powershell
./scripts/phase9_init_mod_template_scaffold_test.ps1
./scripts/phase11_sdk_docs_test.ps1
```

If those phase scripts are not part of your mod repo, validate by building your plugin and confirming options UI behavior in game (hub path when attached, local fallback when hub is unavailable).

For full API reference and complete sample code, see [docs/mod-hub-sdk.md](mod-hub-sdk.md).
