#include "korean_phoneme.h"

/* Korean pronunciation output (phase 1, 2026-10-09; project-owner decision:
 * colloquial standard, Hangul pronunciation string, no vowel length).
 *
 * Each Hangul syllable is rewritten in place by the syllable it is spoken as,
 * so the byte length and every span offset stay the same. A word (a run of
 * Hangul syllables) is read as a whole:
 *   1. a `phoneme_word` row (stdict pronunciations the rules miss, and words
 *      ending in 의) gives the whole word;
 *   2. a listed word followed by a `phoneme_suffix` (조사, 어미) keeps the
 *      word's reading and reads its last syllable with the suffix;
 *   3. otherwise the rules of 표준발음법 below read it: coda neutralization and
 *      double codas, liaison, ㅎ contraction and deletion, palatalization,
 *      nasalization, liquidization, ㄹ after ㅁ ㅇ ㄱ ㅂ, obstruent tensing,
 *      and the colloquial vowels (initial 의 으, other 의 이, the particle 의 으,
 *      ㅖ after a consonant ㅔ, 져 저).
 * Rules apply inside a word only; ㄴ insertion and compound tensing come from
 * the dictionary. Text that is not Hangul is left as it is. */

enum { O_G, O_GG, O_N, O_D, O_DD, O_R, O_M, O_B, O_BB, O_S, O_SS, O_NG, O_J, O_JJ, O_CH, O_K, O_T, O_P, O_H };
enum {
    C_0, C_G, C_GG, C_GS, C_N, C_NJ, C_NH, C_D, C_L, C_LG, C_LM, C_LB, C_LS, C_LT, C_LP, C_LH,
    C_M, C_B, C_BS, C_S, C_SS, C_NG, C_J, C_CH, C_K, C_T, C_P, C_H
};
enum { V_A = 0, V_AE = 1, V_YA = 2, V_YAE = 3, V_EO = 4, V_E = 5, V_YEO = 6, V_YE = 7, V_O = 8, V_YO = 12,
       V_U = 13, V_YU = 17, V_EU = 18, V_UI = 19, V_I = 20 };

typedef struct {
    int o, v, c, fixed;
} syl_t;

#define MAX_WORD 64

/* The onset a single coda becomes before a vowel (-1: none). */
static const int coda_onset[28] = {-1, O_G, O_GG, -1, O_N, -1, -1, O_D, O_R, -1, -1, -1, -1, -1, -1, -1,
                                   O_M, O_B, -1, O_S, O_SS, -1, O_J, O_CH, O_K, O_T, O_P, -1};

static int tense(int o) {
    switch (o) {
    case O_G: return O_GG;
    case O_D: return O_DD;
    case O_B: return O_BB;
    case O_S: return O_SS;
    case O_J: return O_JJ;
    default: return o;
    }
}

/* The representative coda before a consonant; *trigger is set when the coda
 * tenses a following plain obstruent (an obstruent coda, or a verb-stem double
 * coda: 앉고 안꼬, 젊지 점찌, 넓게 널께, 핥다 할따). */
static int neutral(int c, int next, int *trigger) {
    *trigger = 0;
    switch (c) {
    case C_G: case C_GG: case C_K: case C_GS: *trigger = 1; return C_G;
    case C_LG: *trigger = 1; return next == O_G ? C_L : C_G;
    case C_N: case C_NH: return C_N;
    case C_NJ: *trigger = 1; return C_N;
    case C_D: case C_S: case C_SS: case C_J: case C_CH: case C_T: case C_H: *trigger = 1; return C_D;
    case C_L: case C_LH: return C_L;
    case C_LB: case C_LS: case C_LT: *trigger = 1; return C_L;
    case C_LM: *trigger = 1; return C_M;
    case C_M: return C_M;
    case C_B: case C_P: case C_BS: case C_LP: *trigger = 1; return C_B;
    case C_NG: return C_NG;
    default: return c;
    }
}

static int final_coda(int c) {
    int trigger;
    return c == C_0 ? C_0 : neutral(c, -1, &trigger);
}

static void vowels_before(syl_t *s, size_t n, int particle) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (s[i].fixed)
            continue;
        if (s[i].v == V_UI) {
            /* 의사 으사, 회의 회이, 나라의 나라으, 희망 히망 */
            if (s[i].o == O_NG)
                s[i].v = (i == 0 && particle != 0) || (int)i == particle ? V_EU : V_I;
            else
                s[i].v = V_I;
        } else if (s[i].v == V_YE && s[i].o != O_NG)
            s[i].v = V_E; /* 시계 시게 */
    }
}

static void vowels_after(syl_t *s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (s[i].fixed)
            continue;
        if (s[i].v == V_YE && s[i].o != O_NG)
            s[i].v = V_E; /* 간예 가네 */
        if (!(s[i].o == O_J || s[i].o == O_JJ || s[i].o == O_CH))
            continue;
        /* 가져 가저, 다쳐 다처 */
        if (s[i].v == V_YA) s[i].v = V_A;
        else if (s[i].v == V_YAE) s[i].v = V_AE;
        else if (s[i].v == V_YEO) s[i].v = V_EO;
        else if (s[i].v == V_YE) s[i].v = V_E;
        else if (s[i].v == V_YO) s[i].v = V_O;
        else if (s[i].v == V_YU) s[i].v = V_U;
    }
}

static int y_vowel(int v) {
    return v == V_I || v == V_YA || v == V_YAE || v == V_YEO || v == V_YE || v == V_YO || v == V_YU;
}

static void boundary(syl_t *l, syl_t *r) {
    int c = l->c, o = r->o, trigger;
    if (c == C_0 || r->fixed)
        return;
    if (o == O_NG) {
        /* Liaison before a vowel. */
        switch (c) {
        case C_NG: return;
        case C_H: l->c = C_0; return;                       /* 좋아 조아 */
        case C_NH: l->c = C_0; r->o = O_N; return;          /* 않아 아나 */
        case C_LH: l->c = C_0; r->o = O_R; return;          /* 싫어 시러 */
        case C_GS: l->c = C_G; r->o = O_SS; return;         /* 넋이 넉씨 */
        case C_NJ: l->c = C_N; r->o = O_J; return;          /* 앉아 안자 */
        case C_LG: l->c = C_L; r->o = O_G; return;          /* 닭을 달글 */
        case C_LM: l->c = C_L; r->o = O_M; return;          /* 젊어 절머 */
        case C_LB: l->c = C_L; r->o = O_B; return;          /* 넓어 널버 */
        case C_LS: l->c = C_L; r->o = O_SS; return;
        case C_LT:
            l->c = C_L;
            r->o = r->v == V_I ? O_CH : O_T;                 /* 핥이 할치 */
            return;
        case C_LP: l->c = C_L; r->o = O_P; return;          /* 읊어 을퍼 */
        case C_BS: l->c = C_B; r->o = O_SS; return;         /* 값이 갑씨 */
        case C_D:
        case C_T:
            l->c = C_0;
            r->o = r->v == V_I ? (c == C_D ? O_J : O_CH) : coda_onset[c]; /* 굳이 구지, 같이 가치 */
            return;
        default:
            if (coda_onset[c] >= 0) {
                r->o = coda_onset[c];
                l->c = C_0;
            }
            return;
        }
    }
    if (o == O_H) {
        /* ㅎ after an obstruent aspirates it: 축하 추카, 입학 이팍, 못하다 모타다, 닫히다 다치다. */
        switch (c) {
        case C_G: case C_GG: case C_K: l->c = C_0; r->o = O_K; return;
        case C_GS: l->c = C_G; r->o = O_K; return;
        case C_LG: l->c = C_L; r->o = O_K; return;
        case C_D: case C_T:
            l->c = C_0;
            r->o = r->v == V_I ? O_CH : O_T;
            return;
        case C_S: case C_SS: l->c = C_0; r->o = O_T; return;
        case C_J: case C_CH:
            l->c = C_0;
            r->o = y_vowel(r->v) ? O_CH : O_T;
            return;
        case C_NJ: l->c = C_N; r->o = O_CH; return;
        case C_B: case C_P: l->c = C_0; r->o = O_P; return;
        case C_LB: l->c = C_L; r->o = O_P; return;
        case C_BS: l->c = C_B; r->o = O_P; return;
        case C_H: l->c = C_0; return;
        case C_NH: l->c = C_N; return;
        case C_LH: l->c = C_L; return;
        default: return; /* 말하다, 전화: ㅎ is kept */
        }
    }
    if (c == C_H || c == C_NH || c == C_LH) {
        int rest = c == C_H ? C_0 : c == C_NH ? C_N : C_L;
        if (o == O_G || o == O_D || o == O_J) {
            r->o = o == O_G ? O_K : o == O_D ? O_T : O_CH;   /* 놓고 노코, 많다 만타, 싫지 실치 */
            l->c = rest;
        } else if (o == O_S) {
            r->o = O_SS;                                      /* 닿소 다쏘, 많소 만쏘 */
            l->c = rest;
        } else if (o == O_N) {
            if (c == C_LH) {
                l->c = C_L;                                   /* 뚫는 뚤른 */
                r->o = O_R;
            } else
                l->c = C_N;                                   /* 놓는 논는, 않는 안는 */
        } else
            l->c = c == C_H ? C_D : rest;
        return;
    }
    c = neutral(c, o, &trigger);
    if (o == O_N || o == O_M) {
        /* 국민 궁민, 밥맛 밤맏, 맏며느리 만며느리 */
        if (c == C_G) c = C_NG;
        else if (c == C_D) c = C_N;
        else if (c == C_B) c = C_M;
    } else if (o == O_R) {
        if (c == C_M || c == C_NG)
            o = O_N;                                          /* 대통령 대통녕, 종로 종노 */
        else if (c == C_G) { o = O_N; c = C_NG; }             /* 백리 뱅니 */
        else if (c == C_B) { o = O_N; c = C_M; }              /* 협력 혐녁 */
        else if (c == C_D) { o = O_N; c = C_N; }
        else if (c == C_N)
            c = C_L;                                          /* 신라 실라 */
    }
    if (c == C_L && o == O_N)
        o = O_R;                                              /* 칼날 칼랄 */
    if (trigger)
        o = tense(o);                                         /* 학교 학꾜, 읽다 익따 */
    l->c = c;
    r->o = o;
}

/* Stems whose double coda reads its second consonant: 밟 ㅂ (밟다 밥따), 넓 before
   ㅈ ㄷ (넓죽 넙쭉, 넓둥글다 넙뚱글다); nouns in ㄺ read ㄱ before ㄱ (닭갈비 닥깔비). */
static void irregular_codas(syl_t *s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        int next = i + 1 < n ? s[i + 1].o : -1;
        if (s[i].c == C_LB && s[i].o == O_B && s[i].v == V_A && next != O_NG)
            s[i].c = C_B;
        else if (s[i].c == C_LB && s[i].o == O_N && s[i].v == V_EO && (next == O_J || next == O_D))
            s[i].c = C_B;
        else if (s[i].c == C_LG && next == O_G &&
                 ((s[i].o == O_D && s[i].v == V_A) || (s[i].o == O_H && s[i].v == V_EU) ||
                  (s[i].o == O_CH && s[i].v == V_I) || (s[i].o == O_S && s[i].v == V_A)))
            s[i].c = C_G;
    }
}

static void apply_rules(syl_t *s, size_t n, int particle) {
    size_t i;
    irregular_codas(s, n);
    vowels_before(s, n, particle);
    for (i = 0; i + 1 < n; ++i)
        boundary(&s[i], &s[i + 1]);
    if (n)
        s[n - 1].c = final_coda(s[n - 1].c);
    vowels_after(s, n);
}

static int decode_word(const char *text, size_t n, syl_t *s, size_t limit) {
    size_t i, count = 0;
    for (i = 0; i + 3 <= n; i += 3) {
        uint32_t cp;
        size_t w;
        if (count >= limit || ttf_utf8_decode(text + i, n - i, &cp, &w) != 1 || w != 3 || cp < 0xac00 ||
            cp > 0xd7a3)
            return -1;
        cp -= 0xac00;
        s[count].o = (int)(cp / 588);
        s[count].v = (int)(cp % 588 / 28);
        s[count].c = (int)(cp % 28);
        s[count].fixed = 0;
        ++count;
    }
    return i == n ? (int)count : -1;
}

static void encode(const syl_t *s, char *out) {
    uint32_t cp = 0xac00 + (uint32_t)((s->o * 21 + s->v) * 28 + s->c);
    out[0] = (char)(0xe0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[2] = (char)(0x80 | (cp & 0x3f));
}

static const char *lookup(const ttf_engine_t *e, const char *kind, const char *text, size_t n) {
    char key[MAX_WORD * 3 + 1];
    if (n >= sizeof(key))
        return NULL;
    memcpy(key, text, n);
    key[n] = 0;
    return ttf_resource_get(e, kind, "ko-KR", key, NULL);
}

/* Read one word (n syllables, 3 bytes each) into out (same size). */
static void read_word(const ttf_engine_t *e, const char *text, size_t bytes, syl_t *out) {
    syl_t s[MAX_WORD], p[MAX_WORD];
    int n = decode_word(text, bytes, s, MAX_WORD), particle = -1;
    const char *hit;
    int k;
    if (n <= 0)
        return;
    memcpy(out, s, (size_t)n * sizeof(*s));
    hit = lookup(e, "phoneme_word", text, bytes);
    if (hit && decode_word(hit, strlen(hit), p, MAX_WORD) == n) {
        memcpy(out, p, (size_t)n * sizeof(*p));
        return;
    }
    /* 물고기를 물꼬기를: a listed word with a suffix keeps the word's reading. */
    for (k = n - 1; k >= 1; --k) {
        const char *word = lookup(e, "phoneme_word", text, (size_t)k * 3);
        if (!word || !lookup(e, "phoneme_suffix", text + k * 3, bytes - (size_t)k * 3) ||
            decode_word(word, strlen(word), p, MAX_WORD) != k)
            continue;
        memcpy(out, p, (size_t)(k - 1) * sizeof(*p));
        /* The last syllable: the word's onset and vowel, the written coda. */
        out[k - 1].o = p[k - 1].o;
        out[k - 1].v = p[k - 1].v;
        out[k - 1].c = s[k - 1].c;
        out[k - 1].fixed = 1;
        memcpy(out + k, s + k, (size_t)(n - k) * sizeof(*s));
        apply_rules(out + k - 1, (size_t)(n - k + 1),
                    s[k].o == O_NG && s[k].v == V_UI ? 1 : -1);
        out[k - 1].fixed = 0;
        return;
    }
    /* 나라의 나라으: a final 의 is the particle unless the word is listed (회의). */
    if (n >= 2 && s[n - 1].o == O_NG && s[n - 1].v == V_UI && s[n - 1].c == C_0)
        particle = n - 1;
    apply_rules(out, (size_t)n, particle);
}

ttf_status_t ttf_korean_phonemize(const ttf_engine_t *e, char *text, size_t length, size_t begin,
                                  size_t end) {
    size_t at = 0;
    if (!e || (!text && length) || begin > end || end > length)
        return TTF_EINVAL;
    while (at < length) {
        size_t start = at, stop;
        uint32_t cp;
        size_t w;
        if (ttf_utf8_decode(text + at, length - at, &cp, &w) != 1)
            return TTF_EUTF8;
        if (cp < 0xac00 || cp > 0xd7a3) {
            at += w;
            continue;
        }
        stop = at;
        while (stop + 3 <= length && ttf_utf8_decode(text + stop, length - stop, &cp, &w) == 1 && w == 3 &&
               cp >= 0xac00 && cp <= 0xd7a3)
            stop += 3;
        at = stop;
        if (stop <= begin || start >= end || (stop - start) / 3 > MAX_WORD)
            continue;
        {
            syl_t out[MAX_WORD];
            size_t i, count = (stop - start) / 3;
            read_word(e, text + start, stop - start, out);
            for (i = 0; i < count; ++i) {
                size_t pos = start + i * 3;
                if (pos >= begin && pos + 3 <= end)
                    encode(&out[i], text + pos);
            }
        }
    }
    return TTF_OK;
}
