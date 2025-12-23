#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Ada boolean type represented as i64 (0 or 1)
typedef int64_t ada_bool;

// Ada string representation: array of i64 where each element is a character
// First element is length, rest are characters
typedef struct {
    int64_t len;
    char data[];
} ada_string;

// Convert Ada enumeration value to string
void* __ada_image_enum(int64_t value, int64_t first, int64_t last) {
    // For now, just return "En" where n is the position
    // This is a simplified implementation
    static char buf[32];
    int64_t pos = value - first;
    snprintf(buf, sizeof(buf), "E%lld", (long long)(pos + 1));

    // Allocate Ada string format (length + chars)
    size_t len = strlen(buf);
    int64_t *result = malloc((len + 1) * sizeof(int64_t));
    result[0] = len;
    for (size_t i = 0; i < len; i++) {
        result[i + 1] = buf[i];
    }
    return result;
}

// Convert string to integer
int64_t __ada_value_int(void *str_ptr) {
    // str_ptr points to Ada string (length, then chars)
    int64_t *str = (int64_t*)str_ptr;
    int64_t len = str[0];

    // Convert to C string
    char *buf = malloc(len + 1);
    for (int64_t i = 0; i < len; i++) {
        buf[i] = (char)str[i + 1];
    }
    buf[len] = '\0';

    int64_t result = strtoll(buf, NULL, 10);
    free(buf);
    return result;
}

// Convert integer to string
void* __ada_image_int(int64_t value) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);

    size_t len = strlen(buf);
    int64_t *result = malloc((len + 1) * sizeof(int64_t));
    result[0] = len;
    for (size_t i = 0; i < len; i++) {
        result[i + 1] = buf[i];
    }
    return result;
}
