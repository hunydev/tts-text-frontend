/* Optional browser gate: npm ci in tests/web; node browser.cjs [base URL]. */
const { chromium } = require(process.env.TTF_PLAYWRIGHT_MODULE || "playwright");
const assert = require("node:assert/strict");
const path = require("node:path");
(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1100 },
  });
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(process.argv[2] || "http://127.0.0.1:8000");
  await page.waitForFunction(() => !document.querySelector("#run").disabled);
  const run = async () => {
    await page.click("#run");
    await page.waitForFunction(
      () => !document.querySelector("#run").disabled,
      {},
      { timeout: 30000 },
    );
    assert(
      !(await page
        .locator("#notice")
        .evaluate((e) => e.classList.contains("error"))),
      await page.locator("#notice").textContent(),
    );
  };
  assert.equal(await page.inputValue("#source"), "1~2만 원");
  assert.equal(await page.inputValue("#expected"), "만 원에서 이만 원");
  assert.equal(await page.inputValue("#input-format"), "plain");
  assert.equal(await page.inputValue("#output-mode"), "speech");
  assert.equal(await page.inputValue("#unreadable"), "preserve");
  assert.equal(await page.inputValue("#max-control"), "4096");
  assert(
    (await page.locator("#sample-review").textContent()).includes(
      "독립 검토 대기",
    ),
  );
  await run();
  assert.equal(
    await page.locator("#output").textContent(),
    "만 원에서 이만 원",
  );
  // The displayed settings drive the native request; examples select their
  // required grammar instead of interpreting braces silently in plain mode.
  await page.selectOption("#sample", "user-v20-literal-ordinal");
  assert.equal(await page.inputValue("#input-format"), "annotated");
  await page.click('[data-mode="verify"]');
  await run();
  assert.equal(await page.locator("#output").textContent(), "첫번째");
  assert.equal(await page.locator("#verify-status").textContent(), "모든 분할 일치");
  for (const [sample, expected] of [
    ["user-v20-native-twenty-one", "스물하나"],
    ["user-v20-sino-negative", "마이너스 십이"],
    ["user-v20-alphabet-store", "에이비씨마트"],
    ["user-v20-email-digits", "에이일 앳 비이 닷 씨오엠"],
    ["user-v20-email-text-digits", "a1@b2.com"],
    ["user-v20-emoji-zwj-drop", "가 나"],
  ]) {
    await page.selectOption("#sample", sample);
    await run();
    assert.equal(await page.locator("#output").textContent(), expected);
  }
  await page.selectOption("#input-format", "annotated");
  await page.fill("#source", "1{@unknown}");
  await page.click("#run");
  await page.waitForFunction(() => !document.querySelector("#run").disabled);
  assert(await page.locator("#notice").evaluate((e) => e.classList.contains("error")));
  assert((await page.locator("#notice").textContent()).includes("invalid argument"));
  await page.click('[data-mode="normalize"]');
  await page.click("#reset");
  assert.equal(await page.inputValue("#input-format"), "plain");
  assert.equal(await page.inputValue("#output-mode"), "speech");
  assert.equal(await page.inputValue("#unreadable"), "preserve");
  await page.fill("#source", "Apple은 사과입니다.");
  await page.uncheck("#expected-enabled");
  await run();
  assert.equal(
    await page.locator("#output").textContent(),
    "애플은 사과입니다.",
  );
  assert((await page.locator("#resource-validation").textContent()).includes("리소스 정책 검사"));
  // Invalid editable policy is rejected before output; diagnostics name the row/key.
  await page.locator("#customer-tsv").evaluate((e) => e.closest("details").open = true);
  await page.fill("#customer-tsv", "# editable policy\ncontext\tko-KR\tclause_weight\t9999\n");
  await page.click("#run");
  await page.waitForFunction(() => !document.querySelector("#run").disabled);
  assert((await page.locator("#notice").textContent()).includes("customer.tsv:2"));
  assert((await page.locator("#notice").textContent()).includes("context/ko-KR/clause_weight"));
  await page.fill("#customer-tsv", "future.policy\t*\tkey\tvalue\n");
  await run();
  assert((await page.locator("#resource-validation").textContent()).includes("검사기 미등록 1행"));
  await page.fill("#customer-tsv", "");
  await run();
  await page.locator("#inspect-body tr").first().click();
  assert((await page.locator("#span-detail").textContent()).includes("en-US"));
  assert((await page.locator("#pronunciation-trace").textContent()).includes("lexicon/en-US:apple"));
  assert((await page.locator("#pronunciation-trace").textContent()).includes("선택"));
  // Editable target mapping reports its own source separately from English G2P.
  await page.fill("#source", "baba는 표기입니다.");
  await page.fill("#customer-tsv", "phone_vowel\tko-KR\tAE\t4\nphone_onset\tko-KR\tAE\t0\n");
  await run();
  assert.equal(await page.locator("#output").textContent(), "버버는 표기입니다.");
  await page.locator("#inspect-body tr").first().click();
  let pronunciation = await page.locator("#pronunciation-trace").textContent();
  assert(pronunciation.includes("source-pronunciation") && pronunciation.includes("target-phonetic"));
  assert(pronunciation.includes("en-US · system · g2p/b"));
  assert(pronunciation.includes("ko-KR · customer · phone_vowel/AE"));
  assert(!pronunciation.includes("phone_onset/AE"));
  // Rejected mapper work remains inspectable alongside the selected fallback.
  await page.fill("#customer-tsv", "pronunciation\ten-US\tbaba\tAH UNMAPPED\n");
  await run();
  assert.equal(await page.locator("#output").textContent(), "브아브아는 표기입니다.");
  await page.locator("#inspect-body tr").first().click();
  pronunciation = await page.locator("#pronunciation-trace").textContent();
  assert(pronunciation.includes("폐기") && pronunciation.includes("target-phonetic-failed"));
  assert(pronunciation.includes("선택 · target-grapheme"));
  await page.fill("#customer-tsv", "");
  await page.selectOption("#sample", "layer");
  await run();
  assert.equal(
    await page.locator("#output").textContent(),
    "커스텀애플은 사과입니다.",
  );
  await page.selectOption("#sample", "mixed");
  await page.click('[data-mode="verify"]');
  await run();
  assert.equal(
    await page.locator("#verify-status").textContent(),
    "모든 분할 일치",
  );
  await page.click('[data-mode="stream"]');
  await page.selectOption("#sample", "stream");
  await run();
  assert((await page.locator(".callback").count()) > 1);
  const download = page.waitForEvent("download");
  await page.click("#download");
  assert.equal((await download).suggestedFilename(), "ttf-text-report.json");
  // Split at an exact word boundary, then inspect the real native writes.
  await page.selectOption("#sample", "multiword");
  await page.locator("#source").evaluate((e) => e.setSelectionRange(6, 6));
  await page.click("#split-cursor");
  assert.equal(await page.locator("#match").textContent(), "이전 실행 결과");
  assert((await page.inputValue("#chunk-sizes")).startsWith("6, "));
  await run();
  assert.equal(await page.locator(".write-row").count(), 2);
  assert.equal(
    await page.locator("#output").textContent(),
    "애프터 이펙트를 사용합니다.\n뱅크 오브 아메리카는 은행입니다.",
  );
  await page.click('[data-mode="verify"]');
  await run();
  assert(
    (await page.locator("#verify-results").textContent()).includes(
      "custom writes",
    ),
  );
  assert.equal(
    await page.locator("#verify-status").textContent(),
    "모든 분할 일치",
  );
  // Partial UTF-8 input is shown as hex; the output remains valid UTF-8.
  await page.click('[data-mode="stream"]');
  await page.fill("#source", "Apple은 사과입니다.");
  await page.uncheck("#expected-enabled");
  await page.fill("#chunk-sizes", "1, 5, 2");
  await run();
  assert((await page.locator("#writes").textContent()).includes("UTF-8 분할"));
  assert.equal(
    await page.locator("#output").textContent(),
    "애플은 사과입니다.",
  );
  await page.fill("#chunk-sizes", "");
  await page.locator("#stream-panel details").evaluate((e) => (e.open = true));
  if (process.env.TTF_SCREENSHOT_DIR) {
    await page.screenshot({
      path: path.join(
        process.env.TTF_SCREENSHOT_DIR,
        "workbench-write-schedules.png",
      ),
      fullPage: true,
    });
  }
  // Cancel an actively paced request; a partial run cannot masquerade as a pass.
  await page.fill("#source", "Apple은 사과입니다.\n".repeat(12));
  await page.fill("#delay-ms", "70");
  await page.fill("#chunk-bytes", "7");
  await page.click("#run");
  await page.waitForFunction(() => !document.querySelector("#cancel").disabled);
  await page.click("#cancel");
  await page.waitForFunction(() => !document.querySelector("#run").disabled);
  assert((await page.locator("#notice").textContent()).includes("중단"));
  await page.click('[data-mode="normalize"]');
  await page.click("#reset");
  await page.fill(
    "#source",
    "<script>globalThis.PWNED=1</script> Apple은 사과입니다.",
  );
  await page.uncheck("#expected-enabled");
  await run();
  assert.equal(await page.evaluate(() => globalThis.PWNED), undefined);
  assert.equal(await page.locator("#output script").count(), 0);
  await page.click('[data-mode="corpus"]');
  assert(
    (await page.locator("#reference-summary").textContent()).includes(
      "독립 검토 완료 0건",
    ),
  );
  assert(
    (await page.locator("#corpus-panel h2").textContent()).includes(
      "기대 출력 코퍼스",
    ),
  );
  await page.selectOption("#corpus", "protected");
  await run();
  assert(
    (await page.locator("#corpus-review").textContent()).includes(
      "원문 보존 계약",
    ),
  );
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "40건 통과, 0건 실패",
    ),
  );
  await page.selectOption("#corpus", "multiword-v8");
  await run();
  assert(
    (await page.locator("#corpus-review").textContent()).includes(
      "임시 발음값",
    ),
  );
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "30건 통과, 0건 실패",
    ),
  );
  await page.selectOption("#corpus", "scaled-currency-v12");
  await run();
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "16건 통과, 0건 실패",
    ),
  );
  await page.selectOption("#corpus", "english-dates-v13");
  await run();
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "20건 통과, 0건 실패",
    ),
  );
  await page.selectOption("#corpus", "english-forms-v14");
  await run();
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "24건 통과, 0건 실패",
    ),
  );
  await page.selectOption("#corpus", "korean-counters-v15");
  assert((await page.locator("#corpus-review").textContent()).includes("독립 검토 완료 0건"));
  await run();
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "28건 통과, 0건 실패",
    ),
  );
  await page.selectOption("#corpus", "phone-v18");
  await run();
  assert((await page.locator("#corpus-progress").textContent()).includes("55건 통과, 0건 실패"));
  assert((await page.locator("#corpus-review").textContent()).includes("독립 검토 완료 0건"));
  await page.selectOption("#corpus", "user-patterns-v20");
  await run();
  assert((await page.locator("#corpus-progress").textContent()).includes("51건 통과, 0건 실패"));
  assert((await page.locator("#corpus-review").textContent()).includes("독립 검토 완료 0건"));
  await page.click('[data-mode="verify"]');
  await page.selectOption("#sample", "phone-v18-phone-v18-en-international-spaces");
  assert.equal(await page.inputValue("#language"), "en-US");
  await run();
  assert.equal(await page.locator("#output").textContent(),
    "Call me at plus one two one two five five five zero one zero zero.");
  assert.equal(await page.locator("#verify-status").textContent(), "모든 분할 일치");
  await page.locator("#inspect-body tr").filter({
    has: page.getByRole("cell", { name: "PHONE", exact: true }),
  }).first().click();
  assert((await page.locator("#span-detail").textContent()).includes("+1-212-555-0100"));
  assert((await page.locator("#span-detail").textContent()).includes("international"));
  await page.fill("#source", "Call +1 (212) 555-0100 ext. 0042.");
  await page.uncheck("#expected-enabled");
  await run();
  assert.equal(await page.locator("#output").textContent(),
    "Call plus one two one two five five five zero one zero zero extension zero zero four two.");
  await page.locator("#inspect-body tr").filter({
    has: page.getByRole("cell", { name: "PHONE", exact: true }),
  }).first().click();
  assert((await page.locator("#span-detail").textContent()).includes("0042"));
  await page.click('[data-mode="normalize"]');
  for (const [sample, expected, semantic] of [
    ["unicode-grouping-currency", "천이백오십 원", "CURRENCY"],
    ["unicode-grouping-percent", "마이너스 천이백오십 점 오영 퍼센트", "PERCENT"],
    ["unicode-grouping-malformed", "1\u200925원", "TEXT"],
    ["unicode-grouping-range", "천이백오십 원에서 이천오백 원", "RANGE"],
  ]) {
    await page.selectOption("#sample", sample);
    assert.equal(await page.inputValue("#language"), "ko-KR");
    assert((await page.locator("#sample-review").textContent()).includes("독립 검토 대기"));
    await run();
    assert.equal(await page.locator("#output").textContent(), expected);
    assert((await page.locator("#inspect-body").textContent()).includes(semantic));
  }
  await page.click('[data-mode="verify"]');
  await run();
  assert.equal(await page.locator("#verify-status").textContent(), "모든 분할 일치");
  await page.click('[data-mode="normalize"]');
  await page.selectOption("#sample", "mixed");
  await run();
  if (process.env.TTF_SCREENSHOT_DIR) {
    await page.screenshot({
      path: path.join(process.env.TTF_SCREENSHOT_DIR, "workbench-desktop.png"),
      fullPage: true,
    });
  }
  await page.setViewportSize({ width: 390, height: 844 });
  await page.reload();
  await page.waitForFunction(() => !document.querySelector("#run").disabled);
  assert(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= window.innerWidth + 1,
    ),
    "mobile horizontal overflow",
  );
  if (process.env.TTF_SCREENSHOT_DIR) {
    await page.screenshot({
      path: path.join(process.env.TTF_SCREENSHOT_DIR, "workbench-mobile.png"),
      fullPage: true,
    });
  }
  // Source-backed dictionary browser, isolated native preview and exports.
  await page.route("**/api/lexicon", async (route) => {
    await new Promise((resolve) => setTimeout(resolve, 500));
    await route.continue();
  });
  await page.goto((process.argv[2] || "http://127.0.0.1:8000") + "/lexicon");
  await page.fill("#lex-query", "ETF");
  await page.waitForFunction(
    () => document.querySelector("#lex-test-current").disabled === false,
  );
  assert(
    (await page.locator("#lex-summary").textContent()).includes("출처 대조"),
  );
  await page.fill("#lex-query", "ETF");
  await page.locator("#lex-rows button").filter({ hasText: /^ETF$/ }).click();
  await page.click("#lex-test-current");
  await page.waitForFunction(
    () => !document.querySelector("#lex-test-current").disabled,
  );
  assert(
    (await page.locator("#lex-result").textContent()).includes(
      "이티에프의 발음을 확인합니다.",
    ),
  );
  assert(
    (await page.locator("#lex-result").textContent()).includes(
      "lexicon:domain",
    ),
  );
  await page.fill("#lex-pron", "검토발음");
  await page.click("#lex-test-edit");
  await page.waitForFunction(
    () => !document.querySelector("#lex-test-edit").disabled,
  );
  assert(
    (await page.locator("#lex-result").textContent()).includes(
      "검토발음의 발음을 확인합니다.",
    ),
  );
  const dictionaryDownload = page.waitForEvent("download");
  await page.click("#lex-export-tsv");
  assert.equal(
    (await dictionaryDownload).suggestedFilename(),
    "customer-pronunciation.tsv",
  );
  await page.fill(
    "#lex-corpus",
    "Apple의 정보입니다. foobarquux의 정보입니다.",
  );
  await page.click("#lex-scan");
  await page.waitForFunction(
    () => !document.querySelector("#lex-scan").disabled,
  );
  assert(
    (await page.locator("#lex-coverage-summary").textContent()).includes(
      "fallback 1",
    ),
  );
  assert(
    (await page.locator("#lex-coverage").textContent()).includes("foobarquux"),
  );
  await page.fill("#lex-corpus", "a".repeat(20000));
  await page.click("#lex-scan");
  await page.waitForFunction(
    () => !document.querySelector("#lex-scan").disabled,
  );
  assert.equal(await page.locator("#lex-coverage-summary").textContent(), "");
  assert(
    !(await page.locator("#lex-coverage").textContent()).includes("foobarquux"),
  );
  assert(await page.locator("#lex-export-coverage").isDisabled());
  await page.fill(
    "#lex-corpus",
    "ETF의 비용과 foobarquux의 정보를 확인합니다.",
  );
  await page.selectOption("#lex-scan-domain", "finance-vocabulary");
  await page.click("#lex-scan");
  await page.waitForFunction(
    () => !document.querySelector("#lex-scan").disabled,
  );
  assert(
    (await page.locator("#lex-coverage").textContent()).includes(
      "lexicon:domain",
    ),
  );
  assert(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth + 1,
    ),
    "dictionary mobile overflow",
  );
  if (process.env.TTF_SCREENSHOT_DIR)
    await page.screenshot({
      path: path.join(process.env.TTF_SCREENSHOT_DIR, "lexicon-mobile.png"),
      fullPage: true,
    });
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.evaluate(() => scrollTo(0, 0));
  if (process.env.TTF_SCREENSHOT_DIR)
    await page.screenshot({
      path: path.join(process.env.TTF_SCREENSHOT_DIR, "lexicon-desktop.png"),
      fullPage: true,
    });
  assert.deepEqual(errors, []);
  await browser.close();
  console.log(
    "Browser: frontend modes, aliases/patterns, strict errors, speech/email, trace, lexicon, parity, custom writes, split UTF-8, live callbacks, download, cancel, XSS text, corpora and mobile passed.",
  );
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
