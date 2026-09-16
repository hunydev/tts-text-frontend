#include "../core/internal.h"

uint32_t ttf_quote_closer(uint32_t cp) {
    switch (cp) {
    case '"':
        return '"';
    case 0x201c:
        return 0x201d;
    case 0x00ab:
        return 0x00bb;
    case 0x300c:
        return 0x300d;
    case 0x300e:
        return 0x300f;
    default:
        return 0;
    }
}
