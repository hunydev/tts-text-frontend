"use strict";
const $ = (id) => document.getElementById(id);
const labels = {
  pending: "검토 대기",
  "source-checked": "출처 대조",
  "human-reviewed": "사람 검수 완료",
};
const state = {
  data: null,
  selected: null,
  limit: 50,
  coverage: null,
  busy: false,
};
function node(tag, text) {
  const e = document.createElement(tag);
  if (text !== undefined) e.textContent = text;
  return e;
}
function notice(text, error = false) {
  $("lex-notice").textContent = text;
  $("lex-notice").classList.toggle("error", error);
}
function filter() {
  if (!state.data) return;
  const q = $("lex-query").value.toLowerCase().trim(),
    category = $("lex-category").value,
    review = $("lex-review").value;
  const sort = $("lex-sort").value;
  const rows = state.data.entries.filter(
    (r) =>
      (!category || r.category === category) &&
      (!review || r.review_status === review) &&
      (!q ||
        [r.term, r.pronunciation, ...r.aliases].some((s) =>
          s.toLowerCase().includes(q),
        )),
  );
  if (sort !== "source") {
    const sign = sort === "jev-desc" ? -1 : 1;
    rows.sort((a, b) => {
      const x = a.jev?.noul,
        y = b.jev?.noul;
      if (x === undefined && y === undefined) return 0;
      if (x === undefined) return 1;
      if (y === undefined) return -1;
      return sign * (x - y);
    });
  }
  $("lex-count").textContent =
    `${rows.length}건 중 ${Math.min(rows.length, state.limit)}건 표시`;
  $("lex-rows").replaceChildren();
  rows.slice(0, state.limit).forEach((r) => {
    const tr = node("tr"),
      td = node("td"),
      button = node("button", r.term);
    button.className = "text-button";
    button.addEventListener("click", () => select(r));
    td.append(button);
    const score = node("td", r.jev ? r.jev.noul.toFixed(2) : "–");
    score.className = r.jev
      ? r.jev.noul < 0.3
        ? "jev-low"
        : r.jev.noul < 0.6
          ? "jev-mid"
          : "jev-high"
      : "";
    tr.append(
      td,
      node("td", r.pronunciation),
      score,
      node("td", labels[r.review_status]),
    );
    $("lex-rows").append(tr);
  });
  $("lex-more").hidden = rows.length <= state.limit;
}
function select(r) {
  if (state.busy) return;
  state.selected = r;
  $("lex-term").textContent = r.term;
  $("lex-state").textContent =
    `${labels[r.review_status]} · ${r.scope || "system"} · ${r.category} · ${r.id}`;
  $("lex-notes").textContent = r.jev
    ? `Jev 점수 ${r.jev.noul.toFixed(2)} (${r.jev.model}, ${r.jev.checked_at}) · ${r.notes}`
    : r.notes;
  $("lex-aliases").textContent = r.aliases.length
    ? `별칭: ${r.aliases.join(", ")}`
    : "대소문자는 같은 키로 처리합니다.";
  $("lex-alternatives").textContent = r.alternatives?.length
    ? `다른 읽기 후보: ${r.alternatives.join(", ")}`
    : "";
  $("lex-evidence").replaceChildren();
  r.sources.forEach((s) => {
    const li = node("li"),
      a = node("a", s.title);
    a.href = s.url;
    a.target = "_blank";
    a.rel = "noopener";
    li.append(
      a,
      node("span", ` · ${s.kind} · ${s.checked_at}`),
      node("p", s.note),
    );
    $("lex-evidence").append(li);
  });
  $("lex-pron").value = r.pronunciation;
  $("lex-example").value = `${r.term}의 발음을 확인합니다.`;
  for (const id of [
    "lex-pron",
    "lex-example",
    "lex-test-current",
    "lex-test-edit",
    "lex-export-tsv",
    "lex-export-json",
  ])
    $(id).disabled = false;
  $("lex-result").textContent = "현재 사전이나 수정안을 선택해 시험하세요.";
}
function edited() {
  const p = $("lex-pron").value;
  if (
    !/^[가-힣]+(?: [가-힣]+)*$/.test(p) ||
    new TextEncoder().encode(p).length > 500
  )
    throw Error(
      "발음은 한글과 단어 사이 공백으로 입력하세요 (500바이트 이내).",
    );
  return p;
}
function lock(value) {
  state.busy = value;
  for (const id of [
    "lex-test-current",
    "lex-test-edit",
    "lex-scan",
    "lex-pron",
    "lex-example",
  ])
    $(id).disabled = value || (!state.selected && id !== "lex-scan");
}
async function native(text, options = {}) {
  const controller = new AbortController(),
    timer = setTimeout(() => controller.abort(), 50000);
  try {
    const response = await fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mode: "normalize", text, options }),
      signal: controller.signal,
    });
    const raw = await response.text();
    if (!response.ok) throw Error(JSON.parse(raw).error || "실행 실패");
    const events = raw
      .trim()
      .split("\n")
      .map((line) => JSON.parse(line));
    const error = events.find((e) => e.event === "error");
    if (error) throw Error(error.message);
    if (!events.some((e) => e.event === "end" && e.ok))
      throw Error("실행이 완료되지 않았습니다.");
    return events.find((e) => e.event === "result");
  } finally {
    clearTimeout(timer);
  }
}
async function test(override) {
  if (!state.selected || state.busy) return;
  try {
    const r = state.selected,
      options = override
        ? {
            user_lexicon: [
              { word: r.term, pronunciation: edited() },
              ...r.aliases.map((word) => ({ word, pronunciation: edited() })),
            ],
          }
        : {};
    if (r.scope === "finance") options.domain = "finance-vocabulary";
    if (r.scope === "brands") options.brand_lexicon = "on";
    lock(true);
    $("lex-result").textContent = "검사 중입니다.";
    notice("실제 엔진으로 발음과 사전 경로를 검사합니다.");
    const result = await native($("lex-example").value, options);
    const traces = result.spans
      .filter((s) => s.lexicon)
      .map((s) => `${s.surface} → ${s.normalized} [${s.lexicon}]`);
    $("lex-result").textContent = result.text + "\n\n" + traces.join("\n");
    notice(
      override
        ? "수정안 시험이 완료되었습니다. 서버 사전은 변경되지 않았습니다."
        : "현재 사전 검사가 완료되었습니다.",
    );
  } catch (e) {
    $("lex-result").textContent = "검사를 완료하지 못했습니다.";
    notice(e.message, true);
  } finally {
    lock(false);
  }
}
function download(name, value, type) {
  const url = URL.createObjectURL(new Blob([value], { type }));
  const a = node("a");
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
$("lex-export-tsv").addEventListener("click", () => {
  try {
    const r = state.selected,
      p = edited();
    download(
      "customer-pronunciation.tsv",
      r.canonical_keys
        .map(
          (key) => `lexicon\t${r.target_lang}\t${r.source_lang}:${key}\t${p}\n`,
        )
        .join(""),
      "text/tab-separated-values;charset=utf-8",
    );
  } catch (e) {
    notice(e.message, true);
  }
});
$("lex-export-json").addEventListener("click", () => {
  try {
    const r = state.selected;
    download(
      "pronunciation-review-proposal.json",
      JSON.stringify(
        {
          schema_version: 1,
          entry_id: r.id,
          source_file: r.source_file,
          before: r.pronunciation,
          proposed: edited(),
          review_status: "pending",
          note: "User proposal; requires evidence and source review before promotion.",
        },
        null,
        2,
      ),
      "application/json",
    );
  } catch (e) {
    notice(e.message, true);
  }
});
$("lex-test-current").addEventListener("click", () => test(false));
$("lex-test-edit").addEventListener("click", () => test(true));
$("lex-scan").addEventListener("click", async () => {
  if (state.busy) return;
  try {
    lock(true);
    state.coverage = null;
    $("lex-coverage-summary").textContent = "";
    $("lex-coverage").textContent = "검사 중입니다.";
    $("lex-export-coverage").disabled = true;
    notice("원고의 사전 사용 현황을 검사합니다.");
    const result = await native($("lex-corpus").value, {
      domain: $("lex-scan-domain").value,
    });
    state.coverage = result.lexicon_coverage;
    const c = state.coverage;
    $("lex-coverage-summary").textContent =
      `외국어 span ${c.eligible_foreign_spans}개 · 직접 사전 ${c.routes["direct-lexicon"] || 0} · 원어 사전 ${c.routes["source-dictionary"] || 0} · fallback ${c.routes.fallback || 0}`;
    $("lex-coverage").textContent =
      c.terms
        .map(
          (t) =>
            `${t.term} · ${t.count}회 · ${t.trace} · ${labels[t.review_status] || t.review_status}\n  출력: ${t.rendered_examples.join(" / ")}`,
        )
        .join("\n") || "분석 대상인 외국어 발음 span이 없습니다.";
    $("lex-export-coverage").disabled = false;
    notice(
      "사용 현황 검사가 완료되었습니다. Fallback 결과는 사전에 자동 등록하지 않습니다.",
    );
  } catch (e) {
    $("lex-coverage").textContent = "검사를 완료하지 못했습니다.";
    notice(e.message, true);
  } finally {
    lock(false);
  }
});
$("lex-export-coverage").addEventListener("click", () =>
  download(
    "lexicon-coverage.json",
    JSON.stringify(state.coverage, null, 2),
    "application/json",
  ),
);
for (const id of ["lex-query", "lex-category", "lex-review", "lex-sort"])
  $(id).addEventListener(id === "lex-query" ? "input" : "change", () => {
    state.limit = 50;
    filter();
  });
$("lex-more").addEventListener("click", () => {
  state.limit += 50;
  filter();
});
(async () => {
  try {
    const response = await fetch("/api/lexicon");
    const data = await response.json();
    if (!response.ok) throw Error(data.error);
    state.data = data;
    $("lex-total").textContent =
      `${data.summary.entries}항목 · ${data.summary.runtime_keys}키`;
    $("lex-summary").textContent =
      Object.keys(labels)
        .map((k) => `${labels[k]} ${data.summary.review_status[k] || 0}건`)
        .join(" · ") + ` · Jev 점수 ${data.summary.jev_scored || 0}건`;
    if (!data.summary.jev_scored) $("lex-sort").value = "source";
    Object.keys(data.summary.categories).forEach((c) => {
      const o = node("option", c);
      o.value = c;
      $("lex-category").append(o);
    });
    filter();
    select(
      data.entries.find((r) => r.canonical_keys.includes("etf")) ||
        data.entries[0],
    );
    notice("단어를 검색하거나 원고의 미등록 용어를 확인하세요.");
  } catch (e) {
    notice(e.message, true);
  }
})();
