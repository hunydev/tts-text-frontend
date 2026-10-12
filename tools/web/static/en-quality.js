"use strict";
const $ = id => document.getElementById(id);
const names = {exact:"정확 일치",compact:"공백 무시 일치",loose:"대소문자·구두점 무시 일치",variant:"철자 변형 무시 일치",miss:"불일치",rejected:"알려진 오답과 같음",error:"엔진 오류","out-of-scope":"범위 밖","jev-pass":"불일치 · Jev 인정","final-wrong":"최종 틀림 (Jev까지)"};
const datasetNames = {"nemo-googletn":"GoogleTN (NeMo)","nemo-libritts":"LibriTTS (NeMo)","nemo-engconf":"EngConf (NeMo, 문맥 모호)","sproat":"Sproat 표본"};
const modes = {speech:"speech 출력",text:"text 출력"};
const state = {reports:[],data:null,baseline:null,page:0,sequence:0,runSequence:0,detailSequence:0};
const number = v => Number(v).toLocaleString("ko-KR");
const percent = v => v===null || v===undefined ? "—" : (v*100).toFixed(2)+"%";
function node(tag,text){const e=document.createElement(tag);if(text!==undefined)e.textContent=text;return e;}
function message(text,error=false){$("notice").textContent=text;$("notice").classList.toggle("error",error);}
async function get(path,args={}){const r=await fetch(path+"?"+new URLSearchParams(args));const d=await r.json();if(!r.ok)throw Error(d.error||"요청 실패");return d;}
function safely(fn){return (...args)=>Promise.resolve().then(()=>fn(...args)).catch(e=>message(e.message,true));}
function download(name,data){const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)+"\n"],{type:"application/json"}));const a=node("a");a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
function option(value,text){const e=node("option",text);e.value=value;return e;}
function card(title,value,detail){const e=node("div");e.className="quality-card";e.append(node("span",title),node("strong",value),node("small",detail));return e;}
// Benchmark levels, then the misses Jev accepts (black); gray is wrong even for Jev.
function bar(m,j,wide){const e=node("div");e.className="bar"+(wide?" wide":"");
 const parts=[["exact",m.exact_rate||0],["spacing",(m.compact_rate||0)-(m.exact_rate||0)],["seg-loose",(m.loose_rate||0)-(m.compact_rate||0)],["similar",(m.variant_rate||0)-(m.loose_rate||0)],["seg-jev",j&&j.accepted_rate?j.accepted_rate:0]];
 e.title=`정확 ${percent(m.exact_rate)} / 공백 무시 ${percent(m.compact_rate)} / 대소문자·구두점 무시 ${percent(m.loose_rate)} / 철자 변형 무시 ${percent(m.variant_rate)}`+(j?` / Jev 인정 +${percent(j.accepted_rate)} / Jev까지 틀림 ${percent(j.final_wrong_rate)}`:" / Jev 미실행");
 for(const [kind,v] of parts){const x=node("span");x.className=kind;x.style.width=(Math.max(0,v)*100)+"%";e.append(x);}return e;}
function jevVerdict(c){const t=state.data.jev?state.data.jev.threshold:0.7;return c.jev===undefined?"":` · Jev ${c.jev.toFixed(2)} ${c.jev>=t?"인정":"불인정"}`;}
function comparable(){return state.baseline&&state.baseline.comparison_key===state.data.comparison_key;}
function delta(current,old){if(!old)return "—";const d=(current.variant_rate-old.variant_rate)*100;return (d>=0?"+":"")+d.toFixed(2)+"pp";}
function render(){
 const r=state.data,m=r.sentences,mode=r.protocol.output_mode;
 const j=r.jev;
 $("cards").replaceChildren(
  card("철자 변형 무시 일치",percent(m.variant_rate),`${number(m.variant)} / ${number(m.scorable)}문장 · 주 지표`),
  card("Jev까지 틀림",j?percent(j.sentences.final_wrong_rate):"—",j?`${number(j.sentences.final_wrong)}문장 · 불일치 ${number(j.sentences.misses)} 중 Jev 인정 ${number(j.sentences.accepted)}`:"Jev 단계를 실행하지 않은 측정"),
  card("대소문자·구두점 무시",percent(m.loose_rate),`${number(m.loose)}문장`),
  card("정확 일치",percent(m.exact_rate),`${number(m.exact)}문장`),
  card("채점 제외",number(m.out_of_scope),`범위 밖 문장 · 엔진 오류 ${number(m.errors)}`));
 $("overall-bar").replaceChildren(bar(m,j&&j.sentences,true));
 $("provenance").textContent=`${modes[mode]||mode} · ${r.created_at} · 엔진 ${r.engine.library_sha256.slice(0,12)} · 범위 ${r.protocol.scope}`;
 const ok=comparable();
 $("compare-note").textContent=state.baseline?(ok?`이전 측정 대비 철자 변형 무시 일치 ${delta(m,state.baseline.sentences)}. 같은 자료·출력 모드·평가기 기준입니다. 아래 사례에서 새로 맞음/새로 틀림을 볼 수 있습니다.`:"비교 불가: 자료·출력 모드·평가기가 다릅니다. 증감 수치를 만들지 않습니다."):"같은 출력 모드의 이전 기록을 선택하면 자료별 증감과 새로 맞음/틀림 문장을 볼 수 있습니다.";
 $("change").disabled=!ok;if(!ok)$("change").value="";
 $("datasets").replaceChildren();
 for(const [id,d] of Object.entries(r.datasets)){const tr=node("tr"),td=node("td"),b=node("button",datasetNames[id]||id);b.className="text-button";b.addEventListener("click",safely(()=>{$("dataset").value=id;state.page=0;return loadCases();}));const dj=j?j.datasets[id]:null;td.append(b,bar(d,dj));
  const old=ok?state.baseline.datasets[id]:null;
  tr.append(td,node("td",number(d.scorable)),node("td",percent(d.exact_rate)),node("td",percent(d.compact_rate)),node("td",percent(d.loose_rate)),node("td",percent(d.variant_rate)),node("td",dj?`${percent(dj.final_wrong_rate)} (Jev 인정 ${number(dj.accepted)})`:"—"),node("td",`${number(d.out_of_scope)} / ${number(d.errors)}`),node("td",delta(d,old)));$("datasets").append(tr);}
 const ul=node("ul");
 const scripts=Object.entries(r.out_of_scope_scripts||{}).map(([k,v])=>`${k} ${number(v)}`).join(", ")||"없음";
 for(const text of [
  `전체 ${number(m.sentences)}문장 중 채점 ${number(m.scorable)}문장. 다른 문자(${scripts})가 섞인 ${number(m.out_of_scope)}문장은 영어 TN 채점 대상이 아니어서 분모에서 뺐습니다.`,
  "라틴 문자(발음 구별 부호 포함), 숫자, 기호, 수식 기호로 쓴 한 글자 그리스 문자는 범위 안입니다.",
  "단계: 정확 일치 ⊂ 공백 무시 ⊂ 대소문자·구두점 무시 ⊂ 철자 변형 무시. 숫자 읽기나 단어 선택은 어떤 단계에서도 접지 않습니다.",
  "철자 변형 무시는 GoogleTN·Sproat 참조가 영국식 철자를 미국식으로 바꾸는(centre → center) 참조 정책을 감점하지 않기 위한 단계입니다.",
  `EngConf처럼 허용 읽기가 여러 개인 문장은 어느 하나와 맞으면 일치입니다. 알려진 오답과 같은 출력: ${number(m.rejected)}문장.`]){ul.append(node("li",text));}
 $("audit").replaceChildren(ul);
 $("jev-about").replaceChildren(node("p",j?`벤치마크(철자 변형 무시 단계까지)에서 맞았다고 나오지 않은 불일치 ${number(j.sentences.misses)}문장만 Jev에 물었습니다. Jev 점수 ${j.threshold} 이상이면 인정(검정)하고, 나머지와 알려진 오답·엔진 오류는 Jev까지 틀림(회색)입니다. 모델 ${j.model||"—"} · 질문 ${j.question||"—"} · ${j.scored_at||""}. 영어 Jev 점수는 보정하지 않았고 Jev가 실제 오류를 인정하는 경우도 있어, Jev까지 틀림은 실제 오류율의 하한입니다.`:"이 측정에는 Jev 단계가 없습니다. 실행: python3 tools/web/en_jev.py "+r.run_id));
 $("protocol").textContent=JSON.stringify({protocol:r.protocol,engine:r.engine,policy:r.policy},null,2);
}
async function loadRun(){const seq=++state.runSequence;const run=$("run").value;if(!run)return;message("측정 결과를 읽고 있습니다.");const r=await get("/api/en-report",{run});if(seq!==state.runSequence)return;
 state.data=r;state.baseline=null;state.page=0;closeDetail();
 $("dataset").replaceChildren(option("","전체"),...Object.keys(r.datasets).map(id=>option(id,datasetNames[id]||id)));
 const same=state.reports.filter(x=>x.run_id!==run&&x.comparison_key===r.comparison_key);
 $("baseline").replaceChildren(option("","비교 안 함"),...state.reports.filter(x=>x.run_id!==run).map(x=>option(x.run_id,x.label+(x.comparison_key===r.comparison_key?"":" · 기준 다름"))));
 if(same.length){$("baseline").value=same[0].run_id;state.baseline=await get("/api/en-report",{run:same[0].run_id});if(seq!==state.runSequence)return;}
 render();await loadCases();if(seq===state.runSequence)message("완료된 전체 측정입니다.");}
async function loadCases(){if(!state.data)return;const seq=++state.sequence;
 const args={run:state.data.run_id,dataset:$("dataset").value,result:$("result").value,page:state.page,q:$("query").value};
 if($("change").value&&comparable()){args.change=$("change").value;args.baseline=state.baseline.run_id;}
 const r=await get("/api/en-cases",args);if(seq!==state.sequence||state.data.run_id!==args.run)return;$("cases").replaceChildren();
 for(const c of r.entries){const tr=node("tr"),td=node("td"),b=node("button",c.text);b.className="text-button";b.setAttribute("aria-haspopup","dialog");b.setAttribute("aria-controls","detail");
  tr.addEventListener("click",safely(e=>{if(e.target.closest("button")||!window.getSelection()?.toString()){b.focus({preventScroll:true});return detail(c);}}));td.append(b);
  const verdict=c.result==="out-of-scope"?`범위 밖 · ${(c.scripts||[]).join(", ")}`:(names[c.result]||c.result)+(c.before_result?` (이전: ${names[c.before_result]||c.before_result})`:"");
  tr.append(td,node("td",c.references?c.references[0]:"채점 안 함"),node("td",c.actual??(c.error||"—")),node("td",`${verdict}${jevVerdict(c)} · ${datasetNames[c.dataset]||c.dataset}`));$("cases").append(tr);}
 $("case-count").textContent=`${number(r.total)}건 중 ${r.entries.length?number(r.page*50+1):0}–${number(r.page*50+r.entries.length)} 표시`;
 $("page").textContent=`${r.page+1} / ${Math.max(1,Math.ceil(r.total/50))}`;$("prev").disabled=r.page===0;$("next").disabled=(r.page+1)*50>=r.total;}
function detailField(box,title,text){const section=node("section"),value=node("p",text??"출력 없음");value.className="case-full-text";section.append(node("h3",title),value);box.append(section);}
function detail(c){
 const dialog=$("detail"),box=$("detail-view");state.detailSequence++;box.replaceChildren();box.scrollTop=0;
 $("detail-meta").textContent=`${datasetNames[c.dataset]||c.dataset} · ${names[c.result]||c.result}${jevVerdict(c)}\n${c.id}`;
 detailField(box,"원문",c.text);
 if(c.references)c.references.forEach((ref,i)=>detailField(box,c.references.length>1?`허용 참조 ${i+1}`:"참조",ref));
 if(c.result!=="out-of-scope")detailField(box,"실제 출력",c.actual??c.error);
 if(c.before!==undefined)detailField(box,`이전 기록 출력 (${names[c.before_result]||c.before_result})`,c.before);
 if(c.scripts)box.append(node("p",`섞인 문자: ${c.scripts.join(", ")}. 영어 TN 채점에서 제외된 문장입니다.`));
 if(!dialog.open)dialog.showModal();document.body.classList.add("case-dialog-open");
}
function closeDetail(){state.detailSequence++;document.body.classList.remove("case-dialog-open");if($("detail").open)$("detail").close();}
$("detail-close").addEventListener("click",closeDetail);
$("detail").addEventListener("cancel",e=>{e.preventDefault();closeDetail();});
let backdropPress=false;
$("detail").addEventListener("pointerdown",e=>{backdropPress=e.target===$("detail");});
$("detail").addEventListener("click",e=>{if(backdropPress&&e.target===$("detail"))closeDetail();backdropPress=false;});
$("run").addEventListener("change",safely(loadRun));
$("baseline").addEventListener("change",safely(async()=>{const id=$("baseline").value,run=state.data.run_id;const r=id?await get("/api/en-report",{run:id}):null;if(state.data.run_id!==run||$("baseline").value!==id)return;state.baseline=r;render();state.page=0;await loadCases();}));
$("search").addEventListener("click",safely(()=>{state.page=0;return loadCases();}));
$("query").addEventListener("keydown",safely(e=>{if(e.key==="Enter"){state.page=0;return loadCases();}}));
for(const id of ["dataset","result"])$(id).addEventListener("change",safely(()=>{state.page=0;return loadCases();}));
// A newly matched sentence is not a miss: the change filter replaces the result filter.
$("change").addEventListener("change",safely(()=>{if($("change").value)$("result").value="";state.page=0;return loadCases();}));
$("prev").addEventListener("click",safely(()=>{state.page=Math.max(0,state.page-1);return loadCases();}));$("next").addEventListener("click",safely(()=>{state.page++;return loadCases();}));
$("download").addEventListener("click",()=>state.data&&download(state.data.run_id+".json",state.data));
safely(async()=>{const i=await get("/api/jev/instructions");const en=i.languages&&i.languages["en-US"];$("jev-request").textContent=JSON.stringify({endpoint:i.endpoint,pass_rule:i.pass_rule,request:en?en.request:null},null,2);})();
safely(async()=>{const r=await get("/api/en-reports");state.reports=r.reports;$("run").replaceChildren(...r.reports.map(x=>option(x.run_id,x.label)));if(!r.latest){message("아직 완료된 영어 보고서가 없습니다.");return;}$("run").value=r.latest;await loadRun();})();
