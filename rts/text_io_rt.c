#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Stub implementations for TEXT_IO runtime

void __text_io_new_line(void) {
    printf("\n");
}

void __text_io_get_char(int64_t c_addr) {
    // c_addr is the address where to store the character
    int ch = getchar();
    *(int64_t*)c_addr = ch;
}

void __text_io_put_char(int64_t c) {
    putchar((int)c);
}

void __text_io_get_line(int64_t str_addr, int64_t last_addr) {
    // Stub - just set last to 0
    *(int64_t*)last_addr = 0;
}

void __text_io_put_line(int64_t str_addr) {
    // str_addr points to Ada string (length, then chars)
    int64_t *str = (int64_t*)str_addr;
    int64_t len = str[0];
    for (int64_t i = 0; i < len; i++) {
        putchar((int)str[i + 1]);
    }
    putchar('\n');
}

void __text_io_put(int64_t str_addr) {
    int64_t *str = (int64_t*)str_addr;
    int64_t len = str[0];
    for (int64_t i = 0; i < len; i++) {
        putchar((int)str[i + 1]);
    }
}
