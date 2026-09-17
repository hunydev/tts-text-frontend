"""Independent Korean ordinal/count/identifier text and streaming contracts.

These original references predate candidate execution; they are not imported
from the engine's output or resource tables. Native forms below 100 are an
explicit reference table. Larger ordinals and ambiguous bare ``번`` use the
documented project policy, not a claim that all other readings are wrong.

Primary sources, reviewed 2026-09-17 (facts paraphrased, no source text copied):
https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?qna_seq=329611
    11번째 uses 열한; 1001번째 has no uniquely prescribed reading.
https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?qna_seq=325931
    Count and identifier 번 depend on context; no absolute general criterion.
https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?qna_seq=306013
    Ordinal prefix 제- attaches to Sino-Korean numerals.
"""

import argparse
import importlib.util
import json
import subprocess
import tempfile
from pathlib import Path

from test_english_dates import PROFILES, exact_span, stream

ROOT = Path(__file__).resolve().parents[2]

# Explicit independently authored readings; no engine/resource lookup occurs.
NATIVE = """
한 두 세 네 다섯 여섯 일곱 여덟 아홉
열 열한 열두 열세 열네 열다섯 열여섯 열일곱 열여덟 열아홉
스무 스물한 스물두 스물세 스물네 스물다섯 스물여섯 스물일곱 스물여덟 스물아홉
서른 서른한 서른두 서른세 서른네 서른다섯 서른여섯 서른일곱 서른여덟 서른아홉
마흔 마흔한 마흔두 마흔세 마흔네 마흔다섯 마흔여섯 마흔일곱 마흔여덟 마흔아홉
쉰 쉰한 쉰두 쉰세 쉰네 쉰다섯 쉰여섯 쉰일곱 쉰여덟 쉰아홉
예순 예순한 예순두 예순세 예순네 예순다섯 예순여섯 예순일곱 예순여덟 예순아홉
일흔 일흔한 일흔두 일흔세 일흔네 일흔다섯 일흔여섯 일흔일곱 일흔여덟 일흔아홉
여든 여든한 여든두 여든세 여든네 여든다섯 여든여섯 여든일곱 여든여덟 여든아홉
아흔 아흔한 아흔두 아흔세 아흔네 아흔다섯 아흔여섯 아흔일곱 아흔여덟 아흔아홉
""".split()
assert len(NATIVE) == 99


def reference(text, expected, surface, semantic, value, unit, secondary,
              source="ko-KR", language="ko-KR"):
    return dict(text=text, expected=expected, surface=surface,
                semantic_type=semantic, value=value, unit=unit,
                secondary=secondary, source_lang=source, context_lang=language)


def references():
    rows = []
    for number, native in enumerate(NATIVE, 1):
        word = "첫" if number == 1 else native
        rows.append(reference(f"{number}번째", f"{word} 번째", f"{number}번째",
                              "ORDINAL", str(number), "번째", ""))
    for number, spoken in ((100, "백"), (101, "백일"), (110, "백십"), (120, "백이십"),
                           (121, "백이십일"), (200, "이백"), (999, "구백구십구"),
                           (1000, "천"), (1001, "천일"), (1020, "천이십"),
                           (2026, "이천이십육"), (10000, "만")):
        rows.append(reference(f"{number}번째", f"{spoken} 번째", f"{number}번째",
                              "ORDINAL", str(number), "번째", ""))
    for number, native in ((1, "첫"), (2, "두"), (3, "세"), (4, "네"), (11, "열한"),
                           (20, "스무"), (21, "스물한"), (99, "아흔아홉")):
        surface = f"{number} 번째"
        rows.append(reference(f"이번은 {surface} 촬영입니다.", f"이번은 {native} 번째 촬영입니다.",
                              surface, "ORDINAL", str(number), "번째", ""))
    for digits, spoken in (("01", "첫"), ("0002", "두"), ("011", "열한"), ("020", "스무")):
        rows.append(reference(f"{digits}번째", f"{spoken} 번째", f"{digits}번째",
                              "ORDINAL", digits, "번째", ""))
    rows.append(reference("3\t번째", "세 번째", "3\t번째", "ORDINAL", "3", "번째", ""))
    for separator in (",", "\u00a0", "\u202f"):
        for text, expected, unit, secondary in (
            (f"1{separator}001번째", "천일 번째", "번째", ""),
            (f"제1{separator}001호", "제천일 호", "호", "prefix"),
        ):
            rows.append(reference(text, expected, text, "ORDINAL", "1001", unit, secondary))
    for text, expected, value, secondary in (
        ("1~3번째", "첫 번째에서 세 번째", "1", "3"),
        ("1번째~3번째", "첫 번째에서 세 번째", "1", "3"),
        ("11~21번째", "열한 번째에서 스물한 번째", "11", "21"),
        ("3번째~5", "세 번째에서 다섯 번째", "3", "5"),
    ):
        rows.append(reference(text, expected, text, "RANGE", value, "번째", secondary))

    count_readings = ((1, "한"), (2, "두"), (3, "세"), (4, "네"), (11, "열한"),
                      (20, "스무"), (21, "스물한"), (30, "서른"), (99, "아흔아홉"))
    for number, spoken in count_readings:
        for before, after in (("총 ", " 검사했습니다."), ("모두 ", " 확인했습니다."),
                              ("", " 반복합니다."), ("", " 시도합니다.")):
            rows.append(reference(f"{before}{number}번{after}", f"{before}{spoken} 번{after}",
                                  f"{number}번", "COUNTER", str(number), "번", "frequency"))
    for text, expected, surface, value in (
        ("총3번 확인했습니다.", "총세 번 확인했습니다.", "3번", "3"),
        ("매일 2번 확인합니다.", "매일 두 번 확인합니다.", "2번", "2"),
        ("버튼을 3번 누릅니다.", "버튼을 세 번 누릅니다.", "3번", "3"),
        ("알림을 4번 실행합니다.", "알림을 네 번 실행합니다.", "4번", "4"),
        ("3 번 반복합니다.", "세 번 반복합니다.", "3 번", "3"),
        ("3번을 반복합니다.", "세 번을 반복합니다.", "3번", "3"),
        ("매일 3번씩 반복합니다.", "매일 세 번씩 반복합니다.", "3번", "3"),
    ):
        rows.append(reference(text, expected, surface, "COUNTER", value, "번", "frequency"))
    for particle in ("만", "씩", "까지만", "만큼"):
        rows.append(reference(f"3번{particle} 반복합니다.", f"세 번{particle} 반복합니다.",
                              "3번", "COUNTER", "3", "번", "frequency"))

    for number, spoken in ((1, "일"), (2, "이"), (3, "삼"), (11, "십일"),
                           (20, "이십"), (21, "이십일")):
        for before, after in (("번호는 ", "입니다."), ("문항 ", "을 확인합니다."),
                              ("", " 출구입니다."), ("", " 버스입니다.")):
            rows.append(reference(f"{before}{number}번{after}", f"{before}{spoken} 번{after}",
                                  f"{number}번", "COUNTER", str(number), "번", "identifier"))
    # Identifier evidence takes precedence when both readings have signals.
    rows.append(reference("번호는 3번 반복합니다.", "번호는 삼 번 반복합니다.",
                          "3번", "COUNTER", "3", "번", "identifier"))
    for number, spoken in ((1, "일"), (3, "삼"), (20, "이십"), (101, "백일")):
        rows.append(reference(f"{number}번", f"{spoken} 번", f"{number}번",
                              "COUNTER", str(number), "번", "", source="und"))
    for text, expected in (("총\n3번", "총\n삼 번"),
                           ("3번\n반복합니다.", "삼 번\n반복합니다.")):
        rows.append(reference(text, expected, "3번", "COUNTER", "3", "번", "", source="und"))
    rows.append(reference("3\n번째", "삼\n번째", "3", "CARDINAL", "3", "", "", source="und"))
    rows.append(reference("제\n3장", "제\n세 장", "3장", "COUNTER", "3", "장", "", source="und"))
    rows.append(reference("경제3장", "경제세 장", "3장", "COUNTER", "3", "장", "", source="und"))

    for number, spoken in ((1, "일"), (3, "삼"), (11, "십일")):
        for unit in ("장", "회", "차", "조", "항", "절", "권", "부", "호", "번"):
            surface = f"제{number}{unit}"
            rows.append(reference(surface, f"제{spoken} {unit}", surface,
                                  "ORDINAL", str(number), unit, "prefix"))
    for digits, spoken in (("1", "일"), ("3", "삼"), ("20", "이십"), ("01", "일")):
        rows.append(reference(f"제{digits}", f"제{spoken}", f"제{digits}",
                              "ORDINAL", digits, "", "prefix"))

    for text in ("0번째", "00번째", "+3번째", "-3번째", "1.5번째", "0.5번째",
                 "-1.5번째", "+1.5번째", "제0장", "제-3장", "제+3장", "제1.5장",
                 "제0", "제-3", "제1.5", "제1,00호", "1,00번째",
                 "제1\u202f00호", "1\u00a000번째", "3번째~0", "3번째~1.5kg",
                 "1번째~2번째a", "제3-5장"):
        rows.append(reference(text, text, text, "TEXT", "", "", ""))
    for surface in ("-3번째", "1.5번째", "제1.5장"):
        text = f"잘못된 표기는 {surface}입니다."
        rows.append(reference(text, text, surface, "TEXT", "", "", ""))

    # Earlier counter behavior must survive the role classifier. These explicit
    # references are ordinary count units, with neutral source digits.
    for unit, spoken in (("개", "세"), ("명", "세"), ("살", "세"), ("시", "세"),
                         ("분", "삼"), ("초", "삼"), ("장", "세"), ("권", "세"),
                         ("대", "세"), ("마리", "세"), ("잔", "세"), ("병", "세"),
                         ("벌", "세"), ("켤레", "세"), ("송이", "세"), ("자루", "세"),
                         ("시간", "세"), ("달", "세"), ("개월", "삼"), ("층", "삼")):
        rows.append(reference(f"3{unit}", f"{spoken} {unit}", f"3{unit}",
                              "COUNTER", "3", unit, "", source="und"))
    return rows


def native_references(rows):
    selected = {"1번째", "11번째", "20번째", "21번째", "1001번째", "0002번째",
                "이번은 3 번째 촬영입니다.", "총3번 확인했습니다.", "총 20번 검사했습니다.",
                "3번 반복합니다.", "번호는 3번 반복합니다.", "3번 출구입니다.",
                "3 번 반복합니다.", "제3장", "제3회", "제20", "제1.5장", "-3번째",
                "1.5번째", "0번째", "3번", "3장", "1~3번째", "1번째~3번째",
                "제1,001호", "1\u202f001번째", "제1\u202f00호", "3번을 반복합니다.",
                "매일 3번씩 반복합니다.", "3번째~5", "3번째~0", "3번째~1.5kg",
                "1번째~2번째a", "제3-5장"}
    fixtures = [row for row in rows if row["text"] in selected]
    assert len(fixtures) == len(selected), selected - {row["text"] for row in fixtures}
    return fixtures


def check_reference(result, row):
    assert result["text"] == row["expected"], (row, result)
    span = exact_span(result, row["text"], row["surface"])
    for key in ("semantic_type", "value", "unit", "secondary", "source_lang", "context_lang"):
        assert span[key] == row[key], (row["text"], key, row[key], span)
    assert span["render_lang"] == row["context_lang"], (row, span)
    assert span["rule"], (row, span)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    checks = native_checks = 0
    rows = references()
    assert len({row["text"] for row in rows}) == len(rows)

    for row in rows:
        run = subprocess.run([str(args.cli), "normalize", "--resources", str(args.resources),
                              "--language", row["context_lang"], row["text"]],
                             capture_output=True, text=True, encoding="utf-8", timeout=15)
        assert run.returncode == 0 and run.stdout == row["expected"] + "\n", (row, run)
        checks += 1
        baseline = None
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, row["text"],
                                language=row["context_lang"], chunk=chunk, profile=profile)
                check_reference(result, row)
                if baseline is None:
                    baseline = result
                assert result == baseline, (row, profile, chunk, baseline, result)
                checks += 1

    corpus = ROOT / "tests/evaluation/korean_counters_v15.jsonl"
    corpus_rows = [json.loads(line) for line in corpus.read_text(encoding="utf-8").splitlines()]
    assert len(corpus_rows) >= 20
    for row in corpus_rows:
        result = stream(args.cli, args.resources, row["text"], language="ko-KR", chunk=7)
        assert result["text"] == row["expected"], (row, result)
        for expected in row["expected_spans"]:
            span = exact_span(result, row["text"], expected["surface"])
            for key, value in expected.items():
                if key != "surface":
                    assert span[key] == value, (row, span, key)
        checks += 1

    with tempfile.TemporaryDirectory(prefix="ttf-korean-counters-") as directory:
        path = Path(directory) / "customer.tsv"
        overlay = ("--customer", str(path))
        for body, text, expected, role, layer in (
            ("ordinal_first\tko-KR\t1\t맨 첫\n", "1번째", "맨 첫 번째", "", "system"),
            ("counter_role_after\tko-KR\t수정\tfrequency\n", "3번 수정합니다.",
             "세 번 수정합니다.", "frequency", "customer"),
            ("counter_role_before\tko-KR\t재생\tfrequency\n"
             "boundary_word\tko-KR\t재생\t1\n", "재생 3번", "재생 세 번", "frequency", "customer"),
            # The longer system cue 반복 wins this prefix match; the unused
            # customer cue 반 must not be reported as the selected resource.
            ("counter_role_after\tko-KR\t반\tidentifier\n", "3번 반복합니다.",
             "세 번 반복합니다.", "frequency", "system"),
        ):
            path.write_text(body, encoding="utf-8")
            for profile in PROFILES:
                for chunk in (1, 7, 64):
                    result = stream(args.cli, args.resources, text, language="ko-KR",
                                    chunk=chunk, profile=profile, overlay=overlay)
                    assert result["text"] == expected, (body, text, result)
                    if role:
                        span = exact_span(result, text, "3번")
                        assert span["secondary"] == role and span["resource_layer"] == layer, span
                    checks += 1
        # Reject malformed policies when the matching rule is evaluated. A bad
        # overlay must not silently change reading/classification priorities.
        bad_resources = [("ordinal_policy\tko-KR\tlarge\tinvalid\n", "1001번째")]
        bad_resources += [(f"classifier_priority\tko-KR\tko:frequency-counter\t{value}\n",
                           "3번 반복합니다.")
                          for value in ("high", "1001", "-1001", "999999999999999999999999999")]
        for body, text in bad_resources:
            path.write_text(body, encoding="utf-8")
            run = subprocess.run([str(args.cli), "normalize", "--resources", str(args.resources),
                                  "--language", "ko-KR", *overlay, text],
                                 capture_output=True, text=True, encoding="utf-8", timeout=15)
            assert run.returncode == 1 and not run.stdout and "invalid resource" in run.stderr, (
                body, text, run.returncode, run.stdout, run.stderr)
            checks += 1

    if args.library:
        spec = importlib.util.spec_from_file_location("ttf_native", ROOT / "tools/web/native.py")
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)
        # Include lookahead after 번, prefix and suffix splits, and UTF-8 splits
        # inside the Korean ordinal/counter. Compare every metadata field too.
        fixtures = native_references(rows)
        for tiny in (False, True):
            options = {"default_language": "ko-KR"}
            if tiny:
                options.update(first_target=1, normal_target=3, max_length=12)
            engine = native.Engine(args.library.resolve(), args.resources.resolve(), options)
            try:
                for row in fixtures:
                    source = row["text"].encode()
                    batch = engine.normalize(source)
                    check_reference(batch, row)
                    for split in range(1, len(source)):
                        actual = native.flatten(engine.stream(source, [split, len(source) - split]))
                        assert actual == batch, (row, split, tiny, batch, actual)
                        native_checks += 1
            finally:
                engine.close()

    print(f"{len(rows)} original Korean ordinal/count/identifier references; "
          f"{checks} text/IR/context/chunk checks; "
          f"{native_checks} native every-two-way-byte-split checks passed")


if __name__ == "__main__":
    main()
