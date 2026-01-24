/* =============================================================================
 * Ada83 Compiler — A literate implementation targeting LLVM IR
 * =============================================================================
 *
 * Philosophy: High-quality code favors clarity over cleverness, but achieves
 * both through judicious abstraction. We follow Knuth's literate programming:
 * explain the "why" in prose, let the "what" speak through well-named code.
 *
 * Architecture:
 *   Lexer    → Token stream from source text
 *   Parser   → Abstract syntax tree from tokens
 *   Semantic → Type-checked AST with symbol resolution
 *   Codegen  → LLVM IR emission from typed AST
 *
 * Influences: GNAT LLVM's type system, Haskell's functional purity,
 *             Ada's explicit naming, C99's clarity.
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <iso646.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/* Safe ctype — undefined behavior lurks when char is signed */
#define IS_ALPHA(c)  isalpha((unsigned char)(c))
#define IS_ALNUM(c)  isalnum((unsigned char)(c))
#define IS_DIGIT(c)  isdigit((unsigned char)(c))
#define IS_XDIGIT(c) isxdigit((unsigned char)(c))
#define IS_SPACE(c)  isspace((unsigned char)(c))
#define TO_LOWER(c)  ((char)tolower((unsigned char)(c)))
#define TO_UPPER(c)  ((char)toupper((unsigned char)(c)))

/* =============================================================================
 * §1. TYPE METRICS — Measuring the Computational Universe
 * =============================================================================
 *
 * In Ada, every type has a Size (in bits). GNAT LLVM derives all layout from
 * target configuration; we follow that discipline rather than hardcoding.
 *
 * The fundamental unit: BPU (Bits Per Unit) = 8 on byte-addressed machines.
 * All sizes flow through to_bits/to_bytes conversions to maintain consistency.
 */

enum { BITS_PER_UNIT = 8 };

/* LLVM standard type widths — architectural constants */
typedef enum {
    WIDTH_BOOL  = 1,
    WIDTH_I8    = 8,
    WIDTH_I16   = 16,
    WIDTH_I32   = 32,
    WIDTH_I64   = 64,
    WIDTH_I128  = 128,
    WIDTH_F32   = 32,
    WIDTH_F64   = 64,
} Type_Width;

/* Target configuration — derived from datalayout, not assumed */
typedef struct {
    uint32_t pointer_width;      /* bits in a pointer (32 or 64 typically) */
    uint32_t pointer_alignment;  /* alignment requirement for pointers */
    uint32_t max_alignment;      /* maximum useful alignment */
} Target_Config;

static Target_Config target = { .pointer_width = 64, .pointer_alignment = 64, .max_alignment = 128 };

/* Unit conversions — ceiling division for bytes, as GNAT does */
static inline uint64_t to_bits(uint64_t bytes)  { return bytes * BITS_PER_UNIT; }
static inline uint64_t to_bytes(uint64_t bits)  { return (bits + BITS_PER_UNIT - 1) / BITS_PER_UNIT; }
static inline uint64_t align_bits(uint64_t bits) { return to_bits(to_bytes(bits)); }

/* Alignment — round up to multiple of align (must be power of 2) */
static inline uint64_t align_to(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

/* Ada standard integer types — GNAT's conventional mappings */
typedef enum {
    ADA_SHORT_SHORT_INTEGER = WIDTH_I8,
    ADA_SHORT_INTEGER       = WIDTH_I16,
    ADA_INTEGER             = WIDTH_I32,  /* The Standard */
    ADA_LONG_INTEGER        = WIDTH_I64,
    ADA_LONG_LONG_INTEGER   = WIDTH_I64,
} Ada_Standard_Width;

/* LLVM type name from bit width — maps to i1, i8, i16, i32, i64, i128 */
static inline const char *llvm_int_type(uint32_t bits) {
    switch (bits) {
        case 1:   return "i1";
        case 8:   return "i8";
        case 16:  return "i16";
        case 32:  return "i32";
        case 64:  return "i64";
        case 128: return "i128";
        default:  return "i128";  /* rare but legal in LLVM */
    }
}

static inline const char *llvm_float_type(uint32_t bits) {
    return bits == 32 ? "float" : "double";
}

/* Range predicates — safe versions without UB when bits == width */
static inline bool fits_in_signed_width(int64_t value, uint32_t bits) {
    if (bits >= 64) return true;
    int64_t max = (1LL << (bits - 1)) - 1;
    int64_t min = -(1LL << (bits - 1));
    return value >= min and value <= max;
}

static inline bool fits_in_unsigned_width(uint64_t value, uint32_t bits) {
    if (bits >= 64) return true;
    uint64_t max = (1ULL << bits) - 1;
    return value <= max;
}

/* Compute minimum bit width for integer range [low..high] */
static inline uint32_t bits_for_signed_range(int64_t low, int64_t high) {
    /* Try progressively wider types until one fits */
    const uint32_t widths[] = { 8, 16, 32, 64 };
    for (size_t i = 0; i < sizeof(widths)/sizeof(widths[0]); i++)
        if (fits_in_signed_width(low, widths[i]) and fits_in_signed_width(high, widths[i]))
            return widths[i];
    return 64;
}

/* =============================================================================
 * §2. MULTIPRECISION INTEGERS — Only What's Needed
 * =============================================================================
 *
 * For literal scanning beyond int64_t range. Keep it minimal: we need
 * addition, multiplication by small constants, and decimal conversion.
 * All in-place to avoid allocation churn during lexing.
 */

typedef struct {
    uint64_t *digits;    /* little-endian: digits[0] is least significant */
    uint32_t  count;     /* number of valid digits */
    uint32_t  capacity;  /* allocated space */
    bool      negative;  /* sign */
} Bigint;

static Bigint *bigint_new(void) {
    Bigint *b = calloc(1, sizeof(Bigint));
    b->capacity = 4;
    b->digits = calloc(b->capacity, sizeof(uint64_t));
    return b;
}

static void bigint_free(Bigint *b) {
    if (b) { free(b->digits); free(b); }
}

static void bigint_ensure_capacity(Bigint *b, uint32_t needed) {
    if (needed <= b->capacity) return;
    uint32_t new_cap = b->capacity * 2;
    while (new_cap < needed) new_cap *= 2;
    b->digits = realloc(b->digits, new_cap * sizeof(uint64_t));
    memset(b->digits + b->capacity, 0, (new_cap - b->capacity) * sizeof(uint64_t));
    b->capacity = new_cap;
}

/* Remove leading zeros */
static inline void bigint_normalize(Bigint *b) {
    while (b->count > 0 and b->digits[b->count - 1] == 0) b->count--;
    if (b->count == 0) b->negative = false;
}

/* In-place multiply by small integer */
static void bigint_multiply_word(Bigint *b, uint64_t multiplier) {
    if (multiplier == 0) { b->count = 0; b->negative = false; return; }
    if (multiplier == 1) return;

    bigint_ensure_capacity(b, b->count + 1);
    uint64_t carry = 0;

    for (uint32_t i = 0; i < b->count; i++) {
        __uint128_t product = (__uint128_t)b->digits[i] * multiplier + carry;
        b->digits[i] = (uint64_t)product;
        carry = (uint64_t)(product >> 64);
    }

    if (carry) b->digits[b->count++] = carry;
}

/* In-place add small integer */
static void bigint_add_word(Bigint *b, uint64_t addend) {
    if (addend == 0) return;

    bigint_ensure_capacity(b, b->count + 1);
    if (b->count == 0) b->count = 1;

    uint64_t carry = addend;
    for (uint32_t i = 0; i < b->count and carry; i++) {
        __uint128_t sum = (__uint128_t)b->digits[i] + carry;
        b->digits[i] = (uint64_t)sum;
        carry = (uint64_t)(sum >> 64);
    }

    if (carry) b->digits[b->count++] = carry;
}

/* Build from decimal string — the lexer's primary use case */
static Bigint *bigint_from_decimal(const char *str) {
    Bigint *b = bigint_new();

    for (const char *p = str; *p; p++) {
        if (IS_DIGIT(*p)) {
            bigint_multiply_word(b, 10);
            bigint_add_word(b, *p - '0');
        }
        /* Skip underscores per Ada literal rules */
    }

    bigint_normalize(b);
    return b;
}

/* Convert to int64_t if it fits, return success */
static bool bigint_to_int64(const Bigint *b, int64_t *out) {
    if (b->count == 0) { *out = 0; return true; }
    if (b->count > 1) return false;  /* too large */

    uint64_t magnitude = b->digits[0];
    if (b->negative) {
        if (magnitude > (uint64_t)INT64_MAX + 1) return false;
        *out = -(int64_t)magnitude;
    } else {
        if (magnitude > (uint64_t)INT64_MAX) return false;
        *out = (int64_t)magnitude;
    }
    return true;
}

/* =============================================================================
 * §3. MEMORY MANAGEMENT — Arena Allocation with Proper Tracking
 * =============================================================================
 *
 * A compiler's allocations follow a clear pattern: parse → analyze → codegen.
 * Arena allocation fits perfectly: allocate freely during each phase, release
 * all at once when done. Unlike the original's "leak by design", we maintain
 * a chunk list for proper cleanup.
 */

typedef struct Arena_Chunk {
    struct Arena_Chunk *next;
    size_t capacity;
    size_t used;
    char   data[];  /* flexible array member */
} Arena_Chunk;

typedef struct {
    Arena_Chunk *current;
    Arena_Chunk *first;     /* for iteration and cleanup */
    size_t default_chunk_size;
} Arena;

static Arena main_arena = { .default_chunk_size = 1024 * 1024 };  /* 1MB chunks */

static Arena_Chunk *arena_new_chunk(size_t size) {
    size_t alloc_size = sizeof(Arena_Chunk) + size;
    Arena_Chunk *chunk = malloc(alloc_size);
    chunk->next = NULL;
    chunk->capacity = size;
    chunk->used = 0;
    return chunk;
}

static void *arena_alloc(Arena *a, size_t size) {
    /* Align all allocations to pointer size for safety */
    size = align_to(size, sizeof(void *));

    /* Need a new chunk? */
    if (not a->current or a->current->used + size > a->current->capacity) {
        size_t chunk_size = size > a->default_chunk_size ? size : a->default_chunk_size;
        Arena_Chunk *chunk = arena_new_chunk(chunk_size);

        if (not a->first) a->first = chunk;
        if (a->current) a->current->next = chunk;
        a->current = chunk;
    }

    void *ptr = a->current->data + a->current->used;
    a->current->used += size;
    return ptr;
}

static void arena_free_all(Arena *a) {
    Arena_Chunk *chunk = a->first;
    while (chunk) {
        Arena_Chunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    a->current = a->first = NULL;
}

/* Convenient wrappers */
#define ALLOC(type)       ((type *)arena_alloc(&main_arena, sizeof(type)))
#define ALLOC_ARRAY(type, n) ((type *)arena_alloc(&main_arena, sizeof(type) * (n)))

static char *string_duplicate(const char *str) {
    size_t len = strlen(str);
    char *copy = arena_alloc(&main_arena, len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}

/* =============================================================================
 * §4. STRING UTILITIES — Functional Style
 * =============================================================================
 *
 * Avoid the original's static ring buffer hack. Pure functions that work with
 * arena allocation are cleaner and safer.
 */

/* String slices — point into existing memory, no allocation */
typedef struct {
    const char *start;
    size_t length;
} String_Slice;

#define SLICE_FMT "%.*s"
#define SLICE_ARG(s) ((int)(s).length), ((s).start)

static inline String_Slice make_slice(const char *str) {
    return (String_Slice){ .start = str, .length = strlen(str) };
}

static inline String_Slice slice_range(const char *start, const char *end) {
    return (String_Slice){ .start = start, .length = end - start };
}

/* Case-insensitive comparison — Ada is case-insensitive */
static bool slice_equal_ignore_case(String_Slice a, String_Slice b) {
    if (a.length != b.length) return false;
    for (size_t i = 0; i < a.length; i++)
        if (TO_LOWER(a.start[i]) != TO_LOWER(b.start[i]))
            return false;
    return true;
}

/* FNV-1a hash for symbol table */
static uint32_t slice_hash_ignore_case(String_Slice s) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < s.length; i++) {
        hash ^= TO_LOWER(s.start[i]);
        hash *= 16777619u;
    }
    return hash;
}

/* Allocate lowercase copy for display */
static char *slice_to_lowercase(String_Slice s) {
    char *result = ALLOC_ARRAY(char, s.length + 1);
    for (size_t i = 0; i < s.length; i++)
        result[i] = TO_LOWER(s.start[i]);
    result[s.length] = '\0';
    return result;
}

/* Edit distance for "did you mean" suggestions */
static int edit_distance(String_Slice a, String_Slice b) {
    if (a.length > b.length) { String_Slice tmp = a; a = b; b = tmp; }

    int *prev = ALLOC_ARRAY(int, a.length + 1);
    int *curr = ALLOC_ARRAY(int, a.length + 1);

    for (size_t i = 0; i <= a.length; i++) prev[i] = i;

    for (size_t j = 1; j <= b.length; j++) {
        curr[0] = j;
        for (size_t i = 1; i <= a.length; i++) {
            int cost = (TO_LOWER(a.start[i-1]) == TO_LOWER(b.start[j-1])) ? 0 : 1;
            curr[i] = curr[i-1] + 1;  /* insertion */
            if (curr[i] > prev[i] + 1) curr[i] = prev[i] + 1;  /* deletion */
            if (curr[i] > prev[i-1] + cost) curr[i] = prev[i-1] + cost;  /* substitution */
        }
        int *tmp = prev; prev = curr; curr = tmp;
    }

    return prev[a.length];
}

/* =============================================================================
 * §5. ERROR REPORTING — Clarity Above All
 * =============================================================================
 *
 * Good error messages are the compiler's user interface. We follow GCC/Clang's
 * lead: filename:line:col: severity: message
 */

typedef struct {
    const char *filename;
    uint32_t line;
    uint32_t column;
} Source_Location;

static int error_count = 0;
static const int MAX_ERRORS = 50;

static void report_error(Source_Location loc, const char *fmt, ...) {
    fprintf(stderr, "%s:%u:%u: error: ", loc.filename, loc.line, loc.column);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    error_count++;
}

static void fatal_error(const char *fmt, ...) {
    fprintf(stderr, "fatal error: ");

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

/* =============================================================================
 * §6. LEXICAL ANALYSIS — From Characters to Tokens
 * =============================================================================
 *
 * Ada's lexical structure is straightforward: case-insensitive keywords,
 * underscore-separated numeric literals, character and string literals with
 * doubling for quotes, -- comments.
 */

typedef enum {
    /* Literals */
    TOK_IDENTIFIER,
    TOK_INTEGER_LITERAL,
    TOK_REAL_LITERAL,
    TOK_CHARACTER_LITERAL,
    TOK_STRING_LITERAL,

    /* Keywords (alphabetical order) */
    TOK_ABORT, TOK_ABS, TOK_ACCEPT, TOK_ACCESS, TOK_ALL, TOK_AND,
    TOK_ARRAY, TOK_AT, TOK_BEGIN, TOK_BODY, TOK_CASE, TOK_CONSTANT,
    TOK_DECLARE, TOK_DELAY, TOK_DELTA, TOK_DIGITS, TOK_DO, TOK_ELSE,
    TOK_ELSIF, TOK_END, TOK_ENTRY, TOK_EXCEPTION, TOK_EXIT, TOK_FOR,
    TOK_FUNCTION, TOK_GENERIC, TOK_GOTO, TOK_IF, TOK_IN, TOK_IS,
    TOK_LIMITED, TOK_LOOP, TOK_MOD, TOK_NEW, TOK_NOT, TOK_NULL,
    TOK_OF, TOK_OR, TOK_OTHERS, TOK_OUT, TOK_PACKAGE, TOK_PRAGMA,
    TOK_PRIVATE, TOK_PROCEDURE, TOK_RAISE, TOK_RANGE, TOK_RECORD,
    TOK_REM, TOK_RENAMES, TOK_RETURN, TOK_REVERSE, TOK_SELECT,
    TOK_SEPARATE, TOK_SUBTYPE, TOK_TASK, TOK_TERMINATE, TOK_THEN,
    TOK_TYPE, TOK_USE, TOK_WHEN, TOK_WHILE, TOK_WITH, TOK_XOR,

    /* Operators and delimiters */
    TOK_AMPERSAND,      /* & */
    TOK_APOSTROPHE,     /* ' */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_STAR,           /* * */
    TOK_PLUS,           /* + */
    TOK_COMMA,          /* , */
    TOK_MINUS,          /* - */
    TOK_DOT,            /* . */
    TOK_SLASH,          /* / */
    TOK_COLON,          /* : */
    TOK_SEMICOLON,      /* ; */
    TOK_LESS,           /* < */
    TOK_EQUAL,          /* = */
    TOK_GREATER,        /* > */
    TOK_PIPE,           /* | */

    TOK_ARROW,          /* => */
    TOK_DOT_DOT,        /* .. */
    TOK_COLON_EQUAL,    /* := */
    TOK_NOT_EQUAL,      /* /= */
    TOK_GREATER_EQUAL,  /* >= */
    TOK_LESS_EQUAL,     /* <= */
    TOK_DOUBLE_STAR,    /* ** */
    TOK_BOX,            /* <> */
    TOK_LSHIFT,         /* << */
    TOK_RSHIFT,         /* >> */

    TOK_EOF,
    TOK_ERROR,
} Token_Kind;

typedef struct {
    Token_Kind kind;
    Source_Location location;
    String_Slice text;

    union {
        int64_t  integer_value;
        double   real_value;
        char     character_value;
        Bigint  *bigint_value;  /* for integer literals > int64 */
    };
} Token;

typedef struct {
    const char *source;
    const char *current;
    const char *filename;
    uint32_t line;
    uint32_t column;
    Token lookahead;
    bool has_lookahead;
} Lexer;

/* Keyword table — sorted for binary search */
static const struct { const char *name; Token_Kind kind; } keywords[] = {
    {"abort",     TOK_ABORT},     {"abs",       TOK_ABS},
    {"accept",    TOK_ACCEPT},    {"access",    TOK_ACCESS},
    {"all",       TOK_ALL},       {"and",       TOK_AND},
    {"array",     TOK_ARRAY},     {"at",        TOK_AT},
    {"begin",     TOK_BEGIN},     {"body",      TOK_BODY},
    {"case",      TOK_CASE},      {"constant",  TOK_CONSTANT},
    {"declare",   TOK_DECLARE},   {"delay",     TOK_DELAY},
    {"delta",     TOK_DELTA},     {"digits",    TOK_DIGITS},
    {"do",        TOK_DO},        {"else",      TOK_ELSE},
    {"elsif",     TOK_ELSIF},     {"end",       TOK_END},
    {"entry",     TOK_ENTRY},     {"exception", TOK_EXCEPTION},
    {"exit",      TOK_EXIT},      {"for",       TOK_FOR},
    {"function",  TOK_FUNCTION},  {"generic",   TOK_GENERIC},
    {"goto",      TOK_GOTO},      {"if",        TOK_IF},
    {"in",        TOK_IN},        {"is",        TOK_IS},
    {"limited",   TOK_LIMITED},   {"loop",      TOK_LOOP},
    {"mod",       TOK_MOD},       {"new",       TOK_NEW},
    {"not",       TOK_NOT},       {"null",      TOK_NULL},
    {"of",        TOK_OF},        {"or",        TOK_OR},
    {"others",    TOK_OTHERS},    {"out",       TOK_OUT},
    {"package",   TOK_PACKAGE},   {"pragma",    TOK_PRAGMA},
    {"private",   TOK_PRIVATE},   {"procedure", TOK_PROCEDURE},
    {"raise",     TOK_RAISE},     {"range",     TOK_RANGE},
    {"record",    TOK_RECORD},    {"rem",       TOK_REM},
    {"renames",   TOK_RENAMES},   {"return",    TOK_RETURN},
    {"reverse",   TOK_REVERSE},   {"select",    TOK_SELECT},
    {"separate",  TOK_SEPARATE},  {"subtype",   TOK_SUBTYPE},
    {"task",      TOK_TASK},      {"terminate", TOK_TERMINATE},
    {"then",      TOK_THEN},      {"type",      TOK_TYPE},
    {"use",       TOK_USE},       {"when",      TOK_WHEN},
    {"while",     TOK_WHILE},     {"with",      TOK_WITH},
    {"xor",       TOK_XOR},
};

static const int keyword_count = sizeof(keywords) / sizeof(keywords[0]);

/* Binary search for keyword — input must be lowercase */
static Token_Kind keyword_lookup(const char *str, size_t len) {
    char buffer[64];
    if (len >= sizeof(buffer)) return TOK_IDENTIFIER;

    for (size_t i = 0; i < len; i++) buffer[i] = TO_LOWER(str[i]);
    buffer[len] = '\0';

    int lo = 0, hi = keyword_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = strcmp(buffer, keywords[mid].name);
        if (cmp == 0) return keywords[mid].kind;
        if (cmp < 0) hi = mid - 1;
        else lo = mid + 1;
    }

    return TOK_IDENTIFIER;
}

static Lexer *lexer_new(const char *filename, const char *source) {
    Lexer *lex = ALLOC(Lexer);
    lex->source = lex->current = source;
    lex->filename = filename;
    lex->line = 1;
    lex->column = 1;
    lex->has_lookahead = false;
    return lex;
}

static inline char peek(const Lexer *lex) {
    return *lex->current;
}

static inline char advance(Lexer *lex) {
    char c = *lex->current++;
    if (c == '\n') { lex->line++; lex->column = 1; }
    else lex->column++;
    return c;
}

/* Skip whitespace and -- comments */
static void skip_trivia(Lexer *lex) {
    for (;;) {
        while (IS_SPACE(peek(lex))) advance(lex);

        /* Ada comment: -- to end of line */
        if (peek(lex) == '-' and lex->current[1] == '-') {
            while (peek(lex) and peek(lex) != '\n') advance(lex);
            continue;
        }

        break;
    }
}

static Token make_token(Lexer *lex, Token_Kind kind, const char *start) {
    return (Token){
        .kind = kind,
        .location = { lex->filename, lex->line, lex->column },
        .text = slice_range(start, lex->current)
    };
}

/* Scan identifier or keyword */
static Token scan_identifier(Lexer *lex) {
    const char *start = lex->current;

    while (IS_ALNUM(peek(lex)) or peek(lex) == '_')
        advance(lex);

    Token tok = make_token(lex, TOK_IDENTIFIER, start);
    tok.kind = keyword_lookup(tok.text.start, tok.text.length);
    return tok;
}

/* Helper: digit value in given base */
static inline int digit_value(char c, int base) {
    int val = IS_DIGIT(c) ? c - '0' : 10 + TO_LOWER(c) - 'a';
    return val < base ? val : -1;
}

/* Scan numeric literal — integers and reals */
static Token scan_number(Lexer *lex) {
    const char *start = lex->current;
    int base = 10;

    /* Collect integer part */
    while (IS_DIGIT(peek(lex)) or peek(lex) == '_') {
        if (peek(lex) != '_') advance(lex);
        else advance(lex);  /* skip underscore */
    }

    /* Based literal? */
    if (peek(lex) == '#') {
        /* Extract base from what we've scanned */
        char base_str[16];
        size_t base_len = lex->current - start;
        if (base_len >= sizeof(base_str)) goto real_literal;

        memcpy(base_str, start, base_len);
        base_str[base_len] = '\0';
        base = atoi(base_str);
        if (base < 2 or base > 16) {
            return make_token(lex, TOK_ERROR, start);
        }

        advance(lex);  /* skip # */

        /* Scan based digits */
        while (digit_value(peek(lex), base) >= 0 or peek(lex) == '_') {
            advance(lex);
        }

        if (peek(lex) != '#') return make_token(lex, TOK_ERROR, start);
        advance(lex);  /* skip closing # */

        /* Based literals with exponent are real */
        if (TO_LOWER(peek(lex)) == 'e') goto real_literal;

        Token tok = make_token(lex, TOK_INTEGER_LITERAL, start);
        /* TODO: convert based literal to integer */
        return tok;
    }

    /* Real literal? */
    if (peek(lex) == '.') {
        /* Careful: .. is range operator, not real literal */
        if (lex->current[1] == '.') goto integer_literal;

        real_literal:
        advance(lex);  /* skip . */

        while (IS_DIGIT(peek(lex)) or peek(lex) == '_')
            advance(lex);

        /* Exponent? */
        if (TO_LOWER(peek(lex)) == 'e') {
            advance(lex);
            if (peek(lex) == '+' or peek(lex) == '-') advance(lex);
            while (IS_DIGIT(peek(lex)) or peek(lex) == '_')
                advance(lex);
        }

        Token tok = make_token(lex, TOK_REAL_LITERAL, start);

        /* Parse as double */
        char buffer[512];
        size_t len = lex->current - start;
        if (len >= sizeof(buffer)) return make_token(lex, TOK_ERROR, start);

        /* Copy without underscores */
        size_t j = 0;
        for (size_t i = 0; i < len; i++)
            if (start[i] != '_') buffer[j++] = start[i];
        buffer[j] = '\0';

        tok.real_value = strtod(buffer, NULL);
        return tok;
    }

    integer_literal:
    /* Exponent on integer makes it real */
    if (TO_LOWER(peek(lex)) == 'e') goto real_literal;

    Token tok = make_token(lex, TOK_INTEGER_LITERAL, start);

    /* Try to fit in int64 */
    char buffer[512];
    size_t len = lex->current - start;
    if (len >= sizeof(buffer)) {
        /* Use bigint */
        tok.bigint_value = bigint_from_decimal(start);
        return tok;
    }

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
        if (start[i] != '_') buffer[j++] = start[i];
    buffer[j] = '\0';

    errno = 0;
    tok.integer_value = strtoll(buffer, NULL, base);
    if (errno == ERANGE) {
        tok.bigint_value = bigint_from_decimal(start);
    }

    return tok;
}

/* Scan character literal */
static Token scan_character_literal(Lexer *lex) {
    const char *start = lex->current;
    advance(lex);  /* skip opening ' */

    if (peek(lex) == '\0') return make_token(lex, TOK_ERROR, start);

    char value = advance(lex);

    if (peek(lex) != '\'') return make_token(lex, TOK_ERROR, start);
    advance(lex);  /* skip closing ' */

    Token tok = make_token(lex, TOK_CHARACTER_LITERAL, start);
    tok.character_value = value;
    return tok;
}

/* Scan string literal */
static Token scan_string_literal(Lexer *lex) {
    const char *start = lex->current;
    advance(lex);  /* skip opening " */

    /* Build string in arena */
    char *buffer = ALLOC_ARRAY(char, 256);
    size_t capacity = 256, length = 0;

    while (peek(lex) != '"') {
        if (peek(lex) == '\0') return make_token(lex, TOK_ERROR, start);

        if (length >= capacity) {
            size_t new_cap = capacity * 2;
            char *new_buf = ALLOC_ARRAY(char, new_cap);
            memcpy(new_buf, buffer, length);
            buffer = new_buf;
            capacity = new_cap;
        }

        char c = advance(lex);

        /* Doubled quote becomes single quote */
        if (c == '"' and peek(lex) == '"') {
            advance(lex);
            buffer[length++] = '"';
        } else {
            buffer[length++] = c;
        }
    }

    advance(lex);  /* skip closing " */
    buffer[length] = '\0';

    Token tok = make_token(lex, TOK_STRING_LITERAL, start);
    tok.text = (String_Slice){ buffer, length };
    return tok;
}

/* Main tokenization routine */
static Token lexer_next_token(Lexer *lex) {
    if (lex->has_lookahead) {
        lex->has_lookahead = false;
        return lex->lookahead;
    }

    skip_trivia(lex);

    const char *start = lex->current;
    char c = peek(lex);

    if (c == '\0') return make_token(lex, TOK_EOF, start);

    /* Identifier or keyword */
    if (IS_ALPHA(c)) return scan_identifier(lex);

    /* Numeric literal */
    if (IS_DIGIT(c)) return scan_number(lex);

    /* Character literal */
    if (c == '\'' and IS_ALNUM(lex->current[1]) and lex->current[2] == '\'')
        return scan_character_literal(lex);

    /* String literal */
    if (c == '"') return scan_string_literal(lex);

    /* Multi-character operators */
    advance(lex);

    switch (c) {
        case '&': return make_token(lex, TOK_AMPERSAND, start);
        case '\'': return make_token(lex, TOK_APOSTROPHE, start);
        case '(': return make_token(lex, TOK_LPAREN, start);
        case ')': return make_token(lex, TOK_RPAREN, start);
        case '*':
            if (peek(lex) == '*') { advance(lex); return make_token(lex, TOK_DOUBLE_STAR, start); }
            return make_token(lex, TOK_STAR, start);
        case '+': return make_token(lex, TOK_PLUS, start);
        case ',': return make_token(lex, TOK_COMMA, start);
        case '-': return make_token(lex, TOK_MINUS, start);
        case '.':
            if (peek(lex) == '.') { advance(lex); return make_token(lex, TOK_DOT_DOT, start); }
            return make_token(lex, TOK_DOT, start);
        case '/':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_NOT_EQUAL, start); }
            return make_token(lex, TOK_SLASH, start);
        case ':':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_COLON_EQUAL, start); }
            return make_token(lex, TOK_COLON, start);
        case ';': return make_token(lex, TOK_SEMICOLON, start);
        case '<':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_LESS_EQUAL, start); }
            if (peek(lex) == '>') { advance(lex); return make_token(lex, TOK_BOX, start); }
            if (peek(lex) == '<') { advance(lex); return make_token(lex, TOK_LSHIFT, start); }
            return make_token(lex, TOK_LESS, start);
        case '=':
            if (peek(lex) == '>') { advance(lex); return make_token(lex, TOK_ARROW, start); }
            return make_token(lex, TOK_EQUAL, start);
        case '>':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_GREATER_EQUAL, start); }
            if (peek(lex) == '>') { advance(lex); return make_token(lex, TOK_RSHIFT, start); }
            return make_token(lex, TOK_GREATER, start);
        case '|': return make_token(lex, TOK_PIPE, start);

        default: return make_token(lex, TOK_ERROR, start);
    }
}

static Token lexer_peek_token(Lexer *lex) {
    if (not lex->has_lookahead) {
        lex->lookahead = lexer_next_token(lex);
        lex->has_lookahead = true;
    }
    return lex->lookahead;
}

/* =============================================================================
 * §7. ABSTRACT SYNTAX TREE — The Structure of Programs
 * =============================================================================
 */

typedef enum {
    /* Literals and names */
    NODE_IDENTIFIER,
    NODE_INTEGER_LITERAL,
    NODE_REAL_LITERAL,
    NODE_CHARACTER_LITERAL,
    NODE_STRING_LITERAL,
    NODE_NULL_LITERAL,

    /* Expressions */
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_FUNCTION_CALL,
    NODE_INDEXED_COMPONENT,
    NODE_SELECTED_COMPONENT,
    NODE_SLICE,
    NODE_ATTRIBUTE,
    NODE_QUALIFIED_EXPRESSION,
    NODE_ALLOCATOR,
    NODE_AGGREGATE,

    /* Statements */
    NODE_ASSIGNMENT,
    NODE_IF_STATEMENT,
    NODE_CASE_STATEMENT,
    NODE_LOOP_STATEMENT,
    NODE_WHILE_LOOP,
    NODE_FOR_LOOP,
    NODE_BLOCK_STATEMENT,
    NODE_EXIT_STATEMENT,
    NODE_RETURN_STATEMENT,
    NODE_GOTO_STATEMENT,
    NODE_PROCEDURE_CALL,
    NODE_RAISE_STATEMENT,
    NODE_NULL_STATEMENT,

    /* Declarations */
    NODE_OBJECT_DECLARATION,
    NODE_NUMBER_DECLARATION,
    NODE_TYPE_DECLARATION,
    NODE_SUBTYPE_DECLARATION,
    NODE_SUBPROGRAM_DECLARATION,
    NODE_SUBPROGRAM_BODY,
    NODE_PACKAGE_DECLARATION,
    NODE_PACKAGE_BODY,
    NODE_PARAMETER_DECLARATION,
    NODE_EXCEPTION_DECLARATION,
    NODE_GENERIC_DECLARATION,
    NODE_USE_CLAUSE,
    NODE_RENAME_DECLARATION,

    /* Type definitions */
    NODE_ENUMERATION_TYPE,
    NODE_INTEGER_TYPE,
    NODE_REAL_TYPE,
    NODE_ARRAY_TYPE,
    NODE_RECORD_TYPE,
    NODE_ACCESS_TYPE,
    NODE_DERIVED_TYPE,
    NODE_PRIVATE_TYPE,

    /* Other */
    NODE_COMPILATION_UNIT,
    NODE_WITH_CLAUSE,
    NODE_RANGE,
    NODE_CONSTRAINT,
} Node_Kind;

/* Forward declarations */
typedef struct Type_Info Type_Info;
typedef struct Symbol Symbol;
typedef struct AST_Node AST_Node;

struct AST_Node {
    Node_Kind kind;
    Source_Location location;
    Type_Info *type;
    Symbol *symbol;

    union {
        /* Literals */
        struct { int64_t value; Bigint *big_value; } integer_literal;
        struct { double value; } real_literal;
        struct { char value; } character_literal;
        struct { const char *value; } string_literal;

        /* Binary operation */
        struct {
            Token_Kind operator;
            AST_Node *left;
            AST_Node *right;
        } binary_op;

        /* Unary operation */
        struct {
            Token_Kind operator;
            AST_Node *operand;
        } unary_op;

        /* Function call or indexed component */
        struct {
            AST_Node *function;
            AST_Node **arguments;
            int argument_count;
        } call;

        /* Selected component: record.field */
        struct {
            AST_Node *object;
            const char *field_name;
        } selected;

        /* Attribute: X'Attribute */
        struct {
            AST_Node *prefix;
            const char *attribute_name;
            AST_Node *argument;  /* optional */
        } attribute;

        /* Aggregate: (component => value, ...) */
        struct {
            AST_Node **components;
            int component_count;
        } aggregate;

        /* Assignment: target := value */
        struct {
            AST_Node *target;
            AST_Node *value;
        } assignment;

        /* If statement */
        struct {
            AST_Node *condition;
            AST_Node **then_statements;
            int then_count;
            AST_Node **elsif_parts;
            int elsif_count;
            AST_Node **else_statements;
            int else_count;
        } if_stmt;

        /* Loop statement */
        struct {
            AST_Node *iteration_scheme;  /* while/for, or NULL for infinite */
            AST_Node **body;
            int body_count;
            const char *label;
        } loop_stmt;

        /* For loop iteration */
        struct {
            const char *iterator_name;
            bool reverse;
            AST_Node *range;
        } for_iteration;

        /* Declaration */
        struct {
            const char *name;
            AST_Node *type_spec;
            AST_Node *initializer;
            bool is_constant;
        } declaration;

        /* Subprogram */
        struct {
            const char *name;
            AST_Node **parameters;
            int parameter_count;
            AST_Node *return_type;  /* NULL for procedure */
            AST_Node **body;
            int body_count;
        } subprogram;

        /* Package */
        struct {
            const char *name;
            AST_Node **declarations;
            int declaration_count;
        } package;

        /* Compilation unit */
        struct {
            AST_Node **context_clauses;
            int context_clause_count;
            AST_Node *unit;
        } compilation_unit;
    };
};

/* Growable vector for AST nodes — simple stretchy buffer */
typedef struct {
    AST_Node **items;
    int count;
    int capacity;
} AST_Vector;

static void ast_vector_push(AST_Vector *vec, AST_Node *node) {
    if (vec->count >= vec->capacity) {
        int new_cap = vec->capacity == 0 ? 8 : vec->capacity * 2;
        AST_Node **new_items = ALLOC_ARRAY(AST_Node *, new_cap);
        if (vec->items) memcpy(new_items, vec->items, vec->count * sizeof(AST_Node *));
        vec->items = new_items;
        vec->capacity = new_cap;
    }
    vec->items[vec->count++] = node;
}

/* =============================================================================
 * §8. TYPE SYSTEM — Ada's Rich Type Structure
 * =============================================================================
 *
 * Ada 83 has one of the richest type systems in imperative languages:
 * - Scalar types (integer, real, enumeration)
 * - Composite types (arrays, records with discriminants)
 * - Access types (pointers with accessibility rules)
 * - Derived types and subtypes with constraints
 *
 * Type_Info captures all this structure for semantic analysis and codegen.
 */

typedef enum {
    TYPE_INVALID,

    /* Scalar types */
    TYPE_INTEGER,
    TYPE_REAL,
    TYPE_ENUMERATION,
    TYPE_BOOLEAN,

    /* Composite types */
    TYPE_ARRAY,
    TYPE_RECORD,
    TYPE_STRING,

    /* Access types */
    TYPE_ACCESS,

    /* Special types */
    TYPE_SUBTYPE,
    TYPE_DERIVED,
    TYPE_INCOMPLETE,
    TYPE_PRIVATE,
} Type_Kind;

typedef struct Type_Info {
    Type_Kind kind;
    String_Slice name;

    /* Type relationships */
    struct Type_Info *base_type;      /* for subtypes */
    struct Type_Info *element_type;   /* for arrays and access types */
    struct Type_Info *index_type;     /* for arrays */
    struct Type_Info *parent_type;    /* for derived types */

    /* Constraints */
    int64_t low_bound;
    int64_t high_bound;
    bool has_constraint;

    /* Layout information — sizes in BITS following GNAT */
    uint32_t size;
    uint32_t alignment;

    /* Record components */
    AST_Vector components;

    /* Enumeration literals */
    struct Symbol **enum_literals;
    int enum_literal_count;

    /* Compile-time flags */
    bool is_constrained;
    bool is_anonymous;
} Type_Info;

/* Standard type predeclared instances */
static Type_Info *type_integer;
static Type_Info *type_boolean;
static Type_Info *type_character;
static Type_Info *type_string;
static Type_Info *type_float;

static Type_Info *type_info_new(Type_Kind kind) {
    Type_Info *ty = ALLOC(Type_Info);
    ty->kind = kind;
    ty->alignment = BITS_PER_UNIT;  /* default byte alignment */
    return ty;
}

/* Check if two types are the same (identity, not equivalence) */
static bool types_same(const Type_Info *a, const Type_Info *b) {
    if (a == b) return true;
    if (not a or not b) return false;

    /* Subtypes refer to same base type */
    if (a->kind == TYPE_SUBTYPE) return types_same(a->base_type, b);
    if (b->kind == TYPE_SUBTYPE) return types_same(a, b->base_type);

    return false;
}

/* Check if value is in type's range */
static bool in_range(const Type_Info *ty, int64_t value) {
    if (not ty->has_constraint) return true;
    return value >= ty->low_bound and value <= ty->high_bound;
}

/* =============================================================================
 * §9. SYMBOL TABLE — Scoped Name Resolution
 * =============================================================================
 *
 * Ada's visibility rules require careful scope management:
 * - Block scopes (procedures, functions, blocks)
 * - Package scopes (with private parts)
 * - Use clauses (make names directly visible)
 * - Overloading (same name, different signatures)
 */

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_TYPE,
    SYMBOL_SUBPROGRAM,
    SYMBOL_PACKAGE,
    SYMBOL_EXCEPTION,
    SYMBOL_PARAMETER,
    SYMBOL_ENUM_LITERAL,
} Symbol_Kind;

typedef struct Symbol {
    String_Slice name;
    Symbol_Kind kind;
    Type_Info *type;
    AST_Node *declaration;

    /* Scope information */
    int scope_level;
    struct Symbol *next_in_scope;  /* for hash chain */

    /* Overloading */
    struct Symbol **overloads;
    int overload_count;

    /* For subprograms */
    AST_Node **parameters;
    int parameter_count;

    /* For packages */
    struct Symbol_Table *package_scope;

    /* Unique ID for code generation */
    uint32_t uid;
} Symbol;

#define SYMBOL_TABLE_SIZE 4096

typedef struct Symbol_Table {
    Symbol *buckets[SYMBOL_TABLE_SIZE];
    struct Symbol_Table *parent;
    int scope_level;
} Symbol_Table;

static uint32_t next_symbol_uid = 0;

static Symbol_Table *symbol_table_new(Symbol_Table *parent) {
    Symbol_Table *table = ALLOC(Symbol_Table);
    table->parent = parent;
    table->scope_level = parent ? parent->scope_level + 1 : 0;
    return table;
}

static Symbol *symbol_new(String_Slice name, Symbol_Kind kind) {
    Symbol *sym = ALLOC(Symbol);
    sym->name = name;
    sym->kind = kind;
    sym->uid = next_symbol_uid++;
    return sym;
}

static void symbol_table_insert(Symbol_Table *table, Symbol *sym) {
    uint32_t hash = slice_hash_ignore_case(sym->name) % SYMBOL_TABLE_SIZE;
    sym->next_in_scope = table->buckets[hash];
    sym->scope_level = table->scope_level;
    table->buckets[hash] = sym;
}

/* Lookup in current scope only */
static Symbol *symbol_table_lookup_local(Symbol_Table *table, String_Slice name) {
    uint32_t hash = slice_hash_ignore_case(name) % SYMBOL_TABLE_SIZE;

    for (Symbol *sym = table->buckets[hash]; sym; sym = sym->next_in_scope) {
        if (slice_equal_ignore_case(sym->name, name))
            return sym;
    }

    return NULL;
}

/* Lookup with scope chain traversal */
static Symbol *symbol_table_lookup(Symbol_Table *table, String_Slice name) {
    for (Symbol_Table *t = table; t; t = t->parent) {
        Symbol *sym = symbol_table_lookup_local(t, name);
        if (sym) return sym;
    }
    return NULL;
}

/* =============================================================================
 * §10. PARSER — From Tokens to AST
 * =============================================================================
 *
 * Recursive descent parser for Ada 83 syntax. We follow the grammar closely
 * but make pragmatic choices for error recovery and disambiguation.
 *
 * Key design decisions:
 * - Predictive parsing with one-token lookahead
 * - Expression parsing via operator precedence (cleaner than full recursion)
 * - Proper handling of Ada's keyword-heavy syntax
 * - No "pretend token exists" hacks for error recovery
 */

typedef struct Parser {
    Lexer *lexer;
    Token current;
    Symbol_Table *symbols;
    Symbol_Table *global_scope;
    int error_count;
} Parser;

static Parser *parser_new(Lexer *lexer) {
    Parser *p = ALLOC(Parser);
    p->lexer = lexer;
    p->current = lexer_next_token(lexer);
    p->global_scope = symbol_table_new(NULL);
    p->symbols = p->global_scope;
    return p;
}

static Token parser_peek(Parser *p) {
    return p->current;
}

static Token parser_advance(Parser *p) {
    Token prev = p->current;
    p->current = lexer_next_token(p->lexer);
    return prev;
}

static bool parser_match(Parser *p, Token_Kind kind) {
    return p->current.kind == kind;
}

static bool parser_consume(Parser *p, Token_Kind kind) {
    if (parser_match(p, kind)) {
        parser_advance(p);
        return true;
    }
    return false;
}

static void parser_expect(Parser *p, Token_Kind kind, const char *message) {
    if (not parser_consume(p, kind)) {
        report_error(p->current.location, "%s (got %d)", message, p->current.kind);
        p->error_count++;
    }
}

static AST_Node *ast_node_new(Node_Kind kind, Source_Location loc) {
    AST_Node *node = ALLOC(AST_Node);
    node->kind = kind;
    node->location = loc;
    return node;
}

/* Forward declarations for recursive grammar */
static AST_Node *parse_expression(Parser *p);
static AST_Node *parse_statement(Parser *p);
static AST_Node *parse_declaration(Parser *p);
static AST_Node *parse_type_definition(Parser *p);

/* =============================================================================
 * Expression Parsing — Operator Precedence
 * =============================================================================
 *
 * Ada's expression syntax has these precedence levels (lowest to highest):
 *   1. Logical:     and, or, xor
 *   2. Relational:  =, /=, <, <=, >, >=, in, not in
 *   3. Additive:    +, -, &
 *   4. Multiplicative: *, /, mod, rem
 *   5. Unary:       +, -, not, abs
 *   6. Exponential: **
 *   7. Primary:     literals, names, aggregates, (expr)
 */

typedef enum {
    PREC_NONE,
    PREC_LOGICAL,      /* and or xor */
    PREC_RELATIONAL,   /* = /= < <= > >= */
    PREC_ADDITIVE,     /* + - & */
    PREC_MULTIPLICATIVE, /* * / mod rem */
    PREC_UNARY,        /* unary + - not abs */
    PREC_EXPONENTIAL,  /* ** */
    PREC_PRIMARY,
} Precedence;

static Precedence get_precedence(Token_Kind kind) {
    switch (kind) {
        case TOK_AND: case TOK_OR: case TOK_XOR:
            return PREC_LOGICAL;
        case TOK_EQUAL: case TOK_NOT_EQUAL:
        case TOK_LESS: case TOK_LESS_EQUAL:
        case TOK_GREATER: case TOK_GREATER_EQUAL:
        case TOK_IN:
            return PREC_RELATIONAL;
        case TOK_PLUS: case TOK_MINUS: case TOK_AMPERSAND:
            return PREC_ADDITIVE;
        case TOK_STAR: case TOK_SLASH: case TOK_MOD: case TOK_REM:
            return PREC_MULTIPLICATIVE;
        case TOK_DOUBLE_STAR:
            return PREC_EXPONENTIAL;
        default:
            return PREC_NONE;
    }
}

static AST_Node *parse_primary(Parser *p);

static AST_Node *parse_binary_expression(Parser *p, Precedence min_precedence) {
    AST_Node *left = parse_primary(p);

    while (get_precedence(p->current.kind) >= min_precedence) {
        Token op_token = parser_advance(p);
        Precedence precedence = get_precedence(op_token.kind);

        /* Right-associative for ** */
        Precedence next_prec = (op_token.kind == TOK_DOUBLE_STAR)
            ? precedence
            : (Precedence)(precedence + 1);

        AST_Node *right = parse_binary_expression(p, next_prec);

        AST_Node *binary = ast_node_new(NODE_BINARY_OP, op_token.location);
        binary->binary_op.operator = op_token.kind;
        binary->binary_op.left = left;
        binary->binary_op.right = right;
        left = binary;
    }

    return left;
}

static AST_Node *parse_primary(Parser *p) {
    Token tok = p->current;

    /* Literals */
    if (tok.kind == TOK_INTEGER_LITERAL) {
        parser_advance(p);
        AST_Node *node = ast_node_new(NODE_INTEGER_LITERAL, tok.location);
        node->integer_literal.value = tok.integer_value;
        node->integer_literal.big_value = tok.bigint_value;
        return node;
    }

    if (tok.kind == TOK_REAL_LITERAL) {
        parser_advance(p);
        AST_Node *node = ast_node_new(NODE_REAL_LITERAL, tok.location);
        node->real_literal.value = tok.real_value;
        return node;
    }

    if (tok.kind == TOK_CHARACTER_LITERAL) {
        parser_advance(p);
        AST_Node *node = ast_node_new(NODE_CHARACTER_LITERAL, tok.location);
        node->character_literal.value = tok.character_value;
        return node;
    }

    if (tok.kind == TOK_STRING_LITERAL) {
        parser_advance(p);
        AST_Node *node = ast_node_new(NODE_STRING_LITERAL, tok.location);
        node->string_literal.value = string_duplicate(tok.text.start);
        return node;
    }

    if (tok.kind == TOK_NULL) {
        parser_advance(p);
        return ast_node_new(NODE_NULL_LITERAL, tok.location);
    }

    /* Identifier */
    if (tok.kind == TOK_IDENTIFIER) {
        parser_advance(p);
        AST_Node *node = ast_node_new(NODE_IDENTIFIER, tok.location);
        node->string_literal.value = slice_to_lowercase(tok.text);
        return node;
    }

    /* Parenthesized expression */
    if (tok.kind == TOK_LPAREN) {
        parser_advance(p);
        AST_Node *expr = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "expected ')' after expression");
        return expr;
    }

    /* Unary operators */
    if (tok.kind == TOK_PLUS or tok.kind == TOK_MINUS or tok.kind == TOK_NOT or tok.kind == TOK_ABS) {
        parser_advance(p);
        AST_Node *operand = parse_binary_expression(p, PREC_UNARY);
        AST_Node *node = ast_node_new(NODE_UNARY_OP, tok.location);
        node->unary_op.operator = tok.kind;
        node->unary_op.operand = operand;
        return node;
    }

    /* Error case */
    report_error(tok.location, "unexpected token in expression");
    parser_advance(p);
    return ast_node_new(NODE_IDENTIFIER, tok.location);  /* error recovery */
}

static AST_Node *parse_expression(Parser *p) {
    return parse_binary_expression(p, PREC_LOGICAL);
}

/* =============================================================================
 * Statement Parsing
 * =============================================================================
 */

static AST_Node *parse_assignment_or_call(Parser *p) {
    Token name_tok = parser_peek(p);
    AST_Node *target = parse_expression(p);

    if (parser_consume(p, TOK_COLON_EQUAL)) {
        AST_Node *value = parse_expression(p);
        AST_Node *node = ast_node_new(NODE_ASSIGNMENT, name_tok.location);
        node->assignment.target = target;
        node->assignment.value = value;
        return node;
    }

    /* It's a procedure call */
    AST_Node *node = ast_node_new(NODE_PROCEDURE_CALL, name_tok.location);
    node->call.function = target;
    return node;
}

static AST_Node *parse_if_statement(Parser *p) {
    Source_Location loc = parser_peek(p).location;
    parser_expect(p, TOK_IF, "expected 'if'");

    AST_Node *condition = parse_expression(p);
    parser_expect(p, TOK_THEN, "expected 'then' after condition");

    AST_Vector then_stmts = {0};
    while (not parser_match(p, TOK_ELSIF) and not parser_match(p, TOK_ELSE) and not parser_match(p, TOK_END)) {
        ast_vector_push(&then_stmts, parse_statement(p));
    }

    /* TODO: Handle elsif and else */

    parser_expect(p, TOK_END, "expected 'end'");
    parser_expect(p, TOK_IF, "expected 'if' after 'end'");
    parser_expect(p, TOK_SEMICOLON, "expected ';'");

    AST_Node *node = ast_node_new(NODE_IF_STATEMENT, loc);
    node->if_stmt.condition = condition;
    node->if_stmt.then_statements = then_stmts.items;
    node->if_stmt.then_count = then_stmts.count;
    return node;
}

static AST_Node *parse_while_loop(Parser *p) {
    Source_Location loc = parser_peek(p).location;
    parser_expect(p, TOK_WHILE, "expected 'while'");

    AST_Node *condition = parse_expression(p);
    parser_expect(p, TOK_LOOP, "expected 'loop'");

    AST_Vector body = {0};
    while (not parser_match(p, TOK_END)) {
        ast_vector_push(&body, parse_statement(p));
    }

    parser_expect(p, TOK_END, "expected 'end'");
    parser_expect(p, TOK_LOOP, "expected 'loop' after 'end'");
    parser_expect(p, TOK_SEMICOLON, "expected ';'");

    AST_Node *node = ast_node_new(NODE_WHILE_LOOP, loc);
    node->loop_stmt.iteration_scheme = condition;
    node->loop_stmt.body = body.items;
    node->loop_stmt.body_count = body.count;
    return node;
}

static AST_Node *parse_return_statement(Parser *p) {
    Source_Location loc = parser_peek(p).location;
    parser_expect(p, TOK_RETURN, "expected 'return'");

    AST_Node *node = ast_node_new(NODE_RETURN_STATEMENT, loc);

    if (not parser_match(p, TOK_SEMICOLON)) {
        node->assignment.value = parse_expression(p);
    }

    parser_expect(p, TOK_SEMICOLON, "expected ';'");
    return node;
}

static AST_Node *parse_statement(Parser *p) {
    /* Null statement */
    if (parser_consume(p, TOK_NULL)) {
        parser_expect(p, TOK_SEMICOLON, "expected ';'");
        return ast_node_new(NODE_NULL_STATEMENT, p->current.location);
    }

    /* Control flow */
    if (parser_match(p, TOK_IF)) return parse_if_statement(p);
    if (parser_match(p, TOK_WHILE)) return parse_while_loop(p);
    if (parser_match(p, TOK_RETURN)) return parse_return_statement(p);

    /* Assignment or procedure call */
    AST_Node *stmt = parse_assignment_or_call(p);
    parser_expect(p, TOK_SEMICOLON, "expected ';'");
    return stmt;
}

/* =============================================================================
 * Declaration Parsing — Types, Variables, Subprograms
 * =============================================================================
 */

static AST_Node *parse_type_definition(Parser *p) {
    /* Type definitions come after "type Name is ..." */
    Token tok = parser_peek(p);

    /* Range type: type T is range Low .. High; */
    if (parser_match(p, TOK_RANGE)) {
        parser_advance(p);
        AST_Node *low = parse_expression(p);
        parser_expect(p, TOK_DOT_DOT, "expected '..' in range");
        AST_Node *high = parse_expression(p);

        AST_Node *node = ast_node_new(NODE_INTEGER_TYPE, tok.location);
        node->assignment.target = low;
        node->assignment.value = high;
        return node;
    }

    /* Array type: type T is array (Index_Type) of Element_Type; */
    if (parser_match(p, TOK_ARRAY)) {
        parser_advance(p);
        parser_expect(p, TOK_LPAREN, "expected '(' after 'array'");

        AST_Node *index = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "expected ')' after index type");
        parser_expect(p, TOK_OF, "expected 'of' after array index");

        AST_Node *element = parse_expression(p);

        AST_Node *node = ast_node_new(NODE_ARRAY_TYPE, tok.location);
        node->assignment.target = index;
        node->assignment.value = element;
        return node;
    }

    /* Access type: type T is access Some_Type; */
    if (parser_match(p, TOK_ACCESS)) {
        parser_advance(p);
        AST_Node *target = parse_expression(p);

        AST_Node *node = ast_node_new(NODE_ACCESS_TYPE, tok.location);
        node->unary_op.operand = target;
        return node;
    }

    /* Record type: type T is record ... end record; */
    if (parser_match(p, TOK_RECORD)) {
        parser_advance(p);

        AST_Vector components = {0};
        while (not parser_match(p, TOK_END)) {
            /* Parse component: Name : Type; */
            if (not parser_match(p, TOK_IDENTIFIER)) break;

            Token name = parser_advance(p);
            parser_expect(p, TOK_COLON, "expected ':' after component name");
            AST_Node *type = parse_expression(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after component");

            AST_Node *component = ast_node_new(NODE_OBJECT_DECLARATION, name.location);
            component->declaration.name = slice_to_lowercase(name.text);
            component->declaration.type_spec = type;
            ast_vector_push(&components, component);
        }

        parser_expect(p, TOK_END, "expected 'end' after record");
        parser_expect(p, TOK_RECORD, "expected 'record' after 'end'");

        AST_Node *node = ast_node_new(NODE_RECORD_TYPE, tok.location);
        node->aggregate.components = components.items;
        node->aggregate.component_count = components.count;
        return node;
    }

    /* Otherwise, it's a subtype or derived type reference */
    return parse_expression(p);
}

static AST_Node *parse_declaration(Parser *p) {
    Token tok = parser_peek(p);

    /* Type declaration: type Name is ... */
    if (parser_match(p, TOK_TYPE)) {
        parser_advance(p);

        if (not parser_match(p, TOK_IDENTIFIER)) {
            report_error(tok.location, "expected type name after 'type'");
            return NULL;
        }

        Token name = parser_advance(p);
        parser_expect(p, TOK_IS, "expected 'is' after type name");

        AST_Node *definition = parse_type_definition(p);
        parser_expect(p, TOK_SEMICOLON, "expected ';' after type declaration");

        AST_Node *node = ast_node_new(NODE_TYPE_DECLARATION, tok.location);
        node->declaration.name = slice_to_lowercase(name.text);
        node->declaration.type_spec = definition;

        /* Register in symbol table */
        Symbol *sym = symbol_new(name.text, SYMBOL_TYPE);
        sym->declaration = node;
        symbol_table_insert(p->symbols, sym);

        return node;
    }

    /* Subtype declaration: subtype Name is Type_Name; */
    if (parser_match(p, TOK_SUBTYPE)) {
        parser_advance(p);

        Token name = parser_advance(p);
        parser_expect(p, TOK_IS, "expected 'is' after subtype name");

        AST_Node *base = parse_expression(p);
        parser_expect(p, TOK_SEMICOLON, "expected ';' after subtype");

        AST_Node *node = ast_node_new(NODE_SUBTYPE_DECLARATION, tok.location);
        node->declaration.name = slice_to_lowercase(name.text);
        node->declaration.type_spec = base;
        return node;
    }

    /* Variable/constant declaration: Name : [constant] Type [:= Initial]; */
    if (parser_match(p, TOK_IDENTIFIER)) {
        Token name = parser_advance(p);
        parser_expect(p, TOK_COLON, "expected ':' after variable name");

        bool is_constant = parser_consume(p, TOK_CONSTANT);

        AST_Node *type_spec = parse_expression(p);
        AST_Node *initializer = NULL;

        if (parser_consume(p, TOK_COLON_EQUAL)) {
            initializer = parse_expression(p);
        }

        parser_expect(p, TOK_SEMICOLON, "expected ';' after declaration");

        AST_Node *node = ast_node_new(NODE_OBJECT_DECLARATION, tok.location);
        node->declaration.name = slice_to_lowercase(name.text);
        node->declaration.type_spec = type_spec;
        node->declaration.initializer = initializer;
        node->declaration.is_constant = is_constant;

        /* Register in symbol table */
        Symbol *sym = symbol_new(name.text, is_constant ? SYMBOL_CONSTANT : SYMBOL_VARIABLE);
        sym->declaration = node;
        symbol_table_insert(p->symbols, sym);

        return node;
    }

    /* Procedure/function declaration */
    if (parser_match(p, TOK_PROCEDURE) or parser_match(p, TOK_FUNCTION)) {
        bool is_function = parser_match(p, TOK_FUNCTION);
        parser_advance(p);

        Token name = parser_advance(p);

        /* Parameters */
        AST_Vector params = {0};
        if (parser_consume(p, TOK_LPAREN)) {
            while (not parser_match(p, TOK_RPAREN)) {
                Token param_name = parser_advance(p);
                parser_expect(p, TOK_COLON, "expected ':' after parameter");

                /* Mode: in, out, in out */
                bool is_in = parser_consume(p, TOK_IN);
                bool is_out = parser_consume(p, TOK_OUT);
                (void)is_in; (void)is_out;  /* TODO: use these */

                AST_Node *param_type = parse_expression(p);

                AST_Node *param = ast_node_new(NODE_PARAMETER_DECLARATION, param_name.location);
                param->declaration.name = slice_to_lowercase(param_name.text);
                param->declaration.type_spec = param_type;
                ast_vector_push(&params, param);

                if (not parser_match(p, TOK_RPAREN))
                    parser_expect(p, TOK_SEMICOLON, "expected ';' between parameters");
            }
            parser_expect(p, TOK_RPAREN, "expected ')' after parameters");
        }

        /* Return type for functions */
        AST_Node *return_type = NULL;
        if (is_function) {
            parser_expect(p, TOK_RETURN, "expected 'return' for function");
            return_type = parse_expression(p);
        }

        /* Check if it's a declaration or body */
        if (parser_match(p, TOK_IS)) {
            /* Subprogram body */
            parser_advance(p);

            /* Declarative part */
            AST_Vector declarations = {0};
            while (not parser_match(p, TOK_BEGIN)) {
                AST_Node *decl = parse_declaration(p);
                if (decl) ast_vector_push(&declarations, decl);
                else break;
            }

            parser_expect(p, TOK_BEGIN, "expected 'begin' in subprogram body");

            /* Statement part */
            AST_Vector statements = {0};
            while (not parser_match(p, TOK_END)) {
                ast_vector_push(&statements, parse_statement(p));
            }

            parser_expect(p, TOK_END, "expected 'end' after statements");
            if (parser_match(p, TOK_IDENTIFIER)) parser_advance(p);  /* optional name */
            parser_expect(p, TOK_SEMICOLON, "expected ';' after subprogram");

            AST_Node *node = ast_node_new(NODE_SUBPROGRAM_BODY, tok.location);
            node->subprogram.name = slice_to_lowercase(name.text);
            node->subprogram.parameters = params.items;
            node->subprogram.parameter_count = params.count;
            node->subprogram.return_type = return_type;
            node->subprogram.body = statements.items;
            node->subprogram.body_count = statements.count;
            return node;
        } else {
            /* Just a declaration */
            parser_expect(p, TOK_SEMICOLON, "expected ';' after subprogram declaration");

            AST_Node *node = ast_node_new(NODE_SUBPROGRAM_DECLARATION, tok.location);
            node->subprogram.name = slice_to_lowercase(name.text);
            node->subprogram.parameters = params.items;
            node->subprogram.parameter_count = params.count;
            node->subprogram.return_type = return_type;
            return node;
        }
    }

    return NULL;
}

/* =============================================================================
 * §11. SEMANTIC ANALYSIS — Type Checking and Resolution
 * =============================================================================
 *
 * The semantic phase walks the AST and:
 * - Resolves all identifier references to their declarations
 * - Checks type compatibility for operations and assignments
 * - Validates constraints (ranges, indices, discriminants)
 * - Computes expression types
 *
 * Following GNAT's approach: semantic analysis is a separate pass that
 * annotates the AST with type information, preparing it for codegen.
 */

typedef struct {
    Symbol_Table *symbols;
    int error_count;
} Semantic_Analyzer;

static Type_Info *resolve_type_expression(Semantic_Analyzer *sem, AST_Node *node) {
    if (not node) return NULL;

    switch (node->kind) {
        case NODE_IDENTIFIER: {
            /* Look up type name */
            String_Slice name = make_slice(node->string_literal.value);
            Symbol *sym = symbol_table_lookup(sem->symbols, name);

            if (not sym) {
                report_error(node->location, "undefined type '%.*s'", SLICE_ARG(name));
                sem->error_count++;
                return NULL;
            }

            if (sym->kind != SYMBOL_TYPE) {
                report_error(node->location, "'%.*s' is not a type", SLICE_ARG(name));
                sem->error_count++;
                return NULL;
            }

            return sym->type;
        }

        case NODE_INTEGER_TYPE: {
            /* Create anonymous integer type with range */
            Type_Info *ty = type_info_new(TYPE_INTEGER);
            ty->is_anonymous = true;
            ty->has_constraint = true;
            /* TODO: evaluate range bounds */
            return ty;
        }

        case NODE_ARRAY_TYPE: {
            Type_Info *ty = type_info_new(TYPE_ARRAY);
            ty->index_type = resolve_type_expression(sem, node->assignment.target);
            ty->element_type = resolve_type_expression(sem, node->assignment.value);
            return ty;
        }

        case NODE_ACCESS_TYPE: {
            Type_Info *ty = type_info_new(TYPE_ACCESS);
            ty->element_type = resolve_type_expression(sem, node->unary_op.operand);
            ty->size = target.pointer_width;
            ty->alignment = target.pointer_alignment;
            return ty;
        }

        case NODE_RECORD_TYPE: {
            Type_Info *ty = type_info_new(TYPE_RECORD);
            /* TODO: process components */
            return ty;
        }

        default:
            return NULL;
    }
}

static void analyze_declaration(Semantic_Analyzer *sem, AST_Node *node) {
    if (not node) return;

    switch (node->kind) {
        case NODE_TYPE_DECLARATION: {
            /* Create type info and attach to symbol */
            Type_Info *ty = resolve_type_expression(sem, node->declaration.type_spec);
            if (ty) {
                ty->name = make_slice(node->declaration.name);

                /* Find symbol and attach type */
                Symbol *sym = symbol_table_lookup_local(sem->symbols, ty->name);
                if (sym) sym->type = ty;
            }
            break;
        }

        case NODE_OBJECT_DECLARATION: {
            /* Resolve variable type */
            Type_Info *ty = resolve_type_expression(sem, node->declaration.type_spec);

            String_Slice name = make_slice(node->declaration.name);
            Symbol *sym = symbol_table_lookup_local(sem->symbols, name);
            if (sym) {
                sym->type = ty;
                node->type = ty;
            }

            /* Type check initializer */
            if (node->declaration.initializer) {
                /* TODO: check initializer type matches variable type */
            }
            break;
        }

        case NODE_SUBPROGRAM_BODY:
        case NODE_SUBPROGRAM_DECLARATION: {
            /* TODO: process parameters and return type */
            break;
        }

        default:
            break;
    }
}

static Semantic_Analyzer *semantic_analyzer_new(Symbol_Table *symbols) {
    Semantic_Analyzer *sem = ALLOC(Semantic_Analyzer);
    sem->symbols = symbols;
    return sem;
}

static void analyze_program(Semantic_Analyzer *sem, AST_Node **declarations, int count) {
    for (int i = 0; i < count; i++) {
        analyze_declaration(sem, declarations[i]);
    }
}

/* =============================================================================
 * §12. CODE GENERATION — LLVM IR Emission (Simplified)
 * =============================================================================
 *
 * Direct LLVM IR text generation. A full implementation would use the LLVM C
 * API, but for demonstration we emit textual IR showing the structure.
 */

typedef struct {
    FILE *output;
    Symbol_Table *symbols;
    int temp_counter;
    int label_counter;
} Code_Generator;

static Code_Generator *codegen_new(FILE *output, Symbol_Table *symbols) {
    Code_Generator *cg = ALLOC(Code_Generator);
    cg->output = output;
    cg->symbols = symbols;
    return cg;
}

static const char *codegen_temp(Code_Generator *cg) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%%t%d", cg->temp_counter++);
    return string_duplicate(buf);
}

static const char *codegen_label(Code_Generator *cg) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "L%d", cg->label_counter++);
    return string_duplicate(buf);
}

static void codegen_expression(Code_Generator *cg, AST_Node *node, const char **result) {
    if (not node) return;

    switch (node->kind) {
        case NODE_INTEGER_LITERAL: {
            *result = codegen_temp(cg);
            fprintf(cg->output, "  %s = add i32 0, %ld  ; constant\n",
                    *result, (long)node->integer_literal.value);
            break;
        }

        case NODE_IDENTIFIER: {
            /* Load from variable */
            *result = codegen_temp(cg);
            fprintf(cg->output, "  %s = load i32, i32* %%%s\n",
                    *result, node->string_literal.value);
            break;
        }

        case NODE_BINARY_OP: {
            const char *left_val = NULL, *right_val = NULL;
            codegen_expression(cg, node->binary_op.left, &left_val);
            codegen_expression(cg, node->binary_op.right, &right_val);

            *result = codegen_temp(cg);
            const char *op = "add";

            switch (node->binary_op.operator) {
                case TOK_PLUS:  op = "add"; break;
                case TOK_MINUS: op = "sub"; break;
                case TOK_STAR:  op = "mul"; break;
                case TOK_SLASH: op = "sdiv"; break;
                default: break;
            }

            fprintf(cg->output, "  %s = %s i32 %s, %s\n",
                    *result, op, left_val ? left_val : "0", right_val ? right_val : "0");
            break;
        }

        default:
            *result = NULL;
            break;
    }
}

static void codegen_statement(Code_Generator *cg, AST_Node *node) {
    if (not node) return;

    switch (node->kind) {
        case NODE_ASSIGNMENT: {
            const char *value = NULL;
            codegen_expression(cg, node->assignment.value, &value);

            if (value and node->assignment.target->kind == NODE_IDENTIFIER) {
                fprintf(cg->output, "  store i32 %s, i32* %%%s\n",
                        value, node->assignment.target->string_literal.value);
            }
            break;
        }

        case NODE_RETURN_STATEMENT: {
            if (node->assignment.value) {
                const char *value = NULL;
                codegen_expression(cg, node->assignment.value, &value);
                fprintf(cg->output, "  ret i32 %s\n", value ? value : "0");
            } else {
                fprintf(cg->output, "  ret void\n");
            }
            break;
        }

        case NODE_IF_STATEMENT: {
            const char *cond = NULL;
            codegen_expression(cg, node->if_stmt.condition, &cond);

            const char *then_label = codegen_label(cg);
            const char *end_label = codegen_label(cg);

            fprintf(cg->output, "  br i1 %s, label %%%s, label %%%s\n",
                    cond ? cond : "false", then_label, end_label);

            fprintf(cg->output, "%s:\n", then_label);
            for (int i = 0; i < node->if_stmt.then_count; i++) {
                codegen_statement(cg, node->if_stmt.then_statements[i]);
            }
            fprintf(cg->output, "  br label %%%s\n", end_label);

            fprintf(cg->output, "%s:\n", end_label);
            break;
        }

        default:
            break;
    }
}

static void codegen_subprogram(Code_Generator *cg, AST_Node *node) {
    if (node->kind != NODE_SUBPROGRAM_BODY) return;

    /* Function signature */
    const char *ret_type = node->subprogram.return_type ? "i32" : "void";
    fprintf(cg->output, "define %s @%s() {\n", ret_type, node->subprogram.name);
    fprintf(cg->output, "entry:\n");

    /* Generate statements */
    for (int i = 0; i < node->subprogram.body_count; i++) {
        codegen_statement(cg, node->subprogram.body[i]);
    }

    /* Ensure return if none */
    if (not node->subprogram.return_type) {
        fprintf(cg->output, "  ret void\n");
    }

    fprintf(cg->output, "}\n\n");
}

/* =============================================================================
 * Main Driver — Orchestrating the Compilation Pipeline
 * =============================================================================
 */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    FILE *f = fopen(filename, "rb");
    if (not f) fatal_error("cannot open %s: %s", filename, strerror(errno));

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    Lexer *lexer = lexer_new(filename, source);

    /* Simple test: tokenize and print */
    for (;;) {
        Token tok = lexer_next_token(lexer);
        printf("Line %u: ", tok.location.line);

        switch (tok.kind) {
            case TOK_IDENTIFIER:
                printf("IDENTIFIER %.*s\n", (int)tok.text.length, tok.text.start);
                break;
            case TOK_INTEGER_LITERAL:
                printf("INTEGER %ld\n", (long)tok.integer_value);
                break;
            case TOK_REAL_LITERAL:
                printf("REAL %f\n", tok.real_value);
                break;
            case TOK_STRING_LITERAL:
                printf("STRING \"%.*s\"\n", (int)tok.text.length, tok.text.start);
                break;
            case TOK_EOF:
                printf("EOF\n");
                goto done;
            default:
                printf("TOKEN %d\n", tok.kind);
        }
    }

done:
    free(source);
    arena_free_all(&main_arena);

    return error_count > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
