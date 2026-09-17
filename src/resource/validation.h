#ifndef TTF_INTERNAL_RESOURCE_VALIDATION_H
#define TTF_INTERNAL_RESOURCE_VALIDATION_H
#include "tts_text_frontend/ttf_resource_validation.h"
#include "tts_text_frontend/ttf_language_pack.h"
/* Shared, locale-independent scalar validators. Integer spelling is optional
   ASCII sign followed by one or more digits, with no whitespace or suffix. */
int ttf_validation_integer(const char *value, long minimum, long maximum, long *parsed);
int ttf_validation_enum(const char *value, const char *choices); /* choices separated by | */
ttf_status_t ttf_validation_error(ttf_resource_diagnostic_t *diagnostic, const char *message);
extern const ttf_resource_validator_t ttf_resource_validator_core;
#endif
