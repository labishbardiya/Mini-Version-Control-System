// app.js - Pure client-side visualizer for cs repositories.
// The user selects the .cs directory via the browser.

const state = {
  files: [],          // all File objects from the chosen directory
  commits: [],        // parsed commit objects, newest first
  blobs: {},          // hash -> File
  index: null,        // parsed index map
  head: null          // head commit id
};

function setText(id, text) {
  const el = document.getElementById(id);
  if (el) el.textContent = text;
}

function appendStep(text) {
  const list = document.getElementById("algorithm-steps");
  if (!list) return;
  const li = document.createElement("li");
  li.textContent = text;
  list.appendChild(li);
}

function clearSteps() {
  const list = document.getElementById("algorithm-steps");
  if (list) list.innerHTML = "";
}

// ------------------------------
// Loading .cs directory
// ------------------------------

async function readFileAsText(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result));
    reader.onerror = () => reject(reader.error);
    reader.readAsText(file);
  });
}

function relativePathOf(file) {
  // Some browsers may not populate webkitRelativePath in all cases.
  // Fall back to simple name so single-file selection still works.
  return file.webkitRelativePath && file.webkitRelativePath.length
    ? file.webkitRelativePath
    : file.name;
}

function findFileByPath(relPath) {
  return state.files.find(f => relativePathOf(f).endsWith(relPath));
}

async function loadHead() {
  const f = findFileByPath(".cs/HEAD");
  if (!f) {
    state.head = null;
    return;
  }
  const text = (await readFileAsText(f)).trim();
  state.head = text || null;
}

async function loadIndex() {
  const f = findFileByPath(".cs/index");
  if (!f) {
    state.index = {};
    return;
  }
  const text = await readFileAsText(f);
  const lines = text.split("\n");
  const map = {};
  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    const parts = trimmed.split(/\s+/);
    if (parts.length >= 2) {
      const path = parts[0];
      const hash = parts[1];
      map[path] = hash;
    }
  }
  state.index = map;
}

function loadBlobs() {
  state.blobs = {};
  for (const f of state.files) {
    const rel = relativePathOf(f);
    const prefix = ".cs/objects/blobs/";
    if (rel.includes(prefix)) {
      const hash = rel.substring(rel.indexOf(prefix) + prefix.length);
      state.blobs[hash] = f;
    }
  }
}

async function parseCommitFile(file, id) {
  const text = await readFileAsText(file);
  const lines = text.split("\n");
  let parent = null;
  let ts = 0;
  let message = "";
  const files = {};
  let mode = "header";
  for (const raw of lines) {
    const line = raw.trimEnd();
    if (!line) continue;
    if (mode === "header") {
      if (line.startsWith("parent ")) {
        const val = line.slice(7).trim();
        parent = (val === "null" ? null : val);
      } else if (line.startsWith("timestamp ")) {
        ts = Number(line.slice(10).trim());
      } else if (line.startsWith("message ")) {
        message = line.slice(8);
      } else if (line === "files") {
        mode = "files";
      }
    } else {
      const parts = line.split(/\s+/);
      if (parts.length >= 2) {
        const path = parts[0];
        const hash = parts[1];
        files[path] = hash;
      }
    }
  }
  return { id, parent, timestamp: ts, message, files };
}

async function loadCommits() {
  const commits = [];
  // map hash -> file
  const commitFiles = {};
  for (const f of state.files) {
    const rel = relativePathOf(f);
    const prefix = ".cs/objects/commits/";
    if (rel.includes(prefix)) {
      const id = rel.substring(rel.indexOf(prefix) + prefix.length);
      commitFiles[id] = f;
    }
  }
  if (!state.head) {
    state.commits = [];
    return;
  }

  // follow HEAD backwards
  let current = state.head;
  const seen = new Set();
  while (current && !seen.has(current)) {
    seen.add(current);
    const cf = commitFiles[current];
    if (!cf) {
      // broken reference
      commits.push({ id: current, parent: null, timestamp: 0, message: "[missing commit]", files: {}, broken: true });
      break;
    }
    const parsed = await parseCommitFile(cf, current);
    commits.push(parsed);
    current = parsed.parent;
  }
  state.commits = commits; // newest first
}

async function handleDirectoryChosen(fileList) {
  state.files = Array.from(fileList);
  clearSteps();
  appendStep("Loaded directory with " + state.files.length + " files.");

  await loadHead();
  await loadIndex();
  loadBlobs();
  await loadCommits();

  setText("repo-status", state.head ? ("HEAD at " + state.head.slice(0, 8) + "...") : "No commits yet");
  updateCommitGraph();
  updateTimeline();
  updateFilesystemView();
  runIntegrityCheck();

  document.getElementById("learning-text").textContent =
    "Click on a commit node to see how the file map and blobs form a snapshot. " +
    "Click on a file path to trace its history along the linear commit chain.";
}

// ------------------------------
// Commit graph visualization
// ------------------------------

function updateCommitGraph(activeId) {
  const container = document.getElementById("commit-list");
  container.innerHTML = "";
  if (!state.commits.length) {
    container.textContent = "No commits.";
    return;
  }
  for (const c of state.commits) {
    const div = document.createElement("div");
    div.className = "commit-node";
    if (c.broken) div.classList.add("broken");
    if (activeId && c.id === activeId) div.classList.add("active");

    const idSpan = document.createElement("div");
    idSpan.className = "commit-id";
    idSpan.textContent = c.id.slice(0, 10) + "...";

    const msgDiv = document.createElement("div");
    msgDiv.className = "commit-message";
    msgDiv.textContent = c.message || "(no message)";

    const tsDiv = document.createElement("div");
    tsDiv.textContent = c.timestamp ? new Date(c.timestamp * 1000).toLocaleString() : "";
    tsDiv.style.fontSize = "11px";
    tsDiv.style.color = "#9ca3af";

    div.appendChild(idSpan);
    div.appendChild(msgDiv);
    div.appendChild(tsDiv);

    div.addEventListener("click", () => {
      updateCommitGraph(c.id);
      showCommitInFilesystem(c);
      clearSteps();
      appendStep("Selected commit " + c.id.slice(0, 10) + " for inspection.");
      appendStep("This node stores: parent id, timestamp, message, and a file map (path -> blob hash).");
    });

    container.appendChild(div);
  }
}

// ------------------------------
// Timeline
// ------------------------------

function updateTimeline() {
  const slider = document.getElementById("time-slider");
  const label = document.getElementById("time-label");
  if (!state.commits.length) {
    slider.min = 0;
    slider.max = 0;
    slider.value = 0;
    label.textContent = "No commits.";
    return;
  }
  // commits are newest first; timeline 0 = oldest
  slider.min = 0;
  slider.max = state.commits.length - 1;
  slider.value = state.commits.length - 1;
  label.textContent = "Showing HEAD commit.";

  slider.oninput = () => {
    const idxFromOldest = Number(slider.value);
    const c = state.commits[state.commits.length - 1 - idxFromOldest];
    if (!c) return;
    updateCommitGraph(c.id);
    showCommitInFilesystem(c);
    label.textContent = "Time at commit " + c.id.slice(0, 10) + " (" +
      new Date(c.timestamp * 1000).toLocaleString() + ")";
    clearSteps();
    appendStep("Moved timeline slider to commit " + c.id.slice(0, 10) + ".");
    appendStep("This models walking the linear history backward from HEAD.");
  };
}

// ------------------------------
// Filesystem view
// ------------------------------

function renderFileMapList(ul, map) {
  ul.innerHTML = "";
  const paths = Object.keys(map).sort();
  for (const p of paths) {
    const li = document.createElement("li");
    const spanPath = document.createElement("span");
    spanPath.className = "path";
    spanPath.textContent = p;
    const spanHash = document.createElement("span");
    spanHash.className = "file-hash";
    spanHash.textContent = map[p];

    spanPath.addEventListener("click", () => {
      showFileTrace(p);
    });

    li.appendChild(spanPath);
    li.appendChild(spanHash);
    ul.appendChild(li);
  }
}

function updateFilesystemView() {
  const committedUl = document.getElementById("fs-committed");
  const stagingUl = document.getElementById("fs-staging");
  const workingUl = document.getElementById("fs-working");

  // committed = HEAD commit file map
  if (state.commits.length && state.head) {
    const headCommit = state.commits.find(c => c.id === state.head) || state.commits[0];
    renderFileMapList(committedUl, headCommit.files);
  } else {
    committedUl.innerHTML = "<li>(no committed files)</li>";
  }

  // staging = index
  if (state.index && Object.keys(state.index).length) {
    renderFileMapList(stagingUl, state.index);
  } else {
    stagingUl.innerHTML = "<li>(index empty)</li>";
  }

  // working directory: we approximate as "current index view plus committed"
  const working = {};
  if (state.commits.length && state.head) {
    const headCommit = state.commits.find(c => c.id === state.head) || state.commits[0];
    for (const p of Object.keys(headCommit.files)) {
      working[p] = headCommit.files[p];
    }
  }
  if (state.index) {
    for (const p of Object.keys(state.index)) {
      working[p] = state.index[p];
    }
  }
  if (Object.keys(working).length) {
    renderFileMapList(workingUl, working);
  } else {
    workingUl.innerHTML = "<li>(no tracked files)</li>";
  }
}

function showCommitInFilesystem(commit) {
  const committedUl = document.getElementById("fs-committed");
  renderFileMapList(committedUl, commit.files);
}

// ------------------------------
// File trace
// ------------------------------

function showFileTrace(path) {
  clearSteps();
  appendStep("Tracing file " + path + " through the commit history.");
  const events = [];
  let lastHash = null;
  // walk oldest to newest
  for (let i = state.commits.length - 1; i >= 0; i--) {
    const c = state.commits[i];
    const hash = c.files[path];
    if (!hash) continue;
    if (!lastHash || lastHash !== hash) {
      events.push({ commit: c, hash });
      lastHash = hash;
    }
  }
  if (!events.length) {
    appendStep("This file never existed in any commit.");
    return;
  }
  const learning = document.getElementById("learning-text");
  learning.textContent =
    "Creation commit and change commits for " + path +
    " are highlighted below in order. Each step corresponds to a different blob hash.";

  events.forEach(e => {
    appendStep("Commit " + e.commit.id.slice(0, 10) + " stores blob " + e.hash.slice(0, 8) + " for this file.");
  });
}

// ------------------------------
// Integrity visualization
// ------------------------------

function runIntegrityCheck() {
  const div = document.getElementById("integrity-status");
  if (!state.commits.length || !state.head) {
    div.textContent = "No commits to check.";
    div.className = "";
    return;
  }
  let ok = true;
  let message = "All reachable commits and referenced blobs were found in the loaded .cs directory.";
  for (const c of state.commits) {
    if (c.broken) {
      ok = false;
      message = "Broken commit reference detected while walking from HEAD.";
      break;
    }
    for (const hash of Object.values(c.files)) {
      if (!state.blobs[hash]) {
        ok = false;
        message = "Missing blob " + hash.slice(0, 8) + " referenced by commit " + c.id.slice(0, 10) + ".";
        break;
      }
    }
    if (!ok) break;
  }
  div.textContent = message;
  div.className = ok ? "integrity-ok" : "integrity-error";
}

// ------------------------------
// Initialization
// ------------------------------

window.addEventListener("DOMContentLoaded", () => {
  const input = document.getElementById("dir-input");
  input.addEventListener("change", async (e) => {
    const files = e.target.files;
    if (!files || !files.length) return;
    await handleDirectoryChosen(files);
  });
});


