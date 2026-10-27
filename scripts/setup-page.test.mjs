#!/usr/bin/env node
// Walks the setup page (Resources/web/bootstrap.html) in jsdom against the fake binding in
// setup-fake.mjs: every setup page, the defaults for a new project and a re-run, choices kept
// across Back and Next, the keyboard, and the exact JSON the page sends to runsetup.
//
//   node --test scripts/setup-page.test.mjs
//
// The repo has no package.json, so jsdom is resolved like any require: a node_modules above
// this file, or NODE_PATH. `npm i --no-save jsdom` in the repo root provides one.
import { afterEach, test } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";
import { cannedPlan, planFor, stubbedPage } from "./setup-fake.mjs";

const require = createRequire(import.meta.url);
let JSDOM;
try {
  ({ JSDOM } = require("jsdom"));
} catch {
  console.error("jsdom is not installed: run `npm i --no-save jsdom` in the repo root, or set NODE_PATH to a node_modules that has it.");
  process.exit(1);
}

const here = path.dirname(fileURLToPath(import.meta.url));
const html = fs.readFileSync(path.resolve(here, "../StudioHost/Plugins/UE_MCP_Studio/Resources/web/bootstrap.html"), "utf-8");

// Windows a test left open, closed after it so a failure cannot keep the run alive.
const opened = new Set();
afterEach(() => {
  for (const w of opened) w.close();
  opened.clear();
});

function open(scenario, { theme = "ue-mcp", page = "", plan = planFor(scenario) } = {}) {
  const dom = new JSDOM(stubbedPage(html, { scenario, plan, theme, page, fast: true }), { runScripts: "dangerously", url: "http://localhost/" });
  const w = dom.window;
  // jsdom has no layout; record scrolls so a test can see each page open at the top.
  w.__scrolls = [];
  w.scrollTo = (x, y) => w.__scrolls.push([x, y]);
  opened.add(w);
  const d = w.document;
  const $ = (sel) => d.querySelector(sel);
  const ui = {
    w, d, $,
    kicker: () => $(".kicker")?.textContent,
    text: () => $("#step").textContent,
    calls: (name) => w.__studioCalls.filter((c) => c.name === name),
    next: () => $("#next").click(),
    back: () => $("#back").click(),
    box: (sel) => {
      const el = $(sel);
      assert.ok(el, `no ${sel} on ${ui.kicker()}`);
      return el;
    },
    set(sel, on) {
      const el = ui.box(sel);
      if (el.checked !== on) el.click();
    },
    checked: (sel) => ui.box(sel).checked,
    /** The feedback mode radios: [value, checked] in page order. */
    modes: () => [...d.querySelectorAll('input[name="feedback-mode"]')].map((r) => [r.value, r.checked]),
    async until(fn, what, ms = 3000) {
      const end = Date.now() + ms;
      for (;;) {
        const v = fn();
        if (v) return v;
        if (Date.now() > end) throw new Error(`timed out waiting for ${what}; card: ${ui.text()}`);
        await new Promise((r) => setTimeout(r, 10));
      }
    },
    /** The strip under Bridge: [index of the current segment, segment count]. */
    segments() {
      const segs = [...d.querySelectorAll("#steps li:nth-child(2) .pages i")];
      return [segs.findIndex((s) => s.className === "now"), segs.length];
    },
    sent() {
      const runs = ui.calls("runsetup");
      assert.equal(runs.length, 1, "runsetup was called once");
      return JSON.parse(runs[0].arg);
    },
    // Let the page's pending calls settle, then stop its timers.
    async close() {
      await new Promise((r) => setTimeout(r, 50));
      w.close();
      opened.delete(w);
    },
  };
  return ui;
}

async function ready(ui) {
  await ui.until(() => ui.$("#next") && !ui.$("#next").disabled, "the setup pages");
}

const NEW_PAGES = ["Project", "Tools", "Native tools", "Context", "MCP clients", "Agent", "Review"];

test("new project: every page in order, init's defaults, the strip, and the JSON Install sends", async () => {
  const ui = open("install-new");
  await ready(ui);
  assert.equal(ui.$("#toggle"), null, "no Options toggle");
  assert.equal(ui.d.querySelectorAll("#steps > li").length, 3, "the strip keeps three steps");

  for (let i = 0; i < NEW_PAGES.length; i++) {
    assert.equal(ui.kicker(), `${NEW_PAGES[i]}, ${i + 1} of 7`);
    assert.deepEqual(ui.segments(), [i, 7]);
    assert.equal(ui.$("#back") === null, i === 0, "Back on every page but the first");
    assert.equal(ui.$("#next").textContent, i === NEW_PAGES.length - 1 ? "Install" : "Next");
    const page = NEW_PAGES[i];
    if (page === "Project") {
      assert.match(ui.text(), /Preview/);
      assert.match(ui.text(), /Bridge port51734/);
      assert.match(ui.text(), /Derived from the project path/);
    }
    if (page === "Tools") {
      const boxes = [...ui.d.querySelectorAll("[data-cat]")];
      assert.equal(boxes.length, 15);
      assert.ok(boxes.every((b) => b.checked), "every category on");
    }
    if (page === "Native tools") {
      assert.ok(ui.checked("#native-on"));
      assert.equal(ui.d.querySelectorAll("[data-native]:checked").length, 9);
    }
    if (page === "Context") assert.ok(ui.checked('input[value="micro"]'));
    if (page === "MCP clients") {
      assert.ok(ui.checked('[data-client="claude-code"]'));
      assert.ok(!ui.checked('[data-client="codex"]'), "a global client is never chosen for the user");
      assert.ok(!ui.checked('[data-client="cursor"]'));
      assert.equal(ui.$('[data-client="claude-desktop"]'), null, "undetected clients are not offered");
    }
    if (page === "Agent") {
      assert.ok(ui.checked("#feedback"), "feedback is on for a new project");
      assert.equal(ui.$("#hook"), null, "the hook is not offered");
      assert.equal(ui.$("#skills"), null, "the skills are not offered");
      assert.ok(ui.$("#signin"), "sign-in offered");
      assert.deepEqual(ui.modes(), [["auto-approve", true], ["interactive", false], ["defer", false]], "no mode chosen yet, so the recommended one");
      assert.match(ui.text(), /AskEdit, approve or discard each report in the chat\./);
      assert.match(ui.text(), /File automatically \(recommended\)Reports go straight to GitHub\./);
      assert.match(ui.text(), /HoldReview later with npx ue-mcp feedback review\./);
      assert.doesNotMatch(ui.text(), /Current/, "a new project marks no mode as current");
    }
    if (page === "Review") assert.match(ui.text(), /Feedback modeFile automaticallyReports go straight to GitHub\./);
    if (page !== "Review") ui.next();
  }

  ui.next();
  assert.deepEqual(ui.sent(), {
    clients: ["claude-code"], disable: [], nativeTools: true, nativeExclude: [], context: "micro", feedback: true, feedbackMode: "auto-approve", hook: false, skills: false,
  });
  await ui.until(() => /What setup did/.test(ui.text()), "the result steps");
  assert.match(ui.text(), /Installed prebuilt binaries/);
  assert.match(ui.text(), /ue-mcp.yml created/);
  assert.deepEqual(ui.segments(), [-1, 7], "every page shows done once Install runs");
  await ui.close();
});

test("re-run: each page starts from the project's choices", async () => {
  const ui = open("install-rerun");
  await ready(ui);
  assert.match(ui.text(), /already set up/);
  ui.next();
  assert.ok(!ui.checked('[data-cat="networking"]'));
  assert.ok(!ui.checked('[data-cat="demo"]'));
  assert.ok(ui.checked('[data-cat="pcg"]'));
  ui.next();
  assert.ok(!ui.checked('[data-native="pcg"]'));
  assert.ok(ui.checked('[data-native="gas"]'));
  ui.next();
  assert.ok(ui.checked('input[value="lean"]'));
  assert.match(ui.text(), /leanCurrent/);
  ui.next();
  ui.next();
  assert.ok(ui.checked("#feedback"));
  assert.match(ui.text(), /Signed in to GitHub as @octo/);
  assert.equal(ui.$("#hook"), null, "the hook is not offered; the project's choice is sent");
  assert.deepEqual(ui.modes(), [["auto-approve", false], ["interactive", false], ["defer", true]], "the project's mode is chosen");
  assert.match(ui.text(), /HoldCurrent/);
  ui.next();
  assert.match(ui.text(), /13 of 15 categories on/);
  assert.match(ui.text(), /Feedback modeHold/);
  ui.next();
  assert.deepEqual(ui.sent(), {
    clients: ["claude-code"], disable: ["demo", "networking"], nativeTools: true, nativeExclude: ["pcg"], context: "lean", feedback: true, feedbackMode: "defer", hook: true, skills: true,
  });
  await ui.close();
});

test("choices persist across Back and Next, Enter moves on, and Change returns to a page", async () => {
  const ui = open("install-new");
  await ready(ui);
  ui.next();
  ui.set('[data-cat="pcg"]', false);
  ui.set('[data-cat="niagara"]', false);
  ui.next();
  assert.equal(ui.$('[data-native="pcg"]'), null, "a category turned off is not offered native tools");
  ui.set('[data-native="animation"]', false);
  ui.back();
  assert.ok(!ui.checked('[data-cat="pcg"]'));
  assert.ok(!ui.checked('[data-cat="niagara"]'));
  ui.set('[data-cat="animation"]', false);
  ui.next();
  assert.equal(ui.kicker(), "Native tools, 3 of 7");
  // Enter on a checkbox presses Next.
  ui.$("#native-on").dispatchEvent(new ui.w.KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
  assert.equal(ui.kicker(), "Context, 4 of 7");
  ui.w.__scrolls.length = 0;
  ui.next();
  ui.back();
  assert.deepEqual(ui.w.__scrolls, [[0, 0], [0, 0]], "Next and Back open the page at the top");
  ui.$('input[value="full"]').click();
  ui.next();
  ui.back();
  assert.ok(ui.checked('input[value="full"]'));
  ui.next();
  ui.set('[data-client="claude-code"]', false);
  ui.set('[data-client="cursor"]', true);
  ui.next();
  assert.equal(ui.$("#hook"), null, "no hook without Claude Code");
  assert.equal(ui.$("#skills"), null, "no skills without Claude Code");
  ui.set("#feedback", false);
  assert.equal(ui.$("#signin"), null, "no sign-in with feedback off");
  assert.deepEqual(ui.modes(), [], "no modes with feedback off");
  ui.back();
  assert.ok(!ui.checked('[data-client="claude-code"]'));
  assert.ok(ui.checked('[data-client="cursor"]'));
  ui.next();
  assert.ok(!ui.checked("#feedback"));
  ui.next();
  assert.match(ui.text(), /12 of 15 categories on/);
  assert.doesNotMatch(ui.text(), /Feedback mode/);
  ui.$('[data-goto="context"]').click();
  assert.equal(ui.kicker(), "Context, 4 of 7");
  for (let i = 0; i < 3; i++) ui.next();
  assert.equal(ui.kicker(), "Review, 7 of 7");
  ui.d.body.dispatchEvent(new ui.w.KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
  assert.deepEqual(ui.sent(), {
    clients: ["cursor"], disable: ["pcg", "niagara", "animation"], nativeTools: true, nativeExclude: [], context: "full", feedback: false, hook: false, skills: false,
  });
  await ui.close();
});

test("the top Next moves on like the bottom one, and reads Install on the last page", async () => {
  const ui = open("install-new");
  await ready(ui);
  for (let i = 1; i < 7; i++) {
    assert.equal(ui.$("#next-top").textContent, "Next");
    ui.$("#next-top").click();
    assert.equal(ui.kicker(), `${NEW_PAGES[i]}, ${i + 1} of 7`);
  }
  assert.equal(ui.$("#next-top").textContent, "Install");
  ui.$("#next-top").click();
  assert.ok(ui.sent(), "Install sent the choices");
  await ui.close();
});

test("UE 5.7: no native tools page, and the project's native settings are sent unchanged", async () => {
  const ui = open("install-57");
  await ready(ui);
  assert.equal(ui.kicker(), "Project, 1 of 6");
  ui.next();
  ui.next();
  assert.equal(ui.kicker(), "Context, 3 of 6");
  for (let i = 0; i < 4; i++) ui.next();
  assert.deepEqual(ui.sent(), {
    clients: ["claude-code"], disable: [], nativeTools: true, nativeExclude: [], context: "micro", feedback: true, feedbackMode: "auto-approve", hook: false, skills: false,
  });
  await ui.close();
});

test("an older ue-mcp plan: no Tools page, no native category list, no port, and the sign-in command", async () => {
  const ui = open("install-legacy");
  await ready(ui);
  assert.equal(ui.kicker(), "Project, 1 of 6");
  assert.doesNotMatch(ui.text(), /Bridge port/);
  ui.next();
  assert.equal(ui.kicker(), "Native tools, 2 of 6");
  assert.equal(ui.$("[data-native]"), null);
  ui.set("#native-on", false);
  ui.next();
  ui.next();
  ui.next();
  assert.match(ui.text(), /npx ue-mcp auth/);
  assert.equal(ui.$("#signin"), null);
  assert.deepEqual(ui.modes(), [], "no mode radios without a mode in the plan");
  ui.next();
  assert.doesNotMatch(ui.text(), /categories on/);
  assert.doesNotMatch(ui.text(), /Feedback mode/);
  ui.next();
  assert.deepEqual(ui.sent(), {
    clients: ["claude-code"], disable: [], nativeTools: false, nativeExclude: [], context: "micro", feedback: true, hook: false, skills: false,
  });
  await ui.close();
});

test("re-run with each feedback mode: the project's mode is chosen, shown and sent, and a change is sent", async () => {
  const labels = { interactive: "Ask", "auto-approve": "File automatically", defer: "Hold" };
  for (const mode of Object.keys(labels)) {
    const plan = cannedPlan({ isReInit: true, feedbackMode: { mode, scope: "project", set: null } });
    const ui = open("install-rerun", { page: "agent", plan });
    await ready(ui);
    assert.equal(ui.kicker(), "Agent, 6 of 7");
    assert.deepEqual(ui.modes().filter(([, on]) => on).map(([v]) => v), [mode]);
    assert.match(ui.text(), new RegExp(labels[mode] + (mode === "auto-approve" ? " \\(recommended\\)" : "") + "Current"));
    ui.next();
    assert.match(ui.text(), new RegExp("Feedback mode" + labels[mode]));
    ui.next();
    assert.equal(ui.sent().feedbackMode, mode);
    await ui.close();
  }

  const ui = open("install-rerun", { page: "agent" });
  await ready(ui);
  ui.$('input[name="feedback-mode"][value="auto-approve"]').click();
  // Turning feedback off and on again keeps the mode chosen.
  ui.set("#feedback", false);
  ui.set("#feedback", true);
  assert.deepEqual(ui.modes(), [["auto-approve", true], ["interactive", false], ["defer", false]]);
  ui.next();
  assert.match(ui.text(), /Feedback modeFile automaticallyReports go straight to GitHub\./);
  ui.next();
  assert.deepEqual(ui.sent(), {
    clients: ["claude-code"], disable: ["demo", "networking"], nativeTools: true, nativeExclude: ["pcg"], context: "lean", feedback: true, feedbackMode: "auto-approve", hook: true, skills: true,
  });
  await ui.close();
});

test("a plan whose feedback mode the page does not know starts at the recommended mode", async () => {
  const ui = open("install-new", { page: "agent", plan: cannedPlan({ feedbackMode: { scope: "default" } }) });
  await ready(ui);
  assert.deepEqual(ui.modes(), [["auto-approve", true], ["interactive", false], ["defer", false]]);
  ui.next();
  ui.next();
  assert.equal(ui.sent().feedbackMode, "auto-approve");
  await ui.close();
});

test("feedback turned off on a re-run sends no mode", async () => {
  const ui = open("install-rerun", { page: "agent" });
  await ready(ui);
  ui.set("#feedback", false);
  ui.next();
  ui.next();
  assert.deepEqual(ui.sent(), {
    clients: ["claude-code"], disable: ["demo", "networking"], nativeTools: true, nativeExclude: ["pcg"], context: "lean", feedback: false, hook: false, skills: true,
  });
  await ui.close();
});

test("GitHub sign-in: the code, the link, then the account", async () => {
  const ui = open("install-new", { page: "agent" });
  await ready(ui);
  assert.equal(ui.kicker(), "Agent, 6 of 7");
  ui.$("#signin").click();
  await ui.until(() => /WDJB-MJHT/.test(ui.text()), "the device code");
  assert.match(ui.text(), /https:\/\/github.com\/login\/device/);
  ui.$("#opengithub").click();
  assert.equal(ui.calls("opengithubsignin").length, 1);
  assert.equal(ui.calls("startfeedbackauth").length, 1);
  await ui.until(() => /Signed in to GitHub as @octo/.test(ui.text()), "the signed-in account", 6000);
  ui.next();
  assert.match(ui.text(), /As @octo on GitHub/);
  await ui.close();
});

test("a failed init is shown at once, with its steps, and Back returns to the review", async () => {
  const ui = open("install-fails", { page: "review" });
  await ready(ui);
  ui.next();
  await ui.until(() => /The setup did not finish/.test(ui.text()), "the failure");
  assert.match(ui.text(), /engine version of Preview is unknown/);
  assert.match(ui.text(), /What setup did/);
  ui.$("#back").click();
  assert.equal(ui.kicker(), "Review, 7 of 7");
  await ui.close();
});

test("no plan after 90 seconds: Install with init's defaults", async () => {
  const ui = open("install-noplan");
  await ui.until(() => /Reading this project's setup/.test(ui.text()), "the loading page");
  const now = ui.w.Date.now.bind(ui.w.Date);
  ui.w.Date.now = () => now() + 91 * 1000;
  await ui.until(() => ui.$("#install"), "the default install", 3000);
  ui.$("#install").click();
  assert.equal(ui.calls("installbridge").length, 1);
  assert.equal(ui.calls("runsetup").length, 0);
  await ui.close();
});

test("themes: Studio's theme is applied, with the editor's colors for the editor themes", async () => {
  const plain = open("install-new");
  await ready(plain);
  assert.equal(plain.d.documentElement.getAttribute("data-theme"), null);
  await plain.close();
  for (const theme of ["editor", "editor-orange"]) {
    const ui = open("install-new", { theme });
    await ready(ui);
    await ui.until(() => ui.d.documentElement.style.getPropertyValue("--ue-panel"), "the editor colors");
    assert.equal(ui.d.documentElement.getAttribute("data-theme"), theme);
    assert.equal(ui.d.documentElement.style.getPropertyValue("--ue-panel"), "#242424");
    await ui.close();
  }
});

test("the restart screens list what setup did", async () => {
  const ui = open("restart-binary");
  await ui.until(() => /Restart to load the bridge/.test(ui.text()), "the restart screen");
  await ui.until(() => /What setup did/.test(ui.text()), "the result steps");
  await ui.close();
});
