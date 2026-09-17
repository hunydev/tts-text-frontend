#ifndef TTF_RESOURCE_VALIDATION_H
#define TTF_RESOURCE_VALIDATION_H
#include "ttf.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TTF_RESOURCE_VALIDATOR_ABI_VERSION 1u
/* All strings in a record are borrowed and valid only for the callback. */
typedef struct {
    uint32_t struct_size;
    const char *kind, *language, *key, *value;
    ttf_resource_layer_t layer;
} ttf_resource_record_t;
typedef struct {
    uint32_t struct_size;
    ttf_resource_layer_t layer;
    size_t line; /* 1-based during a file load; zero for final engine checks. */
    char path[512], kind[64], language[TTF_LANGUAGE_TAG_SIZE], key[256], message[256];
} ttf_resource_diagnostic_t;
typedef struct {
    uint32_t struct_size;
    size_t records, validated_records, unvalidated_records;
} ttf_resource_validation_report_t;
typedef struct {
    uint32_t struct_size, abi_version;
    const char
        *name; /* Unique nonempty ID. Descriptor, strings and userdata must outlive engine. */
    void *userdata;
    /* Pure row check; never mutate the engine or retain record pointers. Set
       handled=1 only when this validator knows this record's semantics. Multiple
       validators may check the same row; every applicable check must succeed.
       Error callbacks should write diagnostic.message (other fields are supplied
       by the core). Return TTF_ERESOURCE for an invalid resource. */
    ttf_status_t (*validate_record)(const ttf_resource_record_t *record, int *handled,
                                    ttf_resource_diagnostic_t *diagnostic, void *userdata);
    /* Read-only final check of effective resources, e.g. min <= max. All rows
       have passed validate_record first. Use ttf_resource_get for layered values.
       Callbacks must not recursively invoke validation or destroy the engine.
       Mutable setup engines reject reentrant setup; frozen validation relies
       on the callback's purity and is safe to run concurrently only under it.
       May populate diagnostic kind/language/key/message when rejecting. */
    ttf_status_t (*validate_engine)(const ttf_engine_t *engine,
                                    ttf_resource_diagnostic_t *diagnostic, void *userdata);
} ttf_resource_validator_t;
TTF_API void ttf_resource_diagnostic_init(ttf_resource_diagnostic_t *diagnostic);
TTF_API void ttf_resource_validation_report_init(ttf_resource_validation_report_t *report);
/* Additive APIs: existing engine/config/pack/public span ABI layouts are unchanged.
   Setup is caller-serialized. Validators are registered before freeze. Unknown
   resource namespaces remain loadable and are counted as unvalidated, not silently
   certified. Registration is non-owning and does not mutate loaded resources. */
TTF_API ttf_status_t ttf_engine_register_resource_validator(
    ttf_engine_t *engine, const ttf_resource_validator_t *validator);
TTF_API ttf_status_t ttf_engine_create_ex(const ttf_config_t *config, ttf_engine_t **out,
                                          ttf_resource_diagnostic_t *diagnostic);
TTF_API ttf_status_t ttf_resource_load_ex(ttf_engine_t *engine, const char *path,
                                          ttf_resource_layer_t layer,
                                          ttf_resource_diagnostic_t *diagnostic);
/* Const/read-only, may be called before freeze; report/diagnostic are optional.
   Caller initializes non-NULL outputs with the helpers. No unknown-kind rejection
   is implied by TTF_OK; inspect unvalidated_records for certification coverage. */
TTF_API ttf_status_t ttf_engine_validate_resources(const ttf_engine_t *engine,
                                                   ttf_resource_validation_report_t *report,
                                                   ttf_resource_diagnostic_t *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
