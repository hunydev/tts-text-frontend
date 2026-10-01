"use strict";
const $ = (id) => document.getElementById(id);
const PAGE = 100;
// Long digit strings read one by one: Jev cannot compare them digit by digit.
const WEAK = new Set(["business_number", "id_card_number", "generated_number", "personal_id_number", "finance_bank", "phone_number"]);
const state = { set: null, rows: [], outputs: {}, scores: {}, systems: [], page: 0, dirty: false, busy: false };
const number = (v) => Number(v).toLocaleString("ko-KR");
const percent = (v) => (v === null || v === undefined ? "—" : (v * 100).toFixed(1) + "%");
function node(tag, text) { const e = document.createElement(tag); if (text !== undefined) e.textContent = text; return e; }
function option(value, text) { const e = node("option", text); e.value = value; return e; }
function message(text, error = false) { $("notice").textContent = text; $("notice").classList.toggle("error", error); }
async function get(path, args = {}) { const r = await fetch(path + "?" + new URLSearchParams(args)); const d = await r.json(); if (!r.ok) throw Error(d.error || "요청 실패"); return d; }
async function post(path, body) { const r = await fetch(path, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) }); const d = await r.json(); if (!r.ok) throw Error(d.error || "요청 실패"); return d; }
function safely(fn) { return (...a) => Promise.resolve().then(() => fn(...a)).catch((e) => message(e.message, true)); }
function download(name, text, type) { const url = URL.createObjectURL(new Blob([text], { type })); const a = node("a"); a.href = url; a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(url), 1000); }
function lock(v) { state.busy = v; for (const id of ["fill", "save", "score", "score-changed", "import", "system", "set"]) $(id).disabled = v; document.querySelectorAll(".row-score").forEach((b) => (b.disabled = v)); }
function changedIds() { return state.rows.filter((r) => state.outputs[r.id] && !scoreOf(r)).map((r) => r.id); }
function systemName() { return ($("system-name").value || "").trim(); }
function filled() { return state.rows.filter((r) => state.outputs[r.id]).length; }
function scoreOf(r) { const s = state.scores[r.id]; return s && s.output === state.outputs[r.id] ? s : null; }
function markDirty() { state.dirty = true; $("fill-info").textContent = `결과 ${number(filled())} / ${number(state.rows.length)}문장 채움 · 저장하지 않은 변경 있음`; updateChanged(); }
function updateChanged() { const n = changedIds().length; $("score-changed").textContent = n ? `수정된 내용만 평가 (${number(n)})` : "수정된 내용만 평가"; }

function renderInfo() {
  const m = state.set.meta;
  $("set-info").textContent = `${m.source} · 대표 원문 ${number(state.rows.length)}문장 · 카테고리당 최대 ${m.per_category} · 입력 그룹·숫자 가린 틀마다 하나, 겹침이 적은 문장 우선 · 참조 정답 미사용`;
  $("fill-info").textContent = `결과 ${number(filled())} / ${number(state.rows.length)}문장 채움` + (state.dirty ? " · 저장하지 않은 변경 있음" : "");
  updateChanged();
}
function renderCompare() {
  const scored = state.systems.filter((s) => s.summary && s.summary.overall.scored);
  const cats = [...new Map(state.rows.map((r) => [r.category_en, r.category])).entries()];
  $("cards").replaceChildren(...scored.map((s) => {
    const o = s.summary.overall, e = node("div"); e.className = "quality-card jev";
    e.append(node("span", s.system), node("strong", percent(o.pass_rate)), node("small", `인정 ${number(o.passed)} / ${number(o.scored)} · 평균 ${o.mean} · 채움 ${number(o.filled)}/${number(o.sentences)}`));
    return e;
  }));
  const head = node("tr"); head.append(node("th", "카테고리"), node("th", "문장"));
  for (const s of scored) { const th = node("th", s.system); th.className = "jev-text"; head.append(th); }
  $("compare-head").replaceChildren(head);
  const rows = [["", "전체", state.rows.length]].concat(cats.map(([id, name]) => [id, name, state.rows.filter((r) => r.category_en === id).length]));
  $("compare").replaceChildren(...rows.map(([id, name, n]) => {
    const tr = node("tr"), td = node("td", name + (WEAK.has(id) ? " ※" : ""));
    if (id) { td.className = "text-link"; td.addEventListener("click", () => { $("category").value = id; state.page = 0; renderRows(); }); }
    tr.append(td, node("td", number(n)));
    for (const s of scored) {
      const m = id ? s.summary.categories.find((c) => c.id === id) : s.summary.overall;
      const cell = node("td"); cell.className = "jev-text";
      cell.append(node("span", m ? percent(m.pass_rate) : "—"));
      if (m && m.pass_rate !== null) { const b = node("div"); b.className = "bar jev-bar"; const x = node("span"); x.style.width = m.pass_rate * 100 + "%"; b.append(x); cell.append(b); cell.title = `인정 ${m.passed}/${m.scored} · 평균 ${m.mean} · 원문 표기 남음 ${m.unread}`; }
      tr.append(cell);
    }
    return tr;
  }));
}
function visible() {
  const cat = $("category").value, view = $("view").value, sort = $("sort").value;
  const rows = state.rows.filter((r) => {
    if (cat && r.category_en !== cat) return false;
    const s = scoreOf(r);
    if (view === "empty") return !state.outputs[r.id];
    if (view === "fail") return s && !s.passed;
    if (view === "pass") return s && s.passed;
    return true;
  });
  if (!sort) return rows;
  // Worst first (or best first); rows without a current score go last.
  const sign = sort === "desc" ? -1 : 1;
  return rows.slice().sort((a, b) => {
    const x = scoreOf(a), y = scoreOf(b);
    if (!x || !y) return (x ? 0 : 1) - (y ? 0 : 1);
    return sign * (x.noul - y.noul);
  });
}
function renderRows() {
  const rows = visible(), pages = Math.max(1, Math.ceil(rows.length / PAGE));
  state.page = Math.min(state.page, pages - 1);
  const slice = rows.slice(state.page * PAGE, (state.page + 1) * PAGE);
  $("rows").replaceChildren(...slice.map((r) => {
    const tr = node("tr"), input = node("textarea"), cell = node("td"), result = node("div"), button = node("button", "평가");
    input.rows = 2; input.value = state.outputs[r.id] || ""; input.placeholder = "이 원문을 TN으로 변환한 결과";
    const paint = () => {
      const s = scoreOf(r);
      result.className = "jev-text row-result" + (s ? (s.passed ? " pass" : " fail") : "");
      result.textContent = s ? `${s.noul.toFixed(2)} ${s.passed ? "인정" : "불인정"}${s.unread.length ? " · 남은 표기 " + s.unread.join(" ") : ""}`
        : state.outputs[r.id] ? (state.scores[r.id] ? "수정됨 · 다시 평가 필요" : "미평가") : "—";
      button.disabled = state.busy || !state.outputs[r.id];
    };
    input.addEventListener("input", () => { if (input.value) state.outputs[r.id] = input.value; else delete state.outputs[r.id]; markDirty(); paint(); });
    button.className = "secondary row-score"; button.title = "이 문장 하나만 Jev로 평가합니다";
    button.addEventListener("click", safely(() => scoreRows([r.id], paint)));
    paint();
    cell.append(result, button);
    tr.append(node("td", r.category + (WEAK.has(r.category_en) ? " ※" : "")), node("td", r.text), node("td"), cell);
    tr.children[2].append(input);
    return tr;
  }));
  $("row-count").textContent = `${number(rows.length)}문장 중 ${rows.length ? number(state.page * PAGE + 1) : 0}–${number(state.page * PAGE + slice.length)}`;
  $("page").textContent = `${state.page + 1} / ${pages}`;
  $("prev").disabled = state.page === 0; $("next").disabled = state.page + 1 >= pages;
}
function renderAll() { renderInfo(); renderCompare(); renderRows(); }

async function loadSystems() {
  const r = await get("/api/jev-eval/systems", { set: state.set.id });
  state.systems = r.systems;
  const current = $("system").value;
  $("system").replaceChildren(option("", "새 시스템"), ...r.systems.map((s) => option(s.system, s.system + (s.summary?.overall.scored ? ` · Jev ${percent(s.summary.overall.pass_rate)}` : ""))));
  if (r.systems.some((s) => s.system === current)) $("system").value = current;
}
async function loadSet(id) {
  const r = await get("/api/jev-eval/set", { set: id });
  state.set = { id, meta: r.meta }; state.rows = r.rows; state.outputs = {}; state.scores = {}; state.dirty = false; state.page = 0;
  $("category").replaceChildren(option("", "전체"), ...[...new Map(r.rows.map((x) => [x.category_en, x.category])).entries()].map(([k, v]) => option(k, v + (WEAK.has(k) ? " ※" : ""))));
  await loadSystems(); renderAll();
}
async function loadSystem() {
  const name = $("system").value;
  if (state.dirty && !confirm("저장하지 않은 결과가 있습니다. 버리고 불러올까요?")) return;
  $("system-name").value = name;
  if (!name) { state.outputs = {}; state.scores = {}; state.dirty = false; renderAll(); return; }
  const d = await get("/api/jev-eval/system", { set: state.set.id, system: name });
  state.outputs = d.outputs || {}; state.scores = d.scores || {}; state.dirty = false; renderAll();
  message(`${name} 결과를 불러왔습니다${d.scored_at ? " · 마지막 Jev 평가 " + d.scored_at.slice(0, 16).replace("T", " ") : ""}.`);
}
function parseImport(name, text) {
  const out = {}, ids = new Set(state.rows.map((r) => r.id)), lines = text.replace(/\r/g, "").split("\n");
  if (/\.json$/i.test(name)) {
    const data = JSON.parse(text);
    if (Array.isArray(data)) for (const x of data) out[x.id] = x.actual ?? x.output ?? x.tn;
    else Object.assign(out, data);
  } else if (/\.jsonl$/i.test(name)) {
    for (const line of lines) if (line.trim()) { const x = JSON.parse(line); out[x.id] = x.actual ?? x.output ?? x.tn; }
  } else if (/\.(tsv|csv)$/i.test(name)) {
    const sep = /\.csv$/i.test(name) ? "," : "\t";
    for (const line of lines) if (line.trim()) { const cols = line.split(sep); if (ids.has(cols[0])) out[cols[0]] = cols[cols.length - 1]; }
  } else {
    const body = lines[lines.length - 1] === "" ? lines.slice(0, -1) : lines;
    if (body.length !== state.rows.length) throw Error(`TXT는 원문 순서대로 ${state.rows.length}줄이어야 합니다 (${body.length}줄).`);
    state.rows.forEach((r, i) => (out[r.id] = body[i]));
  }
  const unknown = Object.keys(out).filter((k) => !ids.has(k));
  if (unknown.length) throw Error(`평가셋에 없는 id ${unknown.length}개 (예: ${unknown[0]})`);
  return out;
}
async function save() {
  const name = systemName();
  if (!name) throw Error("시스템 이름을 입력하세요.");
  const r = await post("/api/jev-eval/save", { set: state.set.id, system: name, outputs: state.outputs });
  state.dirty = false; await loadSystems(); $("system").value = name; renderAll();
  message(`${name} 결과를 저장했습니다 (${number(r.summary.overall.filled)}문장).`);
}
async function streamScore(body) {
  const response = await fetch("/api/jev-eval/score", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  if (!response.ok) { const e = await response.json(); throw Error(e.error || "Jev 평가 실패"); }
  const reader = response.body.getReader(), decoder = new TextDecoder();
  let pending = "", result = null;
  for (;;) {
    const { value, done } = await reader.read();
    pending += decoder.decode(value || new Uint8Array(), { stream: !done });
    const lines = pending.split("\n"); pending = lines.pop();
    for (const line of lines) {
      if (!line.trim()) continue;
      const ev = JSON.parse(line);
      if (ev.event === "progress" && body.onProgress) body.onProgress(ev);
      else if (ev.event === "error") throw Error(ev.message);
      else if (ev.event === "result") result = ev;
    }
    if (done) break;
  }
  if (!result) throw Error("Jev 평가가 완료되지 않았습니다.");
  return result;
}
// One row or the edited rows: send just their outputs, keep every other score.
async function scoreRows(ids, repaint) {
  const name = systemName();
  if (!name) throw Error("시스템 이름을 입력하세요.");
  ids = ids.filter((id) => state.outputs[id]);
  if (!ids.length) throw Error("평가할 결과가 없습니다.");
  lock(true);
  try {
    if (ids.length > 1) { document.querySelector(".jev-progress").hidden = false; $("progress-fill").style.width = "0%"; $("progress-text").textContent = `${number(ids.length)}문장을 평가합니다…`; }
    const patch = Object.fromEntries(ids.map((id) => [id, state.outputs[id]]));
    const onProgress = (ev) => { $("progress-fill").style.width = (ev.total ? (ev.done / ev.total) * 100 : 100) + "%"; };
    const result = await streamScore({ set: state.set.id, system: name, ids, patch, onProgress });
    Object.assign(state.scores, result.scores);
    await loadSystems(); $("system").value = name;
    if (ids.length > 1) { $("progress-fill").style.width = "100%"; $("progress-text").textContent = `완료 · ${number(ids.length)}문장 평가 · 새로 채점 ${number(result.summary.newly_scored)}`; }
    renderInfo(); renderCompare();
    if (repaint) repaint(); else renderRows();
    const one = ids.length === 1 ? result.scores[ids[0]] : null;
    message(one ? `Jev ${one.noul.toFixed(2)} · ${one.passed ? "인정" : "불인정"} · 전체 인정 ${percent(result.summary.overall.pass_rate)}` : `${number(ids.length)}문장을 평가했습니다 · 전체 인정 ${percent(result.summary.overall.pass_rate)}`);
  } finally { lock(false); if (repaint) repaint(); }
}
async function score() {
  const name = systemName();
  if (!name) throw Error("시스템 이름을 입력하세요.");
  if (!filled()) throw Error("채워진 결과가 없습니다.");
  if (filled() < state.rows.length && !confirm(`${state.rows.length - filled()}문장이 비어 있습니다. 채운 문장만 평가할까요?`)) return;
  lock(true);
  try {
    if (state.dirty || !state.systems.some((s) => s.system === name)) await save();
    $("progress-fill").style.width = "0%"; document.querySelector(".jev-progress").hidden = false;
    $("progress-text").textContent = "Jev 평가를 시작합니다…";
    const response = await fetch("/api/jev-eval/score", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ set: state.set.id, system: name }) });
    if (!response.ok) { const e = await response.json(); throw Error(e.error || "Jev 평가 실패"); }
    const reader = response.body.getReader(), decoder = new TextDecoder();
    let pending = "", result = null;
    for (;;) {
      const { value, done } = await reader.read();
      pending += decoder.decode(value || new Uint8Array(), { stream: !done });
      const lines = pending.split("\n"); pending = lines.pop();
      for (const line of lines) {
        if (!line.trim()) continue;
        const ev = JSON.parse(line);
        if (ev.event === "progress") {
          $("progress-fill").style.width = (ev.total ? (ev.done / ev.total) * 100 : 100) + "%";
          $("progress-text").textContent = ev.total ? `새로 채점 ${number(ev.done)} / ${number(ev.total)}문장 (이미 채점한 결과는 캐시 사용)` : "모두 캐시에 있는 결과입니다.";
        } else if (ev.event === "error") throw Error(ev.message);
        else if (ev.event === "result") result = ev;
      }
      if (done) break;
    }
    if (!result) throw Error("Jev 평가가 완료되지 않았습니다.");
    $("progress-fill").style.width = "100%";
    $("progress-text").textContent = `완료 · 새로 채점 ${number(result.summary.newly_scored)}문장 · Jev 인정 ${percent(result.summary.overall.pass_rate)}`;
    const d = await get("/api/jev-eval/system", { set: state.set.id, system: name });
    state.scores = d.scores || {}; await loadSystems(); $("system").value = name; renderAll();
    message(`${name}: Jev 인정 ${percent(result.summary.overall.pass_rate)} (${number(result.summary.overall.passed)} / ${number(result.summary.overall.scored)}).`);
  } finally { lock(false); }
}

$("set").addEventListener("change", safely(() => loadSet($("set").value)));
$("system").addEventListener("change", safely(loadSystem));
$("export").addEventListener("click", () => {
  const f = $("export-format").value, id = state.set.id;
  if (f === "jsonl") download(`${id}-sentences.jsonl`, state.rows.map((r) => JSON.stringify({ id: r.id, category: r.category, text: r.text })).join("\n") + "\n", "application/x-ndjson");
  else if (f === "tsv") download(`${id}-sentences.tsv`, state.rows.map((r) => [r.id, r.category, r.text].join("\t")).join("\n") + "\n", "text/tab-separated-values");
  else download(`${id}-sentences.txt`, state.rows.map((r) => r.text).join("\n") + "\n", "text/plain");
});
$("fill").addEventListener("click", safely(async () => {
  if (Object.keys(state.outputs).length && !confirm("현재 결과를 우리 TN 결과로 덮어쓸까요?")) return;
  lock(true); message("우리 TN으로 모든 원문을 변환하는 중입니다…");
  try {
    const r = await post("/api/jev-eval/fill", { set: state.set.id });
    state.outputs = r.outputs; state.scores = {}; if (!systemName()) $("system-name").value = "ours";
    markDirty(); renderAll(); message(`우리 TN 결과 ${number(filled())}문장을 채웠습니다. 저장하거나 Jev 평가를 실행하세요.`);
  } finally { lock(false); }
}));
$("import").addEventListener("change", safely(async () => {
  const file = $("import").files[0]; if (!file) return;
  const parsed = parseImport(file.name, await file.text());
  Object.assign(state.outputs, Object.fromEntries(Object.entries(parsed).filter(([, v]) => v)));
  $("import").value = ""; markDirty(); renderAll();
  message(`${file.name}에서 ${number(Object.keys(parsed).length)}문장을 가져왔습니다. 시스템 이름을 확인하고 저장하세요.`);
}));
$("save").addEventListener("click", safely(save));
$("score").addEventListener("click", safely(score));
$("score-changed").addEventListener("click", safely(() => scoreRows(changedIds())));
for (const id of ["category", "view", "sort"]) $(id).addEventListener("change", () => { state.page = 0; renderRows(); });
$("prev").addEventListener("click", () => { state.page = Math.max(0, state.page - 1); renderRows(); });
$("next").addEventListener("click", () => { state.page++; renderRows(); });
window.addEventListener("beforeunload", (e) => { if (state.dirty) { e.preventDefault(); e.returnValue = ""; } });
safely(async () => {
  const r = await get("/api/jev-eval/sets");
  if (!r.sets.length) { message("평가셋이 없습니다. select_representative.py --sentences-only로 만드세요.", true); return; }
  $("set").replaceChildren(...r.sets.map((s) => option(s.set_id, `${s.set_id} · ${number(s.sentences)}문장`)));
  await loadSet(r.sets[0].set_id);
  message("시스템을 고르거나 새 이름을 입력하고, 결과를 채운 뒤 Jev 평가를 실행하세요.");
})();
