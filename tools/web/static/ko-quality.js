"use strict";
const $ = id => document.getElementById(id);
const names = {pattern:"패턴 후보",lexicon:"사전 후보","context-control":"문맥·명시 제어 후보",review:"검토 필요",exact:"정확 일치",spacing:"공백만 다름",variant:"정책 변이만 다름",different:"공백·정책 변이 외 차이",unaligned:"정렬 실패",unscorable:"미채점",error:"엔진 오류"};
const state = {reports:[],data:null,baseline:null,page:0,detail:null,sequence:0,runSequence:0,detailSequence:0};
const number = v => Number(v).toLocaleString("ko-KR");
const percent = v => v===null || v===undefined ? "—" : (v*100).toFixed(2)+"%";
function node(tag,text){const e=document.createElement(tag);if(text!==undefined)e.textContent=text;return e;}
function message(text,error=false){$("notice").textContent=text;$("notice").classList.toggle("error",error);}
async function get(path,args={}){const r=await fetch(path+"?"+new URLSearchParams(args));const d=await r.json();if(!r.ok)throw Error(d.error||"요청 실패");return d;}
function safely(fn){return (...args)=>Promise.resolve().then(()=>fn(...args)).catch(e=>message(e.message,true));}
function download(name,data){const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)+"\n"],{type:"application/json"}));const a=node("a");a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
function option(value,text){const e=node("option",text);e.value=value;return e;}
function card(title,value,detail){const e=node("div");e.className="quality-card";e.append(node("span",title),node("strong",value),node("small",detail));return e;}
function bar(m){const e=node("div");e.className="bar";e.title=`정확 ${percent(m.exact_rate)} / 공백 제거 ${percent(m.compact_rate)}`;for(const [kind,v] of [["exact",m.exact_rate||0],["spacing",m.scorable?m.spacing_only/m.scorable:0]]){const x=node("span");x.className=kind;x.style.width=(v*100)+"%";e.append(x);}return e;}
function delta(current,old){if(!old)return "—";const one=(a,b)=>a===null||b===null?"—":((a-b)*100>=0?"+":"")+((a-b)*100).toFixed(2)+"pp";return `정확 ${one(current.exact_rate,old.exact_rate)} / 공백 ${one(current.compact_rate,old.compact_rate)}`+(current.variant_rate!==undefined&&old.variant_rate!==undefined?` / 변이 ${one(current.variant_rate,old.variant_rate)}`:"");}
function render(){
 const r=state.data,m=r.sentences;$("cards").replaceChildren(
  card("정확 일치",percent(m.exact_rate),`${number(m.exact)} / ${number(m.scorable)}문장`),
  card("공백 제거 일치",percent(m.compact_rate),`${number(m.compact)} / ${number(m.scorable)}문장`),
  ...(m.variant_rate!==undefined?[card("정책 변이 무시 일치",percent(m.variant_rate),`${number(m.variant)} / ${number(m.scorable)}문장 · 영/공·숫자 사이 구분자`)]:[card("공백만 다른 문장",number(m.spacing_only),"정확 불일치 중 공백 제거 후 일치")]),
  card("전체 입력",number(m.total),`미채점 ${number(m.unscorable)} · 엔진 오류 ${number(m.errors)}`));
 $("provenance").textContent=`${r.created_at} · 원본 ${r.source.split} · 엔진 ${r.engine.library_sha256.slice(0,12)} · HEAD ${r.engine.git_head.slice(0,7)}${r.engine.git_dirty?" + 작업 중 변경":""}`;
 const comparable=state.baseline&&state.baseline.comparison_key===r.comparison_key;
 $("compare-note").textContent=state.baseline?(comparable?`이전 측정 대비: ${delta(m,state.baseline.sentences)}. 같은 입력·설정·평가기·검토 파일 기준입니다.`:"비교 불가: 데이터·설정·평가기 또는 검토 파일이 다릅니다. 증감 수치를 만들지 않습니다."):"같은 기준의 이전 기록을 선택하면 카테고리별 증감을 표시합니다. 첫 기록은 기준선이며 개선량이 아닙니다.";
 $("categories").replaceChildren();
 for(const c of r.categories){const tr=node("tr"),td=node("td"),button=node("button",c.name);button.className="text-button";button.addEventListener("click",safely(()=>{$("category").value=c.id;state.page=0;return loadCases();}));td.append(button,bar(c.sentences));const old=comparable?state.baseline.categories.find(x=>x.id===c.id)?.sentences:null;
 tr.append(td,node("td",number(c.sentences.scorable)),node("td",percent(c.sentences.exact_rate)),node("td",percent(c.sentences.compact_rate)),node("td",c.sentences.variant_rate!==undefined?percent(c.sentences.variant_rate):number(c.sentences.spacing_only)),node("td",`${number(c.sentences.unscorable)} / ${number(c.sentences.errors)}`),node("td",number(c.flagged)),node("td",delta(c.sentences,old)));$("categories").append(tr);}
 const ul=node("ul");
 for(const text of [
  `원본 ${number(r.source.counts.source_records)}건 → 가져온 문장 ${number(r.source.counts.imported)}건. 구조 오류 격리 ${number(r.source.counts.quarantined||0)}건. 미채점 문장도 전체 입력 수에 남습니다.`,
  `구조 검토 신호가 있는 문장 ${number(r.source.counts.flagged_records)}건. 자동 경고이며 정답 오류 확정이 아닙니다.`,
  `구조 검사를 통과한 문장: ${number(r.structurally_clean_sentences.scorable)}건, 정확 ${percent(r.structurally_clean_sentences.exact_rate)}, 공백 제거 ${percent(r.structurally_clean_sentences.compact_rate)}. 언어 검수를 통과했다는 뜻은 아닙니다.`,
  `공백을 제거한 입력·참조 쌍 중복 제거: ${number(r.unique_pair_sentences.scorable)}건, 정확 ${percent(r.unique_pair_sentences.exact_rate)}, 공백 제거 ${percent(r.unique_pair_sentences.compact_rate)}. 원본 중복 입력 ${number(r.source.duplicate_input_records)}건, 상충 입력 그룹 ${number(r.source.conflicting_input_groups)}개.`,
  `별도 검토 파일의 승인·정정 문장: ${number(r.reviewed_sentences.scorable)}건, 정확 ${percent(r.reviewed_sentences.exact_rate)}, 공백 제거 ${percent(r.reviewed_sentences.compact_rate)}. 검토 파일을 적용해도 위의 원본 기준 점수는 바뀌지 않습니다.`,
  ...(r.adjusted_sentences && r.review_counts && Object.keys(r.review_counts).length ? [`참조 오류 제외·정정 반영 점수: ${number(r.adjusted_sentences.scorable)}문장, 정확 ${percent(r.adjusted_sentences.exact_rate)}, 공백 제거 ${percent(r.adjusted_sentences.compact_rate)} (제외 ${number(r.review_counts.excluded||0)}, 정정 ${number(r.review_counts.corrected||0)}).`] : []),
  "공백 제거는 Unicode 공백 문자만 제거합니다. 대소문자·기호·숫자 값은 그대로 비교합니다. 의미 정확도나 발음 허용형까지 판정하지 않습니다.",
  "정책 변이 무시는 공백 제거에 더해, 숫자 읽기 안에서만 영↔공, 숫자 사이 구분자 다시/에, 맨 앞 일만/만을 참조와 출력 양쪽에서 똑같이 접습니다. 사람마다 갈리는 표기 관행을 오류로 세지 않기 위한 보조 지표입니다. 고유어 숫자(하나·둘)나 서른/삼십 같은 읽기 선택은 접지 않습니다."]){ul.append(node("li",text));}
 for(const [flag,count] of Object.entries(r.flags))ul.append(node("li",`${flag}: ${number(count)}건`));$("audit").replaceChildren(ul);
 $("protocol").textContent=JSON.stringify({protocol:r.protocol,engine:r.engine,harness:r.harness,review_counts:r.review_counts,policy:r.policy},null,2);
 renderGroups();
}
function scopeOptions(){const span=$("scope").value==="span";$("theme").disabled=!span;
 if(!span)$("theme").value="";
 for(const o of $("status").options){o.disabled=span?["error","unscorable"].includes(o.value):o.value==="unaligned";}
 if($("status").selectedOptions[0]?.disabled)$("status").value="different";
}
function renderGroups(){const r=state.data;$("span-note").textContent=`전체 참조 구간 ${number(r.spans.total)}개, 엔진 출력과 경계 정렬 실패 ${number(r.spans.unaligned)}개. 정렬 실패를 분모에서 빼지 않습니다. 구간 점수와 문장 점수는 서로 다른 지표입니다.`;$("groups").replaceChildren();
 for(const g of r[$("group").value]){const m=g.spans,tr=node("tr");tr.append(node("td",names[g.name]||g.name),node("td",number(m.scorable)),node("td",percent(m.exact_rate)),node("td",percent(m.compact_rate)),node("td",number(m.unaligned)));$("groups").append(tr);}}
function renderSpacing(){
 const all=state.spacing;if(!all)return;
 const audit=(all.categories||[]).find(c=>c.id===$("spacing-category").value)||all;
 $("spacing-groups").replaceChildren();
 $("spacing-note").textContent=`${audit.name||"전체"} · 공백만 다른 ${number(audit.sentences)}문장 · 서로 다른 공백 경계 ${number(audit.boundaries)}곳. 유형별 문장 수는 중복될 수 있습니다.`;
 for(const group of audit.groups){
  const row=node("tr"),samples=node("td");
  for(const [index,event] of group.examples.entries()){
   const button=node("button",`사례 ${index+1}`);button.className="text-button";
   button.title=`${event.left} | ${event.right}`;button.setAttribute("aria-haspopup","dialog");button.setAttribute("aria-controls","detail");
   button.addEventListener("click",safely(()=>detail(event.id)));samples.append(button,node("br"));
  }
  row.append(node("td",group.title),node("td",number(group.boundaries)),node("td",number(group.sentences)),node("td",group.guidance),samples);
  $("spacing-groups").append(row);
 }
}
async function loadSpacing(run){
 state.spacing=null;$("spacing-groups").replaceChildren();$("spacing-note").textContent="공백 진단을 불러오는 중입니다.";
 $("spacing-category").replaceChildren(option("","전체"));$("spacing-category").disabled=true;
 try {
  const audit=await get("/api/ko-spacing",{run});
  if(state.data.run_id!==run)return;
  if(!audit.available){$("spacing-note").textContent="이 측정에는 별도 공백 진단이 아직 없습니다.";return;}
  state.spacing=audit;
  $("spacing-category").append(...(audit.categories||[]).map(c=>option(c.id,c.name)));
  $("spacing-category").disabled=!(audit.categories||[]).length;renderSpacing();
 } catch(error){if(state.data.run_id===run)$("spacing-note").textContent=`공백 진단을 확인할 수 없습니다: ${error.message}`;}
}
$("spacing-category").addEventListener("change",renderSpacing);
async function loadRun(){const seq=++state.runSequence;const run=$("run").value;if(!run)return;message("측정 결과를 읽고 있습니다.");const r=await get("/api/ko-report",{run});if(seq!==state.runSequence)return;state.data=r;state.detail=null;state.baseline=null;closeDetail();$("detail-view").replaceChildren();state.page=0;
 $("category").replaceChildren(option("","전체"),...r.categories.map(c=>option(c.id,c.name)));
 $("theme").replaceChildren(option("","전체"),...r.themes.map(g=>option(g.name,g.name)));scopeOptions();
 $("baseline").replaceChildren(option("","비교 안 함"),...state.reports.filter(x=>x.run_id!==run).map(x=>option(x.run_id,x.label+(x.comparison_key===r.comparison_key?"":" · 기준 다름"))));render();await Promise.all([loadCases(),loadSpacing(run)]);if(seq===state.runSequence)message("완료된 전체 측정입니다. 정답 검토와 수정 방향은 별도로 확인하세요.");}
async function loadCases(){if(!state.data)return;scopeOptions();const seq=++state.sequence;const args={run:state.data.run_id,scope:$("scope").value,category:$("category").value,route:$("route").value,status:$("status").value,page:state.page,q:$("query").value,theme:$("theme").value};if($("flagged").checked)args.flagged="1";
 const r=await get("/api/ko-cases",args);if(seq!==state.sequence||state.data.run_id!==args.run)return;$("cases").replaceChildren();
 for(const c of r.entries){const s=c.selected_span;const tr=node("tr"),td=node("td"),b=node("button",s?s.written:c.text);b.className="text-button";b.setAttribute("aria-haspopup","dialog");b.setAttribute("aria-controls","detail");tr.addEventListener("click",safely(e=>{if(e.target.closest("button")||!window.getSelection()?.toString()){b.focus({preventScroll:true});return detail(c.id,s);}}));td.append(b);tr.append(td,node("td",(s?s.spoken:c.expected)??"참조 미채점"),node("td",(s?s.actual:c.actual)??"정렬 실패 또는 엔진 오류"),node("td",`${names[s?s.status:c.status]||c.status} · ${names[s?s.route:c.route]}${c.flags.length?" · 검토 신호 있음":""}`));$("cases").append(tr);}
 $("case-count").textContent=`${number(r.total)}건 중 ${r.entries.length?number(r.page*50+1):0}–${number(r.page*50+r.entries.length)} 표시`;
 $("page").textContent=`${r.page+1} / ${Math.max(1,Math.ceil(r.total/50))}`;$("prev").disabled=r.page===0;$("next").disabled=(r.page+1)*50>=r.total;}
function detailField(box,title,text){const section=node("section"),value=node("p",text??"미채점 / 출력 없음");value.className="case-full-text";section.append(node("h3",title),value);box.append(section);}
async function detail(id,selectedSpan){
 const run=state.data.run_id,seq=++state.detailSequence,dialog=$("detail"),box=$("detail-view");
 state.detail=null;$("review-export").disabled=true;$("detail-meta").textContent=id;
 box.replaceChildren(node("p","사례 전체 내용을 불러오는 중입니다."));box.scrollTop=0;
 if(!dialog.open)dialog.showModal();document.body.classList.add("case-dialog-open");
 try {
  const r=await get("/api/ko-cases",{run,id});
  if(seq!==state.detailSequence||state.data.run_id!==run||!dialog.open)return;
  const d=r.entries[0];if(!d)throw Error("사례를 찾을 수 없습니다.");
  state.detail=d;$("review-export").disabled=false;box.replaceChildren();
  $("detail-meta").textContent=`${d.category||d.category_en||""} · ${names[d.status]||d.status} · ${names[d.route]||d.route}\n${d.id}`;
  if(selectedSpan){const selected=node("section");selected.className="case-selected-span";selected.append(node("h3","선택한 참조 구간"));
   for(const [title,text] of [["구간 원문",selectedSpan.written],["구간 참조",selectedSpan.spoken],["구간 실제 출력",selectedSpan.actual??(selectedSpan.alignment_issue==="engine-error"?"엔진 오류로 출력이 없습니다.":"정렬 실패로 문장 출력에서 이 구간을 분리할 수 없습니다.")]])detailField(selected,title,text);
   selected.append(node("p",`원본 주제: ${selectedSpan.theme} · ${names[selectedSpan.status]||selectedSpan.status}`));box.append(selected);}
  for(const [title,text] of [["원문 전체",d.text],["제공 참조 전체 (주석 원본)",d.annotated_tn],["비교용 참조 전체",d.expected],["실제 출력 전체",d.actual]])detailField(box,title,text);
  const trace=node("details");trace.append(node("summary","검토 신호·구간별 판정·엔진 IR"),node("pre",JSON.stringify({id:d.id,flags:d.flags,error:d.error,route:d.route,route_candidates:d.route_candidates,review:d.review,spans:d.spans,engine_spans:d.engine_spans},null,2)));box.append(trace);
 } catch(e){if(seq===state.detailSequence&&dialog.open){const error=node("p",e.message);error.setAttribute("role","alert");box.replaceChildren(error);}}
}
// Invalidate pending requests synchronously. A native close event is queued and
// can arrive after the same case has already been reopened with the keyboard.
function closeDetail(){state.detailSequence++;document.body.classList.remove("case-dialog-open");$("detail").close();}
$("detail-close").addEventListener("click",closeDetail);
$("detail").addEventListener("cancel",e=>{e.preventDefault();closeDetail();});
let backdropPress=false;
$("detail").addEventListener("pointerdown",e=>{backdropPress=e.target===$("detail");});
$("detail").addEventListener("click",e=>{if(backdropPress&&e.target===$("detail"))closeDetail();backdropPress=false;});
$("run").addEventListener("change",safely(loadRun));
$("baseline").addEventListener("change",safely(async()=>{const id=$("baseline").value,run=state.data.run_id;const r=id?await get("/api/ko-report",{run:id}):null;if(state.data.run_id!==run||$("baseline").value!==id)return;state.baseline=r;render();}));
$("group").addEventListener("change",()=>state.data&&renderGroups());
$("search").addEventListener("click",safely(()=>{state.page=0;return loadCases();}));
$("query").addEventListener("keydown",safely(e=>{if(e.key==="Enter"){state.page=0;return loadCases();}}));
for(const id of ["scope","category","theme","route","status","flagged"])$(id).addEventListener("change",safely(()=>{state.page=0;return loadCases();}));
$("prev").addEventListener("click",safely(()=>{state.page=Math.max(0,state.page-1);return loadCases();}));$("next").addEventListener("click",safely(()=>{state.page++;return loadCases();}));
$("download").addEventListener("click",()=>state.data&&download(state.data.run_id+".json",state.data));
$("review-export").addEventListener("click",()=>{const d=state.detail;if(d)download("review-proposal.json",{id:d.id,record_sha256:d.record_sha256,status:"needs-review",reviewer:"",reviewed_at:"",reason:"",route:d.route,expected:d.expected});});
safely(async()=>{const r=await get("/api/ko-reports");state.reports=r.reports;$("run").replaceChildren(...r.reports.map(x=>option(x.run_id,x.label)));if(!r.latest){message("아직 완료된 한국어 참조 보고서가 없습니다. 오프라인 평가가 끝나면 이 페이지에 표시됩니다.");return;}$("run").value=r.latest;await loadRun();})();
