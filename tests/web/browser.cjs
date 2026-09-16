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
  await page.fill("#source", "Apple은 사과입니다.");
  await run();
  assert.equal(
    await page.locator("#output").textContent(),
    "애플은 사과입니다.",
  );
  await page.locator("#inspect-body tr").first().click();
  assert((await page.locator("#span-detail").textContent()).includes("en-US"));
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
    "애프터 이펙츠를 사용합니다.\n뱅크 오브 아메리카는 은행입니다.",
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
  await page.selectOption("#corpus", "protected");
  await run();
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "40건 통과, 0건 실패",
    ),
  );
  await page.selectOption("#corpus", "multiword-v8");
  await run();
  assert(
    (await page.locator("#corpus-progress").textContent()).includes(
      "30건 통과, 0건 실패",
    ),
  );
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
  assert.deepEqual(errors, []);
  await browser.close();
  console.log(
    "Browser: normalize, trace, user lexicon, parity, custom writes, split UTF-8, live callbacks, download, cancel, XSS text, corpora and mobile passed.",
  );
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
