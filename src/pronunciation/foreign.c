#include "../core/internal.h"
#include "trace.h"

/* ABI v1 keeps provenance in the existing fixed-size field. The reserved
   envelope reports SOURCE phone resources, never the target phone mapper. */
static ttf_status_t pronunciation_provenance(const ttf_pronunciation_t *pron, char trace[64],
                                             ttf_resource_layer_t *layer) {
    static const char reserved[] = "ttf-pron-";
    static const char version[] = "ttf-pron-v1:";
    const char *path = pron->provenance;
    const char *separator;
    size_t i, length;
    int found = 0;
    *layer = TTF_LAYER_COMMON;
    if (!memchr(pron->provenance, 0, sizeof(pron->provenance)) ||
        !memchr(pron->phones, 0, sizeof(pron->phones)) ||
        !memchr(pron->alphabet, 0, sizeof(pron->alphabet)))
        return TTF_ERESOURCE;
    if (!strncmp(path, reserved, sizeof(reserved) - 1)) {
        if (strncmp(path, version, sizeof(version) - 1))
            return TTF_ERESOURCE;
        path += sizeof(version) - 1;
        separator = strchr(path, ':');
        if (!separator)
            return TTF_ERESOURCE;
        length = (size_t)(separator - path);
        for (i = TTF_LAYER_COMMON; i <= TTF_LAYER_USER; ++i) {
            const char *name = ttf_layer_name((ttf_resource_layer_t)i);
            if (strlen(name) == length && !memcmp(name, path, length)) {
                *layer = (ttf_resource_layer_t)i;
                found = 1;
                break;
            }
        }
        if (!found)
            return TTF_ERESOURCE;
        path = separator + 1;
        if (!*path)
            return TTF_ERESOURCE;
        /* Paths are stable identifiers, not another metadata serialization. */
        for (i = 0; path[i]; ++i)
            if (!((path[i] >= 'a' && path[i] <= 'z') || (path[i] >= '0' && path[i] <= '9') ||
                  (i && (path[i] == '-' || path[i] == '_' || path[i] == '.'))))
                return TTF_ERESOURCE;
    }
    (void)snprintf(trace, 64, "%s", path);
    return TTF_OK;
}
static ttf_status_t lookup_kind(const ttf_engine_t *e, const char *kind, const char *source,
                                const char *target, const char *text, size_t n, const char **hit,
                                ttf_resource_layer_t *layer);
ttf_status_t ttf_foreign_lookup(const ttf_engine_t *e, const char *source, const char *target,
                                const char *text, size_t n, const char **hit,
                                ttf_resource_layer_t *layer) {
    return lookup_kind(e, "lexicon", source, target, text, n, hit, layer);
}
/* A dictionary loanword spelling (loanword, 2026-10-05): the guess for a word with
   no declared reading (unknown_foreign=guess only), never a declared word itself,
   so letter-digit labels (ham7239), addresses, handles and phrase decisions keep
   following the project lexicon only. */
ttf_status_t ttf_foreign_loanword(const ttf_engine_t *e, const char *source, const char *target,
                                  const char *text, size_t n, const char **hit,
                                  ttf_resource_layer_t *layer) {
    return lookup_kind(e, "loanword", source, target, text, n, hit, layer);
}
static ttf_status_t lookup_kind(const ttf_engine_t *e, const char *kind, const char *source,
                                const char *target, const char *text, size_t n, const char **hit,
                                ttf_resource_layer_t *layer) {
    char *key;
    size_t a, i;
    if (!e || !source || !target || !hit || !layer || (!text && n))
        return TTF_EINVAL;
    *hit = NULL;
    a = strlen(source);
    if (n > SIZE_MAX - a - 2)
        return TTF_ELIMIT;
    key = (char *)malloc(a + n + 2);
    if (!key)
        return TTF_ENOMEM;
    memcpy(key, source, a);
    key[a] = ':';
    if (n)
        memcpy(key + a + 1, text, n);
    key[a + n + 1] = 0;
    for (i = a + 1; i < a + n + 1; i++)
        if (key[i] >= 'A' && key[i] <= 'Z')
            key[i] += 'a' - 'A';
    *hit = ttf_resource_get(e, kind, target, key, layer);
    /* lexicon_case: lower entries (kg, cm) do not apply to an all-capital
       surface (KG이니시스, KB국민은행 are letters); upper entries (5G, 4G) apply
       only to a surface without lowercase letters (생강 2g is a measure). The
       rule restricts entries of its own layer or below: a domain, customer or
       user entry above it (MY 마이, ETF 이티에프) keeps applying (2026-10-03). */
    if (*hit && n >= 2) {
        ttf_resource_layer_t rule_layer = TTF_LAYER_COMMON;
        const char *rule = ttf_resource_get(e, "lexicon_case", target, key, &rule_layer);
        if (rule && *layer > rule_layer)
            rule = NULL;
        int capitals = 1, lowercase = 0;
        for (i = 0; i < n; i++) {
            if (text[i] >= 'a' && text[i] <= 'z')
                lowercase = 1;
            if (text[i] < 'A' || text[i] > 'Z')
                capitals = 0;
        }
        if (rule && ((!strcmp(rule, "lower") && capitals) ||
                     (!strcmp(rule, "upper") && lowercase)))
            *hit = NULL;
        /* A loanword is always lower-case only: PET, CAT are initialisms. */
        if (capitals && !strcmp(kind, "loanword"))
            *hit = NULL;
    }
    free(key);
    return TTF_OK;
}
ttf_status_t ttf_trace_foreign_lexicon(const ttf_engine_t *e, const char *source,
                                       const char *target, const char *text, size_t n,
                                       const char *detail) {
    ttf_status_t status;
    char *word, *key, *canonical = NULL;
    size_t prefix = strlen(source), i;
    if (!e->trace_state)
        return TTF_OK;
    if (n > SIZE_MAX - prefix - 2)
        return TTF_ELIMIT;
    word = ttf_strndup(text, n);
    key = malloc(prefix + n + 2);
    if (!word || !key) {
        free(word);
        free(key);
        return TTF_ENOMEM;
    }
    memcpy(key, source, prefix);
    key[prefix] = ':';
    memcpy(key + prefix + 1, word, n + 1);
    for (i = prefix + 1; i < prefix + n + 1; ++i)
        if (key[i] >= 'A' && key[i] <= 'Z')
            key[i] += 'a' - 'A';
    status = ttf_lexicon_canonical_key(key, &canonical);
    if (status == TTF_OK)
        status = ttf_trace_resource(e, "lexicon", target, canonical, word, detail, SIZE_MAX);
    free(canonical);
    free(key);
    free(word);
    return status;
}
static ttf_status_t render_at(const ttf_engine_t *, const ttf_language_pack_t *,
                              const ttf_language_pack_t *, const char *, size_t,
                              ttf_lexical_class_t, ttf_buffer_t *, char[64], ttf_resource_layer_t *,
                              size_t);

/* The source language's own numeral, rendered into the target language, so a
   word label reads SAVE20 as 세이브 트웬티 rather than 세이브 이십. */
static ttf_status_t source_numeral(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                   const ttf_language_pack_t *target, const ttf_semantic_t *sem,
                                   const char *text, size_t n, ttf_buffer_t *out,
                                   ttf_resource_layer_t *layer, size_t part_begin) {
    ttf_buffer_t spoken = {0};
    char part_trace[64] = {0};
    ttf_resource_layer_t part_layer = TTF_LAYER_COMMON;
    ttf_status_t st = source->verbalize(e, sem, text, n, &spoken);
    if (st == TTF_OK && spoken.length)
        st = render_at(e, source, target, spoken.data, spoken.length, TTF_LEX_WORD, out, part_trace,
                       &part_layer, part_begin);
    else if (st == TTF_OK)
        st = target->verbalize(e, sem, text, n, out);
    ttf_buffer_free(&spoken);
    if (st == TTF_OK && part_layer > *layer)
        *layer = part_layer;
    return st;
}

/* A declared word reading: the target lexicon knows the spelling, so an
   uppercase run such as SAVE is a word rather than spelled letters. */
static int declared_word(const ttf_engine_t *e, const ttf_language_pack_t *source,
                         const ttf_language_pack_t *target, const char *text, size_t n) {
    const char *hit = NULL;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (!source || !target || source == target || n < 2)
        return 0;
    return ttf_foreign_lookup(e, source->language_tag, target->language_tag, text, n, &hit,
                              &layer) == TTF_OK &&
           hit != NULL;
}

/* Letters joined by single hyphens or apostrophes (Coca-Cola, McDonald's). */
static int joined_latin(const char *text, size_t n) {
    size_t i;
    int joiner = 0;
    if (n < 3)
        return 0;
    for (i = 0; i < n; ++i) {
        char c = text[i];
        if (c == '-' || c == '\'') {
            if (!i || i + 1 == n || text[i - 1] == '-' || text[i - 1] == '\'')
                return 0;
            joiner = 1;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            return 0;
    }
    return joiner;
}

/* A declared reading is one the source pack takes from its pronunciation
   dictionary; a generated/grapheme guess is not. */
static int declared_pronunciation(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                  const char *text, size_t n, ttf_lexical_class_t lexical) {
    ttf_pronunciation_t probe;
    size_t attempt = ttf_trace_mark(e);
    int declared = 0;
    if (!source->pronounce)
        return 0;
    memset(&probe, 0, sizeof(probe));
    if (source->pronounce(e, text, n, lexical, &probe) == TTF_OK &&
        memchr(probe.provenance, 0, sizeof(probe.provenance)) &&
        strstr(probe.provenance, "pronunciation-dictionary"))
        declared = 1;
    ttf_trace_reject_since(e, attempt);
    return declared;
}

/* Coca-Cola, McDonald's: a word of letter runs joined by hyphens or apostrophes
   that the source cannot pronounce whole is read part by part. The joiners are
   silent and a final 's takes the target's latin_possessive reading (스). A
   capital part keeps the initialism class. */
static ttf_status_t render_joined(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                  const ttf_language_pack_t *target, const char *text, size_t n,
                                  ttf_buffer_t *out, char trace[64], ttf_resource_layer_t *layer,
                                  size_t part_begin) {
    ttf_status_t st = TTF_OK;
    const char *possessive =
        ttf_resource_get(e, "latin_possessive", target->language_tag, "s", NULL);
    size_t pos = 0;
    *layer = TTF_LAYER_COMMON;
    while (pos < n) {
        size_t end = pos, k;
        int upper = 1;
        if (text[pos] == '-' || text[pos] == '\'') {
            ++pos;
            continue;
        }
        while (end < n && text[end] != '-' && text[end] != '\'')
            ++end;
        for (k = pos; k < end; ++k)
            if (text[k] < 'A' || text[k] > 'Z')
                upper = 0;
        if (possessive && end == n && end - pos == 1 && text[pos - 1] == '\'' &&
            (text[pos] == 's' || text[pos] == 'S')) {
            ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin + pos - 1, part_begin + end);
            st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                target->language_tag, "decision", "latin-possessive", "'s",
                                possessive, "latin-possessive", SIZE_MAX);
            if (st == TTF_OK)
                st = ttf_buffer_puts(out, possessive);
        } else {
            char part_trace[64] = {0};
            ttf_resource_layer_t part_layer = TTF_LAYER_COMMON;
            st = render_at(e, source, target, text + pos, end - pos,
                           upper && end - pos > 1 ? TTF_LEX_INITIALISM : TTF_LEX_WORD, out,
                           part_trace, &part_layer, part_begin + pos);
            if (part_layer > *layer)
                *layer = part_layer;
        }
        if (st != TTF_OK)
            return st;
        pos = end;
    }
    snprintf(trace, 64, "joined:parts");
    return TTF_OK;
}

static ttf_status_t render_at(const ttf_engine_t *e, const ttf_language_pack_t *source,
                              const ttf_language_pack_t *target, const char *text, size_t n,
                              ttf_lexical_class_t lexical, ttf_buffer_t *out, char trace[64],
                              ttf_resource_layer_t *layer, size_t part_begin) {
    const char *hit;
    ttf_status_t st;
    ttf_pronunciation_t pron;
    size_t before = out->length, attempt;
    int source_pronounced = 0;
    char *trace_text = NULL;
    if (!source || !target)
        return TTF_EINVAL;
    /* A lone letter in another language's sentence names its letter (A 학점, x 값). */
    if (n == 1 && ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z')))
        lexical = TTF_LEX_INITIALISM;
    ttf_trace_stage(e, TTF_TRACE_LEXICON, part_begin, part_begin + n);
    st = ttf_foreign_lookup(e, source->language_tag, target->language_tag, text, n, &hit, layer);
    if (st != TTF_OK)
        return st;
    if (hit) {
        snprintf(trace, 64, "lexicon:%s", ttf_layer_name(*layer));
        st = ttf_trace_foreign_lexicon(e, source->language_tag, target->language_tag, text, n,
                                       "whole-part-lexicon");
        return st == TTF_OK ? ttf_buffer_puts(out, hit) : st;
    }
    if (lexical != TTF_LEX_INITIALISM && e->unknown_foreign_policy == TTF_UNKNOWN_FOREIGN_GUESS) {
        st = ttf_foreign_loanword(e, source->language_tag, target->language_tag, text, n, &hit,
                                  layer);
        if (st != TTF_OK)
            return st;
        if (hit) {
            snprintf(trace, 64, "loanword:%s", ttf_layer_name(*layer));
            st = ttf_trace_emit(e, TTF_TRACE_SELECTED, *layer, 0, target->language_tag, "loanword",
                                source->language_tag, hit, hit, "whole-part-loanword", SIZE_MAX);
            return st == TTF_OK ? ttf_buffer_puts(out, hit) : st;
        }
    }
    /* R&D: letter runs keep initialism phones; the joiner follows the output
       mode, a target symbol name in speech and the written mark in text. */
    if (lexical == TTF_LEX_INITIALISM && n > 1 && memchr(text, '&', n)) {
        size_t pos = 0;
        const char *joiner =
            e->frontend_options.output_mode == TTF_OUTPUT_SPEECH
                ? ttf_resource_get(e, "speech_symbol", target->language_tag, "&", NULL)
                : "&";
        *layer = TTF_LAYER_COMMON;
        while (pos < n) {
            size_t end = pos;
            if (text[pos] == '&') {
                ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin + pos, part_begin + pos + 1);
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                    target->language_tag, "decision", "initialism-joiner", "&",
                                    joiner ? joiner : "&", "ampersand-initialism", SIZE_MAX);
                if (st == TTF_OK)
                    st = ttf_buffer_puts(out, joiner ? joiner : "&");
                end = pos + 1;
            } else {
                char part_trace[64] = {0};
                ttf_resource_layer_t part_layer = TTF_LAYER_COMMON;
                while (end < n && text[end] != '&')
                    ++end;
                st = render_at(e, source, target, text + pos, end - pos, TTF_LEX_INITIALISM, out,
                               part_trace, &part_layer, part_begin + pos);
                if (part_layer > *layer)
                    *layer = part_layer;
            }
            if (st != TTF_OK)
                return st;
            pos = end;
        }
        snprintf(trace, 64, "initialism:ampersand");
        return TTF_OK;
    }
    /* Preserve whole-term overrides above. Otherwise an alphanumeric label
       consists of spoken word/letter runs and target-language numeric runs. */
    if (lexical == TTF_LEX_ALNUM_TERM) {
        size_t pos = 0;
        int valid = 1, digits = 0;
        size_t prefix = 0;
        const char *initialism_policy =
            ttf_resource_get(e, "alnum_number", target->language_tag, "initialism", NULL);
        /* alnum_number <lang> word_label single-digit: only a single digit after a word keeps
           the source numeral (best5 베스트 파이브); WELCOME2026 웰컴 이천이십육, IP68 아이피
           육십팔 (project-owner decision 2026-10-16) */
        const char *word_policy =
            ttf_resource_get(e, "alnum_number", target->language_tag, "word_label", NULL);
        int word_single = word_policy && !strcmp(word_policy, "single-digit");
        while (prefix < n && text[prefix] >= 'A' && text[prefix] <= 'Z')
            ++prefix;
        int code_word = prefix >= 2 && prefix < n && declared_word(e, source, target, text, prefix);
        int spell_digits = 0;
        if (prefix >= 2 && prefix < n && !code_word &&
            (text[prefix] == '-' || (text[prefix] >= '0' && text[prefix] <= '9')) &&
            initialism_policy &&
            (!strcmp(initialism_policy, "source-digits") || !strcmp(initialism_policy, "target-digits"))) {
            /* GV70, PS5: a model number of one or two digits ends the label and
               is read like G80 (지브이칠십, 피에스파이브). Longer codes (ABC123)
               keep their digits one by one, with the source numeral words. */
            size_t run = prefix;
            while (run < n && text[run] >= '0' && text[run] <= '9')
                ++run;
            if (!(run == n && run - prefix <= 2))
                spell_digits = 1;
        }
        /* R-20261015 알 이공이육일공일오: one letter and five digits or more are a code
           (target-digits, project-owner decision 2026-10-16) */
        if (!spell_digits && prefix == 1 && initialism_policy && !strcmp(initialism_policy, "target-digits")) {
            size_t r0 = prefix + (prefix < n && text[prefix] == '-'), r1 = r0;
            while (r1 < n && text[r1] >= '0' && text[r1] <= '9')
                ++r1;
            if (r1 - r0 >= 5)
                spell_digits = 1;
        }
        for (size_t k = 0; k < n; ++k) {
            unsigned char c = (unsigned char)text[k];
            if (c >= '0' && c <= '9')
                digits = 1;
            else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-'))
                valid = 0;
        }
        if (valid && digits) {
            int separate = 0, word_label = 0;
            *layer = TTF_LAYER_COMMON;
            while (pos < n) {
                size_t end = pos;
                if (text[pos] == '-') {
                    /* The hyphen is a label joiner, not a negative sign. */
                    ++pos;
                    continue;
                }
                if (text[pos] >= '0' && text[pos] <= '9') {
                    ttf_semantic_t sem = {0};
                    while (end < n && text[end] >= '0' && text[end] <= '9')
                        ++end;
                    if (end - pos >= sizeof(sem.value))
                        return TTF_ELIMIT;
                    sem.struct_size = sizeof(sem);
                    sem.type = end - pos > 1 && text[pos] == '0' ? TTF_SEM_DIGIT_SEQUENCE
                                                                 : TTF_SEM_CARDINAL;
                    memcpy(sem.value, text + pos, end - pos);
                    ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin + pos, part_begin + end);
                    /* A word label keeps the source numeral (SAVE20 → 세이브 트웬티),
                       and so does a single digit after letters (k7 → 케이세븐).
                       Longer runs stay in the target language (G80 → 지팔십,
                       N10번 버스 엔십번); a long code is read digit by digit. */
                    if (spell_digits && end - pos > 1 && !strcmp(initialism_policy, "target-digits")) {
                        /* target-digits: a group of two digits or more is a Korean digit sequence
                           (KE081 케이이 공팔일, RTX4090 알티엑스사공구공); GPT-4 keeps 포
                           (project-owner decision 2026-10-16) */
                        /* the identifier reading of the digit groups (0 is 공), four digits
                           at a time */
                        size_t c;
                        st = TTF_OK;
                        for (c = pos; c < end && st == TTF_OK; c += 4) {
                            ttf_semantic_t part = sem;
                            size_t w = end - c < 4 ? end - c : 4;
                            part.type = TTF_SEM_DIGIT_SEQUENCE;
                            snprintf(part.unit, sizeof(part.unit), "spaced_groups");
                            memcpy(part.value, text + c, w);
                            part.value[w] = 0;
                            st = target->verbalize(e, &part, text + c, w, out);
                        }
                    } else if (spell_digits) {
                        st = TTF_OK;
                        for (size_t d = pos; d < end && st == TTF_OK; ++d) {
                            ttf_semantic_t one = sem;
                            one.type = TTF_SEM_CARDINAL;
                            one.value[0] = text[d];
                            one.value[1] = 0;
                            st = source_numeral(e, source, target, &one, text + d, 1, out, layer,
                                                part_begin + d);
                        }
                    } else if ((word_label && !(word_single && end - pos > 1)) ||
                               (sem.type == TTF_SEM_CARDINAL && end - pos == 1 &&
                                /* alnum_number <lang> single_digit target: Khu D4 D bốn (vi, 2026-10-21) */
                                !(ttf_resource_get(e, "alnum_number", target->language_tag, "single_digit", NULL) &&
                                  !strcmp(ttf_resource_get(e, "alnum_number", target->language_tag, "single_digit", NULL),
                                          "target"))))
                        st = source_numeral(e, source, target, &sem, text + pos, end - pos, out,
                                            layer, part_begin + pos);
                    else
                        st = target->verbalize(e, &sem, text + pos, end - pos, out);
                    if (st == TTF_OK)
                        st = ttf_trace_emit(
                            e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                            "decision", "alnum-number", sem.value, sem.value,
                            sem.type == TTF_SEM_DIGIT_SEQUENCE ? "leading-zero-digits"
                                                               : "cardinal-label",
                            SIZE_MAX);
                } else {
                    int upper = 1;
                    char part_trace[64] = {0};
                    ttf_resource_layer_t part_layer = TTF_LAYER_COMMON;
                    while (end < n && ((text[end] >= 'a' && text[end] <= 'z') ||
                                       (text[end] >= 'A' && text[end] <= 'Z'))) {
                        if (text[end] >= 'a' && text[end] <= 'z')
                            upper = 0;
                        ++end;
                    }
                    /* A label whose letters have a declared word reading is a
                       word followed by a number, so the two stand apart. */
                    if (declared_word(e, source, target, text + pos, end - pos)) {
                        word_label = 1;
                        separate = end < n;
                    }
                    {
                        /* rm1207 알엠: a lowercase run without a vowel is letters (owner v2,
                           2026-10-16) */
                        size_t v;
                        int vowel = 0;
                        for (v = pos; v < end; ++v)
                            vowel |= strchr("aeiouyAEIOUY", text[v]) != NULL;
                        if (!vowel)
                            upper = 1;
                    }
                    st = render_at(e, source, target, text + pos, end - pos,
                                   (upper || end - pos == 1) ? TTF_LEX_INITIALISM : TTF_LEX_WORD,
                                   out, part_trace, &part_layer, part_begin + pos);
                    if (part_layer > *layer)
                        *layer = part_layer;
                }
                if (st != TTF_OK)
                    return st;
                pos = end;
                if (separate && pos < n) {
                    st = ttf_buffer_puts(out, " ");
                    if (st != TTF_OK)
                        return st;
                    separate = 0;
                }
            }
            snprintf(trace, 64, "alnum:composed");
            return TTF_OK;
        }
    }
    *layer = TTF_LAYER_COMMON;
    st = TTF_EINVAL;
    /* A word with no declared reading follows the caller's policy: guess it,
       spell its letters, or keep the source text. Letter labels and declared
       spellings above are unaffected. */
    if (e->unknown_foreign_policy != TTF_UNKNOWN_FOREIGN_GUESS && lexical != TTF_LEX_INITIALISM &&
        lexical != TTF_LEX_ALNUM_TERM && !declared_pronunciation(e, source, text, n, lexical)) {
        const char *detail = e->unknown_foreign_policy == TTF_UNKNOWN_FOREIGN_SPELL
                                 ? "unknown-foreign-spelled"
                                 : "unknown-foreign-preserved";
        ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin, part_begin + n);
        st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                            "decision", "unknown-foreign", "", detail, detail, SIZE_MAX);
        if (st != TTF_OK)
            return st;
        snprintf(trace, 64, "%s", detail);
        if (e->unknown_foreign_policy == TTF_UNKNOWN_FOREIGN_PRESERVE)
            return ttf_buffer_append(out, text, n);
        {
            ttf_semantic_t sem = {0};
            size_t i;
            if (n >= sizeof(sem.value))
                return ttf_buffer_append(out, text, n);
            for (i = 0; i < n; ++i)
                if (!((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') ||
                      (text[i] >= '0' && text[i] <= '9')))
                    return ttf_buffer_append(out, text, n);
            sem.struct_size = sizeof(sem);
            sem.type = TTF_SEM_CUSTOM;
            snprintf(sem.unit, sizeof(sem.unit), "@alphabet");
            memcpy(sem.value, text, n);
            return target->verbalize(e, &sem, sem.value, n, out);
        }
    }
    if (!target->render_phones && !target->render_graphemes) {
        snprintf(trace, 64, "preserved:no-renderer");
        ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin, part_begin + n);
        (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                             "decision", "preserve", "", "", "target-has-no-renderer", SIZE_MAX);
        return ttf_buffer_append(out, text, n);
    }
    /* A source verbalizer can produce several words (e.g. an ordinal). Resolve
       the complete phrase above first, then preserve its whitespace and apply
       the same layered pipeline to individual words. */
    if (memchr(text, ' ', n) || memchr(text, '\t', n) || memchr(text, '\n', n)) {
        unsigned paths = 0;
        size_t at = 0;
        *layer = TTF_LAYER_COMMON;
        while (at < n) {
            size_t end = at;
            int whitespace = text[at] == ' ' || text[at] == '\t' || text[at] == '\n';
            while (end < n &&
                   ((text[end] == ' ' || text[end] == '\t' || text[end] == '\n') == whitespace))
                end++;
            if (whitespace)
                st = ttf_buffer_append(out, text + at, end - at);
            else {
                char part_trace[64] = {0};
                ttf_resource_layer_t part_layer = TTF_LAYER_COMMON;
                st = render_at(e, source, target, text + at, end - at, TTF_LEX_WORD, out,
                               part_trace, &part_layer, part_begin + at);
                if (part_layer > *layer)
                    *layer = part_layer;
                if (strstr(part_trace, "lexicon"))
                    paths |= 1;
                else if (strstr(part_trace, "dictionary"))
                    paths |= 2;
                else if (strstr(part_trace, "grapheme-fallback"))
                    paths |= 8;
                else
                    paths |= 4;
            }
            if (st != TTF_OK)
                return st;
            at = end;
        }
        snprintf(trace, 64, "phrase:%s%s%s%s", paths & 1 ? "lexicon " : "",
                 paths & 2 ? "dictionary " : "", paths & 4 ? "g2p " : "",
                 paths & 8 ? "grapheme-fallback" : "");
        return TTF_OK;
    }
    if (e->trace_state) {
        trace_text = ttf_strndup(text, n);
        if (!trace_text)
            return TTF_ENOMEM;
    }
    memset(&pron, 0, sizeof(pron));
    attempt = ttf_trace_mark(e);
    source_pronounced = 0;
    if (source->pronounce && target->render_phones) {
        char source_trace[64];
        int target_attempted = 0;
        ttf_resource_layer_t source_layer = TTF_LAYER_COMMON;
        ttf_trace_stage(e, TTF_TRACE_SOURCE_PRONUNCIATION, part_begin, part_begin + n);
        st = source->pronounce(e, text, n, lexical, &pron);
        if (st == TTF_OK) {
            source_pronounced = 1;
            st = pronunciation_provenance(&pron, source_trace, &source_layer);
            if (st != TTF_OK) {
                free(trace_text);
                return st;
            }
            ttf_trace_stage(e, TTF_TRACE_SOURCE_PRONUNCIATION, part_begin, part_begin + n);
            (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, source->language_tag,
                                 "pronunciation-ir", pron.alphabet, pron.phones, trace_text,
                                 source_trace, SIZE_MAX);
            ttf_trace_stage(e, TTF_TRACE_TARGET_PHONETIC, part_begin, part_begin + n);
            target_attempted = 1;
            st = target->render_phones(e, &pron, out);
        }
        if (st == TTF_OK && out->length > before) {
            snprintf(trace, 64, "%s", source_trace);
            *layer = source_layer;
            (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                                 "decision", "phone-renderer", "", trace_text,
                                 "phone-renderer-success", SIZE_MAX);
            free(trace_text);
            return ttf_trace_status(e);
        }
        out->length = before;
        if (out->data)
            out->data[before] = 0;
        ttf_trace_reject_since(e, attempt);
        ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin, part_begin + n);
        (void)ttf_trace_emit(e, TTF_TRACE_REJECTED, TTF_LAYER_COMMON, 0,
                             target_attempted ? target->language_tag : source->language_tag,
                             "decision", "phone-pipeline", ttf_status_string(st), trace_text,
                             !target_attempted ? "source-pronunciation-failed"
                             : st == TTF_OK    ? "empty-phonetic-output"
                                               : "target-phonetic-failed",
                             SIZE_MAX);
        if (st == TTF_ENOMEM || st == TTF_ELIMIT) {
            free(trace_text);
            return st;
        }
    }
    /* The source could not pronounce the joined spelling as a whole. */
    if (joined_latin(text, n) && out->length == before && (!source->pronounce || !source_pronounced)) {
        free(trace_text);
        return render_joined(e, source, target, text, n, out, trace, layer, part_begin);
    }
    if (target->render_graphemes) {
        ttf_trace_stage(e, TTF_TRACE_TARGET_GRAPHEME, part_begin, part_begin + n);
        st = target->render_graphemes(e, text, n, out);
        if (st == TTF_OK && out->length > before) {
            snprintf(trace, 64, "grapheme-fallback");
            (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                                 "decision", "grapheme-renderer", "", trace_text,
                                 "grapheme-renderer-success", SIZE_MAX);
            free(trace_text);
            return ttf_trace_status(e);
        }
    }
    free(trace_text);
    return st == TTF_OK ? TTF_EINVAL : st;
}
/* The ASCII base letter of an accented Latin letter (é e, ở o, Đ D), '-' for a
   combining mark to drop, 0 for anything else. */
static char latin_base(uint32_t cp) {
    static const char latin1[] = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUY*saaaaaaaceeeeiiiidnooooo*ouuuuy*y";
    static const char extended_a[] = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLl"
                                     "NnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
    if (cp >= 0xc0 && cp <= 0xff)
        return latin1[cp - 0xc0] == '*' ? 0 : latin1[cp - 0xc0];
    if (cp >= 0x100 && cp <= 0x17f)
        return extended_a[cp - 0x100];
    if (cp == 0x1a0 || cp == 0x1af)
        return cp == 0x1a0 ? 'O' : 'U';
    if (cp == 0x1a1 || cp == 0x1b0)
        return cp == 0x1a1 ? 'o' : 'u';
    if (cp >= 0x1ea0 && cp <= 0x1ef9) {
        /* Vietnamese: A 24, E 16, I 4, O 24, U 14, Y 8, capital first in each pair */
        static const struct { uint32_t last; char base; } rows[] = {
            {0x1eb7, 'a'}, {0x1ec7, 'e'}, {0x1ecb, 'i'}, {0x1ee3, 'o'}, {0x1ef1, 'u'}, {0x1ef9, 'y'}};
        size_t k;
        for (k = 0; k < sizeof(rows) / sizeof(*rows); ++k)
            if (cp <= rows[k].last)
                return cp % 2 ? rows[k].base : (char)(rows[k].base - 'a' + 'A');
    }
    if (cp >= 0x300 && cp <= 0x36f)
        return '-';
    return 0;
}

ttf_status_t ttf_foreign_render(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                const ttf_language_pack_t *target, const char *text, size_t n,
                                ttf_lexical_class_t lexical, ttf_buffer_t *out, char trace[64],
                                ttf_resource_layer_t *layer) {
    /* Café, Pokémon, phở in a Korean sentence: the accents go, the letters stay (cafe,
       pokemon, pho), so the reading keeps every letter; before, the grapheme guess dropped
       é and ở (크아프으; 2026-10-14). Only when every non-ASCII character is a Latin letter
       or a combining mark, and never for a pack that writes these letters itself
       (vi-VN, `latin_sentence`). */
    char folded[128];
    size_t at = 0, used = 0;
    int changed = 0;
    const char *own = ttf_resource_get(e, "context", target->language_tag, "latin_sentence", NULL);
    if (own && strcmp(own, "0"))
        at = n + 1;
    while (at < n && used + 1 < sizeof(folded)) {
        uint32_t cp;
        size_t w;
        unsigned char c = (unsigned char)text[at];
        char base;
        if (c < 0x80) {
            folded[used++] = (char)c;
            ++at;
            continue;
        }
        if (ttf_utf8_decode(text + at, n - at, &cp, &w) != 1 || !(base = latin_base(cp)))
            break;
        if (base != '-')
            folded[used++] = base;
        changed = 1;
        at += w;
    }
    if (changed && at == n && used)
        return render_at(e, source, target, folded, used, lexical, out, trace, layer, 0);
    return render_at(e, source, target, text, n, lexical, out, trace, layer, 0);
}
