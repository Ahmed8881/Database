#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Case-insensitive substring search
// Returns a pointer to the first occurrence of the substring needle in haystack,
// or NULL if not found. The search is case-insensitive.
char* strcasestr(const char* haystack, const char* needle) {
    if (!needle[0]) {
        return (char*)haystack;
    }
    
    for (; *haystack; haystack++) {
        if (toupper(*haystack) == toupper(*needle)) {
            const char* h = haystack;
            const char* n = needle;
            
            while (*h && *n && toupper(*h) == toupper(*n)) {
                h++;
                n++;
            }
            
            if (!*n) {
                return (char*)haystack;
            }
        }
    }
    
    return NULL;
}

int count_commas(const char* str, size_t len) {
    int count = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == ',') {
            count++;
        }
    }
    return count;
}

char* my_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* new_str = malloc(len);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}