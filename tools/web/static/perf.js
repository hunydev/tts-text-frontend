"use strict";
const $ = (id) => document.getElementById(id);
const fmt = (v, d = 1) => (v === undefined || v === null ? "—" : Number(v).toLocaleString("ko-KR", { maximumFractionDigits: d, minimumFractionDigits: d }) + " ms");
function node(tag, text) { const e = document.createElement(tag); if (text !== undefined) e.textContent = text; return e; }
function card(label, value, note) { const e = node("div"); e.className = "quality-card"; e.append(node("span", label), node("strong", value), node("small", note || "")); return e; }
async function stream(body, onEvent) {
  const r = await fetch("/api/run", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  if (!r.ok) throw Error((await r.json()).error || "요청 실패");
  const reader = r.body.getReader(), dec = new TextDecoder(); let buf = "";
  for (;;) { const { value, done } = await reader.read(); if (done) break; buf += dec.decode(value, { stream: true }); let i; while ((i = buf.indexOf("\n")) >= 0) { const line = buf.slice(0, i); buf = buf.slice(i + 1); if (line.trim()) onEvent(JSON.parse(line)); } }
}
async function run() {
  $("run").disabled = true; $("rows").replaceChildren(); $("cards").replaceChildren(); $("notice").textContent = "측정 중입니다…";
  try {
    let perf = null;
    const t0 = performance.now();
    await stream({ mode: "perf", text: "", options: {} }, (e) => {
      if (e.event === "progress") { const r = e.row, tr = node("tr"); tr.append(node("td", r.sentences.toLocaleString("ko-KR")), node("td", r.chars.toLocaleString("ko-KR")), node("td", r.repeats + "회"), node("td", fmt(r.normalize_ms, 2)), node("td", fmt(r.per_sentence_ms, 3)), node("td", r.spans.toLocaleString("ko-KR"))); $("rows").append(tr); }
      if (e.event === "result") perf = e.perf;
      if (e.event === "error") throw Error(e.message);
    });
    const perfMs = performance.now() - t0;
    // A real one-sentence web normalization, timed in the browser.
    let single = null; const t1 = performance.now();
    await stream({ mode: "normalize", text: perf.sample, options: {} }, (e) => { if (e.event === "result") single = e; if (e.event === "error") throw Error(e.message); });
    const roundTrip = performance.now() - t1;
    $("cards").append(
      card("웹 왕복 (1문장)", fmt(roundTrip), "브라우저 → 서버 → 새 작업 프로세스 → 결과"),
      card("작업 프로세스 시작", fmt(perf.process_start_ms), "Python 시작과 모듈 로드"),
      card("엔진 로드", fmt(perf.engine_load_ms), "라이브러리·리소스 로드 (요청마다)"),
      card("정규화 (1문장)", fmt(perf.rows[0].normalize_ms, 2), "엔진 로드 이후 · 상세 추적 켜면 " + fmt(perf.traced_one_ms, 2)),
      card("사전 커버리지 계산", fmt(perf.coverage_ms), "캐시 사용 · 캐시 없으면 " + fmt(perf.coverage_uncached_ms)),
      card("작업 내부 경과", fmt(single.elapsed_ms), "엔진 로드 이후 정규화·토큰·커버리지"));
    $("meta").textContent = `숫자·영문 혼합 문장 ${perf.mixed_sentences.toLocaleString("ko-KR")}개 · 표는 상세 추적 끔 · 측정 요청 ${fmt(perfMs, 0)}`;
    $("notice").textContent = "측정을 마쳤습니다.";
  } catch (e) { $("notice").textContent = e.message; $("notice").classList.add("error"); }
  finally { $("run").disabled = false; }
}
$("run").addEventListener("click", run);
