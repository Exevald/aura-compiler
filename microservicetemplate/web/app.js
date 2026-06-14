const TASK_API_BASE = document.body.dataset.taskApiBase || window.__TASK_API_BASE__ || "http://127.0.0.1:8082";
const AI_API_BASE = document.body.dataset.aiApiBase || window.__AI_API_BASE__ || window.location.origin;
const STAGES = ["todo", "in_progress", "resolved"];

const state = {
  token: localStorage.getItem("taskTrackerToken") || "",
  tasks: [],
  editingId: null,
  highlightedIds: new Set(),
};

const els = {
  pipeline: document.getElementById("pipeline"),
  countTodo: document.getElementById("countTodo"),
  countInProgress: document.getElementById("countInProgress"),
  countResolved: document.getElementById("countResolved"),
  countTotal: document.getElementById("countTotal"),
  tokenInput: document.getElementById("tokenInput"),
  authState: document.getElementById("authState"),
  titleInput: document.getElementById("titleInput"),
  descriptionInput: document.getElementById("descriptionInput"),
  statusInput: document.getElementById("statusInput"),
  submitTaskBtn: document.getElementById("submitTaskBtn"),
  resetFormBtn: document.getElementById("resetFormBtn"),
  formTitle: document.getElementById("formTitle"),
  logArea: document.getElementById("logArea"),
  statusText: document.getElementById("statusText"),
  summaryResult: document.getElementById("summaryResult"),
  highlightResult: document.getElementById("highlightResult"),
};

function setStatus(text) {
  els.statusText.textContent = text;
}

function log(message) {
  els.logArea.textContent = message;
}

function authHeaders() {
  return state.token ? { Authorization: "Bearer " + state.token } : {};
}

async function api(base, path, options = {}) {
  const response = await fetch(base + path, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...authHeaders(),
      ...(options.headers || {}),
    },
  });
  const text = await response.text();
  let body = text;
  try {
    body = text ? JSON.parse(text) : null;
  } catch (_) {}
  if (!response.ok) {
    const message = body && body.error ? body.error : text || response.statusText;
    throw new Error(message);
  }
  return body;
}

function syncAuthState() {
  els.tokenInput.value = state.token;
  els.authState.textContent = state.token ? "Token loaded" : "No token";
}

function statusLabel(status) {
  return status || "todo";
}

function sanitizePromptValue(value) {
  return String(value).replaceAll("|", "/").replaceAll("\n", " ").replaceAll("\r", " ");
}

function buildTaskLines() {
  return state.tasks.map((task) => [
    "TASK",
    task.id,
    task.status || "todo",
    sanitizePromptValue(task.title || ""),
    sanitizePromptValue(task.description || ""),
  ].join("|"));
}

function buildHighlightPrompt() {
  return [
    "You are helping prioritize tasks.",
    "Return at most three lines.",
    "Each line must use the format id|reason.",
    "Prefer unresolved tasks with the highest impact or blocking risk.",
    "",
    ...buildTaskLines(),
  ].join("\n");
}

function buildSummaryPrompt() {
  return [
    "You are summarizing a task board.",
    "Write one short paragraph with the current state and next actions.",
    "",
    ...buildTaskLines(),
  ].join("\n");
}

function parseHighlightResponse(responseText) {
  const lines = String(responseText || "")
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean);
  const ids = [];
  const rendered = [];
  for (const line of lines) {
    const separator = line.indexOf("|");
    if (separator <= 0) {
      continue;
    }
    const id = Number(line.slice(0, separator));
    const reason = line.slice(separator + 1).trim();
    if (Number.isFinite(id)) {
      ids.push(id);
      rendered.push("#" + id + " - " + reason);
    }
  }
  return { ids, lines: rendered };
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function resetForm() {
  state.editingId = null;
  els.formTitle.textContent = "Create task";
  els.submitTaskBtn.textContent = "Create";
  els.titleInput.value = "";
  els.descriptionInput.value = "";
  els.statusInput.value = "todo";
}

function stageTasks(status) {
  return state.tasks.filter((task) => (task.status || "todo") === status);
}

function stageCount(status) {
  return stageTasks(status).length;
}

function taskClass(task) {
  const classes = ["task-card", task.status === "resolved" ? "resolved" : ""];
  if (state.highlightedIds.has(task.id)) {
    classes.push("highlighted");
  }
  return classes.filter(Boolean).join(" ");
}

function taskActions(task) {
  const actions = [
    `<button data-action="edit" data-id="${task.id}">Edit</button>`,
    `<button data-action="delete" data-id="${task.id}" class="danger">Delete</button>`,
  ];
  if (task.status !== "resolved") {
    actions.unshift(`<button data-action="resolve" data-id="${task.id}" class="good">Resolve</button>`);
  }
  return actions.join("");
}

function renderPipeline() {
  els.countTodo.textContent = String(stageCount("todo"));
  els.countInProgress.textContent = String(stageCount("in_progress"));
  els.countResolved.textContent = String(stageCount("resolved"));
  els.countTotal.textContent = String(state.tasks.length);

  els.pipeline.innerHTML = STAGES.map((status) => {
    const tasks = stageTasks(status);
    const empty = tasks.length === 0 ? '<div class="muted">No cards in this lane.</div>' : "";
    return `
      <section class="stage">
        <div class="stage-head">
          <h3 class="stage-title">
            <span>${statusLabel(status)}</span>
            <small>${tasks.length}</small>
          </h3>
        </div>
        <div class="stage-body">
          ${empty}
          ${tasks.map((task) => `
            <article class="${taskClass(task)}">
              <div class="task-title">${escapeHtml(task.title)}</div>
              <div class="task-desc">${escapeHtml(task.description)}</div>
              <div class="task-meta">
                <span class="pill ${task.status}">${statusLabel(task.status)}</span>
                <span class="pill">#${task.id}</span>
              </div>
              <div class="task-actions">
                ${taskActions(task)}
              </div>
            </article>
          `).join("")}
        </div>
      </section>
    `;
  }).join("");
}

function render() {
  renderPipeline();
}

async function refreshTasks() {
  setStatus("Loading tasks...");
  try {
    const data = await api(TASK_API_BASE, "/api/v1/tasks", { method: "GET" });
    state.tasks = Array.isArray(data) ? data : [];
    render();
    log("Loaded " + state.tasks.length + " tasks.");
    setStatus("Ready");
  } catch (error) {
    log("Failed to load tasks: " + error.message);
    setStatus("Error");
  }
}

async function saveTask() {
  const payload = {
    title: els.titleInput.value.trim(),
    description: els.descriptionInput.value.trim(),
    status: els.statusInput.value,
  };
  if (!payload.title) {
    log("Title is required.");
    return;
  }
  setStatus("Saving task...");
  try {
    if (state.editingId) {
      await api(TASK_API_BASE, "/api/v1/tasks/" + state.editingId, {
        method: "PUT",
        body: JSON.stringify(payload),
      });
      log("Task updated.");
    } else {
      await api(TASK_API_BASE, "/api/v1/tasks", {
        method: "POST",
        body: JSON.stringify(payload),
      });
      log("Task created.");
    }
    resetForm();
    await refreshTasks();
  } catch (error) {
    log("Task save failed: " + error.message);
    setStatus("Error");
  }
}

async function deleteTask(id) {
  if (!confirm("Delete task #" + id + "?")) {
    return;
  }
  setStatus("Deleting task...");
  try {
    await api(TASK_API_BASE, "/api/v1/tasks/" + id, { method: "DELETE" });
    log("Task deleted.");
    await refreshTasks();
  } catch (error) {
    log("Delete failed: " + error.message);
    setStatus("Error");
  }
}

async function resolveTask(id) {
  const task = state.tasks.find((item) => item.id === id);
  if (!task) {
    return;
  }
  setStatus("Resolving task...");
  try {
    await api(TASK_API_BASE, "/api/v1/tasks/" + id, {
      method: "PUT",
      body: JSON.stringify({
        title: task.title,
        description: task.description,
        status: "resolved",
      }),
    });
    log("Task resolved.");
    await refreshTasks();
  } catch (error) {
    log("Resolve failed: " + error.message);
    setStatus("Error");
  }
}

async function editTask(id) {
  const task = state.tasks.find((item) => item.id === id);
  if (!task) {
    return;
  }
  state.editingId = id;
  els.formTitle.textContent = "Edit task #" + id;
  els.submitTaskBtn.textContent = "Save";
  els.titleInput.value = task.title;
  els.descriptionInput.value = task.description;
  els.statusInput.value = task.status;
  log("Editing task #" + id + ".");
}

async function useDemoToken() {
  try {
    const data = await api(TASK_API_BASE, "/api/dev/token", { method: "POST", body: "{}" });
    state.token = data.token || "";
    localStorage.setItem("taskTrackerToken", state.token);
    syncAuthState();
    log("Demo token loaded.");
    await refreshTasks();
  } catch (error) {
    log("Demo token request failed: " + error.message);
  }
}

async function highlightTasks() {
  setStatus("Asking AI to highlight...");
  try {
    const data = await api(AI_API_BASE, "/api/highlight", {
      method: "POST",
      body: JSON.stringify({
        prompt: buildHighlightPrompt(),
        temperature: 0.2,
      }),
    });
    const parsed = parseHighlightResponse(data.response || data.raw || "");
    state.highlightedIds = new Set(parsed.ids);
    render();
    els.highlightResult.textContent = parsed.lines.join("\n") || "No highlights returned.";
    log("AI highlight ready.");
    setStatus("Ready");
  } catch (error) {
    log("AI highlight failed: " + error.message);
    setStatus("Error");
  }
}

async function summarizeTasks() {
  setStatus("Asking AI to summarize...");
  try {
    const data = await api(AI_API_BASE, "/api/summary", {
      method: "POST",
      body: JSON.stringify({
        prompt: buildSummaryPrompt(),
        temperature: 0.2,
      }),
    });
    els.summaryResult.textContent = data.response || data.summary || data.raw || "No summary returned.";
    log("AI summary ready.");
    setStatus("Ready");
  } catch (error) {
    log("AI summary failed: " + error.message);
    setStatus("Error");
  }
}

function loadInitialToken() {
  syncAuthState();
  if (!state.token) {
    els.tokenInput.value = "";
    return;
  }
  refreshTasks();
}

document.getElementById("refreshBtn").addEventListener("click", refreshTasks);
document.getElementById("demoTokenBtn").addEventListener("click", useDemoToken);
document.getElementById("saveTokenBtn").addEventListener("click", () => {
  state.token = els.tokenInput.value.trim();
  localStorage.setItem("taskTrackerToken", state.token);
  syncAuthState();
  refreshTasks();
});
document.getElementById("clearTokenBtn").addEventListener("click", () => {
  state.token = "";
  state.highlightedIds = new Set();
  localStorage.removeItem("taskTrackerToken");
  syncAuthState();
  render();
  log("Token cleared.");
});
document.getElementById("submitTaskBtn").addEventListener("click", saveTask);
document.getElementById("resetFormBtn").addEventListener("click", resetForm);
document.getElementById("highlightBtn").addEventListener("click", highlightTasks);
document.getElementById("summaryBtn").addEventListener("click", summarizeTasks);

els.pipeline.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-action]");
  if (!button) {
    return;
  }
  const id = Number(button.dataset.id);
  const action = button.dataset.action;
  if (action === "edit") {
    editTask(id);
  }
  if (action === "resolve") {
    resolveTask(id);
  }
  if (action === "delete") {
    deleteTask(id);
  }
});

syncAuthState();
resetForm();
refreshTasks();
