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
 *             submission Fab builds itself, checks it against Fab's file rules, then
 *             builds the unpacked zip with BuildPlugin the way Fab does.
 *             --fab-url sets FabURL once the listing exists.
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
  buildPlugin(uat, engine, UPLUGIN, pkg);
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
    for (const d of ["Source", "Resources", "Config"]) fs.cpSync(path.join(path.dirname(UPLUGIN), d), path.join(fabPkg, d), { recursive: true });
    // The Fab copy compiles with UE_MCP_STUDIO_FAB=1, so it never offers to replace its own files.
    const buildCs = path.join(fabPkg, "Source", "UE_MCP_Studio", "UE_MCP_Studio.Build.cs");
    const rules = fs.readFileSync(buildCs, "utf-8");
    if (rules.split("const bool bFabBuild = false;").length !== 2) throw new Error(`${buildCs} has no single "const bool bFabBuild = false;" line to switch`);
    fs.writeFileSync(buildCs, rules.replace("const bool bFabBuild = false;", "const bool bFabBuild = true;"));
    // Fab's layout lists Content; the plugin has none, so it ships empty.
    fs.mkdirSync(path.join(fabPkg, "Content"));
    const fabDescriptor = { ...descriptor, EngineVersion: `${engine}.0` };
    if (fabUrl) fabDescriptor.FabURL = fabUrl;
    fs.writeFileSync(path.join(fabPkg, "UE_MCP_Studio.uplugin"), JSON.stringify(fabDescriptor, null, "\t") + "\n");
    const fabName = `UE_MCP_Studio-${version}-UE${engine}-Fab.zip`;
    const fabZip = path.join(OUT, fabName);
    zip(fabStage, "UE_MCP_Studio", fabZip);
    fs.rmSync(fabStage, { recursive: true, force: true });
    checkFabZip(fabZip);
    // Fab builds the submission itself; build the unpacked zip the same way, without unity or PCHs so every header stands alone.
    const verify = path.join(os.tmpdir(), "uemcps", `UE${engine}-fabcheck`);
    fs.rmSync(verify, { recursive: true, force: true });
    fs.mkdirSync(verify, { recursive: true });
    execFileSync(tar, ["-x", "-f", fabZip, "-C", verify], { stdio: "inherit" });
    buildPlugin(uat, engine, path.join(verify, "UE_MCP_Studio", "UE_MCP_Studio.uplugin"), path.join(verify, "Package", "UE_MCP_Studio"), ["-StrictIncludes"]);
    fs.rmSync(verify, { recursive: true, force: true });
    console.log(`${fabName} packaged and built clean`);
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

/** RunUAT BuildPlugin for Win64, plus any extra flags; throws on failure or on any warning in the plugin's own sources. */
function buildPlugin(uat, engine, uplugin, pkg, extra = []) {
  const argsLine = [uat, "BuildPlugin", `-Plugin=${uplugin}`, `-Package=${pkg}`, "-TargetPlatforms=Win64", "-Rocket", ...extra].map((x) => `"${x}"`).join(" ");
  const r = spawnSync(process.env.ComSpec ?? "cmd.exe", ["/d", "/s", "/c", `"${argsLine}"`], { stdio: ["ignore", "pipe", "inherit"], encoding: "utf-8", maxBuffer: 256 * 1024 * 1024, windowsVerbatimArguments: true });
  process.stdout.write(r.stdout ?? "");
  if (r.status !== 0) throw new Error(`BuildPlugin ${uplugin} failed for UE ${engine}`);
  const warnings = buildWarnings(r.stdout ?? "");
  if (warnings.length) throw new Error(`UE ${engine} build has warnings in the plugin's sources:\n${warnings.join("\n")}`);
}

/**
 * Fab's file rules, read from the zip itself: no scripts, executables or build output, no
 * license of our own (Fab's EULA governs), names of English letters, digits and underscores,
 * paths at most 170 characters, and a copyright first line in every source file.
 */
function checkFabZip(file) {
  const entries = execFileSync(tar, ["-t", "-f", file], { encoding: "utf-8" }).split(/\r?\n/).filter(Boolean).map((e) => e.replace(/\/$/, ""));
  const problems = [];
  const banned = /\.(sh|bat|cmd|ps1|mjs|cjs|js|exe|msi|dll|pdb|lib|so|dylib)$/i;
  for (const entry of entries) {
    const parts = entry.split("/");
    if (entry.length > 170) problems.push(`${entry}: longer than 170 characters`);
    if (banned.test(entry)) problems.push(`${entry}: a file type Fab does not accept`);
    for (const part of parts) {
      if (["Binaries", "Intermediate", "Saved", "DerivedDataCache"].includes(part)) problems.push(`${entry}: build output`);
      if (!/^[A-Za-z0-9_]+(\.[A-Za-z0-9_]+)*$/.test(part)) problems.push(`${entry}: "${part}" is not English letters, digits and underscores`);
    }
    if (/^LICENSE/i.test(parts[parts.length - 1])) problems.push(`${entry}: the Fab copy is under Fab's EULA`);
  }
  const unpacked = fs.mkdtempSync(path.join(os.tmpdir(), "uemcps-check-"));
  try {
    execFileSync(tar, ["-x", "-f", file, "-C", unpacked], { stdio: "inherit" });
    for (const f of walk(unpacked).filter((f) => /\.(h|hpp|inl|cpp|cs)$/i.test(f))) {
      const first = fs.readFileSync(f, "utf-8").replace(/^﻿/, "").split(/\r?\n/, 1)[0];
      if (!/^\/\/ Copyright \d{4} \S.*\. All Rights Reserved\.$/.test(first)) problems.push(`${path.relative(unpacked, f)}: the first line is not a copyright notice`);
    }
  } finally {
    fs.rmSync(unpacked, { recursive: true, force: true });
  }
  for (const required of ["UE_MCP_Studio/UE_MCP_Studio.uplugin", "UE_MCP_Studio/Config/FilterPlugin.ini", "UE_MCP_Studio/Resources/Icon128.png"]) {
    if (!entries.includes(required)) problems.push(`${required}: missing`);
  }
  if (problems.length) throw new Error(`${path.basename(file)} breaks Fab's rules:\n${problems.join("\n")}`);
}

/** Compiler warnings raised in UE_MCP_Studio's own source files, from BuildPlugin's output. */
function buildWarnings(output) {
  const found = new Set();
  for (const line of output.split(/\r?\n/)) {
    if (/UE_MCP_Studio[\\/]Source/.test(line) && /\bwarning\b/i.test(line)) found.add(line.trim());
  }
  return [...found];
}
