#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#ifndef DPREADER_LOVE_BOOK
#define DPREADER_LOVE_BOOK 0
#endif
// LumenOS uses -love; accept the historical -love-vn suffix as the same edition.
static inline bool firmware_love_edition(const char *version, size_t capacity) {
    if(!version || !memchr(version,0,capacity))return false;
    size_t n=strlen(version);
    return (n>=5 && !strcmp(version+n-5,"-love")) ||
           (n>=8 && !strcmp(version+n-8,"-love-vn"));
}
static inline bool firmware_same_edition(const char *a, size_t an, const char *b, size_t bn) {
    return a && b && memchr(a,0,an) && memchr(b,0,bn) &&
           (firmware_love_edition(a,an)==firmware_love_edition(b,bn));
}
