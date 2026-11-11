#!/usr/bin/env node
/**
 * Build, package and publish an editor plugin release.
 *
 *   node scripts/release.mjs [--engines 5.7,5.8,5.9] [--rebuild] [--publish] [--notes-file notes.md]
 *
 * Writes release/UE_MCP_Studio-<version>-UE<engine>-Win64.zip for each engine, plus
 * release/plugin.json, the manifest Studio's updater reads. The version is the .uplugin's VersionName.
 *
 * When nothing under the plugin's Source changed since the last published release, and the
 * descriptor changed only in Version and VersionName, each zip is that release's zip with this
 * tree's Resources and Content, this version in its descriptor, and its Binaries untouched.
 * Otherwise, or with --rebuild, RunUAT BuildPlugin compiles each engine, failing on any
 * compiler warning in the plugin's own sources. UBT runs with -NoMutex beside other builds.
 *
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
const PLUGIN_DIR = path.dirname(UPLUGIN);
const PLUGIN_REL = "StudioHost/Plugins/UE_MCP_Studio";

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
const engines = arg("--engines", "5.7,5.8,5.9").split(",");

fs.rmSync(OUT, { recursive: true, force: true });
fs.mkdirSync(OUT, { recursive: true });

// BuildPlugin writes a default Config/FilterPlugin.ini into the source plugin; keep the tree clean.
const filterIni = path.join(PLUGIN_DIR, "Config", "FilterPlugin.ini");
const hadFilterIni = fs.existsSync(filterIni);
const tag = `v${version}`;
const prev = previousRelease(tag);
const sourceChange = compileReason(prev);
const zips = [];
for (const engine of engines) {
  const engineRoot = path.join("C:/Program Files/Epic Games", `UE_${engine}`);
  const uat = path.join(engineRoot, "Engine", "Build", "BatchFiles", "RunUAT.bat");
  const name = `UE_MCP_Studio-${version}-UE${engine}-Win64.zip`;
  const reason = sourceChange ?? (prev.manifest.files?.[engine] ? null : `${prev.tag} has no zip for UE ${engine}`);
  if (reason) {
    console.log(`UE ${engine}: compiling (${reason})`);
    if (!fs.existsSync(uat)) throw new Error(`no UE ${engine} at ${engineRoot}: every release builds every engine`);
    const stage = path.join(os.tmpdir(), "uemcps", `UE${engine}`);
    const pkg = path.join(stage, "UE_MCP_Studio");
    fs.rmSync(stage, { recursive: true, force: true });
    buildPlugin(uat, engine, UPLUGIN, pkg);
    for (const d of ["Intermediate", "HostProject"]) fs.rmSync(path.join(pkg, d), { recursive: true, force: true });
    zip(stage, "UE_MCP_Studio", path.join(OUT, name));
    fs.rmSync(stage, { recursive: true, force: true });
    if (!hadFilterIni) fs.rmSync(path.dirname(filterIni), { recursive: true, force: true });
  } else {
    console.log(`UE ${engine}: Source unchanged since ${prev.tag}, repackaging its binaries`);
    repackage(prev, engine, path.join(OUT, name));
  }
  zips.push({ engine, name });
  console.log(`${name} packaged`);
}

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

/**
 * RunUAT BuildPlugin for Win64; throws on failure or on any warning in the plugin's own sources.
 * BuildPlugin has no -ubtargs, so -NoMutex reaches UBT through UBT_EXTRA_ARGS: it neither waits on nor blocks other UBT runs.
 */
function buildPlugin(uat, engine, uplugin, pkg) {
  const argsLine = [uat, "BuildPlugin", `-Plugin=${uplugin}`, `-Package=${pkg}`, "-TargetPlatforms=Win64", "-Rocket"].map((x) => `"${x}"`).join(" ");
  const env = { ...process.env, UBT_EXTRA_ARGS: [process.env.UBT_EXTRA_ARGS, "-NoMutex"].filter(Boolean).join(" ") };
  const r = spawnSync(process.env.ComSpec ?? "cmd.exe", ["/d", "/s", "/c", `"${argsLine}"`], { stdio: ["ignore", "pipe", "inherit"], encoding: "utf-8", maxBuffer: 256 * 1024 * 1024, windowsVerbatimArguments: true, env });
  process.stdout.write(r.stdout ?? "");
  if (r.status !== 0) throw new Error(`BuildPlugin ${uplugin} failed for UE ${engine}`);
  const warnings = buildWarnings(r.stdout ?? "");
  if (warnings.length) throw new Error(`UE ${engine} build has warnings in the plugin's sources:\n${warnings.join("\n")}`);
}

/** The newest published release other than tag, with its plugin.json, or null when there is none. */
function previousRelease(tag) {
  const list = JSON.parse(execFileSync("gh", ["release", "list", "--repo", REPO, "--exclude-drafts", "--limit", "50", "--json", "tagName,publishedAt"], { encoding: "utf-8" }));
  const latest = list.filter((r) => r.tagName !== tag).sort((a, b) => b.publishedAt.localeCompare(a.publishedAt))[0];
  if (!latest) return null;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "uemcps-prev-"));
  try {
    run("gh", ["release", "download", latest.tagName, "--repo", REPO, "--pattern", "plugin.json", "--dir", dir]);
    return { tag: latest.tagName, manifest: JSON.parse(fs.readFileSync(path.join(dir, "plugin.json"), "utf-8")) };
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

/** Why the plugin has to be compiled rather than repackaged from prev, or null when prev's binaries still hold. */
function compileReason(prev) {
  if (has("--rebuild")) return "--rebuild";
  if (!prev) return "no earlier release";
  const git = (...args) => execFileSync("git", ["-C", ROOT, ...args], { encoding: "utf-8" });
  try {
    git("rev-parse", "--verify", "--quiet", `refs/tags/${prev.tag}`);
  } catch {
    git("fetch", "--quiet", "--no-tags", "origin", "tag", prev.tag);
  }
  // The working tree against the tag, so uncommitted and untracked sources count too.
  const source = `${PLUGIN_REL}/Source`;
  const changed = (git("diff", "--name-only", prev.tag, "--", source) + git("ls-files", "--others", "--exclude-standard", "--", source)).trim();
  if (changed) return `Source changed since ${prev.tag}: ${changed.split(/\r?\n/).join(", ")}`;
  const before = JSON.parse(git("show", `${prev.tag}:${PLUGIN_REL}/UE_MCP_Studio.uplugin`));
  if (beyondVersion(before) !== beyondVersion(descriptor)) return `UE_MCP_Studio.uplugin changed beyond Version and VersionName since ${prev.tag}`;
  return null;
}

/** A descriptor without Version and VersionName, for comparing everything else. */
function beyondVersion({ Version, VersionName, ...rest }) {
  return JSON.stringify(rest);
}

/** Writes out from prev's zip for engine: its Binaries and Source, this tree's Resources and Content, and this version in its descriptor. */
function repackage(prev, engine, out) {
  const entry = prev.manifest.files[engine];
  const prevName = entry.url.split("/").pop();
  const work = path.join(os.tmpdir(), "uemcps", `UE${engine}-repack`);
  const stage = path.join(work, "stage");
  fs.rmSync(work, { recursive: true, force: true });
  fs.mkdirSync(stage, { recursive: true });
  run("gh", ["release", "download", prev.tag, "--repo", REPO, "--pattern", prevName, "--dir", work]);
  const prevZip = path.join(work, prevName);
  if (sha256(prevZip) !== entry.sha256) throw new Error(`${prevName} does not match the SHA-256 in ${prev.tag}'s plugin.json`);
  execFileSync(tar, ["-x", "-f", prevZip, "-C", stage], { stdio: "inherit" });
  const pkg = path.join(stage, "UE_MCP_Studio");
  for (const d of ["Resources", "Content"]) {
    fs.rmSync(path.join(pkg, d), { recursive: true, force: true });
    if (fs.existsSync(path.join(PLUGIN_DIR, d))) fs.cpSync(path.join(PLUGIN_DIR, d), path.join(pkg, d), { recursive: true });
  }
  // The packaged descriptor is BuildPlugin's rewrite (Installed, EngineVersion); only its version moves.
  const pkgUplugin = path.join(pkg, "UE_MCP_Studio.uplugin");
  let text = fs.readFileSync(pkgUplugin, "utf-8");
  for (const [key, value] of [["Version", String(descriptor.Version)], ["VersionName", JSON.stringify(version)]]) {
    const re = new RegExp(`("${key}"\\s*:\\s*)("[^"]*"|\\d+)`);
    if (!re.test(text)) throw new Error(`${prevName}'s descriptor has no ${key}`);
    text = text.replace(re, (_, lead) => lead + value);
  }
  fs.writeFileSync(pkgUplugin, text);
  zip(stage, "UE_MCP_Studio", out);
  checkRepackage(prevZip, out);
  fs.rmSync(work, { recursive: true, force: true });
}

/** Throws unless next keeps prev's Binaries and Source byte for byte, carries this tree's Resources and Content, and differs from prev's descriptor only in this version. */
function checkRepackage(prevZip, nextZip) {
  const unpack = (file) => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "uemcps-check-"));
    execFileSync(tar, ["-x", "-f", file, "-C", dir], { stdio: "inherit" });
    return dir;
  };
  const hashes = (dir) => new Map(fs.existsSync(dir) ? walk(dir).map((f) => [path.relative(dir, f).split(path.sep).join("/"), sha256(f)]) : []);
  const same = (a, b) => a.size === b.size && [...a].every(([k, v]) => b.get(k) === v);
  const prevDir = unpack(prevZip);
  const nextDir = unpack(nextZip);
  const problems = [];
  try {
    const p = path.join(prevDir, "UE_MCP_Studio");
    const n = path.join(nextDir, "UE_MCP_Studio");
    for (const d of new Set([...fs.readdirSync(p), ...fs.readdirSync(n)])) {
      if (d === "UE_MCP_Studio.uplugin") continue;
      const fromTree = ["Resources", "Content"].includes(d);
      if (!same(hashes(path.join(n, d)), hashes(path.join(fromTree ? PLUGIN_DIR : p, d)))) problems.push(`${d} is not ${fromTree ? "this tree's" : "the previous zip's"}`);
    }
    const binaries = hashes(path.join(n, "Binaries"));
    if (![...binaries.keys()].some((f) => f.endsWith(".dll"))) problems.push("Binaries has no DLL");
    const before = JSON.parse(fs.readFileSync(path.join(p, "UE_MCP_Studio.uplugin"), "utf-8"));
    const after = JSON.parse(fs.readFileSync(path.join(n, "UE_MCP_Studio.uplugin"), "utf-8"));
    if (after.Version !== descriptor.Version || after.VersionName !== version) problems.push(`descriptor is ${after.VersionName} (${after.Version}), not ${version} (${descriptor.Version})`);
    if (beyondVersion(after) !== beyondVersion(before)) problems.push("descriptor changed beyond Version and VersionName");
    if (!problems.length) {
      const prevResources = hashes(path.join(p, "Resources"));
      const changed = [...hashes(path.join(n, "Resources"))].filter(([k, v]) => prevResources.get(k) !== v).map(([k]) => k);
      console.log(`${path.basename(nextZip)}: ${binaries.size} Binaries files identical to ${path.basename(prevZip)}; Resources changed: ${changed.join(", ") || "none"}; descriptor ${before.VersionName} (${before.Version}) to ${after.VersionName} (${after.Version})`);
    }
  } finally {
    fs.rmSync(prevDir, { recursive: true, force: true });
    fs.rmSync(nextDir, { recursive: true, force: true });
  }
  if (problems.length) throw new Error(`${path.basename(nextZip)} is not a clean repackage of ${path.basename(prevZip)}:\n${problems.join("\n")}`);
}

/** Compiler warnings raised in UE_MCP_Studio's own source files, from BuildPlugin's output. */
function buildWarnings(output) {
  const found = new Set();
  for (const line of output.split(/\r?\n/)) {
    if (/UE_MCP_Studio[\\/]Source/.test(line) && /\bwarning\b/i.test(line)) found.add(line.trim());
  }
  return [...found];
}
