"use strict";
const $ = (id) => document.getElementById(id);
const state = {
  mode: "normalize",
  view: "spans",
  catalog: null,
  events: [],
  result: null,
  controller: null,
  request: null,
  sampleReview: null,
};
const descriptions = {
  normalize: "정규화된 텍스트와 모든 span을 확인합니다.",
  stream: "입력을 나누어 보내고 실제 출력 콜백을 관찰합니다.",
  verify: "같은 설정의 batch와 여러 청크 분할 결과를 비교합니다.",
  corpus: "개발 기대값·메타데이터·청크 일치를 확인하고 기준의 검토 상태를 표시합니다.",
};
const reviewLabels = {
  "reference-policy": "개발 정책 기대값",
  "provisional-pronunciation": "임시 발음값",
  "preservation-contract": "원문 보존 계약",
  "needs-review": "추가 검토 필요",
};
function reviewText(review) {
  const label = reviewLabels[review?.classification] || "사용자 지정 기대 출력";
  return `${label} · 독립 검토 대기`;
}
function corpusReview() {
  const selected = state.catalog?.corpora.find((item) => item.id === $("corpus").value);
  const review = selected?.reference_review;
  $("corpus-review").textContent = review
    ? Object.entries(review.classifications)
        .map(([key, value]) => `${reviewLabels[key] || key} ${value}건`)
        .join(" · ") + ` · 독립 검토 완료 ${review.human_approved_cases}건`
    : "기준의 검토 상태가 없습니다.";
}
function loadSample(sample) {
  $("sample").value = sample.id;
  $("source").value = sample.text;
  $("expected").value = sample.expected ?? "";
  $("expected-enabled").checked = sample.expected !== undefined;
  $("expected-details").open = sample.expected !== undefined;
  state.sampleReview = sample.reference_review;
  $("sample-review").textContent = reviewText(state.sampleReview);
  settings(sample.options);
  count();
}
const labels = {
  normalize: "정규화 실행 ↗",
  stream: "스트리밍 시작 ↗",
  verify: "청크 검증 실행 ↗",
  corpus: "코퍼스 검증 실행 ↗",
};
function node(tag, text, className) {
  const e = document.createElement(tag);
  if (text !== undefined) e.textContent = text;
  if (className) e.className = className;
  return e;
}
function editLock(locked) {
  document
    .querySelectorAll(
      "#playground input,#playground textarea,#playground select,#config-panel input,#config-panel textarea,#config-panel select,#config-panel button,#clear,#corpus",
    )
    .forEach((e) => (e.disabled = locked));
}
function message(text, error = false) {
  $("notice").textContent = text;
  $("notice").classList.toggle("error", error);
}
function bytes(text) {
  return new TextEncoder().encode(text).length;
}
function count() {
  $("input-count").textContent =
    bytes($("source").value).toLocaleString() + " bytes";
}
function badge(id, text, good) {
  const e = $(id);
  e.textContent = text;
  e.className = "badge " + (good === null ? "muted" : good ? "" : "fail");
}
function markStale() {
  if (state.result && !state.controller) {
    badge("match", "이전 실행 결과", null);
    badge("verify-status", "이전 실행 결과", null);
  }
}
function settings(options = {}) {
  const o = { ...state.catalog.defaults, ...options };
  const fields = {
    default_language: "language",
    render_policy: "policy",
    domain: "domain",
    first_target: "first-target",
    normal_target: "normal-target",
    max_length: "max-length",
    max_pending_bytes: "max-pending",
    chunk_bytes: "chunk-bytes",
    delay_ms: "delay-ms",
    random_seed: "random-seed",
  };
  Object.entries(fields).forEach(([k, id]) => ($(id).value = o[k]));
  $("sentence-split").checked = o.sentence_split;
  $("phrase-split").checked = o.phrase_split;
  $("verify-chunks").value = o.verify_chunks.join(", ");
  $("chunk-sizes").value = o.chunk_sizes.join(", ");
  $("lexicon").value = o.user_lexicon
    .map((x) => x.word + "=" + x.pronunciation)
    .join("\n");
  $("customer-tsv").value = o.customer_tsv;
  $("remove-wifi").checked = o.remove_system_words.includes("wifi");
}
function options() {
  const number = (id) => {
    const n = Number($(id).value);
    if (!$(id).value.trim() || !Number.isInteger(n))
      throw Error(id + ": 정수를 입력하세요.");
    return n;
  };
  const lexicon = $("lexicon")
    .value.split("\n")
    .filter((x) => x.trim())
    .map((line) => {
      const at = line.search(/[=\t]/);
      if (at < 1) throw Error("사전은 한 줄에 단어=발음으로 입력하세요.");
      return {
        word: line.slice(0, at).trim(),
        pronunciation: line.slice(at + 1).trim(),
      };
    });
  const chunkSizes = $("chunk-sizes").value.trim();
  return {
    default_language: $("language").value,
    render_policy: $("policy").value,
    domain: $("domain").value,
    first_target: number("first-target"),
    normal_target: number("normal-target"),
    max_length: number("max-length"),
    max_pending_bytes: number("max-pending"),
    chunk_bytes: number("chunk-bytes"),
    chunk_sizes: chunkSizes
      ? chunkSizes.split(",").map((x) => {
          if (!/^\d+$/.test(x.trim()))
            throw Error(
              "직접 지정 write 크기는 쉼표로 구분한 양의 정수입니다.",
            );
          return Number(x.trim());
        })
      : [],
    delay_ms: number("delay-ms"),
    sentence_split: $("sentence-split").checked,
    phrase_split: $("phrase-split").checked,
    verify_chunks: $("verify-chunks")
      .value.split(",")
      .map((x) => {
        if (!/^\d+$/.test(x.trim()))
          throw Error("청크 크기는 쉼표로 구분한 정수입니다.");
        return Number(x.trim());
      }),
    random_seed: number("random-seed"),
    customer_tsv: $("customer-tsv").value,
    user_lexicon: lexicon,
    remove_system_words: $("remove-wifi").checked ? ["wifi"] : [],
  };
}
function mode(value) {
  if (state.controller) return;
  state.mode = value;
  $("stream-settings").open = value === "stream" || value === "verify";
  document.querySelectorAll("[data-mode]").forEach((x) => {
    const active = x.dataset.mode === value;
    x.classList.toggle("active", active);
    x.setAttribute("aria-pressed", String(active));
  });
  $("playground").hidden =
    $("config-panel").hidden =
    $("inspect-panel").hidden =
      value === "corpus";
  $("corpus-panel").hidden = value !== "corpus";
  $("stream-panel").hidden = value !== "stream";
  $("verify-panel").hidden = value !== "verify";
  $("mode-description").textContent = descriptions[value];
  $("run").textContent = labels[value];
  message(descriptions[value]);
}
function details(row) {
  const target = $("span-detail");
  target.replaceChildren(node("span", "SPAN DETAILS", "eyebrow"));
  const fields = {
    surface: "원문",
    normalized: "출력",
    semantic_type: "Semantic class",
    value: "값",
    secondary: "보조 값 / 범위 끝",
    unit: "단위",
    source_lang: "Source language",
    context_lang: "Context language",
    render_lang: "Render language",
    rule: "적용 규칙",
    resource_layer: "규칙 리소스 계층",
    lexicon: "발음 처리 경로",
    pronunciation_layer: "발음 리소스 계층",
    verbalizer: "Verbalizer",
    context_trace: "문맥 판단 trace",
    script: "문자 분류",
    input_begin: "원문 시작 byte",
    input_end: "원문 끝 byte",
    begin: "출력 시작 byte",
    end: "출력 끝 byte",
  };
  const dl = node("dl");
  Object.entries(fields).forEach(([key, label]) => {
    if (row[key] !== undefined) {
      dl.append(node("dt", label), node("dd", String(row[key]) || "—"));
    }
  });
  target.append(dl);
}
function inspect() {
  const tokens = state.view === "tokens";
  const rows = state.result?.[tokens ? "tokens" : "spans"] || [];
  const head = node("tr");
  (tokens
    ? ["원문", "문자 분류", "byte 위치"]
    : ["원문 → 출력", "의미", "Source → Context", "규칙"]
  ).forEach((x) => head.append(node("th", x)));
  $("inspect-head").replaceChildren(head);
  $("inspect-body").replaceChildren();
  $("inspect-empty").hidden = rows.length > 0;
  rows.forEach((row) => {
    const tr = node("tr", undefined, "selectable");
    tr.tabIndex = 0;
    const values = tokens
      ? [row.surface, row.script, `${row.begin}–${row.end}`]
      : [
          `${row.surface} → ${row.normalized}`,
          row.semantic_type,
          `${row.source_lang} → ${row.context_lang}`,
          row.rule,
        ];
    values.forEach((text) => {
      const td = node("td", text);
      td.title = text;
      tr.append(td);
    });
    const select = () => {
      $("inspect-body")
        .querySelectorAll("tr")
        .forEach((x) => x.classList.remove("selected"));
      tr.classList.add("selected");
      details(row);
    };
    tr.addEventListener("click", select);
    tr.addEventListener("keydown", (e) => {
      if (e.key === "Enter" || e.key === " ") {
        e.preventDefault();
        select();
      }
    });
    $("inspect-body").append(tr);
  });
  if (rows.length) details(rows[0]);
  else $("span-detail").replaceChildren(node("p", "분석할 행을 선택하세요."));
}
function callback(event) {
  const box = node("div", undefined, "callback");
  const index = node("div");
  index.append(
    node("div", String(event.sequence + 1).padStart(2, "0"), "number"),
    node("small", event.elapsed_ms + " ms"),
  );
  const body = node("div");
  body.append(
    node(
      "small",
      `${event.stage === "flush" ? "마지막 flush" : "write #" + (event.write_index + 1)} · 원문 ${event.input_begin}–${event.input_end} bytes`,
    ),
    node("pre", event.text),
  );
  box.append(index, body);
  $("callbacks").append(box);
  $("output").classList.remove("empty");
  $("output").textContent += event.text;
  $("stream-status").textContent = event.sequence + 1 + "회 콜백";
}
function inputWrite(event) {
  const input = new TextEncoder()
    .encode(state.request.text)
    .slice(event.begin, event.end);
  let preview;
  try {
    preview = JSON.stringify(
      new TextDecoder("utf-8", { fatal: true }).decode(input),
    );
  } catch {
    preview =
      "UTF-8 분할 · hex " +
      Array.from(input.slice(0, 40), (x) =>
        x.toString(16).padStart(2, "0"),
      ).join(" ");
  }
  if (preview.length > 160) preview = preview.slice(0, 160) + "…";
  const row = node("div", undefined, "write-row");
  row.append(
    node(
      "small",
      `write #${event.sequence + 1} · ${event.begin}–${event.end} bytes · ${event.elapsed_ms} ms`,
    ),
    node("code", preview),
  );
  $("writes").append(row);
  if ($("writes").children.length > 128) $("writes").firstElementChild.remove();
  $("write-count").textContent = event.sequence + 1 + "회";
}
function verification(report) {
  badge(
    "verify-status",
    report.passed ? "모든 분할 일치" : "불일치 발견",
    report.passed,
  );
  const table = node("table");
  const head = node("tr");
  [
    "입력 분할",
    "출력 텍스트",
    "Span 메타데이터",
    "콜백 경계",
    "콜백 수",
  ].forEach((x) => head.append(node("th", x)));
  table.append(head);
  report.variants.forEach((v) => {
    const tr = node("tr");
    [
      v.label,
      v.match ? "일치" : "불일치",
      v.span_match ? "일치" : "불일치",
      v.segments_match ? "일치" : "불일치",
      String(v.segment_count),
    ].forEach((x) => tr.append(node("td", x)));
    table.append(tr);
  });
  const wrap = node("div", undefined, "table-wrap");
  wrap.append(table);
  $("verify-results").replaceChildren(wrap);
}
function caseResult(event) {
  const d = node("details", undefined, "case");
  const summary = node("summary");
  summary.append(
    node("span", event.id),
    node(
      "span",
      event.passed ? "회귀 일치" : "불일치",
      "badge " + (event.passed ? "" : "fail"),
    ),
  );
  d.append(summary);
  d.append(node("p", reviewText(event.reference_review), "small"));
  for (const [label, key] of [
    ["입력", "text"],
    ["기대 출력", "expected"],
    ["출력", "actual"],
  ])
    d.append(node("small", label), node("pre", event[key]));
  d.append(
    node(
      "p",
      `청크 일치: ${event.parity ? "통과" : "실패"} · 메타데이터 오류: ${event.metadata_failures.length}`,
      "small",
    ),
  );
  if (event.metadata_failures.length)
    d.append(node("pre", JSON.stringify(event.metadata_failures, null, 2)));
  $("corpus-results").append(d);
  const done = state.events.filter((x) => x.event === "case").length;
  const pass = state.events.filter(
    (x) => x.event === "case" && x.passed,
  ).length;
  $("corpus-progress").textContent =
    `검증 중 · ${done}건 완료 / ${pass}건 통과`;
}
function handle(event) {
  state.events.push(event);
  if (event.event === "error") throw Error(event.message);
  if (event.event === "chunk") {
    inputWrite(event);
    $("stream-status").textContent =
      `입력 ${event.end} bytes 전달 · 콜백 ${state.events.filter((x) => x.event === "segment").length}회`;
  }
  if (event.event === "segment") callback(event);
  if (event.event === "case") caseResult(event);
  if (event.event === "result") {
    state.result = event;
    if (state.mode === "corpus") {
      $("corpus-progress").textContent =
        `회귀 검증 완료 · ${event.total}건 중 ${event.passed}건 통과, ${event.failed}건 실패 · 독립 검토 완료 ${event.human_approved_cases}건`;
      return;
    }
    $("output").textContent = event.text || "(빈 출력)";
    $("output").classList.toggle("empty", !event.text);
    badge(
      "match",
      event.expected_match === null
        ? "기대 출력 미지정"
        : event.expected_match
          ? "기대 출력 일치"
          : "기대 출력 불일치",
      event.expected_match,
    );
    $("summary").textContent =
      `${event.spans.length} spans · ${bytes(event.text).toLocaleString()} bytes · ${event.elapsed_ms} ms`;
    const resourceCheck = event.resource_validation;
    $("resource-validation").textContent = resourceCheck
      ? `리소스 정책 검사 ${resourceCheck.validated_records}/${resourceCheck.records}행 · 검사기 미등록 ${resourceCheck.unvalidated_records}행 · 발음 정답 검토는 별도`
      : "";
    if (event.verification) verification(event.verification);
    inspect();
  }
}
async function execute() {
  if (state.controller) return;
  let body;
  try {
    body =
      state.mode === "corpus"
        ? { corpus: $("corpus").value }
        : { mode: state.mode, text: $("source").value, options: options() };
    if (state.mode !== "corpus" && $("expected-enabled").checked)
      body.expected = $("expected").value;
  } catch (e) {
    message(e.message, true);
    return;
  }
  editLock(true);
  state.request = body;
  state.events = [];
  state.result = null;
  $("resource-validation").textContent = "";
  state.controller = new AbortController();
  $("run").disabled = true;
  $("cancel").disabled = false;
  $("copy").disabled =
    $("download").disabled =
    $("corpus-download").disabled =
      true;
  document.querySelectorAll("[data-mode]").forEach((x) => (x.disabled = true));
  $("output").textContent = "";
  $("callbacks").replaceChildren();
  $("writes").replaceChildren();
  $("write-count").textContent = "0회";
  $("verify-results").replaceChildren();
  $("corpus-results").replaceChildren();
  $("corpus-progress").textContent = "실행 중";
  badge("match", "실행 중", null);
  badge("verify-status", "실행 중", null);
  inspect();
  message("실행 중입니다…");
  let reader;
  try {
    const response = await fetch(
      state.mode === "corpus" ? "/api/corpus" : "/api/run",
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
        signal: state.controller.signal,
      },
    );
    if (!response.ok) {
      const e = await response.json();
      throw Error(e.error || "서버 요청 실패");
    }
    reader = response.body.getReader();
    const decoder = new TextDecoder();
    let pending = "";
    let ended = false;
    while (true) {
      const { value, done } = await reader.read();
      pending += decoder.decode(value, { stream: !done });
      const lines = pending.split("\n");
      pending = lines.pop();
      for (const line of lines) {
        if (!line.trim()) continue;
        const e = JSON.parse(line);
        handle(e);
        if (e.event === "end" && e.ok) ended = true;
      }
      if (done) break;
    }
    if (pending.trim() || !ended || !state.result)
      throw Error(
        "완료 이벤트 없이 연결이 종료되었습니다. 결과가 불완전합니다.",
      );
    const failed =
      state.result.expected_match === false ||
      state.result.verification?.passed === false ||
      state.result.failed > 0;
    message(
      failed
        ? "검증이 끝났습니다. 불일치 항목을 확인하세요."
        : "실행이 완료되었습니다.",
      failed,
    );
    $("download").disabled = false;
    $("corpus-download").disabled = false;
    $("copy").disabled = state.mode === "corpus";
  } catch (e) {
    message(
      e.name === "AbortError"
        ? "실행을 중단했습니다. 부분 결과는 완료된 검증이 아닙니다."
        : e.message,
      e.name !== "AbortError",
    );
    badge("match", "미완료", null);
    badge("verify-status", "미완료", null);
    if (reader) await reader.cancel().catch(() => {});
  } finally {
    state.controller = null;
    editLock(false);
    $("run").disabled = false;
    $("cancel").disabled = true;
    document
      .querySelectorAll("[data-mode]")
      .forEach((x) => (x.disabled = false));
  }
}
document
  .querySelectorAll(
    "#playground input,#playground textarea,#config-panel input,#config-panel select,#config-panel textarea",
  )
  .forEach((x) =>
    x.addEventListener("input", () => {
      if (state.result && !state.controller) {
        message(
          "설정 또는 입력이 변경되었습니다. 표시된 결과는 이전 실행입니다. 다시 실행하세요.",
        );
        markStale();
      }
    }),
  );
$("run").addEventListener("click", execute);
$("cancel").addEventListener("click", () => state.controller?.abort());
$("source").addEventListener("input", count);
for (const id of ["source", "expected"]) {
  $(id).addEventListener("input", () => {
    state.sampleReview = null;
    $("sample-review").textContent = "직접 편집한 입력·기대 출력 · 독립 검토 상태 없음";
  });
}
$("corpus").addEventListener("change", corpusReview);
$("clear").addEventListener("click", () => {
  $("source").value = "";
  state.sampleReview = null;
  $("sample-review").textContent = "직접 편집한 입력·기대 출력 · 독립 검토 상태 없음";
  count();
  markStale();
});
$("reset").addEventListener("click", () => {
  settings();
  markStale();
  message("기본 설정을 불러왔습니다. 실행 버튼으로 확인하세요.");
});
$("split-cursor").addEventListener("click", () => {
  const source = $("source");
  const at = source.selectionStart;
  if (at <= 0 || at >= source.value.length) {
    message("입력 텍스트의 처음과 끝 사이에 커서를 놓으세요.", true);
    return;
  }
  if (/^[\uDC00-\uDFFF]/.test(source.value.slice(at))) {
    message("문자 중간에 커서가 있습니다. 문자 사이를 선택하세요.", true);
    return;
  }
  const first = bytes(source.value.slice(0, at));
  $("chunk-sizes").value = first + ", " + (bytes(source.value) - first);
  markStale();
  message(
    `write #1: ${first} bytes, write #2: 나머지 입력으로 설정했습니다. 스트리밍 또는 청크 검증을 실행하세요.`,
  );
});
$("sentence-preset").addEventListener("click", () => {
  $("sentence-split").checked = true;
  $("phrase-split").checked = false;
  $("max-length").value = 1000000;
  markStale();
  message(
    "문장 우선: 길이 기준 강제 분할을 최소화했습니다. 문장 경계 또는 최종 flush에서 출력합니다.",
  );
});
$("fast-preset").addEventListener("click", () => {
  $("sentence-split").checked = $("phrase-split").checked = true;
  $("first-target").value = 24;
  $("normal-target").value = 80;
  $("max-length").value = 200;
  markStale();
});
document
  .querySelectorAll("[data-mode]")
  .forEach((x) => x.addEventListener("click", () => mode(x.dataset.mode)));
document.querySelectorAll("[data-view]").forEach((x) =>
  x.addEventListener("click", () => {
    state.view = x.dataset.view;
    document
      .querySelectorAll("[data-view]")
      .forEach((y) => y.classList.toggle("active", y === x));
    inspect();
  }),
);
$("sample").addEventListener("change", () => {
  const s = state.catalog.samples.find((x) => x.id === $("sample").value);
  if (!s) return;
  loadSample(s);
  markStale();
  message("예제와 권장 설정을 불러왔습니다. 실행 버튼으로 확인하세요.");
});
$("copy").addEventListener("click", async () => {
  try {
    await navigator.clipboard.writeText(state.result.text);
    message("결과를 복사했습니다.");
  } catch {
    message("복사 권한이 없습니다. 결과 영역에서 직접 복사하세요.", true);
  }
});
$("corpus-download").addEventListener("click", () => $("download").click());
$("download").addEventListener("click", () => {
  const report = {
    workbench_version: state.catalog.version,
    request: state.request,
    events: state.events,
    result: state.result,
    selected_reference_review: state.sampleReview,
    reference_summary: state.catalog.reference_summary,
  };
  const url = URL.createObjectURL(
    new Blob([JSON.stringify(report, null, 2)], { type: "application/json" }),
  );
  const a = node("a");
  a.href = url;
  a.download = "ttf-text-report.json";
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});
document.addEventListener("keydown", (e) => {
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
    e.preventDefault();
    execute();
  }
});
(async () => {
  try {
    const r = await fetch("/api/catalog");
    if (!r.ok) throw Error("카탈로그를 불러오지 못했습니다.");
    state.catalog = await r.json();
    $("version").textContent =
      "v" + state.catalog.version + " · ABI " + state.catalog.abi_version;
    const groups = new Map();
    state.catalog.samples.forEach((s) => {
      if (!groups.has(s.category)) {
        const g = node("optgroup");
        g.label = s.category;
        groups.set(s.category, g);
        $("sample").append(g);
      }
      const o = node("option", s.title);
      o.value = s.id;
      groups.get(s.category).append(o);
    });
    state.catalog.corpora.forEach((c) => {
      const o = node("option", `${c.title} · ${c.count}건`);
      o.value = c.id;
      $("corpus").append(o);
    });
    loadSample(state.catalog.samples[0]);
    const audit = state.catalog.reference_summary;
    $("reference-summary").textContent =
      `저장소 기준 ${audit.cases}건 · 독립 검토 대기 ${audit.pending_independent_review}건 · 독립 검토 완료 ${audit.human_approved_cases}건`;
    corpusReview();
    $("run").disabled = false;
    inspect();
  } catch (e) {
    message(e.message, true);
    $("version").textContent = "연결 실패";
  }
})();
