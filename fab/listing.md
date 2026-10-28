# Fab listing: UE-MCP Studio

## Title

UE-MCP Studio

## Short description

Editor plugin that installs and opens UE-MCP Studio, an agent that works in Unreal Editor through ue-mcp, in its own window or docked in an editor tab.

## Long description

UE-MCP Studio is an agent that works inside Unreal Editor through ue-mcp's editor actions. This plugin is the editor side of Studio. It sets Studio up for your project, starts it, and shows it.

What the plugin does:

- Adds a **Studio** button to the level editor toolbar, **Tools > UE-MCP Studio** and the console command `Studio.Open`.
- Adds the **UE-MCP Studio** tab. Until Studio can run, the tab shows a setup page that checks for Node.js, installs the UE-MCP bridge plugin for the project and starts the project's ue-mcp daemon. After that, it shows the status of the daemon, the editor connection, the window and updates.
- Starts the project's ue-mcp daemon with the editor. The daemon is a separate process and keeps running when the editor closes.
- Opens Studio in its own window, a separate process, so the chat keeps working while the editor is busy. **Dock in editor** shows the same window inside the UE-MCP Studio tab, where it moves, resizes and hides with the tab. **Open as window** brings it back out. The choice is kept per project.
- Adds **Ask Studio**: Ctrl+Alt+Space or Ctrl+Alt+right-click opens a one-line prompt at the cursor, with what is under the cursor (a point in the level, a graph and its nodes) as context. **Chat about this** on selected actors and assets does the same with them as context.
- Adds the **UE-MCP Docs** tab, which shows the ue-mcp documentation inside the editor.

What installs at first start, and only after you allow it:

The first time you open Studio, the plugin asks before it downloads or installs anything. It lists each part and where it goes. Nothing is downloaded until you choose Allow, and you are asked once per user.

- The UE-MCP bridge plugin, as prebuilt binaries, into your project's `Plugins/UE_MCP_Bridge` folder, from the ue-mcp GitHub releases.
- The ue-mcp server from npm, run with npx and kept in npm's cache.
- The Studio extension (the agent, its UI and Studio's window app) into `%USERPROFILE%\.ue-mcp`, from the UE-MCP Studio GitHub releases.

Models: Studio runs models from Anthropic, OpenAI and Google with your own API key, or with a ChatGPT sign-in for OpenAI models. Claude models can also run on your Claude plan through your Claude Code login: you run `claude /login` in a terminal and Studio reads that login. Studio has no Claude sign-in of its own and never sees your Claude credentials. You need an account or API key with at least one of these providers. Model use is billed or limited by that provider.

Approvals: **Settings > Approvals & review** sets, per kind, whether the agent asks first: shell commands, file changes, restarting or rebuilding the editor, and ue-mcp actions that delete or overwrite. Each kind is Ask or Allow, and shell commands and file changes can also be allowed only inside the project folder. A new install asks for all four. The settings are kept per project. An approval waits in the chat for Allow, Allow for this session or Deny.

Prerequisites:

- Windows 10 or 11.
- Unreal Engine 5.7 or 5.8.
- Node.js 20 or newer (LTS), installed by you from nodejs.org. The plugin does not install it.
- Microsoft Edge WebView2 Runtime for Studio's window. Windows 11 includes it. Without it, Studio opens in your web browser.
- An internet connection for the first-start install, updates and model calls.
- Visual Studio with the Desktop development with C++ workload is needed only to build the bridge from source. Studio's own setup installs prebuilt binaries and does not need it.

License: this Fab distribution is under the Fab EULA (Standard License). The plugin's source, the Studio extension and ue-mcp are also published under the MIT license on GitHub.

Documentation: https://ue-mcp.com/docs/studio

## Technical details

**Features:**

- Toolbar button, Tools menu entry and `Studio.Open` console command that open Studio.
- UE-MCP Studio dock tab with the setup page and a status view.
- One-time install consent prompt that lists every part and its install path before anything is downloaded.
- Setup page: Node.js check with a link to the download page, bridge install with ue-mcp init's choices on step-by-step pages and a review, Studio download and daemon start, with progress written to `Saved/Logs/UE_MCP_Studio.log`.
- Loads a newly installed bridge without an editor restart when every engine plugin it needs is already loaded.
- Starts the project's ue-mcp daemon with the editor, outside the editor's process.
- Studio in its own window or docked in the UE-MCP Studio tab, kept per project.
- Ask Studio prompt at the cursor (rebindable in Editor Preferences > Keyboard Shortcuts) and Chat about this on actors and assets.
- UE-MCP Docs tab.
- Checks for a newer Studio extension at editor start and hands it to Studio's updater.

**Code modules:**

- `UE_MCP_Studio` (Editor, loading phase PostEngineInit)

**Number of Blueprints:** 0

**Number of C++ classes:** 14

**Network replicated:** No

**Supported development platforms:** Windows 10 and 11 (Win64)

**Supported target build platforms:** None. Editor-only plugin; it adds nothing to packaged games.

**Supported engine versions:** 5.7, 5.8

**Documentation:** https://ue-mcp.com/docs/studio

**Example project:** None

**Important notes:**

- Requires Node.js 20 or newer, installed by the user from https://nodejs.org. Not included.
- Requires the Microsoft Edge WebView2 Runtime for Studio's own window (included in Windows 11). Without it, Studio opens in the web browser.
- At first start, after the user's explicit consent, the plugin downloads and installs the UE-MCP bridge plugin (MIT) into the project, the ue-mcp npm package (MIT) through npx, and the UE-MCP Studio extension (MIT). See the third-party software declaration.
- Requires an internet connection for the first-start install, updates and model calls.
- Model use requires the user's own account or API key with Anthropic, OpenAI or Google. Claude models can run on the user's Claude plan through their existing Claude Code login (`claude /login`), which Studio reads. Studio has no Claude sign-in of its own.
- The agent can run shell commands and change files in the project folder. By default shell commands, file changes, editor restarts and ue-mcp actions that delete or overwrite wait for the user's approval. **Settings > Approvals & review** sets each kind.
- Visual Studio with the Desktop development with C++ workload is needed only to build the bridge from source; Studio's setup uses prebuilt binaries.
- Editor only. Not included in packaged builds.

## Tags

- UE-MCP
- MCP
- Model Context Protocol
- AI
- AI agent
- Editor tools
- Editor plugin
- Automation
- Chat
- Productivity
- Workflow
- Tools

### Question for you: CreatedWithAI

Fab's `CreatedWithAI` tag applies when a material portion of the product was generated with generative AI. Decide whether it applies to this plugin's code and text, and add the tag if it does.

## Gallery shot list

Specs: each image at least 1920x1080, JPEG or PNG, under 3 MB each, under 25 MB in total. Capture at 1920x1080 or larger from the editor on a clean project (StudioE2E), with no personal paths or account names visible.

1. **Hero.** The editor with Studio docked in the UE-MCP Studio tab beside Details, mid-conversation, the level viewport showing the change the agent just made.
2. **Own window.** Studio in its own window beside the editor, showing a session with an editor action result.
3. **Toolbar and menu.** The **Studio** toolbar button and **Tools > UE-MCP Studio** menu entry, highlighted.
4. **Consent prompt.** The UE-MCP Studio tab showing **Allow Studio to install its parts?** with the list of parts and paths.
5. **Setup page.** The setup page on a Bridge page such as Tools, with the page segments under the Bridge step.
6. **Status view.** The UE-MCP Studio tab's status view: daemon version and port, editor connected, window docked, updates current.
7. **Approval.** An approval card in the chat for a file change, with the diff and **Allow**, **Allow for this session** and **Deny**.
8. **Model picker.** The model picker open in the chat box, showing the vendors, the ChatGPT sign-in and the Claude row that runs on the Claude Code login.
9. **Ask Studio.** The Ask Studio prompt at the cursor over the level viewport, with its context chip.
10. **Chat about this.** The context menu on a selected actor in the Outliner with **Chat about this**.
11. **Settings.** Settings > General with the Updates card (Studio, Editor plugin, ue-mcp rows) and Settings > Approvals & review with the **Ask first** rows, one per kind.

## Video shot list (60 to 90 seconds)

Specs: 1920x1080, MP4, MOV or WEBM, 300 MB or less. Screen capture of the editor and Studio. On-screen captions are plain statements of what is shown.

| Time | Shot | Caption |
|---|---|---|
| 0:00 to 0:06 | Editor open on a clean project; cursor clicks the **Studio** toolbar button. | Open Studio from the toolbar. |
| 0:06 to 0:16 | The consent prompt; scroll the list of parts and paths; click **Allow**. | Nothing installs until you allow it. |
| 0:16 to 0:28 | Setup page: Bridge step, click **Install**, progress, then Studio downloads and the daemon starts. Cut the waiting time. | The bridge and Studio install for this project. |
| 0:28 to 0:36 | Studio's window opens. Pick a model in the chat box. | Pick a model, then use a sign-in or your API key. |
| 0:36 to 0:50 | Type a request that places and changes actors in the level; the agent's editor actions appear; the viewport updates. | The agent works in the editor through ue-mcp. |
| 0:50 to 1:00 | The agent proposes a file change; the approval card with its diff; click **Allow**. | Commands and file changes wait for your approval. |
| 1:00 to 1:10 | Click **Dock in editor**; Studio moves into the UE-MCP Studio tab; resize the tab and it follows. | Use Studio in its own window or docked in a tab. |
| 1:10 to 1:22 | Ctrl+Alt+right-click on a point in the level; the Ask Studio prompt opens with the context chip; send a short request; the reply appears. | Ask Studio about what is under the cursor. |
| 1:22 to 1:30 | Settings > General, Updates card showing everything up to date. End on the docs URL. | Documentation: ue-mcp.com/docs/studio |

## Submission notes

- Fab delivers one project file per engine version through a Google Drive, Dropbox or OneDrive share link. Build them with `node scripts/release.mjs --fab --fab-url <listing URL>`, which writes `UE_MCP_Studio-<version>-UE5.7-Fab.zip` and `UE_MCP_Studio-<version>-UE5.8-Fab.zip`.
- Fab installs the plugin into the engine under `Engine\Plugins\Fab`.
- Third-party software declaration and Fab form answers: `fab/third-party.md`.
