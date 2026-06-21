const TASK_API_BASE = document.body.dataset.taskApiBase || window.__TASK_API_BASE__ || "http://127.0.0.1:8082";
const AI_API_BASE = document.body.dataset.aiApiBase || window.__AI_API_BASE__ || window.location.origin;
const STAGES = ["todo", "in_progress", "resolved"];

const state = {
  token: localStorage.getItem("taskTrackerToken") || "",
  tasks: [],
  editingId: null,
  highlightedIds: new Set(),
  filters: {
    query: "",
    priority: "",
    tag: "",
    showArchived: true,
  },
};

const els = {
  pipeline: document.getElementById("pipeline"),
  countTodo: document.getElementById("countTodo"),
  countInProgress: document.getElementById("countInProgress"),
  countResolved: document.getElementById("countResolved"),
  countTotal: document.getElementById("countTotal"),
  countArchived: document.getElementById("countArchived"),
  tokenInput: document.getElementById("tokenInput"),
  authState: document.getElementById("authState"),
  searchInput: document.getElementById("searchInput"),
  priorityFilterInput: document.getElementById("priorityFilterInput"),
  tagFilterInput: document.getElementById("tagFilterInput"),
  showArchivedInput: document.getElementById("showArchivedInput"),
  clearFiltersBtn: document.getElementById("clearFiltersBtn"),
  titleInput: document.getElementById("titleInput"),
  descriptionInput: document.getElementById("descriptionInput"),
  statusInput: document.getElementById("statusInput"),
  priorityInput: document.getElementById("priorityInput"),
  dueDateInput: document.getElementById("dueDateInput"),
  tagsInput: document.getElementById("tagsInput"),
  checklistInput: document.getElementById("checklistInput"),
  archivedInput: document.getElementById("archivedInput"),
  submitTaskBtn: document.getElementById("submitTaskBtn"),
  resetFormBtn: document.getElementById("resetFormBtn"),
  formTitle: document.getElementById("formTitle"),
  logArea: document.getElementById("logArea"),
  statusText: document.getElementById("statusText"),
  summaryResult: document.getElementById("summaryResult"),
  highlightResult: document.getElementById("highlightResult"),
  archivePanel: document.getElementById("archivePanel"),
  archiveLane: document.getElementById("archiveLane"),
  archiveCount: document.getElementById("archiveCount"),
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

function syncFilterState() {
  els.searchInput.value = state.filters.query;
  els.priorityFilterInput.value = state.filters.priority;
  els.tagFilterInput.value = state.filters.tag;
  els.showArchivedInput.checked = state.filters.showArchived;
}

function statusLabel(status) {
  return status || "todo";
}

function priorityLabel(priority) {
  return priority || "normal";
}

function isArchived(task) {
  return task && (task.archived === true || task.archived === "true" || task.archived === 1 || task.archived === "1");
}

function splitList(value) {
  return String(value || "")
    .split(/[\n,]/)
    .map((item) => item.trim())
    .filter(Boolean);
}

function parseChecklist(value) {
  return splitList(value).map((item) => {
    const match = item.match(/^\[(x| )\]\s*(.*)$/i);
    if (match) {
      return { done: match[1].toLowerCase() === "x", text: match[2].trim() };
    }
    return { done: false, text: item };
  });
}

function formatTags(value) {
  return splitList(value);
}

function formatTaskDate(value) {
  return String(value || "").trim();
}

function matchesBoardFilters(task, includeArchived) {
  const query = state.filters.query.trim().toLowerCase();
  const priority = state.filters.priority.trim().toLowerCase();
  const tag = state.filters.tag.trim().toLowerCase();
  const archived = isArchived(task);
  if (!includeArchived && archived) {
    return false;
  }
  if (priority && priority !== (task.priority || "normal").toLowerCase()) {
    return false;
  }
  if (tag) {
    const tags = formatTags(task.tags || "").map((item) => item.toLowerCase());
    if (!tags.some((item) => item.includes(tag))) {
      return false;
    }
  }
  if (query) {
    const haystack = [
      task.title,
      task.description,
      task.tags,
      task.priority,
      task.due_date,
      task.checklist,
      task.status,
    ]
      .join(" ")
      .toLowerCase();
    if (!haystack.includes(query)) {
      return false;
    }
  }
  return true;
}

function sanitizePromptValue(value) {
  return String(value).replaceAll("|", "/").replaceAll("\n", " ").replaceAll("\r", " ");
}

function buildTaskLines() {
  return state.tasks.map((task) => [
    "TASK",
    task.id,
    task.status || "todo",
    priorityLabel(task.priority),
    formatTaskDate(task.due_date || ""),
    isArchived(task) ? "archived" : "active",
    sanitizePromptValue(task.title || ""),
    sanitizePromptValue(task.description || ""),
    sanitizePromptValue(task.tags || ""),
    sanitizePromptValue(task.checklist || ""),
  ].join("|"));
}

function buildHighlightPrompt() {
  return [
    "You are helping prioritize tasks.",
    "Return at most three lines.",
    "Each line must use the format id|reason.",
    "Prefer unresolved tasks with the highest impact, urgency, overdue due dates, or blocking risk.",
    "",
    ...buildTaskLines(),
  ].join("\n");
}

function buildSummaryPrompt() {
  return [
    "You are summarizing a task board.",
    "Write one short paragraph with the current state and next actions.",
    "Take priority, due date, tags, and archive state into account.",
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
  els.priorityInput.value = "normal";
  els.dueDateInput.value = "";
  els.tagsInput.value = "";
  els.checklistInput.value = "";
  els.archivedInput.checked = false;
}

function stageTasks(status) {
  return filteredTasks().filter((task) => (task.status || "todo") === status && !isArchived(task));
}

function stageCount(status) {
  return stageTasks(status).length;
}

function filteredTasks() {
  return state.tasks.filter((task) => matchesBoardFilters(task, state.filters.showArchived));
}

function overdue(task) {
  const due = formatTaskDate(task.due_date || "");
  if (!due || isArchived(task) || (task.status || "todo") === "resolved") {
    return false;
  }
  const dueDate = new Date(due + "T00:00:00");
  if (Number.isNaN(dueDate.getTime())) {
    return false;
  }
  const today = new Date();
  today.setHours(0, 0, 0, 0);
  return dueDate < today;
}

function taskClass(task) {
  const classes = ["task-card", task.status === "resolved" ? "resolved" : ""];
  if (state.highlightedIds.has(task.id)) {
    classes.push("highlighted");
  }
  if (isArchived(task)) {
    classes.push("archived");
  }
  if (overdue(task)) {
    classes.push("overdue");
  }
  return classes.filter(Boolean).join(" ");
}

function priorityClass(task) {
  return "priority-" + priorityLabel(task.priority);
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

function renderChecklist(task) {
  const items = parseChecklist(task.checklist || "");
  if (items.length === 0) {
    return "";
  }
  const doneCount = items.filter((item) => item.done).length;
  return `
    <div class="task-summary">
      <span class="pill">Checklist ${doneCount}/${items.length}</span>
    </div>
    <div class="checklist">
      ${items.map((item) => `
        <div class="checklist-item ${item.done ? "done" : ""}">
          <span>${item.done ? "☑" : "☐"}</span>
          <span>${escapeHtml(item.text)}</span>
        </div>
      `).join("")}
    </div>
  `;
}

function renderTaskCard(task) {
  const tags = formatTags(task.tags || "");
  const pieces = [];
  if (task.due_date) {
    pieces.push(`<span class="pill">${escapeHtml(task.due_date)}</span>`);
  }
  if (isArchived(task)) {
    pieces.push(`<span class="pill archived">archived</span>`);
  }
  if (overdue(task)) {
    pieces.push(`<span class="pill danger">overdue</span>`);
  }
  return `
    <article class="${taskClass(task)}">
      <div class="task-head">
        <div>
          <div class="task-title">${escapeHtml(task.title)}</div>
          <div class="task-desc">${escapeHtml(task.description)}</div>
        </div>
        <span class="pill ${priorityClass(task)}">${priorityLabel(task.priority)}</span>
      </div>
      <div class="task-meta">
        <span class="pill ${task.status}">${statusLabel(task.status)}</span>
        <span class="pill">#${task.id}</span>
        ${pieces.join("")}
      </div>
      ${tags.length ? `<div class="task-tags">${tags.map((tag) => `<span class="tag-chip">${escapeHtml(tag)}</span>`).join("")}</div>` : ""}
      ${renderChecklist(task)}
      <div class="task-actions">
        ${taskActions(task)}
      </div>
    </article>
  `;
}

function renderPipeline() {
  const tasks = filteredTasks();
  const archivedTasks = state.tasks.filter((task) => isArchived(task));
  els.countTodo.textContent = String(stageCount("todo"));
  els.countInProgress.textContent = String(stageCount("in_progress"));
  els.countResolved.textContent = String(stageCount("resolved"));
  els.countTotal.textContent = String(tasks.filter((task) => !isArchived(task)).length);
  els.countArchived.textContent = String(archivedTasks.length);

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
          ${tasks.map((task) => renderTaskCard(task)).join("")}
        </div>
      </section>
    `;
  }).join("");

  els.archiveCount.textContent = String(archivedTasks.length) + " tasks";
  const archivedVisible = archivedTasks.filter((task) => matchesBoardFilters(task, true));
  els.archiveLane.innerHTML = state.filters.showArchived
    ? (archivedVisible.length
      ? archivedVisible.map((task) => renderTaskCard(task)).join("")
      : '<div class="muted">No archived tasks match the current filters.</div>')
    : '<div class="muted">Archived tasks are hidden. Enable “Show archived” to inspect them.</div>';
}

function render() {
  renderPipeline();
}

function taskFormPayload() {
  return {
    title: els.titleInput.value.trim(),
    description: els.descriptionInput.value.trim(),
    status: els.statusInput.value,
    priority: els.priorityInput.value,
    due_date: formatTaskDate(els.dueDateInput.value),
    tags: formatTags(els.tagsInput.value).join(", "),
    checklist: splitList(els.checklistInput.value)
      .map((item) => item.replace(/^[-*]\s*/, ""))
      .join("\n"),
    archived: els.archivedInput.checked ? "true" : "false",
  };
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
  const payload = taskFormPayload();
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
        priority: task.priority || "normal",
        due_date: task.due_date || "",
        tags: task.tags || "",
        archived: isArchived(task) ? "true" : "false",
        checklist: task.checklist || "",
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
  els.priorityInput.value = task.priority || "normal";
  els.dueDateInput.value = formatTaskDate(task.due_date || "");
  els.tagsInput.value = task.tags || "";
  els.checklistInput.value = task.checklist || "";
  els.archivedInput.checked = isArchived(task);
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
  syncFilterState();
  if (!state.token) {
    els.tokenInput.value = "";
  }
  if (!state.filters.showArchived) {
    els.showArchivedInput.checked = false;
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
els.searchInput.addEventListener("input", () => {
  state.filters.query = els.searchInput.value;
  render();
});
els.priorityFilterInput.addEventListener("change", () => {
  state.filters.priority = els.priorityFilterInput.value;
  render();
});
els.tagFilterInput.addEventListener("input", () => {
  state.filters.tag = els.tagFilterInput.value;
  render();
});
els.showArchivedInput.addEventListener("change", () => {
  state.filters.showArchived = els.showArchivedInput.checked;
  render();
});
els.clearFiltersBtn.addEventListener("click", () => {
  state.filters = {
    query: "",
    priority: "",
    tag: "",
    showArchived: true,
  };
  syncFilterState();
  render();
  log("Filters cleared.");
});

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
syncFilterState();
resetForm();
refreshTasks();
