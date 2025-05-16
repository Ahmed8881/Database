#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>

// Case-insensitive substring search
char* strcasestr(const char* haystack, const char* needle);

uint32_t count_commas(char *str, int len);

char* my_strdup(const char* str);
#endif
