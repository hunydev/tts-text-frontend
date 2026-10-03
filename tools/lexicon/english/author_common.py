"""Author the common English word lexicon (Korean context) and its source record.

Readings follow the 외래어 표기법 English rules and established Korean usage; they are
project-authored. Word selection used local frequency counts (Korean benchmark,
NIKL NE dictionary example sentences); no entry is copied from those sources.
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path.home() / "ttf-r"
SOURCE = "project-english-common-v24"
WORDS = """
of 오브|for 포|to 투|on 온|with 위드|you 유|we 위|me 미|all 올|my 마이|by 바이|is 이즈|or 오어|our 아워|
your 유어|how 하우|come 컴|this 디스|that 댓|their 데어|as 애즈|out 아웃|again 어게인|against 어게인스트|
do 두|down 다운|know 노|eat 잇|thank 생크|
korea 코리아|korean 코리안|asia 아시아|asian 아시안|journal 저널|international 인터내셔널|new 뉴|talk 토크|
award 어워드|awards 어워즈|air 에어|lab 랩|life 라이프|live 라이브|job 잡|good 굿|education 에듀케이션|
research 리서치|society 소사이어티|youth 유스|affiliation 어필리에이션|organization 오거니제이션|group 그룹|
bye 바이|safety 세이프티|future 퓨처|together 투게더|school 스쿨|materials 머티리얼즈|arts 아츠|film 필름|
learning 러닝|health 헬스|buy 바이|cooperation 코퍼레이션|science 사이언스|people 피플|web 웹|vehicle 비히클|
name 네임|martial 마셜|business 비즈니스|love 러브|angels 에인절스|bridge 브리지|care 케어|urban 어반|
address 어드레스|economic 이코노믹|tire 타이어|tyre 타이어|financing 파이낸싱|shop 숍|honor 아너|like 라이크|
cancer 캔서|citation 사이테이션|development 디벨롭먼트|advanced 어드밴스드|corona 코로나|virus 바이러스|
solution 솔루션|excellence 엑설런스|excellent 엑설런트|club 클럽|association 어소시에이션|space 스페이스|
healthcare 헬스케어|time 타임|times 타임스|convention 컨벤션|tier 티어|travel 트래블|complex 콤플렉스|
auto 오토|automation 오토메이션|social 소셜|national 내셔널|scholarship 스칼러십|line 라인|young 영|edge 에지|
god 갓|challenge 챌린지|value 밸류|american 아메리칸|mind 마인드|institute 인스티튜트|reference 레퍼런스|
home 홈|treatment 트리트먼트|pneumonia 뉴모니아|analytics 애널리틱스|processing 프로세싱|deep 딥|
management 매니지먼트|exhibition 엑시비션|lounge 라운지|vessel 베슬|cities 시티스|wall 월|big 빅|
governance 거버넌스|place 플레이스|hall 홀|league 리그|safe 세이프|olympic 올림픽|games 게임스|council 카운슬|
climate 클라이밋|ocean 오션|fair 페어|alliance 얼라이언스|director 디렉터|cooking 쿠킹|strategy 스트래티지|
forever 포에버|senior 시니어|toxicology 톡시콜로지|wearable 웨어러블|welfare 웰페어|needs 니즈|
beautiful 뷰티풀|human 휴먼|engineering 엔지니어링|universities 유니버시티스|laboratory 래버러토리|
outstanding 아웃스탠딩|molecular 몰레큘러|cellular 셀룰러|middle 미들|requiem 레퀴엠|bishop 비숍|golden 골든|
lemon 레몬|yuzu 유자|agricultural 애그리컬처럴|case 케이스|control 컨트롤|cheer 치어|merry 메리|supply 서플라이|
finance 파이낸스|private 프라이빗|consumer 컨슈머|electronics 일렉트로닉스|electronic 일렉트로닉|natural 내추럴|
reports 리포츠|washington 워싱턴|components 컴포넌츠|biological 바이올로지컬|intelligence 인텔리전스|
building 빌딩|sensors 센서스|corporation 코퍼레이션|taste 테이스트|aquaculture 아쿠아컬처|
surveillance 서베일런스|innovative 이노베이티브|tour 투어|identity 아이덴티티|compact 콤팩트|general 제너럴|
bicycle 바이시클|formula 포뮬러|championship 챔피언십|essential 에센셜|patients 페이션츠|chemistry 케미스트리|
special 스페셜|access 액세스|resources 리소시스|device 디바이스|swan 스완|tomorrow 투모로|mother 마더|
movies 무비스|paper 페이퍼|book 북|friendship 프렌드십|citizen 시티즌|bio 바이오|anything 애니싱|lite 라이트|
aviation 에이비에이션|texas 텍사스|vietnam 베트남|improvement 임프루브먼트|manufacturing 매뉴팩처링|
quality 퀄리티|match 매치|united 유나이티드|sports 스포츠|orchestra 오케스트라|drive 드라이브|
generation 제너레이션|venture 벤처|stewardship 스튜어드십|union 유니언|reputation 레퓨테이션|white 화이트|
media 미디어|deal 딜|metal 메탈|powder 파우더|horizon 호라이즌|incubator 인큐베이터|idea 아이디어|disease 디지즈|
genome 게놈|open 오픈|magnesium 마그네슘|sustainability 서스테이너빌리티|reality 리얼리티|
james 제임스|michael 마이클|george 조지|mike 마이크|williams 윌리엄스|edward 에드워드|alan 앨런|
ost 오에스티|ebs 이비에스|ott 오티티|abc 에이비씨|vod 브이오디|vip 브이아이피|ok 오케이|ev 이브이|epl 이피엘|
ocn 오씨엔|iot 아이오티|imf 아이엠에프|dna 디엔에이|adhd 에이디에이치디|iptv 아이피티브이|etf 이티에프|eu 이유|
ic 아이씨|rotc 알오티씨|cu 씨유|kpop 케이팝|xbox 엑스박스|kcal 킬로칼로리|dr 닥터|hmm 흠|gym 짐|
ha 하|haha 하하|hahaha 하하하|hankook 한국|guam 괌
"""


def main():
    pairs = []
    for item in WORDS.replace("\n", "").split("|"):
        item = item.strip()
        if item:
            term, reading = item.split(" ", 1)
            pairs.append((term, reading))
    assert len({t for t, _ in pairs}) == len(pairs), "duplicate term"
    owned = set()
    for p in (ROOT / "lexicons/ko-KR").glob("*.jsonl"):
        if p.name == "english-common-v24.jsonl":
            continue
        for line in p.read_text(encoding="utf-8").splitlines():
            if line.strip():
                r = json.loads(line)
                if r.get("scope", "system") == "system":
                    owned.add(r["term"].lower())
    rows, skipped = [], []
    for term, reading in pairs:
        if term in owned:
            skipped.append(term)
            continue
        rows.append(dict(id="english-common-v24-" + term, source_lang="en-US", target_lang="ko-KR", term=term,
                         aliases=[], pronunciation=reading, category="common", review_status="pending",
                         evidence=[SOURCE],
                         notes="Authored 2026-10-03: common English word read in Korean context by the 외래어 "
                               "표기법 English rules and established usage; English context stays English. "
                               "Selected because the spelling guess was wrong in Korean-context text."))
    (ROOT / "lexicons/ko-KR/english-common-v24.jsonl").write_text(
        "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in rows), encoding="utf-8")
    src = ROOT / "lexicons/sources/project.json"
    data = json.loads(src.read_text(encoding="utf-8"))
    if not any(s["id"] == SOURCE for s in data["sources"]):
        data["sources"].append(dict(
            id=SOURCE, title="Project readings of common English words in Korean context, v24",
            url="https://www.korean.go.kr/front/page/pageView.do?mn_id=97&page_id=P000124", kind="editorial-policy",
            checked_at="2026-10-03",
            note="Authored by the project from the 외래어 표기법 English rules and established usage. Words were "
                 "chosen by frequency in local Korean-context text whose spelling guess was wrong; no reading is "
                 "copied from a corpus."))
        src.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    # All-capital spellings stay letters (AS 에이에스, IS 아이에스, HMM 에이치엠엠): every entry is
    # lexicon_case lower, kept as one generated block in the ko-KR numeric rules.
    rules = ROOT / "resources/languages/ko-KR/rules/numeric.tsv"
    begin, end = "# BEGIN english-common-v24 lexicon_case (tools/lexicon/english/author_common.py)\n", \
        "# END english-common-v24 lexicon_case\n"
    text = rules.read_text(encoding="utf-8")
    if begin in text:
        text = text[:text.index(begin)] + text[text.index(end) + len(end):]
    block = begin + "".join("lexicon_case\tko-KR\ten-US:%s\tlower\n" % r["term"] for r in rows) + end
    rules.write_text(text.rstrip("\n") + "\n" + block, encoding="utf-8")
    print("entries", len(rows), "skipped (owned)", skipped)
    subprocess.run([sys.executable, str(ROOT / "tools/lexicon/manage.py"), "build"], check=True)


if __name__ == "__main__":
    main()
