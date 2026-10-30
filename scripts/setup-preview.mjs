#!/usr/bin/env node
// Serves the setup page (Resources/web/bootstrap.html) in a browser with a fake
// window.ue.studio, so every step of it can be seen and clicked without an editor
// or an uninstall.
//
//   node scripts/setup-preview.mjs                       canned plan
//   node scripts/setup-preview.mjs --project <.uproject>  the real `ue-mcp init --dry-run` plan for that project (writes nothing)
//   --port <n>                                           default 5180
//
// The URL picks what is shown: ?s=<scenario>&page=<setup page>&theme=<ue-mcp|editor|editor-orange>,
// for example /?s=install-rerun&page=agent&theme=editor. The picker in the corner sets s and theme.
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { SCENARIOS, THEMES, planFor, stubbedPage } from "./setup-fake.mjs";

const here = path.dirname(fileURLToPath(import.meta.url));
const pluginDir = path.resolve(here, "../StudioHost/Plugins/UE_MCP_Studio");
const web = path.join(pluginDir, "Resources", "web");
const arg = (name) => {
  const i = process.argv.indexOf(name);
  return i > 0 ? process.argv[i + 1] : undefined;
};
const port = Number(arg("--port") ?? 5180);
const project = arg("--project");

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

const types = { ".html": "text/html", ".woff2": "font/woff2", ".css": "text/css", ".js": "text/javascript", ".svg": "image/svg+xml", ".png": "image/png" };

http.createServer((req, res) => {
  const url = new URL(req.url, "http://localhost");
  if (url.pathname === "/" || url.pathname === "/bootstrap.html") {
    const scenario = SCENARIOS[url.searchParams.get("s")] ? url.searchParams.get("s") : "install-new";
    const theme = THEMES.includes(url.searchParams.get("theme")) ? url.searchParams.get("theme") : "ue-mcp";
    const page = /^[a-z]+$/.test(url.searchParams.get("page") ?? "") ? url.searchParams.get("page") : "";
    const html = fs.readFileSync(path.join(web, "bootstrap.html"), "utf-8");
    const body = stubbedPage(html, { scenario, plan: realPlan ?? planFor(scenario), theme, page, bar: url.searchParams.get("bar") !== "0" });
    res.writeHead(200, { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" });
    return res.end(body);
  }
  // The page's fonts sit in Source/ThirdParty, which its relative URLs resolve to from the root.
  const rel = decodeURIComponent(url.pathname);
  const root = rel.startsWith("/Source/ThirdParty/") ? path.join(pluginDir, "Source", "ThirdParty") : web;
  const file = path.join(root === web ? web : pluginDir, rel);
  if (!file.startsWith(root) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
    res.writeHead(404);
    return res.end();
  }
  res.writeHead(200, { "content-type": types[path.extname(file)] ?? "application/octet-stream" });
  fs.createReadStream(file).pipe(res);
}).listen(port, "127.0.0.1", () => console.log(`setup page preview: http://localhost:${port}/`));
