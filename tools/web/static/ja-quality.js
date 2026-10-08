"use strict";
const $ = (id) => document.getElementById(id);
const node = (tag, text, cls) => {
  const el = document.createElement(tag);
  if (text !== undefined && text !== null) el.textContent = text;
  if (cls) el.className = cls;
  return el;
};
const pct = (v) => (v * 100).toFixed(1) + "%";
const SYSTEMS = ["raw", "wetext", "nemo", "ours", "ours-kata"];
let offset = 0;

async function get(path) {
  const response = await fetch(path);
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || response.statusText);
  return data;
}

function card(label, value, note) {
  const box = node("div", null, "quality-card");
  box.append(node("small", label), node("strong", value), node("small", note));
  return box;
}

function renderSummary(s) {
  $("provenance").textContent = `${s.tag} · ${s.source} · 읽기기 ${s.reader} · ${s.seconds}초 · 검토 ${s.review ? s.review.cases : 0}문장`;
  const adj = s.subsets["digits-adjusted"] || s.subsets.digits;
  const digits = s.subsets.digits;
  const cards = $("cards");
  cards.replaceChildren();
  cards.append(card("숫자 구간 정확도 (우리)", pct(digits.number_spans.rate), `${digits.number_spans.ok}/${digits.number_spans.n} 구간`));
  for (const [name, label] of [["ours", "우리 + 읽기기"], ["raw", "pyopenjtalk 단독"], ["wetext", "wetext + 읽기기"], ["nemo", "NeMo + 읽기기"]]) {
    if (!adj[name]) continue;
    cards.append(card(`${label} · 조정 문장 일치`, pct(adj[name].exact_rate), `${adj[name].exact}/${adj.n} · CER ${pct(adj[name].cer)}`));
  }
  const body = $("subsets");
  body.replaceChildren();
  for (const [subset, block] of Object.entries(s.subsets)) {
    const tr = node("tr");
    tr.append(node("td", subset), node("td", String(block.n)));
    for (const name of SYSTEMS) {
      const v = block[name];
      tr.append(node("td", v ? `${v.exact}/${block.n} (${pct(v.exact_rate)}) · CER ${pct(v.cer)}` : "—"));
    }
    const ns = block.number_spans;
    tr.append(node("td", ns && ns.n ? `${ns.ok}/${ns.n} (${pct(ns.rate)})` : "—"));
    body.append(tr);
  }
}

function renderCases(data) {
  $("case-count").textContent = `${data.total}문장`;
  $("page").textContent = data.total ? `${data.offset + 1}–${Math.min(data.total, data.offset + data.rows.length)}` : "";
  $("prev").disabled = data.offset === 0;
  $("next").disabled = data.offset + data.rows.length >= data.total;
  const body = $("cases");
  body.replaceChildren();
  for (const r of data.rows) {
    const tr = node("tr");
    const text = node("td", r.text, "out");
    if (r.review) text.append(node("div", "검토: " + r.review, "small"));
    tr.append(text, node("td", r.reference, "out"), node("td", (r.ours || {}).normalized || "", "out"));
    const spans = node("td", null, "out");
    for (const s of r.spans || []) spans.append(node("div", `${s.written} → ${s.reading}`, s.ok ? "" : "err"));
    tr.append(spans);
    const readings = node("td", null, "out");
    for (const [name, label] of [["ours", "우리"], ["raw", "단독"], ["wetext", "wetext"], ["nemo", "NeMo"]]) {
      const v = r[name];
      if (!v) continue;
      readings.append(node("div", `${label} ${v.exact ? "✓" : "✗"} ${v.kana || v.error || ""}`, v.exact ? "" : "err"));
    }
    tr.append(readings);
    body.append(tr);
  }
}

async function loadCases() {
  const params = new URLSearchParams({ run: $("run").value, kind: $("kind").value, q: $("query").value, offset });
  renderCases(await get("/api/ja-quality/cases?" + params));
}

async function loadRun() {
  renderSummary(await get("/api/ja-quality/summary?run=" + encodeURIComponent($("run").value)));
  offset = 0;
  await loadCases();
}

async function main() {
  try {
    const runs = await get("/api/ja-quality/runs");
    if (!runs.length) throw new Error("아직 일본어 측정 기록이 없습니다.");
    for (const run of runs) {
      const option = node("option", `${run.tag} · ${new Date(run.mtime * 1000).toLocaleString("ko-KR")}`);
      option.value = run.tag;
      $("run").append(option);
    }
    $("run").addEventListener("change", () => loadRun().catch(fail));
    $("search").addEventListener("click", () => { offset = 0; loadCases().catch(fail); });
    $("kind").addEventListener("change", () => { offset = 0; loadCases().catch(fail); });
    $("prev").addEventListener("click", () => { offset = Math.max(0, offset - 50); loadCases().catch(fail); });
    $("next").addEventListener("click", () => { offset += 50; loadCases().catch(fail); });
    await loadRun();
    $("notice").textContent = "";
  } catch (error) {
    fail(error);
  }
}
function fail(error) {
  $("notice").textContent = "불러오지 못했습니다: " + error.message;
  $("notice").className = "error";
}
main();
