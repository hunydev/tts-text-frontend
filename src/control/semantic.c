#include "../core/internal.h"
#include "../pronunciation/trace.h"

/* Explicit interpretations are shared syntax, not language-pair converters.
   Packs opt in from CUSTOM and still own every spoken label and number policy. */
static int digit(char c) { return c >= '0' && c <= '9'; }

/* Bounded common numeric contract: 18 integral digits (safe in every built-in
   pack), optional sign and strict comma groups. No exponent or guessing. */
static ttf_status_t number(const char *s, size_t n, int decimal, char out[TTF_IR_VALUE_SIZE]) {
    size_t i = 0, k = 0, count = 0, group = 0;
    int grouped = 0;
    if (!n)
        return TTF_EINVAL;
    if (n >= TTF_IR_VALUE_SIZE)
        return TTF_ELIMIT;
    if (s[i] == '+' || s[i] == '-')
        out[k++] = s[i++];
    for (; i < n && s[i] != '.'; ++i) {
        if (digit(s[i])) {
            out[k++] = s[i];
            ++count;
            ++group;
        } else if (s[i] == ',' && group && (grouped ? group == 3 : group <= 3)) {
            grouped = 1;
            group = 0;
        } else
            return TTF_EINVAL;
    }
    if (!count || (grouped && group != 3) || (decimal == 1 && i == n) ||
        (decimal == 0 && i != n))
        return TTF_EINVAL;
    if (count > 18)
        return TTF_ELIMIT;
    if (i < n) {
        out[k++] = s[i++];
        if (i == n)
            return TTF_EINVAL;
        for (; i < n; ++i) {
            if (!digit(s[i]))
                return TTF_EINVAL;
            out[k++] = s[i];
        }
    }
    out[k] = '\0';
    return TTF_OK;
}
static int nonzero(const char *s) {
    for (; *s; ++s)
        if (*s >= '1' && *s <= '9')
            return 1;
    return 0;
}
static int uint_part(const char *s, size_t n, size_t *at, size_t min, size_t max,
                     unsigned *value) {
    size_t begin = *at;
    *value = 0;
    while (*at < n && digit(s[*at]) && *at - begin < max) {
        *value = *value * 10 + (unsigned)(s[(*at)++] - '0');
    }
    return *at - begin >= min && (*at == n || !digit(s[*at]));
}
static int calendar(const char *s, size_t n, const char *name, ttf_semantic_t *ir) {
    unsigned a, b, c, year, month, day;
    size_t at = 0;
    int ymd = !strcmp(name, "@date");
    char sep;
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (!uint_part(s, n, &at, ymd ? 4 : 1, ymd ? 4 : 2, &a) || at == n)
        return 0;
    sep = s[at++];
    if ((sep != '-' && sep != '/' && sep != '.') ||
        !uint_part(s, n, &at, 1, 2, &b) || at == n || s[at++] != sep ||
        !uint_part(s, n, &at, ymd ? 1 : 4, ymd ? 2 : 4, &c) || at != n)
        return 0;
    year = ymd ? a : c;
    month = ymd ? b : !strcmp(name, "@date-mdy") ? a : b;
    day = ymd ? c : !strcmp(name, "@date-mdy") ? b : a;
    if (!year || !month || month > 12 || !day ||
        day > days[month - 1] + (month == 2 && year % 4 == 0 &&
                                 (year % 100 != 0 || year % 400 == 0)))
        return 0;
    ir->type = TTF_SEM_DATE;
    (void)snprintf(ir->value, sizeof(ir->value), "%04u-%02u-%02u", year, month, day);
    /* mdy/dmy specifies input order, not target language's date reading style. */
    return 1;
}
static ttf_status_t telephone(const char *s, size_t n, ttf_semantic_t *ir) {
    size_t i = 0, k = 0;
    int international = n && s[0] == '+';
    if (!n || n >= sizeof(ir->value))
        return n ? TTF_ELIMIT : TTF_EINVAL;
    if (international)
        ir->value[k++] = s[i++];
    while (i < n) {
        size_t begin = i;
        while (i < n && digit(s[i]))
            ir->value[k++] = s[i++];
        if (i == begin)
            return TTF_EINVAL;
        if (i < n) {
            if (s[i] == '-')
                ++i;
            else if (s[i] == ' ')
                while (i < n && s[i] == ' ')
                    ++i;
            else
                return TTF_EINVAL;
            if (i == n || !digit(s[i]))
                return TTF_EINVAL;
            ir->value[k++] = '-';
        }
    }
    if (k == (size_t)international)
        return TTF_EINVAL;
    ir->value[k] = '\0';
    ir->type = TTF_SEM_PHONE;
    strcpy(ir->unit, international ? "international" : "national");
    return TTF_OK;
}
static ttf_status_t dotted(const char *s, size_t n, int ip, ttf_semantic_t *ir) {
    size_t at = 0, parts = 0;
    if (!n || n >= sizeof(ir->value))
        return n ? TTF_ELIMIT : TTF_EINVAL;
    if (!ip && (s[0] == 'v' || s[0] == 'V'))
        ir->secondary[0] = s[at++];
    while (at < n) {
        size_t start = at;
        unsigned octet = 0;
        while (at < n && digit(s[at])) {
            if (ip && at - start >= 3)
                return TTF_EINVAL;
            if (ip)
                octet = octet * 10 + (unsigned)(s[at] - '0');
            ++at;
        }
        if (at == start || (ip && octet > 255))
            return TTF_EINVAL;
        if (at - start > 18)
            return TTF_ELIMIT;
        ++parts;
        if (at != n && (s[at++] != '.' || at == n))
            return TTF_EINVAL;
    }
    if (ip ? parts != 4 : parts < 2)
        return TTF_EINVAL;
    memcpy(ir->value, s, n);
    ir->type = ip ? TTF_SEM_IP_ADDRESS : TTF_SEM_VERSION;
    return TTF_OK;
}
static ttf_status_t classified(const ttf_engine_t *e, const ttf_language_pack_t *pack,
                                const char *text, size_t n, ttf_semantic_type_t type,
                                ttf_semantic_t *ir) {
    ttf_token_t *tokens = NULL;
    size_t count = 0;
    ttf_status_t status = ttf_tokenize(text, n, &tokens, &count);
    ttf_pack_input_t input = {text, n, tokens, count, 0, e};
    if (status != TTF_OK)
        return status;
    status = TTF_EINVAL;
    /* Prefer the pack's specialized grammar, but only a complete match of the
       requested type is eligible. Automatic TEXT-preserve is not success. */
    for (int probe = 0; count && probe < 2; ++probe) {
        ttf_semantic_t candidate = {0};
        candidate.struct_size = sizeof(candidate);
        size_t mark = ttf_trace_mark(e);
        size_t consumed = probe ? ttf_classify_universal(&input, &candidate)
                                : pack->classify ? pack->classify(&input, &candidate) : 0;
        if (consumed == count && candidate.input_end == n && candidate.type == type) {
            *ir = candidate;
            status = TTF_OK;
            break;
        }
        ttf_trace_reject_since(e, mark);
    }
    ttf_tokens_free(tokens);
    if (status == TTF_OK && type != TTF_SEM_TIME) {
        char checked[TTF_IR_VALUE_SIZE];
        if (type != TTF_SEM_RANGE || strcmp(ir->unit, "time")) {
            status = number(ir->value, strlen(ir->value), -1, checked);
            if (status == TTF_OK && type == TTF_SEM_RANGE)
                status = number(ir->secondary, strlen(ir->secondary), -1, checked);
        }
    }
    return status;
}

/* X{@break:N}: read the bound atom or group one character at a time. Digits
   read as {@digits} reads them and Latin letters by name, both through the
   render pack; other characters are kept as written; spaces and ASCII
   punctuation inside a group are dropped. Items are joined by a space (1), a
   comma (2) or a period (3), and strength 3 also ends with a period. */
static ttf_status_t spelled_with_breaks(const ttf_engine_t *e, const ttf_language_pack_t *pack,
                                        const ttf_semantic_t *request, const char *text,
                                        size_t n, int strength, ttf_buffer_t *out) {
    static const char *const separators[] = {"", " ", ", ", ". "};
    size_t at = 0, items = 0, width;
    uint32_t cp;
    ttf_status_t status;
    char detail[32];
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, n);
    (void)snprintf(detail, sizeof(detail), "strength=%d", strength);
    status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_USER, 0, pack->language_tag,
                            "control-semantic", request->unit, "BREAK", "", detail, SIZE_MAX);
    while (status == TTF_OK && at < n) {
        ttf_semantic_t item = {0};
        if (ttf_utf8_decode(text + at, n - at, &cp, &width) != 1)
            return TTF_EUTF8;
        if (cp < 128 && !digit((char)cp) && !((cp | 32) >= 'a' && (cp | 32) <= 'z')) {
            at += width; /* spaces and ASCII punctuation inside a group */
            continue;
        }
        if (items && (status = ttf_buffer_puts(out, separators[strength])) != TTF_OK)
            return status;
        item.struct_size = sizeof(item);
        item.input_end = width;
        memcpy(item.value, text + at, width);
        memcpy(item.context_lang, request->context_lang, sizeof(item.context_lang));
        memcpy(item.render_lang, request->render_lang, sizeof(item.render_lang));
        memcpy(item.source_lang, request->source_lang, sizeof(item.source_lang));
        if (digit((char)cp)) {
            item.type = TTF_SEM_DIGIT_SEQUENCE;
            status = pack->verbalize(e, &item, text + at, width, out);
        } else if (cp < 128) {
            item.type = TTF_SEM_CUSTOM;
            (void)snprintf(item.unit, sizeof(item.unit), "@alphabet");
            status = pack->verbalize(e, &item, text + at, width, out);
        } else
            status = ttf_buffer_append(out, text + at, width);
        ++items;
        at += width;
    }
    if (status != TTF_OK)
        return status;
    if (!items)
        return TTF_EINVAL;
    return strength == 3 ? ttf_buffer_puts(out, ".") : TTF_OK;
}

ttf_status_t ttf_semantic_control(const ttf_engine_t *e, const ttf_semantic_t *request,
                                  const char *text, size_t n, ttf_buffer_t *out) {
    const char *name = request->unit;
    const ttf_language_pack_t *pack = ttf_find_pack(e, request->render_lang);
    ttf_semantic_t ir = {0};
    ttf_status_t status = TTF_EINVAL;
    char detail[256], cased[TTF_IR_VALUE_SIZE];
    const char *groups = NULL; /* {@digits:N...} group sizes */
    static const struct { const char *name; ttf_semantic_type_t type; } grammars[] = {
        {"@time", TTF_SEM_TIME}, {"@currency", TTF_SEM_CURRENCY},
        {"@percent", TTF_SEM_PERCENT}, {"@measure", TTF_SEM_MEASURE},
        {"@counter", TTF_SEM_COUNTER}, {"@range", TTF_SEM_RANGE}
    };
    if (!pack || !n)
        return TTF_EINVAL;
    if (!strcmp(name, "@preserve"))
        return ttf_buffer_append(out, text, n);
    if (!strcmp(name, "@skip"))
        return TTF_OK; /* Caller owns adjoining whitespace, just like a literal. */
    if (!strncmp(name, "@break:", 7) && name[7] >= '1' && name[7] <= '3' && !name[8])
        return spelled_with_breaks(e, pack, request, text, n, name[7] - '0', out);
    if (n >= sizeof(ir.value))
        return TTF_ELIMIT;
    if (!strcmp(name, "@cardinal") || !strcmp(name, "@decimal")) {
        ir.type = TTF_SEM_CARDINAL;
        status = number(text, n, !strcmp(name, "@decimal"), ir.value);
    } else if (!strcmp(name, "@digits") || !strncmp(name, "@digits:", 8) ||
               !strcmp(name, "@ordinal") || !strcmp(name, "@year")) {
        for (size_t i = 0; i < n; ++i)
            if (!digit(text[i]))
                return TTF_EINVAL;
        memcpy(ir.value, text, n);
        if (!strncmp(name, "@digits:", 8)) {
            groups = name + 8;
            for (const char *g = groups; *g; ++g)
                if (*g < '1' || *g > '9')
                    return TTF_EINVAL;
            if (!*groups)
                return TTF_EINVAL;
            ir.type = TTF_SEM_DIGIT_SEQUENCE;
        } else if (!strcmp(name, "@digits"))
            ir.type = TTF_SEM_DIGIT_SEQUENCE;
        else if (!strcmp(name, "@ordinal")) {
            if (!nonzero(ir.value))
                return TTF_EINVAL;
            if (n > 18)
                return TTF_ELIMIT;
            ir.type = TTF_SEM_ORDINAL;
        } else {
            if (n != 4 || !nonzero(ir.value))
                return TTF_EINVAL;
            ir.type = TTF_SEM_DATE;
            strcpy(ir.unit, "year");
        }
        status = TTF_OK;
    } else if (!strcmp(name, "@date") || !strcmp(name, "@date-mdy") ||
               !strcmp(name, "@date-dmy")) {
        status = calendar(text, n, name, &ir) ? TTF_OK : TTF_EINVAL;
    } else if (!strcmp(name, "@fraction")) {
        const char *slash = memchr(text, '/', n);
        if (!slash || slash + 1 == text + n)
            return TTF_EINVAL;
        ir.type = TTF_SEM_FRACTION;
        status = number(text, (size_t)(slash - text), 0, ir.value);
        if (status == TTF_OK)
            status = number(slash + 1, n - (size_t)(slash + 1 - text), 0, ir.secondary);
        if (status == TTF_OK && (!nonzero(ir.secondary) || !digit(ir.secondary[0])))
            status = TTF_EINVAL;
    } else if (!strcmp(name, "@phone")) {
        status = telephone(text, n, &ir);
    } else if (!strcmp(name, "@score")) {
        /* 10-10, 40:30: two whole numbers joined by a hyphen or colon. */
        size_t i = 0, join;
        while (i < n && digit(text[i]))
            ++i;
        join = i;
        if (!join || join + 1 >= n || (text[join] != '-' && text[join] != ':') || join > 18 ||
            n - join - 1 > 18)
            return TTF_EINVAL;
        for (i = join + 1; i < n; ++i)
            if (!digit(text[i]))
                return TTF_EINVAL;
        memcpy(ir.value, text, join);
        memcpy(ir.secondary, text + join + 1, n - join - 1);
        ir.type = TTF_SEM_RANGE;
        snprintf(ir.rule, sizeof(ir.rule), "syntax.score");
        status = TTF_OK;
    } else if (!strcmp(name, "@unit")) {
        /* 3M{@unit}: a number with a Latin unit is a measure even where a
           lexicon term or letter case reads it otherwise. The unit is tried as
           written, then lowercase (3M 삼 미터), then uppercase (5w 오 와트). */
        for (int pass = 0; pass < 3 && status != TTF_OK; ++pass) {
            for (size_t i = 0; i < n; ++i) {
                char c = text[i];
                if (pass == 1 && c >= 'A' && c <= 'Z')
                    c = (char)(c - 'A' + 'a');
                else if (pass == 2 && c >= 'a' && c <= 'z')
                    c = (char)(c - 'a' + 'A');
                cased[i] = c;
            }
            status = classified(e, pack, cased, n, TTF_SEM_MEASURE, &ir);
        }
        if (status == TTF_OK)
            text = cased;
    } else if (!strcmp(name, "@math")) {
        status = ttf_math_expression(text, n, &ir) ? TTF_OK : TTF_EINVAL;
    } else if (!strcmp(name, "@version") || !strcmp(name, "@ip")) {
        status = dotted(text, n, !strcmp(name, "@ip"), &ir);
    } else {
        for (size_t i = 0; i < sizeof(grammars) / sizeof(grammars[0]); ++i)
            if (!strcmp(name, grammars[i].name)) {
                status = classified(e, pack, text, n, grammars[i].type, &ir);
                break;
            }
    }
    if (status != TTF_OK)
        return status;
    ir.struct_size = sizeof(ir);
    ir.input_begin = 0;
    ir.input_end = n;
    memcpy(ir.context_lang, request->context_lang, sizeof(ir.context_lang));
    memcpy(ir.render_lang, request->render_lang, sizeof(ir.render_lang));
    if (!ir.source_lang[0])
        memcpy(ir.source_lang, request->source_lang, sizeof(ir.source_lang));
    /* Keep the inner rule: RANGE verbalizers use syntax.range vs quantity-range.
       Outer public metadata stays CUSTOM, with the requested name and source. */
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, n);
    (void)snprintf(detail, sizeof(detail), "rule=%s;unit=%s;secondary=%s",
                   ir.rule[0] ? ir.rule : "explicit-type", ir.unit, ir.secondary);
    status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_USER, 0, pack->language_tag,
                            "control-semantic", name, ttf_semantic_type_name(ir.type), ir.value,
                            detail, SIZE_MAX);
    if (status != TTF_OK)
        return status;
    if (groups) {
        /* 123123{@digits:12} 일 이삼 일 이삼: each group reads like {@digits};
           a value shorter than the group is one group. */
        size_t at = 0, step = 0, count = strlen(groups);
        while (at < n) {
            size_t size = (size_t)(groups[step++ % count] - '0');
            ttf_semantic_t part = ir;
            if (size > n - at)
                size = n - at;
            memcpy(part.value, text + at, size);
            part.value[size] = '\0';
            part.input_end = size;
            if (at && (status = ttf_buffer_puts(out, " ")) != TTF_OK)
                return status;
            if ((status = pack->verbalize(e, &part, text + at, size, out)) != TTF_OK)
                return status;
            at += size;
        }
        return TTF_OK;
    }
    return pack->verbalize(e, &ir, text, n, out);
}
