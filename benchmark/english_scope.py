"""Which sentences an English-only TN evaluation scores.

English TN is evaluated on English letters, digits and symbols only. A sentence
that also contains Hangul, Han, Kana, Cyrillic or another script is a mixed
sentence whose reading is another language's (or the matrix language's)
decision, so it is left out of the English denominator and reported as out of
scope instead of being scored as an English failure.

Latin letters with diacritics (café, Nguyễn) stay in scope: they are English
text's loanwords and names. A single Greek letter (Δ ( x ), 5 μm) is a symbol in
English technical text and stays in scope; two or more in a row are Greek words.
"""
import unicodedata

VERSION = "english-scope/1"
SCRIPT_NAMES = {"CJK": "HAN", "HIRAGANA": "KANA", "KATAKANA": "KANA",
                "KATAKANA-HIRAGANA": "KANA", "HALFWIDTH": "KANA"}


def _latin_like(ch):
    code = ord(ch)
    return (code < 0x250                          # ASCII, Latin-1 (µ ª º), Latin Extended-A/B
            or 0x2100 <= code < 0x2150            # letterlike symbols (ℓ ℃ №)
            or unicodedata.category(ch) == "Lm"  # modifier letters (ʻ ˈ)
            or "LATIN" in unicodedata.name(ch, ""))


def foreign_scripts(text):
    """Sorted non-English scripts whose letters appear in text; empty means in scope."""
    found = set()
    for at, ch in enumerate(text):
        if not unicodedata.category(ch).startswith("L") or _latin_like(ch):
            continue
        word = unicodedata.name(ch, "UNKNOWN").split()[0]
        if word == "GREEK":
            neighbours = text[at - 1:at] + text[at + 1:at + 2]
            if not any(unicodedata.name(n, "").startswith("GREEK") for n in neighbours):
                continue                          # a lone Greek letter is a symbol (Δ, μm)
        found.add(SCRIPT_NAMES.get(word, word))
    return sorted(found)
