/* ═══════════════════════════════════════════════════════════════════════════
 * ADA83 COMPILER — A Complete Ada 1983 Compiler Targeting LLVM IR
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * This compiler follows the Literate Programming tradition: code and
 * documentation are interwoven to tell the story of compilation.
 *
 * Architecture (four phases, each a pure transformation):
 *   Source Text → Lexer → Token Stream
 *   Token Stream → Parser → Abstract Syntax Tree
 *   AST → Semantic Analyzer → Typed AST + Symbol Table
 *   Typed AST → Code Generator → LLVM IR
 *
 * Design principles:
 *   • Functional composition over imperative mutation
 *   • Single point of definition for each concept
 *   • Ada-like descriptive names throughout
 *   • Explicit is better than implicit
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * §1  FOUNDATIONAL TYPES — The Atoms of Representation
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * "Begin at the beginning," the King said gravely, "and go on till you
 * come to the end: then stop." — Lewis Carroll
 *
 * We start with the smallest pieces: characters, strings, locations.
 */

/* Safe character classification (ctype.h requires unsigned char) */
#define IS_ALPHA(c)  isalpha((unsigned char)(c))
#define IS_ALNUM(c)  isalnum((unsigned char)(c))
#define IS_DIGIT(c)  isdigit((unsigned char)(c))
#define IS_XDIGIT(c) isxdigit((unsigned char)(c))
#define IS_SPACE(c)  isspace((unsigned char)(c))
#define TO_LOWER(c)  ((char)tolower((unsigned char)(c)))
#define TO_UPPER(c)  ((char)toupper((unsigned char)(c)))

/* String_Slice: A view into text (non-owning, immutable) */
typedef struct { const char *text; uint32_t length; } String_Slice;
#define S(lit) ((String_Slice){lit, sizeof(lit) - 1})
#define EMPTY_STRING ((String_Slice){0, 0})

/* Source_Location: Where in the source text we are */
typedef struct { const char *file; uint32_t line, column; } Source_Location;
#define HERE(f,l,c) ((Source_Location){f, l, c})

/* ═══════════════════════════════════════════════════════════════════════════
 * §2  TYPE METRICS — The Measure of All Things
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Following GNAT LLVM's architecture: all size/alignment calculations
 * flow through these primitives. Units are explicit in function names.
 */

enum { BITS_PER_BYTE = 8 };

/* LLVM type widths (in bits) */
enum {
    WIDTH_1 = 1, WIDTH_8 = 8, WIDTH_16 = 16, WIDTH_32 = 32,
    WIDTH_64 = 64, WIDTH_128 = 128, WIDTH_POINTER = 64,
    WIDTH_FLOAT = 32, WIDTH_DOUBLE = 64
};

/* Ada standard integer widths per RM §3.5.4 */
enum {
    ADA_SHORT_SHORT_INTEGER_BITS = WIDTH_8,
    ADA_SHORT_INTEGER_BITS       = WIDTH_16,
    ADA_INTEGER_BITS             = WIDTH_32,
    ADA_LONG_INTEGER_BITS        = WIDTH_64
};

/* Conversion functions — names encode units */
static inline uint64_t bytes_to_bits(uint64_t b) { return b * BITS_PER_BYTE; }
static inline uint64_t bits_to_bytes(uint64_t b) { return (b + 7) / 8; }
static inline uint64_t align_up(uint64_t size, uint64_t alignment) {
    return alignment ? (size + alignment - 1) & ~(alignment - 1) : size;
}

/* Select smallest LLVM integer type for given bit width */
static inline const char *llvm_integer_type_for_bits(uint32_t bits) {
    if (bits <= 1)  return "i1";
    if (bits <= 8)  return "i8";
    if (bits <= 16) return "i16";
    if (bits <= 32) return "i32";
    if (bits <= 64) return "i64";
    return "i128";
}

/* Compute minimum bits needed to represent range [low, high] */
static uint32_t bits_required_for_range(int64_t low, int64_t high) {
    if (low >= 0) {  /* Unsigned range */
        if ((uint64_t)high < 256ULL) return WIDTH_8;
        if ((uint64_t)high < 65536ULL) return WIDTH_16;
        if ((uint64_t)high < 4294967296ULL) return WIDTH_32;
        return WIDTH_64;
    }
    /* Signed range: find smallest n where -2^(n-1) <= low and high <= 2^(n-1)-1 */
    for (uint32_t n = 8; n <= 64; n *= 2) {
        int64_t min_val = -(1LL << (n - 1));
        int64_t max_val = (1LL << (n - 1)) - 1;
        if (low >= min_val && high <= max_val) return n;
    }
    return WIDTH_64;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §3  MEMORY ARENA — Simple Bump Allocation
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * A compiler is a batch process: allocate forward, free everything at end.
 * The arena gives us fast allocation with trivial "garbage collection".
 */

typedef struct Arena_Block {
    struct Arena_Block *previous;
    char *current, *end;
    char data[];
} Arena_Block;

static struct { Arena_Block *current; } Global_Arena;

static void *arena_allocate(size_t size) {
    size = align_up(size, 8);
    Arena_Block *block = Global_Arena.current;
    if (!block || block->current + size > block->end) {
        size_t block_size = (1 << 24);  /* 16 MB blocks */
        if (size > block_size - sizeof(Arena_Block))
            block_size = size + sizeof(Arena_Block);
        Arena_Block *new_block = malloc(sizeof(Arena_Block) + block_size);
        new_block->previous = block;
        new_block->current = new_block->data;
        new_block->end = new_block->data + block_size;
        Global_Arena.current = block = new_block;
    }
    void *result = block->current;
    block->current += size;
    return memset(result, 0, size);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §4  STRING OPERATIONS — Text Manipulation
 * ═══════════════════════════════════════════════════════════════════════════
 */

static String_Slice string_duplicate(String_Slice s) {
    char *copy = arena_allocate(s.length + 1);
    memcpy(copy, s.text, s.length);
    return (String_Slice){copy, s.length};
}

static bool string_equal_ignore_case(String_Slice a, String_Slice b) {
    if (a.length != b.length) return false;
    for (uint32_t i = 0; i < a.length; i++)
        if (TO_LOWER(a.text[i]) != TO_LOWER(b.text[i])) return false;
    return true;
}

static uint64_t string_hash(String_Slice s) {
    uint64_t h = 14695981039346656037ULL;  /* FNV-1a */
    for (uint32_t i = 0; i < s.length; i++)
        h = (h ^ (uint8_t)TO_LOWER(s.text[i])) * 1099511628211ULL;
    return h;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §5  ERROR REPORTING — Communicating Problems
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int Error_Count = 0;

static void report_error(Source_Location loc, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%u:%u: error: ", loc.file, loc.line, loc.column);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    Error_Count++;
}

static _Noreturn void fatal_error(Source_Location loc, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%u:%u: fatal: ", loc.file, loc.line, loc.column);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    exit(1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §6  ARBITRARY PRECISION INTEGERS — For Literal Scanning
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ada allows integer literals of arbitrary size. We represent them as
 * arrays of 64-bit limbs in little-endian order.
 */

typedef struct {
    uint64_t *limbs;
    uint32_t count, capacity;
    bool is_negative;
} Big_Integer;

static Big_Integer *bigint_new(uint32_t capacity) {
    Big_Integer *b = arena_allocate(sizeof(Big_Integer));
    b->limbs = arena_allocate(capacity * sizeof(uint64_t));
    b->capacity = capacity;
    return b;
}

static void bigint_ensure_capacity(Big_Integer *b, uint32_t needed) {
    if (needed <= b->capacity) return;
    uint64_t *new_limbs = arena_allocate(needed * sizeof(uint64_t));
    memcpy(new_limbs, b->limbs, b->count * sizeof(uint64_t));
    b->limbs = new_limbs;
    b->capacity = needed;
}

static void bigint_normalize(Big_Integer *b) {
    while (b->count > 0 && b->limbs[b->count - 1] == 0) b->count--;
    if (b->count == 0) b->is_negative = false;
}

/* Multiply by small value and add digit (for parsing) */
static void bigint_multiply_add(Big_Integer *b, uint64_t multiplier, uint64_t addend) {
    bigint_ensure_capacity(b, b->count + 1);
    __uint128_t carry = addend;
    for (uint32_t i = 0; i < b->count; i++) {
        __uint128_t product = (__uint128_t)b->limbs[i] * multiplier + carry;
        b->limbs[i] = (uint64_t)product;
        carry = product >> 64;
    }
    if (carry) b->limbs[b->count++] = (uint64_t)carry;
    bigint_normalize(b);
}

static Big_Integer *bigint_from_decimal(const char *text) {
    Big_Integer *result = bigint_new(4);
    bool negative = (*text == '-');
    if (*text == '-' || *text == '+') text++;
    while (*text) {
        if (*text >= '0' && *text <= '9')
            bigint_multiply_add(result, 10, *text - '0');
        text++;
    }
    result->is_negative = negative && result->count > 0;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §7  TOKEN KINDS — The Vocabulary of Ada
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ada 83 has 63 reserved words plus operators and delimiters.
 * We encode them all as a single enumeration for efficient dispatch.
 */

typedef enum {
    /* End and error markers */
    TOKEN_END_OF_FILE = 0, TOKEN_ERROR,
    /* Literals and identifiers */
    TOKEN_IDENTIFIER, TOKEN_INTEGER, TOKEN_REAL, TOKEN_CHARACTER, TOKEN_STRING,
    /* Single-character delimiters */
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_TICK,
    /* Multi-character operators and delimiters */
    TOKEN_ASSIGN, TOKEN_ARROW, TOKEN_DOUBLE_DOT, TOKEN_LEFT_LABEL, TOKEN_RIGHT_LABEL,
    TOKEN_BOX, TOKEN_BAR,
    /* Comparison operators */
    TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    /* Arithmetic operators */
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_AMPERSAND, TOKEN_POWER,
    /* Reserved words (alphabetical) */
    TOKEN_ABORT, TOKEN_ABS, TOKEN_ACCEPT, TOKEN_ACCESS, TOKEN_ALL, TOKEN_AND,
    TOKEN_AND_THEN, TOKEN_ARRAY, TOKEN_AT, TOKEN_BEGIN, TOKEN_BODY, TOKEN_CASE,
    TOKEN_CONSTANT, TOKEN_DECLARE, TOKEN_DELAY, TOKEN_DELTA, TOKEN_DIGITS, TOKEN_DO,
    TOKEN_ELSE, TOKEN_ELSIF, TOKEN_END, TOKEN_ENTRY, TOKEN_EXCEPTION, TOKEN_EXIT,
    TOKEN_FOR, TOKEN_FUNCTION, TOKEN_GENERIC, TOKEN_GOTO, TOKEN_IF, TOKEN_IN,
    TOKEN_IS, TOKEN_LIMITED, TOKEN_LOOP, TOKEN_MOD, TOKEN_NEW, TOKEN_NOT,
    TOKEN_NULL, TOKEN_OF, TOKEN_OR, TOKEN_OR_ELSE, TOKEN_OTHERS, TOKEN_OUT,
    TOKEN_PACKAGE, TOKEN_PRAGMA, TOKEN_PRIVATE, TOKEN_PROCEDURE, TOKEN_RAISE,
    TOKEN_RANGE, TOKEN_RECORD, TOKEN_REM, TOKEN_RENAMES, TOKEN_RETURN, TOKEN_REVERSE,
    TOKEN_SELECT, TOKEN_SEPARATE, TOKEN_SUBTYPE, TOKEN_TASK, TOKEN_TERMINATE,
    TOKEN_THEN, TOKEN_TYPE, TOKEN_USE, TOKEN_WHEN, TOKEN_WHILE, TOKEN_WITH, TOKEN_XOR,
    TOKEN_COUNT
} Token_Kind;

/* Token kind to display name (for error messages) */
static const char *Token_Names[TOKEN_COUNT] = {
    [TOKEN_END_OF_FILE] = "end of file", [TOKEN_ERROR] = "error",
    [TOKEN_IDENTIFIER] = "identifier", [TOKEN_INTEGER] = "integer",
    [TOKEN_REAL] = "real", [TOKEN_CHARACTER] = "character", [TOKEN_STRING] = "string",
    [TOKEN_LEFT_PAREN] = "(", [TOKEN_RIGHT_PAREN] = ")", [TOKEN_LEFT_BRACKET] = "[",
    [TOKEN_RIGHT_BRACKET] = "]", [TOKEN_COMMA] = ",", [TOKEN_DOT] = ".",
    [TOKEN_SEMICOLON] = ";", [TOKEN_COLON] = ":", [TOKEN_TICK] = "'",
    [TOKEN_ASSIGN] = ":=", [TOKEN_ARROW] = "=>", [TOKEN_DOUBLE_DOT] = "..",
    [TOKEN_LEFT_LABEL] = "<<", [TOKEN_RIGHT_LABEL] = ">>", [TOKEN_BOX] = "<>",
    [TOKEN_BAR] = "|", [TOKEN_EQUAL] = "=", [TOKEN_NOT_EQUAL] = "/=",
    [TOKEN_LESS] = "<", [TOKEN_LESS_EQUAL] = "<=", [TOKEN_GREATER] = ">",
    [TOKEN_GREATER_EQUAL] = ">=", [TOKEN_PLUS] = "+", [TOKEN_MINUS] = "-",
    [TOKEN_STAR] = "*", [TOKEN_SLASH] = "/", [TOKEN_AMPERSAND] = "&",
    [TOKEN_POWER] = "**", [TOKEN_ABORT] = "ABORT", [TOKEN_ABS] = "ABS",
    [TOKEN_ACCEPT] = "ACCEPT", [TOKEN_ACCESS] = "ACCESS", [TOKEN_ALL] = "ALL",
    [TOKEN_AND] = "AND", [TOKEN_AND_THEN] = "AND THEN", [TOKEN_ARRAY] = "ARRAY",
    [TOKEN_AT] = "AT", [TOKEN_BEGIN] = "BEGIN", [TOKEN_BODY] = "BODY",
    [TOKEN_CASE] = "CASE", [TOKEN_CONSTANT] = "CONSTANT", [TOKEN_DECLARE] = "DECLARE",
    [TOKEN_DELAY] = "DELAY", [TOKEN_DELTA] = "DELTA", [TOKEN_DIGITS] = "DIGITS",
    [TOKEN_DO] = "DO", [TOKEN_ELSE] = "ELSE", [TOKEN_ELSIF] = "ELSIF",
    [TOKEN_END] = "END", [TOKEN_ENTRY] = "ENTRY", [TOKEN_EXCEPTION] = "EXCEPTION",
    [TOKEN_EXIT] = "EXIT", [TOKEN_FOR] = "FOR", [TOKEN_FUNCTION] = "FUNCTION",
    [TOKEN_GENERIC] = "GENERIC", [TOKEN_GOTO] = "GOTO", [TOKEN_IF] = "IF",
    [TOKEN_IN] = "IN", [TOKEN_IS] = "IS", [TOKEN_LIMITED] = "LIMITED",
    [TOKEN_LOOP] = "LOOP", [TOKEN_MOD] = "MOD", [TOKEN_NEW] = "NEW",
    [TOKEN_NOT] = "NOT", [TOKEN_NULL] = "NULL", [TOKEN_OF] = "OF",
    [TOKEN_OR] = "OR", [TOKEN_OR_ELSE] = "OR ELSE", [TOKEN_OTHERS] = "OTHERS",
    [TOKEN_OUT] = "OUT", [TOKEN_PACKAGE] = "PACKAGE", [TOKEN_PRAGMA] = "PRAGMA",
    [TOKEN_PRIVATE] = "PRIVATE", [TOKEN_PROCEDURE] = "PROCEDURE",
    [TOKEN_RAISE] = "RAISE", [TOKEN_RANGE] = "RANGE", [TOKEN_RECORD] = "RECORD",
    [TOKEN_REM] = "REM", [TOKEN_RENAMES] = "RENAMES", [TOKEN_RETURN] = "RETURN",
    [TOKEN_REVERSE] = "REVERSE", [TOKEN_SELECT] = "SELECT",
    [TOKEN_SEPARATE] = "SEPARATE", [TOKEN_SUBTYPE] = "SUBTYPE", [TOKEN_TASK] = "TASK",
    [TOKEN_TERMINATE] = "TERMINATE", [TOKEN_THEN] = "THEN", [TOKEN_TYPE] = "TYPE",
    [TOKEN_USE] = "USE", [TOKEN_WHEN] = "WHEN", [TOKEN_WHILE] = "WHILE",
    [TOKEN_WITH] = "WITH", [TOKEN_XOR] = "XOR"
};

/* Keyword table: map identifier text to token kind */
static struct { String_Slice text; Token_Kind kind; } Keywords[] = {
    {S("abort"), TOKEN_ABORT}, {S("abs"), TOKEN_ABS}, {S("accept"), TOKEN_ACCEPT},
    {S("access"), TOKEN_ACCESS}, {S("all"), TOKEN_ALL}, {S("and"), TOKEN_AND},
    {S("array"), TOKEN_ARRAY}, {S("at"), TOKEN_AT}, {S("begin"), TOKEN_BEGIN},
    {S("body"), TOKEN_BODY}, {S("case"), TOKEN_CASE}, {S("constant"), TOKEN_CONSTANT},
    {S("declare"), TOKEN_DECLARE}, {S("delay"), TOKEN_DELAY}, {S("delta"), TOKEN_DELTA},
    {S("digits"), TOKEN_DIGITS}, {S("do"), TOKEN_DO}, {S("else"), TOKEN_ELSE},
    {S("elsif"), TOKEN_ELSIF}, {S("end"), TOKEN_END}, {S("entry"), TOKEN_ENTRY},
    {S("exception"), TOKEN_EXCEPTION}, {S("exit"), TOKEN_EXIT}, {S("for"), TOKEN_FOR},
    {S("function"), TOKEN_FUNCTION}, {S("generic"), TOKEN_GENERIC},
    {S("goto"), TOKEN_GOTO}, {S("if"), TOKEN_IF}, {S("in"), TOKEN_IN},
    {S("is"), TOKEN_IS}, {S("limited"), TOKEN_LIMITED}, {S("loop"), TOKEN_LOOP},
    {S("mod"), TOKEN_MOD}, {S("new"), TOKEN_NEW}, {S("not"), TOKEN_NOT},
    {S("null"), TOKEN_NULL}, {S("of"), TOKEN_OF}, {S("or"), TOKEN_OR},
    {S("others"), TOKEN_OTHERS}, {S("out"), TOKEN_OUT}, {S("package"), TOKEN_PACKAGE},
    {S("pragma"), TOKEN_PRAGMA}, {S("private"), TOKEN_PRIVATE},
    {S("procedure"), TOKEN_PROCEDURE}, {S("raise"), TOKEN_RAISE},
    {S("range"), TOKEN_RANGE}, {S("record"), TOKEN_RECORD}, {S("rem"), TOKEN_REM},
    {S("renames"), TOKEN_RENAMES}, {S("return"), TOKEN_RETURN},
    {S("reverse"), TOKEN_REVERSE}, {S("select"), TOKEN_SELECT},
    {S("separate"), TOKEN_SEPARATE}, {S("subtype"), TOKEN_SUBTYPE},
    {S("task"), TOKEN_TASK}, {S("terminate"), TOKEN_TERMINATE},
    {S("then"), TOKEN_THEN}, {S("type"), TOKEN_TYPE}, {S("use"), TOKEN_USE},
    {S("when"), TOKEN_WHEN}, {S("while"), TOKEN_WHILE}, {S("with"), TOKEN_WITH},
    {S("xor"), TOKEN_XOR}, {EMPTY_STRING, TOKEN_END_OF_FILE}
};

static Token_Kind lookup_keyword(String_Slice identifier) {
    for (int i = 0; Keywords[i].text.text; i++)
        if (string_equal_ignore_case(identifier, Keywords[i].text))
            return Keywords[i].kind;
    return TOKEN_IDENTIFIER;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §8  TOKEN STRUCTURE — What the Lexer Produces
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
    Token_Kind kind;
    Source_Location location;
    String_Slice text;
    int64_t integer_value;
    double real_value;
    Big_Integer *big_integer;
} Token;

static Token make_token(Token_Kind kind, Source_Location loc, String_Slice text) {
    return (Token){kind, loc, text, 0, 0.0, NULL};
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9  LEXER STATE — Tracking Position in Source
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
    const char *start, *current, *end;
    const char *filename;
    uint32_t line, column;
    Token_Kind previous_kind;
} Lexer;

static Lexer lexer_create(const char *source, size_t length, const char *filename) {
    return (Lexer){source, source, source + length, filename, 1, 1, TOKEN_END_OF_FILE};
}

static char lexer_peek(Lexer *L, int offset) {
    return (L->current + offset < L->end) ? L->current[offset] : '\0';
}

static char lexer_advance(Lexer *L) {
    if (L->current >= L->end) return '\0';
    char c = *L->current++;
    if (c == '\n') { L->line++; L->column = 1; }
    else L->column++;
    return c;
}

static void lexer_skip_whitespace_and_comments(Lexer *L) {
    while (L->current < L->end) {
        if (IS_SPACE(*L->current)) {
            lexer_advance(L);
        } else if (L->current + 1 < L->end && L->current[0] == '-' && L->current[1] == '-') {
            while (L->current < L->end && *L->current != '\n') lexer_advance(L);
        } else break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §10  LEXER SCANNING — Recognizing Tokens
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Each scan_* function handles one category of token.
 */

static Token scan_identifier(Lexer *L) {
    Source_Location loc = HERE(L->filename, L->line, L->column);
    const char *start = L->current;
    while (IS_ALNUM(lexer_peek(L, 0)) || lexer_peek(L, 0) == '_') lexer_advance(L);
    String_Slice text = {start, L->current - start};
    return make_token(lookup_keyword(text), loc, text);
}

/* Unified digit value for any base (0-9, A-F) */
static int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static Token scan_number(Lexer *L) {
    Source_Location loc = HERE(L->filename, L->line, L->column);
    const char *start = L->current;
    int base = 10;
    bool is_real = false, has_exponent = false;

    /* Scan initial digits */
    while (IS_DIGIT(lexer_peek(L, 0)) || lexer_peek(L, 0) == '_') lexer_advance(L);

    /* Check for based literal (16#FFFF# or 2#1010#) */
    if (lexer_peek(L, 0) == '#' || (lexer_peek(L, 0) == ':' && IS_XDIGIT(lexer_peek(L, 1)))) {
        char delim = lexer_peek(L, 0);
        char base_buf[16] = {0};
        int bi = 0;
        for (const char *p = start; p < L->current && bi < 15; p++)
            if (*p != '_') base_buf[bi++] = *p;
        base = atoi(base_buf);
        lexer_advance(L);  /* skip delimiter */
        while (IS_XDIGIT(lexer_peek(L, 0)) || lexer_peek(L, 0) == '_' || lexer_peek(L, 0) == '.')
            { if (lexer_peek(L, 0) == '.') is_real = true; lexer_advance(L); }
        if (lexer_peek(L, 0) == delim) lexer_advance(L);
    } else {
        /* Decimal: check for fractional part */
        if (lexer_peek(L, 0) == '.' && lexer_peek(L, 1) != '.' && !IS_ALPHA(lexer_peek(L, 1))) {
            is_real = true;
            lexer_advance(L);
            while (IS_DIGIT(lexer_peek(L, 0)) || lexer_peek(L, 0) == '_') lexer_advance(L);
        }
    }

    /* Exponent */
    if (TO_LOWER(lexer_peek(L, 0)) == 'e') {
        has_exponent = true;
        lexer_advance(L);
        if (lexer_peek(L, 0) == '+' || lexer_peek(L, 0) == '-') lexer_advance(L);
        while (IS_DIGIT(lexer_peek(L, 0)) || lexer_peek(L, 0) == '_') lexer_advance(L);
    }

    String_Slice text = {start, L->current - start};
    Token tok = make_token(is_real ? TOKEN_REAL : TOKEN_INTEGER, loc, text);

    /* Parse value (simplified: for non-based decimals) */
    if (base == 10 && !is_real) {
        char buf[128] = {0};
        int j = 0;
        for (const char *p = start; p < L->current && j < 127; p++)
            if (*p != '_') buf[j++] = *p;
        if (has_exponent) {
            tok.real_value = strtod(buf, NULL);
            if (tok.real_value == (double)(int64_t)tok.real_value)
                tok.integer_value = (int64_t)tok.real_value;
        } else {
            tok.big_integer = bigint_from_decimal(buf);
            tok.integer_value = (tok.big_integer->count == 1) ? tok.big_integer->limbs[0] : 0;
        }
    } else if (is_real) {
        char buf[128] = {0};
        int j = 0;
        for (const char *p = start; p < L->current && j < 127; p++)
            if (*p != '_' && *p != '#' && *p != ':') buf[j++] = *p;
        tok.real_value = strtod(buf, NULL);
    }
    return tok;
}

static Token scan_character(Lexer *L) {
    Source_Location loc = HERE(L->filename, L->line, L->column);
    lexer_advance(L);  /* skip opening ' */
    char c = lexer_peek(L, 0);
    lexer_advance(L);
    if (lexer_peek(L, 0) != '\'')
        return make_token(TOKEN_ERROR, loc, S("unterminated character"));
    lexer_advance(L);
    char *buf = arena_allocate(1);
    *buf = c;
    Token tok = make_token(TOKEN_CHARACTER, loc, (String_Slice){buf, 1});
    tok.integer_value = (unsigned char)c;
    return tok;
}

static Token scan_string(Lexer *L) {
    Source_Location loc = HERE(L->filename, L->line, L->column);
    char delim = lexer_peek(L, 0);
    lexer_advance(L);
    int capacity = 256, length = 0;
    char *buffer = arena_allocate(capacity);

    while (lexer_peek(L, 0)) {
        if (lexer_peek(L, 0) == delim) {
            if (lexer_peek(L, 1) == delim) {
                lexer_advance(L); lexer_advance(L);
                if (length >= capacity - 1) {
                    char *nb = arena_allocate(capacity * 2);
                    memcpy(nb, buffer, length);
                    buffer = nb; capacity *= 2;
                }
                buffer[length++] = delim;
            } else break;
        } else {
            if (length >= capacity - 1) {
                char *nb = arena_allocate(capacity * 2);
                memcpy(nb, buffer, length);
                buffer = nb; capacity *= 2;
            }
            buffer[length++] = lexer_peek(L, 0);
            lexer_advance(L);
        }
    }
    if (lexer_peek(L, 0) == delim) lexer_advance(L);
    else return make_token(TOKEN_ERROR, loc, S("unterminated string"));
    buffer[length] = '\0';
    return make_token(TOKEN_STRING, loc, (String_Slice){buffer, length});
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §11  LEXER MAIN — The Token Stream Generator
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Token lexer_next_token(Lexer *L) {
    lexer_skip_whitespace_and_comments(L);
    Source_Location loc = HERE(L->filename, L->line, L->column);
    char c = lexer_peek(L, 0);

    if (!c) { L->previous_kind = TOKEN_END_OF_FILE; return make_token(TOKEN_END_OF_FILE, loc, EMPTY_STRING); }
    if (IS_ALPHA(c)) { Token t = scan_identifier(L); L->previous_kind = t.kind; return t; }
    if (IS_DIGIT(c)) { Token t = scan_number(L); L->previous_kind = t.kind; return t; }

    /* Character literal vs tick: 'x' is char if followed by closing ' */
    if (c == '\'') {
        if (lexer_peek(L, 1) && lexer_peek(L, 2) == '\'' &&
            L->previous_kind != TOKEN_IDENTIFIER) {
            Token t = scan_character(L); L->previous_kind = t.kind; return t;
        }
        lexer_advance(L);
        L->previous_kind = TOKEN_TICK;
        return make_token(TOKEN_TICK, loc, S("'"));
    }

    if (c == '"' || c == '%') { Token t = scan_string(L); L->previous_kind = t.kind; return t; }

    lexer_advance(L);
    Token_Kind kind;

    switch (c) {
    case '(': kind = TOKEN_LEFT_PAREN; break;
    case ')': kind = TOKEN_RIGHT_PAREN; break;
    case '[': kind = TOKEN_LEFT_BRACKET; break;
    case ']': kind = TOKEN_RIGHT_BRACKET; break;
    case ',': kind = TOKEN_COMMA; break;
    case ';': kind = TOKEN_SEMICOLON; break;
    case '&': kind = TOKEN_AMPERSAND; break;
    case '|': kind = TOKEN_BAR; break;
    case '+': kind = TOKEN_PLUS; break;
    case '-': kind = TOKEN_MINUS; break;
    case '/': kind = (lexer_peek(L,0)=='=') ? (lexer_advance(L), TOKEN_NOT_EQUAL) : TOKEN_SLASH; break;
    case '*': kind = (lexer_peek(L,0)=='*') ? (lexer_advance(L), TOKEN_POWER) : TOKEN_STAR; break;
    case '=': kind = (lexer_peek(L,0)=='>') ? (lexer_advance(L), TOKEN_ARROW) : TOKEN_EQUAL; break;
    case ':': kind = (lexer_peek(L,0)=='=') ? (lexer_advance(L), TOKEN_ASSIGN) : TOKEN_COLON; break;
    case '.': kind = (lexer_peek(L,0)=='.') ? (lexer_advance(L), TOKEN_DOUBLE_DOT) : TOKEN_DOT; break;
    case '<': kind = (lexer_peek(L,0)=='=') ? (lexer_advance(L), TOKEN_LESS_EQUAL) :
                     (lexer_peek(L,0)=='<') ? (lexer_advance(L), TOKEN_LEFT_LABEL) :
                     (lexer_peek(L,0)=='>') ? (lexer_advance(L), TOKEN_BOX) : TOKEN_LESS; break;
    case '>': kind = (lexer_peek(L,0)=='=') ? (lexer_advance(L), TOKEN_GREATER_EQUAL) :
                     (lexer_peek(L,0)=='>') ? (lexer_advance(L), TOKEN_RIGHT_LABEL) : TOKEN_GREATER; break;
    default:  kind = TOKEN_ERROR; break;
    }
    L->previous_kind = kind;
    return make_token(kind, loc, EMPTY_STRING);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §12  GENERIC VECTORS — Growable Arrays
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * One macro to generate type-safe vector operations.
 */

#define DEFINE_VECTOR(Name, Element_Type)                                      \
    typedef struct { Element_Type *items; uint32_t count, capacity; } Name;    \
    static void Name##_push(Name *v, Element_Type e) {                         \
        if (v->count >= v->capacity) {                                         \
            v->capacity = v->capacity ? v->capacity * 2 : 8;                   \
            Element_Type *new_items = arena_allocate(v->capacity * sizeof(e)); \
            if (v->items) memcpy(new_items, v->items, v->count * sizeof(e));   \
            v->items = new_items;                                              \
        }                                                                      \
        v->items[v->count++] = e;                                              \
    }

/* Forward declarations for recursive types */
typedef struct Syntax_Node Syntax_Node;
typedef struct Type_Info Type_Info;
typedef struct Symbol Symbol;

DEFINE_VECTOR(Node_Vector, Syntax_Node *)
DEFINE_VECTOR(Symbol_Vector, Symbol *)
DEFINE_VECTOR(String_Vector, String_Slice)

/* ═══════════════════════════════════════════════════════════════════════════
 * §13  NODE KINDS — The Grammar of the AST
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Each Node_Kind corresponds to a production in the Ada 83 grammar.
 * Names follow Ada conventions: descriptive, unambiguous.
 */

typedef enum {
    /* Errors and placeholders */
    NODE_ERROR = 0,
    /* Literals and names */
    NODE_IDENTIFIER, NODE_INTEGER_LITERAL, NODE_REAL_LITERAL,
    NODE_CHARACTER_LITERAL, NODE_STRING_LITERAL, NODE_NULL_LITERAL,
    /* Expressions */
    NODE_BINARY_OPERATION, NODE_UNARY_OPERATION, NODE_ATTRIBUTE,
    NODE_QUALIFIED_EXPRESSION, NODE_FUNCTION_CALL, NODE_INDEXED_COMPONENT,
    NODE_SLICE, NODE_SELECTED_COMPONENT, NODE_ALLOCATOR, NODE_AGGREGATE,
    NODE_TYPE_CONVERSION, NODE_DEREFERENCE,
    /* Type definitions */
    NODE_RANGE, NODE_CONSTRAINT, NODE_COMPONENT_DECLARATION, NODE_VARIANT,
    NODE_VARIANT_PART, NODE_DISCRIMINANT_SPECIFICATION,
    NODE_ENUMERATION_TYPE, NODE_INTEGER_TYPE, NODE_FLOAT_TYPE, NODE_FIXED_TYPE,
    NODE_ARRAY_TYPE, NODE_RECORD_TYPE, NODE_ACCESS_TYPE, NODE_PRIVATE_TYPE,
    NODE_SUBTYPE_INDICATION, NODE_DERIVED_TYPE,
    /* Declarations */
    NODE_OBJECT_DECLARATION, NODE_NUMBER_DECLARATION, NODE_TYPE_DECLARATION,
    NODE_SUBTYPE_DECLARATION, NODE_EXCEPTION_DECLARATION, NODE_RENAMING_DECLARATION,
    NODE_PARAMETER_SPECIFICATION, NODE_PROCEDURE_SPECIFICATION, NODE_FUNCTION_SPECIFICATION,
    NODE_PROCEDURE_BODY, NODE_FUNCTION_BODY, NODE_PROCEDURE_DECLARATION, NODE_FUNCTION_DECLARATION,
    NODE_PACKAGE_SPECIFICATION, NODE_PACKAGE_BODY, NODE_PACKAGE_DECLARATION,
    NODE_TASK_SPECIFICATION, NODE_TASK_BODY, NODE_TASK_DECLARATION,
    NODE_ENTRY_DECLARATION,
    NODE_GENERIC_DECLARATION, NODE_GENERIC_INSTANTIATION,
    NODE_GENERIC_TYPE_PARAMETER, NODE_GENERIC_VALUE_PARAMETER, NODE_GENERIC_SUBPROGRAM_PARAMETER,
    /* Statements */
    NODE_ASSIGNMENT_STATEMENT, NODE_IF_STATEMENT, NODE_CASE_STATEMENT,
    NODE_LOOP_STATEMENT, NODE_BLOCK_STATEMENT, NODE_EXIT_STATEMENT,
    NODE_RETURN_STATEMENT, NODE_GOTO_STATEMENT, NODE_RAISE_STATEMENT,
    NODE_NULL_STATEMENT, NODE_PROCEDURE_CALL_STATEMENT, NODE_CODE_STATEMENT,
    NODE_ACCEPT_STATEMENT, NODE_SELECT_STATEMENT, NODE_DELAY_STATEMENT,
    NODE_ABORT_STATEMENT, NODE_ENTRY_CALL_STATEMENT,
    /* Exception handling */
    NODE_EXCEPTION_HANDLER,
    /* Associations and choices */
    NODE_CHOICE, NODE_ASSOCIATION,
    /* Context and compilation */
    NODE_WITH_CLAUSE, NODE_USE_CLAUSE, NODE_PRAGMA,
    NODE_REPRESENTATION_CLAUSE, NODE_CONTEXT_CLAUSE, NODE_COMPILATION_UNIT,
    NODE_LIST,  /* Generic list container */
    NODE_LABEL,
    NODE_COUNT
} Node_Kind;

/* ═══════════════════════════════════════════════════════════════════════════
 * §14  SYNTAX NODE — The AST Node Type
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Each node carries:
 *   - kind: syntactic category
 *   - location: source position
 *   - ty: resolved type (after semantic analysis)
 *   - symbol: defining occurrence (for references)
 *   - union: node-specific payload
 */

struct Syntax_Node {
    Node_Kind kind;
    Source_Location location;
    Type_Info *type;
    Symbol *symbol;
    union {
        /* Literals */
        String_Slice string_value;
        int64_t integer_value;
        double real_value;
        /* Binary operation: left OP right */
        struct { Token_Kind operator; Syntax_Node *left, *right; } binary;
        /* Unary operation: OP operand */
        struct { Token_Kind operator; Syntax_Node *operand; } unary;
        /* Attribute: prefix'name(args) */
        struct { Syntax_Node *prefix; String_Slice name; Node_Vector arguments; } attribute;
        /* Qualified: type'(aggregate) */
        struct { Syntax_Node *type_mark, *operand; } qualified;
        /* Call/Index: prefix(arguments) */
        struct { Syntax_Node *prefix; Node_Vector arguments; } apply;
        /* Slice: prefix(low..high) */
        struct { Syntax_Node *prefix, *low, *high; } slice;
        /* Selected: prefix.selector */
        struct { Syntax_Node *prefix; String_Slice selector; } selected;
        /* Allocator: new subtype[(init)] */
        struct { Syntax_Node *subtype, *initializer; } allocator;
        /* Range: low .. high */
        struct { Syntax_Node *low, *high; } range;
        /* Constraint: subtype or range with constraints */
        struct { Syntax_Node *subtype_mark, *range_constraint; Node_Vector index_constraints; } constraint;
        /* Component: name : type := init */
        struct { String_Slice name; Syntax_Node *type_mark, *initializer; uint32_t offset; } component;
        /* Variant: when choices => components */
        struct { Node_Vector choices, components; } variant;
        /* Variant part: case discriminant is variants */
        struct { Syntax_Node *discriminant; Node_Vector variants; } variant_part;
        /* Parameter: name : mode type := default */
        struct { String_Slice name; Syntax_Node *type_mark, *default_value; uint8_t mode; } parameter;
        /* Subprogram spec: name(params) return type */
        struct { String_Slice name; Node_Vector parameters; Syntax_Node *return_type; } subprogram;
        /* Body: spec + decls + stmts + handlers */
        struct { Syntax_Node *specification; Node_Vector declarations, statements, handlers; } body;
        /* Package spec: name + decls + private_decls */
        struct { String_Slice name; Node_Vector declarations, private_declarations; } package_spec;
        /* Package body: name + decls + stmts + handlers */
        struct { String_Slice name; Node_Vector declarations, statements, handlers; } package_body;
        /* Object decl: names : [constant] type := init */
        struct { Node_Vector names; Syntax_Node *type_mark, *initializer; bool is_constant; } object;
        /* Type decl: name is definition */
        struct { String_Slice name; Syntax_Node *definition; Node_Vector discriminants; bool is_new, is_derived; } type_decl;
        /* Subtype decl: name is constraint */
        struct { String_Slice name; Syntax_Node *constraint; } subtype_decl;
        /* Exception decl: names : exception [renames] */
        struct { Node_Vector names; Syntax_Node *renamed; } exception;
        /* Renaming: name renames entity */
        struct { String_Slice name; Syntax_Node *renamed; } renaming;
        /* Assignment: target := value */
        struct { Syntax_Node *target, *value; } assignment;
        /* If: condition then stmts [elsif] [else] */
        struct { Syntax_Node *condition; Node_Vector then_stmts, elsif_parts, else_stmts; } if_stmt;
        /* Case: expression is alternatives */
        struct { Syntax_Node *expression; Node_Vector alternatives; } case_stmt;
        /* Loop: [label] [iteration] loop stmts */
        struct { String_Slice label; Syntax_Node *iterator; bool is_reverse; Node_Vector statements; } loop_stmt;
        /* Block: [label] [declare decls] begin stmts [exception handlers] */
        struct { String_Slice label; Node_Vector declarations, statements, handlers; } block;
        /* Exit: [label] [when condition] */
        struct { String_Slice label; Syntax_Node *condition; } exit_stmt;
        /* Return: [expression] */
        struct { Syntax_Node *value; } return_stmt;
        /* Raise: [exception] */
        struct { Syntax_Node *exception_name; } raise_stmt;
        /* Handler: when choices => stmts */
        struct { Node_Vector exception_choices, statements; } handler;
        /* Association: [choices =>] value */
        struct { Node_Vector choices; Syntax_Node *value; } association;
        /* Aggregate: (items) */
        struct { Node_Vector items; } aggregate;
        /* Context: with/use clauses */
        struct { Node_Vector with_clauses, use_clauses; } context;
        /* With: package name */
        struct { String_Slice package_name; } with_clause;
        /* Use: type or package */
        struct { Syntax_Node *name; } use_clause;
        /* Pragma: name(args) */
        struct { String_Slice name; Node_Vector arguments; } pragma_node;
        /* Compilation unit: context + units */
        struct { Syntax_Node *context; Node_Vector units; } compilation_unit;
        /* Generic decl: formals + unit */
        struct { Node_Vector formals; Syntax_Node *unit; } generic_decl;
        /* Generic instantiation: name is new generic(actuals) */
        struct { String_Slice name, generic_name; Node_Vector actuals; } instantiation;
        /* Task spec/body */
        struct { String_Slice name; Node_Vector entries, declarations, statements, handlers; } task;
        /* Accept: entry(params) do stmts */
        struct { String_Slice name; Node_Vector index_constraints, parameters, statements, handlers; } accept;
        /* Select */
        struct { Node_Vector alternatives, else_statements; } select_stmt;
        /* Entry: name(family)(params) */
        struct { String_Slice name; Node_Vector family_index, parameters; } entry;
        /* List container */
        struct { Node_Vector items; } list;
        /* Conversion: type(expression) */
        struct { Syntax_Node *type_mark, *expression; } conversion;
        /* Dereference: expression.all */
        struct { Syntax_Node *expression; } dereference;
    };
};

/* Node constructor */
static Syntax_Node *node_create(Node_Kind kind, Source_Location loc) {
    Syntax_Node *n = arena_allocate(sizeof(Syntax_Node));
    n->kind = kind;
    n->location = loc;
    return n;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §15  TYPE SYSTEM — Ada Type Representation
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Types are classified by kind. Each kind has specific attributes.
 * All sizes are stored in BYTES for consistency.
 */

typedef enum {
    TYPE_VOID = 0, TYPE_BOOLEAN, TYPE_CHARACTER, TYPE_INTEGER, TYPE_FLOAT,
    TYPE_FIXED, TYPE_ENUMERATION, TYPE_ARRAY, TYPE_RECORD, TYPE_ACCESS,
    TYPE_STRING, TYPE_SUBPROGRAM, TYPE_TASK, TYPE_PRIVATE, TYPE_UNIVERSAL_INTEGER,
    TYPE_UNIVERSAL_REAL, TYPE_COUNT
} Type_Kind;

struct Type_Info {
    Type_Kind kind;
    String_Slice name;
    uint32_t size_bytes;       /* Size in bytes */
    uint32_t alignment_bytes;  /* Alignment in bytes */
    int64_t low_bound, high_bound;  /* For scalar types */
    Type_Info *base_type;      /* For subtypes and derived types */
    Type_Info *element_type;   /* For arrays and access */
    Type_Info *index_type;     /* For arrays */
    Type_Info *designated;     /* For access types */
    Node_Vector components;    /* For records */
    Node_Vector parameters;    /* For subprograms */
    Type_Info *return_type;    /* For functions */
    Symbol *type_symbol;       /* Defining symbol */
    uint32_t dimension_count;  /* For multi-dimensional arrays */
    bool is_constrained;
    bool is_limited;
    bool is_private;
};

static Type_Info *type_create(Type_Kind kind, String_Slice name) {
    Type_Info *t = arena_allocate(sizeof(Type_Info));
    t->kind = kind;
    t->name = name;
    t->size_bytes = 4;       /* Default: 32-bit */
    t->alignment_bytes = 4;
    return t;
}

/* Predefined types (initialized later) */
static Type_Info *Type_Boolean, *Type_Character, *Type_Integer, *Type_Float;
static Type_Info *Type_String, *Type_Universal_Integer, *Type_Universal_Real;

/* ═══════════════════════════════════════════════════════════════════════════
 * §16  SYMBOL TABLE — Names and Their Meanings
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Symbols represent named entities: variables, types, subprograms, packages.
 * The table uses hash chaining with case-insensitive lookup.
 */

typedef enum {
    SYMBOL_VARIABLE = 1, SYMBOL_CONSTANT, SYMBOL_TYPE, SYMBOL_SUBTYPE,
    SYMBOL_PROCEDURE, SYMBOL_FUNCTION, SYMBOL_PARAMETER, SYMBOL_PACKAGE,
    SYMBOL_EXCEPTION, SYMBOL_ENTRY, SYMBOL_LABEL, SYMBOL_COMPONENT,
    SYMBOL_ENUMERATION_LITERAL, SYMBOL_GENERIC, SYMBOL_LOOP_PARAMETER
} Symbol_Kind;

struct Symbol {
    Symbol_Kind kind;
    String_Slice name;
    Type_Info *type;
    Syntax_Node *declaration;  /* Defining occurrence */
    Symbol *parent;            /* Enclosing scope */
    Symbol *next_in_scope;     /* Hash chain */
    Symbol *overload_next;     /* Overload chain */
    uint32_t scope_level;
    uint32_t elaboration_order;
    uint8_t visibility;        /* 1=visible, 2=use-visible */
    uint8_t mode;              /* For parameters: in/out/in out */
    bool is_initialized;
    int64_t constant_value;    /* For numeric constants */
};

#define SYMBOL_TABLE_SIZE 4096

typedef struct {
    Symbol *buckets[SYMBOL_TABLE_SIZE];
    Symbol *current_scope;
    uint32_t scope_level;
    uint32_t next_elaboration_order;
    Symbol_Vector scope_stack;
} Symbol_Manager;

static Symbol *symbol_create(Symbol_Kind kind, String_Slice name) {
    Symbol *s = arena_allocate(sizeof(Symbol));
    s->kind = kind;
    s->name = name;
    s->visibility = 1;
    return s;
}

static uint32_t symbol_bucket(String_Slice name) {
    return string_hash(name) % SYMBOL_TABLE_SIZE;
}

static void symbol_insert(Symbol_Manager *M, Symbol *s) {
    uint32_t bucket = symbol_bucket(s->name);
    s->parent = M->current_scope;
    s->scope_level = M->scope_level;
    s->elaboration_order = M->next_elaboration_order++;
    s->next_in_scope = M->buckets[bucket];
    M->buckets[bucket] = s;
}

static Symbol *symbol_lookup(Symbol_Manager *M, String_Slice name) {
    uint32_t bucket = symbol_bucket(name);
    for (Symbol *s = M->buckets[bucket]; s; s = s->next_in_scope)
        if (string_equal_ignore_case(s->name, name) && s->visibility)
            return s;
    return NULL;
}

static void symbol_push_scope(Symbol_Manager *M, Symbol *scope_symbol) {
    Symbol_Vector_push(&M->scope_stack, M->current_scope);
    M->current_scope = scope_symbol;
    M->scope_level++;
}

static void symbol_pop_scope(Symbol_Manager *M) {
    if (M->scope_stack.count > 0)
        M->current_scope = M->scope_stack.items[--M->scope_stack.count];
    if (M->scope_level > 0) M->scope_level--;
}

/* Initialize predefined environment */
static void symbol_manager_initialize(Symbol_Manager *M) {
    memset(M, 0, sizeof(*M));

    /* Create predefined types */
    Type_Boolean = type_create(TYPE_BOOLEAN, S("Boolean"));
    Type_Boolean->low_bound = 0; Type_Boolean->high_bound = 1;
    Type_Boolean->size_bytes = 1;

    Type_Character = type_create(TYPE_CHARACTER, S("Character"));
    Type_Character->low_bound = 0; Type_Character->high_bound = 255;
    Type_Character->size_bytes = 1;

    Type_Integer = type_create(TYPE_INTEGER, S("Integer"));
    Type_Integer->low_bound = INT32_MIN; Type_Integer->high_bound = INT32_MAX;
    Type_Integer->size_bytes = 4;

    Type_Float = type_create(TYPE_FLOAT, S("Float"));
    Type_Float->size_bytes = 8;

    Type_String = type_create(TYPE_STRING, S("String"));
    Type_String->element_type = Type_Character;
    Type_String->size_bytes = 16;  /* Fat pointer */

    Type_Universal_Integer = type_create(TYPE_UNIVERSAL_INTEGER, S("universal_integer"));
    Type_Universal_Real = type_create(TYPE_UNIVERSAL_REAL, S("universal_real"));

    /* Register predefined types as symbols */
    Symbol *s;
    #define REGISTER_TYPE(T) \
        s = symbol_create(SYMBOL_TYPE, T->name); s->type = T; symbol_insert(M, s);
    REGISTER_TYPE(Type_Boolean)
    REGISTER_TYPE(Type_Character)
    REGISTER_TYPE(Type_Integer)
    REGISTER_TYPE(Type_Float)
    REGISTER_TYPE(Type_String)
    #undef REGISTER_TYPE

    /* Boolean literals */
    s = symbol_create(SYMBOL_ENUMERATION_LITERAL, S("False"));
    s->type = Type_Boolean; s->constant_value = 0; symbol_insert(M, s);
    s = symbol_create(SYMBOL_ENUMERATION_LITERAL, S("True"));
    s->type = Type_Boolean; s->constant_value = 1; symbol_insert(M, s);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §17  PARSER STATE — Tracking Parse Progress
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
    Lexer lexer;
    Token current, peek;
    int error_count;
    bool panic_mode;
} Parser;

static Parser parser_create(const char *source, size_t length, const char *filename) {
    Parser P = {lexer_create(source, length, filename), {0}, {0}, 0, false};
    P.current = lexer_next_token(&P.lexer);
    P.peek = lexer_next_token(&P.lexer);
    /* Handle compound tokens: AND THEN, OR ELSE */
    if (P.current.kind == TOKEN_AND && P.peek.kind == TOKEN_THEN) {
        P.current.kind = TOKEN_AND_THEN;
        P.peek = lexer_next_token(&P.lexer);
    }
    if (P.current.kind == TOKEN_OR && P.peek.kind == TOKEN_ELSE) {
        P.current.kind = TOKEN_OR_ELSE;
        P.peek = lexer_next_token(&P.lexer);
    }
    return P;
}

static void parser_advance(Parser *P) {
    P->current = P->peek;
    P->peek = lexer_next_token(&P->lexer);
    /* Handle compound tokens */
    if (P->current.kind == TOKEN_AND && P->peek.kind == TOKEN_THEN) {
        P->current.kind = TOKEN_AND_THEN;
        P->peek = lexer_next_token(&P->lexer);
    }
    if (P->current.kind == TOKEN_OR && P->peek.kind == TOKEN_ELSE) {
        P->current.kind = TOKEN_OR_ELSE;
        P->peek = lexer_next_token(&P->lexer);
    }
}

static bool parser_at(Parser *P, Token_Kind kind) { return P->current.kind == kind; }
static bool parser_match(Parser *P, Token_Kind kind) {
    if (!parser_at(P, kind)) return false;
    parser_advance(P);
    return true;
}

static Source_Location parser_location(Parser *P) { return P->current.location; }

static void parser_expect(Parser *P, Token_Kind expected) {
    if (P->current.kind == expected) { parser_advance(P); return; }
    report_error(P->current.location, "expected '%s', found '%s'",
                 Token_Names[expected], Token_Names[P->current.kind]);
    P->error_count++;
    /* Recovery: for closing delimiters, pretend they exist */
    if (expected == TOKEN_SEMICOLON || expected == TOKEN_RIGHT_PAREN ||
        expected == TOKEN_THEN || expected == TOKEN_IS || expected == TOKEN_LOOP)
        return;
    parser_advance(P);
}

static String_Slice parser_expect_identifier(Parser *P) {
    if (P->current.kind != TOKEN_IDENTIFIER) {
        report_error(P->current.location, "expected identifier");
        return EMPTY_STRING;
    }
    String_Slice name = P->current.text;
    parser_advance(P);
    return name;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §18  PARSER HELPERS — Unified Parsing Patterns
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * These helpers eliminate the code duplication identified in the analysis.
 * Each helper handles a common parsing pattern used in multiple places.
 */

/* Forward declarations for recursive descent */
static Syntax_Node *parse_expression(Parser *P);
static Syntax_Node *parse_name(Parser *P);
static Syntax_Node *parse_simple_name(Parser *P);
static Syntax_Node *parse_type_indication(Parser *P);
static Syntax_Node *parse_statement(Parser *P);
static Syntax_Node *parse_declaration(Parser *P);
static void parse_statement_sequence(Parser *P, Node_Vector *stmts);
static void parse_declarative_part(Parser *P, Node_Vector *decls);

/* Parse a comma-separated identifier list: A, B, C */
static void parse_identifier_list(Parser *P, Node_Vector *names) {
    do {
        Syntax_Node *id = node_create(NODE_IDENTIFIER, parser_location(P));
        id->string_value = parser_expect_identifier(P);
        Node_Vector_push(names, id);
    } while (parser_match(P, TOKEN_COMMA));
}

/* Parse an association list: (A, B => C, D => E) - used for calls, aggregates */
static void parse_association_list(Parser *P, Node_Vector *items, Token_Kind close) {
    if (parser_at(P, close)) return;
    do {
        Source_Location loc = parser_location(P);
        Syntax_Node *first = parse_expression(P);
        if (parser_match(P, TOKEN_ARROW)) {
            /* Named association: choices => value */
            Syntax_Node *assoc = node_create(NODE_ASSOCIATION, loc);
            Node_Vector_push(&assoc->association.choices, first);
            while (parser_match(P, TOKEN_BAR)) {
                Node_Vector_push(&assoc->association.choices, parse_expression(P));
            }
            assoc->association.value = parse_expression(P);
            Node_Vector_push(items, assoc);
        } else {
            /* Positional association */
            Node_Vector_push(items, first);
        }
    } while (parser_match(P, TOKEN_COMMA));
}

/* Parse exception handlers: exception when ... => ... */
static void parse_exception_handlers(Parser *P, Node_Vector *handlers) {
    if (!parser_match(P, TOKEN_EXCEPTION)) return;
    while (parser_at(P, TOKEN_WHEN)) {
        Source_Location loc = parser_location(P);
        parser_advance(P);  /* skip WHEN */
        Syntax_Node *handler = node_create(NODE_EXCEPTION_HANDLER, loc);
        /* Parse exception choices */
        do {
            if (parser_match(P, TOKEN_OTHERS)) {
                Syntax_Node *others = node_create(NODE_IDENTIFIER, loc);
                others->string_value = S("others");
                Node_Vector_push(&handler->handler.exception_choices, others);
            } else {
                Node_Vector_push(&handler->handler.exception_choices, parse_name(P));
            }
        } while (parser_match(P, TOKEN_BAR));
        parser_expect(P, TOKEN_ARROW);
        parse_statement_sequence(P, &handler->handler.statements);
        Node_Vector_push(handlers, handler);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §19  EXPRESSION PARSING — Operator Precedence
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ada precedence (low to high): or/and/xor, relational, +/-, */mod/rem, **, unary, primary
 */

static Syntax_Node *parse_primary(Parser *P) {
    Source_Location loc = parser_location(P);

    /* Numeric literals */
    if (parser_at(P, TOKEN_INTEGER)) {
        Syntax_Node *n = node_create(NODE_INTEGER_LITERAL, loc);
        n->integer_value = P->current.integer_value;
        parser_advance(P);
        return n;
    }
    if (parser_at(P, TOKEN_REAL)) {
        Syntax_Node *n = node_create(NODE_REAL_LITERAL, loc);
        n->real_value = P->current.real_value;
        parser_advance(P);
        return n;
    }
    if (parser_at(P, TOKEN_CHARACTER)) {
        Syntax_Node *n = node_create(NODE_CHARACTER_LITERAL, loc);
        n->integer_value = P->current.integer_value;
        n->string_value = P->current.text;
        parser_advance(P);
        return n;
    }
    if (parser_at(P, TOKEN_STRING)) {
        Syntax_Node *n = node_create(NODE_STRING_LITERAL, loc);
        n->string_value = P->current.text;
        parser_advance(P);
        return n;
    }
    if (parser_match(P, TOKEN_NULL)) return node_create(NODE_NULL_LITERAL, loc);

    /* Parenthesized expression or aggregate */
    if (parser_match(P, TOKEN_LEFT_PAREN)) {
        Syntax_Node *expr = parse_expression(P);
        if (parser_at(P, TOKEN_COMMA) || parser_at(P, TOKEN_ARROW) ||
            parser_at(P, TOKEN_BAR) || parser_at(P, TOKEN_WITH)) {
            /* Aggregate */
            Syntax_Node *agg = node_create(NODE_AGGREGATE, loc);
            Node_Vector_push(&agg->aggregate.items, expr);
            if (parser_at(P, TOKEN_ARROW)) {
                /* Re-parse as association */
                agg->aggregate.items.count = 0;
                Syntax_Node *assoc = node_create(NODE_ASSOCIATION, loc);
                Node_Vector_push(&assoc->association.choices, expr);
                parser_advance(P);  /* skip => */
                assoc->association.value = parse_expression(P);
                Node_Vector_push(&agg->aggregate.items, assoc);
            }
            while (parser_match(P, TOKEN_COMMA))
                parse_association_list(P, &agg->aggregate.items, TOKEN_RIGHT_PAREN);
            parser_expect(P, TOKEN_RIGHT_PAREN);
            return agg;
        }
        parser_expect(P, TOKEN_RIGHT_PAREN);
        return expr;
    }

    /* NEW allocator */
    if (parser_match(P, TOKEN_NEW)) {
        Syntax_Node *alloc = node_create(NODE_ALLOCATOR, loc);
        alloc->allocator.subtype = parse_type_indication(P);
        if (parser_match(P, TOKEN_TICK)) {
            parser_expect(P, TOKEN_LEFT_PAREN);
            alloc->allocator.initializer = parse_expression(P);
            parser_expect(P, TOKEN_RIGHT_PAREN);
        }
        return alloc;
    }

    /* Unary operators */
    if (parser_at(P, TOKEN_NOT) || parser_at(P, TOKEN_ABS)) {
        Token_Kind op = P->current.kind;
        parser_advance(P);
        Syntax_Node *n = node_create(NODE_UNARY_OPERATION, loc);
        n->unary.operator = op;
        n->unary.operand = parse_primary(P);
        return n;
    }

    /* Name (identifier, selected, indexed, etc.) */
    return parse_name(P);
}

/* Parse postfix: name.selector, name(args), name'attr */
static Syntax_Node *parse_name(Parser *P) {
    Syntax_Node *n = parse_simple_name(P);

    while (true) {
        Source_Location loc = parser_location(P);

        /* Dot selection or .all dereference */
        if (parser_match(P, TOKEN_DOT)) {
            if (parser_match(P, TOKEN_ALL)) {
                Syntax_Node *deref = node_create(NODE_DEREFERENCE, loc);
                deref->dereference.expression = n;
                n = deref;
            } else {
                Syntax_Node *sel = node_create(NODE_SELECTED_COMPONENT, loc);
                sel->selected.prefix = n;
                sel->selected.selector = parser_expect_identifier(P);
                n = sel;
            }
            continue;
        }

        /* Tick: attribute or qualified expression */
        if (parser_match(P, TOKEN_TICK)) {
            if (parser_match(P, TOKEN_LEFT_PAREN)) {
                /* Qualified expression: Type'(expr) */
                Syntax_Node *qual = node_create(NODE_QUALIFIED_EXPRESSION, loc);
                qual->qualified.type_mark = n;
                qual->qualified.operand = parse_expression(P);
                parser_expect(P, TOKEN_RIGHT_PAREN);
                n = qual;
            } else {
                /* Attribute */
                Syntax_Node *attr = node_create(NODE_ATTRIBUTE, loc);
                attr->attribute.prefix = n;
                attr->attribute.name = parser_expect_identifier(P);
                if (parser_match(P, TOKEN_LEFT_PAREN)) {
                    parse_association_list(P, &attr->attribute.arguments, TOKEN_RIGHT_PAREN);
                    parser_expect(P, TOKEN_RIGHT_PAREN);
                }
                n = attr;
            }
            continue;
        }

        /* Parentheses: call, index, or slice */
        if (parser_match(P, TOKEN_LEFT_PAREN)) {
            Syntax_Node *first = parse_expression(P);
            if (parser_match(P, TOKEN_DOUBLE_DOT)) {
                /* Slice */
                Syntax_Node *slice = node_create(NODE_SLICE, loc);
                slice->slice.prefix = n;
                slice->slice.low = first;
                slice->slice.high = parse_expression(P);
                parser_expect(P, TOKEN_RIGHT_PAREN);
                n = slice;
            } else {
                /* Call or index (resolved later) */
                Syntax_Node *apply = node_create(NODE_FUNCTION_CALL, loc);
                apply->apply.prefix = n;
                Node_Vector_push(&apply->apply.arguments, first);
                while (parser_match(P, TOKEN_COMMA)) {
                    if (parser_at(P, TOKEN_RIGHT_PAREN)) break;
                    Node_Vector_push(&apply->apply.arguments, parse_expression(P));
                }
                parser_expect(P, TOKEN_RIGHT_PAREN);
                n = apply;
            }
            continue;
        }

        break;
    }
    return n;
}

static Syntax_Node *parse_simple_name(Parser *P) {
    Source_Location loc = parser_location(P);
    Syntax_Node *n = node_create(NODE_IDENTIFIER, loc);
    n->string_value = parser_expect_identifier(P);
    return n;
}

/* Binary operator precedence parsing using Pratt-style */
static int precedence(Token_Kind op) {
    switch (op) {
    case TOKEN_OR: case TOKEN_OR_ELSE: case TOKEN_XOR: return 1;
    case TOKEN_AND: case TOKEN_AND_THEN: return 2;
    case TOKEN_EQUAL: case TOKEN_NOT_EQUAL: case TOKEN_LESS: case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER: case TOKEN_GREATER_EQUAL: case TOKEN_IN: case TOKEN_NOT: return 3;
    case TOKEN_PLUS: case TOKEN_MINUS: case TOKEN_AMPERSAND: return 4;
    case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_MOD: case TOKEN_REM: return 5;
    case TOKEN_POWER: return 6;
    default: return 0;
    }
}

static bool is_binary_operator(Token_Kind k) { return precedence(k) > 0; }

static Syntax_Node *parse_expression_prec(Parser *P, int min_prec) {
    /* Handle leading unary +/- */
    Source_Location loc = parser_location(P);
    Syntax_Node *left;
    if ((parser_at(P, TOKEN_PLUS) || parser_at(P, TOKEN_MINUS)) && min_prec <= 4) {
        Token_Kind op = P->current.kind;
        parser_advance(P);
        left = node_create(NODE_UNARY_OPERATION, loc);
        left->unary.operator = op;
        left->unary.operand = parse_expression_prec(P, 5);
    } else {
        left = parse_primary(P);
    }

    while (is_binary_operator(P->current.kind) && precedence(P->current.kind) >= min_prec) {
        Token_Kind op = P->current.kind;
        int prec = precedence(op);
        loc = parser_location(P);
        parser_advance(P);

        /* Handle NOT IN as membership test */
        bool is_not_in = (op == TOKEN_NOT && parser_match(P, TOKEN_IN));
        if (is_not_in) op = TOKEN_NOT;  /* Flag for NOT IN */

        /* Right associativity for ** */
        int next_prec = (op == TOKEN_POWER) ? prec : prec + 1;
        Syntax_Node *right = parse_expression_prec(P, next_prec);

        Syntax_Node *bin = node_create(NODE_BINARY_OPERATION, loc);
        bin->binary.operator = op;
        bin->binary.left = left;
        bin->binary.right = right;
        left = bin;
    }
    return left;
}

static Syntax_Node *parse_expression(Parser *P) {
    return parse_expression_prec(P, 1);
}

/* Parse range: low .. high */
static Syntax_Node *parse_range(Parser *P) {
    Source_Location loc = parser_location(P);
    if (parser_match(P, TOKEN_BOX)) return node_create(NODE_RANGE, loc);  /* <> */
    Syntax_Node *low = parse_expression(P);
    if (!parser_match(P, TOKEN_DOUBLE_DOT)) return low;
    Syntax_Node *rng = node_create(NODE_RANGE, loc);
    rng->range.low = low;
    rng->range.high = parse_expression(P);
    return rng;
}

static Syntax_Node *parse_type_indication(Parser *P) {
    return parse_name(P);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §20  STATEMENT PARSING — Executable Constructs
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *parse_if_statement(Parser *P) {
    Source_Location loc = parser_location(P);
    parser_expect(P, TOKEN_IF);
    Syntax_Node *n = node_create(NODE_IF_STATEMENT, loc);
    n->if_stmt.condition = parse_expression(P);
    parser_expect(P, TOKEN_THEN);
    parse_statement_sequence(P, &n->if_stmt.then_stmts);
    while (parser_match(P, TOKEN_ELSIF)) {
        Syntax_Node *elsif = node_create(NODE_IF_STATEMENT, parser_location(P));
        elsif->if_stmt.condition = parse_expression(P);
        parser_expect(P, TOKEN_THEN);
        parse_statement_sequence(P, &elsif->if_stmt.then_stmts);
        Node_Vector_push(&n->if_stmt.elsif_parts, elsif);
    }
    if (parser_match(P, TOKEN_ELSE))
        parse_statement_sequence(P, &n->if_stmt.else_stmts);
    parser_expect(P, TOKEN_END);
    parser_expect(P, TOKEN_IF);
    parser_expect(P, TOKEN_SEMICOLON);
    return n;
}

static Syntax_Node *parse_case_statement(Parser *P) {
    Source_Location loc = parser_location(P);
    parser_expect(P, TOKEN_CASE);
    Syntax_Node *n = node_create(NODE_CASE_STATEMENT, loc);
    n->case_stmt.expression = parse_expression(P);
    parser_expect(P, TOKEN_IS);
    while (parser_match(P, TOKEN_WHEN)) {
        Syntax_Node *alt = node_create(NODE_ASSOCIATION, parser_location(P));
        do {
            if (parser_match(P, TOKEN_OTHERS)) {
                Syntax_Node *o = node_create(NODE_IDENTIFIER, parser_location(P));
                o->string_value = S("others");
                Node_Vector_push(&alt->association.choices, o);
            } else {
                Node_Vector_push(&alt->association.choices, parse_range(P));
            }
        } while (parser_match(P, TOKEN_BAR));
        parser_expect(P, TOKEN_ARROW);
        Node_Vector stmts = {0};
        parse_statement_sequence(P, &stmts);
        Syntax_Node *list = node_create(NODE_LIST, loc);
        list->list.items = stmts;
        alt->association.value = list;
        Node_Vector_push(&n->case_stmt.alternatives, alt);
    }
    parser_expect(P, TOKEN_END);
    parser_expect(P, TOKEN_CASE);
    parser_expect(P, TOKEN_SEMICOLON);
    return n;
}

static Syntax_Node *parse_loop_statement(Parser *P, String_Slice label) {
    Source_Location loc = parser_location(P);
    Syntax_Node *n = node_create(NODE_LOOP_STATEMENT, loc);
    n->loop_stmt.label = label;

    if (parser_match(P, TOKEN_WHILE)) {
        n->loop_stmt.iterator = parse_expression(P);
    } else if (parser_match(P, TOKEN_FOR)) {
        Syntax_Node *iter = node_create(NODE_BINARY_OPERATION, parser_location(P));
        iter->binary.operator = TOKEN_IN;
        iter->binary.left = parse_simple_name(P);
        parser_expect(P, TOKEN_IN);
        n->loop_stmt.is_reverse = parser_match(P, TOKEN_REVERSE);
        iter->binary.right = parse_range(P);
        n->loop_stmt.iterator = iter;
    }
    parser_expect(P, TOKEN_LOOP);
    parse_statement_sequence(P, &n->loop_stmt.statements);
    parser_expect(P, TOKEN_END);
    parser_expect(P, TOKEN_LOOP);
    if (parser_at(P, TOKEN_IDENTIFIER)) parser_advance(P);  /* optional label */
    parser_expect(P, TOKEN_SEMICOLON);
    return n;
}

static Syntax_Node *parse_block_statement(Parser *P, String_Slice label) {
    Source_Location loc = parser_location(P);
    Syntax_Node *n = node_create(NODE_BLOCK_STATEMENT, loc);
    n->block.label = label;
    if (parser_match(P, TOKEN_DECLARE))
        parse_declarative_part(P, &n->block.declarations);
    parser_expect(P, TOKEN_BEGIN);
    parse_statement_sequence(P, &n->block.statements);
    parse_exception_handlers(P, &n->block.handlers);
    parser_expect(P, TOKEN_END);
    if (parser_at(P, TOKEN_IDENTIFIER)) parser_advance(P);
    parser_expect(P, TOKEN_SEMICOLON);
    return n;
}

static Syntax_Node *parse_statement(Parser *P) {
    Source_Location loc = parser_location(P);
    String_Slice label = EMPTY_STRING;

    /* Check for label: <<label>> or name: */
    if (parser_match(P, TOKEN_LEFT_LABEL)) {
        label = parser_expect_identifier(P);
        parser_expect(P, TOKEN_RIGHT_LABEL);
    } else if (parser_at(P, TOKEN_IDENTIFIER) && P->peek.kind == TOKEN_COLON) {
        label = P->current.text;
        parser_advance(P);
        parser_advance(P);
    }

    if (parser_match(P, TOKEN_NULL)) {
        parser_expect(P, TOKEN_SEMICOLON);
        return node_create(NODE_NULL_STATEMENT, loc);
    }
    if (parser_at(P, TOKEN_IF)) return parse_if_statement(P);
    if (parser_at(P, TOKEN_CASE)) return parse_case_statement(P);
    if (parser_at(P, TOKEN_LOOP) || parser_at(P, TOKEN_WHILE) || parser_at(P, TOKEN_FOR))
        return parse_loop_statement(P, label);
    if (parser_at(P, TOKEN_DECLARE) || parser_at(P, TOKEN_BEGIN))
        return parse_block_statement(P, label);

    if (parser_match(P, TOKEN_RETURN)) {
        Syntax_Node *n = node_create(NODE_RETURN_STATEMENT, loc);
        if (!parser_at(P, TOKEN_SEMICOLON))
            n->return_stmt.value = parse_expression(P);
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }
    if (parser_match(P, TOKEN_EXIT)) {
        Syntax_Node *n = node_create(NODE_EXIT_STATEMENT, loc);
        if (parser_at(P, TOKEN_IDENTIFIER)) n->exit_stmt.label = parser_expect_identifier(P);
        if (parser_match(P, TOKEN_WHEN)) n->exit_stmt.condition = parse_expression(P);
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }
    if (parser_match(P, TOKEN_GOTO)) {
        Syntax_Node *n = node_create(NODE_GOTO_STATEMENT, loc);
        n->string_value = parser_expect_identifier(P);
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }
    if (parser_match(P, TOKEN_RAISE)) {
        Syntax_Node *n = node_create(NODE_RAISE_STATEMENT, loc);
        if (!parser_at(P, TOKEN_SEMICOLON)) n->raise_stmt.exception_name = parse_name(P);
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }

    /* Assignment or procedure call */
    Syntax_Node *target = parse_name(P);
    if (parser_match(P, TOKEN_ASSIGN)) {
        Syntax_Node *n = node_create(NODE_ASSIGNMENT_STATEMENT, loc);
        n->assignment.target = target;
        n->assignment.value = parse_expression(P);
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }
    /* Procedure call (already parsed as name with optional args) */
    Syntax_Node *n = node_create(NODE_PROCEDURE_CALL_STATEMENT, loc);
    n->apply.prefix = target;
    parser_expect(P, TOKEN_SEMICOLON);
    return n;
}

static void parse_statement_sequence(Parser *P, Node_Vector *stmts) {
    while (!parser_at(P, TOKEN_END) && !parser_at(P, TOKEN_ELSIF) &&
           !parser_at(P, TOKEN_ELSE) && !parser_at(P, TOKEN_WHEN) &&
           !parser_at(P, TOKEN_EXCEPTION) && !parser_at(P, TOKEN_END_OF_FILE)) {
        Node_Vector_push(stmts, parse_statement(P));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §21  DECLARATION PARSING — Declarative Constructs
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *parse_parameter_list(Parser *P) {
    if (!parser_match(P, TOKEN_LEFT_PAREN)) return NULL;
    Syntax_Node *list = node_create(NODE_LIST, parser_location(P));
    do {
        Source_Location loc = parser_location(P);
        Syntax_Node *param = node_create(NODE_PARAMETER_SPECIFICATION, loc);
        Node_Vector names = {0};
        parse_identifier_list(P, &names);
        param->parameter.name = names.count > 0 ? names.items[0]->string_value : EMPTY_STRING;
        parser_expect(P, TOKEN_COLON);
        /* Mode: in, out, in out */
        if (parser_match(P, TOKEN_IN)) {
            param->parameter.mode = parser_match(P, TOKEN_OUT) ? 3 : 1;
        } else if (parser_match(P, TOKEN_OUT)) {
            param->parameter.mode = 2;
        } else {
            param->parameter.mode = 1;  /* Default: in */
        }
        param->parameter.type_mark = parse_type_indication(P);
        if (parser_match(P, TOKEN_ASSIGN))
            param->parameter.default_value = parse_expression(P);
        /* Push one param per name */
        for (uint32_t i = 0; i < names.count; i++) {
            Syntax_Node *p = (i == 0) ? param : node_create(NODE_PARAMETER_SPECIFICATION, loc);
            if (i > 0) *p = *param;
            p->parameter.name = names.items[i]->string_value;
            Node_Vector_push(&list->list.items, p);
        }
    } while (parser_match(P, TOKEN_SEMICOLON) && !parser_at(P, TOKEN_RIGHT_PAREN));
    parser_expect(P, TOKEN_RIGHT_PAREN);
    return list;
}

static Syntax_Node *parse_subprogram_specification(Parser *P, bool is_function) {
    Source_Location loc = parser_location(P);
    parser_advance(P);  /* skip PROCEDURE/FUNCTION */
    Syntax_Node *spec = node_create(is_function ? NODE_FUNCTION_SPECIFICATION : NODE_PROCEDURE_SPECIFICATION, loc);
    spec->subprogram.name = parser_expect_identifier(P);
    Syntax_Node *params = parse_parameter_list(P);
    if (params) spec->subprogram.parameters = params->list.items;
    if (is_function) {
        parser_expect(P, TOKEN_RETURN);
        spec->subprogram.return_type = parse_type_indication(P);
    }
    return spec;
}

static Syntax_Node *parse_subprogram_body(Parser *P, Syntax_Node *spec) {
    Source_Location loc = spec->location;
    Syntax_Node *body = node_create(spec->kind == NODE_FUNCTION_SPECIFICATION ?
                                    NODE_FUNCTION_BODY : NODE_PROCEDURE_BODY, loc);
    body->body.specification = spec;
    parser_expect(P, TOKEN_IS);
    if (parser_match(P, TOKEN_SEPARATE)) {
        parser_expect(P, TOKEN_SEMICOLON);
        return body;  /* Stub */
    }
    parse_declarative_part(P, &body->body.declarations);
    parser_expect(P, TOKEN_BEGIN);
    parse_statement_sequence(P, &body->body.statements);
    parse_exception_handlers(P, &body->body.handlers);
    parser_expect(P, TOKEN_END);
    if (parser_at(P, TOKEN_IDENTIFIER)) parser_advance(P);
    parser_expect(P, TOKEN_SEMICOLON);
    return body;
}

static Syntax_Node *parse_type_definition(Parser *P) {
    Source_Location loc = parser_location(P);

    /* Enumeration: (A, B, C) */
    if (parser_match(P, TOKEN_LEFT_PAREN)) {
        Syntax_Node *n = node_create(NODE_ENUMERATION_TYPE, loc);
        do {
            Syntax_Node *lit = node_create(NODE_IDENTIFIER, parser_location(P));
            if (parser_at(P, TOKEN_CHARACTER)) {
                lit->string_value = P->current.text;
                parser_advance(P);
            } else {
                lit->string_value = parser_expect_identifier(P);
            }
            Node_Vector_push(&n->aggregate.items, lit);
        } while (parser_match(P, TOKEN_COMMA));
        parser_expect(P, TOKEN_RIGHT_PAREN);
        return n;
    }

    /* Range: range low .. high */
    if (parser_match(P, TOKEN_RANGE)) {
        Syntax_Node *n = node_create(NODE_INTEGER_TYPE, loc);
        n->range.low = parse_expression(P);
        parser_expect(P, TOKEN_DOUBLE_DOT);
        n->range.high = parse_expression(P);
        return n;
    }

    /* Array */
    if (parser_match(P, TOKEN_ARRAY)) {
        Syntax_Node *n = node_create(NODE_ARRAY_TYPE, loc);
        parser_expect(P, TOKEN_LEFT_PAREN);
        do {
            Node_Vector_push(&n->constraint.index_constraints, parse_type_indication(P));
        } while (parser_match(P, TOKEN_COMMA));
        parser_expect(P, TOKEN_RIGHT_PAREN);
        parser_expect(P, TOKEN_OF);
        n->constraint.subtype_mark = parse_type_indication(P);
        return n;
    }

    /* Record */
    if (parser_match(P, TOKEN_RECORD)) {
        Syntax_Node *n = node_create(NODE_RECORD_TYPE, loc);
        while (!parser_at(P, TOKEN_END) && !parser_at(P, TOKEN_CASE)) {
            if (parser_match(P, TOKEN_NULL)) { parser_expect(P, TOKEN_SEMICOLON); continue; }
            Source_Location cloc = parser_location(P);
            Node_Vector names = {0};
            parse_identifier_list(P, &names);
            parser_expect(P, TOKEN_COLON);
            Syntax_Node *ty = parse_type_indication(P);
            Syntax_Node *init = parser_match(P, TOKEN_ASSIGN) ? parse_expression(P) : NULL;
            parser_expect(P, TOKEN_SEMICOLON);
            for (uint32_t i = 0; i < names.count; i++) {
                Syntax_Node *comp = node_create(NODE_COMPONENT_DECLARATION, cloc);
                comp->component.name = names.items[i]->string_value;
                comp->component.type_mark = ty;
                comp->component.initializer = init;
                Node_Vector_push(&n->aggregate.items, comp);
            }
        }
        parser_expect(P, TOKEN_END);
        parser_expect(P, TOKEN_RECORD);
        return n;
    }

    /* Access */
    if (parser_match(P, TOKEN_ACCESS)) {
        Syntax_Node *n = node_create(NODE_ACCESS_TYPE, loc);
        n->constraint.subtype_mark = parse_type_indication(P);
        return n;
    }

    /* Private/Limited */
    if (parser_match(P, TOKEN_LIMITED)) {
        parser_match(P, TOKEN_PRIVATE);
        return node_create(NODE_PRIVATE_TYPE, loc);
    }
    if (parser_match(P, TOKEN_PRIVATE))
        return node_create(NODE_PRIVATE_TYPE, loc);

    /* New (derived type) */
    if (parser_match(P, TOKEN_NEW)) {
        Syntax_Node *n = node_create(NODE_DERIVED_TYPE, loc);
        n->constraint.subtype_mark = parse_type_indication(P);
        return n;
    }

    return parse_type_indication(P);
}

static Syntax_Node *parse_declaration(Parser *P) {
    Source_Location loc = parser_location(P);

    /* Type declaration */
    if (parser_match(P, TOKEN_TYPE)) {
        Syntax_Node *n = node_create(NODE_TYPE_DECLARATION, loc);
        n->type_decl.name = parser_expect_identifier(P);
        if (parser_match(P, TOKEN_IS)) {
            n->type_decl.definition = parse_type_definition(P);
        }
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }

    /* Subtype declaration */
    if (parser_match(P, TOKEN_SUBTYPE)) {
        Syntax_Node *n = node_create(NODE_SUBTYPE_DECLARATION, loc);
        n->subtype_decl.name = parser_expect_identifier(P);
        parser_expect(P, TOKEN_IS);
        n->subtype_decl.constraint = parse_type_indication(P);
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }

    /* Procedure/Function */
    if (parser_at(P, TOKEN_PROCEDURE)) {
        Syntax_Node *spec = parse_subprogram_specification(P, false);
        if (parser_at(P, TOKEN_IS))
            return parse_subprogram_body(P, spec);
        parser_expect(P, TOKEN_SEMICOLON);
        Syntax_Node *decl = node_create(NODE_PROCEDURE_DECLARATION, loc);
        decl->body.specification = spec;
        return decl;
    }
    if (parser_at(P, TOKEN_FUNCTION)) {
        Syntax_Node *spec = parse_subprogram_specification(P, true);
        if (parser_at(P, TOKEN_IS))
            return parse_subprogram_body(P, spec);
        parser_expect(P, TOKEN_SEMICOLON);
        Syntax_Node *decl = node_create(NODE_FUNCTION_DECLARATION, loc);
        decl->body.specification = spec;
        return decl;
    }

    /* Package */
    if (parser_match(P, TOKEN_PACKAGE)) {
        if (parser_match(P, TOKEN_BODY)) {
            Syntax_Node *n = node_create(NODE_PACKAGE_BODY, loc);
            n->package_body.name = parser_expect_identifier(P);
            parser_expect(P, TOKEN_IS);
            parse_declarative_part(P, &n->package_body.declarations);
            if (parser_match(P, TOKEN_BEGIN))
                parse_statement_sequence(P, &n->package_body.statements);
            parse_exception_handlers(P, &n->package_body.handlers);
            parser_expect(P, TOKEN_END);
            if (parser_at(P, TOKEN_IDENTIFIER)) parser_advance(P);
            parser_expect(P, TOKEN_SEMICOLON);
            return n;
        }
        Syntax_Node *n = node_create(NODE_PACKAGE_SPECIFICATION, loc);
        n->package_spec.name = parser_expect_identifier(P);
        parser_expect(P, TOKEN_IS);
        parse_declarative_part(P, &n->package_spec.declarations);
        if (parser_match(P, TOKEN_PRIVATE))
            parse_declarative_part(P, &n->package_spec.private_declarations);
        parser_expect(P, TOKEN_END);
        if (parser_at(P, TOKEN_IDENTIFIER)) parser_advance(P);
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }

    /* Use clause */
    if (parser_match(P, TOKEN_USE)) {
        Syntax_Node *n = node_create(NODE_USE_CLAUSE, loc);
        n->use_clause.name = parse_name(P);
        while (parser_match(P, TOKEN_COMMA)) {
            Syntax_Node *u = node_create(NODE_USE_CLAUSE, parser_location(P));
            u->use_clause.name = parse_name(P);
            /* Ignore additional use clauses in single node for simplicity */
        }
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }

    /* Pragma */
    if (parser_match(P, TOKEN_PRAGMA)) {
        Syntax_Node *n = node_create(NODE_PRAGMA, loc);
        n->pragma_node.name = parser_expect_identifier(P);
        if (parser_match(P, TOKEN_LEFT_PAREN)) {
            parse_association_list(P, &n->pragma_node.arguments, TOKEN_RIGHT_PAREN);
            parser_expect(P, TOKEN_RIGHT_PAREN);
        }
        parser_expect(P, TOKEN_SEMICOLON);
        return n;
    }

    /* Object declaration: A, B : [constant] Type [:= init]; */
    Node_Vector names = {0};
    parse_identifier_list(P, &names);
    parser_expect(P, TOKEN_COLON);
    bool is_constant = parser_match(P, TOKEN_CONSTANT);
    Syntax_Node *n = node_create(NODE_OBJECT_DECLARATION, loc);
    n->object.names = names;
    n->object.is_constant = is_constant;
    if (parser_match(P, TOKEN_EXCEPTION)) {
        /* Exception declaration */
        n->kind = NODE_EXCEPTION_DECLARATION;
    } else {
        n->object.type_mark = parse_type_indication(P);
        if (parser_match(P, TOKEN_ASSIGN))
            n->object.initializer = parse_expression(P);
    }
    parser_expect(P, TOKEN_SEMICOLON);
    return n;
}

static void parse_declarative_part(Parser *P, Node_Vector *decls) {
    while (!parser_at(P, TOKEN_BEGIN) && !parser_at(P, TOKEN_END) &&
           !parser_at(P, TOKEN_PRIVATE) && !parser_at(P, TOKEN_EXCEPTION) &&
           !parser_at(P, TOKEN_END_OF_FILE)) {
        Node_Vector_push(decls, parse_declaration(P));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §22  COMPILATION UNIT PARSING — Top Level
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *parse_context_clause(Parser *P) {
    Source_Location loc = parser_location(P);
    Syntax_Node *ctx = node_create(NODE_CONTEXT_CLAUSE, loc);
    while (parser_at(P, TOKEN_WITH) || parser_at(P, TOKEN_USE) || parser_at(P, TOKEN_PRAGMA)) {
        if (parser_match(P, TOKEN_WITH)) {
            do {
                Syntax_Node *w = node_create(NODE_WITH_CLAUSE, parser_location(P));
                w->with_clause.package_name = parser_expect_identifier(P);
                Node_Vector_push(&ctx->context.with_clauses, w);
            } while (parser_match(P, TOKEN_COMMA));
            parser_expect(P, TOKEN_SEMICOLON);
        } else if (parser_match(P, TOKEN_USE)) {
            do {
                Syntax_Node *u = node_create(NODE_USE_CLAUSE, parser_location(P));
                u->use_clause.name = parse_name(P);
                Node_Vector_push(&ctx->context.use_clauses, u);
            } while (parser_match(P, TOKEN_COMMA));
            parser_expect(P, TOKEN_SEMICOLON);
        } else if (parser_at(P, TOKEN_PRAGMA)) {
            parse_declaration(P);  /* Pragma */
        }
    }
    return ctx;
}

static Syntax_Node *parse_compilation_unit(Parser *P) {
    Source_Location loc = parser_location(P);
    Syntax_Node *unit = node_create(NODE_COMPILATION_UNIT, loc);
    unit->compilation_unit.context = parse_context_clause(P);
    while (!parser_at(P, TOKEN_END_OF_FILE)) {
        Node_Vector_push(&unit->compilation_unit.units, parse_declaration(P));
    }
    return unit;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §23  SEMANTIC ANALYSIS — Type Checking and Resolution
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Type_Info *resolve_expression(Symbol_Manager *M, Syntax_Node *n, Type_Info *expected);
static void resolve_statement(Symbol_Manager *M, Syntax_Node *n);
static void resolve_declaration(Symbol_Manager *M, Syntax_Node *n);

static Type_Info *resolve_expression(Symbol_Manager *M, Syntax_Node *n, Type_Info *expected) {
    if (!n) return NULL;
    switch (n->kind) {
    case NODE_INTEGER_LITERAL: n->type = expected ? expected : Type_Universal_Integer; break;
    case NODE_REAL_LITERAL: n->type = expected ? expected : Type_Universal_Real; break;
    case NODE_CHARACTER_LITERAL: n->type = Type_Character; break;
    case NODE_STRING_LITERAL: n->type = Type_String; break;
    case NODE_NULL_LITERAL: n->type = expected; break;
    case NODE_IDENTIFIER: {
        Symbol *s = symbol_lookup(M, n->string_value);
        if (!s) { report_error(n->location, "undefined: %.*s", n->string_value.length, n->string_value.text); n->type = Type_Integer; }
        else { n->symbol = s; n->type = s->type; if (s->kind == SYMBOL_ENUMERATION_LITERAL) { n->kind = NODE_INTEGER_LITERAL; n->integer_value = s->constant_value; } }
        break;
    }
    case NODE_BINARY_OPERATION: {
        Type_Info *lt = resolve_expression(M, n->binary.left, NULL);
        resolve_expression(M, n->binary.right, lt);
        Token_Kind op = n->binary.operator;
        n->type = (op >= TOKEN_EQUAL && op <= TOKEN_GREATER_EQUAL) || op == TOKEN_AND || op == TOKEN_OR ? Type_Boolean : lt;
        break;
    }
    case NODE_UNARY_OPERATION:
        n->type = resolve_expression(M, n->unary.operand, expected);
        if (n->unary.operator == TOKEN_NOT) n->type = Type_Boolean;
        break;
    case NODE_FUNCTION_CALL:
        resolve_expression(M, n->apply.prefix, NULL);
        for (uint32_t i = 0; i < n->apply.arguments.count; i++) resolve_expression(M, n->apply.arguments.items[i], NULL);
        if (n->apply.prefix->type && n->apply.prefix->type->kind == TYPE_ARRAY) { n->kind = NODE_INDEXED_COMPONENT; n->type = n->apply.prefix->type->element_type; }
        else n->type = n->apply.prefix->type ? n->apply.prefix->type->return_type : NULL;
        break;
    case NODE_SELECTED_COMPONENT:
        resolve_expression(M, n->selected.prefix, NULL);
        n->type = n->selected.prefix->type;
        break;
    case NODE_ATTRIBUTE:
        resolve_expression(M, n->attribute.prefix, NULL);
        n->type = string_equal_ignore_case(n->attribute.name, S("First")) || string_equal_ignore_case(n->attribute.name, S("Last")) ? Type_Integer : n->attribute.prefix->type;
        break;
    case NODE_AGGREGATE:
        for (uint32_t i = 0; i < n->aggregate.items.count; i++) resolve_expression(M, n->aggregate.items.items[i], NULL);
        n->type = expected;
        break;
    default: break;
    }
    return n->type;
}

static void resolve_statement(Symbol_Manager *M, Syntax_Node *n) {
    if (!n) return;
    switch (n->kind) {
    case NODE_ASSIGNMENT_STATEMENT:
        resolve_expression(M, n->assignment.target, NULL);
        resolve_expression(M, n->assignment.value, n->assignment.target->type);
        break;
    case NODE_IF_STATEMENT:
        resolve_expression(M, n->if_stmt.condition, Type_Boolean);
        for (uint32_t i = 0; i < n->if_stmt.then_stmts.count; i++) resolve_statement(M, n->if_stmt.then_stmts.items[i]);
        for (uint32_t i = 0; i < n->if_stmt.else_stmts.count; i++) resolve_statement(M, n->if_stmt.else_stmts.items[i]);
        break;
    case NODE_LOOP_STATEMENT:
        if (n->loop_stmt.iterator) resolve_expression(M, n->loop_stmt.iterator, NULL);
        for (uint32_t i = 0; i < n->loop_stmt.statements.count; i++) resolve_statement(M, n->loop_stmt.statements.items[i]);
        break;
    case NODE_BLOCK_STATEMENT:
        for (uint32_t i = 0; i < n->block.declarations.count; i++) resolve_declaration(M, n->block.declarations.items[i]);
        for (uint32_t i = 0; i < n->block.statements.count; i++) resolve_statement(M, n->block.statements.items[i]);
        break;
    case NODE_RETURN_STATEMENT: if (n->return_stmt.value) resolve_expression(M, n->return_stmt.value, NULL); break;
    case NODE_PROCEDURE_CALL_STATEMENT: resolve_expression(M, n->apply.prefix, NULL); break;
    default: break;
    }
}

static void resolve_declaration(Symbol_Manager *M, Syntax_Node *n) {
    if (!n) return;
    switch (n->kind) {
    case NODE_OBJECT_DECLARATION:
        resolve_expression(M, n->object.type_mark, NULL);
        if (n->object.initializer) resolve_expression(M, n->object.initializer, n->object.type_mark ? n->object.type_mark->type : NULL);
        for (uint32_t i = 0; i < n->object.names.count; i++) {
            Symbol *s = symbol_create(n->object.is_constant ? SYMBOL_CONSTANT : SYMBOL_VARIABLE, n->object.names.items[i]->string_value);
            s->type = n->object.type_mark ? n->object.type_mark->type : Type_Integer;
            symbol_insert(M, s);
        }
        break;
    case NODE_TYPE_DECLARATION: {
        Type_Info *t = type_create(TYPE_INTEGER, n->type_decl.name);
        if (n->type_decl.definition && n->type_decl.definition->kind == NODE_ENUMERATION_TYPE) {
            t->kind = TYPE_ENUMERATION;
            for (uint32_t i = 0; i < n->type_decl.definition->aggregate.items.count; i++) {
                Symbol *ls = symbol_create(SYMBOL_ENUMERATION_LITERAL, n->type_decl.definition->aggregate.items.items[i]->string_value);
                ls->type = t; ls->constant_value = i; symbol_insert(M, ls);
            }
        }
        Symbol *s = symbol_create(SYMBOL_TYPE, n->type_decl.name);
        s->type = t; symbol_insert(M, s);
        break;
    }
    case NODE_PROCEDURE_BODY: case NODE_FUNCTION_BODY: {
        Syntax_Node *spec = n->body.specification;
        Symbol *s = symbol_create(n->kind == NODE_FUNCTION_BODY ? SYMBOL_FUNCTION : SYMBOL_PROCEDURE, spec->subprogram.name);
        Type_Info *ft = type_create(TYPE_SUBPROGRAM, spec->subprogram.name);
        if (spec->subprogram.return_type) { resolve_expression(M, spec->subprogram.return_type, NULL); ft->return_type = spec->subprogram.return_type->type; }
        s->type = ft; symbol_insert(M, s);
        symbol_push_scope(M, s);
        for (uint32_t i = 0; i < spec->subprogram.parameters.count; i++) {
            Syntax_Node *p = spec->subprogram.parameters.items[i];
            resolve_expression(M, p->parameter.type_mark, NULL);
            Symbol *ps = symbol_create(SYMBOL_PARAMETER, p->parameter.name);
            ps->type = p->parameter.type_mark ? p->parameter.type_mark->type : Type_Integer;
            symbol_insert(M, ps);
        }
        for (uint32_t i = 0; i < n->body.declarations.count; i++) resolve_declaration(M, n->body.declarations.items[i]);
        for (uint32_t i = 0; i < n->body.statements.count; i++) resolve_statement(M, n->body.statements.items[i]);
        symbol_pop_scope(M);
        break;
    }
    case NODE_PACKAGE_SPECIFICATION:
        symbol_push_scope(M, symbol_create(SYMBOL_PACKAGE, n->package_spec.name));
        for (uint32_t i = 0; i < n->package_spec.declarations.count; i++) resolve_declaration(M, n->package_spec.declarations.items[i]);
        symbol_pop_scope(M);
        break;
    default: break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §24  LLVM IR CODE GENERATION — Emitting Target Code
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The code generator traverses the typed AST and emits LLVM IR.
 * We emit textual IR for simplicity; it can be assembled by llc.
 */

typedef struct {
    FILE *output;
    int temp_counter;
    int label_counter;
    int string_counter;
    Symbol_Manager *symbols;
} Code_Generator;

static int emit_temp(Code_Generator *G) { return G->temp_counter++; }
static int emit_label(Code_Generator *G) { return G->label_counter++; }

static void emit(Code_Generator *G, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(G->output, format, args);
    va_end(args);
}

static const char *llvm_type_for(Type_Info *t) {
    if (!t) return "i32";
    switch (t->kind) {
    case TYPE_BOOLEAN: return "i1";
    case TYPE_CHARACTER: return "i8";
    case TYPE_INTEGER: case TYPE_ENUMERATION: return "i32";
    case TYPE_FLOAT: return "double";
    case TYPE_ACCESS: return "ptr";
    case TYPE_STRING: return "{ptr, i32}";  /* Fat pointer */
    default: return "i32";
    }
}

static int generate_expression(Code_Generator *G, Syntax_Node *n);
static void generate_statement(Code_Generator *G, Syntax_Node *n);
static void generate_declaration(Code_Generator *G, Syntax_Node *n);

static int generate_expression(Code_Generator *G, Syntax_Node *n) {
    if (!n) return 0;
    int result = emit_temp(G);
    const char *ty = llvm_type_for(n->type);

    switch (n->kind) {
    case NODE_INTEGER_LITERAL:
        emit(G, "  %%%d = add %s 0, %lld\n", result, ty, (long long)n->integer_value);
        break;

    case NODE_REAL_LITERAL:
        emit(G, "  %%%d = fadd double 0.0, %f\n", result, n->real_value);
        break;

    case NODE_STRING_LITERAL: {
        int str_id = G->string_counter++;
        emit(G, "  ; string literal: \"%.*s\"\n", n->string_value.length, n->string_value.text);
        emit(G, "  %%%d = alloca [%d x i8]\n", result, n->string_value.length + 1);
        break;
    }

    case NODE_IDENTIFIER:
        if (n->symbol) {
            emit(G, "  %%%d = load %s, ptr %%%.*s\n", result, ty,
                 n->string_value.length, n->string_value.text);
        }
        break;

    case NODE_BINARY_OPERATION: {
        int left = generate_expression(G, n->binary.left);
        int right = generate_expression(G, n->binary.right);
        const char *op;
        switch (n->binary.operator) {
        case TOKEN_PLUS: op = "add"; break;
        case TOKEN_MINUS: op = "sub"; break;
        case TOKEN_STAR: op = "mul"; break;
        case TOKEN_SLASH: op = "sdiv"; break;
        case TOKEN_MOD: op = "srem"; break;
        case TOKEN_EQUAL: op = "icmp eq"; break;
        case TOKEN_NOT_EQUAL: op = "icmp ne"; break;
        case TOKEN_LESS: op = "icmp slt"; break;
        case TOKEN_LESS_EQUAL: op = "icmp sle"; break;
        case TOKEN_GREATER: op = "icmp sgt"; break;
        case TOKEN_GREATER_EQUAL: op = "icmp sge"; break;
        case TOKEN_AND: case TOKEN_AND_THEN: op = "and"; break;
        case TOKEN_OR: case TOKEN_OR_ELSE: op = "or"; break;
        case TOKEN_XOR: op = "xor"; break;
        default: op = "add";
        }
        const char *left_ty = llvm_type_for(n->binary.left->type);
        emit(G, "  %%%d = %s %s %%%d, %%%d\n", result, op, left_ty, left, right);
        break;
    }

    case NODE_UNARY_OPERATION: {
        int operand = generate_expression(G, n->unary.operand);
        switch (n->unary.operator) {
        case TOKEN_MINUS:
            emit(G, "  %%%d = sub %s 0, %%%d\n", result, ty, operand);
            break;
        case TOKEN_NOT:
            emit(G, "  %%%d = xor i1 %%%d, 1\n", result, operand);
            break;
        default:
            emit(G, "  %%%d = add %s 0, %%%d\n", result, ty, operand);
        }
        break;
    }

    case NODE_FUNCTION_CALL: {
        /* Generate arguments */
        int args[64];
        for (uint32_t i = 0; i < n->apply.arguments.count && i < 64; i++)
            args[i] = generate_expression(G, n->apply.arguments.items[i]);
        /* Emit call */
        emit(G, "  %%%d = call %s @%.*s(", result,
             n->type ? llvm_type_for(n->type) : "i32",
             n->apply.prefix->string_value.length, n->apply.prefix->string_value.text);
        for (uint32_t i = 0; i < n->apply.arguments.count; i++) {
            if (i > 0) emit(G, ", ");
            emit(G, "%s %%%d", llvm_type_for(n->apply.arguments.items[i]->type), args[i]);
        }
        emit(G, ")\n");
        break;
    }

    default:
        emit(G, "  ; unhandled expression kind %d\n", n->kind);
        emit(G, "  %%%d = add i32 0, 0\n", result);
    }
    return result;
}

static void generate_statement(Code_Generator *G, Syntax_Node *n) {
    if (!n) return;

    switch (n->kind) {
    case NODE_NULL_STATEMENT:
        emit(G, "  ; null statement\n");
        break;

    case NODE_ASSIGNMENT_STATEMENT: {
        int val = generate_expression(G, n->assignment.value);
        if (n->assignment.target->kind == NODE_IDENTIFIER) {
            emit(G, "  store %s %%%d, ptr %%%.*s\n",
                 llvm_type_for(n->assignment.value->type), val,
                 n->assignment.target->string_value.length,
                 n->assignment.target->string_value.text);
        }
        break;
    }

    case NODE_IF_STATEMENT: {
        int cond = generate_expression(G, n->if_stmt.condition);
        int then_lbl = emit_label(G), else_lbl = emit_label(G), end_lbl = emit_label(G);
        emit(G, "  br i1 %%%d, label %%L%d, label %%L%d\n", cond, then_lbl, else_lbl);
        emit(G, "L%d:\n", then_lbl);
        for (uint32_t i = 0; i < n->if_stmt.then_stmts.count; i++)
            generate_statement(G, n->if_stmt.then_stmts.items[i]);
        emit(G, "  br label %%L%d\n", end_lbl);
        emit(G, "L%d:\n", else_lbl);
        for (uint32_t i = 0; i < n->if_stmt.else_stmts.count; i++)
            generate_statement(G, n->if_stmt.else_stmts.items[i]);
        emit(G, "  br label %%L%d\n", end_lbl);
        emit(G, "L%d:\n", end_lbl);
        break;
    }

    case NODE_LOOP_STATEMENT: {
        int loop_lbl = emit_label(G), end_lbl = emit_label(G);
        emit(G, "  br label %%L%d\n", loop_lbl);
        emit(G, "L%d:\n", loop_lbl);
        for (uint32_t i = 0; i < n->loop_stmt.statements.count; i++)
            generate_statement(G, n->loop_stmt.statements.items[i]);
        emit(G, "  br label %%L%d\n", loop_lbl);
        emit(G, "L%d:\n", end_lbl);
        break;
    }

    case NODE_RETURN_STATEMENT:
        if (n->return_stmt.value) {
            int val = generate_expression(G, n->return_stmt.value);
            emit(G, "  ret %s %%%d\n", llvm_type_for(n->return_stmt.value->type), val);
        } else {
            emit(G, "  ret void\n");
        }
        break;

    case NODE_PROCEDURE_CALL_STATEMENT: {
        if (n->apply.prefix->kind == NODE_IDENTIFIER) {
            emit(G, "  call void @%.*s()\n",
                 n->apply.prefix->string_value.length, n->apply.prefix->string_value.text);
        }
        break;
    }

    case NODE_BLOCK_STATEMENT:
        for (uint32_t i = 0; i < n->block.declarations.count; i++)
            generate_declaration(G, n->block.declarations.items[i]);
        for (uint32_t i = 0; i < n->block.statements.count; i++)
            generate_statement(G, n->block.statements.items[i]);
        break;

    default:
        emit(G, "  ; unhandled statement kind %d\n", n->kind);
    }
}

static void generate_declaration(Code_Generator *G, Syntax_Node *n) {
    if (!n) return;

    switch (n->kind) {
    case NODE_OBJECT_DECLARATION:
        for (uint32_t i = 0; i < n->object.names.count; i++) {
            String_Slice name = n->object.names.items[i]->string_value;
            const char *ty = n->object.type_mark ? llvm_type_for(n->object.type_mark->type) : "i32";
            emit(G, "  %%%.*s = alloca %s\n", name.length, name.text, ty);
            if (n->object.initializer) {
                int val = generate_expression(G, n->object.initializer);
                emit(G, "  store %s %%%d, ptr %%%.*s\n", ty, val, name.length, name.text);
            }
        }
        break;

    case NODE_PROCEDURE_BODY:
    case NODE_FUNCTION_BODY: {
        Syntax_Node *spec = n->body.specification;
        const char *ret_ty = (n->kind == NODE_FUNCTION_BODY && spec->subprogram.return_type)
            ? llvm_type_for(spec->subprogram.return_type->type) : "void";
        emit(G, "define %s @%.*s(", ret_ty, spec->subprogram.name.length, spec->subprogram.name.text);
        for (uint32_t i = 0; i < spec->subprogram.parameters.count; i++) {
            if (i > 0) emit(G, ", ");
            Syntax_Node *p = spec->subprogram.parameters.items[i];
            emit(G, "%s %%%.*s", p->parameter.type_mark ? llvm_type_for(p->parameter.type_mark->type) : "i32",
                 p->parameter.name.length, p->parameter.name.text);
        }
        emit(G, ") {\nentry:\n");
        for (uint32_t i = 0; i < n->body.declarations.count; i++)
            generate_declaration(G, n->body.declarations.items[i]);
        for (uint32_t i = 0; i < n->body.statements.count; i++)
            generate_statement(G, n->body.statements.items[i]);
        if (n->kind == NODE_PROCEDURE_BODY)
            emit(G, "  ret void\n");
        emit(G, "}\n\n");
        break;
    }

    case NODE_PACKAGE_BODY:
        for (uint32_t i = 0; i < n->package_body.declarations.count; i++)
            generate_declaration(G, n->package_body.declarations.items[i]);
        break;

    default:
        break;
    }
}

static void generate_compilation_unit(Code_Generator *G, Syntax_Node *unit) {
    emit(G, "; Ada83 Compiler Output\n");
    emit(G, "target triple = \"x86_64-pc-linux-gnu\"\n\n");

    /* Generate all declarations */
    for (uint32_t i = 0; i < unit->compilation_unit.units.count; i++)
        generate_declaration(G, unit->compilation_unit.units.items[i]);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §25  MAIN ENTRY POINT — The Compiler Driver
 * ═══════════════════════════════════════════════════════════════════════════
 */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "error: cannot open '%s'\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    return buffer;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Ada83 Compiler\nUsage: %s <source.adb> [-o output.ll]\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = "output.ll";
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output_file = argv[++i];
    }

    /* Read source file */
    char *source = read_file(input_file);
    if (!source) return 1;

    /* Parse */
    Parser parser = parser_create(source, strlen(source), input_file);
    Syntax_Node *ast = parse_compilation_unit(&parser);

    if (parser.error_count > 0) {
        fprintf(stderr, "%d parse error(s)\n", parser.error_count);
        return 1;
    }

    /* Semantic analysis */
    Symbol_Manager symbols;
    symbol_manager_initialize(&symbols);
    for (uint32_t i = 0; i < ast->compilation_unit.units.count; i++)
        resolve_declaration(&symbols, ast->compilation_unit.units.items[i]);

    if (Error_Count > 0) {
        fprintf(stderr, "%d semantic error(s)\n", Error_Count);
        return 1;
    }

    /* Code generation */
    FILE *output = fopen(output_file, "w");
    if (!output) {
        fprintf(stderr, "error: cannot create '%s'\n", output_file);
        return 1;
    }

    Code_Generator codegen = {output, 0, 0, 0, &symbols};
    generate_compilation_unit(&codegen, ast);
    fclose(output);

    printf("Compiled '%s' -> '%s'\n", input_file, output_file);
    return 0;
}

