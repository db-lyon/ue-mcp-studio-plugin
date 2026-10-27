#!/usr/bin/env node
// Serves the setup page (Resources/web/bootstrap.html) in a browser with a fake
// window.ue.studio, so every step of it can be seen and clicked without an editor
// or an uninstall.
//
//   node scripts/setup-preview.mjs                      canned plan
//   node scripts/setup-preview.mjs --project <.uproject> the real `ue-mcp init --dry-run` plan for that project (writes nothing)
//   --port <n>                                          default 5180
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const web = path.resolve(here, "../StudioHost/Plugins/UE_MCP_Studio/Resources/web");
const arg = (name) => {
  const i = process.argv.indexOf(name);
  return i > 0 ? process.argv[i + 1] : undefined;
};
const port = Number(arg("--port") ?? 5180);
const project = arg("--project");

const canned = (isReInit) => ({
  version: "preview",
  project: { path: "C:/Projects/Preview/Preview.uproject", dir: "C:/Projects/Preview", name: "Preview", engine: "5.8", engineAssociation: "5.8" },
  isReInit,
  clients: [
    { id: "claude-code", name: "Claude Code", detected: true, projectScoped: true, selected: true },
    { id: "codex", name: "Codex", detected: true, projectScoped: false, selected: false },
    { id: "cursor", name: "Cursor", detected: true, projectScoped: true, selected: false },
  ],
  nativeTools: { enabled: true, exclude: [] },
  contextStrategy: "micro",
  feedback: isReInit,
  skills: false,
});

let realPlan = null;
if (project) {
  // The same dry run the plugin's StartSetupPlan spawns.
  const r = spawnSync("npx", ["-y", "ue-mcp@beta", "init", "--yes", "--json", "--dry-run", "--install=binary", `--project=${project}`], { encoding: "utf-8", shell: true });
  try {
    realPlan = JSON.parse(r.stdout).plan;
    console.log(`plan from ue-mcp init --dry-run for ${project}`);
  } catch {
    console.error(`ue-mcp init --dry-run did not return a plan:\n${r.stdout}\n${r.stderr}`);
    process.exit(1);
  }
}

const SCENARIOS = {
  "install-new": "Install, new project",
  "install-rerun": "Install, re-run on a configured project",
  "install-noplan": "Install, plan never arrives",
  "node-missing": "Runtime missing",
  "restart-source": "Restart to build (source bridge)",
  "restart-binary": "Restart to load (binary bridge)",
  "downloading": "Downloading Studio",
  "starting": "Starting the MCP server",
  "connecting": "Connecting",
};

// Runs inside the page before its own script: the fake binding and a scenario bar.
const stub = (scenario, plan) => `<script>
(function () {
  var scenario = ${JSON.stringify(scenario)};
  var plan = ${JSON.stringify(plan)};
  var installedAt = 0;
  var sent = [];
  function status() {
    var s = { node: true, bridgeInstalled: false, bridgeOnDisk: null, bridgeNeedsRestart: false, daemon: false, studioVersion: "0.1.24", log: "Saved/UE_MCP_Studio/setup.log" };
    if (scenario === "node-missing") s.node = false;
    if (scenario === "restart-source") s.bridgeOnDisk = "source";
    if (scenario === "restart-binary") { s.bridgeOnDisk = "binary"; s.bridgeNeedsRestart = true; }
    if (scenario === "downloading") { s.bridgeInstalled = true; s.studioVersion = null; }
    if (scenario === "starting") s.bridgeInstalled = true;
    if (scenario === "connecting") { s.bridgeInstalled = true; s.daemon = true; }
    // After Install: installing for 4s, loading for 3s, then the server steps.
    if (installedAt) {
      var t = Date.now() - installedAt;
      if (t > 4000) s.bridgeOnDisk = "binary";
      if (t > 7000) s.bridgeInstalled = true;
      if (t > 10000) s.daemon = true;
    }
    return s;
  }
  function log(name, a) {
    sent.push(name + (a === undefined ? "" : " " + a));
    var el = document.getElementById("preview-sent");
    if (el) el.textContent = sent.filter(function (x) { return !/^get/.test(x); }).join("\\n");
  }
  var calls = {
    getstatus: function () { return JSON.stringify(status()); },
    startsetupplan: function () { return true; },
    getsetupplan: function () { return plan && scenario !== "install-noplan" ? JSON.stringify({ plan: plan }) : ""; },
    runsetup: function () { installedAt = Date.now(); return true; },
    installbridge: function () { installedAt = Date.now(); return true; },
    geteditortheme: function () { return "dark"; },
  };
  window.ue = { studio: new Proxy({}, { get: function (_, name) { return function (a) { log(name, a); return Promise.resolve(name in calls ? calls[name](a) : true); }; } }) };
  addEventListener("DOMContentLoaded", function () {
    var bar = document.createElement("div");
    bar.style.cssText = "position:fixed;right:8px;bottom:8px;max-width:420px;font:12px/1.4 monospace;background:#111;color:#ddd;border:1px solid #444;border-radius:6px;padding:8px;z-index:99999";
    bar.innerHTML = '<div><b>Preview</b> <select id="preview-pick"></select> <a href="#" id="preview-hide" style="color:#8af">hide</a></div><pre id="preview-sent" style="white-space:pre-wrap;margin:6px 0 0;max-height:160px;overflow:auto"></pre>';
    document.body.appendChild(bar);
    var pick = document.getElementById("preview-pick");
    Object.entries(${JSON.stringify(SCENARIOS)}).forEach(function (e) {
      var o = document.createElement("option"); o.value = e[0]; o.textContent = e[1]; o.selected = e[0] === scenario; pick.appendChild(o);
    });
    pick.onchange = function () { location.search = "?s=" + pick.value; };
    document.getElementById("preview-hide").onclick = function (e) { e.preventDefault(); bar.remove(); };
  });
})();
</script>`;

const types = { ".html": "text/html", ".woff2": "font/woff2", ".css": "text/css", ".js": "text/javascript", ".svg": "image/svg+xml", ".png": "image/png" };

http.createServer((req, res) => {
  const url = new URL(req.url, "http://localhost");
  if (url.pathname === "/" || url.pathname === "/bootstrap.html") {
    const scenario = SCENARIOS[url.searchParams.get("s")] ? url.searchParams.get("s") : "install-new";
    const plan = realPlan ?? canned(scenario === "install-rerun");
    const page = fs.readFileSync(path.join(web, "bootstrap.html"), "utf-8").replace("<script>", stub(scenario, plan) + "\n<script>");
    res.writeHead(200, { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" });
    return res.end(page);
  }
  const file = path.join(web, decodeURIComponent(url.pathname));
  if (!file.startsWith(web) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
    res.writeHead(404);
    return res.end();
  }
  res.writeHead(200, { "content-type": types[path.extname(file)] ?? "application/octet-stream" });
  fs.createReadStream(file).pipe(res);
}).listen(port, "127.0.0.1", () => console.log(`setup page preview: http://localhost:${port}/`));
