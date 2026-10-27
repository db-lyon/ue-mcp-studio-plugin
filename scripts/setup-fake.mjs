// The fake window.ue.studio the setup page (Resources/web/bootstrap.html) runs against
// outside the editor, shared by scripts/setup-preview.mjs and scripts/setup-page.test.mjs.

/** An `ue-mcp init --dry-run` plan. `legacy` drops the fields older ue-mcp releases do not send. */
export function cannedPlan({ isReInit = false, engine = "5.8", legacy = false, feedbackMode } = {}) {
  const plan = {
    version: "preview",
    project: { path: "C:/Projects/Preview/Preview.uproject", dir: "C:/Projects/Preview", name: "Preview", engine, engineAssociation: engine },
    isReInit,
    install: { kind: "binary", existing: isReInit ? "binary" : null, pluginDir: "C:/Projects/Preview/Plugins/UE_MCP_Bridge", installedVersion: isReInit ? "1.4.11" : null, reason: "requested with --install=binary", writes: true, toolchain: { present: false, name: null } },
    requiredPlugins: ["PythonScriptPlugin"],
    disable: isReInit ? ["demo", "networking"] : [],
    nativeTools: { enabled: true, exclude: isReInit ? ["pcg"] : [] },
    contextStrategy: isReInit ? "lean" : "micro",
    clients: [
      { id: "claude-code", name: "Claude Code (project)", configPath: "C:/Projects/Preview/.mcp.json", detected: true, projectScoped: true, selected: true },
      { id: "codex", name: "Codex", configPath: "C:/Users/me/.codex/config.toml", detected: true, projectScoped: false, selected: false },
      { id: "cursor", name: "Cursor", configPath: "C:/Projects/Preview/.cursor/mcp.json", detected: true, projectScoped: true, selected: false },
      { id: "claude-desktop", name: "Claude Desktop", configPath: "C:/Users/me/AppData/Roaming/Claude/claude_desktop_config.json", detected: false, projectScoped: false, selected: false },
    ],
    launch: { pin: null, command: null },
    feedback: isReInit,
    promptHook: isReInit,
    skills: isReInit,
    blockers: [],
  };
  if (legacy) return plan;
  return {
    ...plan,
    categories: [
      ["level", "levels", "spawn/move/select actors, sublevels, world settings"],
      ["blueprint", "blueprints", "Blueprint classes: variables, functions, components, nodes"],
      ["material", "materials", "Materials, Material Functions, parameters"],
      ["asset", "assets", "import/move/rename/delete content browser assets"],
      ["animation", "animation", "anim sequences, montages, AnimBPs, skeletons"],
      ["niagara", "vfx (niagara)", "VFX systems, emitters, modules", ["Niagara"]],
      ["landscape", "landscape", "terrain edit, layers, paint, sculpt"],
      ["pcg", "pcg", "procedural content graphs", ["PCG"]],
      ["foliage", "foliage", "instanced foliage placement and types"],
      ["audio", "audio", "MetaSounds, sound cues, sound classes"],
      ["widget", "ui (widgets)", "UMG widgets, editor utility widgets"],
      ["gameplay", "gameplay / ai", "input mappings, collision, navmesh, PIE", ["EnhancedInput"]],
      ["gas", "gas", "Gameplay Ability System abilities, effects, attributes", ["GameplayAbilities"]],
      ["networking", "networking", "replication, dormancy, RPCs"],
      ["demo", "demo", "tutorial / demo project scaffolding"],
    ].map(([name, label, description, requiredPlugins]) => ({ name, label, description, ...(requiredPlugins ? { requiredPlugins } : {}) })),
    nativeCategories: ["animation", "asset", "blueprint", "gas", "level", "material", "niagara", "pcg", "widget"],
    bridge: { port: 51734, derivedPort: 51734, pinnedBy: null },
    feedbackAuth: { login: isReInit ? "octo" : null },
    feedbackMode: feedbackMode ?? (isReInit ? { mode: "defer", scope: "project", set: null } : { mode: "interactive", scope: "default", set: null }),
  };
}

/** What `ue-mcp init --json` prints when it finishes. */
export function cannedResult(ok) {
  return {
    ok,
    ...(ok ? {} : { error: "cannot pick prebuilt binaries: engine version of Preview is unknown (unset)" }),
    steps: ok
      ? [
          { id: "plugin", status: "changed", target: "C:/Projects/Preview/Plugins/UE_MCP_Bridge", detail: "installed prebuilt binaries from github.com/db-lyon/ue-mcp" },
          { id: "uproject", status: "changed", target: "C:/Projects/Preview/Preview.uproject", detail: "enabled UE_MCP_Bridge, PythonScriptPlugin" },
          { id: "gitignore", status: "changed", target: "C:/Projects/Preview/.gitignore", detail: "ignored ue-mcp.local.yml" },
          { id: "client:claude-code", status: "unchanged", target: "C:/Projects/Preview/.mcp.json", detail: "Claude Code (project) MCP server entry" },
          { id: "config", status: "changed", target: "C:/Projects/Preview/ue-mcp.yml", detail: "ue-mcp.yml created" },
        ]
      : [{ id: "error", status: "failed", target: "C:/Projects/Preview", detail: "cannot pick prebuilt binaries" }],
    warnings: [],
  };
}

export const SCENARIOS = {
  "install-new": "Setup, new project (5.8)",
  "install-rerun": "Setup, re-run on a configured project",
  "install-57": "Setup, UE 5.7 (no native tools)",
  "install-legacy": "Setup, older ue-mcp plan",
  "install-fails": "Setup, init fails",
  "install-noplan": "Setup, plan never arrives",
  "node-missing": "Runtime missing",
  "restart-source": "Restart to build (source bridge)",
  "restart-binary": "Restart to load (binary bridge)",
  "downloading": "Downloading Studio",
  "starting": "Starting the MCP server",
  "connecting": "Connecting",
};

export const THEMES = ["ue-mcp", "editor", "editor-orange"];

export function planFor(scenario) {
  return cannedPlan({
    isReInit: scenario === "install-rerun",
    engine: scenario === "install-57" ? "5.7" : "5.8",
    legacy: scenario === "install-legacy",
  });
}

/**
 * A script to run before the page's own: the fake binding, recording every call in
 * window.__studioCalls. `page` opens the setup on that page (project, tools, native,
 * context, clients, agent, review). `bar` adds the preview's scenario picker. `fast` makes init's result and the
 * sign-in come sooner, for tests; the install keeps its pace so each screen is seen.
 */
export function stubScript({ scenario, plan, theme = "ue-mcp", page = "", bar = false, fast = false }) {
  const resultAt = fast ? 50 : 2000;
  const codeAt = fast ? 100 : 1500;
  const signedInAt = fast ? 2500 : 6000;
  return `<script>
(function () {
  var scenario = ${JSON.stringify(scenario)};
  var plan = ${JSON.stringify(plan)};
  var result = ${JSON.stringify(cannedResult(scenario !== "install-fails"))};
  var theme = ${JSON.stringify(theme)};
  var installedAt = 0, authAt = 0;
  var calls = window.__studioCalls = [];
  // The setup page opens on the page its URL hash names.
  if (${JSON.stringify(page)}) history.replaceState(null, "", "#" + ${JSON.stringify(page)});
  function status() {
    var s = { node: true, bridgeInstalled: false, bridgeOnDisk: null, bridgeNeedsRestart: false, daemon: false, studioVersion: "0.1.24", log: "Saved/UE_MCP_Studio/setup.log" };
    if (scenario === "node-missing") s.node = false;
    if (scenario === "restart-source") s.bridgeOnDisk = "source";
    if (scenario === "restart-binary") { s.bridgeOnDisk = "binary"; s.bridgeNeedsRestart = true; }
    if (scenario === "downloading") { s.bridgeInstalled = true; s.studioVersion = null; }
    if (scenario === "starting") s.bridgeInstalled = true;
    if (scenario === "connecting") { s.bridgeInstalled = true; s.daemon = true; }
    // After Install: installing for 4s, loading for 3s, then the server steps. A failed init never lands.
    if (installedAt && scenario !== "install-fails") {
      var t = Date.now() - installedAt;
      if (t > 4000) s.bridgeOnDisk = "binary";
      if (t > 7000) s.bridgeInstalled = true;
      if (t > 10000) s.daemon = true;
    }
    return s;
  }
  function authLines() {
    if (!authAt) return "";
    var t = Date.now() - authAt, out = [];
    if (t > ${codeAt}) out.push(JSON.stringify({ event: "code", verificationUri: "https://github.com/login/device", userCode: "WDJB-MJHT", expiresAt: 0, interval: 5 }));
    if (t > ${signedInAt}) out.push(JSON.stringify({ event: "signed-in", login: "octo", cached: false }));
    return out.join("\\n");
  }
  var answers = {
    getstatus: function () { return JSON.stringify(status()); },
    startsetupplan: function () { return true; },
    getsetupplan: function () { return plan && scenario !== "install-noplan" ? JSON.stringify({ ok: true, dryRun: true, plan: plan }) : ""; },
    runsetup: function () { installedAt = Date.now(); return true; },
    installbridge: function () { installedAt = Date.now(); return true; },
    // The restart scenarios open as if Install had just finished.
    getsetupresult: function () { return /^restart/.test(scenario) || (installedAt && Date.now() - installedAt > ${resultAt}) ? JSON.stringify(result) : ""; },
    startfeedbackauth: function () { authAt = Date.now(); return true; },
    getfeedbackauth: function () { return authLines(); },
    getstudiotheme: function () { return theme; },
    geteditortheme: function () { return JSON.stringify({ background: "#151515", recessed: "#1a1a1a", panel: "#242424", header: "#2f2f2f", input: "#0f0f0f", dropdown: "#383838", dropdownOutline: "#4c4c4c", hover: "#575757", foreground: "#c0c0c0", foregroundHeader: "#c8c8c8", primary: "#0070e0", primaryHover: "#0e86ff", select: "#0070e0", warning: "#ffb800", error: "#ef3535", success: "#1fe44b" }); },
  };
  function log(name, a) {
    calls.push({ name: name, arg: a });
    var el = document.getElementById("preview-sent");
    if (el) el.textContent = calls.filter(function (c) { return !/^get/.test(c.name); }).map(function (c) { return c.name + (c.arg === undefined ? "" : " " + c.arg); }).join("\\n");
  }
  window.ue = { studio: new Proxy({}, { get: function (_, name) { return function (a) { log(name, a); return Promise.resolve(name in answers ? answers[name](a) : true); }; } }) };
  ${bar ? `addEventListener("DOMContentLoaded", function () {
    var bar = document.createElement("div");
    bar.style.cssText = "position:fixed;right:8px;bottom:8px;max-width:420px;font:12px/1.4 monospace;background:#111;color:#ddd;border:1px solid #444;border-radius:6px;padding:8px;z-index:99999";
    bar.innerHTML = '<div><b>Preview</b> <select id="preview-pick"></select> <select id="preview-theme"></select> <a href="#" id="preview-hide" style="color:#8af">hide</a></div><pre id="preview-sent" style="white-space:pre-wrap;margin:6px 0 0;max-height:160px;overflow:auto"></pre>';
    document.body.appendChild(bar);
    function fill(id, entries, chosen, param) {
      var pick = document.getElementById(id);
      entries.forEach(function (e) { var o = document.createElement("option"); o.value = e[0]; o.textContent = e[1]; o.selected = e[0] === chosen; pick.appendChild(o); });
      pick.onchange = function () { var q = new URLSearchParams(location.search); q.set(param, pick.value); location.search = q.toString(); };
    }
    fill("preview-pick", Object.entries(${JSON.stringify(SCENARIOS)}), scenario, "s");
    fill("preview-theme", ${JSON.stringify(THEMES)}.map(function (t) { return [t, "theme: " + t]; }), theme, "theme");
    document.getElementById("preview-hide").onclick = function (e) { e.preventDefault(); bar.remove(); };
  });` : ""}
})();
</script>`;
}

/** bootstrap.html with the stub injected before its own script. */
export function stubbedPage(html, opts) {
  return html.replace("<script>", stubScript(opts) + "\n<script>");
}
