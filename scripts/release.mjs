#!/usr/bin/env node
/**
 * Build, package and publish an editor plugin release.
 *
 *   node scripts/release.mjs [--engines 5.7,5.8] [--fab [--fab-url <url>]] [--publish] [--notes-file notes.md]
 *
 * Builds UE_MCP_Studio with RunUAT BuildPlugin for each engine and writes
 * release/UE_MCP_Studio-<version>-UE<engine>-Win64.zip, failing on any compiler
 * warning in the plugin's own sources, plus release/plugin.json, the manifest
 * Studio's updater reads. The version is the .uplugin's VersionName.
 *
 * --fab       also writes UE_MCP_Studio-<version>-UE<engine>-Fab.zip, the source-only
 *             submission Fab builds itself. --fab-url sets FabURL once the listing exists.
 * --publish   creates release v<version> on db-lyon/ue-mcp-studio-plugin. A stable version is
 *             marked latest, so .../releases/latest/download/plugin.json resolves to it; a
 *             prerelease is published beside it for testers and never becomes latest.
 *             It refuses unless every engine's zip is attached and plugin.json points at this release.
 */
import * as crypto from "node:crypto";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { execFileSync, spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const OUT = path.join(ROOT, "release");
const REPO = "db-lyon/ue-mcp-studio-plugin";
const UPLUGIN = path.join(ROOT, "StudioHost", "Plugins", "UE_MCP_Studio", "UE_MCP_Studio.uplugin");

const arg = (name, fallback) => {
  const i = process.argv.indexOf(name);
  return i >= 0 ? process.argv[i + 1] : fallback;
};
const has = (name) => process.argv.includes(name);
const run = (cmd, args, opts = {}) => {
  const r = spawnSync(cmd, args, { stdio: "inherit", ...opts });
  if (r.status !== 0) throw new Error(`${cmd} ${args.join(" ")} failed (${r.status})`);
};
const sha256 = (file) => crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");
const tar = process.platform === "win32" ? path.join(process.env.SystemRoot ?? "C:\\Windows", "System32", "tar.exe") : "tar";
const zip = (parent, folder, out) => {
  fs.rmSync(out, { force: true });
  execFileSync(tar, ["-a", "-c", "-f", out, "-C", parent, folder], { stdio: "inherit" });
};

const descriptor = JSON.parse(fs.readFileSync(UPLUGIN, "utf-8"));
const version = descriptor.VersionName;
if (!/^\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$/.test(version ?? "")) throw new Error(`UE_MCP_Studio.uplugin has no usable VersionName (${version})`);
const engines = arg("--engines", "5.7,5.8").split(",");
const fab = has("--fab");
const fabUrl = arg("--fab-url", null);
if (fab && !fabUrl) console.warn("no --fab-url: the Fab zip has no FabURL key, which Fab requires once the listing ID exists");

fs.rmSync(OUT, { recursive: true, force: true });
fs.mkdirSync(OUT, { recursive: true });

// BuildPlugin writes a default Config/FilterPlugin.ini into the source plugin; keep the tree clean.
const filterIni = path.join(path.dirname(UPLUGIN), "Config", "FilterPlugin.ini");
const hadFilterIni = fs.existsSync(filterIni);
const zips = [];
for (const engine of engines) {
  const engineRoot = path.join("C:/Program Files/Epic Games", `UE_${engine}`);
  const uat = path.join(engineRoot, "Engine", "Build", "BatchFiles", "RunUAT.bat");
  if (!fs.existsSync(uat)) throw new Error(`no UE ${engine} at ${engineRoot}: every release builds every engine`);
  const stage = path.join(os.tmpdir(), "uemcps", `UE${engine}`);
  const pkg = path.join(stage, "UE_MCP_Studio");
  fs.rmSync(stage, { recursive: true, force: true });
  const argsLine = [uat, "BuildPlugin", `-Plugin=${UPLUGIN}`, `-Package=${pkg}`, "-TargetPlatforms=Win64", "-Rocket"].map((x) => `"${x}"`).join(" ");
  const r = spawnSync(process.env.ComSpec ?? "cmd.exe", ["/d", "/s", "/c", `"${argsLine}"`], { stdio: ["ignore", "pipe", "inherit"], encoding: "utf-8", maxBuffer: 256 * 1024 * 1024, windowsVerbatimArguments: true });
  process.stdout.write(r.stdout ?? "");
  if (r.status !== 0) throw new Error(`BuildPlugin UE_MCP_Studio failed for UE ${engine}`);
  const warnings = buildWarnings(r.stdout ?? "");
  if (warnings.length) throw new Error(`UE ${engine} build has warnings in the plugin's sources:\n${warnings.join("\n")}`);
  for (const d of ["Intermediate", "HostProject"]) fs.rmSync(path.join(pkg, d), { recursive: true, force: true });
  const name = `UE_MCP_Studio-${version}-UE${engine}-Win64.zip`;
  zip(stage, "UE_MCP_Studio", path.join(OUT, name));
  fs.rmSync(stage, { recursive: true, force: true });
  if (!hadFilterIni) fs.rmSync(path.dirname(filterIni), { recursive: true, force: true });
  zips.push({ engine, name });
  console.log(`${name} packaged`);

  if (fab) {
    const fabStage = path.join(os.tmpdir(), "uemcps", `UE${engine}-fab`);
    const fabPkg = path.join(fabStage, "UE_MCP_Studio");
    fs.rmSync(fabStage, { recursive: true, force: true });
    fs.mkdirSync(fabPkg, { recursive: true });
    for (const d of ["Source", "Resources"]) fs.cpSync(path.join(path.dirname(UPLUGIN), d), path.join(fabPkg, d), { recursive: true });
    const fabDescriptor = { ...descriptor, EngineVersion: `${engine}.0` };
    if (fabUrl) fabDescriptor.FabURL = fabUrl;
    fs.writeFileSync(path.join(fabPkg, "UE_MCP_Studio.uplugin"), JSON.stringify(fabDescriptor, null, "\t") + "\n");
    const long = walk(fabPkg).map((f) => "UE_MCP_Studio/" + path.relative(fabPkg, f).replaceAll("\\", "/")).filter((f) => f.length > 170);
    if (long.length) throw new Error(`Fab caps paths at 170 characters: ${long.join(", ")}`);
    const fabName = `UE_MCP_Studio-${version}-UE${engine}-Fab.zip`;
    zip(fabStage, "UE_MCP_Studio", path.join(OUT, fabName));
    fs.rmSync(fabStage, { recursive: true, force: true });
    console.log(`${fabName} packaged`);
  }
}

const tag = `v${version}`;
const manifest = {
  version,
  files: Object.fromEntries(zips.map(({ engine, name }) => [engine, { url: `https://github.com/${REPO}/releases/download/${tag}/${name}`, sha256: sha256(path.join(OUT, name)) }])),
};
fs.writeFileSync(path.join(OUT, "plugin.json"), JSON.stringify(manifest, null, 2) + "\n");
console.log(`\nplugin.json: ${version} for UE ${Object.keys(manifest.files).join(", ")}`);

if (has("--publish")) {
  // Every release carries every engine's zip, so its page alone installs the plugin.
  for (const engine of engines) {
    const z = zips.find((x) => x.engine === engine);
    if (!z || !fs.existsSync(path.join(OUT, z.name))) throw new Error(`refusing to publish ${tag}: no zip for UE ${engine}`);
    if (!manifest.files[engine]?.url.includes(`/download/${tag}/`)) throw new Error(`refusing to publish ${tag}: plugin.json's UE ${engine} entry is not on this release`);
  }
  const notes = arg("--notes-file", null);
  const notesArgs = notes ? ["--notes-file", notes] : ["--notes", `UE-MCP Studio editor plugin ${version}.`];
  const files = fs.readdirSync(OUT).map((f) => path.join(OUT, f));
  // A prerelease is for testers: it never becomes latest, so Studio's updater cannot pick it up.
  const channel = version.includes("-") ? ["--prerelease", "--latest=false"] : ["--latest"];
  run("gh", ["release", "create", tag, "--repo", REPO, "--title", tag, "--target", "main", ...channel, ...notesArgs, ...files]);
  console.log(`published ${tag} to ${REPO}`);
}

/** Every file under dir. */
function walk(dir) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(dir, e.name)) : [path.join(dir, e.name)]));
}

/** Compiler warnings raised in UE_MCP_Studio's own source files, from BuildPlugin's output. */
function buildWarnings(output) {
  const found = new Set();
  for (const line of output.split(/\r?\n/)) {
    if (/UE_MCP_Studio[\/]Source/.test(line) && /\bwarning\b/i.test(line)) found.add(line.trim());
  }
  return [...found];
}
