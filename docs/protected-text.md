# Literal code, markup and file paths (v0.6)

This policy preserves selected source spans byte for byte while normalizing the
surrounding prose. It supports documents mentioning commands, exported files and
code samples. It does not render HTML, strip tags, decode entities, access files,
or evaluate code. The output remains text, including its literal delimiters.

```text
문서의 <code>GPU</code> 태그를 확인합니다.
→ 문서의 <code>GPU</code> 태그를 확인합니다.

<em>Apple은 사과입니다.</em> 예제는 3개입니다.
→ <em>애플은 사과입니다.</em> 예제는 세 개입니다.

설정 파일 /etc/app/config-v2.json을 확인하고 3초 기다립니다.
→ 설정 파일 /etc/app/config-v2.json을 확인하고 삼 초 기다립니다.
```

## Recognized forms

| Form | Treatment |
| --- | --- |
| `code`, `pre`, `kbd`, `samp` elements | Preserve the complete element and contents, including delimiters |
| Other syntax-valid tags, such as `em` and `span` | Preserve each tag; normalize text between tags |
| HTML-style comments | Preserve from `<!--` through `-->` |
| Backtick code | Preserve between equal-length runs of 1–16 backticks, including multiline contents |
| Unix paths | Explicit `/`, `./`, `../`, or `~/` prefix |
| Windows paths | Explicit drive prefix such as `C:\` or `C:/`, or UNC host/share form |
| Quoted explicit paths | ASCII single/double quotes enclose spaces or UTF-8 names; quotes are preserved |

Tag names are ASCII and case insensitive for recognition; original case is
preserved. Names are limited to 63 bytes. Attribute parsing understands quoted
`>` and simple boolean/unquoted attributes. Same-name opaque elements nest up to
32 levels. This is a bounded lexical grammar, not a conforming HTML or CommonMark
parser. Backtick fences do not interpret language annotations or escape rules.

Unquoted paths use a restricted ASCII character inventory. Hangul suffixes and
sentence punctuation remain outside the protected span. A trailing period is
sentence punctuation unless inside a quoted path. URI queries, ordinary fractions
and prose slashes are guarded against accidental path recognition. Bare filenames
such as `config.json`, numeric-only Unix paths, and unquoted Unicode path names are
not generally recognized; quote an explicit path when names contain spaces or
Unicode. No filesystem existence, expansion or validity check is performed.

## Resource customization

`resources/common/protected.tsv` supplies the default policy. These records are
loaded at the Common layer, with the usual higher-layer overrides:

```text
protection<TAB>*<TAB>markup<TAB>1
protection<TAB>*<TAB>code<TAB>1
protection<TAB>*<TAB>path<TAB>1
protected_tag<TAB>*<TAB>code<TAB>opaque
```

Use actual tabs in a TSV. `1` enables a policy, `0` disables it. `markup` controls
generic tags and comments; `code` controls backticks and opaque elements. They are
independent: disabling generic markup leaves enabled opaque code elements intact.

For example, this customer overlay adds an opaque application tag and makes the
standard `code` element transparent so its body can be normalized:

```python
from pathlib import Path
Path("customer-protection.tsv").write_text(
    "protected_tag\t*\tasset\topaque\n"
    "protected_tag\t*\tcode\ttransparent\n",
    encoding="utf-8",
)
```

Load with `--customer customer-protection.tsv` or `ttf_resource_load` before
freezing the engine. To restore ordinary processing of every form, disable all
three protection policies. Enabling preservation takes precedence over TN and
pronunciation overrides within the literal: even an exact customer rule cannot
replace its contents or consume across it. Disable the policy explicitly when
that transformation is intended.

## Semantic metadata and context isolation

A literal emits a `TEXT` semantic item before identity rendering. Its `unit` is
`code`, `markup` or `path`; `value` and `secondary` are empty. `rule` identifies
`protected.code`, `protected.markup`, `protected.comment` or `protected.path`.
Unclosed code, markup and comments use `protected.unclosed-*` rules. The layer
records the selected policy; an opaque tag also accounts for its declaration layer.

Source, context and render language are `und`, with `verbalizer=identity` and
`context_trace=protected:excluded`. Offsets still refer to the original input and
normalized output in UTF-8 bytes. Paths and code are not truncated to fit the
95-byte numeric IR field; the source span carries the complete literal.

The public universal tokenizer is unchanged. Internally, a shared view collapses
each literal into one neutral `SYMBOL` token. Routing and quote analysis cannot
see the internal English words, Korean text or quotation marks as context signals.
Numeric classifiers and exact/pattern rules receive a view bounded before the next
literal. Pack callbacks that exceed this view are rejected with `TTF_ERESOURCE`.
The public ABI v1, struct layouts and ownership rules remain unchanged.

Applications can distinguish preserved source from ordinary normalized speech
using these metadata fields. Literal preservation itself does not define how a
TTS backend should speak commands, paths or markup.

## Streaming and incomplete input

The same recognizers supply the batch and streaming boundary planner. Sentence
punctuation, spaces and newlines inside a literal cannot commit part of it.
Complete surrounding prose can still be delivered before the rest of the input.
An exact backtick closing run may require one more byte of lookahead.

On final `flush`, a recognized but unclosed code/tag/comment or quoted path keeps
its remaining raw source. Nesting beyond the bound likewise preserves the tail.
Malformed UTF-8 is rejected even inside code. The configured pending byte cap
still returns `TTF_ELIMIT`; it does not truncate or silently split a literal.
An incremental UTF-8 cursor and conservative literal-body cache avoid rechecking
ordinary body bytes on every small write. Potential closing delimiters or syntax
changes restore the full scan; UTF-8 errors are never skipped. Delimiter-dense
inputs may still require repeated work.
Very long opaque blocks can therefore delay a callback and exceed a soft phrase
target. This is an explicit text-preservation tradeoff.

## Text-only evidence

The 40 references in `tests/evaluation/protected_cases.jsonl` were authored and
fixed before running the frozen v0.5 executable and resources (`ff5f847`). They
are original synthetic development cases, with no independent human annotation.

| Same references | v0.5 | v0.6 |
| --- | ---: | ---: |
| Text, metadata and stream parity together | 4/40 | 40/40 |
| Exact text matches | 8/40 | 40/40 |
| Character edit distance / reference codepoints | 402/1,432 | 0/1,432 |

The corpus hash is
`7138d68f356dd5772f9374f30afaab0584d376280fbf8e9d5646f8b64aba1ae6`.
Focused tests add tiny targets, overlays, invalid UTF-8, URL-query guards, complete
JSONL event equality across chunks and callback delivery before EOF. Native tests
cover every two-chunk byte split for representative forms, flush and pending caps.

The historical 15-case challenge file now passes all its fixed references. All
15 are known development cases across v0.5/v0.6, not an unseen holdout. Neither
that result nor 40/40 establishes general document or pronunciation accuracy.
