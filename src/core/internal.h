#ifndef TTF_INTERNAL_H
#define TTF_INTERNAL_H
#include "tts_text_frontend/ttf_language_pack.h"
#include "tts_text_frontend/ttf_resource_validation.h"
#include "tts_text_frontend/ttf_frontend_options.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#define TTF_MAX_PACKS 32
#define TTF_MAX_RESOURCE_VALIDATORS 64
/* Preserve pre-CJK lexical/boundary behavior while exposing richer metadata. */
static inline ttf_script_t ttf_script_legacy(ttf_script_t script) {
    return script == TTF_SCRIPT_HAN || script == TTF_SCRIPT_KANA ? TTF_SCRIPT_OTHER : script;
}

#define TTF_HASH_BUCKETS 1021
/* The resource table has its own, larger size (2026-10-11): with 1021 buckets
   a lookup walked about 24 rows, and lookups were most of the run time. The
   engine keeps it on the heap so request views copy one pointer. 2026-10-24:
   the lookups were 93% of the run time again; with about 43k rows, 65521 buckets
   keep a chain near one row and the stored hash skips the string compares. */
#define TTF_RESOURCE_BUCKETS 65521 /* the starting size; freeze grows it with the rows */
#define TTF_PHRASE_WORDS 8
#define TTF_PHRASE_INPUT 255

typedef struct ttf_entry {
    char *kind, *lang, *key, *value;
    ttf_resource_layer_t layer;
    size_t order;
    uint32_t hash; /* of kind, lang and key; compared before the strings */
    struct ttf_entry *load_next; /* a staging table's rows in insertion order (load merge) */
    struct ttf_entry *next;
    struct ttf_entry *boundary_next;
    struct ttf_entry *phrase_next;
    struct ttf_entry *rule_next;
    struct ttf_entry *cjk_next;
    size_t cjk_length;
    uint32_t cjk_first;
    /* Record validation already done at load: 0 none, 1 handled, 2 not handled
       by any validator, with the validator count it ran under (a validator
       registered later re-checks the row). */
    unsigned char checked;
    size_t checked_validators;
} ttf_entry_t;
/* Owner of the detached CJK rows; word buckets only borrow entry pointers.
   Request-local engine views copy this pointer, never the bucket arrays. */
typedef struct {
    ttf_entry_t **resources; /* the owner's bucket count (ttf_buckets) */
    ttf_entry_t *words[TTF_HASH_BUCKETS];
    size_t max_length;
} ttf_cjk_index_t;
struct ttf_engine {
    ttf_config_t config;
    ttf_frontend_options_t frontend_options;
    ttf_bracket_policy_t bracket_policy; /* Named options live here, not in the public struct. */
    ttf_unknown_foreign_policy_t unknown_foreign_policy;
    int strict_controls; /* invalid_control=error: an invalid control fails the request. */
    int particle_option; /* 0 language policy, 1 off, 2 on (particle_agreement option). */
    int numeric_fallback_off; /* numeric_fallback=preserve: keep rejected numeric spans. */
    ttf_decorative_policy_t decorative_policy;
    ttf_initialism_policy_t initialism_policy;
    int reading_unit_spacing; /* spacing=reading-unit; 0 keeps resource spacing */
    int phoneme_output;       /* pronunciation=hangul: Hangul read as spoken (2026-10-09) */
    ttf_matrix_policy_t matrix_policy; /* matrix=auto (0), legacy or declared */
    int foreign_sentence_localize; /* foreign_sentence=localize: an English reading is written in the
                                      request language (I have 3 apples 아이 해브 쓰리 애플스) */
    ttf_symbol_fallback_policy_t symbol_fallback_policy; /* symbol_fallback=on (0) */
    /* Mapped Unicode fallback files per pack slot; the frozen owner closes them. */
    struct ttf_symbol_fallback *symbol_fallback[TTF_MAX_PACKS];
    char *resource_root, *default_language;
    int frozen, validating;
    struct ttf_trace_state *trace_state;   /* Request view only; frozen owner stays NULL. */
    struct ttf_context_memo *context_memo; /* Unit-local context callback view only. */
    const struct ttf_dict *dict;           /* Request view only: the request dictionary. */
    /* Named dictionaries for {@dictionary:NAME} (owned), and a view inside {@nodict}: no
       exact, lexicon or loanword row of any layer applies (2026-10-27). */
    struct ttf_named_dict *named_dicts;
    size_t named_dict_count;
    int no_dictionary;
    /* Unit-local view only: the previous unit's tail, so a sentence cut into
       units keeps its language (latin_sentence). */
    const char *unit_prefix;
    size_t unit_prefix_length;
    const ttf_language_pack_t *packs[TTF_MAX_PACKS];
    /* A declared-only pack (context <tag> declared_only 1) in a request for
       another language, cached at freeze: per-token lookups cost 8%. */
    unsigned char undeclared[TTF_MAX_PACKS];
    unsigned char han_only[TTF_MAX_PACKS]; /* context han_sentence: classifies only as context */
    size_t pack_count, resource_order;
    const ttf_resource_validator_t *validators[TTF_MAX_RESOURCE_VALIDATORS];
    size_t validator_count;
    size_t builtin_validators; /* engine and pack validators; a row check is reused only with no others */
    ttf_entry_t **resources; /* ttf_buckets(e) heads, allocated on first insert */
    /* The bucket count: 0 (a staging engine) is TTF_RESOURCE_BUCKETS; freeze grows it
       to the row count so a dictionary of a million rows keeps chains short
       (2026-10-25). */
    size_t resource_buckets;
    /* Freeze indexes `exact` rules by hash (rule.c); e->rules then holds only patterns
       and exact keys of 63 bytes or more. */
    int rules_indexed;
    /* Freeze: some exact rule is above the system layer (a domain, customer or user
       dictionary entry), so classification checks for entries inside its span. */
    int dictionary_rules;
    /* exact and pattern rows live in their own table from their first insert, so a
       dictionary of a million rules does not slow every other lookup or the freeze passes
       (2026-10-25). Its TTF_RULE_BUCKETS heads are one sparse calloc: untouched pages cost
       no memory. Freeze grows it past two rows a bucket. */
    ttf_entry_t **rule_table;
    size_t rule_buckets, rule_rows;
    ttf_entry_t *boundary_cues;
    ttf_entry_t *rules;
    ttf_entry_t *phrases[TTF_HASH_BUCKETS];
    ttf_cjk_index_t *cjk;
    /* The byte lengths (bit n = n bytes, bit 63 = 63 or more) of the keys of each kind
       and language, built at freeze in TTF_KIND_SLOTS open-addressed slots: a
       longest-prefix scan tries only lengths that exist (2026-10-24; it was most of
       the lookups). Request views share the owner's. NULL: every length. */
    struct ttf_kind_lengths *kind_lengths;
    /* A file being loaded is parsed into a staging engine; its rows are listed so
       the merge does not walk every bucket (2026-10-24). */
    int staging;
    ttf_entry_t *staged;
};
#define TTF_KIND_SLOTS 4096
#define TTF_DICT_LENGTH_SLOTS 64
/* A request dictionary (dict.c): rows of layer TTF_LAYER_REQUEST in a small table, the
   key lengths of each kind and language, the exact keys of 63 bytes or more
   (rule_next) and the multiword lexicon keys by first word (phrase_next). */
struct ttf_dict {
    ttf_entry_t **buckets;
    size_t bucket_count, rows, order;
    struct {
        const char *kind, *lang;
        uint64_t lengths;
    } lengths[TTF_DICT_LENGTH_SLOTS];
    size_t length_count;
    int lengths_full; /* more kind/language pairs than slots: every length */
    ttf_entry_t *long_rules;
    ttf_entry_t *phrases[TTF_HASH_BUCKETS];
};
const ttf_entry_t *ttf_dict_find(const ttf_dict_t *d, const char *kind, const char *lang,
                                 const char *key);
uint64_t ttf_dict_lengths(const ttf_dict_t *d, const char *kind, const char *lang);
struct ttf_named_dict {
    char name[65];
    ttf_dict_t *dict;
};
/* The engine's dictionary of this name (n bytes, a trailing .tsv ignored), or NULL. */
const ttf_dict_t *ttf_named_dict(const ttf_engine_t *e, const char *name, size_t n);
void ttf_named_dicts_free(ttf_engine_t *e);
/* The kinds a dictionary holds: exact rules, lexicon rows and loanwords ({@nodict}). */
int ttf_dictionary_kind(const char *kind);
/* The phrase bucket of a multiword lexicon word (its first letter run). */
size_t ttf_phrase_bucket(const char *word);
struct ttf_kind_lengths {
    const char *kind, *lang; /* borrowed from a resource row; NULL kind = empty slot */
    uint64_t lengths;
};
/* The key lengths ttf_resource_get(e, kind, lang, …) can match (lang's rows and, for
   another lang, the `*` rows); every bit before freeze. */
uint64_t ttf_resource_key_lengths(const ttf_engine_t *e, const char *kind, const char *lang);
uint32_t ttf_kind_slot(const char *kind, const char *lang);
static inline size_t ttf_buckets(const ttf_engine_t *e) {
    return e->resource_buckets ? e->resource_buckets : TTF_RESOURCE_BUCKETS;
}
uint32_t ttf_entry_hash(const char *kind, const char *lang, const char *key);
/* The main table's chain that holds kind/lang/key rows with this hash. */
const ttf_entry_t *ttf_resource_chain(const ttf_engine_t *e, uint32_t hash);
/* Grows the table to about one bucket per row (freeze; before the CJK split). */
ttf_status_t ttf_resource_resize(ttf_engine_t *e);
#define TTF_RULE_BUCKETS 1048573
/* Grows e->rule_table past two rows a bucket (freeze). */
ttf_status_t ttf_resource_resize_rules(ttf_engine_t *e);
static inline int ttf_rule_resource_kind(const char *kind) {
    return (kind[0] == 'e' && !strcmp(kind, "exact")) || (kind[0] == 'p' && !strcmp(kind, "pattern"));
}
char *ttf_strdup(const char *s);
char *ttf_strndup(const char *s, size_t n);
int ttf_ascii_equal(const char *a, const char *b);
int ttf_utf8_decode(const char *s, size_t n, uint32_t *cp, size_t *width);
ttf_script_t ttf_script_of(uint32_t cp);
/* Delimited quotation scopes; apostrophes remain lexical punctuation. */
uint32_t ttf_quote_closer(uint32_t cp);
ttf_status_t ttf_resource_set(ttf_engine_t *e, const char *kind, const char *lang, const char *key,
                              const char *value, ttf_resource_layer_t layer);
void ttf_resource_destroy(ttf_engine_t *e);
/* Setup transaction only: discard rows appended since checkpoint, before indexing. */
void ttf_resource_rollback(ttf_engine_t *e, size_t checkpoint);
const ttf_entry_t *ttf_resource_entry(const ttf_engine_t *e, const char *kind, const char *lang,
                                      const char *key);
void ttf_validation_copy(char *destination, size_t capacity, const char *source);
ttf_status_t ttf_validate_resource_record(const ttf_engine_t *engine,
                                          const ttf_resource_record_t *record, int *validated,
                                          ttf_resource_diagnostic_t *diagnostic);
const ttf_language_pack_t *ttf_find_pack(const ttf_engine_t *e, const char *tag);
/* The decided language of the current token's sentence, NULL when undecided
   (normalize.c; cached per request). */
const ttf_language_pack_t *ttf_sentence_language(const ttf_pack_input_t *in);
/* Lookup only, before a source verbalizer can replace the original spelling.
   A NULL hit is a normal miss; a hit remains engine-owned. */
ttf_status_t ttf_electronic_render(const ttf_engine_t *engine, const ttf_language_pack_t *source,
                                   const ttf_language_pack_t *target, const char *text,
                                   size_t length, ttf_buffer_t *out);
/* Explicit control syntax validation is independent of automatic URL/TLD cues. */
ttf_status_t ttf_electronic_control(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                                    const char *text, size_t length, ttf_buffer_t *out);
/* Shared explicit Semantic IR controls, called by opting-in language packs. */
ttf_status_t ttf_control_reread(const ttf_engine_t *engine, const ttf_language_pack_t *pack,
                                const char *text, ttf_buffer_t *out);
ttf_status_t ttf_semantic_control(const ttf_engine_t *engine, const ttf_semantic_t *request,
                                  const char *text, size_t length, ttf_buffer_t *out);
ttf_status_t ttf_foreign_loanword(const ttf_engine_t *engine, const char *source, const char *target,
                                  const char *text, size_t n, const char **hit,
                                  ttf_resource_layer_t *layer);
ttf_status_t ttf_foreign_lookup(const ttf_engine_t *engine, const char *source, const char *target,
                                const char *text, size_t length, const char **hit,
                                ttf_resource_layer_t *layer);
const ttf_entry_t *ttf_rule_match(const ttf_engine_t *e, const char *lang, const char *text,
                                  size_t length, size_t *matched_length);
ttf_status_t ttf_register_builtins(ttf_engine_t *e);
/* Bounded multiword pronunciation keys and shared lexical lookahead. */
ttf_status_t ttf_lexicon_canonical_key(const char *key, char **out);
void ttf_phrase_index(ttf_engine_t *engine);
ttf_status_t ttf_cjk_index(ttf_engine_t *engine);
/* A numeric suffix with numeric_suffix_cue (V, A, J; 2026-10-04) is a unit only when
   one of its '|'-separated cue words occurs in the text: after a number these
   letters are mostly model names and codes (253V, 45A) unless the sentence talks
   about voltage, charging or energy. Suffixes without cues are always units. */
static inline int ttf_numeric_suffix_cued(const ttf_engine_t *e, const char *key, const char *text,
                                          size_t length) {
    const char *cues = ttf_resource_get(e, "numeric_suffix_cue", "*", key, NULL), *word;
    if (!cues)
        return 1;
    for (word = cues; *word;) {
        size_t n = strcspn(word, "|"), at;
        for (at = 0; n && at + n <= length; ++at)
            if (!memcmp(text + at, word, n))
                return 1;
        word += n + (word[n] == '|');
    }
    return 0;
}
/* Exactly the thirteen resource kinds separated at freeze (not all kana_* kinds): the
   CJK readings and the Korean pronunciation dictionary (phoneme_word, phoneme_stem,
   phoneme_noun; 2026-10-09),
   large tables whose rows would otherwise lengthen every ordinary lookup. */
static inline int ttf_cjk_resource_kind(const char *kind) {
    /* Every lookup passes here and 'p' kinds (particle, phone_*) are frequent:
       test "phoneme_" by characters before any string compare. */
    if (kind[0] == 'p')
        return kind[1] == 'h' && kind[2] == 'o' && kind[3] == 'n' && kind[4] == 'e' &&
               kind[5] == 'm' && kind[6] == 'e' && kind[7] == '_' &&
               (!strcmp(kind + 8, "word") || !strcmp(kind + 8, "stem") || !strcmp(kind + 8, "noun"));
    if (kind[0] == 'h')
        return !strcmp(kind, "hanja_reading") || !strcmp(kind, "hanja_variant_reading") ||
               !strcmp(kind, "hanja_chinese_reading") || !strcmp(kind, "hanja_japanese_reading") ||
               !strcmp(kind, "hanja_soft_after") ||
               !strcmp(kind, "hanja_before_initial") || !strcmp(kind, "hanja_before_char");
    if (kind[0] == 'c') return !strcmp(kind, "cjk_word");
    return kind[0] == 'k' && (!strcmp(kind, "kana_reading") || !strcmp(kind, "kana_mark") ||
                             !strcmp(kind, "kana_fold") || !strcmp(kind, "kana_long_after") ||
                             !strcmp(kind, "kana_final"));
}
size_t ttf_phrase_protect_until(const ttf_engine_t *engine, const char *text, size_t length,
                                const ttf_token_t *tokens, size_t index);
const char *ttf_phrase_match(const ttf_engine_t *engine, const char *source, const char *target,
                             const char *text, const ttf_token_t *tokens, size_t count,
                             size_t index, size_t *consumed, char value[TTF_IR_VALUE_SIZE],
                             ttf_resource_layer_t *layer);
/* Generic semantic recognition supplied by semantic/classifier.c. */
size_t ttf_classify_universal(const ttf_pack_input_t *input, ttf_semantic_t *out);
/* {@math}: reads all of text as one arithmetic expression (classifier.c). */
int ttf_math_expression(const char *text, size_t n, ttf_semantic_t *out);
/* Normalize one stable context unit; shared by batch and streaming. */
/* A season word (시즌, シーズン, 赛季, season) at `at`, after one optional space. */
int ttf_season_word_at(const char *s, size_t n, size_t at);
ttf_status_t ttf_process_unit(const ttf_engine_t *e, const char *text, size_t length,
                              size_t input_offset, ttf_result_t *out);
/* The same, with up to 64 bytes of the text before the unit for the
   classifiers' bounded look-back (the previous segment of a stream). */
ttf_status_t ttf_process_unit_context(const ttf_engine_t *e, const char *context,
                                      size_t context_length, const char *text, size_t length,
                                      size_t input_offset, ttf_result_t *out);
/* A planner hint only permits skipping bytes that cannot change an incomplete
   literal. It is request-local and never changes the public streaming ABI. */
typedef enum {
    TTF_WAIT_NONE,
    TTF_WAIT_BACKTICKS,
    TTF_WAIT_ELEMENT,
    TTF_WAIT_COMMENT,
    TTF_WAIT_QUOTED,
    TTF_WAIT_PATH
} ttf_wait_kind_t;
typedef struct {
    ttf_wait_kind_t kind;
    size_t begin;
    size_t control_limit; /* Nonzero only for an unfinished annotated control. */
    char quote;
} ttf_boundary_wait_t;
/* Shared lexical boundary planner. A zero boundary requires more lookahead. */
ttf_status_t ttf_next_boundary(const ttf_engine_t *e, const char *text, size_t length, int final,
                               int first, size_t *boundary, ttf_boundary_wait_t *wait);
#endif
