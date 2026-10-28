# Fab listing: UE-MCP Studio

## Title

UE-MCP Studio

## Short description

Editor plugin that installs and opens UE-MCP Studio, an agent that works in Unreal Editor through ue-mcp, in its own window or docked in an editor tab.

## Long description

UE-MCP Studio is a client for working with AI models inside Unreal Editor. You chat with it in its own window beside the editor or docked in an editor tab. It reads and changes your project using UE-MCP's editor actions, the same ones any MCP client gets: Blueprints, materials, levels, Niagara, PCG, animation, GAS, UMG, Sequencer, builds and more.

**Ask from the editor**

Press `Ctrl+Alt+Space` anywhere in the editor, or `Ctrl+Alt+right-click`, to open a prompt at your cursor that starts a new session. Over the level viewport it brings the point under the cursor and the actor there, and the right-click also brings your selected actors. Over a Blueprint, material, Niagara or other graph it brings the graph, the node under the cursor and the selected nodes. **Chat about this** in the context menu of selected actors or assets does the same. The reply appears in the prompt, and **Continue this thread** opens the session in Studio. The shortcut can be changed in Editor Preferences.

**Keeps working while the editor is busy**

Studio runs in its own process. When an editor call blocks the editor, the chat shows "Editor busy" with the call, a timer and a Stop button instead of locking up. A restart or rebuild shows each step with a timer.

**Survives crashes and restarts**

When the editor closes or crashes, a docked Studio becomes its own window, keeps the conversation, and shows "Editor not running" with a Start button. When the editor starts again, Studio docks back into its UE-MCP Studio tab.

**Use the models you already have**

- Your own API keys for Anthropic, OpenAI and Google.
- Your own OpenAI-compatible endpoints, such as a model server on your machine: a base URL, an optional key and the models it lists.
- Sign in with ChatGPT to run OpenAI models on your ChatGPT plan.
- Claude models through your own Claude Code login. You sign in with `claude /login` in a terminal, and Studio picks up that login. Studio has no Claude sign-in of its own and never reads or stores your Claude credentials.

Model use is billed or limited by each provider under your own account.

**You approve what the agent changes**

**Settings > Approvals & review** sets each kind. Shell commands and file changes are Ask, In project or Anywhere. Restarting or rebuilding the editor, and ue-mcp actions that delete or overwrite, are Ask or Allow. Every kind starts at Ask. A waiting call shows the exact command, diff or action in the chat until you choose Allow, Allow for this session or Deny. Reads and searches run without asking.

**Review each turn**

After a turn that changed files, a review card lists the changes by type, each with its diff and Keep or Revert, and Revert all. Assets show Unreal's own diff with before and after values and a picture of each version, and Blueprints also show a Blueprint Diff of their variables, graphs and components. **Open in Unreal diff** opens the engine's diff. Each session keeps a timeline of checkpoints with Restore to here and Undo.

**Playtests**

`/playtest` drafts a playtest of what the conversation is about: a Play In Editor test script whose steps are checked automatically or by you. Edit changes the steps, and Start playtest runs it. **Assign to subagent** hands a playtest to a subagent that changes the project and runs it again until every step passes or it needs you, and each attempt can be reverted. The Playtests page lists every playtest in the project with its latest run.

**Mentions and commands**

Type `@` to mention an asset, a level actor or a project file. `/action` runs one ue-mcp action and `/flow` runs one of the project's flows, each with a form for its inputs.

**Worker**

Studio's built-in subagent. **Settings > Agents** sets its model and effort, and whether it starts when you ask or when work splits into independent parts. It runs on the main agent's sign-in or key.

**Setup**

The first time you open Studio, the plugin lists every part it will download and where it goes, and installs nothing until you choose Allow. A setup page then checks for Node.js, installs the UE-MCP bridge plugin for the project with a page for each group of options and a review, and starts the project's ue-mcp daemon.

**Updates**

**Settings > General > Updates** lists Studio, the editor plugin, the bridge plugin and ue-mcp with the installed and latest versions. An update waits until no turn is running, and Restart now applies it at once. The Beta channel also takes prereleases. The plugin installed from Fab updates through your Fab Library.

**Feedback**

The Feedback button files a problem or an idea as a GitHub issue on Studio's tracker, with no GitHub account needed. Agents can also report gaps they find in ue-mcp. **Settings > Approvals & review** sets whether you approve each report, reports file automatically, or they are held for you.

**Requirements**

- Windows 10 or 11 (Win64) and Unreal Engine 5.7 or 5.8.
- Node.js 20 or newer, installed by you from nodejs.org.
- The Microsoft Edge WebView2 Runtime for Studio's window, included in Windows 11. Without it, Studio opens in your web browser.
- An internet connection, and an account or API key with at least one model provider.

**What installs at first start, after you allow it**

- The UE-MCP bridge plugin, as prebuilt binaries, into your project's `Plugins/UE_MCP_Bridge` folder, from the ue-mcp GitHub releases.
- The ue-mcp server from npm, run with npx.
- The Studio extension (the agent, its UI and its window app) into `%USERPROFILE%\.ue-mcp`, from the UE-MCP Studio GitHub releases.
- Only for Claude models on your Claude Code login: Anthropic's Claude runtime, unmodified, from npm.

Studio is free, and its source is MIT-licensed on GitHub. The copy on Fab is under the Fab EULA (Standard License). Docs: https://ue-mcp.com/docs/studio

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
    - `08-status-after-setup.png`: the editor after setup, with Studio docked in the UE-MCP Studio tab on Settings > General: the Appearance themes and the Updates card reading **Up to date** on the Beta channel.
    - `09-settings-sign-in.png`: Studio in its own window on Settings > Models & accounts: the Claude sign-in (from Claude Code) and ChatGPT sign-in, the API keys, custom endpoints, and the route order with the route each model runs on.

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
