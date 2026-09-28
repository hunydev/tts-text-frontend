/* Optional real-browser gate against a completed local report; no data vendored. */
const { chromium } = require(process.env.TTF_PLAYWRIGHT_MODULE || "playwright");
const assert = require("node:assert/strict");
(async()=>{
 const base=process.argv[2]||"http://127.0.0.1:8000";
 const browser=await chromium.launch({headless:true});
 const page=await browser.newPage({viewport:{width:1440,height:1050}}),errors=[];
 page.on("pageerror",e=>errors.push(e.message));
 page.on("console",m=>{if(m.type()==="error")errors.push(m.text());});
 try {
  await page.goto(base+"/ko-quality");
  await page.waitForFunction(()=>document.querySelectorAll("#categories tr").length>0);
  const catalog=await (await page.request.get(base+"/api/ko-reports")).json();
  const report=await (await page.request.get(base+"/api/ko-report?run="+catalog.latest)).json();
  assert.equal(await page.locator("#categories tr").count(),report.categories.length);
  assert((await page.locator("#cards").textContent()).includes((report.sentences.compact_rate*100).toFixed(2)+"%"));
  const spacing=await (await page.request.get(base+"/api/ko-spacing?run="+catalog.latest)).json();
  if(spacing.available){
   await page.waitForFunction(()=>document.querySelectorAll('#spacing-groups tr').length===7);
   assert.equal(await page.locator('#spacing-groups tr').count(),spacing.groups.length);
   const first=spacing.groups.find(g=>g.examples.length).examples[0];
   await page.locator('#spacing-groups button').first().click();
   await page.waitForFunction(()=>document.querySelector('#detail').matches(':modal')&&document.querySelectorAll('#detail-view .case-full-text').length===4);
   assert((await page.locator('#detail-meta').textContent()).includes(first.id));
   await page.click('#detail-close');
   if(spacing.categories?.length){
    const cat=spacing.categories.find(c=>c.id==='age_birthdate')||spacing.categories[0];
    await page.selectOption('#spacing-category',cat.id);
    assert((await page.locator('#spacing-note').textContent()).includes(cat.name));
    const counts=await page.locator('#spacing-groups tr td:nth-child(2)').allTextContents();
    assert.deepEqual(counts,cat.groups.map(g=>g.boundaries.toLocaleString('ko-KR')));
    const example=cat.groups.find(g=>g.examples.length).examples[0];
    await page.locator('#spacing-groups button').first().click();
    await page.waitForFunction(()=>document.querySelector('#detail').matches(':modal')&&document.querySelectorAll('#detail-view .case-full-text').length===4);
    assert((await page.locator('#detail-meta').textContent()).includes(example.id));
    await page.click('#detail-close');await page.selectOption('#spacing-category','');
   }
  }
  await page.locator("#categories button").first().click();
  await page.selectOption("#status","spacing");
  await page.waitForFunction(()=>document.querySelectorAll("#cases tr").length>0);
  const detailResponse=page.waitForResponse(r=>r.url().includes('/api/ko-cases?')&&new URL(r.url()).searchParams.has('id'));
  // Clicking the output cell also opens the case, not just its source button.
  await page.locator("#cases tr").first().locator("td").nth(2).click();
  const full=(await (await detailResponse).json()).entries[0];
  await page.waitForFunction(()=>document.querySelector("#detail").matches(":modal")&&document.querySelectorAll("#detail-view .case-full-text").length===4);
  assert.deepEqual(await page.locator("#detail-view .case-full-text").allTextContents(),[full.text,full.annotated_tn,full.expected,full.actual]);
  assert((await page.locator("#detail-view").textContent()).includes("engine_rules"));
  const downloadPromise=page.waitForEvent("download");await page.click("#review-export");
  const download=await downloadPromise;assert.equal(download.suggestedFilename(),"review-proposal.json");
  await page.keyboard.press("Escape");
  await page.waitForFunction(()=>!document.querySelector("#detail").open);
  assert(await page.locator("#cases button").first().evaluate(e=>e===document.activeElement));
  await page.keyboard.press("Enter");
  await page.waitForFunction(()=>document.querySelector("#detail").matches(":modal")&&!document.querySelector("#review-export").disabled);
  await page.keyboard.press("Tab");
  assert(await page.evaluate(()=>document.querySelector('#detail').contains(document.activeElement)));
  await page.click("#detail-close");
  await page.locator("#cases button").first().click();
  await page.waitForFunction(()=>document.querySelector("#detail").matches(":modal"));
  await page.mouse.click(1,1);
  await page.waitForFunction(()=>!document.querySelector("#detail").open);
  await page.selectOption("#scope","span");await page.selectOption("#status","unaligned");
  await page.waitForTimeout(300);await page.selectOption("#group","themes");
  assert.equal(await page.locator("#groups tr").count(),report.themes.length);
  await page.selectOption("#category","");await page.selectOption("#status","");
  await page.selectOption("#theme",report.themes[0].name);
  await page.waitForFunction(()=>document.querySelectorAll("#cases tr").length>0);
  await page.locator("#cases button").first().click();
  await page.waitForFunction(()=>document.querySelector('#detail .case-selected-span'));
  await page.setViewportSize({width:390,height:844});
  const modalMobile=await page.locator('#detail').boundingBox();
  assert(modalMobile.x>=0&&modalMobile.y>=0&&modalMobile.x+modalMobile.width<=390&&modalMobile.y+modalMobile.height<=844);
  assert(await page.locator('#detail-view').evaluate(e=>e.scrollWidth<=e.clientWidth));
  await page.screenshot({path:process.env.TTF_KO_MODAL_SCREENSHOT||'/tmp/ttf-ko-quality-modal-mobile.png'});
  await page.click('#detail-close');await page.setViewportSize({width:1440,height:1050});
  // Mock historical summaries only in this test. Never write synthetic history
  // to the report directory or mix it into measured corpus results.
  await page.route("**/api/ko-report?run=fixture-*",route=>{
   const old=JSON.parse(JSON.stringify(report));
   if(route.request().url().includes("fixture-incompatible"))old.comparison_key="different-protocol";
   old.sentences.exact_rate=Math.max(0,old.sentences.exact_rate-.01);
   return route.fulfill({json:old});
  });
  await page.evaluate(()=>{for(const id of ["fixture-compatible","fixture-incompatible"]){const o=document.createElement("option");o.value=id;o.textContent=id;document.querySelector("#baseline").append(o);}});
  await page.selectOption("#baseline","fixture-compatible");
  await page.waitForFunction(()=>document.querySelector("#compare-note").textContent.includes("+1.00pp"));
  await page.selectOption("#baseline","fixture-incompatible");
  await page.waitForFunction(()=>document.querySelector("#compare-note").textContent.includes("비교 불가"));
  await page.selectOption("#baseline","");
  const summaryDownload=page.waitForEvent("download");await page.click("#download");assert((await summaryDownload).suggestedFilename().endsWith(".json"));
  await page.screenshot({path:process.env.TTF_KO_SCREENSHOT||"/tmp/ttf-ko-quality.png",fullPage:false});
  const longText='<img src=x onerror="window.bad=true">\n'+"긴내용".repeat(2500)+"\n마지막까지 표시";
  await page.route("**/api/ko-cases?**",r=>r.fulfill({json:{total:1,page:0,page_size:50,entries:[{id:"fixture",text:longText,annotated_tn:"[참조]",expected:"참조",actual:longText,status:"different",route:"review",flags:[]}]}}));
  await page.click("#search");await page.waitForTimeout(100);
  assert.equal(await page.locator("#cases img").count(),0);assert.equal(await page.evaluate(()=>window.bad),undefined);
  assert((await page.locator("#cases").textContent()).includes("<img"));
  await page.locator('#cases button').first().click();
  await page.waitForFunction(()=>document.querySelectorAll('#detail-view .case-full-text').length===4);
  assert.equal(await page.locator('#detail-view .case-full-text').last().textContent(),longText);
  assert.equal(await page.locator('#detail img').count(),0);
  await page.locator('#detail-view .case-full-text').last().evaluate(e=>e.scrollIntoView({block:'end'}));
  assert(await page.locator('#detail-view .case-full-text').last().evaluate(e=>{const r=e.getBoundingClientRect(),b=document.querySelector('#detail-view').getBoundingClientRect();return r.bottom<=b.bottom+1&&getComputedStyle(e).overflow==='visible';}));
  await page.click('#detail-close');
  await page.setViewportSize({width:390,height:844});await page.screenshot({path:"/tmp/ttf-ko-quality-mobile.png"});
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>window.innerWidth),false);
  assert.deepEqual(errors,[]);console.log("Korean report browser: metrics, filters, modal/full text, keyboard/backdrop/focus, exports, mobile and XSS text handling passed");
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
