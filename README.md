# UE-MCP Studio editor plugin

The Unreal Editor plugin for [UE-MCP Studio](https://github.com/db-lyon/ue-mcp-studio): the toolbar button and Tools menu entry that open Studio, the UE-MCP Studio tab that holds docked Studio and the setup page, the native calls the setup page uses, and the launch of the project's [ue-mcp](https://github.com/db-lyon/ue-mcp) daemon. On first start it installs ue-mcp and the Studio extension for the project. Free and open source under the MIT license.

## Install

Unreal Engine 5.7 and 5.8 on Windows, with [Node.js](https://nodejs.org) installed.

1. Download `UE_MCP_Studio-<version>-UE<engine>-Win64.zip` for your engine from the [latest release](https://github.com/db-lyon/ue-mcp-studio-plugin/releases/latest).
2. Unzip it into your project's `Plugins/` folder, so that `Plugins/UE_MCP_Studio/UE_MCP_Studio.uplugin` exists.
3. Open the project and open Studio from the toolbar button or **Tools > UE-MCP Studio**.

Studio runs in its own window, a separate process, so it keeps working while the editor is busy. **Dock in editor** (in Studio's header, or the UE-MCP Studio tab before Studio runs) shows it in the editor's **UE-MCP Studio** tab instead, beside Details by default: the window stays its own process and is held over the tab, following it as it moves, resizes, floats or goes behind another tab. **Open as window** brings it back out. The choice is kept per project, and when the tab is part of your saved layout Studio appears in it on editor start. Docking needs Studio 0.1.0-beta.31 or later.

Until Studio can run, the UE-MCP Studio tab shows the setup page, then the state of the editor, the daemon and updates.

Studio's **Settings > Update** keeps the plugin, the Studio extension and ue-mcp current with one button.

## Layout

- `StudioHost/Plugins/UE_MCP_Studio/` - the plugin.
- `StudioHost/` - the project used to build it.
- `scripts/release.mjs` - builds the plugin for each engine and publishes a release with every engine's zip and `plugin.json`, the manifest Studio's updater reads.
- `scripts/setup-preview.mjs` - serves the setup page at http://localhost:5180 with a fake editor binding and a scenario and theme picker, so every step can be seen without an editor. `?page=tools` (or project, native, context, clients, agent, review) opens a setup page directly. `--project <.uproject>` uses that project's real `ue-mcp init --dry-run` plan.
- `scripts/setup-page.test.mjs` - walks the setup page in jsdom with the same fake binding: `node --test scripts/setup-page.test.mjs`, with jsdom from `npm i --no-save jsdom` or `NODE_PATH`.

## Releasing

Bump `VersionName` (and `Version`) in `UE_MCP_Studio.uplugin`, commit, then:

```
node scripts/release.mjs --publish --notes-file notes.md
```

Every release builds UE 5.7 and 5.8 and refuses to publish without both zips.
