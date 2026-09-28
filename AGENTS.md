# TTS Text Frontend: bounded continuation

Start with `docs/HANDOFF.md` and `docs/tn-improvement-playbook.ko.md`.
The authoritative remaining scope is `docs/remaining-work.md`; measured data work
is ordered in `benchmark/manifests/quality-work-queue.json`.

- Finish the user's requested issue or one named rule family, then report and stop.
  Do not resume an open-ended goal or attempt to remove every limitation automatically.
- No model API or particular model tier is required. Routine corpus/resource work
  should use a small, bounded iteration. Escalate hard memory/ABI/concurrency defects
  when evidence requires it; do not repeatedly re-research the entire architecture.
- Evaluate text, Semantic IR, provenance and callbacks. No audio synthesis/evaluation.
- First reproduce against the current version, preserving output options and resources.
  A user's historic result is evidence to investigate, not proof of a current bug.
- Author expected outputs independently before executing a candidate. Never rewrite
  gold to pass the implementation. Label policy disagreements and pending review.
- Use `tools/quality/tn_cycle.py` to capture artifacts and compare one immutable corpus.
  Generic patterns need variations and negatives. Do not solve grammar with sentence
  exceptions; use lexicons for conventional product/name pronunciations.
- Keep Core language-independent, use Semantic/Pronunciation IR, and preserve ABI v1.
  Unknown characters preserve by default; controls are opt-in annotated input.
- Keep external/private corpora and captures under ignored `artifacts/` or
  `benchmark/generated/`. Never vendor NC datasets into product resources.
- Run targeted tests while editing, then `tools/ci/run.sh full` once before pushing
  (`quick` = gcc + text-evaluation for small changes). GitHub Actions minutes are a
  budget: pushes do not run CI. Actions runs only on pull requests to main and manual
  dispatch; clang/sanitizers only with `gh workflow run ci.yml -f full=true`. Actions
  and local runs share `tools/ci/run.sh`, so edit jobs there, not in the workflow.
  Push finished work in batches, not after every commit. On this 2-core VM, do not
  run full GCC and sanitizer suites concurrently: the 30k-number gate has a
  deliberate 60-second limit. Do not relax limits to disguise contention/regression.
- Update HANDOFF with the task, exact commands/results, remaining work and artifact
  locations. Commit/push/deploy only when authorized by the session.
- This is an exe.dev VM. Use only documented features:
  https://exe.dev/docs.md and https://exe.dev/docs/proxy.md.
