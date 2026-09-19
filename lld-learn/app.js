(() => {
  const DATA = window.LLD_DATA;
  if (!DATA) {
    document.getElementById("main").innerHTML =
      "<p class='loading'>Missing data/problems.js — run _generate_data.py first.</p>";
    return;
  }

  const state = {
    lang: localStorage.getItem("lld-lang") || "java",
    theme: localStorage.getItem("lld-theme") || "light",
    codeCache: new Map(),
    drafts: new Map(),
    originals: new Map(),
    paths: new Map(),
    cm: null,
    activeProblem: null,
    running: false,
  };

  // Public Piston (emkc.org) began requiring an API key in Feb 2026 and
  // now returns HTTP 401. JavaScript runs in a Worker; other languages
  // use Wandbox's public compiler API (CORS-enabled, no key).
  const WANDBOX_LANG = {
    java: "Java",
    javascript: "JavaScript",
    python: "Python",
    cpp: "C++",
    golang: "Go",
  };

  let wandboxList = null;

  const CM_MODE = {
    java: "text/x-java",
    javascript: "javascript",
    python: "python",
    cpp: "text/x-c++src",
    golang: "go",
  };

  const els = {
    main: document.getElementById("main"),
    sidebarNav: document.getElementById("sidebarNav"),
    toc: document.getElementById("toc"),
    tocNav: document.getElementById("tocNav"),
    themeToggle: document.getElementById("themeToggle"),
    sidebarToggle: document.getElementById("sidebarToggle"),
    backdrop: document.getElementById("backdrop"),
    problemCount: document.getElementById("problemCount"),
    crumb: document.getElementById("crumb"),
  };

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function difficultyChip(d) {
    return `<span class="chip chip-${escapeHtml(d)}">${escapeHtml(d)}</span>`;
  }

  function priorityChip(p) {
    return `<span class="chip ${p === "High" ? "chip-high" : ""}">${escapeHtml(p)} Priority</span>`;
  }

  function applyTheme() {
    document.documentElement.dataset.theme = state.theme === "dark" ? "dark" : "light";
    localStorage.setItem("lld-theme", state.theme);
  }

  function setCrumb(text) {
    if (els.crumb) els.crumb.textContent = text;
  }

  function draftKey(slug, lang) {
    return `${slug}::${lang}`;
  }

  function destroyEditor() {
    state.cm = null;
  }

  function saveCurrentDraft() {
    if (!state.cm || !state.activeProblem) return;
    state.drafts.set(
      draftKey(state.activeProblem.slug, state.lang),
      state.cm.getValue()
    );
  }

  function closeSidebar() {
    document.body.classList.remove("sidebar-open");
    els.backdrop.hidden = true;
  }

  function openSidebar() {
    document.body.classList.add("sidebar-open");
    els.backdrop.hidden = false;
  }

  function getProblem(slug) {
    return DATA.problems.find((p) => p.slug === slug);
  }

  function orderedProblems() {
    const order = DATA.categories.map((c) => c.id);
    return [...DATA.problems].sort(
      (a, b) => order.indexOf(a.category) - order.indexOf(b.category)
    );
  }

  function classifyEntity(name) {
    const n = name.toLowerCase();
    if (/\benum\b/.test(n) || /status|type|level|direction|priority|size\b|color|symbol/.test(n)) {
      return "Enum";
    }
    if (/strategy|interface|channel|appender|formatter|observer/.test(n)) {
      return "Interface";
    }
    if (/manager|service|system|game|board|dispatcher|scheduler|broker|cache|facade/.test(n)) {
      return "Core";
    }
    return "Entity";
  }

  function kindAccent(kind) {
    if (kind === "Enum") return "#9a6412";
    if (kind === "Interface") return "#2f5bff";
    if (kind === "Core") return "#1f7a4d";
    return "#5b657a";
  }

  function parseEntity(raw) {
    const parts = raw.split(/\s*[—–-]+\s*|:\s+/);
    const name = (parts[0] || raw).replace(/\(.*?\)/g, "").trim();
    const blurb = parts.slice(1).join(" — ").trim();
    return { name, blurb, kind: classifyEntity(raw) };
  }

  function bucketClasses(classes) {
    const buckets = { Enum: [], Interface: [], Core: [], Entity: [] };
    for (const c of classes || []) {
      const kind = classifyEntity(c);
      (buckets[kind] || buckets.Entity).push(c);
    }
    return buckets;
  }

  function renderEntityMap(problem) {
    const nodes = (problem.entities || []).map(parseEntity);
    if (!nodes.length && problem.classes?.length) {
      for (const c of problem.classes.slice(0, 12)) {
        nodes.push({ name: c, blurb: "", kind: classifyEntity(c) });
      }
    }
    if (!nodes.length) {
      return `<p class="note">No entities parsed — open the source header for details.</p>`;
    }
    return `
      <div class="entity-map">
        ${nodes
          .map(
            (n) => `
          <div class="entity-node" style="--node-accent:${kindAccent(n.kind)}">
            <div class="kind">${escapeHtml(n.kind)}</div>
            <div class="name">${escapeHtml(n.name)}</div>
            ${n.blurb ? `<div class="blurb">${escapeHtml(n.blurb)}</div>` : ""}
          </div>`
          )
          .join("")}
      </div>`;
  }

  function umlMembers(kind) {
    if (kind === "Enum") return ["+ values", "+ from(...)?"];
    if (kind === "Interface") return ["+ contract()", "<<pluggable>>"];
    if (kind === "Core") return ["- collaborators", "+ orchestrate()"];
    return ["- fields", "+ behavior()"];
  }

  function renderClassDiagram(problem) {
    const buckets = bucketClasses(problem.classes || []);
    const lanes = [
      { key: "Enum", label: "Enums" },
      { key: "Interface", label: "Interfaces" },
      { key: "Entity", label: "Domain" },
      { key: "Core", label: "Orchestrators" },
    ].filter((l) => buckets[l.key].length);

    if (!lanes.length) {
      return `<p class="note">Class list unavailable for this problem.</p>`;
    }

    return `
      <div class="uml-canvas">
        ${lanes
          .map((lane) => {
            const items = buckets[lane.key].slice(0, 6);
            return `
              <div class="uml-lane">
                <div class="uml-lane-label">${escapeHtml(lane.label)}</div>
                ${items
                  .map((name) => {
                    const members = umlMembers(lane.key);
                    return `
                      <div class="uml-box">
                        <div class="uml-title">
                          <span class="uml-stereo">&lt;&lt;${escapeHtml(
                            lane.key.toLowerCase()
                          )}&gt;&gt;</span>
                          ${escapeHtml(name)}
                        </div>
                        <div class="uml-body">
                          ${members.map((m) => `<div>${escapeHtml(m)}</div>`).join("")}
                        </div>
                      </div>`;
                  })
                  .join("")}
              </div>`;
          })
          .join("")}
      </div>
      <div class="relation-legend">
        <span><i>Core</i> orchestrates Domain</span>
        <span><i>Interface</i> plugged into Core</span>
        <span><i>Enum</i> types Domain fields</span>
      </div>`;
  }

  function renderDiagrams(problem) {
    return `
      <div class="diagram-stack">
        <div class="diagram-card">
          <div class="diagram-card-head">
            <h3>Entity map</h3>
            <span>Nouns → responsibilities</span>
          </div>
          <div class="diagram-body">${renderEntityMap(problem)}</div>
        </div>
        <div class="diagram-card">
          <div class="diagram-card-head">
            <h3>Class diagram</h3>
            <span>Grouped by role</span>
          </div>
          <div class="diagram-body">${renderClassDiagram(problem)}</div>
        </div>
      </div>`;
  }

  function setOutput(text, status) {
    const box = document.getElementById("editorOutput");
    const pre = document.getElementById("editorOutputBody");
    const label = document.getElementById("editorOutputLabel");
    if (!box || !pre) return;
    box.classList.remove("is-error", "is-running");
    if (status === "error") box.classList.add("is-error");
    if (status === "running") box.classList.add("is-running");
    if (label) {
      label.textContent =
        status === "running" ? "Terminal · Running" : status === "error" ? "Terminal · Error" : "Terminal";
    }
    pre.textContent = text || "(no output)";
  }

  function toRunnableJs(code) {
    return code
      .replace(
        /if\s*\(\s*require\.main\s*===\s*module\s*\)\s*\{[\s\S]*?\}/g,
        "await main();"
      )
      .replace(/^[ \t]*main\s*\(\s*\)\s*;[ \t]*\r?$/gm, "await main();");
  }

  function runJavaScriptLocal(code) {
    return new Promise((resolve, reject) => {
      const workerSrc = `
        self.onmessage = async (event) => {
          const logs = [];
          const format = (value) => {
            if (typeof value === "string") return value;
            if (value instanceof Error) return value.stack || value.message;
            try { return JSON.stringify(value); } catch (_) { return String(value); }
          };
          const capture = (...args) => logs.push(args.map(format).join(" "));
          console.log = capture;
          console.info = capture;
          console.debug = capture;
          console.warn = capture;
          console.error = capture;

          const module = { exports: {} };
          const exports = module.exports;
          function require(id) {
            throw new Error("Cannot require('" + id + "') in the in-browser runner");
          }
          require.main = module;
          const process = { env: {}, argv: ["node", "main.js"], cwd: () => "/", exit() {} };

          try {
            const run = new Function(
              "console",
              "module",
              "exports",
              "require",
              "process",
              "return (async () => {\\n" + event.data + "\\n})();"
            );
            await run(console, module, exports, require, process);
            self.postMessage({ ok: true, output: logs.join("\\n") });
          } catch (err) {
            if (logs.length) logs.push("");
            logs.push(String(err && err.stack ? err.stack : err));
            self.postMessage({ ok: false, output: logs.join("\\n") });
          }
        };
      `;
      const blob = new Blob([workerSrc], { type: "text/javascript" });
      const url = URL.createObjectURL(blob);
      const worker = new Worker(url);
      const timer = setTimeout(() => {
        worker.terminate();
        URL.revokeObjectURL(url);
        reject(new Error("Timed out after 15s"));
      }, 15000);
      const finish = (fn) => (arg) => {
        clearTimeout(timer);
        worker.terminate();
        URL.revokeObjectURL(url);
        fn(arg);
      };
      worker.onmessage = finish((event) => resolve(event.data));
      worker.onerror = finish((event) =>
        reject(new Error(event.message || "In-browser runner failed"))
      );
      worker.postMessage(toRunnableJs(code));
    });
  }

  function toWandboxJava(code) {
    // Wandbox compiles the snippet as prog.java, so a public top-level
    // type whose name is not "prog" will not compile.
    return code
      .replace(/^public\s+abstract\s+class\s+/gm, "abstract class ")
      .replace(/^public\s+final\s+class\s+/gm, "final class ")
      .replace(/^public\s+class\s+/gm, "class ")
      .replace(/^public\s+enum\s+/gm, "enum ")
      .replace(/^public\s+interface\s+/gm, "interface ");
  }

  async function resolveWandboxCompiler(lang) {
    const language = WANDBOX_LANG[lang];
    if (!language) return null;
    if (!wandboxList) {
      const res = await fetch("https://wandbox.org/api/list.json");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      wandboxList = await res.json();
    }
    const matches = wandboxList.filter((c) => c.language === language);
    const pick = (pred) => matches.find(pred);
    if (lang === "javascript") {
      return pick((c) => c.name.startsWith("nodejs-") && !c.name.includes("head")) || matches[0];
    }
    if (lang === "python") {
      return (
        pick((c) => /^cpython-3\.\d/.test(c.name) && !c.name.includes("head")) ||
        pick((c) => c.name.startsWith("cpython-3")) ||
        matches[0]
      );
    }
    if (lang === "java") {
      return pick((c) => c.name.includes("openjdk") && !c.name.includes("head")) || matches[0];
    }
    if (lang === "cpp") {
      return (
        pick(
          (c) =>
            c.name.startsWith("gcc-") &&
            !/-c$|-pp$/.test(c.name) &&
            !c.name.includes("head")
        ) || matches[0]
      );
    }
    if (lang === "golang") {
      return pick((c) => c.name.startsWith("go-") && !c.name.includes("head")) || matches[0];
    }
    return matches[0];
  }

  async function runWandbox(lang, code) {
    const compiler = await resolveWandboxCompiler(lang);
    if (!compiler) throw new Error("Run is not configured for this language.");
    const body = {
      compiler: compiler.name,
      code: lang === "java" ? toWandboxJava(code) : code,
      save: false,
    };
    if (lang === "cpp") body["compiler-option-raw"] = "-std=c++17";
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 45000);
    try {
      const res = await fetch("https://wandbox.org/api/compile.json", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
        signal: controller.signal,
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return await res.json();
    } finally {
      clearTimeout(timer);
    }
  }

  async function runCode() {
    if (!state.cm || state.running) return;
    const code = state.cm.getValue();
    if (!WANDBOX_LANG[state.lang]) {
      setOutput("Run is not configured for this language.", "error");
      return;
    }

    state.running = true;
    const runBtn = document.getElementById("btnRun");
    if (runBtn) runBtn.disabled = true;

    try {
      if (state.lang === "javascript") {
        setOutput("Running JavaScript in the browser…", "running");
        const result = await runJavaScriptLocal(code);
        setOutput(
          (result.output || "").trim() || "(ran successfully with empty stdout)",
          result.ok ? "ok" : "error"
        );
        return;
      }

      setOutput("Connecting to Wandbox runner…", "running");
      const data = await runWandbox(state.lang, code);
      const parts = [];
      if (data.compiler_error) parts.push(`Compile stderr:\n${data.compiler_error}`);
      if (data.program_output) parts.push(data.program_output);
      if (data.program_error) parts.push(data.program_error);
      if (data.signal) parts.push(`(killed: ${data.signal})`);
      if (data.status != null && String(data.status) !== "0" && !data.program_error) {
        parts.push(`Exit code: ${data.status}`);
      }
      const out = parts.join("\n").trim();
      const failed =
        (data.status != null && String(data.status) !== "0") || Boolean(data.signal);
      setOutput(out || "(ran successfully with empty stdout)", failed ? "error" : "ok");
    } catch (err) {
      const aborted = err && err.name === "AbortError";
      setOutput(
        `${
          aborted ? "Runner timed out." : "Could not reach the online runner."
        }\n${err.message || err}`,
        "error"
      );
    } finally {
      state.running = false;
      if (runBtn) runBtn.disabled = false;
    }
  }

  async function copyCode() {
    if (!state.cm) return;
    try {
      await navigator.clipboard.writeText(state.cm.getValue());
      const btn = document.getElementById("btnCopy");
      if (btn) {
        const prev = btn.textContent;
        btn.textContent = "Copied";
        setTimeout(() => {
          btn.textContent = prev;
        }, 1100);
      }
    } catch (_) {
      setOutput("Clipboard permission denied.", "error");
    }
  }

  function resetCode() {
    if (!state.cm || !state.activeProblem) return;
    const key = draftKey(state.activeProblem.slug, state.lang);
    const original = state.originals.get(key);
    if (original == null) return;
    state.cm.setValue(original);
    state.drafts.set(key, original);
    setOutput("Restored original file from the repo.", "ok");
  }

  async function loadCodeForLang(problem, lang) {
    const path = problem.codeFiles?.[lang];
    if (!path) {
      return { path: null, code: `// No ${lang} source mapped for this problem.` };
    }
    if (state.codeCache.has(path)) {
      return { path, code: state.codeCache.get(path) };
    }
    try {
      const res = await fetch(path);
      if (!res.ok) throw new Error(String(res.status));
      const code = await res.text();
      state.codeCache.set(path, code);
      return { path, code };
    } catch (err) {
      return {
        path,
        code: `// Could not load ${path}\n// Serve from the repo root.\n// ${err.message || err}`,
      };
    }
  }

  function updateStatusBar(path) {
    const fileEl = document.getElementById("statusFile");
    const langEl = document.getElementById("statusLang");
    const lang = DATA.languages.find((l) => l.id === state.lang);
    if (fileEl) fileEl.textContent = (path || "").split("/").pop() || "untitled";
    if (langEl) langEl.textContent = lang?.label || state.lang;
    const crumbPath = document.getElementById("editorCrumb");
    if (crumbPath) crumbPath.textContent = path || "Interview_problems";
  }

  async function fillEditor(problem, lang) {
    const key = draftKey(problem.slug, lang);
    let code = state.drafts.get(key);
    let path = state.paths.get(key);

    if (code == null) {
      const loaded = await loadCodeForLang(problem, lang);
      path = loaded.path;
      code = loaded.code;
      state.originals.set(key, code);
      state.drafts.set(key, code);
      state.paths.set(key, path);
    }

    updateStatusBar(path);
    document.querySelectorAll(".vscode-tab").forEach((tab) => {
      tab.classList.toggle("is-active", tab.dataset.lang === lang);
    });

    const host = document.getElementById("editorHost");
    if (!host || !window.CodeMirror) return;

    if (state.cm) {
      state.cm.setOption("mode", CM_MODE[lang] || "text/plain");
      state.cm.setValue(code);
      state.cm.refresh();
      return;
    }

    host.value = code;
    state.cm = CodeMirror.fromTextArea(host, {
      lineNumbers: true,
      mode: CM_MODE[lang] || "text/plain",
      theme: "vscode",
      indentUnit: 4,
      tabSize: 4,
      lineWrapping: false,
      viewportMargin: Infinity,
    });
    requestAnimationFrame(() => state.cm && state.cm.refresh());
  }

  function wireChatReplay() {
    const thread = document.getElementById("chatThread");
    const btn = document.getElementById("chatReplay");
    if (!thread) return;

    const play = () => {
      thread.classList.remove("is-playing");
      // force reflow so animation restarts
      void thread.offsetWidth;
      thread.classList.add("is-playing");
    };

    play();
    btn?.addEventListener("click", play);
  }

  function wireEditorChrome(problem) {
    document.querySelectorAll(".vscode-tab").forEach((tab) => {
      tab.addEventListener("click", async () => {
        const lang = tab.dataset.lang;
        if (!lang || lang === state.lang) return;
        saveCurrentDraft();
        state.lang = lang;
        localStorage.setItem("lld-lang", lang);
        await fillEditor(problem, lang);
        setOutput(`Switched to ${DATA.languages.find((l) => l.id === lang)?.label || lang}.`, "ok");
      });
    });
    document.getElementById("btnRun")?.addEventListener("click", () => runCode());
    document.getElementById("btnCopy")?.addEventListener("click", () => copyCode());
    document.getElementById("btnReset")?.addEventListener("click", () => resetCode());
  }

  function renderSidebar(activeSlug) {
    const byCat = Object.fromEntries(DATA.categories.map((c) => [c.id, []]));
    for (const p of DATA.problems) {
      if (byCat[p.category]) byCat[p.category].push(p);
    }

    els.problemCount.textContent = String(DATA.problems.length);
    els.sidebarNav.innerHTML = DATA.categories
      .map((cat) => {
        const items = byCat[cat.id] || [];
        if (!items.length) return "";
        return `
          <div class="nav-group">
            <p class="nav-group-title">${escapeHtml(cat.label)}</p>
            ${items
              .map(
                (p) => `
              <button type="button" class="nav-item ${
                p.slug === activeSlug ? "is-active" : ""
              }" data-slug="${escapeHtml(p.slug)}">
                ${escapeHtml(p.title.replace(/^Design\s+/, ""))}
              </button>`
              )
              .join("")}
          </div>`;
      })
      .join("");

    els.sidebarNav.querySelectorAll("[data-slug]").forEach((btn) => {
      btn.addEventListener("click", () => {
        location.hash = `#/problem/${btn.dataset.slug}`;
        closeSidebar();
      });
    });
  }

  function buildToc(sections) {
    els.toc.hidden = false;
    els.tocNav.innerHTML = sections
      .map((s) => `<a href="#${s.id}">${escapeHtml(s.label)}</a>`)
      .join("");
  }

  function guideTip(who, text) {
    const isAlex = who === "alex";
    return `
      <aside class="guide-tip ${isAlex ? "is-alex" : "is-jordan"}">
        <img src="assets/avatar-${isAlex ? "candidate" : "interviewer"}.png" alt="" width="36" height="36" />
        <div>
          <strong>${isAlex ? "Alex" : "Jordan"}</strong>
          <p>${escapeHtml(text)}</p>
        </div>
      </aside>`;
  }

  function renderHome() {
    els.toc.hidden = true;
    setCrumb("Overview");
    const first = orderedProblems()[0];
    els.main.innerHTML = `
      <div class="main-inner is-wide">
        <section class="home-hero">
          <div class="home-hero-copy">
            <p class="eyebrow">Weekday LLD Studio</p>
            <h1>Weekday LLD</h1>
            <p>
              Interview-grade low-level design with Alex &amp; Jordan as your guides —
              requirements, diagrams, and a runnable multi-language workspace.
            </p>
            <div class="home-cta">
              <a class="btn btn-primary" href="#/problem/${escapeHtml(first.slug)}">
                Begin with ${escapeHtml(first.title.replace(/^Design\s+/, ""))}
              </a>
            </div>
          </div>
          <div class="home-cast">
            <div class="home-speech">Ready when you are — pick a problem and let’s design it together.</div>
            <div class="home-cast-card is-alex">
              <img src="assets/avatar-candidate.png" alt="Alex" width="92" height="92" />
              <span>Alex</span>
              <em>Candidate</em>
            </div>
            <div class="home-cast-card is-jordan">
              <img src="assets/avatar-interviewer.png" alt="Jordan" width="92" height="92" />
              <span>Jordan</span>
              <em>Interviewer</em>
            </div>
          </div>
        </section>
        <p class="section-label">Problems</p>
        <div class="card-grid">
          ${orderedProblems()
            .map(
              (p, i) => `
            <a class="problem-card" href="#/problem/${escapeHtml(p.slug)}" style="animation-delay:${
              i * 0.03
            }s">
              <div class="meta-row">
                ${priorityChip(p.priority)}
                ${difficultyChip(p.difficulty)}
                <span class="chip">${p.readMins} min</span>
              </div>
              <h3>${escapeHtml(p.title)}</h3>
              <p>${escapeHtml(p.intro)}</p>
            </a>`
            )
            .join("")}
        </div>
      </div>`;
  }

  async function renderProblem(slug) {
    destroyEditor();
    const problem = getProblem(slug);
    if (!problem) {
      els.main.innerHTML = `<p class="loading">Problem not found.</p>`;
      return;
    }
    state.activeProblem = problem;
    setCrumb(problem.title);

    const sections = [
      { id: "intro", label: "Overview + Clarify" },
      { id: "requirements", label: "Requirements" },
      { id: "entities", label: "Entities" },
      { id: "diagrams", label: "Diagrams" },
      { id: "design", label: "Design" },
      { id: "implementation", label: "Implementation" },
    ];
    buildToc(sections);

    const list = orderedProblems();
    const idx = list.findIndex((p) => p.slug === slug);
    const prev = idx > 0 ? list[idx - 1] : null;
    const next = idx < list.length - 1 ? list[idx + 1] : null;

    const entityRows = (problem.entities || [])
      .map((e) => {
        const parsed = parseEntity(e);
        return `<tr>
          <td><code>${escapeHtml(parsed.name)}</code></td>
          <td>${escapeHtml(parsed.kind)}</td>
          <td>${escapeHtml(parsed.blurb || "See design notes")}</td>
        </tr>`;
      })
      .join("");

    const clarifyingHtml = problem.clarifying?.length
      ? `<section class="clarify-panel" id="clarify">
          <div class="section-head clarify-head">
            <div>
              <h2>Clarifying Requirements</h2>
              <p class="section-lead">Scope the problem before drawing boxes.</p>
            </div>
            ${guideTip("jordan", "Good clarifying questions save you from over-designing.")}
          </div>
          <div class="chat-scene">
            <div class="chat-cast" aria-hidden="true">
              <div class="cast-person cast-candidate">
                <img src="assets/avatar-candidate.png" alt="" width="96" height="96" />
                <span>Alex</span>
                <em>Candidate</em>
              </div>
              <div class="cast-vs">
                <span class="cast-pulse"></span>
                <strong>Live clarifying</strong>
              </div>
              <div class="cast-person cast-interviewer">
                <img src="assets/avatar-interviewer.png" alt="" width="96" height="96" />
                <span>Jordan</span>
                <em>Interviewer</em>
              </div>
            </div>
            <div class="chat-thread" id="chatThread">
              ${problem.clarifying
                .map(
                  ([q, a], i) => `
                <div class="chat-row is-candidate" style="--i:${i * 2}">
                  <img class="chat-avatar" src="assets/avatar-candidate.png" alt="" width="40" height="40" />
                  <div class="chat-bubble">
                    <div class="chat-name">Alex · Candidate</div>
                    <p>${escapeHtml(q)}</p>
                  </div>
                </div>
                <div class="chat-row is-interviewer" style="--i:${i * 2 + 1}">
                  <div class="chat-bubble">
                    <div class="chat-name">Jordan · Interviewer</div>
                    <p>${escapeHtml(a)}</p>
                  </div>
                  <img class="chat-avatar" src="assets/avatar-interviewer.png" alt="" width="40" height="40" />
                </div>`
                )
                .join("")}
            </div>
            <button type="button" class="chat-replay" id="chatReplay">Replay discussion</button>
          </div>
        </section>`
      : "";

    const step = (n) => (problem.clarifying?.length ? n + 1 : n);

    const langTabs = DATA.languages
      .map(
        (l) => `
        <button type="button" class="vscode-tab ${
          l.id === state.lang ? "is-active" : ""
        }" data-lang="${escapeHtml(l.id)}">${escapeHtml(l.label)}</button>`
      )
      .join("");

    els.main.innerHTML = `
      <div class="main-inner is-wide">
        <article class="lesson">
          <div class="screen-open" id="intro">
            <header class="lesson-header">
              <div class="lesson-header-copy">
                <div class="meta-row">
                  ${priorityChip(problem.priority)}
                  ${difficultyChip(problem.difficulty)}
                  <span class="chip">${problem.readMins} min read</span>
                </div>
                <h1>${escapeHtml(problem.title)}</h1>
                <p class="lede">${escapeHtml(problem.intro)}</p>
              </div>
              <div class="lesson-header-cast" aria-hidden="true">
                <img src="assets/avatar-candidate.png" alt="" />
                <img src="assets/avatar-interviewer.png" alt="" />
              </div>
            </header>
            ${clarifyingHtml}
          </div>

          <section class="section" id="requirements">
            <div class="section-head">
              <div>
                <h2>${step(1)}. Requirements</h2>
                <p class="section-lead">Lock functional needs and quality bars before naming classes.</p>
              </div>
              ${guideTip("alex", "I write FR as verbs (“detect winner”) and NFR as constraints (“O(1) get”).")}
            </div>
            <div class="req-grid">
              <div class="req-panel is-func">
                <h3>Functional</h3>
                <ul>
                  ${(problem.functional || [])
                    .map((x) => `<li>${escapeHtml(x)}</li>`)
                    .join("") || "<li class='note'>See source header.</li>"}
                </ul>
              </div>
              <div class="req-panel is-nfr">
                <h3>Non-functional</h3>
                <ul>
                  ${(problem.nonFunctional || [])
                    .map((x) => `<li>${escapeHtml(x)}</li>`)
                    .join("") || "<li class='note'>See source header / design notes.</li>"}
                </ul>
              </div>
            </div>
          </section>

          <section class="section" id="entities">
            <div class="section-head">
              <div>
                <h2>${step(2)}. Core Entities</h2>
                <p class="section-lead">Extract nouns with state or behavior — not every noun becomes a class.</p>
              </div>
              ${guideTip("jordan", "Circle the nouns, then ask: does this own data or just describe it?")}
            </div>
            ${
              entityRows
                ? `<table class="entity-table">
                    <thead><tr><th>Entity</th><th>Kind</th><th>Responsibility</th></tr></thead>
                    <tbody>${entityRows}</tbody>
                  </table>`
                : "<p class='note'>Entity list derived from the source header.</p>"
            }
            ${
              problem.classes?.length
                ? `<div class="class-pills">${problem.classes
                    .map((c) => `<span class="class-pill">${escapeHtml(c)}</span>`)
                    .join("")}</div>`
                : ""
            }
          </section>

          <section class="section" id="diagrams">
            <div class="section-head">
              <div>
                <h2>${step(3)}. Diagrams</h2>
                <p class="section-lead">Structural view before you touch the editor.</p>
              </div>
              ${guideTip("alex", "If I can’t sketch this in 60 seconds, my entities are still fuzzy.")}
            </div>
            ${renderDiagrams(problem)}
          </section>

          <section class="section" id="design">
            <div class="section-head">
              <div>
                <h2>${step(4)}. Design Notes</h2>
                <p class="section-lead">Patterns and trade-offs that shaped this solution.</p>
              </div>
              ${guideTip("jordan", "Call out the pattern by name — interviewers love that signal.")}
            </div>
            <div class="design-layout">
              ${
                problem.designNotes
                  ? `<div class="callout"><strong>Decision log</strong><p style="margin:0.45rem 0 0;color:var(--ink-2)">${escapeHtml(
                      problem.designNotes
                    )}</p></div>`
                  : `<p class="note">Patterns live in the class structure below.</p>`
              }
              <aside class="coach-card">
                <div class="coach-people">
                  <img src="assets/avatar-candidate.png" alt="" />
                  <img src="assets/avatar-interviewer.png" alt="" />
                </div>
                <p>
                  Alex builds the happy path. Jordan stress-tests extensibility —
                  “What happens when we add one more strategy?”
                </p>
              </aside>
            </div>
          </section>

          <section class="section" id="implementation">
            <div class="section-head">
              <div>
                <h2>${step(5)}. Implementation</h2>
                <p class="section-lead">Edit, switch language, run, or copy — same design across five runtimes.</p>
              </div>
              ${guideTip("alex", "I tweak one method, hit Run, then switch language to check the mirror.")}
            </div>
            <div class="vscode" id="codeEditor">
              <div class="vscode-titlebar">
                <div class="vscode-traffic" aria-hidden="true">
                  <span class="r"></span><span class="y"></span><span class="g"></span>
                </div>
                <div class="vscode-title">Weekday LLD — Editor</div>
                <div class="vscode-actions">
                  <button type="button" class="vscode-btn" id="btnReset" title="Reset">Reset</button>
                  <button type="button" class="vscode-btn" id="btnCopy" title="Copy">Copy</button>
                  <button type="button" class="vscode-btn run" id="btnRun" title="Run">Run</button>
                </div>
              </div>
              <div class="vscode-tabs">${langTabs}</div>
              <div class="vscode-breadcrumb">
                <span>Interview_problems</span>
                <span>›</span>
                <strong id="editorCrumb">…</strong>
              </div>
              <div class="vscode-body">
                <textarea id="editorHost"></textarea>
              </div>
              <div class="vscode-terminal" id="editorOutput">
                <div class="vscode-terminal-head">
                  <span id="editorOutputLabel">Terminal</span>
                </div>
                <pre id="editorOutputBody">Ready. Press Run to execute.</pre>
              </div>
              <div class="vscode-statusbar">
                <span id="statusFile">—</span>
                <span id="statusLang">Java</span>
              </div>
            </div>
            <p class="editor-hint">JavaScript runs in the browser. Other languages need the network. Large Java demos may take a few seconds to compile.</p>
          </section>

          <nav class="lesson-nav">
            ${
              prev
                ? `<a href="#/problem/${prev.slug}"><span>Previous</span>${escapeHtml(
                    prev.title
                  )}</a>`
                : "<span></span>"
            }
            ${
              next
                ? `<a href="#/problem/${next.slug}"><span>Next</span>${escapeHtml(
                    next.title
                  )}</a>`
                : ""
            }
          </nav>
        </article>
      </div>`;

    wireEditorChrome(problem);
    await fillEditor(problem, state.lang);
    wireChatReplay();

    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (!entry.isIntersecting) return;
          els.tocNav.querySelectorAll("a").forEach((a) => {
            a.classList.toggle(
              "is-active",
              a.getAttribute("href") === `#${entry.target.id}`
            );
          });
        });
      },
      { rootMargin: "-20% 0px -65% 0px", threshold: 0.01 }
    );
    sections.forEach((s) => {
      const node = document.getElementById(s.id);
      if (node) observer.observe(node);
    });
  }

  function route() {
    const hash = location.hash || "#/";
    const problemMatch = hash.match(/^#\/problem\/([\w-]+)/);

    if (!problemMatch) {
      destroyEditor();
      state.activeProblem = null;
    }

    if (problemMatch) {
      renderSidebar(problemMatch[1]);
      renderProblem(problemMatch[1]);
      return;
    }

    renderSidebar(null);
    renderHome();
  }

  els.themeToggle.addEventListener("click", () => {
    state.theme = state.theme === "dark" ? "light" : "dark";
    applyTheme();
  });
  els.sidebarToggle.addEventListener("click", () => {
    if (document.body.classList.contains("sidebar-open")) closeSidebar();
    else openSidebar();
  });
  els.backdrop.addEventListener("click", closeSidebar);
  window.addEventListener("hashchange", route);

  applyTheme();
  route();
})();
