# Third-party software declaration: UE-MCP Studio

This declaration covers the UE-MCP Studio editor plugin (`UE_MCP_Studio`, version 0.1.3) as distributed on Fab, and every piece of third-party software it ships, requires or installs.

The Fab package contains only the plugin's C++ source, its `.uplugin` and `Resources/web` (the bundled setup page and three fonts). Everything in Sections 3 to 6 is downloaded at first start, and only after the user chooses **Allow** in the plugin's install consent prompt. The plugin never installs Node.js, the WebView2 Runtime or Visual Studio.

No component listed here is under GPL, AGPL, EPL or LGPL. One component is not open source: the Claude Agent SDK and its runtime (Section 6), used only by an optional sign-in.

## 1. Shipped in the Fab package

| Component | Version | License | Use | Source |
|---|---|---|---|---|
| Plus Jakarta Sans (`Resources/web/fonts/PlusJakartaSans.woff2`) | Variable font | SIL Open Font License 1.1 | Font loaded by the bundled setup page (`bootstrap.html`) | https://github.com/tokotype/PlusJakartaSans |
| JetBrains Mono (`Resources/web/fonts/JetBrainsMono.woff2`) | Variable font | SIL Open Font License 1.1 | Font loaded by the setup page | https://github.com/JetBrains/JetBrainsMono |
| Instrument Serif Italic (`Resources/web/fonts/InstrumentSerif-Italic.woff2`) | Single style | SIL Open Font License 1.1 | Font loaded by the setup page | https://github.com/Instrument/instrument-serif |

Each font's license text ships beside it (`*-OFL.txt`). The fonts are unmodified and are not sold on their own.

The plugin's C++ code links only Unreal Engine modules (Core, CoreUObject, Engine, Slate, SlateCore, UnrealEd, LevelEditor, ToolMenus, WorkspaceMenuStructure, WebBrowser, Projects, Json, HTTP, InputCore, ApplicationCore, ContentBrowser, GraphEditor) and the Windows API. It contains no third-party code libraries. The plugin's source is published under the MIT license at https://github.com/db-lyon/ue-mcp-studio-plugin (Copyright (c) 2026 David Bingham). The copy distributed on Fab is licensed under the Fab EULA.

## 2. Prerequisites the user installs

| Component | Version | License | Use | Source |
|---|---|---|---|---|
| Node.js (includes npm and npx) | 20 or newer | MIT (Node.js); the npm CLI is Artistic License 2.0 | Runs the ue-mcp server and the Studio extension. The plugin starts `npx` from the user's Node.js install | https://nodejs.org |
| Microsoft Edge WebView2 Runtime | Evergreen | Microsoft software license terms | Renders Studio's window. Part of Windows 11. Without it Studio opens in the system browser | https://developer.microsoft.com/en-us/microsoft-edge/webview2/ |
| Visual Studio, Desktop development with C++ workload (optional) | 2022 or the version the engine requires | Microsoft software license terms | Needed only to build the bridge from source. Studio's own setup installs the bridge as prebuilt binaries and does not use it | https://visualstudio.microsoft.com/downloads/ |

None of these is shipped or installed by the plugin. When Node.js is missing, the setup page opens the Node.js download page in the system browser.

## 3. Installed at first start: ue-mcp npm package

| Component | Version | License | Use | Source |
|---|---|---|---|---|
| ue-mcp | 1.4.3 at the time of writing. The plugin runs the version matching the installed bridge, else the one Studio's release manifest names | MIT, Copyright (c) 2026 David Bingham | MCP server and daemon for the project, a separate Node.js process | https://www.npmjs.com/package/ue-mcp, https://github.com/db-lyon/ue-mcp |

Downloaded by `npx` from registry.npmjs.org into npm's cache (`%LOCALAPPDATA%\npm-cache\_npx`). Its runtime dependency closure, from `npm install ue-mcp@1.4.3 --omit=dev` and each package's `license` field (95 packages):

| Package | Version | License |
|---|---|---|
| @db-lyon/flowkit | 0.17.3 | MIT |
| @hono/node-server | 2.1.5 | MIT |
| @modelcontextprotocol/sdk | 1.32.1 | MIT |
| accepts | 2.0.0 | MIT |
| ajv | 8.20.0 | MIT |
| ajv-formats | 3.0.1 | MIT |
| argparse | 2.0.1 | Python-2.0 |
| body-parser | 2.3.0 | MIT |
| bytes | 3.1.2 | MIT |
| call-bind-apply-helpers | 1.0.2 | MIT |
| call-bound | 1.0.4 | MIT |
| content-disposition | 2.0.1 | MIT |
| content-type | 2.1.0 | MIT |
| content-type | 1.0.5 | MIT |
| cookie | 0.7.2 | MIT |
| cookie-signature | 1.2.2 | MIT |
| cors | 2.8.6 | MIT |
| cross-spawn | 7.0.6 | MIT |
| debug | 4.4.3 | MIT |
| depd | 2.0.0 | MIT |
| dunder-proto | 1.0.1 | MIT |
| ee-first | 1.1.1 | MIT |
| encodeurl | 2.0.0 | MIT |
| es-define-property | 1.0.1 | MIT |
| es-errors | 1.3.0 | MIT |
| es-object-atoms | 1.1.2 | MIT |
| escape-html | 1.0.3 | MIT |
| etag | 1.8.1 | MIT |
| eventsource | 3.0.7 | MIT |
| eventsource-parser | 3.1.1 | MIT |
| express | 5.3.0 | MIT |
| express-rate-limit | 8.7.1 | MIT |
| fast-deep-equal | 3.1.3 | MIT |
| fast-uri | 3.1.8 | BSD-3-Clause |
| finalhandler | 2.1.1 | MIT |
| forwarded | 0.2.0 | MIT |
| fresh | 2.0.0 | MIT |
| function-bind | 1.1.2 | MIT |
| get-intrinsic | 1.3.0 | MIT |
| get-proto | 1.0.1 | MIT |
| gopd | 1.2.0 | MIT |
| has-symbols | 1.1.0 | MIT |
| hasown | 2.0.4 | MIT |
| hono | 4.13.13 | MIT |
| http-errors | 2.0.1 | MIT |
| iconv-lite | 0.7.3 | MIT |
| inherits | 2.0.4 | ISC |
| ip-address | 10.7.4 | MIT |
| ipaddr.js | 1.9.1 | MIT |
| is-promise | 4.0.0 | MIT |
| isexe | 2.0.0 | ISC |
| jose | 6.2.12 | MIT |
| js-yaml | 4.3.2 | MIT |
| json-schema-traverse | 1.0.0 | MIT |
| json-schema-typed | 8.0.2 | BSD-2-Clause |
| math-intrinsics | 1.1.0 | MIT |
| media-typer | 1.1.1 | MIT |
| merge-descriptors | 2.0.0 | MIT |
| mime-db | 1.54.0 | MIT |
| mime-types | 3.0.2 | MIT |
| ms | 2.1.3 | MIT |
| negotiator | 1.1.0 | MIT |
| object-assign | 4.1.1 | MIT |
| object-inspect | 1.13.4 | MIT |
| on-finished | 2.4.1 | MIT |
| once | 1.4.0 | ISC |
| parseurl | 1.3.3 | MIT |
| path-key | 3.1.1 | MIT |
| path-to-regexp | 8.4.2 | MIT |
| pkce-challenge | 5.0.1 | MIT |
| proxy-addr | 2.0.8 | MIT |
| qs | 6.16.0 | BSD-3-Clause |
| range-parser | 1.3.0 | MIT |
| raw-body | 3.0.2 | MIT |
| require-from-string | 2.0.2 | MIT |
| router | 2.2.0 | MIT |
| safer-buffer | 2.1.2 | MIT |
| send | 1.2.1 | MIT |
| serve-static | 2.2.1 | MIT |
| setprototypeof | 1.2.0 | ISC |
| shebang-command | 2.0.0 | MIT |
| shebang-regex | 3.0.0 | MIT |
| side-channel | 1.1.1 | MIT |
| side-channel-list | 1.0.1 | MIT |
| side-channel-map | 1.0.1 | MIT |
| side-channel-weakmap | 1.0.2 | MIT |
| statuses | 2.0.2 | MIT |
| toidentifier | 1.0.1 | MIT |
| type-is | 2.1.0 | MIT |
| unpipe | 1.0.0 | MIT |
| vary | 1.1.2 | MIT |
| which | 2.0.2 | ISC |
| wrappy | 1.0.2 | ISC |
| ws | 8.22.0 | MIT |
| zod | 3.25.76 | MIT |
| zod-to-json-schema | 3.25.2 | ISC |

## 4. Installed at first start: UE_MCP_Bridge plugin

| Component | Version | License | Use | Source |
|---|---|---|---|---|
| UE_MCP_Bridge (Unreal Editor plugin) | Same as ue-mcp (1.4.3 at the time of writing) | MIT, Copyright (c) 2026 David Bingham | Editor-side bridge the ue-mcp server talks to | https://github.com/db-lyon/ue-mcp (`plugin/ue_mcp_bridge`) |

Installed by `ue-mcp init` as prebuilt binaries into `<Project>/Plugins/UE_MCP_Bridge`, downloaded from the ue-mcp GitHub release for that version (`UE_MCP_Bridge-<version>-UE<engine>-Win64.zip`). It links only Unreal Engine modules and plugins and contains no third-party code libraries.

## 5. Installed at first start: Studio extension

| Component | Version | License | Use | Source |
|---|---|---|---|---|
| UE-MCP Studio extension (agent, web UI, `studio-window.exe`) | 0.1.7 at the time of writing | MIT, Copyright (c) 2026 David Bingham | The agent, its UI, and Studio's window | https://github.com/db-lyon/ue-mcp-studio |
| Microsoft WebView2 SDK (static loader `WebView2LoaderStatic.lib`, linked into `studio-window.exe`) | 1.0.4191.47 | BSD-3-Clause (Microsoft) | Hosts the UI in Studio's window | https://www.nuget.org/packages/Microsoft.Web.WebView2 |

Downloaded from the GitHub releases of db-lyon/ue-mcp-studio (`latest.json`, `studio-<version>.zip`) into `%USERPROFILE%\.ue-mcp\extensions\studio`; the window app is copied to `%USERPROFILE%\.ue-mcp\studio\window`. The release carries a third-party notices file with each package's license text, produced by `scripts/notices.mjs`. The packages bundled in the agent and the UI, as that script lists them for 0.1.7 (120 packages):

| Package | Version | License |
|---|---|---|
| @anthropic-ai/claude-agent-sdk | 0.3.296 | Proprietary, see Section 6 |
| @anthropic-ai/sdk | 0.133.0 | MIT |
| @fontsource-variable/jetbrains-mono | 5.3.0 | OFL-1.1 |
| @fontsource-variable/plus-jakarta-sans | 5.3.0 | OFL-1.1 |
| @fontsource/instrument-serif | 5.3.0 | OFL-1.1 |
| @fontsource/roboto | 5.3.0 | OFL-1.1 |
| @stablelib/base64 | 1.0.1 | MIT |
| @types/debug | 4.1.13 | MIT |
| @types/estree | 1.0.9 | MIT |
| @types/estree-jsx | 1.0.5 | MIT |
| @types/hast | 3.0.5 | MIT |
| @types/mdast | 4.0.4 | MIT |
| @types/ms | 2.1.0 | MIT |
| @types/unist | 3.0.3 | MIT |
| @types/unist | 2.0.11 | MIT |
| @ungap/structured-clone | 1.4.0 | ISC |
| bail | 2.0.2 | MIT |
| ccount | 2.0.1 | MIT |
| character-entities | 2.0.2 | MIT |
| character-entities-html4 | 2.1.0 | MIT |
| character-entities-legacy | 3.0.0 | MIT |
| character-reference-invalid | 2.0.1 | MIT |
| comma-separated-tokens | 2.0.3 | MIT |
| debug | 4.4.3 | MIT |
| decode-named-character-reference | 1.3.0 | MIT |
| dequal | 2.0.3 | MIT |
| devlop | 1.1.0 | MIT |
| escape-string-regexp | 5.0.0 | MIT |
| estree-util-is-identifier-name | 3.0.0 | MIT |
| extend | 3.0.2 | MIT |
| fast-sha256 | 1.3.0 | Unlicense |
| hast-util-to-jsx-runtime | 2.3.6 | MIT |
| hast-util-whitespace | 3.0.0 | MIT |
| html-url-attributes | 3.0.1 | MIT |
| inline-style-parser | 0.2.7 | MIT |
| is-alphabetical | 2.0.1 | MIT |
| is-alphanumerical | 2.0.1 | MIT |
| is-decimal | 2.0.1 | MIT |
| is-hexadecimal | 2.0.1 | MIT |
| is-plain-obj | 4.1.0 | MIT |
| js-tokens | 4.0.0 | MIT |
| longest-streak | 3.1.0 | MIT |
| loose-envify | 1.4.0 | MIT |
| markdown-table | 3.0.4 | MIT |
| mdast-util-find-and-replace | 3.0.3 | MIT |
| mdast-util-from-markdown | 2.1.0 | MIT |
| mdast-util-gfm | 3.1.0 | MIT |
| mdast-util-gfm-autolink-literal | 2.0.1 | MIT |
| mdast-util-gfm-footnote | 2.1.0 | MIT |
| mdast-util-gfm-strikethrough | 2.0.1 | MIT |
| mdast-util-gfm-table | 2.0.0 | MIT |
| mdast-util-gfm-task-list-item | 2.0.0 | MIT |
| mdast-util-mdx-expression | 2.0.1 | MIT |
| mdast-util-mdx-jsx | 3.2.0 | MIT |
| mdast-util-mdxjs-esm | 2.0.1 | MIT |
| mdast-util-newline-to-break | 2.0.0 | MIT |
| mdast-util-phrasing | 4.1.0 | MIT |
| mdast-util-to-hast | 13.2.1 | MIT |
| mdast-util-to-markdown | 2.2.0 | MIT |
| mdast-util-to-string | 4.0.0 | MIT |
| micromark | 4.0.3 | MIT |
| micromark-core-commonmark | 2.0.4 | MIT |
| micromark-extension-gfm | 3.0.0 | MIT |
| micromark-extension-gfm-autolink-literal | 2.1.0 | MIT |
| micromark-extension-gfm-footnote | 2.1.0 | MIT |
| micromark-extension-gfm-strikethrough | 2.1.0 | MIT |
| micromark-extension-gfm-table | 2.1.2 | MIT |
| micromark-extension-gfm-tagfilter | 2.0.0 | MIT |
| micromark-extension-gfm-task-list-item | 2.1.0 | MIT |
| micromark-factory-destination | 2.0.1 | MIT |
| micromark-factory-label | 2.0.1 | MIT |
| micromark-factory-space | 2.1.0 | MIT |
| micromark-factory-title | 2.0.1 | MIT |
| micromark-factory-whitespace | 2.0.1 | MIT |
| micromark-util-character | 2.1.1 | MIT |
| micromark-util-chunked | 2.0.1 | MIT |
| micromark-util-classify-character | 2.0.1 | MIT |
| micromark-util-combine-extensions | 2.0.1 | MIT |
| micromark-util-decode-numeric-character-reference | 2.0.2 | MIT |
| micromark-util-decode-string | 2.0.1 | MIT |
| micromark-util-edit-map | 1.0.0 | MIT |
| micromark-util-encode | 2.0.1 | MIT |
| micromark-util-html-tag-name | 2.0.1 | MIT |
| micromark-util-normalize-identifier | 2.0.1 | MIT |
| micromark-util-resolve-all | 2.0.1 | MIT |
| micromark-util-sanitize-uri | 2.0.1 | MIT |
| micromark-util-subtokenize | 2.1.0 | MIT |
| micromark-util-symbol | 2.0.1 | MIT |
| micromark-util-types | 2.0.3 | MIT |
| ms | 2.1.3 | MIT |
| parse-entities | 4.0.2 | MIT |
| property-information | 7.2.0 | MIT |
| react | 18.3.1 | MIT |
| react-dom | 18.3.1 | MIT |
| react-markdown | 9.1.0 | MIT |
| remark-breaks | 4.0.0 | MIT |
| remark-gfm | 4.0.1 | MIT |
| remark-parse | 11.0.0 | MIT |
| remark-rehype | 11.1.2 | MIT |
| remark-stringify | 11.0.0 | MIT |
| scheduler | 0.23.2 | MIT |
| space-separated-tokens | 2.0.2 | MIT |
| standardwebhooks | 1.1.1 | MIT |
| stringify-entities | 4.0.4 | MIT |
| style-to-js | 1.1.21 | MIT |
| style-to-object | 1.0.14 | MIT |
| trim-lines | 3.0.1 | MIT |
| trough | 2.2.0 | MIT |
| ue-mcp | 1.4.3 | MIT |
| unified | 11.0.5 | MIT |
| unist-util-is | 6.0.1 | MIT |
| unist-util-position | 5.0.0 | MIT |
| unist-util-stringify-position | 4.0.0 | MIT |
| unist-util-visit | 5.1.0 | MIT |
| unist-util-visit-parents | 6.0.2 | MIT |
| vfile | 6.0.3 | MIT |
| vfile-message | 4.0.3 | MIT |
| yaml | 2.9.1 | ISC |
| zod | 4.6.6 | MIT |
| zwitch | 2.0.4 | MIT |

## 6. Downloaded on use: Claude Agent SDK runtime

| Component | Version | License | Use | Source |
|---|---|---|---|---|
| @anthropic-ai/claude-agent-sdk (bundled in the Studio extension, listed above) | 0.3.296 | Proprietary: "© Anthropic PBC. All rights reserved. Use is subject to the Legal Agreements outlined here: https://code.claude.com/docs/en/legal-and-compliance." | Runs the optional Claude subscription sign-in | https://www.npmjs.com/package/@anthropic-ai/claude-agent-sdk |
| @anthropic-ai/claude-agent-sdk-win32-x64 (Claude runtime binary) | Matches the SDK version | Same as above | Downloaded from registry.npmjs.org and checked against the registry's integrity hash the first time the user picks the Claude subscription sign-in. Kept in `%USERPROFILE%\.ue-mcp\studio\runtime`. Runs as a separate process | https://www.npmjs.com/package/@anthropic-ai/claude-agent-sdk-win32-x64 |

This is the only component that is not open source. It runs only when the user chooses the optional Claude subscription sign-in.

## 7. Network services

| Service | Host | When | Purpose |
|---|---|---|---|
| npm registry | registry.npmjs.org | After consent: first start, updates, the Claude runtime download | Downloads ue-mcp and the Claude runtime; ue-mcp's update check |
| GitHub releases | github.com and its release download hosts | After consent: first start, editor start, updates | Bridge binaries, the Studio extension and its `latest.json`, the editor plugin manifest (`plugin.json`) |
| GitHub API | api.github.com | Updates on the Beta channel | Lists releases to find the newest prerelease |
| Anthropic API | api.anthropic.com | The user runs an Anthropic model with an API key | Model calls and the model list |
| Claude account | Through the Claude runtime | The user picks the optional Claude subscription sign-in | Sign-in and model calls on the user's Claude plan |
| OpenAI API | api.openai.com | The user runs an OpenAI model with an API key or the ChatGPT sign-in | Model calls and the model list |
| OpenAI sign-in | auth.openai.com | The user signs in with ChatGPT | OAuth sign-in with PKCE; the callback lands on 127.0.0.1 |
| Google Gemini API | generativelanguage.googleapis.com | The user runs a Google model with an API key | Model calls (OpenAI-compatible endpoint) and the model list |
| ue-mcp documentation | ue-mcp.com | The user opens the UE-MCP Docs tab | Documentation pages |
| ue-mcp plugin registry | plugins.ue-mcp.com | The user browses ue-mcp plugins in Studio | Read-only plugin catalog |
| ue-mcp feedback | feedback.ue-mcp.com | The user sends feedback from Studio, or the agent's feedback tool runs | Opens an issue on the ue-mcp GitHub repository |
| Any web page | Chosen by the agent | The agent calls `web_fetch` | Reads a web page for the agent |

The plugin, the ue-mcp daemon and Studio's window talk to each other on 127.0.0.1 only. API keys and sign-in tokens stay on the user's machine under `%USERPROFILE%\.ue-mcp\studio`.

## Form answers

Fab's Third Party Software Submission Form (https://docs.google.com/forms/d/e/1FAIpQLSe5XW5i2Dkm3QTDw5UyRYEn3eUeBXLIACE8s_yidqt7vG1ziA/viewform) takes at most 10 entries. Its first page asks for Seller Name, Plugin Name and Total Number of TPS (1 to 10). The questions for each entry appear only after a count is chosen and could not be read without filling the form, so each entry below gives name, version, license, license URL, source, use and distribution for pasting into whatever fields it shows.

- **Seller Name:** (your Fab seller name)
- **Plugin Name:** UE-MCP Studio
- **Total Number of TPS:** 9

1. **Node.js** (with npm and npx)
    - Version: 20 or newer, installed by the user
    - License: MIT (Node.js); npm is Artistic License 2.0
    - License URL: https://github.com/nodejs/node/blob/main/LICENSE
    - Source: https://nodejs.org
    - Use: runtime for the ue-mcp server and the Studio extension; the plugin starts `npx` from it as a separate process
    - Distribution: not shipped; the user installs it from nodejs.org

2. **ue-mcp** (npm package, with 95 runtime npm dependencies under MIT, ISC, BSD-2-Clause, BSD-3-Clause and Python-2.0)
    - Version: 1.4.3 at submission
    - License: MIT
    - License URL: https://github.com/db-lyon/ue-mcp/blob/main/LICENSE
    - Source: https://www.npmjs.com/package/ue-mcp
    - Use: MCP server and daemon, a separate Node.js process
    - Distribution: not shipped; downloaded from registry.npmjs.org at first start after the user's consent

3. **UE_MCP_Bridge** (Unreal Editor plugin)
    - Version: matches ue-mcp (1.4.3 at submission)
    - License: MIT
    - License URL: https://github.com/db-lyon/ue-mcp/blob/main/LICENSE
    - Source: https://github.com/db-lyon/ue-mcp
    - Use: editor plugin the ue-mcp server connects to
    - Distribution: not shipped; prebuilt binaries downloaded from GitHub releases into the project's Plugins folder at first start after the user's consent

4. **UE-MCP Studio extension** (agent, UI and studio-window.exe, with 120 bundled npm packages under MIT, ISC, OFL-1.1 and Unlicense, plus the Claude Agent SDK in entry 6)
    - Version: 0.1.7 at submission
    - License: MIT
    - License URL: https://github.com/db-lyon/ue-mcp-studio/blob/main/LICENSE
    - Source: https://github.com/db-lyon/ue-mcp-studio
    - Use: the agent and its UI, loaded by the ue-mcp daemon; the window is a separate process
    - Distribution: not shipped; downloaded from GitHub releases at first start after the user's consent

5. **Microsoft WebView2 SDK** (static loader)
    - Version: 1.0.4191.47
    - License: BSD-3-Clause
    - License URL: https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4191.47/License
    - Source: https://www.nuget.org/packages/Microsoft.Web.WebView2
    - Use: statically linked into studio-window.exe to host the UI
    - Distribution: inside the Studio extension, not in the Fab package

6. **Claude Agent SDK and Claude runtime** (@anthropic-ai/claude-agent-sdk, @anthropic-ai/claude-agent-sdk-win32-x64)
    - Version: 0.3.296
    - License: proprietary, Anthropic's legal agreements
    - License URL: https://code.claude.com/docs/en/legal-and-compliance
    - Source: https://www.npmjs.com/package/@anthropic-ai/claude-agent-sdk
    - Use: the optional Claude subscription sign-in only; the runtime is a separate process
    - Distribution: the SDK is bundled in the Studio extension; the runtime is downloaded from registry.npmjs.org on first use of that sign-in

7. **Microsoft Edge WebView2 Runtime**
    - Version: Evergreen
    - License: Microsoft software license terms
    - License URL: https://developer.microsoft.com/en-us/microsoft-edge/webview2/
    - Source: https://developer.microsoft.com/en-us/microsoft-edge/webview2/
    - Use: renders Studio's window
    - Distribution: not shipped or installed; part of Windows 11, installed by the user on Windows 10

8. **Fonts in the Fab package:** Plus Jakarta Sans, JetBrains Mono, Instrument Serif
    - Version: as published by each project (woff2)
    - License: SIL Open Font License 1.1
    - License URL: https://openfontlicense.org
    - Source: https://github.com/tokotype/PlusJakartaSans, https://github.com/JetBrains/JetBrainsMono, https://github.com/Instrument/instrument-serif
    - Use: loaded by the plugin's bundled setup page
    - Distribution: shipped in `Resources/web/fonts`, unmodified, each with its OFL text

9. **Model provider and download services:** Anthropic API, OpenAI API and sign-in, Google Gemini API, npm registry, GitHub releases
    - Version: not applicable (online services)
    - License: each provider's terms of service, accepted by the user with their own account or API key
    - Source: api.anthropic.com, api.openai.com, auth.openai.com, generativelanguage.googleapis.com, registry.npmjs.org, github.com
    - Use: model calls the user chooses; downloads after the user's consent
    - Distribution: network services only, nothing shipped

Visual Studio is left out of the form because nothing Studio ships or installs uses it. Add it as a tenth entry if Fab wants every optional prerequisite listed.
