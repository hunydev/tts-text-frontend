"use strict";
const $ = (id) => document.getElementById(id);
const node = (tag, text, cls) => {
  const el = document.createElement(tag);
  if (text !== undefined && text !== null) el.textContent = text;
  if (cls) el.className = cls;
  return el;
};
const number = (v) => Number(v).toLocaleString("ko-KR");
const LEVEL = { full: "지원", partial: "일부", none: "미지원" };
const ROUTE = { implemented: "구현됨", planned: "계획" };

function cards(data, tags) {
  const box = $("cards");
  for (const tag of tags) {
    const info = data.support.languages[tag] || { name: tag, status: "정보 없음", activation: "—", evidence: [] };
    const flags = data.flags[tag] || {};
    const res = data.resources[tag] || { total: 0 };
    const con = data.contracts[tag] || { total: 0, files: 0 };
    const pack = data.packs.find((p) => p.tag === tag) || { lines: 0, files: [] };
    const card = node("article", null, "lang-card" + (flags.declared_only === "1" ? " declared" : ""));
    const h = node("h3", info.name);
    h.append(node("span", tag, "tag"));
    card.append(h, node("span", info.status, "badge"));
    const dl = node("dl");
    const add = (k, v) => dl.append(node("dt", k), node("dd", v));
    add("동작", info.activation);
    add("판별 근거", (info.evidence || []).join(" · ") || "—");
    add("다른 문자", flags.absorb_foreign === "0" ? "그대로 둠 (흡수 안 함)" : "자기 말로 읽음 (흡수)");
    add("부착 문법", flags.attached_grammar === "1" ? "선언 (조사·어미 증거)" : "없음");
    add("리소스", `${number(res.total)}행 · ${number(res.kinds || 0)}종`);
    add("계약", `${number(con.total)}건 · ${number(con.files)}개 파일`);
    add("팩 코드", `${pack.files.join(", ")} · ${number(pack.lines)}줄`);
    card.append(dl);
    box.append(card);
  }
  for (const plan of data.support.planned || []) {
    const card = node("article", null, "lang-card planned");
    const h = node("h3", plan.name);
    h.append(node("span", plan.tag, "tag"));
    card.append(h, node("span", "계획", "badge muted"), node("p", plan.note, "small"));
    box.append(card);
  }
}

function routing(data) {
  const list = $("routing");
  for (const rule of data.support.routing) {
    const li = node("li");
    li.append(node("b", rule.rule + " "), node("span", ROUTE[rule.status] || rule.status, "lvl " + rule.status));
    li.append(node("span", rule.detail, "detail"));
    list.append(li);
  }
}

function scope(data, tags) {
  const box = $("scope");
  for (const tag of tags) {
    const info = data.support.languages[tag];
    if (!info) continue;
    const panel = node("div");
    panel.append(node("h3", `${info.name} (${tag})`));
    const table = node("table");
    const body = node("tbody");
    for (const row of info.scope || []) {
      const tr = node("tr");
      const level = node("td");
      level.append(node("span", LEVEL[row.level] || row.level, "lvl " + row.level));
      tr.append(node("td", row.area), level, node("td", row.note));
      body.append(tr);
    }
    table.append(body);
    panel.append(table);
    if ((info.gaps || []).length) panel.append(node("p", "한계·다음: " + info.gaps.join(" · "), "small"));
    if ((info.docs || []).length) panel.append(node("p", "문서: " + info.docs.join(", "), "small"));
    box.append(panel);
  }
}

function matrix(headId, bodyId, items, results, tags, label) {
  const head = node("tr");
  head.append(node("th", label), node("th", "입력"));
  for (const tag of tags) head.append(node("th", tag));
  $(headId).append(head);
  for (const item of items) {
    const tr = node("tr");
    const name = node("td", item.area || item.name, "area");
    if (item.only) name.append(node("span", item.only.join(", ") + " 전용", "only"));
    tr.append(name);
    const inputs = new Set(tags.map((t) => (item.texts || {})[t] || item.text));
    tr.append(node("td", inputs.size === 1 ? item.text : "언어별 문장 (칸 아래)", "input"));
    for (const tag of tags) {
      const r = (results[tag] || {})[item.id];
      let cell;
      if (item.only && !item.only.includes(tag)) {
        cell = node("td", "—", "out same");
        cell.title = item.only.join(", ") + " 전용 제어";
      } else if (!r) cell = node("td", "—", "out same");
      else if (r.error) {
        cell = node("td", label === "제어" ? "미지원" : "오류", "out err");
        cell.title = r.error;
      } else {
        cell = node("td", r.output, "out" + (r.unchanged ? " same" : ""));
        if (inputs.size > 1) cell.append(node("span", r.input, "cell-input"));
      }
      tr.append(cell);
    }
    $(bodyId).append(tr);
  }
}

function resources(data, tags) {
  const box = $("resources");
  for (const tag of tags) {
    const res = data.resources[tag];
    const con = data.contracts[tag];
    const panel = node("div");
    panel.append(node("h3", tag));
    panel.append(node("p", `리소스 ${number(res.total)}행, 파일 ${res.files.length}개`, "small"));
    panel.append(node("div", "많은 종류: " + res.top.map(([k, n]) => `${k} ${number(n)}`).join(" · "), "kinds"));
    panel.append(node("div", "계약 파일(건수 순): " + con.top.map(([k, n]) => `${k} ${n}`).join(" · "), "kinds"));
    box.append(panel);
  }
  const shared = data.resources["*"];
  if (shared) $("shared").textContent = `${number(shared.total)}행 · ` + shared.top.map(([k, n]) => `${k} ${number(n)}`).join(" · ");
}

async function main() {
  try {
    const response = await fetch("/api/languages");
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || response.statusText);
    const tags = data.packs.map((p) => p.tag);
    $("provenance").textContent = `기준: ${data.head || "알 수 없음"} · 정리 자료 갱신 ${data.support.updated}. ${data.support.note}`;
    cards(data, tags);
    routing(data);
    scope(data, tags);
    matrix("samples-head", "samples", data.support.samples, data.samples, tags, "영역");
    matrix("controls-head", "controls", data.support.controls, data.controls, tags, "제어");
    resources(data, tags);
    $("notice").textContent = "";
  } catch (error) {
    $("notice").textContent = "불러오지 못했습니다: " + error.message;
    $("notice").className = "error";
  }
}
main();
