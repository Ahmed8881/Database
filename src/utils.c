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
