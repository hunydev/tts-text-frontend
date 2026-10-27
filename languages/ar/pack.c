/* Arabic pack, phase 1 (2026-10-22, project owner: Arabic next, with the owner's evaluation
 * set benchmark/datasets/user-tn-eval ar). Modern Standard Arabic readings without short
 * vowels. Numbers agree with the counted noun: 3-10 take the opposite gender form and the
 * plural noun (ثلاثة رجال, ثلاث سيارات), 11-99 the accusative singular (أحد عشر طالبا), 2 the
 * dual alone (ساعتين), 100, 1000 … the singular (مئة ريال). The oblique case (اثنين, عشرين)
 * is read unless the noun takes the accusative alif after a nominative written form. Dates
 * read the day as an ordinal (الخامس من مارس), times the hour as a feminine ordinal
 * (التاسعة والنصف), money with its minor unit (ريالا وخمسين هللة), phone numbers and codes
 * digit by digit, Latin letters by their Arabic names (إس في), addresses by known words or
 * letter names. The core folds Arabic-Indic digits and ٫ ٬ ٪ to ASCII; the pack takes an
 * Arabic sentence in any request (arabic_sentence) and works only there (declared_only).
 *
 * classify decides the extent and writes the reading as items in value (and secondary when
 * long); verbalize reads the items. */
#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/core/internal.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AR "ar"
#define TRY(x)                                                                                     \
    do {                                                                                           \
        ttf_status_t try_status = (x);                                                             \
        if (try_status != TTF_OK)                                                                  \
            return try_status;                                                                     \
    } while (0)

static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, AR, key, NULL);
}
static int digit(char c) {
    return c >= '0' && c <= '9';
}
static int alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static int has(const char *t, size_t n, size_t p, const char *s) {
    size_t k = strlen(s);
    return p + k <= n && !memcmp(t + p, s, k);
}
/* An Arabic letter, tatweel or short vowel mark (two bytes) at p: no digit, no ، ؛ ؟ ٪ ٫ ٬. */
static int ar_at(const char *t, size_t n, size_t p) {
    const unsigned char *u = (const unsigned char *)t + p;
    if (p + 2 > n || u[0] < 0xd8 || u[0] > 0xdb)
        return 0;
    if (u[0] == 0xd8 && (u[1] == 0x8c || u[1] == 0x9b || u[1] == 0x9f))
        return 0;
    if (u[0] == 0xd9 && u[1] >= 0xa0 && u[1] <= 0xad)
        return 0;
    if (u[0] == 0xdb && u[1] >= 0xb0 && u[1] <= 0xb9)
        return 0;
    return 1;
}
/* A short vowel mark or the tatweel at p (dropped from a word for lookups). */
static int mark_at(const char *t, size_t n, size_t p) {
    const unsigned char *u = (const unsigned char *)t + p;
    return p + 2 <= n && u[0] == 0xd9 && (u[1] == 0x80 || (u[1] >= 0x8b && u[1] <= 0x9f) || u[1] == 0xb0);
}

/* ---- words around a number ---- */

/* The Arabic word starting at q after spaces: its end, its plain form (marks dropped) in w. */
static size_t word_after(const char *t, size_t n, size_t q, char *w, size_t cap, size_t *begin) {
    size_t z, k = 0;
    while (q < n && t[q] == ' ')
        ++q;
    if (begin)
        *begin = q;
    for (z = q; z < n && ar_at(t, n, z); z += 2)
        if (!mark_at(t, n, z) && k + 2 < cap) {
            w[k++] = t[z];
            w[k++] = t[z + 1];
        }
    w[k] = 0;
    return z == q ? 0 : z;
}
/* The Arabic word ending at p before spaces: its begin, its plain form in w; (size_t)-1 when
   there is none. */
static size_t word_before(const char *t, size_t p, char *w, size_t cap) {
    size_t q = p, b, a, k = 0;
    while (q && t[q - 1] == ' ')
        --q;
    b = q;
    while (b >= 2 && ar_at(t, q, b - 2))
        b -= 2;
    if (b == q) {
        w[0] = 0;
        return (size_t)-1;
    }
    for (a = b; a < q; a += 2)
        if (!mark_at(t, q, a) && k + 2 < cap) {
            w[k++] = t[a];
            w[k++] = t[a + 1];
        }
    w[k] = 0;
    return b;
}
/* The cue form of a word: a leading و or ف dropped, بال كال لل read as ال (للموسم الموسم). */
static void cue_form(const char *w, char *out, size_t cap) {
    const char *s = w;
    if ((has(s, strlen(s), 0, "و") || has(s, strlen(s), 0, "ف")) && strlen(s) > 6)
        s += 2;
    if (has(s, strlen(s), 0, "بال") || has(s, strlen(s), 0, "كال"))
        snprintf(out, cap, "ال%s", s + 6);
    else if (has(s, strlen(s), 0, "لل") && strlen(s) > 6)
        snprintf(out, cap, "ال%s", s + 4);
    else
        snprintf(out, cap, "%s", s);
}
/* The cue of the word before p (ordm, ordf, hour, phone, code) or NULL. */
static const char *cue_before(const ttf_engine_t *e, const char *t, size_t p) {
    char w[64], c[72];
    if (word_before(t, p, w, sizeof(w)) == (size_t)-1)
        return NULL;
    cue_form(w, c, sizeof(c));
    return res(e, "ar_cue", c);
}
/* A phone or code cue among the four words before p. */
static const char *cue_near(const ttf_engine_t *e, const char *t, size_t p, const char *kind) {
    size_t q = p, k;
    for (k = 0; k < 6; ++k) {
        char w[64], c[72];
        size_t b = word_before(t, q, w, sizeof(w));
        const char *v;
        if (b == (size_t)-1)
            return NULL;
        cue_form(w, c, sizeof(c));
        v = res(e, "ar_cue", c);
        if (v && !strcmp(v, kind))
            return v;
        q = b;
    }
    return NULL;
}
static int ends_with(const char *w, const char *s) {
    size_t a = strlen(w), b = strlen(s);
    return a >= b && !memcmp(w + a - b, s, b);
}
static int word_is(const char *w, const char *list) {
    size_t l = strlen(w);
    const char *s = list;
    while (*s) {
        const char *bar = strchr(s, '|');
        size_t k = bar ? (size_t)(bar - s) : strlen(s);
        if (k == l && !memcmp(s, w, l))
            return 1;
        if (!bar)
            break;
        s = bar + 1;
    }
    return 0;
}

/* ---- counted nouns ---- */

typedef struct {
    char key[48];          /* the ar_noun key, or "" for a noun known only by its gender */
    char form[48];         /* the written form */
    int fem;               /* feminine noun */
    int written;           /* s singular, a accusative, p plural, d dual, ? unknown */
    int alif;              /* the noun takes the accusative alif (ريالا) */
    int abbreviation;      /* written as a symbol or abbreviation (ر.س, كم, $) */
    size_t end;            /* end of the noun in the text */
} noun_t;

static int noun_field(const char *v, int k, char *out, size_t cap) {
    const char *s = v;
    size_t l;
    while (k-- > 0) {
        s = strchr(s, '|');
        if (!s)
            return 0;
        ++s;
    }
    l = strcspn(s, "|");
    if (l >= cap)
        return 0;
    memcpy(out, s, l);
    out[l] = 0;
    return 1;
}
/* A noun by its ar_noun key. */
static int noun_by_key(const ttf_engine_t *e, const char *key, noun_t *x) {
    const char *v = res(e, "ar_noun", key);
    char g[4], sg[48], acc[48];
    if (!v || !noun_field(v, 0, g, sizeof(g)) || !noun_field(v, 1, sg, sizeof(sg)) ||
        !noun_field(v, 4, acc, sizeof(acc)))
        return 0;
    snprintf(x->key, sizeof(x->key), "%s", key);
    x->fem = g[0] == 'f';
    x->alif = strcmp(sg, acc) != 0;
    return 1;
}
/* The written word w as a counted noun: a singular key, its accusative (ريالا), its plural in
   ات, or a gender entry; otherwise the gender by its ending (ة, ات feminine). */
static void noun_of_word(const ttf_engine_t *e, const char *w, noun_t *x) {
    char k[52];
    const char *g;
    size_t l = strlen(w);
    memset(x, 0, sizeof(*x));
    snprintf(x->form, sizeof(x->form), "%s", w);
    x->written = '?';
    if (noun_by_key(e, w, x)) {
        x->written = 's';
        return;
    }
    if (l > 4 && ends_with(w, "ا")) {
        snprintf(k, sizeof(k), "%.*s", (int)(l - 2), w);
        if (noun_by_key(e, k, x)) {
            x->written = 'a';
            return;
        }
    }
    if ((g = res(e, "ar_plural", w)) != NULL && noun_by_key(e, g, x)) {
        x->written = 'p';
        return;
    }
    if ((g = res(e, "ar_gender", w)) != NULL) {
        x->fem = g[0] == 'f';
        x->written = 'p';
        return;
    }
    if (l > 6 && ends_with(w, "ات")) {
        snprintf(k, sizeof(k), "%.*sة", (int)(l - 4), w);
        if (noun_by_key(e, k, x)) {
            x->written = 'p';
            return;
        }
        snprintf(k, sizeof(k), "%.*s", (int)(l - 4), w);
        if (noun_by_key(e, k, x)) {
            x->written = 'p';
            return;
        }
        /* تقييمات, تعديلات: the plural of a masculine تفعيل noun */
        x->fem = !has(w, l, 0, "ت");
        x->written = 'p';
        return;
    }
    x->fem = ends_with(w, "ة") || (ends_with(w, "ى") && !res(e, "ar_gender", w));
    x->alif = !x->fem && !ends_with(w, "ا") && !ends_with(w, "ي") && !ends_with(w, "و") &&
              !ends_with(w, "ء");
    if (ends_with(w, "ا") && l > 4)
        x->written = 'a';
}
/* The form of a known noun after the integer n: d dual, p plural, a accusative, s singular. */
static char noun_case(unsigned long long n) {
    unsigned r = (unsigned)(n % 100);
    if (n == 2)
        return 'd';
    if (r >= 3 && r <= 10)
        return 'p';
    if (r >= 11 && r <= 99)
        return 'a';
    return 's';
}
/* The text of a known noun in a form (s, d, p, a). */
static const char *noun_text(const ttf_engine_t *e, const char *key, char form, char *out, size_t cap) {
    const char *v = res(e, "ar_noun", key);
    int k = form == 's' ? 1 : form == 'd' ? 2 : form == 'p' ? 3 : 4;
    if (!v || !noun_field(v, k, out, cap))
        return NULL;
    return out;
}

/* ---- readings ---- */

typedef struct {
    const ttf_engine_t *e;
    ttf_buffer_t *b;
    int and_next; /* the next word is joined with و (مئة وخمسة) */
} out_t;

/* A word, with a space before it unless it starts the output; و attached when pending. */
static ttf_status_t emit(out_t *o, const char *s) {
    if (!s)
        return TTF_ERESOURCE;
    if (o->b->length && o->b->data[o->b->length - 1] != ' ')
        TRY(ttf_buffer_puts(o->b, " "));
    if (o->and_next) {
        o->and_next = 0;
        TRY(ttf_buffer_puts(o->b, "و"));
    }
    return ttf_buffer_puts(o->b, s);
}
static ttf_status_t num(out_t *o, const char *key) {
    return emit(o, res(o->e, "number", key));
}
static ttf_status_t numf(out_t *o, const char *fmt, unsigned v, const char *tail) {
    char key[16];
    snprintf(key, sizeof(key), fmt, v, tail);
    return num(o, key);
}

/* 1-999. fem: the counted noun is feminine; obl: the oblique case; construct: an exact 200
   before a noun (مئتي أسرة). */
static ttf_status_t below1000(out_t *o, unsigned v, int fem, int obl, int construct) {
    unsigned h = v / 100, r = v % 100;
    if (h == 1)
        TRY(num(o, "100"));
    else if (h == 2)
        TRY(num(o, r == 0 && construct ? "200c" : obl ? "200o" : "200"));
    else if (h)
        TRY(numf(o, "%u00", h, ""));
    if (!r)
        return TTF_OK;
    if (h)
        o->and_next = 1;
    if (r == 1)
        return num(o, fem ? "1f" : "1m");
    if (r == 2)
        return num(o, fem ? (obl ? "2fo" : "2f") : (obl ? "2mo" : "2m"));
    if (r <= 10)
        return numf(o, "%u%s", r, fem ? "f" : "m");
    if (r == 11)
        return num(o, fem ? "11f" : "11m");
    if (r == 12)
        return num(o, fem ? (obl ? "12fo" : "12f") : (obl ? "12mo" : "12m"));
    if (r < 20) {
        TRY(numf(o, "%u%s", r - 10, fem ? "f" : "m"));
        return num(o, fem ? "teenf" : "teenm");
    }
    if (r % 10 == 1)
        TRY(num(o, fem ? "1fc" : "1m"));
    else if (r % 10 == 2)
        TRY(num(o, fem ? (obl ? "2fo" : "2f") : (obl ? "2mo" : "2m")));
    else if (r % 10 == 8 && fem)
        TRY(num(o, "8fc")); /* ثمان وأربعين ساعة */
    else if (r % 10)
        TRY(numf(o, "%u%s", r % 10, fem ? "f" : "m"));
    if (r % 10)
        o->and_next = 1;
    return numf(o, "%u%s", r / 10 * 10, obl ? "o" : "");
}

/* An integer of up to 15 digits; flags: f feminine noun, o oblique, c construct before a noun
   (مئتي, ألفي), n no noun follows (a bare 12,500 reads اثني عشر ألفا وخمسمئة either way). */
static ttf_status_t cardinal(out_t *o, const char *digits, const char *flags) {
    static const char scale[] = {0, 'k', 'm', 'g', 't'};
    unsigned group[5] = {0};
    unsigned long long v = 0;
    int fem = strchr(flags, 'f') != NULL, obl = strchr(flags, 'o') != NULL,
        construct = strchr(flags, 'c') != NULL, first = 1, k;
    size_t i, len = strlen(digits);
    if (!len || len > 15)
        return TTF_EINVAL;
    for (i = 0; i < len; ++i) {
        if (!digit(digits[i]))
            return TTF_EINVAL;
        v = v * 10 + (unsigned)(digits[i] - '0');
    }
    if (!v)
        return num(o, "0");
    for (k = 0; k < 5; ++k) {
        group[k] = (unsigned)(v % 1000);
        v /= 1000;
    }
    for (k = 4; k >= 0; --k) {
        unsigned g = group[k], r = g % 100;
        int lower = 0, j;
        char key[8];
        if (!g)
            continue;
        for (j = 0; j < k; ++j)
            lower |= group[j] != 0;
        if (!first)
            o->and_next = 1;
        first = 0;
        if (!k) {
            TRY(below1000(o, g, fem, obl, construct));
            continue;
        }
        if (g == 1) {
            snprintf(key, sizeof(key), "%c1", scale[k]);
        } else if (g == 2) {
            snprintf(key, sizeof(key), "%c2%s", scale[k], construct && !lower ? "c" : obl ? "o" : "");
        } else {
            /* the multiplier takes the case of the whole number: خمسة وعشرون ألفا, خمسة وعشرين ألف
               (the pairs the owner set accepts) */
            TRY(below1000(o, g, 0, obl, 0));
            if (r >= 3 && r <= 10)
                snprintf(key, sizeof(key), "%cp", scale[k]);
            else if (r >= 11 && lower && (!obl || r == 11 || (r >= 13 && r <= 19)))
                snprintf(key, sizeof(key), "%ca", scale[k]);
            else
                snprintf(key, sizeof(key), "%c1", scale[k]);
        }
        TRY(num(o, key));
    }
    return TTF_OK;
}
/* Digits one by one (0 صفر); + زائد, letters by their names. */
static ttf_status_t digits_each(out_t *o, const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        char key[4];
        if (digit(s[i])) {
            if (s[i] == '0')
                TRY(num(o, "0"));
            else
                TRY(numf(o, "%u%s", (unsigned)(s[i] - '0'), "m"));
        } else if (s[i] == '+')
            TRY(num(o, "plus"));
        else if (alpha(s[i])) {
            key[0] = (char)(s[i] | 0x20);
            key[1] = 0;
            TRY(emit(o, res(o->e, "ar_letter", key)));
        }
    }
    return TTF_OK;
}
/* 23.4 ثلاثة وعشرين فاصلة أربعة: the decimals as a number of one or two digits without a
   leading zero, else one by one (2.05 اثنين فاصلة صفر خمسة, 3.673 ثلاثة فاصلة ستة سبعة ثلاثة). */
static ttf_status_t decimal(out_t *o, const char *v, const char *flags) {
    const char *dot = strchr(v, '.');
    char ip[24];
    char f2[8];
    if (!dot)
        return cardinal(o, v, flags);
    if ((size_t)(dot - v) >= sizeof(ip))
        return TTF_EINVAL;
    snprintf(ip, sizeof(ip), "%.*s", (int)(dot - v), v);
    snprintf(f2, sizeof(f2), "%s", strchr(flags, 'o') ? "o" : "");
    TRY(cardinal(o, *ip ? ip : "0", f2));
    TRY(num(o, "point"));
    if (dot[1] == '0' || strlen(dot + 1) > 2)
        return digits_each(o, dot + 1, strlen(dot + 1));
    return cardinal(o, dot + 1, f2);
}
/* An ordinal agreeing with a masculine or feminine noun (oblique in compounds). */
static ttf_status_t ordinal(out_t *o, unsigned n, int fem) {
    const char *g = fem ? "f" : "m";
    unsigned u = n % 10, t = n / 10;
    if (n == 100)
        return num(o, "ot100"); /* الحلقة المئة */
    if (n < 1000 && n % 100 == 0) {
        /* الصفحة 300 الصفحة الثلاثمئة */
        char key[8];
        snprintf(key, sizeof(key), "%u", n);
        if (res(o->e, "number", key)) {
            char word[64];
            snprintf(word, sizeof(word), "ال%s", res(o->e, "number", key));
            return emit(o, word);
        }
    }
    if (n == 0 || n > 99) {
        char d[24];
        snprintf(d, sizeof(d), "%u", n);
        return cardinal(o, d, "o");
    }
    if (n <= 10)
        return numf(o, "o%u%s", n, g);
    if (n < 20) {
        TRY(n == 11 ? num(o, fem ? "oc1f" : "oc1m") : numf(o, "o%u%s", n - 10, g));
        return num(o, fem ? "oteenf" : "oteenm");
    }
    if (u) {
        TRY(u == 1 ? num(o, fem ? "oc1f" : "oc1m") : numf(o, "o%u%s", u, g));
        o->and_next = 1;
    }
    return numf(o, "ot%u%s", t * 10, "");
}
/* The hour 1-12 as a feminine ordinal (الواحدة, التاسعة, الثانية عشرة). */
static ttf_status_t hour_word(out_t *o, unsigned h) {
    if (h == 1)
        return num(o, "h1");
    return ordinal(o, h, 1);
}
/* A counted noun after the integer n: its known form, or the written word. */
static ttf_status_t noun_after(out_t *o, const char *key, unsigned long long n, const char *written) {
    char text[64];
    if (*key && noun_text(o->e, key, noun_case(n), text, sizeof(text)))
        return emit(o, text);
    return written && *written ? emit(o, written) : TTF_OK;
}
/* The integer n with a known counted noun: the dual alone for 2 (ساعتين), the noun before
   واحد for 1 (ريال واحد), else the number and the noun in its form. */
static ttf_status_t counted(out_t *o, const char *digits, const char *key, const char *flags) {
    unsigned long long n = strtoull(digits, NULL, 10);
    char text[64];
    if (n == 2 && *key && noun_text(o->e, key, 'd', text, sizeof(text)))
        return emit(o, text);
    if (n == 1 && *key && noun_text(o->e, key, 's', text, sizeof(text))) {
        TRY(emit(o, text));
        if (word_is(key, "ساعة|دقيقة|ثانية|يوم|أسبوع|شهر|سنة|عام|ليلة"))
            return TTF_OK;
        return num(o, strchr(flags, 'f') ? "1f" : "1m");
    }
    TRY(cardinal(o, digits, flags));
    return noun_after(o, key, n, NULL);
}

/* ---- items: the reading classify writes and verbalize reads ----
   Items are separated by '|'; a leading & joins the item with و.
   N<flags>:digits cardinal, D<flags>:1.5 decimal, C<flags>:digits:key counted noun,
   K<form>:key a noun form (s d p a), O<m|f>:n ordinal, H:h hour, G:chars digits one by one,
   L:letters letter names, R:key a number word, W:text as written, M:n month, J:n Hijri month. */

typedef struct {
    char s[2 * TTF_IR_VALUE_SIZE];
    size_t n;
    int bad;
} items_t;

static void add(items_t *it, const char *fmt, ...) {
    va_list ap;
    int k;
    if (it->bad)
        return;
    if (it->n && it->n + 1 < sizeof(it->s))
        it->s[it->n++] = '|';
    va_start(ap, fmt);
    k = vsnprintf(it->s + it->n, sizeof(it->s) - it->n, fmt, ap);
    va_end(ap);
    if (k < 0 || it->n + (size_t)k >= sizeof(it->s))
        it->bad = 1;
    else
        it->n += (size_t)k;
}

static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end, const char *rule,
                     const items_t *it) {
    size_t count = 0, cut;
    if (it->bad || !it->n || end <= in->tokens[in->index].begin)
        return 0;
    /* value holds the first items, secondary the rest, cut at an item boundary */
    cut = it->n;
    if (cut >= sizeof(s->value)) {
        cut = sizeof(s->value) - 1;
        while (cut && it->s[cut] != '|')
            --cut;
        if (!cut || it->n - cut >= sizeof(s->secondary))
            return 0;
    }
    while (in->index + count < in->token_count && in->tokens[in->index + count].begin < end)
        ++count;
    memcpy(s->value, it->s, cut);
    s->value[cut] = 0;
    snprintf(s->secondary, sizeof(s->secondary), "%s", it->s + cut);
    snprintf(s->rule, sizeof(s->rule), "ar.%s", rule);
    snprintf(s->unit, sizeof(s->unit), "@ar-say");
    snprintf(s->source_lang, sizeof(s->source_lang), AR);
    s->input_end = end;
    s->type = TTF_SEM_COUNTER;
    return count;
}

/* Puts the item first (من before a range found after its first part). */
static void prepend(items_t *it, const char *item) {
    size_t l = strlen(item);
    if (it->bad || it->n + l + 2 >= sizeof(it->s))
        return;
    memmove(it->s + l + 1, it->s, it->n + 1);
    memcpy(it->s, item, l);
    it->s[l] = '|';
    it->n += l + 1;
}

/* ---- numbers in the text ---- */

typedef struct {
    size_t begin, end;
    char d[40];   /* the integer digits, group marks removed */
    char dec[48]; /* the number with its decimals (1.5) */
    int dot, comma;
} num_t;

static size_t parse_num(const char *t, size_t n, size_t p, num_t *x) {
    size_t q = p, k = 0;
    memset(x, 0, sizeof(*x));
    x->begin = p;
    while (q < n && digit(t[q])) {
        if (k + 1 < sizeof(x->d))
            x->d[k++] = t[q];
        ++q;
    }
    while (q + 3 < n && t[q] == ',' && digit(t[q + 1]) && digit(t[q + 2]) && digit(t[q + 3]) &&
           !(q + 4 < n && digit(t[q + 4]))) {
        if (k + 4 < sizeof(x->d)) {
            memcpy(x->d + k, t + q + 1, 3);
            k += 3;
        }
        q += 4;
        x->comma = 1;
    }
    x->d[k] = 0;
    snprintf(x->dec, sizeof(x->dec), "%s", x->d);
    if (q + 1 < n && t[q] == '.' && digit(t[q + 1])) {
        size_t r = q + 1;
        while (r < n && digit(t[r]))
            ++r;
        if (!(r + 1 < n && t[r] == '.' && digit(t[r + 1])) && r - q - 1 < 12) {
            x->dot = 1;
            snprintf(x->dec, sizeof(x->dec), "%s.%.*s", x->d, (int)(r - q - 1), t + q + 1);
            q = r;
        }
    }
    x->end = q;
    return q;
}
/* A dotted currency abbreviation at q (ر.س, د.إ, ج.م …): its key and end. */
static size_t currency_abbr(const ttf_engine_t *e, const char *t, size_t n, size_t q, char *key, size_t cap) {
    static const char *const abbr[] = {"ر.س", "د.إ", "ج.م", "د.ك", "د.ب", "د.أ", "ر.ق", "ر.ع", "ل.ل", NULL};
    int k;
    for (k = 0; abbr[k]; ++k)
        if (has(t, n, q, abbr[k]) && !(q + strlen(abbr[k]) < n && ar_at(t, n, q + strlen(abbr[k])))) {
            snprintf(key, cap, "%s", abbr[k]);
            return res(e, "ar_currency", key) ? q + strlen(abbr[k]) : 0;
        }
    return 0;
}
/* A Latin or symbol unit at q (km, GB, °C, °م, كم/س, ملم): its ar_unit noun key and end; the
   longest written form wins (كم/س before كم) and a unit ends where a word ends. */
static size_t unit_at(const ttf_engine_t *e, const char *t, size_t n, size_t q, char *noun, size_t cap) {
    size_t z = q;
    char key[32];
    while (z < n && z - q < 24 && t[z] != ' ' && t[z] != ',' && t[z] != ')' && t[z] != '(' && t[z] != '.' &&
           t[z] != ';' && t[z] != ':' && t[z] != '\n' && !digit(t[z]) &&
           !(z + 1 < n && (unsigned char)t[z] == 0xd8 &&
             ((unsigned char)t[z + 1] == 0x8c || (unsigned char)t[z + 1] == 0x9b || (unsigned char)t[z + 1] == 0x9f)))
        ++z;
    for (; z > q; --z) {
        const char *v;
        if (z < n && ((unsigned char)t[z] & 0xc0) == 0x80)
            continue; /* not a character boundary */
        if (z - q >= sizeof(key))
            continue;
        if (z < n && (alpha(t[z]) || (ar_at(t, n, z) && !mark_at(t, n, z))))
            continue; /* the word goes on */
        memcpy(key, t + q, z - q);
        key[z - q] = 0;
        if ((v = res(e, "ar_unit", key)) != NULL) {
            snprintf(noun, cap, "%s", v);
            return z;
        }
    }
    return 0;
}
static int scale_word(const char *w, char *key) {
    if (word_is(w, "ألف|آلاف|ألفا"))
        *key = 'k';
    else if (word_is(w, "مليون|ملايين|مليونا"))
        *key = 'm';
    else if (word_is(w, "مليار|مليارات|مليارا"))
        *key = 'g';
    else if (word_is(w, "تريليون|تريليونات"))
        *key = 't';
    else
        return 0;
    return 1;
}
/* The scale word after the integer n (مليون, ملايين, مليونا) as a number key. */
static void scale_key(char scale, const char *digits, int noun_follows, char *key, size_t cap) {
    unsigned long long v = strtoull(digits, NULL, 10);
    unsigned r = (unsigned)(v % 100);
    if (v == 2)
        snprintf(key, cap, "%c2%s", scale, noun_follows ? "c" : "o");
    else if (r >= 3 && r <= 10 && v < 1000)
        snprintf(key, cap, "%cp", scale);
    else
        snprintf(key, cap, "%c1", scale);
}
static int month_number(const ttf_engine_t *e, const char *w, int *hijri) {
    static const char *const levant[] = {"كانون الثاني", "شباط", "آذار", "نيسان", "أيار", "حزيران",
                                         "تموز", "آب", "أيلول", "تشرين الأول", "تشرين الثاني", "كانون الأول"};
    char k[4];
    int m;
    for (m = 1; m <= 12; ++m) {
        const char *v;
        snprintf(k, sizeof(k), "%d", m);
        if ((v = res(e, "ar_month", k)) != NULL && !strcmp(v, w)) {
            *hijri = 0;
            return m;
        }
        if ((v = res(e, "ar_hijri", k)) != NULL && !strcmp(v, w)) {
            *hijri = 1;
            return m;
        }
        if (!strcmp(levant[m - 1], w)) {
            *hijri = 0;
            return m;
        }
    }
    return 0;
}
/* A month name at q (two words for ربيع الأول, ذو القعدة, تشرين الثاني): its number and end. */
static size_t month_at(const ttf_engine_t *e, const char *t, size_t n, size_t q, int *month, int *hijri) {
    char w[64], w2[64], both[132];
    size_t e1 = word_after(t, n, q, w, sizeof(w), NULL), e2;
    if (!e1)
        return 0;
    if ((e2 = word_after(t, n, e1, w2, sizeof(w2), NULL)) != 0) {
        snprintf(both, sizeof(both), "%s %s", w, w2);
        if ((*month = month_number(e, both, hijri)) != 0)
            return e2;
    }
    if ((*month = month_number(e, w, hijri)) != 0)
        return e1;
    return 0;
}
/* هـ after a year (attached or after a space): its end, else 0. */
static size_t hijri_mark(const char *t, size_t n, size_t q) {
    size_t z = q;
    if (z < n && t[z] == ' ')
        ++z;
    if (has(t, n, z, "هـ") || (has(t, n, z, "ه") && !(z + 2 < n && ar_at(t, n, z + 2))))
        return z + (has(t, n, z, "هـ") ? 4 : 2);
    return 0;
}

/* ---- amounts: a number with what follows it ---- */

/* An exact 200 or 2000 ends the number before its noun: the construct form (مئتي أسرة, ألفي جنيه). */
static int construct_ok(const char *digits) {
    unsigned long long v = strtoull(digits, NULL, 10);
    return v % 1000 == 200 || v == 2000 || v == 2000000 || v == 2000000000ULL;
}

static int near_before(const char *t, size_t p, const char *list);

/* Words that never are a counted noun after a number (على, ثم, من …). */
static int function_word(const char *w) {
    return word_is(w, "على|إلى|الى|في|من|عن|مع|حتى|لدى|أو|او|ثم|بعد|قبل|عند|منذ|خلال|حسب|وفق|فقط|تقريبا|أي|"
                      "هو|هي|ما|لا|لم|لن|قد|كل|بين|نحو|حوالي|أكثر|أقل|فوق|تحت|عبر|ضمن|مثل|غير|سوى|إلا|"
                      "يوميا|سنويا|شهريا|أسبوعيا|فما|مقابل|لكل|بس|وبس|يا|وش|و");
}

/* The reading of the number x and what follows it at x->end: a percent sign or word, a scale
   word, a currency, a unit or a counted noun. oblique: the number reads in the oblique case
   (after a sign, the second number of a range) and a signed amount of money as a decimal.
   Returns the end, items in it. */
static size_t amount(const ttf_engine_t *e, const char *t, size_t n, const num_t *x, items_t *it,
                     const char *prefix_currency, int oblique) {
    size_t q = x->end, sp = q, z;
    char w[64], key[64], cur[64];
    noun_t no;
    if (sp < n && t[sp] == ' ')
        ++sp;
    /* 15% خمسة عشر بالمئة; ١٥ بالمئة, في المئة; 12.3 ٪ with a space */
    if (!prefix_currency && ((q < n && t[q] == '%') || (sp < n && t[sp] == '%') || has(t, n, sp, "٪"))) {
        add(it, "Do:%s", x->dec);
        add(it, "R:percent");
        return t[q] == '%' ? q + 1 : t[sp] == '%' ? sp + 1 : sp + strlen("٪");
    }
    z = word_after(t, n, q, w, sizeof(w), NULL);
    if (!prefix_currency && z && word_is(w, "بالمئة|بالمائة|بالمائه|بالميه")) {
        add(it, "Do:%s", x->dec);
        add(it, "R:percent");
        return z;
    }
    if (!prefix_currency && z && !strcmp(w, "في") && has(t, n, z, " المئة")) {
        add(it, "Do:%s", x->dec);
        add(it, "R:percent");
        return z + strlen(" المئة");
    }
    /* 332 ق.م ثلاثمئة واثنين وثلاثين قبل الميلاد */
    if (!x->dot && has(t, n, sp, "ق.م")) {
        add(it, "No:%s", x->d);
        add(it, "W:قبل الميلاد");
        return sp + strlen("ق.م");
    }
    /* 3.2 مليار ريال, $1.5 مليون, 60 ألف: a scale word, maybe a currency after it */
    if (z && scale_word(w, key)) {
        char scale = key[0], sk[8];
        size_t ce = 0, z2;
        const char *cv = prefix_currency;
        char cw[64];
        if (!cv) {
            if (z + 1 < n && t[z] == ' ' && (ce = currency_abbr(e, t, n, z + 1, cur, sizeof(cur))) != 0)
                cv = res(e, "ar_currency", cur);
            else if ((z2 = word_after(t, n, z, cw, sizeof(cw), NULL)) != 0 && (cv = res(e, "ar_currency", cw)) != NULL)
                ce = z2;
            else {
                size_t ue;
                char uk[16];
                if (z + 1 < n && t[z] == ' ' && (ue = unit_at(e, t, n, z + 1, uk, sizeof(uk))) != 0 &&
                    (cv = res(e, "ar_currency", uk)) != NULL)
                    ce = ue;
                else
                    cv = NULL;
            }
        }
        if (x->dot) {
            add(it, "Do:%s", x->dec);
            snprintf(sk, sizeof(sk), "%c1", scale);
        } else {
            add(it, "No:%s", x->d);
            scale_key(scale, x->d, cv != NULL, sk, sizeof(sk));
        }
        if (x->d[0] == '2' && !x->d[1] && !x->dot) {
            /* ٢ مليون أسرة مليوني أسرة: the dual of the scale alone */
            it->n = 0;
            it->s[0] = 0;
            snprintf(sk, sizeof(sk), "%c2%s", scale, "c");
            if (!(z + 1 < n && t[z] == ' ' && ar_at(t, n, z + 1)))
                snprintf(sk, sizeof(sk), "%c2o", scale);
        }
        add(it, "R:%s", sk);
        if (cv) {
            char nk[48];
            noun_field(cv, 0, nk, sizeof(nk));
            add(it, "Ks:%s", nk);
            return ce ? ce : z;
        }
        {
            char uk[48];
            size_t ue = z + 1 < n && t[z] == ' ' ? unit_at(e, t, n, z + 1, uk, sizeof(uk)) : 0;
            if (ue) {
                add(it, "Ks:%s", uk);
                return ue;
            }
        }
        return z;
    }
    /* a currency after the number (500 ر.س, 22 ريال, 84.60 دولارا, 10 SAR) */
    {
        size_t ce = 0;
        int abbreviation = 0, written_alif = 0;
        const char *cv = prefix_currency;
        if (!cv) {
            if ((ce = currency_abbr(e, t, n, sp, cur, sizeof(cur))) != 0) {
                cv = res(e, "ar_currency", cur);
                abbreviation = 1;
            } else if (z && (cv = res(e, "ar_currency", w)) != NULL) {
                ce = z;
                written_alif = ends_with(w, "ا");
            } else if (sp < n && alpha(t[sp])) {
                size_t le = sp;
                char lk[8];
                while (le < n && alpha(t[le]) && le - sp < 4)
                    ++le;
                if (le - sp == 3 && !(le < n && alpha(t[le]))) {
                    snprintf(lk, sizeof(lk), "%.*s", 3, t + sp);
                    if ((cv = res(e, "ar_currency", lk)) != NULL) {
                        ce = le;
                        abbreviation = 1;
                    }
                }
            }
        } else
            abbreviation = 1;
        if (cv) {
            char nk[48], sub[48], md[8];
            unsigned minor = 0;
            unsigned long long main = strtoull(x->d, NULL, 10);
            const char *dot = strchr(x->dec, '.');
            const char *case_flag;
            noun_field(cv, 0, nk, sizeof(nk));
            noun_field(cv, 1, sub, sizeof(sub));
            noun_field(cv, 2, md, sizeof(md));
            memset(&no, 0, sizeof(no));
            noun_by_key(e, nk, &no);
            case_flag = !oblique && (abbreviation || written_alif) && no.alif ? "" : "o";
            if (dot) {
                size_t fl = strlen(dot + 1), want = (size_t)atoi(md);
                /* more decimals than the minor unit has, a signed amount or a price or rate
                   (سعر صرف … 4.12 ريال): a decimal amount */
                if (fl > want || oblique ||
                    (!written_alif && !abbreviation && near_before(t, x->begin, "سعر|أسعار|أغلق|سجل|صرف|السهم|سهم|مقابل"))) {
                    add(it, "D%s:%s", oblique ? "o" : "", x->dec);
                    add(it, "Ks:%s", nk);
                    return ce ? ce : q;
                }
                minor = (unsigned)atoi(dot + 1);
                if (fl == 1)
                    minor *= 10;
            }
            if (main || !minor) {
                add(it, "C%s%s%s%s:%s:%s", no.fem ? "f" : "", case_flag, construct_ok(x->d) ? "c" : "",
                    !abbreviation && !written_alif ? "b" : "", *x->d ? x->d : "0", nk);
                if (!main) {
                    /* ٠ ريال صفر ريال */
                    it->n = 0;
                    it->bad = 0;
                    add(it, "R:0");
                    add(it, "Ks:%s", nk);
                }
            }
            if (minor) {
                /* the minor amount takes the case of a main amount with tens (أربعة وثمانون دولارا
                   وستون سنتا), else the oblique (تسعة دولارات وتسعة وتسعين سنتا); alone after an
                   abbreviation the nominative (0.750 د.ك سبعمئة وخمسون فلسا) */
                noun_t ms;
                char mk[16];
                const char *minor_case = main % 100 >= 20 || (!main && abbreviation) ? case_flag
                                         : abbreviation && !prefix_currency && main % 100 <= 10 ? "" : "o";
                memset(&ms, 0, sizeof(ms));
                noun_by_key(e, sub, &ms);
                snprintf(mk, sizeof(mk), "%u", minor);
                add(it, "%sC%s%s:%s:%s", main ? "&" : "", ms.fem ? "f" : "", minor_case, mk, sub);
            }
            return ce ? ce : q;
        }
    }
    /* a unit (45 ملم, 25 كم/س, 3 GB, -3°م, 65 بوصة) */
    {
        char nk[48];
        size_t ue = unit_at(e, t, n, q, nk, sizeof(nk));
        if (!ue && sp > q)
            ue = unit_at(e, t, n, sp, nk, sizeof(nk));
        if (ue && noun_by_key(e, nk, &no)) {
            if (x->dot) {
                /* 2.5 كم اثنين فاصلة خمسة كيلومتر: a decimal with the singular */
                add(it, "Do:%s", x->dec);
                add(it, "Ks:%s", nk);
            } else {
                size_t us = ue;
                int latin = 0;
                while (us > q && t[us - 1] != ' ' && us > x->end)
                    --us;
                latin = alpha(t[us]);
                (void)latin;
                add(it, "C%s%s%s:%s:%s", no.fem ? "f" : "", no.alif && !oblique ? "" : "o",
                    construct_ok(x->d) ? "c" : "", x->d, nk);
            }
            return ue;
        }
    }
    /* 2026, في 1990: a year (no noun follows it) */
    if (!x->dot && !x->comma && strlen(x->d) == 4 && atoi(x->d) >= 1900 && atoi(x->d) <= 2099) {
        int known = 0;
        if (z) {
            noun_of_word(e, w, &no);
            known = *no.key != 0 || res(e, "ar_gender", w) != NULL;
        }
        if (!known) {
            add(it, "No:%s", x->d);
            return q;
        }
    }
    /* a counted noun (3 سيارات, 11 طالبا, 2 شنطة, 208 ريال, 1 رسالة) */
    /* after a sign a word with an attached و ب ل ك ف is no counted noun (-1 وشحنة, +1 بوحدة) */
    if (z && !x->dot && !function_word(w) && !has(w, strlen(w), 0, "ال") && !(ends_with(w, "وا") && strlen(w) >= 10) &&
        !(oblique && (has(w, strlen(w), 0, "و") || has(w, strlen(w), 0, "ب") || has(w, strlen(w), 0, "ل") ||
                      has(w, strlen(w), 0, "ك") || has(w, strlen(w), 0, "ف")) && !res(e, "ar_noun", w))) {
        unsigned long long v = strtoull(x->d, NULL, 10);
        noun_of_word(e, w, &no);
        if (*no.key && no.written == 'p') {
            add(it, "N%s%s%s:%s", no.fem ? "f" : "", "o", construct_ok(x->d) ? "c" : "", x->d);
            add(it, "Wp:%s", no.key);
            return q;
        }
        if (*no.key && (no.written == 's' || no.written == 'a')) {
            add(it, "C%s%s%s%s:%s:%s", no.fem ? "f" : "", no.written == 'a' && no.alif && !oblique ? "" : "o",
                construct_ok(x->d) ? "c" : "", no.written == 's' ? "b" : "", x->d, no.key);
            return z;
        }
        int bare_noun = !oblique && !has(w, strlen(w), 0, "و") && !has(w, strlen(w), 0, "ف") &&
                        !has(w, strlen(w), 0, "ب") && !has(w, strlen(w), 0, "ل") && !has(w, strlen(w), 0, "ك");
        if (v == 2 && no.written == '?' && strlen(w) > 4 && bare_noun) {
            /* 2 مروحة مروحتين, 2 معهد معهدين: the dual of a singular noun */
            if (ends_with(w, "ة"))
                add(it, "W:%.*sتين", (int)(strlen(w) - 2), w);
            else
                add(it, "W:%sين", w);
            return z;
        }
        if (v == 1 && no.written == '?' && strlen(w) > 4 && bare_noun) {
            /* 1 رسالة رسالة واحدة */
            add(it, "W:%s", w);
            add(it, "R:%s", no.fem ? "1f" : "1m");
            return z;
        }
        add(it, "N%s%s%s:%s", no.fem ? "f" : "", no.written == 'a' && !oblique ? "" : "o",
            construct_ok(x->d) ? "c" : "", x->d);
        return q;
    }
    add(it, "Do:%s", x->dec);
    return q;
}

/* ---- dates, times, codes ---- */

static void date_items(items_t *it, int d, int m, const char *year, int hijri) {
    add(it, "Om:%d", d);
    add(it, "R:of");
    add(it, "%c:%d", hijri ? 'J' : 'M', m);
    if (year && *year) {
        add(it, "No:%s", year);
        if (hijri)
            add(it, "R:hijri");
    }
}
/* The minutes of a clock time after the hour word. */
static void minute_items(items_t *it, int mm) {
    if (!mm)
        return;
    if (mm == 15)
        add(it, "R:quarter");
    else if (mm == 30)
        add(it, "R:halfhour");
    else if (mm == 1)
        add(it, "&R:minute");
    else if (mm == 2)
        add(it, "&R:minute2");
    else if (mm <= 10) {
        add(it, "&Nf:%d", mm);
        add(it, "R:minutes");
    } else {
        add(it, "&Nfo:%d", mm);
        add(it, "R:minute");
    }
}
/* A clock time h:mm with an optional period (am, pm, a written word kept). */
static void clock_items(items_t *it, int h, int mm, const char *period, int written_period) {
    int h12;
    if (h == 0 && mm == 0 && !period) {
        add(it, "R:midnight");
        return;
    }
    if (mm == 45) {
        h12 = (h + 1) % 12 ? (h + 1) % 12 : 12;
        add(it, "H:%d", h12);
        add(it, "R:to_quarter");
    } else {
        h12 = h % 12 ? h % 12 : 12;
        add(it, "H:%d", h12);
        minute_items(it, mm);
    }
    if (period && !written_period)
        add(it, "R:%s", h12 == 12 && mm != 45 && !strcmp(period, "pm") ? "noon_period" : period);
    else if (period && written_period && h == 12 && !strcmp(period, "pm"))
        ; /* 12 ظهرا stays */
    else if (!period && h >= 12 && h <= 23)
        add(it, "R:%s", h <= 14 ? "noon_period" : "pm");
}
/* The period word after q: its key (am, pm) and end; *written when the word stays as it is
   written (صباحا), not when it is an abbreviation read as a word (ص صباحا, م مساء). */
static size_t period_at(const ttf_engine_t *e, const char *t, size_t n, size_t q, const char **key, int *written) {
    char w[48];
    size_t z = word_after(t, n, q, w, sizeof(w), NULL);
    const char *v;
    if (!z || (v = res(e, "ar_period", w)) == NULL)
        return 0;
    *written = strcmp(w, "ص") && strcmp(w, "م");
    *key = !strcmp(w, "ص") ? "am" : !strcmp(w, "م") ? "pm" : v;
    if (word_is(w, "الظهر|العصر|الصبح|الفجر|بالليل|الليل|المغرب|العشاء")) {
        *written = 0;
        *key = !strcmp(w, "الظهر") ? "noon_period" : !strcmp(w, "العصر") ? "afternoon" : !strcmp(w, "الصبح") ? "am"
               : !strcmp(w, "الفجر") ? "dawn" : "night";
    }
    return z;
}
static size_t clock_at(const char *t, size_t n, size_t p, int *h, int *mm) {
    size_t q = p;
    while (q < n && digit(t[q]) && q - p < 3)
        ++q;
    if (q - p == 0 || q - p > 2 || !(q + 2 < n && t[q] == ':' && digit(t[q + 1]) && digit(t[q + 2])) ||
        (q + 3 < n && digit(t[q + 3])))
        return 0;
    *h = atoi(t + p);
    *mm = atoi(t + q + 1);
    if (*h > 24 || *mm > 59)
        return 0;
    q += 3;
    if (q + 2 < n && t[q] == ':' && digit(t[q + 1]) && digit(t[q + 2]))
        q += 3;
    return q;
}
/* Letters and digits of a code (SV 1023, EK203, SN7F2K9, #SA-48213, JL-0098217, 5G, iOS 19). */
static void code_items(const ttf_engine_t *e, items_t *it, const char *s, size_t len) {
    size_t i = 0, runs = 0, letters_run = 0;
    {
        char whole[24];
        size_t k;
        for (k = 0; k < len && k + 1 < sizeof(whole); ++k)
            whole[k] = (char)(alpha(s[k]) ? s[k] | 0x20 : s[k]);
        whole[k] = 0;
        if (k == len && res(e, "ar_word", whole)) {
            add(it, "W:%s", res(e, "ar_word", whole));
            return;
        }
    }
    for (i = 0; i < len;) {
        size_t z = i;
        if (alpha(s[i])) {
            char low[24], key[24];
            size_t k;
            while (z < len && alpha(s[z]))
                ++z;
            for (k = 0; k < z - i && k + 1 < sizeof(low); ++k)
                low[k] = (char)(s[i + k] | 0x20);
            low[k] = 0;
            snprintf(key, sizeof(key), "%s", low);
            if (z - i > 1 && res(e, "ar_word", key))
                add(it, "W:%s", res(e, "ar_word", key));
            else
                add(it, "L:%.*s", (int)(z - i), s + i);
            letters_run = z - i;
        } else if (digit(s[i])) {
            while (z < len && digit(s[z]))
                ++z;
            if (s[i] != '0' && (z - i <= 2 || (z - i == 3 && letters_run == 1)) && runs <= 2)
                add(it, "No:%.*s", (int)(z - i), s + i);
            else
                add(it, "G:%.*s", (int)(z - i), s + i);
            letters_run = 0;
        } else {
            if (s[i] == '-') {
                size_t d = i + 1;
                while (d < len && digit(s[d]))
                    ++d;
                if (!(d - i - 1 >= 1 && d - i - 1 <= 3 && d == len && i && alpha(s[i - 1])))
                    add(it, "R:dash");
            }
            else if (s[i] == '/')
                add(it, "R:slash");
            else if (s[i] == '_')
                add(it, "W:شرطة سفلية");
            z = i + 1;
        }
        ++runs;
        i = z;
    }
}
/* An address (e-mail, URL, handle): labels as known words, numbers or letter names; marks by name. */
static void address_items(const ttf_engine_t *e, items_t *it, const char *s, size_t len) {
    size_t i = 0;
    if (len >= 8 && (has(s, len, 0, "https://") || has(s, len, 0, "http://"))) {
        i = has(s, len, 0, "https://") ? 8 : 7;
    }
    while (i < len) {
        size_t z = i;
        if (alpha(s[i])) {
            char low[48];
            size_t k;
            const char *w;
            while (z < len && alpha(s[z]))
                ++z;
            for (k = 0; k < z - i && k + 1 < sizeof(low); ++k)
                low[k] = (char)(s[i + k] | 0x20);
            low[k] = 0;
            if ((w = res(e, "ar_word", low)) != NULL)
                add(it, "W:%s", w);
            else if (z - i > 3 && s[i] >= 'A' && s[i] <= 'Z' && s[i + 1] >= 'a' && s[i + 1] <= 'z') {
                /* AbsherKSA أبشر كيه إس إيه: a capitalized word, then capitals */
                size_t c = i + 1, k2;
                char part[48];
                while (c < z && s[c] >= 'a' && s[c] <= 'z')
                    ++c;
                for (k2 = 0; k2 < c - i && k2 + 1 < sizeof(part); ++k2)
                    part[k2] = (char)(s[i + k2] | 0x20);
                part[k2] = 0;
                if (res(e, "ar_word", part))
                    add(it, "W:%s", res(e, "ar_word", part));
                else
                    add(it, "L:%.*s", (int)(c - i), s + i);
                if (c < z)
                    add(it, "L:%.*s", (int)(z - c), s + c);
            } else {
                size_t cut, l = strlen(low);
                const char *w1 = NULL, *w2 = NULL;
                for (cut = l > 3 ? l - 3 : 0; cut >= 3 && !(w1 && w2); --cut) {
                    char a1[48], a2[48];
                    snprintf(a1, sizeof(a1), "%.*s", (int)cut, low);
                    snprintf(a2, sizeof(a2), "%s", low + cut);
                    w1 = res(e, "ar_word", a1);
                    w2 = w1 ? res(e, "ar_word", a2) : NULL;
                }
                if (w1 && w2) {
                    add(it, "W:%s", w1);
                    add(it, "W:%s", w2);
                } else
                    add(it, "L:%.*s", (int)(z - i), s + i);
            }
        } else if (digit(s[i])) {
            while (z < len && digit(s[z]))
                ++z;
            if (s[i] != '0' && z - i <= 3 && i && alpha(s[i - 1]))
                add(it, "No:%.*s", (int)(z - i), s + i);
            else
                add(it, "G:%.*s", (int)(z - i), s + i);
        } else {
            const char *k = s[i] == '.' ? "dot" : s[i] == '@' ? "at" : s[i] == '/' ? "slash" : s[i] == '-' ? "dash"
                          : s[i] == '_' ? "underscore" : NULL;
            char mark[2] = {s[i], 0};
            const char *name;
            if (k)
                add(it, "R:%s", k);
            else if ((name = res(e, "address_symbol", mark)) != NULL)
                add(it, "W:%s", name); /* ?id=7 علامة استفهام … يساوي (an {@url} query, 2026-10-27) */
            z = i + 1;
        }
        i = z;
    }
}

/* ---- classification ---- */

static int prev_word_is(const char *t, size_t p, const char *list) {
    char w[64], c[72];
    if (word_before(t, p, w, sizeof(w)) == (size_t)-1)
        return 0;
    cue_form(w, c, sizeof(c));
    return word_is(w, list) || word_is(c, list);
}
/* A word of the list among the five words before p. */
static int near_before(const char *t, size_t p, const char *list) {
    size_t q = p, k;
    for (k = 0; k < 5; ++k) {
        char w[64], c[72];
        size_t b = word_before(t, q, w, sizeof(w));
        if (b == (size_t)-1) {
            /* step over one non-Arabic character (a digit, a mark) */
            while (q && t[q - 1] == ' ')
                --q;
            if (!q)
                return 0;
            --q;
            continue;
        }
        cue_form(w, c, sizeof(c));
        if (word_is(w, list) || word_is(c, list))
            return 1;
        q = b;
    }
    return 0;
}
/* The end of the Latin run of a code or an address at p: letters, digits and - _ . @ / # :. */
static size_t latin_run(const char *t, size_t n, size_t p) {
    size_t z = p;
    while (z < n && (alpha(t[z]) || digit(t[z]) || t[z] == '-' || t[z] == '_' || t[z] == '@' || t[z] == '/' ||
                     t[z] == '#' || t[z] == ':' || (t[z] == '.' && z + 1 < n && (alpha(t[z + 1]) || digit(t[z + 1])))))
        ++z;
    return z;
}
static int is_address(const char *s, size_t len) {
    size_t i, last_dot = 0;
    int letters_after = 0;
    if (memchr(s, '@', len))
        return 1;
    if (len >= 4 && (has(s, len, 0, "www.") || has(s, len, 0, "http")))
        return 1;
    for (i = 0; i < len; ++i)
        if (s[i] == '.')
            last_dot = i;
    if (!last_dot)
        return 0;
    for (i = last_dot + 1; i < len && alpha(s[i]); ++i)
        ++letters_after;
    return letters_after >= 2 && letters_after <= 6 && alpha(s[0]);
}

static size_t classify_number(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t p, const char *lead) {
    const ttf_engine_t *e = in->engine;
    const char *t = in->text, *cue;
    size_t n = in->length, q, z, sp;
    items_t it;
    num_t x, y;
    int h, mm, hijri = 0, month = 0;
    char w[64];
    memset(&it, 0, sizeof(it));
    if (lead && *lead)
        add(&it, "%s", lead);
    parse_num(t, n, p, &x);
    q = x.end;
    cue = cue_before(e, t, p);

    /* 192.168.10.5: an IP address; 5.2.1 after الإصدار: a version */
    {
        size_t z2 = p;
        int dots = 0;
        while (z2 < n && (digit(t[z2]) || (t[z2] == '.' && z2 + 1 < n && digit(t[z2 + 1])))) {
            dots += t[z2] == '.';
            ++z2;
        }
        if (dots >= 2) {
            size_t a = p;
            int ip = dots == 3;
            while (a < z2) {
                size_t b = a;
                while (b < z2 && digit(t[b]))
                    ++b;
                add(&it, "N:%.*s", (int)(b - a), t + a);
                if (b < z2)
                    add(&it, "R:%s", ip ? "dot" : "point");
                a = b + 1;
            }
            return finish(in, s, z2, ip ? "ip" : "version", &it);
        }
    }
    /* 13/10/2026, 13-10-2026, 2026-10-13, 1448/03/01هـ: a date */
    {
        int a = atoi(t + p), b2, c2;
        size_t z1 = p, z2, z3;
        char sep;
        while (z1 < n && digit(t[z1]))
            ++z1;
        if (z1 < n && (t[z1] == '/' || t[z1] == '-' || t[z1] == '.') && z1 + 1 < n && digit(t[z1 + 1]) && !x.comma) {
            sep = t[z1];
            for (z2 = z1 + 1; z2 < n && digit(t[z2]); ++z2)
                ;
            b2 = atoi(t + z1 + 1);
            if (z2 + 1 < n && t[z2] == sep && digit(t[z2 + 1])) {
                for (z3 = z2 + 1; z3 < n && digit(t[z3]); ++z3)
                    ;
                c2 = atoi(t + z2 + 1);
                if (!(z3 < n && (digit(t[z3]) || t[z3] == sep))) {
                    size_t hm = hijri_mark(t, n, z3);
                    char year[8];
                    if (z1 - p == 4 && z2 - z1 - 1 <= 2 && z3 - z2 - 1 <= 2 && b2 >= 1 && b2 <= 12 && c2 >= 1 && c2 <= 31) {
                        snprintf(year, sizeof(year), "%.4s", t + p);
                        date_items(&it, c2, b2, year, hm != 0);
                        return finish(in, s, hm ? hm : z3, "date", &it);
                    }
                    if (z3 - z2 - 1 == 4 && z1 - p <= 2 && z2 - z1 - 1 <= 2 && a >= 1 && a <= 31 && b2 >= 1 && b2 <= 12) {
                        snprintf(year, sizeof(year), "%.4s", t + z2 + 1);
                        date_items(&it, a, b2, year, hm != 0);
                        return finish(in, s, hm ? hm : z3, "date", &it);
                    }
                }
            } else if (sep == '/' && z2 - z1 - 1 == 4 && z1 - p <= 2 && a >= 1 && a <= 12 && !(z2 < n && digit(t[z2]))) {
                /* 03/2027 مارس ألفين وسبعة وعشرين */
                add(&it, "M:%d", a);
                add(&it, "No:%.4s", t + z1 + 1);
                return finish(in, s, z2, "monthyear", &it);
            } else if (sep == '/' && z1 - p <= 2 && z2 - z1 - 1 <= 2 && a >= 1 && a <= 31 && b2 >= 1 && b2 <= 12 &&
                       !(z2 < n && (digit(t[z2]) || t[z2] == '/')) &&
                       (prev_word_is(t, p, "في|يوم|بتاريخ|تاريخ|حتى|موعد|منذ|قبل|بعد|الموافق|ابتداء|اعتبارا|لغاية|"
                                           "التسليم|التقديم|الاستحقاق|السداد|الدفع|الاجتماع|الموعد|السبت|الأحد|"
                                           "الاثنين|الثلاثاء|الأربعاء|الخميس|الجمعة|بكرة|غدا|اليوم|أمس") ||
                        (a > 12 && !near_before(t, p, "نسبة|بنسبة|قسمة|ناتج|كسر")))) {
                date_items(&it, a, b2, NULL, 0);
                return finish(in, s, z2, "date", &it);
            }
        }
    }
    /* 30/2026/4471: a form number of three slash groups */
    {
        size_t a = p, groups = 0, last = p;
        while (a < n && digit(t[a])) {
            size_t b = a;
            while (b < n && digit(t[b]))
                ++b;
            ++groups;
            last = b;
            if (b + 1 < n && t[b] == '/' && digit(t[b + 1]))
                a = b + 1;
            else
                break;
        }
        if (groups >= 3 && !x.comma && !(last < n && (alpha(t[last]) || t[last] == '/'))) {
            size_t b;
            for (b = p; b < last;) {
                size_t c = b;
                int v;
                while (c < last && digit(t[c]))
                    ++c;
                v = atoi(t + b);
                if (t[b] != '0' && (c - b <= 3 || (c - b == 4 && v >= 1900 && v <= 2099)))
                    add(&it, "No:%.*s", (int)(c - b), t + b);
                else
                    add(&it, "G:%.*s", (int)(c - b), t + b);
                if (c < last)
                    add(&it, "R:slash");
                b = c + 1;
            }
            return finish(in, s, last, "code", &it);
        }
    }
    /* 22 مايو 2026, 5 رمضان 1448هـ, 3 مارس: a day, a month name and maybe a year */
    if (!x.dot && !x.comma && strlen(x.d) <= 2 && atoi(x.d) >= 1 && atoi(x.d) <= 31 &&
        (z = month_at(e, t, n, q, &month, &hijri)) != 0) {
        size_t ye = z, ys = z;
        char year[8] = "";
        if (ys < n && t[ys] == ' ')
            ++ys;
        if (ys < n && digit(t[ys])) {
            num_t yy;
            parse_num(t, n, ys, &yy);
            if (strlen(yy.d) == 4 && !yy.dot && !yy.comma) {
                size_t hm = hijri_mark(t, n, yy.end);
                snprintf(year, sizeof(year), "%s", yy.d);
                ye = hm ? hm : yy.end;
                hijri |= hm != 0;
            }
        }
        {
            /* the month as written (تشرين الأول stays), ذو read ذي after من */
            size_t ms = q;
            while (ms < n && t[ms] == ' ')
                ++ms;
            add(&it, "Om:%d", atoi(x.d));
            add(&it, "R:of");
            if (has(t, n, ms, "ذو "))
                add(&it, "W:ذي %.*s", (int)(z - ms - strlen("ذو ")), t + ms + strlen("ذو "));
            else
                add(&it, "W:%.*s", (int)(z - ms), t + ms);
            if (*year) {
                add(&it, "No:%s", year);
                if (hijri)
                    add(&it, "R:hijri");
            }
        }
        (void)month;
        return finish(in, s, ye, "date", &it);
    }
    /* 1448هـ, 2026م: a year with its calendar */
    if (!x.dot && !x.comma && strlen(x.d) == 4 && (z = hijri_mark(t, n, q)) != 0) {
        add(&it, "No:%s", x.d);
        add(&it, "R:hijri");
        return finish(in, s, z, "year", &it);
    }
    if (!x.dot && !x.comma && strlen(x.d) == 4 && has(t, n, q, "م") && !(q + 2 < n && ar_at(t, n, q + 2))) {
        add(&it, "No:%s", x.d);
        add(&it, "R:gregorian");
        return finish(in, s, q + 2, "year", &it);
    }
    /* 9:30, ٩:٠٠ مساء, 14:07: a clock time; 1:45 after a duration word: hours and minutes;
       2:1 after a ratio word: a ratio (no time after a sign: -2 الصبح is minus two) */
    if (!lead && (z = clock_at(t, n, p, &h, &mm)) != 0) {
        const char *period = NULL;
        int written = 0;
        size_t pe = period_at(e, t, n, z, &period, &written);
        if (near_before(t, p, "نسبة|بنسبة|النسبة|بمعدل|مقياس|بمقياس")) {
            add(&it, "No:%d", h);
            add(&it, "R:to");
            add(&it, "No:%d", mm);
            return finish(in, s, z, "ratio", &it);
        }
        if (near_before(t, p, "تستغرق|يستغرق|استغرق|استغرقت|مدة|المدة|مدته|مدتها|زمن")) {
            if (h == 1)
                add(&it, "R:hour");
            else if (h == 2)
                add(&it, "R:hour2");
            else if (h) {
                add(&it, "Nfo:%d", h);
                add(&it, h <= 10 ? "R:hours" : "R:hour");
            }
            if (mm) {
                add(&it, "%sNfo:%d", h ? "&" : "", mm);
                add(&it, mm <= 10 && mm > 2 ? "R:minutes" : "R:minute");
            }
            return finish(in, s, z, "duration", &it);
        }
        clock_items(&it, h, mm, period, written);
        if (period && written)
            pe = 0;
        /* 9:00-17:00, ١:٠٠ إلى ٣:٠٠: a range of times */
        {
            size_t r = pe ? pe : z, r2 = r;
            int h2, mm2;
            while (r2 < n && t[r2] == ' ')
                ++r2;
            if (r2 < n && t[r2] == '-')
                ++r2;
            else if (has(t, n, r2, "إلى"))
                r2 += strlen("إلى");
            else
                r2 = r;
            while (r2 < n && t[r2] == ' ')
                ++r2;
            if (r2 > r && r2 < n && digit(t[r2]) && (z = clock_at(t, n, r2, &h2, &mm2)) != 0) {
                const char *period2 = NULL;
                int written2 = 0;
                size_t pe2 = period_at(e, t, n, z, &period2, &written2);
                if (!prev_word_is(t, p, "من|بين"))
                    prepend(&it, "R:from");
                add(&it, "R:to");
                clock_items(&it, h2, mm2, period2, written2);
                return finish(in, s, pe2 && !written2 ? pe2 : z, "timerange", &it);
            }
        }
        return finish(in, s, pe ? pe : z, "time", &it);
    }
    /* 11 صباحا, الساعة ٩, ٩ ص - ٩ م: an hour with a period word or after الساعة */
    if (!lead && !x.dot && !x.comma && strlen(x.d) <= 2 && atoi(x.d) <= 24) {
        const char *period = NULL;
        int written = 0;
        size_t pe = period_at(e, t, n, q, &period, &written);
        if (pe || (cue && !strcmp(cue, "hour"))) {
            int hh = atoi(x.d);
            clock_items(&it, hh, 0, period, written);
            {
                size_t r = pe && !written ? pe : q, r2 = r;
                num_t b;
                while (r2 < n && t[r2] == ' ')
                    ++r2;
                if (r2 < n && t[r2] == '-')
                    ++r2;
                else if (has(t, n, r2, "إلى"))
                    r2 += strlen("إلى");
                else
                    r2 = r;
                while (r2 < n && t[r2] == ' ')
                    ++r2;
                if (r2 > r && r2 < n && digit(t[r2])) {
                    const char *period2 = NULL;
                    int written2 = 0;
                    size_t pe2;
                    parse_num(t, n, r2, &b);
                    pe2 = period_at(e, t, n, b.end, &period2, &written2);
                    if (!b.dot && strlen(b.d) <= 2 && (pe2 || pe) && !(b.end < n && t[b.end] == ':')) {
                        add(&it, "R:to");
                        clock_items(&it, atoi(b.d), 0, period2, written2);
                        return finish(in, s, pe2 && !written2 ? pe2 : b.end, "hourrange", &it);
                    }
                }
            }
            return finish(in, s, pe && !written ? pe : q, "hour", &it);
        }
    }
    /* المادة 12 المادة الثانية عشرة: an ordinal after a cue noun */
    {
        char nw2[48];
        noun_t nn;
        size_t ne2 = word_after(t, n, q, nw2, sizeof(nw2), NULL);
        if (cue && ne2) {
            char sk2;
            noun_of_word(e, nw2, &nn);
            if (scale_word(nw2, &sk2) || res(e, "ar_currency", nw2) || (*nn.key && nn.written != '?') ||
                (!strcmp(nw2, "من") && ne2 + 1 < n && digit(t[ne2 + 1])))
                cue = NULL;
        }
    }
    if (cue && (!strcmp(cue, "ordm") || !strcmp(cue, "ordf")) && !x.dot && !x.comma && strlen(x.d) <= 3 &&
        !(q < n && (alpha(t[q]) || t[q] == '/' || t[q] == '-' || t[q] == ':' || t[q] == '%' ||
                    has(t, n, q, "°") || has(t, n, q, " %") || has(t, n, q, "٪")))) {
        add(&it, "O%c:%s", cue[3], x.d);
        return finish(in, s, q, "ordinal", &it);
    }
    /* المخرج 4A أربعة إيه: letters after the number of a place are a code, no unit */
    if (cue && !strncmp(cue, "ord", 3) && !x.dot && q < n && alpha(t[q])) {
        size_t le = latin_run(t, n, p);
        code_items(e, &it, t + p, le - p);
        return finish(in, s, le, "code", &it);
    }
    /* 1 1/4 كوب واحد وربع: a whole number and a fraction */
    if (!x.dot && !x.comma && q + 4 < n && t[q] == ' ' && digit(t[q + 1]) && t[q + 2] == '/' && digit(t[q + 3]) &&
        !(q + 4 < n && digit(t[q + 4]))) {
        static const char *const fw[] = {"", "", "نصف", "ثلث", "ربع"};
        int a2 = t[q + 1] - '0', b3 = t[q + 3] - '0';
        if (a2 == 1 && b3 >= 2 && b3 <= 4) {
            add(&it, "No:%s", x.d);
            add(&it, "&W:%s", fw[b3]);
            return finish(in, s, q + 4, "fraction", &it);
        }
        if (a2 == 3 && b3 == 4) {
            add(&it, "No:%s", x.d);
            add(&it, "&W:ثلاثة أرباع");
            return finish(in, s, q + 4, "fraction", &it);
        }
    }
    /* 2/3 ثلثين, 3/4 ثلاثة أرباع, 5/3 خمسة على ثلاثة: a fraction */
    if (!x.dot && !x.comma && q + 1 < n && t[q] == '/' && digit(t[q + 1])) {
        static const char *const fr[] = {"", "", "نصف", "ثلث", "ربع", "خمس", "سدس", "سبع", "ثمن", "تسع", "عشر"};
        static const char *const fr2[] = {"", "", "نصفين", "ثلثين", "ربعين", "خمسين", "سدسين", "سبعين", "ثمنين", "تسعين", "عشرين"};
        static const char *const frp[] = {"", "", "أنصاف", "أثلاث", "أرباع", "أخماس", "أسداس", "أسباع", "أثمان", "أتساع", "أعشار"};
        unsigned a = (unsigned)atoi(x.d), b;
        parse_num(t, n, q + 1, &y);
        b = (unsigned)atoi(y.d);
        if (!y.dot && !(y.end < n && (digit(t[y.end]) || t[y.end] == '/')) && b) {
            if (b <= 10 && a < b && a >= 1) {
                char nw[64];
                int definite = word_after(t, n, y.end, nw, sizeof(nw), NULL) && has(nw, strlen(nw), 0, "ال");
                if (a == 1)
                    add(&it, "W:%s", fr[b]);
                else if (a == 2)
                    add(&it, b == 3 && definite ? "W:ثلثي" : "W:%s", fr2[b]);
                else {
                    add(&it, "N:%u", a);
                    add(&it, "W:%s", frp[b]);
                }
            } else {
                add(&it, "No:%u", a);
                add(&it, "R:over");
                add(&it, "No:%s", y.d);
            }
            return finish(in, s, y.end, "fraction", &it);
        }
    }
    /* 2:1, 16:9, 1:100: a ratio */
    if (!x.dot && !x.comma && q + 1 < n && t[q] == ':' && digit(t[q + 1])) {
        parse_num(t, n, q + 1, &y);
        add(&it, "No:%s", x.d);
        add(&it, "R:to");
        add(&it, "Do:%s", y.dec);
        return finish(in, s, y.end, "ratio", &it);
    }
    /* phone numbers: 0501234567, 011-234-5678, ٨٠٠ ٢٤٤ ١٢٣٤, 800 1234; codes: 7731-5520 */
    if (!x.dot && !x.comma) {
        size_t a = p, digits = 0, groups = 0, last = p, first_len = strlen(x.d);
        char sepc = 0;
        int dash_code;
        while (a < n && digit(t[a])) {
            size_t b = a;
            while (b < n && digit(t[b]))
                ++b;
            digits += b - a;
            ++groups;
            last = b;
            if (b + 1 < n && (t[b] == ' ' || t[b] == '-') && digit(t[b + 1]) && (!sepc || sepc == t[b])) {
                sepc = t[b];
                a = b + 1;
            } else
                break;
        }
        dash_code = groups == 2 && sepc == '-' && first_len == 4 && last - p == 9;
        /* 2026-2030, 1990-2000: a range of years, no code */
        if (dash_code && atoi(x.d) >= 1900 && atoi(x.d) <= 2099 && atoi(t + p + 5) > atoi(x.d) &&
            atoi(t + p + 5) <= 2099)
            dash_code = 0, groups = 0;
        if (dash_code && atoi(t + p + 5) > atoi(x.d) && x.d[0] != '0' && t[p + 5] != '0') {
            /* 4000-6000 ريال: a range when a currency, a unit or a noun follows */
            char nw3[48], uk3[48];
            noun_t n3;
            size_t ne3 = word_after(t, n, last, nw3, sizeof(nw3), NULL);
            int follows = unit_at(e, t, n, last < n && t[last] == ' ' ? last + 1 : last, uk3, sizeof(uk3)) != 0 ||
                          currency_abbr(e, t, n, last < n && t[last] == ' ' ? last + 1 : last, uk3, sizeof(uk3)) != 0;
            if (ne3 && !follows) {
                noun_of_word(e, nw3, &n3);
                follows = *n3.key || res(e, "ar_currency", nw3) != NULL;
            }
            if (follows)
                dash_code = 0, groups = 0;
        }
        if (groups >= 2 && digits >= 7 && (x.d[0] == '0' || first_len == 3 || dash_code || digits >= 8)) {
            size_t b;
            for (b = p; b < last;) {
                size_t c = b;
                while (c < last && digit(t[c]))
                    ++c;
                add(&it, "G:%.*s", (int)(c - b), t + b);
                if (c < last && dash_code)
                    add(&it, "R:dash");
                b = c + 1;
            }
            return finish(in, s, last, dash_code ? "code" : "phone", &it);
        }
    }
    /* 2-1 after a score word: a score */
    if (!x.dot && !x.comma && q + 1 < n && t[q] == '-' && digit(t[q + 1]) &&
        near_before(t, p, "التعادل|بالتعادل|تعادل|تعادلا|بنتيجة|النتيجة|مباراة|المباراة|فاز|فوز|هزم|خسر|تغلب|"
                          "الديربي|بفوز|بخسارة")) {
        parse_num(t, n, q + 1, &y);
        if (strlen(x.d) <= 3 && strlen(y.d) <= 3 && !y.dot) {
            add(&it, "No:%s", x.d);
            add(&it, "R:versus");
            add(&it, "No:%s", y.d);
            return finish(in, s, y.end, "score", &it);
        }
    }
    /* ranges: 3-5 أيام, ٢٥٠-٤٠٠ ريال, 4-6 ديسمبر, 2 إلى 10 سنوات, بين 10 و20 ملم */
    {
        size_t r = q, r2;
        int dash = 0;
        while (r < n && t[r] == ' ')
            ++r;
        r2 = r;
        if (r2 < n && t[r2] == '-' && r2 == q)
            ++r2, dash = 1;
        else if (has(t, n, r2, "إلى "))
            r2 += strlen("إلى ");
        else if (has(t, n, r2, "و") && r2 + 2 < n && digit(t[r2 + 2]) && near_before(t, p, "بين|تتراوح|يتراوح|ما"))
            r2 += 2;
        else
            r2 = r;
        if (r2 > r && r2 < n && digit(t[r2])) {
            items_t second;
            num_t b;
            size_t end2;
            int m2 = 0, hj2 = 0;
            parse_num(t, n, r2, &b);
            if (dash && !x.dot && !b.dot && strtod(b.dec, NULL) < strtod(x.dec, NULL) && atoi(x.d) <= 12 &&
                atoi(b.d) <= 12 && !(b.end < n && (t[b.end] == '/' || t[b.end] == ':' || digit(t[b.end])))) {
                /* ساعات الدوام 9-5 تسعة إلى خمسة */
                if (!prev_word_is(t, p, "من|بين"))
                    add(&it, "R:from");
                add(&it, "No:%s", x.d);
                add(&it, "R:to");
                add(&it, "No:%s", b.d);
                return finish(in, s, b.end, "range", &it);
            }
            if (!(b.end < n && (t[b.end] == '/' || t[b.end] == ':')) &&
                (!dash || strtod(b.dec, NULL) > strtod(x.dec, NULL))) {
                /* 4-6 ديسمبر الرابع إلى السادس من ديسمبر */
                if (!x.dot && !b.dot && strlen(b.d) <= 2 && (z = month_at(e, t, n, b.end, &m2, &hj2)) != 0) {
                    if (dash && !prev_word_is(t, p, "من|بين"))
                        add(&it, "R:from");
                    add(&it, "Om:%s", x.d);
                    add(&it, "R:to");
                    add(&it, "Om:%s", b.d);
                    add(&it, "R:of");
                    add(&it, "%c:%d", hj2 ? 'J' : 'M', m2);
                    return finish(in, s, z, "daterange", &it);
                }
                memset(&second, 0, sizeof(second));
                end2 = amount(e, t, n, &b, &second, NULL, 1);
                if (!second.bad && second.n) {
                    /* the first number agrees with the noun of the second (من ست إلى سبع سنوات) */
                    char first[96];
                    const char *c = strchr(second.s, ':');
                    char fl[8] = "o";
                    if (dash && !prev_word_is(t, p, "من|بين"))
                        add(&it, "R:from");
                    if ((second.s[0] == 'C' || second.s[0] == 'N') && c && strchr(second.s, 'f') &&
                        strchr(second.s, 'f') < c)
                        snprintf(fl, sizeof(fl), "fo");
                    const char *k = second.s[0] == 'C' && c ? strchr(c + 1, ':') : strstr(second.s, "|Wp:") ? strstr(second.s, "|Wp:") + 3 : NULL;
                    char nkey[48] = "";
                    if (k)
                        snprintf(nkey, sizeof(nkey), "%.*s", (int)strcspn(k + 1, "|"), k + 1);
                    char uk2[48];
                    size_t bs = b.end < n && t[b.end] == ' ' ? b.end + 1 : b.end;
                    int unit_written = unit_at(e, t, n, bs, uk2, sizeof(uk2)) != 0 && !ar_at(t, n, bs);
                    if (!strcmp(x.d, "2") && !x.dot && *nkey && !unit_written) {
                        /* ٢-٣ أيام يومين إلى ثلاثة أيام, ٢-٤ أشخاص شخصين: a noun reads its dual */
                        add(&it, "Kd:%s", nkey);
                    } else if (!strcmp(x.d, "1") && !x.dot && *nkey && !unit_written && atoi(b.d) <= 3) {
                        /* 1-2 كوب كوب إلى كوبين: the noun alone for one */
                        add(&it, "Ks:%s", nkey);
                    } else {
                        snprintf(first, sizeof(first), "%s%s:%s", x.dot ? "D" : "N", fl, x.dot ? x.dec : x.d);
                        add(&it, "%s", first);
                    }
                    if (has(t, n, r, "و") && !dash)
                        add(&it, "&%s", second.s);
                    else {
                        add(&it, "R:to");
                        add(&it, "%s", second.s);
                    }
                    return finish(in, s, end2, "range", &it);
                }
            }
        }
    }
    /* digits one by one: five digits or more, a leading zero, or a number after a phone or code
       word (الرقم الموحد ٩٩٧, رمز التحقق 4821) */
    if (!x.dot && !x.comma) {
        char nw[48];
        size_t ne = word_after(t, n, q, nw, sizeof(nw), NULL), ue;
        char nk[48];
        noun_t no;
        int counted_after = 0;
        ue = unit_at(e, t, n, q < n && t[q] == ' ' ? q + 1 : q, nk, sizeof(nk));
        if (ne) {
            noun_of_word(e, nw, &no);
            counted_after = *no.key || res(e, "ar_currency", nw) || scale_word(nw, nk);
        }
        counted_after |= currency_abbr(e, t, n, q < n && t[q] == ' ' ? q + 1 : q, nk, sizeof(nk)) != 0;
        if (((strlen(x.d) >= 5 && !counted_after && !ue) || (x.d[0] == '0' && strlen(x.d) >= 2) ||
             (strlen(x.d) >= 3 && !counted_after && !ue && (cue_near(e, t, p, "phone") || cue_near(e, t, p, "code")))) &&
            !(q < n && (alpha(t[q]) || t[q] == '%'))) {
            add(&it, "G:%s", x.d);
            return finish(in, s, q, "digits", &it);
        }
    }
    /* 7+ سبعة فما فوق */
    if (!x.dot && !x.comma && q < n && t[q] == '+' && !(q + 1 < n && (digit(t[q + 1]) || (t[q + 1] == ' ' && q + 2 < n && digit(t[q + 2]))))) {
        add(&it, "No:%s", x.d);
        add(&it, "W:فما فوق");
        return finish(in, s, q + 1, "number", &it);
    }
    /* 5G, 2B, 4A: digits before Latin letters are a code */
    if (q < n && alpha(t[q]) && !x.dot && !x.comma) {
        size_t le = latin_run(t, n, p);
        if (!is_address(t + p, le - p) && !unit_at(e, t, n, q, w, sizeof(w))) {
            code_items(e, &it, t + p, le - p);
            return finish(in, s, le, "code", &it);
        }
    }
    sp = amount(e, t, n, &x, &it, NULL, lead != NULL);
    return finish(in, s, sp, "number", &it);
}

/* A maths expression from p: numbers joined by operators with a relation or × ÷ (480 ÷ 6 = 80,
   (0.7 × 92) + (0.3 × 85), 0.03 < 0.05). Returns its end with the items, else 0. */
/* The name of an Arabic letter used as a variable (س سين, ع عين). */
static const char *arabic_letter_name(const char *t, size_t n, size_t p, size_t *end) {
    static const char *const names[] = {"أ", "ألف", "ب", "باء", "ت", "تاء", "ث", "ثاء", "ج", "جيم", "ح", "حاء",
                                        "خ", "خاء", "د", "دال", "ذ", "ذال", "ر", "راء", "ز", "زاي", "س", "سين",
                                        "ش", "شين", "ص", "صاد", "ض", "ضاد", "ط", "طاء", "ظ", "ظاء", "ع", "عين",
                                        "غ", "غين", "ف", "فاء", "ق", "قاف", "ك", "كاف", "ل", "لام", "م", "ميم",
                                        "ن", "نون", "ه", "هاء", "و", "واو", "ي", "ياء", NULL};
    int k;
    for (k = 0; names[k]; k += 2)
        if (has(t, n, p, names[k]) && !(p + 2 < n && ar_at(t, n, p + 2)) && !(p >= 2 && ar_at(t, n, p - 2))) {
            *end = p + 2;
            return names[k + 1];
        }
    return NULL;
}
static size_t maths(const ttf_engine_t *e, const char *t, size_t n, size_t p, items_t *it) {
    static const struct {
        const char *mark, *key;
        int strong;
    } op[] = {{"×", "times", 1}, {"÷", "divided", 1}, {"=", "equals", 1}, {"≤", "le", 1}, {"≥", "ge", 1},
              {"≠", "ne", 1}, {"<", "lt", 1}, {">", "gt", 1}, {"+", "plus", 0}, {"-", "minus", 0},
              {"*", "times", 1}, {"±", "plusminus", 1}, {NULL, NULL, 0}};
    size_t z = p, last = 0;
    int numbers = 0, strong = 0, k;
    items_t m;
    memset(&m, 0, sizeof(m));
    for (;;) {
        num_t x;
        while (z < n && (t[z] == ' ' || t[z] == '(' || t[z] == ')'))
            ++z;
        if (!(z < n && digit(t[z]))) {
            size_t le;
            const char *ln = arabic_letter_name(t, n, z, &le);
            if (ln) {
                add(&m, "W:%s", ln);
                ++numbers;
                z = le;
                goto operand_done;
            }
            if (z < n && alpha(t[z]) && !(z + 1 < n && alpha(t[z + 1]))) {
                add(&m, "L:%c", t[z]);
                ++numbers;
                z += 1;
                goto operand_done;
            }
            break;
        }
        parse_num(t, n, z, &x);
        add(&m, numbers ? "Do:%s" : "D:%s", x.dec);
        ++numbers;
        z = x.end;
    operand_done:
        if (has(t, n, z, "²")) {
            add(&m, "R:squared");
            z += strlen("²");
            strong = 1;
        }
        last = z;
        while (z < n && (t[z] == ' ' || t[z] == ')'))
            ++z;
        for (k = 0; op[k].mark; ++k)
            if (has(t, n, z, op[k].mark))
                break;
        if (!op[k].mark)
            break;
        {
            size_t after = z + strlen(op[k].mark), le2;
            while (after < n && (t[after] == ' ' || t[after] == '('))
                ++after;
            if (!(after < n && (digit(t[after]) || arabic_letter_name(t, n, after, &le2) ||
                                (alpha(t[after]) && !(after + 1 < n && alpha(t[after + 1])))))) {
                /* 5 + 1.8 × المسافة: an operator before a word ends the expression */
                if (op[k].strong && numbers >= 1 && after < n && ar_at(t, n, after)) {
                    add(&m, "R:%s", op[k].key);
                    strong = 1;
                    last = z + strlen(op[k].mark);
                    numbers = numbers < 2 ? 2 : numbers;
                }
                break;
            }
            if (op[k].key[0] == 'p' || op[k].key[0] == 'm')
                if (!(z && t[z - 1] == ' '))
                    break;
            /* 15 + 15: a spaced plus between two numbers is an expression too */
            strong |= op[k].strong || (op[k].key[0] == 'p' && digit(t[after]));
            add(&m, "R:%s", op[k].key);
            z = after;
        }
    }
    if (numbers < 2 || !strong || m.bad)
        return 0;
    /* a unit after the last number (10 × 10 × 10 سم عشرة في عشرة في عشرة سنتيمتر) */
    {
        char uk[48];
        size_t us = last < n && t[last] == ' ' ? last + 1 : last, ue = unit_at(e, t, n, us, uk, sizeof(uk));
        if (ue) {
            add(&m, "Ks:%s", uk);
            last = ue;
        }
    }
    memcpy(it, &m, sizeof(m));
    return last;
}

static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const ttf_engine_t *e = in->engine;
    const char *t = in->text;
    size_t n = in->length, p, z;
    items_t it;
    num_t x;
    if (in->index >= in->token_count)
        return 0;
    p = in->tokens[in->index].begin;
    if (p >= n)
        return 0;
    memset(&it, 0, sizeof(it));

    /* ≤ 30%, ≥ 35, < 0.05: a relation before a number */
    {
        static const char *const rel[] = {"≤", "le", "≥", "ge", "≠", "ne", "<", "lt_lead", ">", "gt_lead", NULL};
        int k;
        for (k = 0; rel[k]; k += 2)
            if (has(t, n, p, rel[k])) {
                size_t a = p + strlen(rel[k]);
                while (a < n && t[a] == ' ')
                    ++a;
                if (a < n && digit(t[a])) {
                    add(&it, "R:%s", rel[k + 1]);
                    parse_num(t, n, a, &x);
                    if (!x.dot && x.end + 1 < n && t[x.end] == '/' && digit(t[x.end + 1])) {
                        num_t d2;
                        parse_num(t, n, x.end + 1, &d2);
                        add(&it, "No:%s", x.d);
                        add(&it, "R:over");
                        add(&it, "No:%s", d2.d);
                        return finish(in, s, d2.end, "relation", &it);
                    }
                    z = amount(e, t, n, &x, &it, NULL, 0);
                    return finish(in, s, z, "relation", &it);
                }
            }
    }
    /* $1,299, €45 مليون, £2,450: a currency sign before the amount */
    {
        static const char *const sign[] = {"$", "€", "£", "¥", NULL};
        int k;
        for (k = 0; sign[k]; ++k)
            if (has(t, n, p, sign[k]) && p + strlen(sign[k]) < n && digit(t[p + strlen(sign[k])])) {
                const char *cv = res(e, "ar_currency", sign[k]);
                if (!cv)
                    break;
                parse_num(t, n, p + strlen(sign[k]), &x);
                z = amount(e, t, n, &x, &it, cv, 0);
                return finish(in, s, z, "money", &it);
            }
    }
    /* +٥٢, -3°م, ±0.3, +966 11 234 5678: a sign before a number, or a phone number */
    if ((t[p] == '+' || t[p] == '-' || has(t, n, p, "±")) &&
        !(p && (digit(t[p - 1]) || alpha(t[p - 1]) ||
                (p >= 2 && ar_at(t, n, p - 2) && !mark_at(t, n, p - 2) &&
                 !(has(t, n, p - 2, "و") && (p == 2 || t[p - 3] == ' ')))))) {
        size_t a = p + (t[p] == '+' || t[p] == '-' ? 1 : strlen("±"));
        if (a < n && digit(t[a])) {
            size_t b = a, digits = 0;
            /* + and digit groups of eight digits or more: an international phone number */
            if (t[p] == '+') {
                size_t last = a;
                while (b < n && digit(t[b])) {
                    size_t c = b;
                    while (c < n && digit(t[c]))
                        ++c;
                    digits += c - b;
                    last = c;
                    if (c + 1 < n && (t[c] == ' ' || t[c] == '-') && digit(t[c + 1]))
                        b = c + 1;
                    else
                        break;
                }
                if (digits >= 8) {
                    add(&it, "R:plus");
                    for (b = a; b < last;) {
                        size_t c = b;
                        while (c < last && digit(t[c]))
                            ++c;
                        add(&it, "G:%.*s", (int)(c - b), t + b);
                        b = c + 1;
                    }
                    return finish(in, s, last, "phone", &it);
                }
            }
            if (t[p] == '+') {
                size_t c = a;
                while (c < n && digit(t[c]))
                    ++c;
                if (c - a <= 4 && !(c < n && (t[c] == '.' || t[c] == '%' || t[c] == ',')) &&
                    !(c + 1 < n && t[c] == ' ' && digit(t[c + 1])) &&
                    near_before(t, p, "رقم|مكالمة|مكالمات|اتصال|مفتاح|الدولة|الدول|ب")) {
                    add(&it, "R:plus");
                    add(&it, "G:%.*s", (int)(c - a), t + a);
                    return finish(in, s, c, "phone", &it);
                }
            }
            add(&it, "R:%s", t[p] == '+' ? "plus" : t[p] == '-' ? "minus" : "plusminus");
            {
                char lead[32];
                snprintf(lead, sizeof(lead), "%s", it.s);
                return classify_number(in, s, a, lead);
            }
        }
    }
    if (t[p] == '-' && p >= 2 && ar_at(t, n, p - 2) && !mark_at(t, n, p - 2) && p + 1 < n && digit(t[p + 1]))
        return classify_number(in, s, p + 1, "W: ");
    if ((has(t, n, p, "٪") || t[p] == '%') && p + (t[p] == '%' ? 1 : 2) < n && digit(t[p + (t[p] == '%' ? 1 : 2)]) &&
        !(p && digit(t[p - 1]))) {
        size_t a = p + (t[p] == '%' ? 1 : 2);
        parse_num(t, n, a, &x);
        if (!(x.end < n && (digit(t[x.end]) || alpha(t[x.end])))) {
            add(&it, "Do:%s", x.dec);
            add(&it, "R:percent");
            return finish(in, s, x.end, "percent", &it);
        }
    }
    /* #رمضان_٢٠٢٦: a hashtag of Arabic words */
    if (t[p] == '#' && p + 2 < n && ar_at(t, n, p + 1)) {
        size_t z2 = p + 1, a2;
        add(&it, "W:هاشتاق");
        while (z2 < n) {
            a2 = z2;
            if (ar_at(t, n, z2)) {
                while (z2 < n && ar_at(t, n, z2))
                    z2 += 2;
                add(&it, "W:%.*s", (int)(z2 - a2), t + a2);
            } else if (t[z2] == '_') {
                add(&it, "W:شرطة سفلية");
                ++z2;
            } else if (digit(t[z2])) {
                while (z2 < n && digit(t[z2]))
                    ++z2;
                add(&it, "No:%.*s", (int)(z2 - a2), t + a2);
            } else
                break;
        }
        return finish(in, s, z2, "hashtag", &it);
    }
    /* تقرير_2026.pdf: an Arabic file name, its parts joined by _ - and digits, then .ext */
    if (ar_at(t, n, p)) {
        size_t y = p, a2, x2;
        int joined = 0;
        while (y < n && (ar_at(t, n, y) || t[y] == '_' || t[y] == '-' || digit(t[y]))) {
            joined |= t[y] == '_' || t[y] == '-' || digit(t[y]);
            y += ar_at(t, n, y) ? 2 : 1;
        }
        for (x2 = y + 1; x2 < n && alpha(t[x2]); ++x2)
            ;
        if (joined && y < n && t[y] == '.' && x2 - y - 1 >= 2 && x2 - y - 1 <= 4 &&
            !(x2 < n && (digit(t[x2]) || t[x2] == '_'))) {
            size_t z2 = p;
            while (z2 < y) {
                a2 = z2;
                if (ar_at(t, n, z2)) {
                    while (z2 < y && ar_at(t, n, z2))
                        z2 += 2;
                    add(&it, "W:%.*s", (int)(z2 - a2), t + a2);
                } else if (digit(t[z2])) {
                    while (z2 < y && digit(t[z2]))
                        ++z2;
                    add(&it, "No:%.*s", (int)(z2 - a2), t + a2);
                } else {
                    add(&it, t[z2] == '_' ? "W:شرطة سفلية" : "R:dash");
                    ++z2;
                }
            }
            add(&it, "R:dot");
            address_items(e, &it, t + y + 1, x2 - y - 1);
            return finish(in, s, x2, "address", &it);
        }
    }
    /* .sa, .com: a top-level domain written alone */
    if (t[p] == '.' && (!p || t[p - 1] == ' ' || (p >= 2 && has(t, n, p - 2, "و") && (p == 2 || t[p - 3] == ' '))) &&
        p + 2 < n && alpha(t[p + 1])) {
        size_t le = p + 1;
        while (le < n && alpha(t[le]))
            ++le;
        if (le - p - 1 >= 2 && le - p - 1 <= 6 && !(le < n && (digit(t[le]) || t[le] == '_' || t[le] == '@' ||
                                                             (t[le] == '.' && le + 1 < n && alpha(t[le + 1]))))) {
            add(&it, "R:dot");
            address_items(e, &it, t + p + 1, le - p - 1);
            return finish(in, s, le, "address", &it);
        }
    }
    {
        size_t le;
        if (ar_at(t, n, p) && arabic_letter_name(t, n, p, &le) && (z = maths(e, t, n, p, &it)) != 0)
            return finish(in, s, z, "maths", &it);
    }
    /* الـ3 الثالث, النسخة الـ22 الثانية والعشرين, الساعة الـ8 مساء, الـ5 والنص الخامسة والنصف */
    if (ar_at(t, n, p)) {
        size_t te = in->tokens[in->index].end;
        if (te - p >= 6 && has(t, n, te - 6, "الـ") && te < n && digit(t[te])) {
            size_t q;
            char w[64];
            int hour = 0, fem = 0;
            const char *cue;
            parse_num(t, n, te, &x);
            q = x.end;
            if (x.dot || x.comma || strlen(x.d) > 2 || (q < n && (alpha(t[q]) || t[q] == ':')))
                return 0;
            if (te - p > 6)
                add(&it, "W:%.*s", (int)(te - 6 - p), t + p);
            cue = cue_before(e, t, p);
            {
                const char *period = NULL;
                int written = 0;
                size_t pe = period_at(e, t, n, q, &period, &written);
                char nw[48];
                size_t ne = word_after(t, n, q, nw, sizeof(nw), NULL);
                if (pe || (cue && !strcmp(cue, "hour")) || prev_word_is(t, p, "أخبار|نشرة|الساعة") ||
                    (ne && word_is(nw, "والنص|والنصف|والربع|إلا")))
                    hour = 1;
                else if (prev_word_is(t, p, "من|إلى|حتى|لين|للساعة|بعد|قبل") &&
                         (near_before(t, p, "الساعة|الاجتماع|موعد") ||
                          (word_after(t, n, q, w, sizeof(w), NULL) && word_is(w, "إلى|حتى|العصر|الظهر|المغرب|الصبح"))))
                    hour = 1;
                if (hour) {
                    int hh = atoi(x.d);
                    add(&it, "H:%d", hh % 12 ? hh % 12 : 12);
                    if (ne && word_is(nw, "والنص|والنصف")) {
                        add(&it, "R:halfhour");
                        q = ne;
                    } else if (ne && !strcmp(nw, "والربع")) {
                        add(&it, "R:quarter");
                        q = ne;
                    }
                    return finish(in, s, q, "hour", &it);
                }
            }
            /* the gender of the noun before الـ */
            if (word_before(t, p, w, sizeof(w)) != (size_t)-1) {
                char c[72];
                cue_form(w, c, sizeof(c));
                fem = ends_with(w, "ة") || ends_with(w, "ى") || (res(e, "ar_cue", c) && !strcmp(res(e, "ar_cue", c), "ordf"));
            }
            add(&it, "O%c:%s", fem ? 'f' : 'm', x.d);
            return finish(in, s, q, "ordinal", &it);
        }
        return 0;
    }
    /* SV 1023, iOS 19, EK203, #SA-48213, invoice@store.com, www.moe.gov.sa, @salem_photo,
       v3.4.0: Latin letters with digits, or an address */
    if (alpha(t[p]) || ((t[p] == '#' || t[p] == '@') && p + 1 < n && alpha(t[p + 1]))) {
        size_t a = t[p] == '#' ? p + 1 : p, le = latin_run(t, n, a);
        int digits = 0;
        size_t i;
        while (le > a && (t[le - 1] == '.' || t[le - 1] == '-' || t[le - 1] == ':' || t[le - 1] == '/'))
            --le;
        for (i = a; i < le; ++i)
            digits |= digit(t[i]);
        if (t[p] == '@' || is_address(t + a, le - a)) {
            if (t[p] == '@')
                add(&it, "R:at");
            address_items(e, &it, t + (t[p] == '@' ? p + 1 : a), le - (t[p] == '@' ? p + 1 : a));
            return finish(in, s, le, "address", &it);
        }
        /* v3.4.0 الإصدار ثلاثة فاصلة أربعة فاصلة صفر */
        if ((t[a] == 'v' || t[a] == 'V') && a + 1 < le && digit(t[a + 1]) && memchr(t + a, '.', le - a)) {
            size_t b = a + 1;
            if (!prev_word_is(t, p, "الإصدار"))
                add(&it, "R:version");
            while (b < le) {
                size_t c = b;
                while (c < le && digit(t[c]))
                    ++c;
                add(&it, "N:%.*s", (int)(c - b), t + b);
                if (c < le)
                    add(&it, "R:point");
                b = c + 1;
            }
            return finish(in, s, le, "version", &it);
        }
        /* SAR 1,750: a currency code before the amount */
        if (le - a == 3 && le + 1 < n && t[le] == ' ' && digit(t[le + 1])) {
            char ck[8];
            const char *cv;
            snprintf(ck, sizeof(ck), "%.3s", t + a);
            if ((cv = res(e, "ar_currency", ck)) != NULL) {
                parse_num(t, n, le + 1, &x);
                z = amount(e, t, n, &x, &it, cv, 0);
                return finish(in, s, z, "money", &it);
            }
        }
        /* R = 220 Ω آر يساوي مئتين وعشرين أوم: a letter equal to a number */
        if (le - a == 1 && has(t, n, le, " = ") && le + 3 < n && digit(t[le + 3])) {
            add(&it, "L:%c", t[a]);
            add(&it, "R:equals");
            parse_num(t, n, le + 3, &x);
            if (x.end + 1 < n && t[x.end] == ' ' && has(t, n, x.end + 1, "Ω")) {
                add(&it, "No:%s", x.d);
                add(&it, "Ks:أوم");
                return finish(in, s, x.end + 1 + strlen("Ω"), "maths", &it);
            }
            z = amount(e, t, n, &x, &it, NULL, 1);
            return finish(in, s, z, "maths", &it);
        }
        /* SV 1023, iOS 19, MRN 00928417: letters, a space and digits */
        if (!digits && le < n && t[le] == ' ' && le + 1 < n && digit(t[le + 1]) && le - a <= 12) {
            size_t b = le + 1, c;
            char low[16];
            size_t k;
            int caps = 1;
            for (k = a; k < le; ++k)
                caps &= t[k] >= 'A' && t[k] <= 'Z';
            for (k = 0; k < le - a && k + 1 < sizeof(low); ++k)
                low[k] = (char)(alpha(t[a + k]) ? t[a + k] | 0x20 : t[a + k]);
            low[k] = 0;
            c = b;
            while (c < n && digit(t[c]))
                ++c;
            if ((caps || res(e, "ar_word", low)) && !(c < n && (alpha(t[c]) || t[c] == ',' || t[c] == ':')) &&
                !(c + 1 < n && t[c] == '.' && digit(t[c + 1]) && c - b > 3)) {
                char code[48];
                num_t dn;
                snprintf(code, sizeof(code), "%.*s", (int)(le - a), t + a);
                code_items(e, &it, code, strlen(code));
                parse_num(t, n, b, &dn);
                if (dn.dot)
                    add(&it, "Do:%s", dn.dec);
                else if (t[b] != '0' && c - b <= 4)
                    add(&it, "No:%.*s", (int)(c - b), t + b);
                else
                    add(&it, "G:%.*s", (int)(c - b), t + b);
                return finish(in, s, dn.dot ? dn.end : c, "code", &it);
            }
        }
        if (digits) {
            code_items(e, &it, t + a, le - a);
            return finish(in, s, le, "code", &it);
        }
        return 0;
    }
    if (!digit(t[p]) || (p && (digit(t[p - 1]) || alpha(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',')))
        return 0;
    if ((z = maths(e, t, n, p, &it)) != 0)
        return finish(in, s, z, "maths", &it);
    return classify_number(in, s, p, NULL);
}

/* ---- verbalization ---- */

/* A shared expression: numbers in the oblique case, operators by name, Latin letters by their
   names, brackets silent. */
static ttf_status_t expression(out_t *o, const char *v) {
    static const char *const op[] = {"+", "plus", "-", "minus", "*", "times", "×", "times", "/", "divided",
                                     "÷", "divided", "=", "equals", "<", "lt", ">", "gt", "≤", "le", "≥", "ge",
                                     "≠", "ne", "±", "plusminus", "²", "squared", NULL};
    size_t i = 0, n = strlen(v);
    while (i < n) {
        int k;
        if (v[i] == ' ' || v[i] == '(' || v[i] == ')') {
            ++i;
            continue;
        }
        if (digit(v[i])) {
            char d[48];
            size_t z = i, l;
            while (z < n && (digit(v[z]) || (v[z] == '.' && z + 1 < n && digit(v[z + 1]))))
                ++z;
            l = z - i;
            if (l >= sizeof(d))
                return TTF_EINVAL;
            memcpy(d, v + i, l);
            d[l] = 0;
            TRY(decimal(o, d, "o"));
            i = z;
            continue;
        }
        if (alpha(v[i])) {
            char key[2] = {(char)(v[i] | 0x20), 0};
            TRY(emit(o, res(o->e, "ar_letter", key)));
            ++i;
            continue;
        }
        for (k = 0; op[k]; k += 2)
            if (!strncmp(v + i, op[k], strlen(op[k])))
                break;
        if (!op[k])
            return TTF_EINVAL;
        TRY(num(o, op[k + 1]));
        i += strlen(op[k]);
    }
    return TTF_OK;
}


static ttf_status_t say(out_t *o, const char *v) {
    const char *it = v;
    while (*it) {
        const char *bar = strchr(it, '|');
        size_t l = bar ? (size_t)(bar - it) : strlen(it);
        char item[2 * TTF_IR_VALUE_SIZE], flags[8] = "";
        const char *body;
        size_t f = 0;
        if (l >= sizeof(item))
            return TTF_EINVAL;
        memcpy(item, it, l);
        item[l] = 0;
        body = item;
        if (*body == '&') {
            o->and_next = 1;
            ++body;
        }
        while (body[1 + f] && body[1 + f] != ':' && f + 1 < sizeof(flags)) {
            flags[f] = body[1 + f];
            ++f;
        }
        flags[f] = 0;
        if (body[1 + f] != ':')
            return TTF_EINVAL;
        {
            const char *arg = body + 2 + f;
            switch (body[0]) {
            case 'N':
                TRY(cardinal(o, arg, flags));
                break;
            case 'D':
                TRY(decimal(o, arg, flags));
                break;
            case 'C': {
                const char *colon = strchr(arg, ':');
                char digits[32];
                if (!colon || (size_t)(colon - arg) >= sizeof(digits))
                    return TTF_EINVAL;
                snprintf(digits, sizeof(digits), "%.*s", (int)(colon - arg), arg);
                TRY(counted(o, digits, colon + 1, flags));
                break;
            }
            case 'K': {
                char text[64];
                if (!noun_text(o->e, arg, flags[0], text, sizeof(text)))
                    return TTF_ERESOURCE;
                TRY(emit(o, text));
                break;
            }
            case 'O':
                TRY(ordinal(o, (unsigned)atoi(arg), flags[0] == 'f'));
                break;
            case 'H':
                TRY(hour_word(o, (unsigned)atoi(arg)));
                break;
            case 'G':
                TRY(digits_each(o, arg, strlen(arg)));
                break;
            case 'L': {
                size_t i;
                for (i = 0; arg[i]; ++i) {
                    char key[2] = {(char)(arg[i] | 0x20), 0};
                    TRY(emit(o, res(o->e, "ar_letter", key)));
                }
                break;
            }
            case 'R':
                TRY(num(o, arg));
                break;
            case 'W':
                if (!flags[0])
                    TRY(emit(o, arg));
                break;
            case 'M':
            case 'J':
                TRY(emit(o, res(o->e, body[0] == 'M' ? "ar_month" : "ar_hijri", arg)));
                break;
            default:
                return TTF_EINVAL;
            }
        }
        if (!bar)
            break;
        it = bar + 1;
    }
    return TTF_OK;
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    out_t o = {e, b, 0};
    char all[2 * TTF_IR_VALUE_SIZE + 2];
    if (s->type == TTF_SEM_CUSTOM && !strcmp(s->rule, "control.pattern") && strcmp(s->unit, "@alphabet")) {
        if (!strcmp(s->unit, "@email") || !strcmp(s->unit, "@url")) {
            /* [[a1@b2.com]]{@email} إيه واحد آت بي اثنين دوت كوم: the shared check of the
               address shape, then the Arabic reading of an address in plain text (2026-10-27;
               was invalid resource, the shared reader found no Arabic mark names) */
            items_t it = {{0}, 0, 0};
            size_t keep = b->length;
            ttf_status_t st = ttf_electronic_control(e, s, surface, length, b);
            b->length = keep;
            if (b->data)
                b->data[keep] = 0;
            if (st != TTF_OK)
                return st;
            address_items(e, &it, surface, length);
            if (it.bad || !it.n)
                return TTF_EINVAL;
            return say(&o, it.s);
        }
        if (!strcmp(s->unit, "@native") || !strcmp(s->unit, "@sino") || !strcmp(s->unit, "@counter"))
            return TTF_EINVAL;
        return ttf_semantic_control(e, s, surface, length, b);
    }
    if (strcmp(s->unit, "@ar-say")) {
        /* shared objects in an Arabic sentence */
        if (s->type == TTF_SEM_PHONE || s->type == TTF_SEM_DIGIT_SEQUENCE)
            return digits_each(&o, surface, length);
        if (s->type == TTF_SEM_SYMBOL && !strcmp(s->unit, "@expression") && *s->value) {
            size_t keep = b->length;
            ttf_status_t st = expression(&o, s->value);
            if (st == TTF_OK)
                return st;
            b->length = keep;
        }
        if ((s->type == TTF_SEM_CARDINAL || s->type == TTF_SEM_NUMBER || s->type == TTF_SEM_PERCENT) && *s->value) {
            const char *v = s->value;
            char d[48];
            int sign = *v == '-' ? -1 : *v == '+' ? 1 : 0;
            v += sign != 0;
            if (*v && strlen(v) < sizeof(d) && strspn(v, "0123456789.") == strlen(v) && strchr(v, '.') == strrchr(v, '.')) {
                snprintf(d, sizeof(d), "%s", v);
                if (sign)
                    TRY(num(&o, sign < 0 ? "minus" : "plus"));
                TRY(decimal(&o, d, "o"));
                return s->type == TTF_SEM_PERCENT ? num(&o, "percent") : TTF_OK;
            }
        }
        return ttf_buffer_append(b, surface, length);
    }
    snprintf(all, sizeof(all), "%s%s", s->value, s->secondary);
    return say(&o, all);
}

/* Arabic letters are no script of the core's (OTHER); this pack takes an Arabic sentence
   (arabic_sentence) or a declared request. Latin letters score 1 so that a Latin word in an
   Arabic sentence stays as written unless a code reads it. */
static int script_score(ttf_script_t script) {
    return script == TTF_SCRIPT_LATIN ? 1 : 0;
}
static int context_score(const ttf_pack_input_t *in) {
    const ttf_engine_t *e = in->engine;
    const ttf_language_pack_t *sentence;
    size_t k;
    if (!e || in->index >= in->token_count || in->tokens[in->index].script == TTF_SCRIPT_HANGUL ||
        in->tokens[in->index].script == TTF_SCRIPT_KANA || in->tokens[in->index].script == TTF_SCRIPT_HAN)
        return 0;
    for (k = 0; k + 2 <= in->length; ++k)
        if (ar_at(in->text, in->length, k) && !mark_at(in->text, in->length, k))
            return 1000;
    if (!e->default_language || strcmp(e->default_language, AR))
        return 0;
    sentence = ttf_sentence_language(in);
    if (sentence && strcmp(sentence->language_tag, AR))
        return 0;
    return 1000;
}

const ttf_language_pack_t ttf_pack_ar = {sizeof(ttf_language_pack_t),
                                         TTF_ABI_VERSION,
                                         AR,
                                         NULL,
                                         script_score,
                                         context_score,
                                         classify,
                                         verbalize,
                                         NULL,
                                         NULL,
                                         NULL};
