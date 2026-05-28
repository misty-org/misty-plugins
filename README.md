# Misty Plugins

The official plugin workspace for Misty — including plugin headers, example plugins, and the native preview sandbox used during plugin and panel development.

Misty is closed source. This repository hosts the public plugin-facing headers plus the first-party plugin workspace.

---

## What's Here

| Path | Contents |
|------|----------|
| `include/misty_plugin_api.h` | Raw C ABI between Misty and plugins |
| `include/misty_plugin.h` | C++ convenience wrapper for plugin authors |
| `plugins/preview_manager/` | Sample file preview plugin |
| `plugins/preview_panel/` | Native panel/modal preview plugin |
| `vendor/` | Vendored headers used by plugin examples |

## Build Ownership

This repository owns:

- first-party plugin source
- plugin manifests
- the `misty-plugin-sandbox` preview tool

The Misty host app consumes this workspace by pointing `MISTY_EXTERNAL_PLUGIN_DIR` at it.

## Current Plugin Interface

| Thing | Current value |
|------|--------------|
| Misty version | 1.0 |
| Plugin ABI version (`MISTY_PLUGIN_ABI_VERSION`) | 4 |
| Manifest `schema_version` | 2 |
| Plugin interface version | 1.0 |

## Surface Types

Plugins currently register panels through `MistyPanelReg`. The SDK now also carries
window intent metadata so a panel can request either:

- `MISTY_WINDOW_TYPE_PANEL` for the existing in-app floating panel behavior
- `MISTY_WINDOW_TYPE_EXTERNAL` for a host-managed detached OS window once host support lands

Existing plugins remain valid because the default zero-initialized value is still
`MISTY_WINDOW_TYPE_PANEL`.

## Native Preview Workflow

Typical local loop from the Misty host repo:

```bash
cmake --build client/build/debug --target preview_panel preview_panel_manifest misty_plugin_sandbox
client/build/debug/bin/misty-plugin-sandbox --plugin-dir client/build/debug/bin/plugins/preview_panel
```

`preview_panel` selects host-native scenes, and the sandbox renders the real Misty modal/panel primitives so you can iterate without triggering real app state.
