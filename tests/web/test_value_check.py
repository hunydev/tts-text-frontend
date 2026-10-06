"""Value check for Korean TN results (tools/web/value_check.py): readings it must accept and
value changes it must report. Cases written before the checker was wired into /jev-eval
(2026-10-06), from the ko-v1 Jev review and benchmark references."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/web"))
import value_check  # noqa: E402

OK = [
    ("3만 원을 냈다.", "삼만 원을 냈다."),
    ("5개를 샀다.", "다섯 개를 샀다."),
    ("시즌 1을 봤다", "시즌 원을 봤다"),
    ("1.2kg을", "일 점 이 킬로그램을"),
    ("4.3 희생자", "사삼 희생자"),
    ("2~3명", "두세 명"),
    ("20~30대", "이삼십 대"),
    ("40〜50대", "사오십 대"),
    ("1770~1780년도", "천 칠백 칠팔십 년도"),
    ("20,000~30,000원", "이삼만 원"),
    ("1/2 정도", "이 분의 일 정도"),
    ("K12를", "케이십이를"),
    ("78살", "일흔여덟 살"),
    ("31데이", "써리 원 데이"),
    ("비타500 100병", "비타 오백 백 병"),
    ("1만 8,350㎡", "일만 팔천 삼백 오십 제곱미터"),
    ("나스닥 100이 15,213.32에", "나스닥 백이 만 오천 이백 십 삼 점 삼 이에"),
    ("4.000원", "사천 원"),
    ("010-1234-5678로", "공일공 일이삼사 오육칠팔로"),
    ("015-6198-5431니", "영 하나 오 육 하나 구 팔 오 사 삼 하나니"),
    ("예약번호 129번", "예약번호 백이십구 번"),
    ("10월 6일", "시월 육 일"),
    ("63ｍ 높이", "육십삼 미터 높이"),
    ("오늘은 날씨가 좋다.", "오늘은 날씨가 좋다."),
]
MISMATCH = [
    ("180g으로", "백팔십오 그램으로", "180"),
    ("10명인데", "열세 명인데", "10"),
    ("1.2kg을", "일 점 팔 킬로그램을", "1.2"),
    ("357-00-14789가", "삼오칠 다시 공공 다시 일사칠팔이가", "14789"),
    ("6.37위안", "육 점 육칠위안", "6.37"),
    ("100kg이야", "백이 킬로그램이야", "100"),
    ("1830년 프랑스", "천오백삼십 년 프랑스", "1830"),
]
UNKNOWN = [
    ("2~3분이", "이에서 사 분이"),  # the shape differs: no claim either way
]

checks = 0
for written, spoken in OK:
    result = value_check.check(written, spoken)
    assert result["status"] == "ok", (written, spoken, result)
    checks += 1
for written, spoken, token in MISMATCH:
    result = value_check.check(written, spoken)
    assert result["status"] == "mismatch" and [f["written"] for f in result["findings"]] == [token], (written, result)
    checks += 1
for written, spoken in UNKNOWN:
    assert value_check.check(written, spoken)["status"] != "ok", (written, spoken)
    checks += 1
print("%d value-check cases passed" % checks)
