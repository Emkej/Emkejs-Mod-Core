## Emkejs Mod Core (RE_Kenshi plugin base)

This repository is a clean starter base for a `Emkejs-Mod-Core` RE_Kenshi native plugin.

## Setup
Clone with `--recurse-submodules` or run `git submodule update --init --recursive`.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Build
You can build in Visual Studio, or via the script below.

### Scripted build + deploy
Run:
- `.\scripts\build-deploy.ps1`

Optional parameters:
- `-KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"`
- `-Configuration "Release"`
- `-Platform "x64"`

## Deploy layout
Mod data folder name: `Emkejs-Mod-Core`

After deploy, expected files:
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\Emkejs-Mod-Core.mod`
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\Emkejs-Mod-Core.dll`
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\mod-config.json`

## Config
`mod-config.json` is currently a minimal placeholder for future plugin settings.

## Load Order Recommendation (Mod Hub Consumers)
- If other mods use this Mod Hub integration, place `Emkejs-Mod-Core` before those consumer mods in load order.
- Consumer mods have attach + one retry fallback logic, but loading this mod first gives the most reliable hub UI startup path.

## Hub Scaffold (Phase 9)
- Generate the standard Mod Hub adapter scaffold:
  - `./scripts/init-mod-template.sh --with-hub`
  - `./scripts/init-mod-template.ps1 -WithHub`
- Quick-start integration guide:
  - [`docs/mod-hub-sdk-quickstart.md`](docs/mod-hub-sdk-quickstart.md)
- Optional overrides:
  - `-HubNamespaceId`, `-HubNamespaceDisplayName`, `-HubModId`, `-HubModDisplayName`
- Output files:
  - `src/mod_hub_consumer_adapter.h`
  - `src/mod_hub_consumer_adapter.cpp`

## SDK Packaging (Phase 10)
- Build versioned SDK asset only:
  - `./scripts/package-sdk.sh`
  - `./scripts/package-sdk.ps1`
- Build mod zip + SDK zip together:
  - `./scripts/build-and-package.sh`
  - `./scripts/build-and-package.ps1`
- Default SDK output:
  - `dist/Emkejs-Mod-Core-SDK-<VERSION>.zip`
- Phase 10 validation harness:
  - `./scripts/phase10_sdk_packaging_test.ps1`
