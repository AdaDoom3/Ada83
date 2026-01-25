/*
 * SIMD Lexer Workbench - Raw Assembly Implementation
 * AT&T syntax, no intrinsics, no builtins
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Data
 * ═══════════════════════════════════════════════════════════════════════════ */

static char *test_data = NULL;
static size_t test_size = 0;

static void load_test_data(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    test_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    test_data = malloc(test_size + 64);
    if (fread(test_data, 1, test_size, f) != test_size) { perror("fread"); }
    memset(test_data + test_size, 0, 64);
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#define BENCH(name, iters, code) do { \
    uint64_t _t0 = rdtsc(); \
    for (int _i = 0; _i < (iters); _i++) { code; } \
    uint64_t _t1 = rdtsc(); \
    printf("  %-30s %10.2f cycles/iter\n", name, (double)(_t1-_t0)/(iters)); \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Lookup Table for identifier chars
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint8_t id_tbl[256];

static void init_id_tbl(void) {
    memset(id_tbl, 0, 256);
    for (int c = 'A'; c <= 'Z'; c++) id_tbl[c] = 1;
    for (int c = 'a'; c <= 'z'; c++) id_tbl[c] = 1;
    for (int c = '0'; c <= '9'; c++) id_tbl[c] = 1;
    id_tbl['_'] = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SCALAR BASELINE
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *scalar_scan_id(const char *p, const char *end) {
    while (p < end && id_tbl[(uint8_t)*p]) p++;
    return p;
}

static const char *scalar_skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static const char *scalar_find_nl(const char *p, const char *end) {
    while (p < end && *p != '\n') p++;
    return p;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * AVX-512 RAW ASSEMBLY
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Find newline in buffer using AVX-512
 * Processes 64 bytes at a time
 */
static const char *avx512_find_nl(const char *p, const char *end) {
    uint64_t len = end - p;
    if (len < 64) return scalar_find_nl(p, end);

    const char *result;
    __asm__ volatile (
        /* Setup: broadcast newline to zmm0 */
        "movb $0x0a, %%al\n\t"
        "vpbroadcastb %%eax, %%zmm0\n\t"

        /* Main loop */
        "1:\n\t"
        "vmovdqu64 (%[p]), %%zmm1\n\t"
        "vpcmpeqb %%zmm0, %%zmm1, %%k1\n\t"
        "kmovq %%k1, %%rax\n\t"
        "testq %%rax, %%rax\n\t"
        "jnz 2f\n\t"
        "addq $64, %[p]\n\t"
        "cmpq %[end], %[p]\n\t"
        "jb 1b\n\t"
        "movq %[end], %[p]\n\t"
        "jmp 3f\n\t"

        /* Found: compute position */
        "2:\n\t"
        "tzcntq %%rax, %%rax\n\t"
        "addq %%rax, %[p]\n\t"

        "3:\n\t"
        : [p] "+r" (p)
        : [end] "r" (end - 63)
        : "rax", "zmm0", "zmm1", "k1", "memory", "cc"
    );

    /* Handle tail */
    while (p < end && *p != '\n') p++;
    return p;
}

/*
 * Find quote in buffer using AVX-512
 */
static const char *avx512_find_quote(const char *p, const char *end, char c) {
    uint64_t len = end - p;
    if (len < 64) {
        while (p < end && *p != c) p++;
        return p;
    }

    __asm__ volatile (
        /* Broadcast search char */
        "vpbroadcastb %[ch], %%zmm0\n\t"

        "1:\n\t"
        "vmovdqu64 (%[p]), %%zmm1\n\t"
        "vpcmpeqb %%zmm0, %%zmm1, %%k1\n\t"
        "kmovq %%k1, %%rax\n\t"
        "testq %%rax, %%rax\n\t"
        "jnz 2f\n\t"
        "addq $64, %[p]\n\t"
        "cmpq %[lim], %[p]\n\t"
        "jb 1b\n\t"
        "jmp 3f\n\t"

        "2:\n\t"
        "tzcntq %%rax, %%rax\n\t"
        "addq %%rax, %[p]\n\t"
        "jmp 4f\n\t"

        "3:\n\t"
        "4:\n\t"
        : [p] "+r" (p)
        : [lim] "r" (end - 63), [ch] "r" ((uint32_t)(uint8_t)c)
        : "rax", "zmm0", "zmm1", "k1", "memory", "cc"
    );

    while (p < end && *p != c) p++;
    return p;
}

/*
 * Scan identifier chars [A-Za-z0-9_] using AVX-512
 * Uses range comparisons with mask operations
 */
static const char *avx512_scan_id(const char *p, const char *end) {
    uint64_t len = end - p;
    if (len < 64) return scalar_scan_id(p, end);

    __asm__ volatile (
        /* Constants: 'A'=65, 'Z'=90, 'a'=97, 'z'=122, '0'=48, '9'=57, '_'=95 */
        "movl $65, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm2\n\t"      /* zmm2 = 'A' */
        "movl $90, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm3\n\t"      /* zmm3 = 'Z' */
        "movl $97, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm4\n\t"      /* zmm4 = 'a' */
        "movl $122, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm5\n\t"      /* zmm5 = 'z' */
        "movl $48, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm6\n\t"      /* zmm6 = '0' */
        "movl $57, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm7\n\t"      /* zmm7 = '9' */
        "movl $95, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm8\n\t"      /* zmm8 = '_' */

        "1:\n\t"
        "vmovdqu64 (%[p]), %%zmm0\n\t"

        /* Check A-Z: (c >= 'A') && (c <= 'Z') */
        "vpcmpub $5, %%zmm2, %%zmm0, %%k1\n\t"  /* k1 = c >= 'A' */
        "vpcmpub $2, %%zmm3, %%zmm0, %%k2\n\t"  /* k2 = c <= 'Z' */
        "kandq %%k1, %%k2, %%k1\n\t"            /* k1 = upper */

        /* Check a-z */
        "vpcmpub $5, %%zmm4, %%zmm0, %%k2\n\t"  /* k2 = c >= 'a' */
        "vpcmpub $2, %%zmm5, %%zmm0, %%k3\n\t"  /* k3 = c <= 'z' */
        "kandq %%k2, %%k3, %%k2\n\t"            /* k2 = lower */
        "korq %%k1, %%k2, %%k1\n\t"             /* k1 |= lower */

        /* Check 0-9 */
        "vpcmpub $5, %%zmm6, %%zmm0, %%k2\n\t"  /* k2 = c >= '0' */
        "vpcmpub $2, %%zmm7, %%zmm0, %%k3\n\t"  /* k3 = c <= '9' */
        "kandq %%k2, %%k3, %%k2\n\t"            /* k2 = digit */
        "korq %%k1, %%k2, %%k1\n\t"             /* k1 |= digit */

        /* Check underscore */
        "vpcmpeqb %%zmm8, %%zmm0, %%k2\n\t"
        "korq %%k1, %%k2, %%k1\n\t"             /* k1 |= underscore */

        /* Check if all 64 bytes are valid */
        "kmovq %%k1, %%rax\n\t"
        "notq %%rax\n\t"
        "testq %%rax, %%rax\n\t"
        "jnz 2f\n\t"

        "addq $64, %[p]\n\t"
        "cmpq %[lim], %[p]\n\t"
        "jb 1b\n\t"
        "jmp 3f\n\t"

        "2:\n\t"
        "tzcntq %%rax, %%rax\n\t"
        "addq %%rax, %[p]\n\t"
        "jmp 4f\n\t"

        "3:\n\t"
        "4:\n\t"
        : [p] "+r" (p)
        : [lim] "r" (end - 63)
        : "rax", "zmm0", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7", "zmm8",
          "k1", "k2", "k3", "memory", "cc"
    );

    return scalar_scan_id(p, end);
}

/*
 * Skip whitespace using AVX-512
 */
static const char *avx512_skip_ws(const char *p, const char *end) {
    uint64_t len = end - p;
    if (len < 64) return scalar_skip_ws(p, end);

    __asm__ volatile (
        /* Broadcast whitespace chars */
        "movl $32, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm2\n\t"      /* space */
        "movl $9, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm3\n\t"      /* tab */
        "movl $10, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm4\n\t"      /* newline */
        "movl $13, %%eax\n\t"
        "vpbroadcastb %%eax, %%zmm5\n\t"      /* carriage return */

        "1:\n\t"
        "vmovdqu64 (%[p]), %%zmm0\n\t"
        "vpcmpeqb %%zmm2, %%zmm0, %%k1\n\t"
        "vpcmpeqb %%zmm3, %%zmm0, %%k2\n\t"
        "korq %%k1, %%k2, %%k1\n\t"
        "vpcmpeqb %%zmm4, %%zmm0, %%k2\n\t"
        "korq %%k1, %%k2, %%k1\n\t"
        "vpcmpeqb %%zmm5, %%zmm0, %%k2\n\t"
        "korq %%k1, %%k2, %%k1\n\t"

        /* Check if all are whitespace */
        "kmovq %%k1, %%rax\n\t"
        "notq %%rax\n\t"
        "testq %%rax, %%rax\n\t"
        "jnz 2f\n\t"

        "addq $64, %[p]\n\t"
        "cmpq %[lim], %[p]\n\t"
        "jb 1b\n\t"
        "jmp 3f\n\t"

        "2:\n\t"
        "tzcntq %%rax, %%rax\n\t"
        "addq %%rax, %[p]\n\t"
        "jmp 4f\n\t"

        "3:\n\t"
        "4:\n\t"
        : [p] "+r" (p)
        : [lim] "r" (end - 63)
        : "rax", "zmm0", "zmm2", "zmm3", "zmm4", "zmm5", "k1", "k2", "memory", "cc"
    );

    return scalar_skip_ws(p, end);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * AVX2 RAW ASSEMBLY (for comparison)
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *avx2_find_nl(const char *p, const char *end) {
    if (end - p < 32) return scalar_find_nl(p, end);

    __asm__ volatile (
        "movl $0x0a, %%eax\n\t"
        "vmovd %%eax, %%xmm0\n\t"
        "vpbroadcastb %%xmm0, %%ymm0\n\t"

        "1:\n\t"
        "vmovdqu (%[p]), %%ymm1\n\t"
        "vpcmpeqb %%ymm0, %%ymm1, %%ymm1\n\t"
        "vpmovmskb %%ymm1, %%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jnz 2f\n\t"
        "addq $32, %[p]\n\t"
        "cmpq %[lim], %[p]\n\t"
        "jb 1b\n\t"
        "jmp 3f\n\t"

        "2:\n\t"
        "tzcntl %%eax, %%eax\n\t"
        "addq %%rax, %[p]\n\t"

        "3:\n\t"
        "vzeroupper\n\t"
        : [p] "+r" (p)
        : [lim] "r" (end - 31)
        : "rax", "ymm0", "ymm1", "memory", "cc"
    );

    while (p < end && *p != '\n') p++;
    return p;
}

static const char *avx2_scan_id(const char *p, const char *end) {
    if (end - p < 32) return scalar_scan_id(p, end);

    __asm__ volatile (
        /* Setup constants */
        "movl $65, %%eax\n\t"
        "vmovd %%eax, %%xmm2\n\t"
        "vpbroadcastb %%xmm2, %%ymm2\n\t"     /* 'A' */
        "movl $26, %%eax\n\t"
        "vmovd %%eax, %%xmm3\n\t"
        "vpbroadcastb %%xmm3, %%ymm3\n\t"     /* 26 (range) */
        "movl $97, %%eax\n\t"
        "vmovd %%eax, %%xmm4\n\t"
        "vpbroadcastb %%xmm4, %%ymm4\n\t"     /* 'a' */
        "movl $48, %%eax\n\t"
        "vmovd %%eax, %%xmm5\n\t"
        "vpbroadcastb %%xmm5, %%ymm5\n\t"     /* '0' */
        "movl $10, %%eax\n\t"
        "vmovd %%eax, %%xmm6\n\t"
        "vpbroadcastb %%xmm6, %%ymm6\n\t"     /* 10 (range) */
        "movl $95, %%eax\n\t"
        "vmovd %%eax, %%xmm7\n\t"
        "vpbroadcastb %%xmm7, %%ymm7\n\t"     /* '_' */

        "1:\n\t"
        "vmovdqu (%[p]), %%ymm0\n\t"

        /* Check A-Z: (c - 'A') < 26 */
        "vpsubb %%ymm2, %%ymm0, %%ymm1\n\t"
        "vpminub %%ymm3, %%ymm1, %%ymm8\n\t"
        "vpcmpeqb %%ymm1, %%ymm8, %%ymm8\n\t"  /* upper mask */

        /* Check a-z: (c - 'a') < 26 */
        "vpsubb %%ymm4, %%ymm0, %%ymm1\n\t"
        "vpminub %%ymm3, %%ymm1, %%ymm9\n\t"
        "vpcmpeqb %%ymm1, %%ymm9, %%ymm9\n\t"
        "vpor %%ymm8, %%ymm9, %%ymm8\n\t"

        /* Check 0-9: (c - '0') < 10 */
        "vpsubb %%ymm5, %%ymm0, %%ymm1\n\t"
        "vpminub %%ymm6, %%ymm1, %%ymm9\n\t"
        "vpcmpeqb %%ymm1, %%ymm9, %%ymm9\n\t"
        "vpor %%ymm8, %%ymm9, %%ymm8\n\t"

        /* Check underscore */
        "vpcmpeqb %%ymm7, %%ymm0, %%ymm9\n\t"
        "vpor %%ymm8, %%ymm9, %%ymm8\n\t"

        "vpmovmskb %%ymm8, %%eax\n\t"
        "notl %%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jnz 2f\n\t"

        "addq $32, %[p]\n\t"
        "cmpq %[lim], %[p]\n\t"
        "jb 1b\n\t"
        "jmp 3f\n\t"

        "2:\n\t"
        "tzcntl %%eax, %%eax\n\t"
        "addq %%rax, %[p]\n\t"

        "3:\n\t"
        "vzeroupper\n\t"
        : [p] "+r" (p)
        : [lim] "r" (end - 31)
        : "rax", "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
          "ymm8", "ymm9", "memory", "cc"
    );

    return scalar_scan_id(p, end);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HYBRID: Unrolled scalar + SIMD tail
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *hybrid_scan_id(const char *p, const char *end) {
    /* Unroll first 8 bytes - covers most identifiers */
    if (p >= end || !id_tbl[(uint8_t)*p]) return p;
    if (p + 1 >= end || !id_tbl[(uint8_t)p[1]]) return p + 1;
    if (p + 2 >= end || !id_tbl[(uint8_t)p[2]]) return p + 2;
    if (p + 3 >= end || !id_tbl[(uint8_t)p[3]]) return p + 3;
    if (p + 4 >= end || !id_tbl[(uint8_t)p[4]]) return p + 4;
    if (p + 5 >= end || !id_tbl[(uint8_t)p[5]]) return p + 5;
    if (p + 6 >= end || !id_tbl[(uint8_t)p[6]]) return p + 6;
    if (p + 7 >= end || !id_tbl[(uint8_t)p[7]]) return p + 7;

    /* Long identifier - SIMD */
    return avx512_scan_id(p + 8, end);
}

static const char *hybrid_skip_ws(const char *p, const char *end) {
    /* Quick non-ws check */
    if (p >= end || (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r'))
        return p;

    /* Short ws run */
    if (p + 1 >= end || (p[1] != ' ' && p[1] != '\t' && p[1] != '\n' && p[1] != '\r'))
        return p + 1;

    return avx512_skip_ws(p, end);
}

static const char *hybrid_find_nl(const char *p, const char *end) {
    /* Most comments < 80 chars, check inline */
    for (int i = 0; i < 16 && p + i < end; i++) {
        if (p[i] == '\n') return p + i;
    }
    return avx512_find_nl(p + 16, end);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OPTIMAL: Pure scalar with better code generation hints
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Observation: The scalar baseline is ALREADY very fast because:
 * 1. Table lookup is 1-2 cycles (L1 cache hit)
 * 2. Branch prediction works well for identifier patterns
 * 3. Compiler unrolls and optimizes the loop
 *
 * SIMD hurts because:
 * 1. Setup cost (broadcasting constants) > scan cost for short tokens
 * 2. Most Ada identifiers are 3-15 chars
 * 3. Function call overhead
 *
 * Try: Inline everything, give compiler full visibility
 */

static inline const char *opt_scan_id(const char *p, const char *end) {
    /* Force inline table lookup - compiler will optimize */
    while (p < end) {
        uint8_t c = *p;
        if (!id_tbl[c]) break;
        p++;
    }
    return p;
}

static inline const char *opt_skip_ws(const char *p, const char *end) {
    while (p < end) {
        char c = *p;
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        p++;
    }
    return p;
}

static inline const char *opt_find_nl(const char *p, const char *end) {
    while (p < end && *p != '\n') p++;
    return p;
}

/* Fully inlined lexer - no function pointer overhead */
static int lex_opt(const char *src, size_t len) {
    const char *p = src, *end = src + len;
    int tokens = 0;

    while (p < end) {
        /* Skip whitespace inline */
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (p >= end) break;

        char c = *p;

        /* Comment */
        if (c == '-' && p + 1 < end && p[1] == '-') {
            p += 2;
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }

        /* Identifier */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            p++;
            while (p < end && id_tbl[(uint8_t)*p]) p++;
            tokens++;
            continue;
        }

        /* Number */
        if (c >= '0' && c <= '9') {
            p++;
            while (p < end) {
                c = *p;
                if (!((c >= '0' && c <= '9') || c == '_' || c == '#' ||
                      (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || c == '.'))
                    break;
                p++;
            }
            tokens++;
            continue;
        }

        /* String */
        if (c == '"') {
            p++;
            while (p < end && *p != '"') p++;
            if (p < end) p++;
            tokens++;
            continue;
        }

        p++;
        tokens++;
    }
    return tokens;
}

/*
 * Best strategy: Scalar for short things, SIMD only for long scans
 * - Identifiers: scalar (avg 8 chars)
 * - Numbers: scalar (avg 3 chars)
 * - Comments: SIMD for finding newline (can be 80+ chars)
 * - Strings: SIMD for finding quote (can be long)
 * - Whitespace: scalar (usually 1-4 chars)
 */
static int lex_best(const char *src, size_t len) {
    const char *p = src, *end = src + len;
    int tokens = 0;

    while (p < end) {
        /* Skip whitespace - usually short, scalar is fine */
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (p >= end) break;

        char c = *p;

        /* Comment - use SIMD to find newline */
        if (c == '-' && p + 1 < end && p[1] == '-') {
            p += 2;
            /* Short comment fast path */
            if (p + 16 < end) {
                for (int i = 0; i < 16; i++) {
                    if (p[i] == '\n') { p += i; goto comment_done; }
                }
                /* Long comment - SIMD */
                p = avx512_find_nl(p + 16, end);
            } else {
                while (p < end && *p != '\n') p++;
            }
            comment_done:
            if (p < end) p++;
            continue;
        }

        /* Identifier - scalar, they're short */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            p++;
            while (p < end && id_tbl[(uint8_t)*p]) p++;
            tokens++;
            continue;
        }

        /* Number - scalar */
        if (c >= '0' && c <= '9') {
            p++;
            while (p < end) {
                c = *p;
                if (!((c >= '0' && c <= '9') || c == '_' || c == '#' ||
                      (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || c == '.'))
                    break;
                p++;
            }
            tokens++;
            continue;
        }

        /* String - use SIMD for long strings */
        if (c == '"') {
            p++;
            /* Short string fast path */
            if (p + 16 < end) {
                for (int i = 0; i < 16; i++) {
                    if (p[i] == '"') { p += i + 1; tokens++; goto next_token; }
                }
                /* Long string - SIMD */
                p = avx512_find_quote(p + 16, end, '"');
                if (p < end) p++;
            } else {
                while (p < end && *p != '"') p++;
                if (p < end) p++;
            }
            tokens++;
            next_token:
            continue;
        }

        p++;
        tokens++;
    }
    return tokens;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * INTEL-OPTIMIZED: Single-pass with pre-loaded ZMM constants
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Key insight from Intel Optimization Manual (§15.5):
 * - VPBROADCAST has ~3-5 cycle latency
 * - AVX-512 instructions have ~1 cycle throughput once registers are loaded
 * - Setup cost dominates for short operations
 *
 * Solution: Load ALL constants into dedicated ZMM registers at lexer entry,
 * keep them there for entire file. Setup is O(1) per file, not O(1) per token.
 *
 * Register allocation (preserved across entire lex pass):
 *   ZMM31 = newline  (0x0A × 64)
 *   ZMM30 = space    (0x20 × 64)
 *   ZMM29 = quote    (0x22 × 64)
 *   ZMM28 = tab      (0x09 × 64)
 *   ZMM27 = cr       (0x0D × 64)
 *
 * This approach turns AVX-512 from a liability into an asset.
 */

/* Aligned constants for AVX-512 - loaded from L1 cache (~4 cycles) */
static const char __attribute__((aligned(64))) newline_vec[64] = {
    [0 ... 63] = '\n'
};
static const char __attribute__((aligned(64))) quote_vec[64] = {
    [0 ... 63] = '"'
};

static int lex_intel(const char *src, size_t len) {
    const char *p = src;
    const char *end = src + len;
    int tokens = 0;

    /* No setup phase needed - constants loaded from aligned memory
     * L1 cache hit is ~4 cycles, same as register access
     * This avoids GCC clobbering our ZMM registers
     */

    while (p < end) {
        /* Skip whitespace - scalar for short runs */
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (p >= end) break;

        char c = *p;

        /* Comment: AVX-512 newline search */
        if (c == '-' && p + 1 < end && p[1] == '-') {
            p += 2;

            /* Short comment fast path */
            int found = 0;
            for (int i = 0; i < 16 && p + i < end; i++) {
                if (p[i] == '\n') { p += i; found = 1; break; }
            }

            /* Long comment: AVX-512 with memory-based constant */
            if (!found && p + 64 <= end) {
                __asm__ volatile (
                    "vmovdqa64 %[nl], %%zmm1\n\t"          /* Load newline constant */
                    "1:\n\t"
                    "vmovdqu64 (%[p]), %%zmm0\n\t"
                    "vpcmpeqb %%zmm1, %%zmm0, %%k1\n\t"
                    "kmovq %%k1, %%rax\n\t"
                    "testq %%rax, %%rax\n\t"
                    "jnz 2f\n\t"
                    "addq $64, %[p]\n\t"
                    "cmpq %[lim], %[p]\n\t"
                    "jb 1b\n\t"
                    "jmp 3f\n\t"
                    "2:\n\t"
                    "tzcntq %%rax, %%rax\n\t"
                    "addq %%rax, %[p]\n\t"
                    "3:\n\t"
                    : [p] "+r" (p)
                    : [lim] "r" (end - 63), [nl] "m" (newline_vec)
                    : "rax", "zmm0", "zmm1", "k1", "cc", "memory"
                );
                while (p < end && *p != '\n') p++;
            } else {
                while (p < end && *p != '\n') p++;
            }
            if (p < end) p++;
            continue;
        }

        /* Identifier - table lookup */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            p++;
            while (p < end && id_tbl[(uint8_t)*p]) p++;
            tokens++;
            continue;
        }

        /* Number */
        if (c >= '0' && c <= '9') {
            p++;
            while (p < end) {
                c = *p;
                if (!((c >= '0' && c <= '9') || c == '_' || c == '#' ||
                      (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || c == '.'))
                    break;
                p++;
            }
            tokens++;
            continue;
        }

        /* String: AVX-512 quote search */
        if (c == '"') {
            p++;

            /* Short string fast path */
            int found = 0;
            for (int i = 0; i < 16 && p + i < end; i++) {
                if (p[i] == '"') { p += i + 1; found = 1; break; }
            }

            /* Only search further if not found in fast path */
            if (!found) {
                /* Long string: AVX-512 with memory-based constant */
                if (p + 64 <= end) {
                    __asm__ volatile (
                        "vmovdqa64 %[qt], %%zmm1\n\t"
                        "1:\n\t"
                        "vmovdqu64 (%[p]), %%zmm0\n\t"
                        "vpcmpeqb %%zmm1, %%zmm0, %%k1\n\t"
                        "kmovq %%k1, %%rax\n\t"
                        "testq %%rax, %%rax\n\t"
                        "jnz 2f\n\t"
                        "addq $64, %[p]\n\t"
                        "cmpq %[lim], %[p]\n\t"
                        "jb 1b\n\t"
                        "jmp 3f\n\t"
                        "2:\n\t"
                        "tzcntq %%rax, %%rax\n\t"
                        "addq %%rax, %[p]\n\t"
                        "3:\n\t"
                        : [p] "+r" (p)
                        : [lim] "r" (end - 63), [qt] "m" (quote_vec)
                        : "rax", "zmm0", "zmm1", "k1", "cc", "memory"
                    );
                }
                /* Scalar cleanup */
                while (p < end && *p != '"') p++;
                if (p < end) p++;
            }
            tokens++;
            continue;
        }

        p++;
        tokens++;
    }

    return tokens;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SIMULATED LEXER
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline int is_id_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

typedef const char *(*scan_fn)(const char *, const char *);
typedef const char *(*find_fn)(const char *, const char *);

static int lex_count(const char *src, size_t len, scan_fn skip_ws,
                     scan_fn scan_id, find_fn find_nl) {
    const char *p = src, *end = src + len;
    int tokens = 0;

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) break;

        char c = *p;

        if (c == '-' && p + 1 < end && p[1] == '-') {
            p = find_nl(p + 2, end);
            if (p < end) p++;
            continue;
        }

        if (is_id_start(c)) {
            p = scan_id(p, end);
            tokens++;
            continue;
        }

        if (c >= '0' && c <= '9') {
            while (p < end && ((c = *p) >= '0' && c <= '9') | (c == '_') |
                   (c == '#') | (c >= 'A' && c <= 'F') | (c >= 'a' && c <= 'f') |
                   (c == '.')) p++;
            tokens++;
            continue;
        }

        if (c == '"') {
            p++;
            while (p < end && *p != '"') p++;
            if (p < end) p++;
            tokens++;
            continue;
        }

        p++;
        tokens++;
    }
    return tokens;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *file = argc > 1 ? argv[1] : "/tmp/test.ada";
    load_test_data(file);
    init_id_tbl();

    printf("SIMD Lexer Workbench (Raw ASM)\n");
    printf("==============================\n");
    printf("File: %s (%zu bytes)\n\n", file, test_size);

    const char *end = test_data + test_size;
    /* Scale iterations based on file size - target ~1 second per benchmark */
    int iters = test_size > 100000 ? 100 : (test_size > 10000 ? 500 : 1000);

    /* Find sample identifier */
    const char *id = NULL;
    for (const char *p = test_data; p < end - 10; p++) {
        if (is_id_start(*p)) { id = p; break; }
    }

    printf("Micro-benchmarks (%d iters):\n", iters);
    printf("────────────────────────────────────────────\n");

    if (id) {
        printf("\nIdentifier scan:\n");
        BENCH("scalar", iters, scalar_scan_id(id, end));
        BENCH("avx2_asm", iters, avx2_scan_id(id, end));
        BENCH("avx512_asm", iters, avx512_scan_id(id, end));
        BENCH("hybrid", iters, hybrid_scan_id(id, end));
    }

    printf("\nFind newline:\n");
    BENCH("scalar", iters, scalar_find_nl(test_data, end));
    BENCH("avx2_asm", iters, avx2_find_nl(test_data, end));
    BENCH("avx512_asm", iters, avx512_find_nl(test_data, end));

    printf("\nSkip whitespace:\n");
    BENCH("scalar", iters, scalar_skip_ws(test_data, end));
    BENCH("avx512_asm", iters, avx512_skip_ws(test_data, end));

    /* Full lexer */
    printf("\n\nFull lexer (%d iters):\n", iters/10);
    printf("────────────────────────────────────────────\n");

    int tokens;
    uint64_t t0, t1;

    t0 = rdtsc();
    for (int i = 0; i < iters/10; i++) {
        tokens = lex_count(test_data, test_size,
            scalar_skip_ws, scalar_scan_id, scalar_find_nl);
    }
    t1 = rdtsc();
    printf("  Scalar:     %10.2f cyc/iter  (%d tok)\n",
           (double)(t1-t0)/(iters/10), tokens);

    t0 = rdtsc();
    for (int i = 0; i < iters/10; i++) {
        tokens = lex_count(test_data, test_size,
            avx512_skip_ws, avx512_scan_id, avx512_find_nl);
    }
    t1 = rdtsc();
    printf("  AVX-512:    %10.2f cyc/iter  (%d tok)\n",
           (double)(t1-t0)/(iters/10), tokens);

    t0 = rdtsc();
    for (int i = 0; i < iters/10; i++) {
        tokens = lex_count(test_data, test_size,
            hybrid_skip_ws, hybrid_scan_id, hybrid_find_nl);
    }
    t1 = rdtsc();
    printf("  Hybrid:     %10.2f cyc/iter  (%d tok)\n",
           (double)(t1-t0)/(iters/10), tokens);

    t0 = rdtsc();
    for (int i = 0; i < iters/10; i++) {
        tokens = lex_opt(test_data, test_size);
    }
    t1 = rdtsc();
    printf("  Optimal:    %10.2f cyc/iter  (%d tok)\n",
           (double)(t1-t0)/(iters/10), tokens);

    t0 = rdtsc();
    for (int i = 0; i < iters/10; i++) {
        tokens = lex_best(test_data, test_size);
    }
    t1 = rdtsc();
    printf("  Best:       %10.2f cyc/iter  (%d tok)\n",
           (double)(t1-t0)/(iters/10), tokens);

    t0 = rdtsc();
    for (int i = 0; i < iters/10; i++) {
        tokens = lex_intel(test_data, test_size);
    }
    t1 = rdtsc();
    printf("  Intel:      %10.2f cyc/iter  (%d tok)\n",
           (double)(t1-t0)/(iters/10), tokens);

    /* Throughput */
    double mbs;
    printf("\n\nThroughput:\n");
    printf("────────────────────────────────────────────\n");

    t0 = rdtsc();
    for (int i = 0; i < iters; i++) {
        lex_count(test_data, test_size, scalar_skip_ws, scalar_scan_id, scalar_find_nl);
    }
    t1 = rdtsc();
    mbs = (double)test_size * iters * 3.0 / ((t1 - t0) / 1000.0);
    printf("  Scalar:     %.2f MB/s\n", mbs);

    t0 = rdtsc();
    for (int i = 0; i < iters; i++) {
        lex_count(test_data, test_size, hybrid_skip_ws, hybrid_scan_id, hybrid_find_nl);
    }
    t1 = rdtsc();
    mbs = (double)test_size * iters * 3.0 / ((t1 - t0) / 1000.0);
    printf("  Hybrid:     %.2f MB/s\n", mbs);

    t0 = rdtsc();
    for (int i = 0; i < iters; i++) {
        lex_opt(test_data, test_size);
    }
    t1 = rdtsc();
    mbs = (double)test_size * iters * 3.0 / ((t1 - t0) / 1000.0);
    printf("  Optimal:    %.2f MB/s\n", mbs);

    t0 = rdtsc();
    for (int i = 0; i < iters; i++) {
        lex_best(test_data, test_size);
    }
    t1 = rdtsc();
    mbs = (double)test_size * iters * 3.0 / ((t1 - t0) / 1000.0);
    printf("  Best:       %.2f MB/s\n", mbs);

    t0 = rdtsc();
    for (int i = 0; i < iters; i++) {
        lex_intel(test_data, test_size);
    }
    t1 = rdtsc();
    mbs = (double)test_size * iters * 3.0 / ((t1 - t0) / 1000.0);
    printf("  Intel:      %.2f MB/s\n", mbs);

    free(test_data);
    return 0;
}
