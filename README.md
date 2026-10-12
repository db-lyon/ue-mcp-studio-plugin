# UE-MCP Studio editor plugin

The Unreal Editor plugin for [UE-MCP Studio](https://github.com/db-lyon/ue-mcp-studio): a dock tab that hosts Studio inside the editor, the native calls the page uses, and the launch of the project's [ue-mcp](https://github.com/db-lyon/ue-mcp) daemon. On first start it installs ue-mcp and the Studio extension for the project. Free and open source under the MIT license.

## Install

Unreal Engine 5.7 and 5.8 on Windows, with [Node.js](https://nodejs.org) installed.

1. Download `UE_MCP_Studio-<version>-UE<engine>-Win64.zip` for your engine from the [latest release](https://github.com/db-lyon/ue-mcp-studio-plugin/releases/latest).
2. Unzip it into your project's `Plugins/` folder, so that `Plugins/UE_MCP_Studio/UE_MCP_Studio.uplugin` exists.
3. Open the project and open Studio from the toolbar button or **Tools > UE-MCP Studio**.

Studio's **Settings > Update** keeps the plugin, the Studio extension and ue-mcp current with one button.

## Layout

- `StudioHost/Plugins/UE_MCP_Studio/` - the plugin.
- `StudioHost/` - the project used to build it.
- `scripts/release.mjs` - builds the plugin for each engine and publishes a release with every engine's zip and `plugin.json`, the manifest Studio's updater reads.

## Releasing

Bump `VersionName` (and `Version`) in `UE_MCP_Studio.uplugin`, commit, then:

```
node scripts/release.mjs --publish --notes-file notes.md
```

Every release builds UE 5.7 and 5.8 and refuses to publish without both zips.
