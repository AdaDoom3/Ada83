#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_failed = 0;
static char test_name[256] = "";

// REPORT.TEST procedure
void REPORT__TEST(char* name_ptr, int name_len, char* desc_ptr, int desc_len) {
    strncpy(test_name, name_ptr, name_len < 255 ? name_len : 255);
    test_name[name_len < 255 ? name_len : 255] = '\0';
    printf("TEST: %s\n", test_name);
    test_failed = 0;
}

// REPORT.FAILED procedure
void REPORT__FAILED(char* msg_ptr, int msg_len) {
    char msg[1024];
    strncpy(msg, msg_ptr, msg_len < 1023 ? msg_len : 1023);
    msg[msg_len < 1023 ? msg_len : 1023] = '\0';
    printf("FAILED: %s\n", msg);
    test_failed = 1;
}

// REPORT.RESULT procedure
void REPORT__RESULT() {
    if (test_failed) {
        printf("**** %s FAILED ****\n", test_name);
        exit(1);
    } else {
        printf("**** %s PASSED ****\n", test_name);
        exit(0);
    }
}

// REPORT.IDENT_INT function
long long REPORT__IDENT_INT(long long val) {
    return val;
}

// REPORT.IDENT_CHAR function
char REPORT__IDENT_CHAR(char val) {
    return val;
}

// REPORT.IDENT_BOOL function
char REPORT__IDENT_BOOL(char val) {
    return val;
}
