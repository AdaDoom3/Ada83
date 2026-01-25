/* ═══════════════════════════════════════════════════════════════════════════
 * Ada83 Compiler — A Literate Implementation
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * "Programs must be written for people to read, and only incidentally
 *  for machines to execute." — Abelson & Sussman, SICP
 *
 * This compiler implements Ada 1983 (ANSI/MIL-STD-1815A) targeting LLVM IR.
 * The design follows GNAT LLVM's architecture while embracing Haskell-like
 * functional idioms in C99: immutability where possible, explicit types,
 * and composition over mutation.
 *
 * §1  Type_Metrics     — The measure of representation
 * §2  Memory_Arena     — Bump allocation for AST nodes
 * §3  String_Slice     — Non-owning string views
 * §4  Source_Location  — Diagnostic anchors
 * §5  Error_Handling   — Accumulating error reports
 * §6  Big_Integer      — Arbitrary precision for literals
 * §7  Lexer            — Character stream to tokens
 * §8  Abstract_Syntax  — Parse tree representation
 * §9  Parser           — Recursive descent
 * §10 Type_System      — Ada type semantics
 * §11 Symbol_Table     — Scoped name resolution
 * §12 Semantic_Pass    — Type checking and resolution
 * §13 Code_Generator   — LLVM IR emission
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iso646.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * §1. TYPE METRICS — The Measure of All Things
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Following GNAT LLVM (gnatllvm-types.ads), we centralize all size and
 * alignment computations. All sizes flow through To_Bits/To_Bytes morphisms.
 *
 * INVARIANT: Sizes in Type_Info are stored in BYTES (not bits).
 * This matches LLVM's DataLayout model and simplifies record layout.
 */

/* Safe ctype: C library requires unsigned char to avoid UB on signed char */
static inline int  Is_Alpha(char c)   { return isalpha((unsigned char)c); }
static inline int  Is_Alnum(char c)   { return isalnum((unsigned char)c); }
static inline int  Is_Digit(char c)   { return isdigit((unsigned char)c); }
static inline int  Is_Xdigit(char c)  { return isxdigit((unsigned char)c); }
static inline int  Is_Space(char c)   { return isspace((unsigned char)c); }
static inline char To_Lower(char c)   { return (char)tolower((unsigned char)c); }
static inline char To_Upper(char c)   { return (char)toupper((unsigned char)c); }

/* Bits Per Unit — the addressable quantum (universally 8 on modern targets) */
enum { Bits_Per_Unit = 8 };

/* LLVM integer widths in bits — the atoms of representation */
typedef enum {
    Width_1   = 1,    Width_8   = 8,    Width_16  = 16,
    Width_32  = 32,   Width_64  = 64,   Width_128 = 128,
    Width_Ptr = 64,   Width_Float = 32, Width_Double = 64
} Bit_Width;

/* Ada standard integer widths per RM §3.5.4 and GNAT conventions */
typedef enum {
    Ada_Short_Short_Integer_Bits = Width_8,
    Ada_Short_Integer_Bits       = Width_16,
    Ada_Integer_Bits             = Width_32,
    Ada_Long_Integer_Bits        = Width_64,
    Ada_Long_Long_Integer_Bits   = Width_64
} Ada_Integer_Width;

/* Default metrics when type is unspecified — uses Integer'Size (32 bits) */
enum {
    Default_Size_Bits   = Ada_Integer_Bits,
    Default_Size_Bytes  = Ada_Integer_Bits / Bits_Per_Unit,
    Default_Align_Bytes = Default_Size_Bytes
};

/* ─────────────────────────────────────────────────────────────────────────
 * §1.1 Bit/Byte Conversions — The Morphisms of Size
 *
 * To_Bits:  bytes → bits  (multiplicative, total)
 * To_Bytes: bits → bytes  (ceiling division, rounds up)
 * ───────────────────────────────────────────────────────────────────────── */

static inline uint64_t To_Bits(uint64_t bytes)  { return bytes * Bits_Per_Unit; }
static inline uint64_t To_Bytes(uint64_t bits)  { return (bits + Bits_Per_Unit - 1) / Bits_Per_Unit; }
static inline uint64_t Byte_Align(uint64_t bits){ return To_Bits(To_Bytes(bits)); }

/* Align size up to power-of-2 alignment boundary */
static inline size_t Align_To(size_t size, size_t align) {
    return align ? ((size + align - 1) & ~(align - 1)) : size;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §1.2 LLVM Type Selection — Width to Type Morphism
 *
 * Maps bit width to smallest containing LLVM integer type.
 * ───────────────────────────────────────────────────────────────────────── */

static inline const char *Llvm_Int_Type(uint32_t bits) {
    return bits <= 1   ? "i1"   : bits <= 8   ? "i8"  : bits <= 16  ? "i16" :
           bits <= 32  ? "i32"  : bits <= 64  ? "i64" : "i128";
}

static inline const char *Llvm_Float_Type(uint32_t bits) {
    return bits <= Width_Float ? "float" : "double";
}

/* ─────────────────────────────────────────────────────────────────────────
 * §1.3 Range Predicates — Determining Representation Width
 *
 * Compute minimum bits needed for a range [lo, hi].
 * ───────────────────────────────────────────────────────────────────────── */

static inline bool Fits_In_Signed(int64_t lo, int64_t hi, uint32_t bits) {
    if (bits >= 64) return true;
    int64_t min = -(1LL << (bits - 1)), max = (1LL << (bits - 1)) - 1;
    return lo >= min && hi <= max;
}

static inline bool Fits_In_Unsigned(int64_t lo, int64_t hi, uint32_t bits) {
    if (bits >= 64) return lo >= 0;
    return lo >= 0 && (uint64_t)hi < (1ULL << bits);
}

static inline uint32_t Bits_For_Range(int64_t lo, int64_t hi) {
    if (lo >= 0) {
        return (uint64_t)hi < 256ULL       ? Width_8  :
               (uint64_t)hi < 65536ULL     ? Width_16 :
               (uint64_t)hi < 4294967296ULL ? Width_32 : Width_64;
    }
    return Fits_In_Signed(lo, hi, 8)  ? Width_8  :
           Fits_In_Signed(lo, hi, 16) ? Width_16 :
           Fits_In_Signed(lo, hi, 32) ? Width_32 : Width_64;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §2. MEMORY ARENA — Bump Allocation for the Compilation Session
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * A simple bump allocator for AST nodes and strings. All memory persists
 * for the compilation session — we trade fragmentation for simplicity.
 *
 * Unlike the original, we maintain a linked list of chunks for cleanup.
 */

typedef struct Arena_Chunk Arena_Chunk;
struct Arena_Chunk {
    Arena_Chunk *previous;
    char        *base;
    char        *current;
    char        *end;
};

typedef struct {
    Arena_Chunk *head;
    size_t       chunk_size;
} Memory_Arena;

static Memory_Arena Global_Arena = {0};
enum { Default_Chunk_Size = 1 << 24 };  /* 16 MiB chunks */

static void *Arena_Allocate(size_t size) {
    size = Align_To(size, 8);

    if (!Global_Arena.head || Global_Arena.head->current + size > Global_Arena.head->end) {
        size_t chunk_size = Default_Chunk_Size;
        if (size > chunk_size) chunk_size = size + sizeof(Arena_Chunk);

        Arena_Chunk *chunk = malloc(sizeof(Arena_Chunk) + chunk_size);
        if (!chunk) { fprintf(stderr, "Out of memory\n"); exit(1); }

        chunk->previous = Global_Arena.head;
        chunk->base = chunk->current = (char*)(chunk + 1);
        chunk->end = chunk->base + chunk_size;
        Global_Arena.head = chunk;
    }

    void *result = Global_Arena.head->current;
    Global_Arena.head->current += size;
    return memset(result, 0, size);
}

static void Arena_Free_All(void) {
    Arena_Chunk *chunk = Global_Arena.head;
    while (chunk) {
        Arena_Chunk *prev = chunk->previous;
        free(chunk);
        chunk = prev;
    }
    Global_Arena.head = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §3. STRING SLICE — Non-Owning String Views
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * A slice is a pointer + length, borrowed from source or arena.
 * This avoids strlen() calls and enables substring views without allocation.
 */

typedef struct {
    const char *data;
    uint32_t    length;
} String_Slice;

#define S(literal) ((String_Slice){literal, sizeof(literal) - 1})
static const String_Slice Empty_Slice = {NULL, 0};

static inline String_Slice Slice_From_Cstring(const char *s) {
    return (String_Slice){s, s ? (uint32_t)strlen(s) : 0};
}

static String_Slice Slice_Duplicate(String_Slice s) {
    if (!s.length) return Empty_Slice;
    char *copy = Arena_Allocate(s.length + 1);
    memcpy(copy, s.data, s.length);
    return (String_Slice){copy, s.length};
}

static bool Slice_Equal_Ignore_Case(String_Slice a, String_Slice b) {
    if (a.length != b.length) return false;
    for (uint32_t i = 0; i < a.length; i++)
        if (To_Lower(a.data[i]) != To_Lower(b.data[i])) return false;
    return true;
}

/* FNV-1a hash with case folding for case-insensitive symbol lookup */
static uint64_t Slice_Hash(String_Slice s) {
    uint64_t h = 14695981039346656037ULL;
    for (uint32_t i = 0; i < s.length; i++)
        h = (h ^ (uint8_t)To_Lower(s.data[i])) * 1099511628211ULL;
    return h;
}

/* Levenshtein distance for "did you mean?" suggestions */
__attribute__((unused))
static int Edit_Distance(String_Slice a, String_Slice b) {
    if (a.length > 20 || b.length > 20) return 100;
    int d[21][21];
    for (uint32_t i = 0; i <= a.length; i++) d[i][0] = (int)i;
    for (uint32_t j = 0; j <= b.length; j++) d[0][j] = (int)j;
    for (uint32_t i = 1; i <= a.length; i++)
        for (uint32_t j = 1; j <= b.length; j++) {
            int cost = To_Lower(a.data[i-1]) != To_Lower(b.data[j-1]);
            int del = d[i-1][j] + 1, ins = d[i][j-1] + 1, sub = d[i-1][j-1] + cost;
            d[i][j] = del < ins ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
        }
    return d[a.length][b.length];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §4. SOURCE LOCATION — Anchoring Diagnostics to Source
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
    const char *filename;
    uint32_t    line;
    uint32_t    column;
} Source_Location;

static const Source_Location No_Location = {NULL, 0, 0};

/* ═══════════════════════════════════════════════════════════════════════════
 * §5. ERROR HANDLING — Accumulating Diagnostics
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Errors accumulate rather than immediately aborting, allowing the compiler
 * to report multiple issues in a single pass.
 */

static int Error_Count = 0;

static void Report_Error(Source_Location loc, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%u:%u: error: ",
            loc.filename ? loc.filename : "<unknown>", loc.line, loc.column);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    Error_Count++;
}

__attribute__((unused, noreturn))
static void Fatal_Error(Source_Location loc, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%u:%u: INTERNAL ERROR: ",
            loc.filename ? loc.filename : "<unknown>", loc.line, loc.column);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    exit(1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §6. BIG INTEGER — Arbitrary Precision for Literal Values
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ada literals can exceed 64-bit range. We represent magnitudes as arrays
 * of 64-bit limbs (little-endian). For literal parsing, we only need:
 *   - Construction from decimal string
 *   - Multiply by small constant (base)
 *   - Add small constant (digit)
 *   - Comparison and extraction
 *
 * This is a drastically simplified bigint focused on parsing, not general
 * arithmetic. For full arithmetic, we'd need Karatsuba etc.
 */

typedef struct {
    uint64_t *limbs;
    uint32_t  count;
    uint32_t  capacity;
    bool      is_negative;
} Big_Integer;

static Big_Integer *Big_Integer_New(uint32_t capacity) {
    Big_Integer *bi = Arena_Allocate(sizeof(Big_Integer));
    bi->limbs = Arena_Allocate(capacity * sizeof(uint64_t));
    bi->capacity = capacity;
    return bi;
}

static void Big_Integer_Ensure_Capacity(Big_Integer *bi, uint32_t needed) {
    if (needed <= bi->capacity) return;
    uint32_t new_cap = bi->capacity * 2;
    if (new_cap < needed) new_cap = needed;
    uint64_t *new_limbs = Arena_Allocate(new_cap * sizeof(uint64_t));
    memcpy(new_limbs, bi->limbs, bi->count * sizeof(uint64_t));
    bi->limbs = new_limbs;
    bi->capacity = new_cap;
}

/* Normalize: remove leading zero limbs, ensure zero is non-negative */
static void Big_Integer_Normalize(Big_Integer *bi) {
    while (bi->count > 0 && bi->limbs[bi->count - 1] == 0) bi->count--;
    if (bi->count == 0) bi->is_negative = false;
}

/* Multiply in-place by small factor and add small addend */
static void Big_Integer_Mul_Add_Small(Big_Integer *bi, uint64_t factor, uint64_t addend) {
    __uint128_t carry = addend;
    for (uint32_t i = 0; i < bi->count; i++) {
        carry += (__uint128_t)bi->limbs[i] * factor;
        bi->limbs[i] = (uint64_t)carry;
        carry >>= 64;
    }
    if (carry) {
        Big_Integer_Ensure_Capacity(bi, bi->count + 1);
        bi->limbs[bi->count++] = (uint64_t)carry;
    }
}

/* Parse decimal string into big integer */
static Big_Integer *Big_Integer_From_Decimal(const char *str) {
    Big_Integer *bi = Big_Integer_New(4);
    bi->is_negative = (*str == '-');
    if (*str == '-' || *str == '+') str++;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            if (bi->count == 0) { bi->limbs[0] = 0; bi->count = 1; }
            Big_Integer_Mul_Add_Small(bi, 10, (uint64_t)(*str - '0'));
        }
        str++;
    }
    Big_Integer_Normalize(bi);
    return bi;
}

/* Check if value fits in int64_t and extract if so */
static bool Big_Integer_Fits_Int64(const Big_Integer *bi, int64_t *out) {
    if (bi->count == 0) { *out = 0; return true; }
    if (bi->count > 1) return false;
    uint64_t v = bi->limbs[0];
    if (bi->is_negative) {
        if (v > (uint64_t)INT64_MAX + 1) return false;
        *out = -(int64_t)v;
    } else {
        if (v > (uint64_t)INT64_MAX) return false;
        *out = (int64_t)v;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §7. LEXER — Transforming Characters into Tokens
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The lexer maintains a cursor over the source buffer and produces tokens
 * on demand. Ada lexical rules from RM §2.
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §7.1 Token Kinds — The Vocabulary of Ada
 * ───────────────────────────────────────────────────────────────────────── */

typedef enum {
    /* Sentinel & error */
    TK_EOF = 0, TK_ERROR,

    /* Literals */
    TK_IDENTIFIER, TK_INTEGER, TK_REAL, TK_CHARACTER, TK_STRING,

    /* Delimiters */
    TK_LPAREN, TK_RPAREN, TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_DOT, TK_SEMICOLON, TK_COLON, TK_TICK,

    /* Compound delimiters */
    TK_ASSIGN, TK_ARROW, TK_DOTDOT, TK_LSHIFT, TK_RSHIFT, TK_BOX, TK_BAR,

    /* Operators */
    TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_AMPERSAND, TK_EXPON,

    /* Reserved words (Ada 83) */
    TK_ABORT, TK_ABS, TK_ACCEPT, TK_ACCESS, TK_ALL, TK_AND, TK_AND_THEN,
    TK_ARRAY, TK_AT, TK_BEGIN, TK_BODY, TK_CASE, TK_CONSTANT, TK_DECLARE,
    TK_DELAY, TK_DELTA, TK_DIGITS, TK_DO, TK_ELSE, TK_ELSIF, TK_END,
    TK_ENTRY, TK_EXCEPTION, TK_EXIT, TK_FOR, TK_FUNCTION, TK_GENERIC,
    TK_GOTO, TK_IF, TK_IN, TK_IS, TK_LIMITED, TK_LOOP, TK_MOD, TK_NEW,
    TK_NOT, TK_NULL, TK_OF, TK_OR, TK_OR_ELSE, TK_OTHERS, TK_OUT,
    TK_PACKAGE, TK_PRAGMA, TK_PRIVATE, TK_PROCEDURE, TK_RAISE, TK_RANGE,
    TK_RECORD, TK_REM, TK_RENAMES, TK_RETURN, TK_REVERSE, TK_SELECT,
    TK_SEPARATE, TK_SUBTYPE, TK_TASK, TK_TERMINATE, TK_THEN, TK_TYPE,
    TK_USE, TK_WHEN, TK_WHILE, TK_WITH, TK_XOR,

    TK_COUNT
} Token_Kind;

/* Token kind names for diagnostics */
static const char *Token_Name[TK_COUNT] = {
    [TK_EOF]="<eof>", [TK_ERROR]="<error>", [TK_IDENTIFIER]="identifier",
    [TK_INTEGER]="integer", [TK_REAL]="real", [TK_CHARACTER]="character",
    [TK_STRING]="string", [TK_LPAREN]="(", [TK_RPAREN]=")", [TK_LBRACKET]="[",
    [TK_RBRACKET]="]", [TK_COMMA]=",", [TK_DOT]=".", [TK_SEMICOLON]=";",
    [TK_COLON]=":", [TK_TICK]="'", [TK_ASSIGN]=":=", [TK_ARROW]="=>",
    [TK_DOTDOT]="..", [TK_LSHIFT]="<<", [TK_RSHIFT]=">>", [TK_BOX]="<>",
    [TK_BAR]="|", [TK_EQ]="=", [TK_NE]="/=", [TK_LT]="<", [TK_LE]="<=",
    [TK_GT]=">", [TK_GE]=">=", [TK_PLUS]="+", [TK_MINUS]="-", [TK_STAR]="*",
    [TK_SLASH]="/", [TK_AMPERSAND]="&", [TK_EXPON]="**",
    [TK_ABORT]="ABORT", [TK_ABS]="ABS", [TK_ACCEPT]="ACCEPT",
    [TK_ACCESS]="ACCESS", [TK_ALL]="ALL", [TK_AND]="AND",
    [TK_AND_THEN]="AND THEN", [TK_ARRAY]="ARRAY", [TK_AT]="AT",
    [TK_BEGIN]="BEGIN", [TK_BODY]="BODY", [TK_CASE]="CASE",
    [TK_CONSTANT]="CONSTANT", [TK_DECLARE]="DECLARE", [TK_DELAY]="DELAY",
    [TK_DELTA]="DELTA", [TK_DIGITS]="DIGITS", [TK_DO]="DO", [TK_ELSE]="ELSE",
    [TK_ELSIF]="ELSIF", [TK_END]="END", [TK_ENTRY]="ENTRY",
    [TK_EXCEPTION]="EXCEPTION", [TK_EXIT]="EXIT", [TK_FOR]="FOR",
    [TK_FUNCTION]="FUNCTION", [TK_GENERIC]="GENERIC", [TK_GOTO]="GOTO",
    [TK_IF]="IF", [TK_IN]="IN", [TK_IS]="IS", [TK_LIMITED]="LIMITED",
    [TK_LOOP]="LOOP", [TK_MOD]="MOD", [TK_NEW]="NEW", [TK_NOT]="NOT",
    [TK_NULL]="NULL", [TK_OF]="OF", [TK_OR]="OR", [TK_OR_ELSE]="OR ELSE",
    [TK_OTHERS]="OTHERS", [TK_OUT]="OUT", [TK_PACKAGE]="PACKAGE",
    [TK_PRAGMA]="PRAGMA", [TK_PRIVATE]="PRIVATE", [TK_PROCEDURE]="PROCEDURE",
    [TK_RAISE]="RAISE", [TK_RANGE]="RANGE", [TK_RECORD]="RECORD",
    [TK_REM]="REM", [TK_RENAMES]="RENAMES", [TK_RETURN]="RETURN",
    [TK_REVERSE]="REVERSE", [TK_SELECT]="SELECT", [TK_SEPARATE]="SEPARATE",
    [TK_SUBTYPE]="SUBTYPE", [TK_TASK]="TASK", [TK_TERMINATE]="TERMINATE",
    [TK_THEN]="THEN", [TK_TYPE]="TYPE", [TK_USE]="USE", [TK_WHEN]="WHEN",
    [TK_WHILE]="WHILE", [TK_WITH]="WITH", [TK_XOR]="XOR"
};

/* Keyword lookup table — sorted for potential binary search, but linear is fine for 63 keywords */
static struct { String_Slice name; Token_Kind kind; } Keywords[] = {
    {S("abort"),TK_ABORT},{S("abs"),TK_ABS},{S("accept"),TK_ACCEPT},{S("access"),TK_ACCESS},
    {S("all"),TK_ALL},{S("and"),TK_AND},{S("array"),TK_ARRAY},{S("at"),TK_AT},
    {S("begin"),TK_BEGIN},{S("body"),TK_BODY},{S("case"),TK_CASE},{S("constant"),TK_CONSTANT},
    {S("declare"),TK_DECLARE},{S("delay"),TK_DELAY},{S("delta"),TK_DELTA},{S("digits"),TK_DIGITS},
    {S("do"),TK_DO},{S("else"),TK_ELSE},{S("elsif"),TK_ELSIF},{S("end"),TK_END},
    {S("entry"),TK_ENTRY},{S("exception"),TK_EXCEPTION},{S("exit"),TK_EXIT},{S("for"),TK_FOR},
    {S("function"),TK_FUNCTION},{S("generic"),TK_GENERIC},{S("goto"),TK_GOTO},{S("if"),TK_IF},
    {S("in"),TK_IN},{S("is"),TK_IS},{S("limited"),TK_LIMITED},{S("loop"),TK_LOOP},
    {S("mod"),TK_MOD},{S("new"),TK_NEW},{S("not"),TK_NOT},{S("null"),TK_NULL},
    {S("of"),TK_OF},{S("or"),TK_OR},{S("others"),TK_OTHERS},{S("out"),TK_OUT},
    {S("package"),TK_PACKAGE},{S("pragma"),TK_PRAGMA},{S("private"),TK_PRIVATE},
    {S("procedure"),TK_PROCEDURE},{S("raise"),TK_RAISE},{S("range"),TK_RANGE},
    {S("record"),TK_RECORD},{S("rem"),TK_REM},{S("renames"),TK_RENAMES},{S("return"),TK_RETURN},
    {S("reverse"),TK_REVERSE},{S("select"),TK_SELECT},{S("separate"),TK_SEPARATE},
    {S("subtype"),TK_SUBTYPE},{S("task"),TK_TASK},{S("terminate"),TK_TERMINATE},
    {S("then"),TK_THEN},{S("type"),TK_TYPE},{S("use"),TK_USE},{S("when"),TK_WHEN},
    {S("while"),TK_WHILE},{S("with"),TK_WITH},{S("xor"),TK_XOR},
    {Empty_Slice, TK_EOF}  /* Sentinel */
};

static Token_Kind Lookup_Keyword(String_Slice name) {
    for (int i = 0; Keywords[i].name.data; i++)
        if (Slice_Equal_Ignore_Case(name, Keywords[i].name))
            return Keywords[i].kind;
    return TK_IDENTIFIER;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §7.2 Token Structure
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct {
    Token_Kind      kind;
    Source_Location location;
    String_Slice    text;

    /* Semantic value (valid based on kind) */
    union {
        int64_t      integer_value;
        double       float_value;
        Big_Integer *big_integer;
    };
} Token;

/* ─────────────────────────────────────────────────────────────────────────
 * §7.3 Lexer State
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *source_start;
    const char *current;
    const char *source_end;
    const char *filename;
    uint32_t    line;
    uint32_t    column;
} Lexer;

static Lexer Lexer_New(const char *source, size_t length, const char *filename) {
    return (Lexer){source, source, source + length, filename, 1, 1};
}

static inline char Lexer_Peek(const Lexer *lex, size_t offset) {
    return lex->current + offset < lex->source_end ? lex->current[offset] : '\0';
}

static inline char Lexer_Advance(Lexer *lex) {
    if (lex->current >= lex->source_end) return '\0';
    char c = *lex->current++;
    if (c == '\n') { lex->line++; lex->column = 1; }
    else lex->column++;
    return c;
}

static void Lexer_Skip_Whitespace_And_Comments(Lexer *lex) {
    for (;;) {
        while (lex->current < lex->source_end && Is_Space(*lex->current))
            Lexer_Advance(lex);

        /* Ada comment: -- to end of line */
        if (lex->current + 1 < lex->source_end &&
            lex->current[0] == '-' && lex->current[1] == '-') {
            while (lex->current < lex->source_end && *lex->current != '\n')
                Lexer_Advance(lex);
        } else break;
    }
}

static inline Token Make_Token(Token_Kind kind, Source_Location loc, String_Slice text) {
    return (Token){kind, loc, text, {0}};
}

/* ─────────────────────────────────────────────────────────────────────────
 * §7.4 Scanning Functions
 * ───────────────────────────────────────────────────────────────────────── */

static Token Scan_Identifier(Lexer *lex) {
    Source_Location loc = {lex->filename, lex->line, lex->column};
    const char *start = lex->current;

    while (Is_Alnum(Lexer_Peek(lex, 0)) || Lexer_Peek(lex, 0) == '_')
        Lexer_Advance(lex);

    String_Slice text = {start, (uint32_t)(lex->current - start)};
    Token_Kind kind = Lookup_Keyword(text);
    return Make_Token(kind, loc, text);
}

/* Parse digit value in any base up to 16 */
static inline int Digit_Value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static Token Scan_Number(Lexer *lex) {
    Source_Location loc = {lex->filename, lex->line, lex->column};
    const char *start = lex->current;
    int base = 10;
    bool is_real = false, has_exponent = false;

    /* Scan integer part (possibly base specifier) */
    while (Is_Digit(Lexer_Peek(lex, 0)) || Lexer_Peek(lex, 0) == '_')
        Lexer_Advance(lex);

    /* Based literal: 16#FFFF# or 2#1010# */
    if (Lexer_Peek(lex, 0) == '#' ||
        (Lexer_Peek(lex, 0) == ':' && Is_Xdigit(Lexer_Peek(lex, 1)))) {
        char delim = Lexer_Peek(lex, 0);

        /* Parse base from what we've scanned so far */
        char base_buf[16] = {0};
        int bi = 0;
        for (const char *p = start; p < lex->current && bi < 15; p++)
            if (*p != '_') base_buf[bi++] = *p;
        base = atoi(base_buf);

        Lexer_Advance(lex); /* consume # or : */

        /* Scan mantissa */
        while (Is_Xdigit(Lexer_Peek(lex, 0)) || Lexer_Peek(lex, 0) == '_')
            Lexer_Advance(lex);

        if (Lexer_Peek(lex, 0) == '.') {
            is_real = true;
            Lexer_Advance(lex);
            while (Is_Xdigit(Lexer_Peek(lex, 0)) || Lexer_Peek(lex, 0) == '_')
                Lexer_Advance(lex);
        }

        if (Lexer_Peek(lex, 0) == delim) Lexer_Advance(lex);

        if (To_Lower(Lexer_Peek(lex, 0)) == 'e') {
            has_exponent = true;
            Lexer_Advance(lex);
            if (Lexer_Peek(lex, 0) == '+' || Lexer_Peek(lex, 0) == '-')
                Lexer_Advance(lex);
            while (Is_Digit(Lexer_Peek(lex, 0)) || Lexer_Peek(lex, 0) == '_')
                Lexer_Advance(lex);
        }
    } else {
        /* Decimal literal with optional fraction and exponent */
        if (Lexer_Peek(lex, 0) == '.' && Lexer_Peek(lex, 1) != '.' && !Is_Alpha(Lexer_Peek(lex, 1))) {
            is_real = true;
            Lexer_Advance(lex);
            while (Is_Digit(Lexer_Peek(lex, 0)) || Lexer_Peek(lex, 0) == '_')
                Lexer_Advance(lex);
        }

        if (To_Lower(Lexer_Peek(lex, 0)) == 'e') {
            has_exponent = true;
            is_real = is_real || true; /* E without dot is still considered for real context */
            Lexer_Advance(lex);
            if (Lexer_Peek(lex, 0) == '+' || Lexer_Peek(lex, 0) == '-')
                Lexer_Advance(lex);
            while (Is_Digit(Lexer_Peek(lex, 0)) || Lexer_Peek(lex, 0) == '_')
                Lexer_Advance(lex);
        }
    }

    String_Slice text = {start, (uint32_t)(lex->current - start)};
    Token tok = Make_Token(is_real ? TK_REAL : TK_INTEGER, loc, text);

    /* Convert to value: strip underscores, parse with base */
    char clean[512];
    int ci = 0;
    for (const char *p = start; p < lex->current && ci < 510; p++)
        if (*p != '_' && *p != '#' && *p != ':') clean[ci++] = *p;
    clean[ci] = '\0';

    if (is_real) {
        tok.float_value = strtod(clean, NULL);
        if (base != 10) {
            /* Based real: compute manually (simplified) */
            /* Full implementation would parse mantissa in base, apply exponent */
        }
    } else {
        if (base == 10 && !has_exponent) {
            tok.big_integer = Big_Integer_From_Decimal(clean);
            int64_t v;
            if (Big_Integer_Fits_Int64(tok.big_integer, &v))
                tok.integer_value = v;
        } else {
            /* Based integer: parse in given base */
            int64_t value = 0;
            for (int i = 0; clean[i]; i++) {
                int d = Digit_Value(clean[i]);
                if (d >= 0 && d < base) value = value * base + d;
            }
            tok.integer_value = value;
        }
    }

    return tok;
}

static Token Scan_Character_Literal(Lexer *lex) {
    Source_Location loc = {lex->filename, lex->line, lex->column};
    Lexer_Advance(lex); /* consume opening ' */

    char c = Lexer_Advance(lex);
    if (Lexer_Peek(lex, 0) != '\'') {
        Report_Error(loc, "unterminated character literal");
        return Make_Token(TK_ERROR, loc, S(""));
    }
    Lexer_Advance(lex); /* consume closing ' */

    Token tok = Make_Token(TK_CHARACTER, loc, (String_Slice){lex->current - 3, 3});
    tok.integer_value = (unsigned char)c;
    return tok;
}

static Token Scan_String_Literal(Lexer *lex) {
    Source_Location loc = {lex->filename, lex->line, lex->column};
    Lexer_Advance(lex); /* consume opening " */

    size_t capacity = 64, length = 0;
    char *buffer = Arena_Allocate(capacity);

    while (lex->current < lex->source_end) {
        if (*lex->current == '"') {
            if (Lexer_Peek(lex, 1) == '"') {
                /* Escaped quote: "" becomes " */
                if (length >= capacity - 1) {
                    char *newbuf = Arena_Allocate(capacity * 2);
                    memcpy(newbuf, buffer, length);
                    buffer = newbuf;
                    capacity *= 2;
                }
                buffer[length++] = '"';
                Lexer_Advance(lex);
                Lexer_Advance(lex);
            } else {
                Lexer_Advance(lex); /* consume closing " */
                break;
            }
        } else {
            if (length >= capacity - 1) {
                char *newbuf = Arena_Allocate(capacity * 2);
                memcpy(newbuf, buffer, length);
                buffer = newbuf;
                capacity *= 2;
            }
            buffer[length++] = Lexer_Advance(lex);
        }
    }
    buffer[length] = '\0';

    Token tok = Make_Token(TK_STRING, loc, (String_Slice){buffer, (uint32_t)length});
    return tok;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §7.5 Main Lexer Entry Point
 * ───────────────────────────────────────────────────────────────────────── */

static Token Lexer_Next_Token(Lexer *lex) {
    Lexer_Skip_Whitespace_And_Comments(lex);

    if (lex->current >= lex->source_end)
        return Make_Token(TK_EOF, (Source_Location){lex->filename, lex->line, lex->column}, S(""));

    Source_Location loc = {lex->filename, lex->line, lex->column};
    char c = Lexer_Peek(lex, 0);

    /* Identifiers and keywords */
    if (Is_Alpha(c)) return Scan_Identifier(lex);

    /* Numeric literals */
    if (Is_Digit(c)) return Scan_Number(lex);

    /* Character literal: 'X' where X is any graphic character */
    /* Need to check for printable character (not just alpha) since '1' etc. are valid */
    {
        char middle = Lexer_Peek(lex, 1);
        if (c == '\'' && middle != '\'' && middle >= ' ' && Lexer_Peek(lex, 2) == '\'')
            return Scan_Character_Literal(lex);
    }

    /* String literal */
    if (c == '"') return Scan_String_Literal(lex);

    /* Operators and delimiters */
    Lexer_Advance(lex);
    char c2 = Lexer_Peek(lex, 0);

    switch (c) {
        case '(': return Make_Token(TK_LPAREN, loc, S("("));
        case ')': return Make_Token(TK_RPAREN, loc, S(")"));
        case '[': return Make_Token(TK_LBRACKET, loc, S("["));
        case ']': return Make_Token(TK_RBRACKET, loc, S("]"));
        case ',': return Make_Token(TK_COMMA, loc, S(","));
        case ';': return Make_Token(TK_SEMICOLON, loc, S(";"));
        case '&': return Make_Token(TK_AMPERSAND, loc, S("&"));
        case '|': return Make_Token(TK_BAR, loc, S("|"));
        case '+': return Make_Token(TK_PLUS, loc, S("+"));
        case '-': return Make_Token(TK_MINUS, loc, S("-"));
        case '\'': return Make_Token(TK_TICK, loc, S("'"));

        case '.':
            if (c2 == '.') { Lexer_Advance(lex); return Make_Token(TK_DOTDOT, loc, S("..")); }
            return Make_Token(TK_DOT, loc, S("."));

        case ':':
            if (c2 == '=') { Lexer_Advance(lex); return Make_Token(TK_ASSIGN, loc, S(":=")); }
            return Make_Token(TK_COLON, loc, S(":"));

        case '*':
            if (c2 == '*') { Lexer_Advance(lex); return Make_Token(TK_EXPON, loc, S("**")); }
            return Make_Token(TK_STAR, loc, S("*"));

        case '/':
            if (c2 == '=') { Lexer_Advance(lex); return Make_Token(TK_NE, loc, S("/=")); }
            return Make_Token(TK_SLASH, loc, S("/"));

        case '=':
            if (c2 == '>') { Lexer_Advance(lex); return Make_Token(TK_ARROW, loc, S("=>")); }
            return Make_Token(TK_EQ, loc, S("="));

        case '<':
            if (c2 == '=') { Lexer_Advance(lex); return Make_Token(TK_LE, loc, S("<=")); }
            if (c2 == '<') { Lexer_Advance(lex); return Make_Token(TK_LSHIFT, loc, S("<<")); }
            if (c2 == '>') { Lexer_Advance(lex); return Make_Token(TK_BOX, loc, S("<>")); }
            return Make_Token(TK_LT, loc, S("<"));

        case '>':
            if (c2 == '=') { Lexer_Advance(lex); return Make_Token(TK_GE, loc, S(">=")); }
            if (c2 == '>') { Lexer_Advance(lex); return Make_Token(TK_RSHIFT, loc, S(">>")); }
            return Make_Token(TK_GT, loc, S(">"));

        default:
            Report_Error(loc, "unexpected character '%c'", c);
            return Make_Token(TK_ERROR, loc, S(""));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §8. ABSTRACT SYNTAX TREE — Parse Tree Representation
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The AST uses a tagged union design. Each node kind has a specific payload.
 * We use GNAT LLVM's principle: preserve enough structure for later passes.
 */

/* Forward declarations */
typedef struct Syntax_Node Syntax_Node;
typedef struct Type_Info Type_Info;
typedef struct Symbol Symbol;

/* Dynamic array of syntax nodes */
typedef struct {
    Syntax_Node **items;
    uint32_t      count;
    uint32_t      capacity;
} Node_List;

static void Node_List_Push(Node_List *list, Syntax_Node *node) {
    if (list->count >= list->capacity) {
        uint32_t new_cap = list->capacity ? list->capacity * 2 : 8;
        Syntax_Node **new_items = Arena_Allocate(new_cap * sizeof(Syntax_Node*));
        if (list->items) memcpy(new_items, list->items, list->count * sizeof(Syntax_Node*));
        list->items = new_items;
        list->capacity = new_cap;
    }
    list->items[list->count++] = node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §8.1 Node Kinds
 * ───────────────────────────────────────────────────────────────────────── */

typedef enum {
    /* Literals and primaries */
    NK_INTEGER, NK_REAL, NK_STRING, NK_CHARACTER, NK_NULL, NK_OTHERS,
    NK_IDENTIFIER, NK_SELECTED, NK_ATTRIBUTE, NK_QUALIFIED,

    /* Expressions */
    NK_BINARY_OP, NK_UNARY_OP, NK_AGGREGATE, NK_ALLOCATOR,
    NK_APPLY,       /* Unified: call, index, slice — resolved later */
    NK_RANGE,       /* a .. b */
    NK_ASSOCIATION, /* name => value */

    /* Type definitions */
    NK_SUBTYPE_INDICATION, NK_RANGE_CONSTRAINT, NK_INDEX_CONSTRAINT,
    NK_DISCRIMINANT_CONSTRAINT, NK_ARRAY_TYPE, NK_RECORD_TYPE,
    NK_ACCESS_TYPE, NK_DERIVED_TYPE, NK_ENUMERATION_TYPE,
    NK_INTEGER_TYPE, NK_REAL_TYPE, NK_COMPONENT_DECL, NK_VARIANT_PART,
    NK_VARIANT, NK_DISCRIMINANT_SPEC,

    /* Statements */
    NK_ASSIGNMENT, NK_CALL_STMT, NK_RETURN, NK_IF, NK_CASE, NK_LOOP,
    NK_BLOCK, NK_EXIT, NK_GOTO, NK_RAISE, NK_NULL_STMT, NK_LABEL,
    NK_ACCEPT, NK_SELECT, NK_DELAY, NK_ABORT, NK_CODE,

    /* Declarations */
    NK_OBJECT_DECL, NK_TYPE_DECL, NK_SUBTYPE_DECL, NK_EXCEPTION_DECL,
    NK_PROCEDURE_SPEC, NK_FUNCTION_SPEC, NK_PROCEDURE_BODY, NK_FUNCTION_BODY,
    NK_PACKAGE_SPEC, NK_PACKAGE_BODY, NK_TASK_SPEC, NK_TASK_BODY,
    NK_ENTRY_DECL, NK_SUBPROGRAM_RENAMING, NK_PACKAGE_RENAMING,
    NK_EXCEPTION_RENAMING, NK_GENERIC_DECL, NK_GENERIC_INST,
    NK_PARAM_SPEC, NK_USE_CLAUSE, NK_WITH_CLAUSE, NK_PRAGMA,
    NK_REPRESENTATION_CLAUSE, NK_EXCEPTION_HANDLER,
    NK_CONTEXT_CLAUSE, NK_COMPILATION_UNIT,

    /* Generic formals */
    NK_GENERIC_TYPE_PARAM, NK_GENERIC_OBJECT_PARAM, NK_GENERIC_SUBPROGRAM_PARAM,

    NK_COUNT
} Node_Kind;

/* ─────────────────────────────────────────────────────────────────────────
 * §8.2 Syntax Node Structure
 *
 * Each node carries its kind, location, optional type annotation (from
 * semantic analysis), and a payload specific to the kind.
 * ───────────────────────────────────────────────────────────────────────── */

struct Syntax_Node {
    Node_Kind        kind;
    Source_Location  location;
    Type_Info       *type;      /* Set during semantic analysis */
    Symbol          *symbol;    /* Set during name resolution */

    union {
        /* NK_INTEGER */
        struct { int64_t value; Big_Integer *big_value; } integer_lit;

        /* NK_REAL */
        struct { double value; } real_lit;

        /* NK_STRING, NK_CHARACTER, NK_IDENTIFIER */
        struct { String_Slice text; } string_val;

        /* NK_SELECTED: prefix.selector */
        struct { Syntax_Node *prefix; String_Slice selector; } selected;

        /* NK_ATTRIBUTE: prefix'attribute(arg) */
        struct { Syntax_Node *prefix; String_Slice name; Syntax_Node *argument; } attribute;

        /* NK_QUALIFIED: subtype_mark'(expression) */
        struct { Syntax_Node *subtype_mark; Syntax_Node *expression; } qualified;

        /* NK_BINARY_OP, NK_UNARY_OP */
        struct { Token_Kind op; Syntax_Node *left; Syntax_Node *right; } binary;
        struct { Token_Kind op; Syntax_Node *operand; } unary;

        /* NK_AGGREGATE */
        struct { Node_List items; bool is_named; } aggregate;

        /* NK_ALLOCATOR: new subtype_mark'(expression) or new subtype_mark */
        struct { Syntax_Node *subtype_mark; Syntax_Node *expression; } allocator;

        /* NK_APPLY: prefix(arguments) — unified call/index/slice */
        struct { Syntax_Node *prefix; Node_List arguments; } apply;

        /* NK_RANGE: low .. high */
        struct { Syntax_Node *low; Syntax_Node *high; } range;

        /* NK_ASSOCIATION: choices => expression */
        struct { Node_List choices; Syntax_Node *expression; } association;

        /* NK_SUBTYPE_INDICATION: subtype_mark constraint */
        struct { Syntax_Node *subtype_mark; Syntax_Node *constraint; } subtype_ind;

        /* NK_*_CONSTRAINT */
        struct { Node_List ranges; } index_constraint;
        struct { Syntax_Node *range; } range_constraint;
        struct { Node_List associations; } discriminant_constraint;

        /* NK_ARRAY_TYPE */
        struct { Node_List indices; Syntax_Node *component_type; bool is_constrained; } array_type;

        /* NK_RECORD_TYPE */
        struct {
            Node_List discriminants;
            Node_List components;
            Syntax_Node *variant_part;
            bool is_null;
        } record_type;

        /* NK_ACCESS_TYPE */
        struct { Syntax_Node *designated; bool is_constant; } access_type;

        /* NK_DERIVED_TYPE */
        struct { Syntax_Node *parent_type; Syntax_Node *constraint; } derived_type;

        /* NK_ENUMERATION_TYPE */
        struct { Node_List literals; } enum_type;

        /* NK_INTEGER_TYPE, NK_REAL_TYPE */
        struct { Syntax_Node *range; int64_t modulus; } integer_type;
        struct { Syntax_Node *precision; Syntax_Node *range; Syntax_Node *delta; } real_type;

        /* NK_COMPONENT_DECL */
        struct { Node_List names; Syntax_Node *component_type; Syntax_Node *init; } component;

        /* NK_VARIANT_PART */
        struct { String_Slice discriminant; Node_List variants; } variant_part;

        /* NK_VARIANT */
        struct { Node_List choices; Node_List components; Syntax_Node *variant_part; } variant;

        /* NK_DISCRIMINANT_SPEC */
        struct { Node_List names; Syntax_Node *disc_type; Syntax_Node *default_expr; } discriminant;

        /* NK_ASSIGNMENT */
        struct { Syntax_Node *target; Syntax_Node *value; } assignment;

        /* NK_RETURN */
        struct { Syntax_Node *expression; } return_stmt;

        /* NK_IF */
        struct {
            Syntax_Node *condition;
            Node_List then_stmts;
            Node_List elsif_parts;  /* each is another NK_IF for elsif */
            Node_List else_stmts;
        } if_stmt;

        /* NK_CASE */
        struct { Syntax_Node *expression; Node_List alternatives; } case_stmt;

        /* NK_LOOP */
        struct {
            String_Slice label;
            Syntax_Node *iteration_scheme;  /* for/while condition */
            Node_List statements;
            bool is_reverse;
        } loop_stmt;

        /* NK_BLOCK */
        struct {
            String_Slice label;
            Node_List declarations;
            Node_List statements;
            Node_List handlers;
        } block_stmt;

        /* NK_EXIT */
        struct { String_Slice loop_name; Syntax_Node *condition; } exit_stmt;

        /* NK_GOTO, NK_LABEL */
        struct { String_Slice name; } label_ref;

        /* NK_RAISE */
        struct { Syntax_Node *exception_name; } raise_stmt;

        /* NK_ACCEPT */
        struct {
            String_Slice entry_name;
            Syntax_Node *index;
            Node_List parameters;
            Node_List statements;
        } accept_stmt;

        /* NK_SELECT */
        struct { Node_List alternatives; Syntax_Node *else_part; } select_stmt;

        /* NK_DELAY */
        struct { Syntax_Node *expression; } delay_stmt;

        /* NK_ABORT */
        struct { Node_List task_names; } abort_stmt;

        /* NK_OBJECT_DECL */
        struct {
            Node_List names;
            Syntax_Node *object_type;
            Syntax_Node *init;
            bool is_constant;
            bool is_aliased;
        } object_decl;

        /* NK_TYPE_DECL, NK_SUBTYPE_DECL */
        struct {
            String_Slice name;
            Node_List discriminants;
            Syntax_Node *definition;
            bool is_limited;
            bool is_private;
        } type_decl;

        /* NK_EXCEPTION_DECL */
        struct { Node_List names; } exception_decl;

        /* NK_PROCEDURE_SPEC, NK_FUNCTION_SPEC */
        struct {
            String_Slice name;
            Node_List parameters;
            Syntax_Node *return_type;  /* NULL for procedures */
        } subprogram_spec;

        /* NK_PROCEDURE_BODY, NK_FUNCTION_BODY */
        struct {
            Syntax_Node *specification;
            Node_List declarations;
            Node_List statements;
            Node_List handlers;
            bool is_separate;
        } subprogram_body;

        /* NK_PACKAGE_SPEC */
        struct {
            String_Slice name;
            Node_List visible_decls;
            Node_List private_decls;
        } package_spec;

        /* NK_PACKAGE_BODY */
        struct {
            String_Slice name;
            Node_List declarations;
            Node_List statements;
            Node_List handlers;
            bool is_separate;
        } package_body;

        /* NK_TASK_SPEC */
        struct {
            String_Slice name;
            Node_List entries;  /* Entry declarations */
            bool is_type;       /* true if TASK TYPE, false if single TASK */
        } task_spec;

        /* NK_TASK_BODY */
        struct {
            String_Slice name;
            Node_List declarations;
            Node_List statements;
            Node_List handlers;
            bool is_separate;
        } task_body;

        /* NK_ENTRY_DECL */
        struct {
            String_Slice name;
            Node_List parameters;  /* Parameter specs */
            Node_List index_constraints;  /* For entry families */
        } entry_decl;

        /* NK_PARAM_SPEC */
        struct {
            Node_List names;
            Syntax_Node *param_type;
            Syntax_Node *default_expr;
            enum { MODE_IN, MODE_OUT, MODE_IN_OUT } mode;
        } param_spec;

        /* NK_GENERIC_DECL */
        struct {
            Node_List formals;
            Syntax_Node *unit;  /* The procedure/function/package being made generic */
        } generic_decl;

        /* NK_GENERIC_INST */
        struct {
            Syntax_Node *generic_name;
            Node_List actuals;
            String_Slice instance_name;
            Token_Kind unit_kind;  /* TK_PROCEDURE, TK_FUNCTION, or TK_PACKAGE */
        } generic_inst;

        /* NK_GENERIC_TYPE_PARAM: type T is ... */
        struct {
            String_Slice name;
            int def_kind;  /* 0=PRIVATE, 1=LIMITED_PRIVATE, 2=DISCRETE, 3=INTEGER, 4=FLOAT, 5=FIXED */
            Syntax_Node *def_detail;
        } generic_type_param;

        /* NK_GENERIC_OBJECT_PARAM: X : [mode] type [:= default] */
        struct {
            Node_List names;
            Syntax_Node *object_type;
            Syntax_Node *default_expr;
            int mode;  /* GEN_MODE_IN, GEN_MODE_IN_OUT */
        } generic_object_param;

        /* NK_GENERIC_SUBPROGRAM_PARAM: with procedure/function spec [is name|<>] */
        struct {
            Syntax_Node *subprogram_spec;
            Syntax_Node *default_name;
            bool is_box;
        } generic_subprogram_param;

        /* NK_WITH_CLAUSE, NK_USE_CLAUSE */
        struct { Node_List names; } use_clause;

        /* NK_PRAGMA */
        struct { String_Slice name; Node_List arguments; } pragma_node;

        /* NK_EXCEPTION_HANDLER */
        struct { Node_List exceptions; Node_List statements; } handler;

        /* NK_REPRESENTATION_CLAUSE (RM 13.1) */
        struct {
            Syntax_Node *entity_name;    /* Type or object being represented */
            String_Slice attribute;       /* 'SIZE, 'ALIGNMENT, etc. (empty if record/enum rep) */
            Syntax_Node *expression;      /* Attribute value or address expression */
            Node_List    component_clauses; /* For record representation: component positions */
            bool         is_record_rep;   /* true if FOR T USE RECORD ... */
            bool         is_enum_rep;     /* true if FOR T USE (literals...) */
        } rep_clause;

        /* NK_CONTEXT_CLAUSE */
        struct { Node_List with_clauses; Node_List use_clauses; } context;

        /* NK_COMPILATION_UNIT */
        struct { Syntax_Node *context; Syntax_Node *unit; } compilation_unit;
    };
};

/* Node constructor */
static Syntax_Node *Node_New(Node_Kind kind, Source_Location loc) {
    Syntax_Node *node = Arena_Allocate(sizeof(Syntax_Node));
    node->kind = kind;
    node->location = loc;
    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9. PARSER — Recursive Descent with Unified Postfix Handling
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Key design decisions following the checklist analysis:
 *
 * 1. UNIFIED APPLY NODE: All X(...) forms parse as NK_APPLY. Semantic analysis
 *    later distinguishes calls, indexing, slicing, and type conversions.
 *
 * 2. UNIFIED ASSOCIATION PARSING: One helper handles positional, named, and
 *    choice associations used in aggregates, calls, and generic actuals.
 *
 * 3. UNIFIED POSTFIX CHAIN: One loop handles .selector, 'attribute, (args).
 *
 * 4. NO "PRETEND TOKEN EXISTS": Error recovery synchronizes to known tokens
 *    rather than silently accepting malformed syntax.
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §9.1 Parser State
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct {
    Lexer        lexer;
    Token        current_token;
    Token        previous_token;
    bool         had_error;
    bool         panic_mode;

    /* Progress tracking to detect stuck parsers */
    uint32_t     last_line;
    uint32_t     last_column;
    Token_Kind   last_kind;
} Parser;

static Parser Parser_New(const char *source, size_t length, const char *filename) {
    Parser p = {0};
    p.lexer = Lexer_New(source, length, filename);
    p.current_token = Lexer_Next_Token(&p.lexer);
    return p;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.2 Token Movement
 * ───────────────────────────────────────────────────────────────────────── */

static inline bool Parser_At(Parser *p, Token_Kind kind) {
    return p->current_token.kind == kind;
}

static inline bool Parser_At_Any(Parser *p, Token_Kind k1, Token_Kind k2) {
    return Parser_At(p, k1) || Parser_At(p, k2);
}

static Token Parser_Advance(Parser *p) {
    p->previous_token = p->current_token;
    p->current_token = Lexer_Next_Token(&p->lexer);

    /* Handle compound keywords: AND THEN, OR ELSE */
    if (p->previous_token.kind == TK_AND && Parser_At(p, TK_THEN)) {
        p->previous_token.kind = TK_AND_THEN;
        p->current_token = Lexer_Next_Token(&p->lexer);
    } else if (p->previous_token.kind == TK_OR && Parser_At(p, TK_ELSE)) {
        p->previous_token.kind = TK_OR_ELSE;
        p->current_token = Lexer_Next_Token(&p->lexer);
    }

    return p->previous_token;
}

static bool Parser_Match(Parser *p, Token_Kind kind) {
    if (!Parser_At(p, kind)) return false;
    Parser_Advance(p);
    return true;
}

static Source_Location Parser_Location(Parser *p) {
    return p->current_token.location;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.3 Error Recovery
 *
 * Instead of "pretending tokens exist", we synchronize to statement/decl
 * boundaries. This produces cleaner error recovery and prevents infinite loops.
 * ───────────────────────────────────────────────────────────────────────── */

static void Parser_Error(Parser *p, const char *message) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    p->had_error = true;
    Report_Error(p->current_token.location, "%s", message);
}

static void Parser_Error_At_Current(Parser *p, const char *expected) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    p->had_error = true;
    Report_Error(p->current_token.location, "expected %s, got %s",
                 expected, Token_Name[p->current_token.kind]);
}

/* Synchronize to a recovery point (statement/declaration boundary) */
static void Parser_Synchronize(Parser *p) {
    p->panic_mode = false;

    while (!Parser_At(p, TK_EOF)) {
        if (p->previous_token.kind == TK_SEMICOLON) return;

        switch (p->current_token.kind) {
            case TK_BEGIN: case TK_END: case TK_IF: case TK_CASE: case TK_LOOP:
            case TK_FOR: case TK_WHILE: case TK_RETURN: case TK_DECLARE:
            case TK_EXCEPTION: case TK_PROCEDURE: case TK_FUNCTION:
            case TK_PACKAGE: case TK_TASK: case TK_TYPE: case TK_SUBTYPE:
            case TK_PRAGMA: case TK_ACCEPT: case TK_SELECT:
                return;
            default:
                Parser_Advance(p);
        }
    }
}

/* Check for parser progress — detect stuck parsing loops */
static bool Parser_Check_Progress(Parser *p) {
    if (p->current_token.location.line == p->last_line &&
        p->current_token.location.column == p->last_column &&
        p->current_token.kind == p->last_kind) {
        Parser_Advance(p);
        return false;
    }
    p->last_line = p->current_token.location.line;
    p->last_column = p->current_token.location.column;
    p->last_kind = p->current_token.kind;
    return true;
}

/* Expect a specific token; report error and return false if not found */
static bool Parser_Expect(Parser *p, Token_Kind kind) {
    if (Parser_At(p, kind)) {
        Parser_Advance(p);
        return true;
    }
    Parser_Error_At_Current(p, Token_Name[kind]);
    return false;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.4 Identifier Parsing
 * ───────────────────────────────────────────────────────────────────────── */

static String_Slice Parser_Identifier(Parser *p) {
    if (!Parser_At(p, TK_IDENTIFIER)) {
        Parser_Error_At_Current(p, "identifier");
        return Empty_Slice;
    }
    String_Slice name = Slice_Duplicate(p->current_token.text);
    Parser_Advance(p);
    return name;
}

/* Check END identifier matches expected name */
static void Parser_Check_End_Name(Parser *p, String_Slice expected_name) {
    if (Parser_At(p, TK_IDENTIFIER)) {
        String_Slice end_name = p->current_token.text;
        if (!Slice_Equal_Ignore_Case(end_name, expected_name)) {
            Report_Error(p->current_token.location,
                        "END name does not match (expected '%.*s', got '%.*s')",
                        expected_name.length, expected_name.data,
                        end_name.length, end_name.data);
        }
        Parser_Advance(p);
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.5 Expression Parsing — Operator Precedence
 *
 * Ada precedence (highest to lowest):
 *   ** (right associative)
 *   ABS NOT (unary prefix)
 *   * / MOD REM
 *   + - & (binary) + - (unary)
 *   = /= < <= > >= IN NOT IN
 *   AND OR XOR AND THEN OR ELSE
 *
 * We use a clean precedence-climbing approach.
 * ───────────────────────────────────────────────────────────────────────── */

/* Forward declarations */
static Syntax_Node *Parse_Expression(Parser *p);
static Syntax_Node *Parse_Choice(Parser *p);
static Syntax_Node *Parse_Name(Parser *p);
static Syntax_Node *Parse_Simple_Name(Parser *p);  /* identifier or dotted, no parens/ticks */
static Syntax_Node *Parse_Subtype_Indication(Parser *p);
static Syntax_Node *Parse_Array_Type(Parser *p);
static void Parse_Association_List(Parser *p, Node_List *list);

/* Parse primary: literals, names, aggregates, allocators, parenthesized */
static Syntax_Node *Parse_Primary(Parser *p) {
    Source_Location loc = Parser_Location(p);

    /* Integer literal */
    if (Parser_At(p, TK_INTEGER)) {
        Syntax_Node *node = Node_New(NK_INTEGER, loc);
        node->integer_lit.value = p->current_token.integer_value;
        node->integer_lit.big_value = p->current_token.big_integer;
        Parser_Advance(p);
        return node;
    }

    /* Real literal */
    if (Parser_At(p, TK_REAL)) {
        Syntax_Node *node = Node_New(NK_REAL, loc);
        node->real_lit.value = p->current_token.float_value;
        Parser_Advance(p);
        return node;
    }

    /* Character literal */
    if (Parser_At(p, TK_CHARACTER)) {
        Syntax_Node *node = Node_New(NK_CHARACTER, loc);
        node->string_val.text = Slice_Duplicate(p->current_token.text);
        node->integer_lit.value = p->current_token.integer_value;
        Parser_Advance(p);
        return node;
    }

    /* String literal */
    if (Parser_At(p, TK_STRING)) {
        Syntax_Node *node = Node_New(NK_STRING, loc);
        node->string_val.text = Slice_Duplicate(p->current_token.text);
        Parser_Advance(p);
        return node;
    }

    /* NULL */
    if (Parser_Match(p, TK_NULL)) {
        return Node_New(NK_NULL, loc);
    }

    /* OTHERS (in aggregates) */
    if (Parser_Match(p, TK_OTHERS)) {
        return Node_New(NK_OTHERS, loc);
    }

    /* NEW allocator */
    if (Parser_Match(p, TK_NEW)) {
        Syntax_Node *node = Node_New(NK_ALLOCATOR, loc);
        node->allocator.subtype_mark = Parse_Subtype_Indication(p);
        if (Parser_Match(p, TK_TICK)) {
            Parser_Expect(p, TK_LPAREN);
            node->allocator.expression = Parse_Expression(p);
            Parser_Expect(p, TK_RPAREN);
        }
        return node;
    }

    /* Unary operators: NOT, ABS, +, - */
    if (Parser_At_Any(p, TK_NOT, TK_ABS) ||
        Parser_At_Any(p, TK_PLUS, TK_MINUS)) {
        Token_Kind op = p->current_token.kind;
        Parser_Advance(p);
        Syntax_Node *node = Node_New(NK_UNARY_OP, loc);
        node->unary.op = op;
        node->unary.operand = Parse_Primary(p);
        return node;
    }

    /* Parenthesized expression or aggregate */
    if (Parser_Match(p, TK_LPAREN)) {
        Syntax_Node *expr = Parse_Expression(p);

        /* Check for aggregate indicators */
        if (Parser_At(p, TK_COMMA) || Parser_At(p, TK_ARROW) ||
            Parser_At(p, TK_BAR) || Parser_At(p, TK_WITH)) {
            /* This is an aggregate */
            Syntax_Node *node = Node_New(NK_AGGREGATE, loc);

            if (Parser_Match(p, TK_WITH)) {
                /* Extension aggregate: (ancestor with components) */
                Node_List_Push(&node->aggregate.items, expr);
                node->aggregate.is_named = true;
                Parse_Association_List(p, &node->aggregate.items);
            } else if (Parser_At(p, TK_DOTDOT)) {
                /* First element is a range: expr .. high */
                Syntax_Node *range = Node_New(NK_RANGE, loc);
                range->range.low = expr;
                Parser_Advance(p);  /* consume .. */
                range->range.high = Parse_Expression(p);

                /* Check for choice list or named association */
                if (Parser_At(p, TK_BAR) || Parser_At(p, TK_ARROW)) {
                    Syntax_Node *assoc = Node_New(NK_ASSOCIATION, loc);
                    Node_List_Push(&assoc->association.choices, range);

                    while (Parser_Match(p, TK_BAR)) {
                        Node_List_Push(&assoc->association.choices, Parse_Choice(p));
                    }
                    if (Parser_Match(p, TK_ARROW)) {
                        assoc->association.expression = Parse_Expression(p);
                    }
                    Node_List_Push(&node->aggregate.items, assoc);
                } else {
                    Node_List_Push(&node->aggregate.items, range);
                }

                if (Parser_Match(p, TK_COMMA)) {
                    Parse_Association_List(p, &node->aggregate.items);
                }
            } else if (Parser_At(p, TK_BAR) || Parser_At(p, TK_ARROW)) {
                /* First element is part of a choice list */
                Syntax_Node *assoc = Node_New(NK_ASSOCIATION, loc);
                Node_List_Push(&assoc->association.choices, expr);

                /* Collect additional choices */
                while (Parser_Match(p, TK_BAR)) {
                    Node_List_Push(&assoc->association.choices, Parse_Choice(p));
                }

                /* Named association: choices => value */
                if (Parser_Match(p, TK_ARROW)) {
                    assoc->association.expression = Parse_Expression(p);
                }

                Node_List_Push(&node->aggregate.items, assoc);

                /* Continue with remaining associations */
                if (Parser_Match(p, TK_COMMA)) {
                    Parse_Association_List(p, &node->aggregate.items);
                }
            } else {
                /* First element is positional, followed by more */
                Node_List_Push(&node->aggregate.items, expr);
                Parser_Advance(p);  /* consume the comma we know is there */
                Parse_Association_List(p, &node->aggregate.items);
            }
            Parser_Expect(p, TK_RPAREN);
            return node;
        }

        Parser_Expect(p, TK_RPAREN);
        return expr;
    }

    /* Name (identifier, selected, indexed, etc.) */
    return Parse_Name(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.6 Unified Postfix Parsing
 *
 * Handles: .selector, 'attribute, (arguments) — all in one loop.
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Name(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *node;

    /* Base: identifier or operator symbol */
    if (Parser_At(p, TK_IDENTIFIER)) {
        node = Node_New(NK_IDENTIFIER, loc);
        node->string_val.text = Parser_Identifier(p);
    } else if (Parser_At(p, TK_STRING)) {
        /* Operator symbol as name: "+" etc */
        node = Node_New(NK_IDENTIFIER, loc);
        node->string_val.text = Slice_Duplicate(p->current_token.text);
        Parser_Advance(p);
    } else {
        Parser_Error_At_Current(p, "name");
        return Node_New(NK_IDENTIFIER, loc);
    }

    /* Postfix chain */
    for (;;) {
        Source_Location postfix_loc = Parser_Location(p);

        /* .selector or .ALL */
        if (Parser_Match(p, TK_DOT)) {
            if (Parser_Match(p, TK_ALL)) {
                /* Dereference: prefix.ALL */
                Syntax_Node *deref = Node_New(NK_UNARY_OP, postfix_loc);
                deref->unary.op = TK_ALL;
                deref->unary.operand = node;
                node = deref;
            } else {
                /* Selection: prefix.component */
                Syntax_Node *sel = Node_New(NK_SELECTED, postfix_loc);
                sel->selected.prefix = node;
                sel->selected.selector = Parser_Identifier(p);
                node = sel;
            }
            continue;
        }

        /* 'attribute or '(qualified) */
        if (Parser_Match(p, TK_TICK)) {
            if (Parser_Match(p, TK_LPAREN)) {
                /* Qualified expression: Type'(Expr) */
                Syntax_Node *qual = Node_New(NK_QUALIFIED, postfix_loc);
                qual->qualified.subtype_mark = node;
                qual->qualified.expression = Parse_Expression(p);
                Parser_Expect(p, TK_RPAREN);
                node = qual;
            } else {
                /* Attribute: prefix'Name or prefix'Name(arg) */
                Syntax_Node *attr = Node_New(NK_ATTRIBUTE, postfix_loc);
                attr->attribute.prefix = node;

                /* Attribute name can be reserved word or identifier */
                if (Parser_At(p, TK_IDENTIFIER)) {
                    attr->attribute.name = Parser_Identifier(p);
                } else if (Parser_At(p, TK_RANGE) || Parser_At(p, TK_DIGITS) ||
                           Parser_At(p, TK_DELTA) || Parser_At(p, TK_ACCESS) ||
                           Parser_At(p, TK_MOD)) {
                    attr->attribute.name = Slice_Duplicate(p->current_token.text);
                    Parser_Advance(p);
                } else {
                    Parser_Error_At_Current(p, "attribute name");
                }

                /* Optional attribute argument */
                if (Parser_Match(p, TK_LPAREN)) {
                    attr->attribute.argument = Parse_Expression(p);
                    Parser_Expect(p, TK_RPAREN);
                }
                node = attr;
            }
            continue;
        }

        /* (arguments) — call, index, slice, or type conversion */
        if (Parser_Match(p, TK_LPAREN)) {
            Syntax_Node *apply = Node_New(NK_APPLY, postfix_loc);
            apply->apply.prefix = node;
            Parse_Association_List(p, &apply->apply.arguments);
            Parser_Expect(p, TK_RPAREN);
            node = apply;
            continue;
        }

        break;
    }

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.6.1 Simple Name Parsing (no parentheses or attributes)
 *
 * Used for generic unit names in instantiations where we don't want
 * parentheses interpreted as function calls.
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Simple_Name(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *node;

    /* Base: identifier */
    if (Parser_At(p, TK_IDENTIFIER)) {
        node = Node_New(NK_IDENTIFIER, loc);
        node->string_val.text = Parser_Identifier(p);
    } else {
        Parser_Error_At_Current(p, "identifier");
        return Node_New(NK_IDENTIFIER, loc);
    }

    /* Only follow dotted selections, not parentheses or ticks */
    for (;;) {
        if (Parser_Match(p, TK_DOT)) {
            Source_Location sel_loc = Parser_Location(p);
            Syntax_Node *sel = Node_New(NK_SELECTED, sel_loc);
            sel->selected.prefix = node;
            sel->selected.selector = Parser_Identifier(p);
            node = sel;
            continue;
        }
        break;
    }

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.7 Unified Association Parsing
 *
 * Handles positional, named (=>), and choice (|) associations for:
 * - Aggregates
 * - Function/procedure calls
 * - Generic instantiations
 * ───────────────────────────────────────────────────────────────────────── */

/* Helper to parse a choice (expression or range) */
static Syntax_Node *Parse_Choice(Parser *p) {
    Source_Location loc = Parser_Location(p);

    /* OTHERS choice */
    if (Parser_Match(p, TK_OTHERS)) {
        return Node_New(NK_OTHERS, loc);
    }

    Syntax_Node *expr = Parse_Expression(p);

    /* Check if this is a range: expr .. expr */
    if (Parser_Match(p, TK_DOTDOT)) {
        Syntax_Node *range = Node_New(NK_RANGE, loc);
        range->range.low = expr;
        range->range.high = Parse_Expression(p);
        return range;
    }

    return expr;
}

static void Parse_Association_List(Parser *p, Node_List *list) {
    if (Parser_At(p, TK_RPAREN)) return;  /* Empty list */

    do {
        Source_Location loc = Parser_Location(p);
        Syntax_Node *first = Parse_Choice(p);

        /* Check for choice list with | or named association with => */
        if (Parser_At(p, TK_BAR) || Parser_At(p, TK_ARROW)) {
            Syntax_Node *assoc = Node_New(NK_ASSOCIATION, loc);
            Node_List_Push(&assoc->association.choices, first);

            /* Collect additional choices */
            while (Parser_Match(p, TK_BAR)) {
                Node_List_Push(&assoc->association.choices, Parse_Choice(p));
            }

            /* Named association: choices => value */
            if (Parser_Match(p, TK_ARROW)) {
                assoc->association.expression = Parse_Expression(p);
            }

            Node_List_Push(list, assoc);
        } else {
            /* Positional association */
            Node_List_Push(list, first);
        }
    } while (Parser_Match(p, TK_COMMA));
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.8 Binary Expression Parsing — Precedence Climbing
 * ───────────────────────────────────────────────────────────────────────── */

/* Precedence levels */
typedef enum {
    PREC_NONE = 0,
    PREC_LOGICAL,      /* AND, OR, XOR, AND_THEN, OR_ELSE */
    PREC_RELATIONAL,   /* = /= < <= > >= IN */
    PREC_ADDITIVE,     /* + - & */
    PREC_MULTIPLICATIVE, /* * / MOD REM */
    PREC_EXPONENTIAL,  /* ** */
    PREC_UNARY,        /* NOT ABS + - */
    PREC_PRIMARY
} Precedence;

static Precedence Get_Infix_Precedence(Token_Kind kind) {
    switch (kind) {
        case TK_AND: case TK_OR: case TK_XOR:
        case TK_AND_THEN: case TK_OR_ELSE:
            return PREC_LOGICAL;
        case TK_EQ: case TK_NE: case TK_LT: case TK_LE:
        case TK_GT: case TK_GE: case TK_IN: case TK_NOT:
            return PREC_RELATIONAL;
        case TK_PLUS: case TK_MINUS: case TK_AMPERSAND:
            return PREC_ADDITIVE;
        case TK_STAR: case TK_SLASH: case TK_MOD: case TK_REM:
            return PREC_MULTIPLICATIVE;
        case TK_EXPON:
            return PREC_EXPONENTIAL;
        default:
            return PREC_NONE;
    }
}

static bool Is_Right_Associative(Token_Kind kind) {
    return kind == TK_EXPON;
}

static Syntax_Node *Parse_Expression_Precedence(Parser *p, Precedence min_prec);

static Syntax_Node *Parse_Unary(Parser *p) {
    Source_Location loc = Parser_Location(p);

    if (Parser_At_Any(p, TK_PLUS, TK_MINUS) ||
        Parser_At_Any(p, TK_NOT, TK_ABS)) {
        Token_Kind op = p->current_token.kind;
        Parser_Advance(p);
        Syntax_Node *node = Node_New(NK_UNARY_OP, loc);
        node->unary.op = op;
        node->unary.operand = Parse_Unary(p);
        return node;
    }

    return Parse_Primary(p);
}

static Syntax_Node *Parse_Expression_Precedence(Parser *p, Precedence min_prec) {
    Syntax_Node *left = Parse_Unary(p);

    for (;;) {
        Token_Kind op = p->current_token.kind;
        Precedence prec = Get_Infix_Precedence(op);

        if (prec < min_prec) break;

        Source_Location loc = Parser_Location(p);
        Parser_Advance(p);

        /* Handle NOT IN specially */
        if (op == TK_NOT && Parser_At(p, TK_IN)) {
            Parser_Advance(p);
            Syntax_Node *node = Node_New(NK_BINARY_OP, loc);
            node->binary.op = TK_NOT;  /* NOT IN encoded as NOT with IN semantics */
            node->binary.left = left;
            node->binary.right = Parse_Expression_Precedence(p, prec + 1);
            left = node;
            continue;
        }

        /* Handle IN with possible range */
        if (op == TK_IN) {
            Syntax_Node *right = Parse_Expression_Precedence(p, prec + 1);

            /* Check for range: X in A .. B */
            if (Parser_Match(p, TK_DOTDOT)) {
                Syntax_Node *range = Node_New(NK_RANGE, loc);
                range->range.low = right;
                range->range.high = Parse_Expression_Precedence(p, prec + 1);
                right = range;
            }

            Syntax_Node *node = Node_New(NK_BINARY_OP, loc);
            node->binary.op = TK_IN;
            node->binary.left = left;
            node->binary.right = right;
            left = node;
            continue;
        }

        /* Standard binary operation */
        Precedence next_prec = Is_Right_Associative(op) ? prec : prec + 1;
        Syntax_Node *right = Parse_Expression_Precedence(p, next_prec);

        Syntax_Node *node = Node_New(NK_BINARY_OP, loc);
        node->binary.op = op;
        node->binary.left = left;
        node->binary.right = right;
        left = node;
    }

    return left;
}

static Syntax_Node *Parse_Expression(Parser *p) {
    return Parse_Expression_Precedence(p, PREC_LOGICAL);
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.9 Range Parsing
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Range(Parser *p) {
    Source_Location loc = Parser_Location(p);

    /* BOX: <> for unconstrained */
    if (Parser_Match(p, TK_BOX)) {
        return Node_New(NK_RANGE, loc);  /* Empty range = unconstrained */
    }

    Syntax_Node *low = Parse_Expression(p);

    if (Parser_Match(p, TK_DOTDOT)) {
        Syntax_Node *node = Node_New(NK_RANGE, loc);
        node->range.low = low;
        node->range.high = Parse_Expression(p);
        return node;
    }

    /* Could be a subtype name used as a range */
    return low;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.10 Subtype Indication Parsing
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Subtype_Indication(Parser *p) {
    Source_Location loc = Parser_Location(p);

    Syntax_Node *subtype_mark = Parse_Name(p);

    /* Check for constraint */
    if (Parser_Match(p, TK_RANGE)) {
        Syntax_Node *ind = Node_New(NK_SUBTYPE_INDICATION, loc);
        ind->subtype_ind.subtype_mark = subtype_mark;

        Syntax_Node *constraint = Node_New(NK_RANGE_CONSTRAINT, loc);
        constraint->range_constraint.range = Parse_Range(p);
        ind->subtype_ind.constraint = constraint;
        return ind;
    }

    if (Parser_Match(p, TK_LPAREN)) {
        /* Index or discriminant constraint */
        Syntax_Node *ind = Node_New(NK_SUBTYPE_INDICATION, loc);
        ind->subtype_ind.subtype_mark = subtype_mark;

        Syntax_Node *constraint = Node_New(NK_INDEX_CONSTRAINT, loc);
        do {
            Node_List_Push(&constraint->index_constraint.ranges, Parse_Range(p));
        } while (Parser_Match(p, TK_COMMA));
        Parser_Expect(p, TK_RPAREN);

        ind->subtype_ind.constraint = constraint;
        return ind;
    }

    return subtype_mark;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.11 Statement Parsing
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Forward declarations */
static Syntax_Node *Parse_Statement(Parser *p);
static void Parse_Statement_Sequence(Parser *p, Node_List *list);
static void Parse_Declarative_Part(Parser *p, Node_List *list);
static Syntax_Node *Parse_Declaration(Parser *p);
static Syntax_Node *Parse_Type_Definition(Parser *p);
static Syntax_Node *Parse_Subprogram_Body(Parser *p, Syntax_Node *spec);
static Syntax_Node *Parse_Block_Statement(Parser *p, String_Slice label);
static Syntax_Node *Parse_Loop_Statement(Parser *p, String_Slice label);
static Syntax_Node *Parse_Pragma(Parser *p);

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.1 Simple Statements
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Assignment_Or_Call(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *target = Parse_Name(p);

    if (Parser_Match(p, TK_ASSIGN)) {
        Syntax_Node *node = Node_New(NK_ASSIGNMENT, loc);
        node->assignment.target = target;
        node->assignment.value = Parse_Expression(p);
        return node;
    }

    /* Procedure call (target is already an NK_APPLY or NK_IDENTIFIER) */
    Syntax_Node *call = Node_New(NK_CALL_STMT, loc);
    call->assignment.target = target;  /* Reuse field for simplicity */
    return call;
}

static Syntax_Node *Parse_Return_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_RETURN);

    Syntax_Node *node = Node_New(NK_RETURN, loc);
    if (!Parser_At(p, TK_SEMICOLON)) {
        node->return_stmt.expression = Parse_Expression(p);
    }
    return node;
}

static Syntax_Node *Parse_Exit_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_EXIT);

    Syntax_Node *node = Node_New(NK_EXIT, loc);
    if (Parser_At(p, TK_IDENTIFIER)) {
        node->exit_stmt.loop_name = Parser_Identifier(p);
    }
    if (Parser_Match(p, TK_WHEN)) {
        node->exit_stmt.condition = Parse_Expression(p);
    }
    return node;
}

static Syntax_Node *Parse_Goto_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_GOTO);

    Syntax_Node *node = Node_New(NK_GOTO, loc);
    node->label_ref.name = Parser_Identifier(p);
    return node;
}

static Syntax_Node *Parse_Raise_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_RAISE);

    Syntax_Node *node = Node_New(NK_RAISE, loc);
    if (Parser_At(p, TK_IDENTIFIER)) {
        node->raise_stmt.exception_name = Parse_Name(p);
    }
    return node;
}

static Syntax_Node *Parse_Delay_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_DELAY);

    Syntax_Node *node = Node_New(NK_DELAY, loc);
    node->delay_stmt.expression = Parse_Expression(p);
    return node;
}

static Syntax_Node *Parse_Abort_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_ABORT);

    Syntax_Node *node = Node_New(NK_ABORT, loc);
    do {
        Node_List_Push(&node->abort_stmt.task_names, Parse_Name(p));
    } while (Parser_Match(p, TK_COMMA));
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.2 If Statement
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_If_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_IF);

    Syntax_Node *node = Node_New(NK_IF, loc);
    node->if_stmt.condition = Parse_Expression(p);
    Parser_Expect(p, TK_THEN);
    Parse_Statement_Sequence(p, &node->if_stmt.then_stmts);

    /* ELSIF parts */
    while (Parser_At(p, TK_ELSIF)) {
        Source_Location elsif_loc = Parser_Location(p);
        Parser_Advance(p);

        Syntax_Node *elsif = Node_New(NK_IF, elsif_loc);
        elsif->if_stmt.condition = Parse_Expression(p);
        Parser_Expect(p, TK_THEN);
        Parse_Statement_Sequence(p, &elsif->if_stmt.then_stmts);

        Node_List_Push(&node->if_stmt.elsif_parts, elsif);
    }

    /* ELSE part */
    if (Parser_Match(p, TK_ELSE)) {
        Parse_Statement_Sequence(p, &node->if_stmt.else_stmts);
    }

    Parser_Expect(p, TK_END);
    Parser_Expect(p, TK_IF);
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.3 Case Statement
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Case_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_CASE);

    Syntax_Node *node = Node_New(NK_CASE, loc);
    node->case_stmt.expression = Parse_Expression(p);
    Parser_Expect(p, TK_IS);

    /* Parse alternatives */
    while (Parser_At(p, TK_WHEN)) {
        Source_Location alt_loc = Parser_Location(p);
        Parser_Advance(p);

        Syntax_Node *alt = Node_New(NK_ASSOCIATION, alt_loc);

        /* Parse choices */
        do {
            Node_List_Push(&alt->association.choices, Parse_Expression(p));
        } while (Parser_Match(p, TK_BAR));

        Parser_Expect(p, TK_ARROW);

        /* Statements for this alternative stored as expression temporarily */
        Syntax_Node *stmts = Node_New(NK_BLOCK, alt_loc);
        Parse_Statement_Sequence(p, &stmts->block_stmt.statements);
        alt->association.expression = stmts;

        Node_List_Push(&node->case_stmt.alternatives, alt);
    }

    Parser_Expect(p, TK_END);
    Parser_Expect(p, TK_CASE);
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.4 Loop Statement
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Loop_Statement(Parser *p, String_Slice label) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *node = Node_New(NK_LOOP, loc);
    node->loop_stmt.label = label;

    /* WHILE loop */
    if (Parser_Match(p, TK_WHILE)) {
        node->loop_stmt.iteration_scheme = Parse_Expression(p);
    }
    /* FOR loop */
    else if (Parser_Match(p, TK_FOR)) {
        Source_Location for_loc = Parser_Location(p);
        Syntax_Node *iter = Node_New(NK_BINARY_OP, for_loc);
        iter->binary.op = TK_IN;

        /* Iterator identifier */
        Syntax_Node *id = Node_New(NK_IDENTIFIER, for_loc);
        id->string_val.text = Parser_Identifier(p);
        iter->binary.left = id;

        Parser_Expect(p, TK_IN);

        node->loop_stmt.is_reverse = Parser_Match(p, TK_REVERSE);

        /* Discrete range */
        iter->binary.right = Parse_Range(p);
        node->loop_stmt.iteration_scheme = iter;
    }

    Parser_Expect(p, TK_LOOP);
    Parse_Statement_Sequence(p, &node->loop_stmt.statements);
    Parser_Expect(p, TK_END);
    Parser_Expect(p, TK_LOOP);

    if (label.data && Parser_At(p, TK_IDENTIFIER)) {
        Parser_Check_End_Name(p, label);
    }

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.5 Block Statement
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Block_Statement(Parser *p, String_Slice label) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *node = Node_New(NK_BLOCK, loc);
    node->block_stmt.label = label;

    if (Parser_Match(p, TK_DECLARE)) {
        Parse_Declarative_Part(p, &node->block_stmt.declarations);
    }

    Parser_Expect(p, TK_BEGIN);
    Parse_Statement_Sequence(p, &node->block_stmt.statements);

    if (Parser_Match(p, TK_EXCEPTION)) {
        while (Parser_At(p, TK_WHEN)) {
            Source_Location h_loc = Parser_Location(p);
            Parser_Advance(p);

            Syntax_Node *handler = Node_New(NK_EXCEPTION_HANDLER, h_loc);

            /* Exception choices */
            do {
                if (Parser_Match(p, TK_OTHERS)) {
                    Node_List_Push(&handler->handler.exceptions, Node_New(NK_OTHERS, h_loc));
                } else {
                    Node_List_Push(&handler->handler.exceptions, Parse_Name(p));
                }
            } while (Parser_Match(p, TK_BAR));

            Parser_Expect(p, TK_ARROW);
            Parse_Statement_Sequence(p, &handler->handler.statements);

            Node_List_Push(&node->block_stmt.handlers, handler);
        }
    }

    Parser_Expect(p, TK_END);
    if (label.data && Parser_At(p, TK_IDENTIFIER)) {
        Parser_Check_End_Name(p, label);
    }

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.6 Accept Statement
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Accept_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_ACCEPT);

    Syntax_Node *node = Node_New(NK_ACCEPT, loc);
    node->accept_stmt.entry_name = Parser_Identifier(p);

    /* Optional index */
    if (Parser_Match(p, TK_LPAREN)) {
        node->accept_stmt.index = Parse_Expression(p);
        Parser_Expect(p, TK_RPAREN);
    }

    /* Optional formal part */
    if (Parser_Match(p, TK_LPAREN)) {
        Parse_Association_List(p, &node->accept_stmt.parameters);
        Parser_Expect(p, TK_RPAREN);
    }

    /* Optional body */
    if (Parser_Match(p, TK_DO)) {
        Parse_Statement_Sequence(p, &node->accept_stmt.statements);
        Parser_Expect(p, TK_END);
        if (Parser_At(p, TK_IDENTIFIER)) {
            Parser_Check_End_Name(p, node->accept_stmt.entry_name);
        }
    }

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.7 Select Statement
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Select_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_SELECT);

    Syntax_Node *node = Node_New(NK_SELECT, loc);

    /* Parse alternatives */
    do {
        Source_Location alt_loc = Parser_Location(p);

        if (Parser_Match(p, TK_WHEN)) {
            /* Guarded alternative */
            Syntax_Node *alt = Node_New(NK_ASSOCIATION, alt_loc);
            Node_List_Push(&alt->association.choices, Parse_Expression(p));
            Parser_Expect(p, TK_ARROW);
            alt->association.expression = Parse_Statement(p);
            Node_List_Push(&node->select_stmt.alternatives, alt);
        } else if (Parser_Match(p, TK_TERMINATE)) {
            Syntax_Node *term = Node_New(NK_NULL_STMT, alt_loc);
            Node_List_Push(&node->select_stmt.alternatives, term);
            Parser_Expect(p, TK_SEMICOLON);
        } else if (Parser_Match(p, TK_DELAY)) {
            Syntax_Node *delay = Node_New(NK_DELAY, alt_loc);
            delay->delay_stmt.expression = Parse_Expression(p);
            Parser_Expect(p, TK_SEMICOLON);
            Node_List_Push(&node->select_stmt.alternatives, delay);
        } else if (Parser_At(p, TK_ACCEPT)) {
            Node_List_Push(&node->select_stmt.alternatives, Parse_Accept_Statement(p));
        } else {
            break;
        }
    } while (Parser_Match(p, TK_OR));

    if (Parser_Match(p, TK_ELSE)) {
        node->select_stmt.else_part = Node_New(NK_BLOCK, loc);
        Parse_Statement_Sequence(p, &node->select_stmt.else_part->block_stmt.statements);
    }

    Parser_Expect(p, TK_END);
    Parser_Expect(p, TK_SELECT);
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.11.8 Statement Dispatch
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Statement(Parser *p) {
    Source_Location loc = Parser_Location(p);

    /* Check for label: <<label>> or identifier: */
    String_Slice label = Empty_Slice;

    if (Parser_Match(p, TK_LSHIFT)) {
        label = Parser_Identifier(p);
        Parser_Expect(p, TK_RSHIFT);

        Syntax_Node *lbl = Node_New(NK_LABEL, loc);
        lbl->label_ref.name = label;
        /* Label attaches to next statement */
    } else if (Parser_At(p, TK_IDENTIFIER)) {
        /* Lookahead for "identifier :" (label) vs assignment/call */
        /* This requires 2-token lookahead which we handle by trying */
    }

    /* Null statement */
    if (Parser_Match(p, TK_NULL)) {
        Parser_Expect(p, TK_SEMICOLON);
        return Node_New(NK_NULL_STMT, loc);
    }

    /* Compound statements */
    if (Parser_At(p, TK_IF)) return Parse_If_Statement(p);
    if (Parser_At(p, TK_CASE)) return Parse_Case_Statement(p);
    if (Parser_At(p, TK_LOOP) || Parser_At(p, TK_WHILE) || Parser_At(p, TK_FOR))
        return Parse_Loop_Statement(p, label);
    if (Parser_At(p, TK_DECLARE) || Parser_At(p, TK_BEGIN))
        return Parse_Block_Statement(p, label);
    if (Parser_At(p, TK_ACCEPT)) return Parse_Accept_Statement(p);
    if (Parser_At(p, TK_SELECT)) return Parse_Select_Statement(p);

    /* Simple statements */
    if (Parser_At(p, TK_RETURN)) return Parse_Return_Statement(p);
    if (Parser_At(p, TK_EXIT)) return Parse_Exit_Statement(p);
    if (Parser_At(p, TK_GOTO)) return Parse_Goto_Statement(p);
    if (Parser_At(p, TK_RAISE)) return Parse_Raise_Statement(p);
    if (Parser_At(p, TK_DELAY)) return Parse_Delay_Statement(p);
    if (Parser_At(p, TK_ABORT)) return Parse_Abort_Statement(p);

    /* Pragma in statement sequence (Ada 83 RM 2.8) */
    if (Parser_At(p, TK_PRAGMA)) return Parse_Pragma(p);

    /* Assignment or procedure call */
    return Parse_Assignment_Or_Call(p);
}

static void Parse_Statement_Sequence(Parser *p, Node_List *list) {
    while (!Parser_At(p, TK_EOF) &&
           !Parser_At(p, TK_END) &&
           !Parser_At(p, TK_ELSE) &&
           !Parser_At(p, TK_ELSIF) &&
           !Parser_At(p, TK_WHEN) &&
           !Parser_At(p, TK_EXCEPTION) &&
           !Parser_At(p, TK_OR)) {

        if (!Parser_Check_Progress(p)) break;

        Syntax_Node *stmt = Parse_Statement(p);
        Node_List_Push(list, stmt);

        if (!Parser_At(p, TK_END) && !Parser_At(p, TK_ELSE) &&
            !Parser_At(p, TK_ELSIF) && !Parser_At(p, TK_WHEN) &&
            !Parser_At(p, TK_EXCEPTION) && !Parser_At(p, TK_OR)) {
            Parser_Expect(p, TK_SEMICOLON);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.12 Declaration Parsing
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §9.12.1 Object Declaration (variables, constants)
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Object_Declaration(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *node = Node_New(NK_OBJECT_DECL, loc);

    /* Identifier list */
    do {
        Syntax_Node *id = Node_New(NK_IDENTIFIER, Parser_Location(p));
        id->string_val.text = Parser_Identifier(p);
        Node_List_Push(&node->object_decl.names, id);
    } while (Parser_Match(p, TK_COMMA));

    Parser_Expect(p, TK_COLON);

    /* Check for exception declaration: identifier_list : EXCEPTION */
    if (Parser_Match(p, TK_EXCEPTION)) {
        /* Convert to exception declaration */
        node->kind = NK_EXCEPTION_DECL;
        /* Names are already parsed into object_decl.names, copy to exception_decl.names */
        node->exception_decl.names = node->object_decl.names;
        return node;
    }

    node->object_decl.is_aliased = Parser_Match(p, TK_ACCESS);  /* ALIASED uses ACCESS token? */
    node->object_decl.is_constant = Parser_Match(p, TK_CONSTANT);

    /* Named number (number declaration): identifier : CONSTANT := static_expression; */
    /* No type specified, goes directly to := */
    if (!node->object_decl.is_constant || !Parser_At(p, TK_ASSIGN)) {
        /* Check for anonymous array type: ARRAY (...) OF ... */
        if (Parser_At(p, TK_ARRAY)) {
            node->object_decl.object_type = Parse_Array_Type(p);
        } else {
            node->object_decl.object_type = Parse_Subtype_Indication(p);
        }
    }

    /* Renames */
    if (Parser_Match(p, TK_RENAMES)) {
        node->kind = NK_SUBPROGRAM_RENAMING;  /* Repurpose for object renames */
        node->object_decl.init = Parse_Name(p);
        return node;
    }

    /* Initialization */
    if (Parser_Match(p, TK_ASSIGN)) {
        node->object_decl.init = Parse_Expression(p);
    }

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.12.2 Type Declaration
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Discriminant_Part(Parser *p) {
    if (!Parser_Match(p, TK_LPAREN)) return NULL;

    Source_Location loc = Parser_Location(p);
    Syntax_Node *disc_list = Node_New(NK_BLOCK, loc);  /* Container for discriminants */

    do {
        Source_Location d_loc = Parser_Location(p);
        Syntax_Node *disc = Node_New(NK_DISCRIMINANT_SPEC, d_loc);

        /* Name list */
        do {
            Syntax_Node *id = Node_New(NK_IDENTIFIER, Parser_Location(p));
            id->string_val.text = Parser_Identifier(p);
            Node_List_Push(&disc->discriminant.names, id);
        } while (Parser_Match(p, TK_COMMA));

        Parser_Expect(p, TK_COLON);
        disc->discriminant.disc_type = Parse_Subtype_Indication(p);

        if (Parser_Match(p, TK_ASSIGN)) {
            disc->discriminant.default_expr = Parse_Expression(p);
        }

        Node_List_Push(&disc_list->block_stmt.declarations, disc);
    } while (Parser_Match(p, TK_SEMICOLON));

    Parser_Expect(p, TK_RPAREN);
    return disc_list;
}

static Syntax_Node *Parse_Type_Declaration(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_TYPE);

    Syntax_Node *node = Node_New(NK_TYPE_DECL, loc);
    node->type_decl.name = Parser_Identifier(p);

    /* Discriminant part */
    if (Parser_At(p, TK_LPAREN)) {
        Syntax_Node *discs = Parse_Discriminant_Part(p);
        if (discs) {
            node->type_decl.discriminants = discs->block_stmt.declarations;
        }
    }

    /* Incomplete type declaration */
    if (Parser_Match(p, TK_SEMICOLON)) {
        return node;
    }

    Parser_Expect(p, TK_IS);

    node->type_decl.is_limited = Parser_Match(p, TK_LIMITED);
    node->type_decl.is_private = Parser_Match(p, TK_PRIVATE);

    if (!node->type_decl.is_private) {
        node->type_decl.definition = Parse_Type_Definition(p);
    }

    return node;
}

static Syntax_Node *Parse_Subtype_Declaration(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_SUBTYPE);

    Syntax_Node *node = Node_New(NK_SUBTYPE_DECL, loc);
    node->type_decl.name = Parser_Identifier(p);

    Parser_Expect(p, TK_IS);
    node->type_decl.definition = Parse_Subtype_Indication(p);

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.12.3 Type Definitions
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Enumeration_Type(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_LPAREN);

    Syntax_Node *node = Node_New(NK_ENUMERATION_TYPE, loc);

    do {
        Source_Location lit_loc = Parser_Location(p);
        Syntax_Node *lit = Node_New(NK_IDENTIFIER, lit_loc);

        if (Parser_At(p, TK_IDENTIFIER)) {
            lit->string_val.text = Parser_Identifier(p);
        } else if (Parser_At(p, TK_CHARACTER)) {
            lit->string_val.text = Slice_Duplicate(p->current_token.text);
            Parser_Advance(p);
        } else {
            Parser_Error_At_Current(p, "enumeration literal");
            break;
        }

        Node_List_Push(&node->enum_type.literals, lit);
    } while (Parser_Match(p, TK_COMMA));

    Parser_Expect(p, TK_RPAREN);
    return node;
}

static Syntax_Node *Parse_Discrete_Range(Parser *p);

static Syntax_Node *Parse_Array_Type(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_ARRAY);
    Parser_Expect(p, TK_LPAREN);

    Syntax_Node *node = Node_New(NK_ARRAY_TYPE, loc);

    /* Index types: can be discrete_subtype_indication or discrete_range */
    do {
        Syntax_Node *idx = Parse_Discrete_Range(p);
        /* Check for RANGE <> (unconstrained) */
        if (Parser_Match(p, TK_RANGE)) {
            Parser_Expect(p, TK_BOX);
            /* Mark as unconstrained */
            node->array_type.is_constrained = false;
        }
        Node_List_Push(&node->array_type.indices, idx);
    } while (Parser_Match(p, TK_COMMA));

    /* Determine if constrained based on what we parsed */
    node->array_type.is_constrained = true;
    for (size_t i = 0; i < node->array_type.indices.count; i++) {
        Syntax_Node *idx = node->array_type.indices.items[i];
        /* If any index has a BOX, it's unconstrained */
        if (idx->kind == NK_IDENTIFIER) {
            /* Check if we saw RANGE <> */
        }
    }

    Parser_Expect(p, TK_RPAREN);
    Parser_Expect(p, TK_OF);
    node->array_type.component_type = Parse_Subtype_Indication(p);

    return node;
}

/* Parse discrete_range: can be subtype_indication or range */
static Syntax_Node *Parse_Discrete_Range(Parser *p) {
    Source_Location loc = Parser_Location(p);

    /* Check if this starts with an integer literal (anonymous range) */
    if (Parser_At(p, TK_INTEGER) || Parser_At(p, TK_CHARACTER)) {
        Syntax_Node *range = Node_New(NK_RANGE, loc);
        range->range.low = Parse_Expression(p);
        if (Parser_Match(p, TK_DOTDOT)) {
            range->range.high = Parse_Expression(p);
        }
        return range;
    }

    /* Otherwise try to parse as name, then check for range or constraint */
    Syntax_Node *name = Parse_Name(p);

    if (Parser_Match(p, TK_RANGE)) {
        /* Type RANGE low..high or Type RANGE <> */
        if (Parser_At(p, TK_BOX)) {
            /* Unconstrained - just return the type mark */
            return name;
        }
        Syntax_Node *range = Node_New(NK_RANGE, loc);
        range->range.low = Parse_Expression(p);
        Parser_Expect(p, TK_DOTDOT);
        range->range.high = Parse_Expression(p);

        /* Create subtype indication with range constraint */
        Syntax_Node *ind = Node_New(NK_SUBTYPE_INDICATION, loc);
        ind->subtype_ind.subtype_mark = name;
        Syntax_Node *constraint = Node_New(NK_RANGE_CONSTRAINT, loc);
        constraint->range_constraint.range = range;
        ind->subtype_ind.constraint = constraint;
        return ind;
    }

    if (Parser_Match(p, TK_DOTDOT)) {
        /* Name is actually the low bound of a range */
        Syntax_Node *range = Node_New(NK_RANGE, loc);
        range->range.low = name;
        range->range.high = Parse_Expression(p);
        return range;
    }

    /* Just a type name */
    return name;
}

/* Forward declaration for variant part parsing */
static Syntax_Node *Parse_Variant_Part(Parser *p);

static Syntax_Node *Parse_Record_Type(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_RECORD);

    Syntax_Node *node = Node_New(NK_RECORD_TYPE, loc);

    /* NULL RECORD */
    if (Parser_Match(p, TK_NULL)) {
        node->record_type.is_null = true;
        return node;
    }

    /* Component list */
    while (!Parser_At(p, TK_END) && !Parser_At(p, TK_CASE) && !Parser_At(p, TK_EOF)) {
        if (!Parser_Check_Progress(p)) break;

        Source_Location c_loc = Parser_Location(p);
        Syntax_Node *comp = Node_New(NK_COMPONENT_DECL, c_loc);

        /* Component names */
        do {
            Syntax_Node *id = Node_New(NK_IDENTIFIER, Parser_Location(p));
            id->string_val.text = Parser_Identifier(p);
            Node_List_Push(&comp->component.names, id);
        } while (Parser_Match(p, TK_COMMA));

        Parser_Expect(p, TK_COLON);
        comp->component.component_type = Parse_Subtype_Indication(p);

        if (Parser_Match(p, TK_ASSIGN)) {
            comp->component.init = Parse_Expression(p);
        }

        Node_List_Push(&node->record_type.components, comp);
        Parser_Expect(p, TK_SEMICOLON);
    }

    /* Variant part */
    if (Parser_At(p, TK_CASE)) {
        node->record_type.variant_part = Parse_Variant_Part(p);
    }

    Parser_Expect(p, TK_END);
    Parser_Expect(p, TK_RECORD);
    return node;
}

static Syntax_Node *Parse_Variant_Part(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_CASE);

    Syntax_Node *node = Node_New(NK_VARIANT_PART, loc);
    node->variant_part.discriminant = Parser_Identifier(p);
    Parser_Expect(p, TK_IS);

    /* Variants */
    while (Parser_At(p, TK_WHEN)) {
        Source_Location v_loc = Parser_Location(p);
        Parser_Advance(p);

        Syntax_Node *variant = Node_New(NK_VARIANT, v_loc);

        /* Choices */
        do {
            Node_List_Push(&variant->variant.choices, Parse_Expression(p));
        } while (Parser_Match(p, TK_BAR));

        Parser_Expect(p, TK_ARROW);

        /* Components in this variant */
        while (!Parser_At(p, TK_WHEN) && !Parser_At(p, TK_END) &&
               !Parser_At(p, TK_CASE) && !Parser_At(p, TK_EOF)) {
            if (!Parser_Check_Progress(p)) break;

            Source_Location c_loc = Parser_Location(p);
            Syntax_Node *comp = Node_New(NK_COMPONENT_DECL, c_loc);

            do {
                Syntax_Node *id = Node_New(NK_IDENTIFIER, Parser_Location(p));
                id->string_val.text = Parser_Identifier(p);
                Node_List_Push(&comp->component.names, id);
            } while (Parser_Match(p, TK_COMMA));

            Parser_Expect(p, TK_COLON);
            comp->component.component_type = Parse_Subtype_Indication(p);

            if (Parser_Match(p, TK_ASSIGN)) {
                comp->component.init = Parse_Expression(p);
            }

            Node_List_Push(&variant->variant.components, comp);
            Parser_Expect(p, TK_SEMICOLON);
        }

        /* Nested variant part */
        if (Parser_At(p, TK_CASE)) {
            variant->variant.variant_part = Parse_Variant_Part(p);
        }

        Node_List_Push(&node->variant_part.variants, variant);
    }

    Parser_Expect(p, TK_END);
    Parser_Expect(p, TK_CASE);
    Parser_Expect(p, TK_SEMICOLON);
    return node;
}

static Syntax_Node *Parse_Access_Type(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_ACCESS);

    Syntax_Node *node = Node_New(NK_ACCESS_TYPE, loc);
    node->access_type.is_constant = Parser_Match(p, TK_CONSTANT);
    node->access_type.designated = Parse_Subtype_Indication(p);
    return node;
}

static Syntax_Node *Parse_Derived_Type(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_NEW);

    Syntax_Node *node = Node_New(NK_DERIVED_TYPE, loc);
    node->derived_type.parent_type = Parse_Subtype_Indication(p);

    if (Parser_At(p, TK_RANGE) || Parser_At(p, TK_LPAREN)) {
        node->derived_type.constraint = Parse_Subtype_Indication(p);
    }

    return node;
}

static Syntax_Node *Parse_Type_Definition(Parser *p) {
    Source_Location loc = Parser_Location(p);

    /* Enumeration type */
    if (Parser_At(p, TK_LPAREN)) {
        return Parse_Enumeration_Type(p);
    }

    /* Array type */
    if (Parser_At(p, TK_ARRAY)) {
        return Parse_Array_Type(p);
    }

    /* Record type */
    if (Parser_At(p, TK_RECORD)) {
        return Parse_Record_Type(p);
    }

    /* Access type */
    if (Parser_At(p, TK_ACCESS)) {
        return Parse_Access_Type(p);
    }

    /* Derived type */
    if (Parser_At(p, TK_NEW)) {
        return Parse_Derived_Type(p);
    }

    /* Integer types: range, mod */
    if (Parser_Match(p, TK_RANGE)) {
        Syntax_Node *node = Node_New(NK_INTEGER_TYPE, loc);
        node->integer_type.range = Parse_Range(p);
        return node;
    }

    if (Parser_Match(p, TK_MOD)) {
        Syntax_Node *node = Node_New(NK_INTEGER_TYPE, loc);
        Syntax_Node *mod_expr = Parse_Expression(p);
        /* Store modulus as integer value if constant */
        node->integer_type.modulus = 0;  /* Will be evaluated during semantic analysis */
        node->integer_type.range = mod_expr;
        return node;
    }

    /* Real types: digits, delta */
    if (Parser_Match(p, TK_DIGITS)) {
        Syntax_Node *node = Node_New(NK_REAL_TYPE, loc);
        node->real_type.precision = Parse_Expression(p);
        if (Parser_Match(p, TK_RANGE)) {
            node->real_type.range = Parse_Range(p);
        }
        return node;
    }

    if (Parser_Match(p, TK_DELTA)) {
        Syntax_Node *node = Node_New(NK_REAL_TYPE, loc);
        node->real_type.delta = Parse_Expression(p);
        if (Parser_Match(p, TK_RANGE)) {
            node->real_type.range = Parse_Range(p);
        }
        return node;
    }

    Parser_Error(p, "expected type definition");
    return Node_New(NK_INTEGER_TYPE, loc);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.13 Subprogram Declarations and Bodies
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §9.13.1 Parameter Specification
 * ───────────────────────────────────────────────────────────────────────── */

static void Parse_Parameter_List(Parser *p, Node_List *params) {
    if (!Parser_Match(p, TK_LPAREN)) return;

    do {
        Source_Location loc = Parser_Location(p);
        Syntax_Node *param = Node_New(NK_PARAM_SPEC, loc);

        /* Identifier list */
        do {
            Syntax_Node *id = Node_New(NK_IDENTIFIER, Parser_Location(p));
            id->string_val.text = Parser_Identifier(p);
            Node_List_Push(&param->param_spec.names, id);
        } while (Parser_Match(p, TK_COMMA));

        Parser_Expect(p, TK_COLON);

        /* Mode */
        if (Parser_Match(p, TK_IN)) {
            if (Parser_Match(p, TK_OUT)) {
                param->param_spec.mode = MODE_IN_OUT;
            } else {
                param->param_spec.mode = MODE_IN;
            }
        } else if (Parser_Match(p, TK_OUT)) {
            param->param_spec.mode = MODE_OUT;
        } else {
            param->param_spec.mode = MODE_IN;  /* Default */
        }

        param->param_spec.param_type = Parse_Subtype_Indication(p);

        /* Default expression */
        if (Parser_Match(p, TK_ASSIGN)) {
            param->param_spec.default_expr = Parse_Expression(p);
        }

        Node_List_Push(params, param);
    } while (Parser_Match(p, TK_SEMICOLON));

    Parser_Expect(p, TK_RPAREN);
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.13.2 Procedure/Function Specification
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Procedure_Specification(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_PROCEDURE);

    Syntax_Node *node = Node_New(NK_PROCEDURE_SPEC, loc);

    /* Name (can be identifier or operator string) */
    if (Parser_At(p, TK_STRING)) {
        node->subprogram_spec.name = Slice_Duplicate(p->current_token.text);
        Parser_Advance(p);
    } else {
        node->subprogram_spec.name = Parser_Identifier(p);
    }

    Parse_Parameter_List(p, &node->subprogram_spec.parameters);
    return node;
}

static Syntax_Node *Parse_Function_Specification(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_FUNCTION);

    Syntax_Node *node = Node_New(NK_FUNCTION_SPEC, loc);

    /* Name */
    if (Parser_At(p, TK_STRING)) {
        node->subprogram_spec.name = Slice_Duplicate(p->current_token.text);
        Parser_Advance(p);
    } else {
        node->subprogram_spec.name = Parser_Identifier(p);
    }

    Parse_Parameter_List(p, &node->subprogram_spec.parameters);

    Parser_Expect(p, TK_RETURN);
    node->subprogram_spec.return_type = Parse_Subtype_Indication(p);

    return node;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §9.13.3 Subprogram Body
 * ───────────────────────────────────────────────────────────────────────── */

static Syntax_Node *Parse_Subprogram_Body(Parser *p, Syntax_Node *spec) {
    Source_Location loc = spec ? spec->location : Parser_Location(p);
    bool is_function = spec && spec->kind == NK_FUNCTION_SPEC;

    Syntax_Node *node = Node_New(is_function ? NK_FUNCTION_BODY : NK_PROCEDURE_BODY, loc);
    node->subprogram_body.specification = spec;

    Parser_Expect(p, TK_IS);

    /* Check for SEPARATE */
    if (Parser_Match(p, TK_SEPARATE)) {
        node->subprogram_body.is_separate = true;
        return node;
    }

    Parse_Declarative_Part(p, &node->subprogram_body.declarations);

    Parser_Expect(p, TK_BEGIN);
    Parse_Statement_Sequence(p, &node->subprogram_body.statements);

    if (Parser_Match(p, TK_EXCEPTION)) {
        while (Parser_At(p, TK_WHEN)) {
            Source_Location h_loc = Parser_Location(p);
            Parser_Advance(p);

            Syntax_Node *handler = Node_New(NK_EXCEPTION_HANDLER, h_loc);

            do {
                if (Parser_Match(p, TK_OTHERS)) {
                    Node_List_Push(&handler->handler.exceptions, Node_New(NK_OTHERS, h_loc));
                } else {
                    Node_List_Push(&handler->handler.exceptions, Parse_Name(p));
                }
            } while (Parser_Match(p, TK_BAR));

            Parser_Expect(p, TK_ARROW);
            Parse_Statement_Sequence(p, &handler->handler.statements);

            Node_List_Push(&node->subprogram_body.handlers, handler);
        }
    }

    Parser_Expect(p, TK_END);
    if (spec && Parser_At(p, TK_IDENTIFIER)) {
        Parser_Check_End_Name(p, spec->subprogram_spec.name);
    }

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.14 Package Declarations and Bodies
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *Parse_Package_Specification(Parser *p) {
    /* Note: caller must consume TK_PACKAGE before calling */
    Source_Location loc = Parser_Location(p);

    Syntax_Node *node = Node_New(NK_PACKAGE_SPEC, loc);
    node->package_spec.name = Parser_Identifier(p);

    Parser_Expect(p, TK_IS);

    /* Visible declarations */
    Parse_Declarative_Part(p, &node->package_spec.visible_decls);

    /* Private part */
    if (Parser_Match(p, TK_PRIVATE)) {
        Parse_Declarative_Part(p, &node->package_spec.private_decls);
    }

    Parser_Expect(p, TK_END);
    if (Parser_At(p, TK_IDENTIFIER)) {
        Parser_Check_End_Name(p, node->package_spec.name);
    }

    return node;
}

static Syntax_Node *Parse_Package_Body(Parser *p) {
    /* Note: caller must consume TK_PACKAGE and TK_BODY before calling */
    Source_Location loc = Parser_Location(p);

    Syntax_Node *node = Node_New(NK_PACKAGE_BODY, loc);
    node->package_body.name = Parser_Identifier(p);

    Parser_Expect(p, TK_IS);

    /* Check for SEPARATE */
    if (Parser_Match(p, TK_SEPARATE)) {
        node->package_body.is_separate = true;
        return node;
    }

    Parse_Declarative_Part(p, &node->package_body.declarations);

    if (Parser_Match(p, TK_BEGIN)) {
        Parse_Statement_Sequence(p, &node->package_body.statements);

        if (Parser_Match(p, TK_EXCEPTION)) {
            while (Parser_At(p, TK_WHEN)) {
                Source_Location h_loc = Parser_Location(p);
                Parser_Advance(p);

                Syntax_Node *handler = Node_New(NK_EXCEPTION_HANDLER, h_loc);

                do {
                    if (Parser_Match(p, TK_OTHERS)) {
                        Node_List_Push(&handler->handler.exceptions, Node_New(NK_OTHERS, h_loc));
                    } else {
                        Node_List_Push(&handler->handler.exceptions, Parse_Name(p));
                    }
                } while (Parser_Match(p, TK_BAR));

                Parser_Expect(p, TK_ARROW);
                Parse_Statement_Sequence(p, &handler->handler.statements);

                Node_List_Push(&node->package_body.handlers, handler);
            }
        }
    }

    Parser_Expect(p, TK_END);
    if (Parser_At(p, TK_IDENTIFIER)) {
        Parser_Check_End_Name(p, node->package_body.name);
    }

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.15 Generic Units
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void Parse_Generic_Formal_Part(Parser *p, Node_List *formals) {
    while (!Parser_At(p, TK_PROCEDURE) && !Parser_At(p, TK_FUNCTION) &&
           !Parser_At(p, TK_PACKAGE) && !Parser_At(p, TK_EOF)) {

        if (!Parser_Check_Progress(p)) break;

        Source_Location loc = Parser_Location(p);

        /* Generic type formal: type T is private | type T is (<>) | etc */
        if (Parser_Match(p, TK_TYPE)) {
            Syntax_Node *formal = Node_New(NK_GENERIC_TYPE_PARAM, loc);

            /* Parse type name */
            formal->generic_type_param.name = Parser_Identifier(p);

            Parser_Expect(p, TK_IS);

            /* Parse type definition form */
            if (Parser_Match(p, TK_LIMITED)) {
                Parser_Expect(p, TK_PRIVATE);
                formal->generic_type_param.def_kind = 1;  /* LIMITED_PRIVATE */
            } else if (Parser_Match(p, TK_PRIVATE)) {
                formal->generic_type_param.def_kind = 0;  /* PRIVATE */
            } else if (Parser_Match(p, TK_LPAREN)) {
                /* (<>) for discrete types */
                Parser_Expect(p, TK_BOX);
                Parser_Expect(p, TK_RPAREN);
                formal->generic_type_param.def_kind = 2;  /* DISCRETE */
            } else if (Parser_Match(p, TK_RANGE)) {
                /* range <> for integer types */
                Parser_Expect(p, TK_BOX);
                formal->generic_type_param.def_kind = 3;  /* INTEGER */
            } else if (Parser_Match(p, TK_DIGITS)) {
                /* digits <> for float types */
                Parser_Expect(p, TK_BOX);
                formal->generic_type_param.def_kind = 4;  /* FLOAT */
            } else if (Parser_Match(p, TK_DELTA)) {
                /* delta <> for fixed types */
                Parser_Expect(p, TK_BOX);
                formal->generic_type_param.def_kind = 5;  /* FIXED */
            } else {
                /* Other forms - just consume until semicolon for now */
                formal->generic_type_param.def_kind = 0;
            }

            Node_List_Push(formals, formal);
            Parser_Expect(p, TK_SEMICOLON);
            continue;
        }

        /* Generic object formal */
        if (Parser_At(p, TK_IDENTIFIER)) {
            Syntax_Node *formal = Node_New(NK_GENERIC_OBJECT_PARAM, loc);
            Node_List_Push(formals, formal);
            Parser_Expect(p, TK_SEMICOLON);
            continue;
        }

        /* Generic subprogram formal */
        if (Parser_At(p, TK_WITH)) {
            Parser_Advance(p);
            Syntax_Node *formal = Node_New(NK_GENERIC_SUBPROGRAM_PARAM, loc);
            if (Parser_At(p, TK_PROCEDURE)) {
                /* Parse formal procedure */
            } else if (Parser_At(p, TK_FUNCTION)) {
                /* Parse formal function */
            }
            Node_List_Push(formals, formal);
            Parser_Expect(p, TK_SEMICOLON);
            continue;
        }

        break;
    }
}

static Syntax_Node *Parse_Generic_Declaration(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_GENERIC);

    Syntax_Node *node = Node_New(NK_GENERIC_DECL, loc);
    Parse_Generic_Formal_Part(p, &node->generic_decl.formals);

    /* The actual unit */
    if (Parser_At(p, TK_PROCEDURE)) {
        node->generic_decl.unit = Parse_Procedure_Specification(p);
    } else if (Parser_At(p, TK_FUNCTION)) {
        node->generic_decl.unit = Parse_Function_Specification(p);
    } else if (Parser_At(p, TK_PACKAGE)) {
        Parser_Advance(p);  /* consume PACKAGE */
        node->generic_decl.unit = Parse_Package_Specification(p);
    }

    return node;
}

__attribute__((unused))
static Syntax_Node *Parse_Generic_Instantiation(Parser *p, Token_Kind unit_kind) {
    Source_Location loc = Parser_Location(p);
    Parser_Advance(p);  /* consume PROCEDURE/FUNCTION/PACKAGE */

    Syntax_Node *node = Node_New(NK_GENERIC_INST, loc);
    node->generic_inst.unit_kind = unit_kind;
    node->generic_inst.instance_name = Parser_Identifier(p);

    Parser_Expect(p, TK_IS);
    Parser_Expect(p, TK_NEW);

    node->generic_inst.generic_name = Parse_Name(p);

    /* Generic actuals */
    if (Parser_Match(p, TK_LPAREN)) {
        Parse_Association_List(p, &node->generic_inst.actuals);
        Parser_Expect(p, TK_RPAREN);
    }

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.16 Use and With Clauses
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *Parse_Use_Clause(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_USE);

    Syntax_Node *node = Node_New(NK_USE_CLAUSE, loc);

    do {
        Node_List_Push(&node->use_clause.names, Parse_Name(p));
    } while (Parser_Match(p, TK_COMMA));

    return node;
}

static Syntax_Node *Parse_With_Clause(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_WITH);

    Syntax_Node *node = Node_New(NK_WITH_CLAUSE, loc);

    do {
        Node_List_Push(&node->use_clause.names, Parse_Name(p));
    } while (Parser_Match(p, TK_COMMA));

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.17 Pragmas
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *Parse_Pragma(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_PRAGMA);

    Syntax_Node *node = Node_New(NK_PRAGMA, loc);
    node->pragma_node.name = Parser_Identifier(p);

    if (Parser_Match(p, TK_LPAREN)) {
        Parse_Association_List(p, &node->pragma_node.arguments);
        Parser_Expect(p, TK_RPAREN);
    }

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.18 Exception Declaration
 * ═══════════════════════════════════════════════════════════════════════════
 */

__attribute__((unused))
static Syntax_Node *Parse_Exception_Declaration(Parser *p) {
    Source_Location loc = Parser_Location(p);

    Syntax_Node *node = Node_New(NK_EXCEPTION_DECL, loc);

    do {
        Syntax_Node *id = Node_New(NK_IDENTIFIER, Parser_Location(p));
        id->string_val.text = Parser_Identifier(p);
        Node_List_Push(&node->exception_decl.names, id);
    } while (Parser_Match(p, TK_COMMA));

    Parser_Expect(p, TK_COLON);
    Parser_Expect(p, TK_EXCEPTION);

    /* Renames */
    if (Parser_Match(p, TK_RENAMES)) {
        node->kind = NK_EXCEPTION_RENAMING;
        /* Parse renamed exception */
    }

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.19 Representation Clauses
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *Parse_Representation_Clause(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Parser_Expect(p, TK_FOR);

    Syntax_Node *node = Node_New(NK_REPRESENTATION_CLAUSE, loc);

    /* Parse: FOR entity_name'attribute USE expression;
     *    or: FOR type_name USE RECORD ... END RECORD;
     *    or: FOR type_name USE (enum_rep_list);
     *    or: FOR object_name USE AT address; */

    /* Parse entity name (possibly qualified: T or T'ATTRIBUTE) */
    node->rep_clause.entity_name = Parse_Name(p);

    /* Check if this is an attribute clause: FOR T'SIZE USE 32; */
    if (Parser_At(p, TK_TICK)) {
        Parser_Advance(p);  /* consume tick */
        if (Parser_At(p, TK_IDENTIFIER)) {
            node->rep_clause.attribute = p->current_token.text;
            Parser_Advance(p);  /* consume attribute name */
        }
    }

    Parser_Expect(p, TK_USE);

    /* Check for different representation clause forms */
    if (Parser_Match(p, TK_RECORD)) {
        /* Record representation clause: FOR T USE RECORD ... END RECORD; */
        node->rep_clause.is_record_rep = true;

        /* Parse optional alignment: AT MOD alignment; */
        if (Parser_Match(p, TK_AT)) {
            Parser_Expect(p, TK_MOD);
            node->rep_clause.expression = Parse_Expression(p);
            Parser_Expect(p, TK_SEMICOLON);
        }

        /* Parse component clauses: component_name AT position RANGE first_bit..last_bit; */
        while (!Parser_At(p, TK_END) && !Parser_At(p, TK_EOF)) {
            Syntax_Node *comp_clause = Node_New(NK_ASSOCIATION, Parser_Location(p));
            Node_List_Push(&comp_clause->association.choices, Parse_Name(p));
            Parser_Expect(p, TK_AT);
            comp_clause->association.expression = Parse_Expression(p);
            /* Optional RANGE clause */
            if (Parser_Match(p, TK_RANGE)) {
                /* bit_range is now part of the expression */
                Parse_Range(p);  /* first_bit .. last_bit */
            }
            Parser_Expect(p, TK_SEMICOLON);
            Node_List_Push(&node->rep_clause.component_clauses, comp_clause);
        }
        Parser_Expect(p, TK_END);
        Parser_Expect(p, TK_RECORD);
    } else if (Parser_At(p, TK_LPAREN)) {
        /* Enumeration representation: FOR T USE (A => 0, B => 1, ...); */
        node->rep_clause.is_enum_rep = true;
        Parser_Advance(p);  /* consume ( */
        do {
            Syntax_Node *enum_val = Parse_Expression(p);
            Node_List_Push(&node->rep_clause.component_clauses, enum_val);
        } while (Parser_Match(p, TK_COMMA));
        Parser_Expect(p, TK_RPAREN);
    } else if (Parser_Match(p, TK_AT)) {
        /* Address clause: FOR X USE AT address; */
        node->rep_clause.expression = Parse_Expression(p);
    } else {
        /* Attribute value: FOR T'SIZE USE 32; */
        node->rep_clause.expression = Parse_Expression(p);
    }

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.20 Declaration Dispatch
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *Parse_Declaration(Parser *p) {
    Source_Location loc = Parser_Location(p);

    /* Generic */
    if (Parser_At(p, TK_GENERIC)) {
        return Parse_Generic_Declaration(p);
    }

    /* Procedure/Function - could be spec, body, or generic instantiation */
    if (Parser_At(p, TK_PROCEDURE) || Parser_At(p, TK_FUNCTION)) {
        Token_Kind kind = p->current_token.kind;
        Parser_Advance(p);  /* consume PROCEDURE/FUNCTION */

        /* Get the name - can be identifier or operator string for functions */
        String_Slice name;
        if (Parser_At(p, TK_STRING)) {
            name = Slice_Duplicate(p->current_token.text);
            Parser_Advance(p);
        } else {
            name = Parser_Identifier(p);
        }

        /* Check for generic instantiation: NAME IS NEW */
        if (Parser_At(p, TK_IS)) {
            /* Peek ahead to see if it's IS NEW */
            Token saved = p->current_token;
            Lexer saved_lexer = p->lexer;
            Parser_Advance(p);  /* consume IS */

            if (Parser_At(p, TK_NEW)) {
                Parser_Advance(p);  /* consume NEW */

                /* Create generic instantiation node */
                Syntax_Node *node = Node_New(NK_GENERIC_INST, loc);
                node->generic_inst.unit_kind = kind;
                node->generic_inst.instance_name = name;

                /* Parse the generic unit name */
                node->generic_inst.generic_name = Parse_Simple_Name(p);

                /* Generic actuals */
                if (Parser_Match(p, TK_LPAREN)) {
                    Parse_Association_List(p, &node->generic_inst.actuals);
                    Parser_Expect(p, TK_RPAREN);
                }

                Parser_Expect(p, TK_SEMICOLON);
                return node;
            }

            /* Not IS NEW - restore and parse as spec/body */
            p->current_token = saved;
            p->lexer = saved_lexer;
        }

        /* Parse parameters (if any) - Parse_Parameter_List handles the parens */
        Node_List params = {0};
        if (Parser_At(p, TK_LPAREN)) {
            Parse_Parameter_List(p, &params);
        }

        /* Create the spec node */
        Syntax_Node *spec = Node_New(kind == TK_PROCEDURE ? NK_PROCEDURE_SPEC : NK_FUNCTION_SPEC, loc);
        spec->subprogram_spec.name = name;
        spec->subprogram_spec.parameters = params;

        /* For functions, parse return type */
        if (kind == TK_FUNCTION) {
            Parser_Expect(p, TK_RETURN);
            spec->subprogram_spec.return_type = Parse_Name(p);
        }

        /* Check for body or just spec */
        if (Parser_At(p, TK_IS)) {
            return Parse_Subprogram_Body(p, spec);
        }

        /* Just a specification */
        Parser_Expect(p, TK_SEMICOLON);
        return spec;
    }

    /* Package */
    if (Parser_At(p, TK_PACKAGE)) {
        Parser_Advance(p);  /* consume PACKAGE */
        if (Parser_At(p, TK_BODY)) {
            Parser_Advance(p);  /* consume BODY */
            return Parse_Package_Body(p);
        }
        return Parse_Package_Specification(p);
    }

    /* Task declaration */
    if (Parser_At(p, TK_TASK)) {
        Parser_Advance(p);  /* consume TASK */

        /* TASK BODY name IS ... */
        if (Parser_At(p, TK_BODY)) {
            Parser_Advance(p);  /* consume BODY */
            Source_Location t_loc = Parser_Location(p);
            Syntax_Node *node = Node_New(NK_TASK_BODY, t_loc);
            node->task_body.name = Parser_Identifier(p);
            Parser_Expect(p, TK_IS);

            if (Parser_Match(p, TK_SEPARATE)) {
                Parser_Expect(p, TK_SEMICOLON);
                return node;
            }

            Parse_Declarative_Part(p, &node->task_body.declarations);
            Parser_Expect(p, TK_BEGIN);
            Parse_Statement_Sequence(p, &node->task_body.statements);

            if (Parser_Match(p, TK_EXCEPTION)) {
                while (Parser_At(p, TK_WHEN)) {
                    Source_Location h_loc = Parser_Location(p);
                    Parser_Advance(p);

                    Syntax_Node *handler = Node_New(NK_EXCEPTION_HANDLER, h_loc);
                    do {
                        if (Parser_Match(p, TK_OTHERS)) {
                            Node_List_Push(&handler->handler.exceptions, Node_New(NK_OTHERS, h_loc));
                        } else {
                            Node_List_Push(&handler->handler.exceptions, Parse_Name(p));
                        }
                    } while (Parser_Match(p, TK_BAR));

                    Parser_Expect(p, TK_ARROW);
                    Parse_Statement_Sequence(p, &handler->handler.statements);
                    Node_List_Push(&node->task_body.handlers, handler);
                }
            }

            Parser_Expect(p, TK_END);
            if (Parser_At(p, TK_IDENTIFIER)) {
                Parser_Check_End_Name(p, node->task_body.name);
            }
            Parser_Expect(p, TK_SEMICOLON);
            return node;
        }

        /* TASK [TYPE] name [IS ... END name]; */
        bool is_type = Parser_Match(p, TK_TYPE);
        Source_Location t_loc = Parser_Location(p);
        Syntax_Node *node = Node_New(NK_TASK_SPEC, t_loc);
        node->task_spec.name = Parser_Identifier(p);
        node->task_spec.is_type = is_type;

        if (Parser_Match(p, TK_IS)) {
            /* Task spec with entries */
            while (!Parser_At(p, TK_END) && !Parser_At(p, TK_EOF)) {
                if (!Parser_Check_Progress(p)) break;

                if (Parser_Match(p, TK_ENTRY)) {
                    Source_Location e_loc = Parser_Location(p);
                    Syntax_Node *entry = Node_New(NK_ENTRY_DECL, e_loc);
                    entry->entry_decl.name = Parser_Identifier(p);

                    /* Entry may have family index: ENTRY name(index) */
                    /* and/or parameters: ENTRY name(...) or ENTRY name(index)(...) */
                    if (Parser_At(p, TK_LPAREN)) {
                        /* For simplicity, parse as parameter list */
                        Parse_Parameter_List(p, &entry->entry_decl.parameters);
                    }
                    Parser_Expect(p, TK_SEMICOLON);
                    Node_List_Push(&node->task_spec.entries, entry);
                } else if (Parser_At(p, TK_PRAGMA)) {
                    Node_List_Push(&node->task_spec.entries, Parse_Pragma(p));
                } else if (Parser_At(p, TK_FOR)) {
                    /* Representation clause in task spec */
                    Node_List_Push(&node->task_spec.entries, Parse_Representation_Clause(p));
                } else {
                    Parser_Error(p, "expected ENTRY, PRAGMA, FOR, or END in task spec");
                    Parser_Advance(p);
                }
            }
            Parser_Expect(p, TK_END);
            if (Parser_At(p, TK_IDENTIFIER)) {
                Parser_Check_End_Name(p, node->task_spec.name);
            }
        }

        Parser_Expect(p, TK_SEMICOLON);
        return node;
    }

    /* Type declaration */
    if (Parser_At(p, TK_TYPE)) {
        return Parse_Type_Declaration(p);
    }

    /* Subtype declaration */
    if (Parser_At(p, TK_SUBTYPE)) {
        return Parse_Subtype_Declaration(p);
    }

    /* Use clause */
    if (Parser_At(p, TK_USE)) {
        return Parse_Use_Clause(p);
    }

    /* Pragma */
    if (Parser_At(p, TK_PRAGMA)) {
        return Parse_Pragma(p);
    }

    /* FOR representation clause */
    if (Parser_At(p, TK_FOR)) {
        return Parse_Representation_Clause(p);
    }

    /* Object or exception declaration */
    if (Parser_At(p, TK_IDENTIFIER)) {
        /* Need lookahead to distinguish object from exception */
        Syntax_Node *node = Parse_Object_Declaration(p);

        /* Check if it was actually an exception */
        /* The object parser handles this via Parse_Subtype_Indication seeing EXCEPTION */
        return node;
    }

    Parser_Error(p, "expected declaration");
    Parser_Synchronize(p);
    return Node_New(NK_NULL_STMT, loc);
}

static void Parse_Declarative_Part(Parser *p, Node_List *list) {
    while (!Parser_At(p, TK_BEGIN) && !Parser_At(p, TK_END) &&
           !Parser_At(p, TK_PRIVATE) && !Parser_At(p, TK_EOF)) {

        if (!Parser_Check_Progress(p)) break;

        Syntax_Node *decl = Parse_Declaration(p);
        Node_List_Push(list, decl);

        Parser_Match(p, TK_SEMICOLON);  /* Optional here as some decls include it */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §9.21 Compilation Unit
 * ═══════════════════════════════════════════════════════════════════════════
 */

static Syntax_Node *Parse_Context_Clause(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *node = Node_New(NK_CONTEXT_CLAUSE, loc);

    while (Parser_At(p, TK_WITH) || Parser_At(p, TK_USE) || Parser_At(p, TK_PRAGMA)) {
        if (Parser_At(p, TK_WITH)) {
            Node_List_Push(&node->context.with_clauses, Parse_With_Clause(p));
            Parser_Expect(p, TK_SEMICOLON);
        } else if (Parser_At(p, TK_USE)) {
            Node_List_Push(&node->context.use_clauses, Parse_Use_Clause(p));
            Parser_Expect(p, TK_SEMICOLON);
        } else if (Parser_At(p, TK_PRAGMA)) {
            Parse_Pragma(p);  /* Configuration pragmas */
            Parser_Expect(p, TK_SEMICOLON);
        }
    }

    return node;
}

static Syntax_Node *Parse_Compilation_Unit(Parser *p) {
    Source_Location loc = Parser_Location(p);
    Syntax_Node *node = Node_New(NK_COMPILATION_UNIT, loc);

    node->compilation_unit.context = Parse_Context_Clause(p);

    /* Separate unit */
    if (Parser_Match(p, TK_SEPARATE)) {
        Parser_Expect(p, TK_LPAREN);
        Parse_Name(p);  /* Parent unit name */
        Parser_Expect(p, TK_RPAREN);
        /* Parse the actual subunit */
    }

    /* Main unit */
    node->compilation_unit.unit = Parse_Declaration(p);
    Parser_Match(p, TK_SEMICOLON);

    return node;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §10. TYPE SYSTEM — Ada Type Semantics
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ada's type system is nominally typed with structural subtyping for
 * anonymous types. Key features:
 *
 * - Type ≠ Subtype: Types define structure; subtypes add constraints
 * - Derived types: New types from existing, with inherited operations
 * - Universal types: Universal_Integer, Universal_Real for literals
 * - Class-wide types (Ada 95+): Not in Ada 83
 *
 * INVARIANT: All sizes are stored in BYTES, not bits.
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §10.1 Type Kinds
 * ───────────────────────────────────────────────────────────────────────── */

typedef enum {
    TYPE_UNKNOWN = 0,

    /* Scalar types */
    TYPE_BOOLEAN,
    TYPE_CHARACTER,
    TYPE_INTEGER,
    TYPE_MODULAR,
    TYPE_ENUMERATION,
    TYPE_FLOAT,
    TYPE_FIXED,

    /* Composite types */
    TYPE_ARRAY,
    TYPE_RECORD,
    TYPE_STRING,      /* Special case of array */

    /* Access types */
    TYPE_ACCESS,

    /* Special types */
    TYPE_UNIVERSAL_INTEGER,
    TYPE_UNIVERSAL_REAL,
    TYPE_TASK,
    TYPE_SUBPROGRAM,  /* For formal subprogram parameters */
    TYPE_PRIVATE,
    TYPE_LIMITED_PRIVATE,
    TYPE_INCOMPLETE,
    TYPE_PACKAGE,     /* For package namespaces */

    TYPE_COUNT
} Type_Kind;

/* ─────────────────────────────────────────────────────────────────────────
 * §10.2 Type Information Structure
 *
 * Each type has:
 * - Kind and name
 * - Size and alignment (in BYTES)
 * - Bounds for scalars
 * - Component info for composites
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct Type_Info Type_Info;
typedef struct Symbol Symbol;

/* Bound representation: explicit tagged union to avoid bitcast hacks */
typedef struct {
    enum { BOUND_INTEGER, BOUND_FLOAT, BOUND_EXPR } kind;
    union {
        int64_t      int_value;
        double       float_value;
        Syntax_Node *expr;
    };
} Type_Bound;

/* Component information for records */
typedef struct {
    String_Slice  name;
    Type_Info    *component_type;
    uint32_t      byte_offset;
    uint32_t      bit_offset;    /* For representation clauses */
    uint32_t      bit_size;
} Component_Info;

/* Index information for arrays */
typedef struct {
    Type_Info *index_type;
    Type_Bound low_bound;
    Type_Bound high_bound;
} Index_Info;

struct Type_Info {
    Type_Kind    kind;
    String_Slice name;
    Symbol      *defining_symbol;

    /* Size and alignment in BYTES (not bits!) */
    uint32_t     size;
    uint32_t     alignment;

    /* Scalar bounds */
    Type_Bound   low_bound;
    Type_Bound   high_bound;
    int64_t      modulus;        /* For modular types */

    /* Base/parent type for subtypes and derived types */
    Type_Info   *base_type;
    Type_Info   *parent_type;    /* For derived types */

    /* Composite type info */
    union {
        struct {  /* TYPE_ARRAY */
            Index_Info *indices;
            uint32_t    index_count;
            Type_Info  *element_type;
            bool        is_constrained;
        } array;

        struct {  /* TYPE_RECORD */
            Component_Info *components;
            uint32_t        component_count;
            /* Discriminant info would go here */
        } record;

        struct {  /* TYPE_ACCESS */
            Type_Info *designated_type;
            bool       is_access_constant;
        } access;

        struct {  /* TYPE_ENUMERATION */
            String_Slice *literals;
            uint32_t      literal_count;
            int64_t      *rep_values;    /* Optional representation clause values */
        } enumeration;

        struct {  /* TYPE_FIXED */
            double delta;   /* User-specified delta (smallest increment) */
            double small;   /* Implementation small: power of 2 <= delta */
            int    scale;   /* Scale factor: value = mantissa * 2^scale */
        } fixed;
    };

    /* Runtime check suppression */
    uint32_t     suppressed_checks;

    /* Pragma Pack - pack components to minimum size */
    bool         is_packed;

    /* Freezing status - once frozen, representation cannot change */
    bool         is_frozen;

    /* Implicitly generated equality function name (set at freeze time) */
    const char  *equality_func_name;
};

/* ─────────────────────────────────────────────────────────────────────────
 * §10.2.1 Frozen Composite Types List
 *
 * Track composite types that need implicit equality operators.
 * These are added during Freeze_Type and processed during code generation.
 * ───────────────────────────────────────────────────────────────────────── */

static Type_Info *Frozen_Composite_Types[256];
static uint32_t   Frozen_Composite_Count = 0;

/* Global list of exception symbols for code generation */
static Symbol    *Exception_Symbols[256];
static uint32_t   Exception_Symbol_Count = 0;

/* ─────────────────────────────────────────────────────────────────────────
 * §10.3 Type Construction
 * ───────────────────────────────────────────────────────────────────────── */

static Type_Info *Type_New(Type_Kind kind, String_Slice name) {
    Type_Info *t = Arena_Allocate(sizeof(Type_Info));
    t->kind = kind;
    t->name = name;
    t->size = Default_Size_Bytes;
    t->alignment = Default_Align_Bytes;
    return t;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §10.4 Type Predicates
 * ───────────────────────────────────────────────────────────────────────── */

static inline bool Type_Is_Scalar(const Type_Info *t) {
    return t && t->kind >= TYPE_BOOLEAN && t->kind <= TYPE_FIXED;
}

static inline bool Type_Is_Discrete(const Type_Info *t) {
    return t && (t->kind == TYPE_BOOLEAN || t->kind == TYPE_CHARACTER ||
                 t->kind == TYPE_INTEGER || t->kind == TYPE_MODULAR ||
                 t->kind == TYPE_ENUMERATION);
}

static inline bool Type_Is_Numeric(const Type_Info *t) {
    return t && (t->kind == TYPE_INTEGER || t->kind == TYPE_MODULAR ||
                 t->kind == TYPE_FLOAT || t->kind == TYPE_FIXED ||
                 t->kind == TYPE_UNIVERSAL_INTEGER || t->kind == TYPE_UNIVERSAL_REAL);
}

static inline bool Type_Is_Real(const Type_Info *t) {
    return t && (t->kind == TYPE_FLOAT || t->kind == TYPE_FIXED ||
                 t->kind == TYPE_UNIVERSAL_REAL);
}

static inline bool Type_Is_Composite(const Type_Info *t) {
    return t && (t->kind == TYPE_ARRAY || t->kind == TYPE_RECORD ||
                 t->kind == TYPE_STRING);
}

static inline bool Type_Is_Access(const Type_Info *t) {
    return t && t->kind == TYPE_ACCESS;
}

/* Check if array type is unconstrained (needs fat pointer representation) */
static inline bool Type_Is_Unconstrained_Array(const Type_Info *t) {
    return t && (t->kind == TYPE_ARRAY || t->kind == TYPE_STRING) &&
           !t->array.is_constrained;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §10.5 Base Type Traversal
 *
 * Per RM 3.3.1: The base type of a type is the ultimate ancestor.
 * For subtypes, follow base_type links to find the original type.
 * ───────────────────────────────────────────────────────────────────────── */

static Type_Info *Type_Base(Type_Info *t) {
    while (t && t->base_type) t = t->base_type;
    return t;
}

/*
 * NOTE: Type compatibility checking is consolidated in Type_Covers()
 * defined in §11.6.2 (Overload Resolution section) following GNAT's
 * sem_type.adb Covers function. That function provides comprehensive
 * coverage checking for:
 * - Same type identity
 * - Universal type compatibility
 * - Base type matching
 * - Array/string structural compatibility
 * - Access type designated type compatibility
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §10.6 Type Freezing
 *
 * Freezing determines the point at which a type's representation is fixed.
 * Per RM 13.14 and GNAT design:
 * - Types are frozen by object declarations, bodies, end of declarative part
 * - Subtypes freeze their base type
 * - Composite types freeze their component types
 * - Once frozen, size/alignment/layout cannot change
 * ───────────────────────────────────────────────────────────────────────── */

/* Forward declaration for Symbol */
typedef struct Symbol Symbol;

/* Freeze a type and all its dependencies
 * Per RM 13.14: When a type is frozen, its representation is fixed */
static void Freeze_Type(Type_Info *t) {
    if (!t || t->is_frozen) return;

    /* Mark as frozen first to prevent infinite recursion */
    t->is_frozen = true;

    /* Freeze base type if present */
    if (t->base_type) {
        Freeze_Type(t->base_type);
    }

    /* Freeze parent type for derived types */
    if (t->parent_type) {
        Freeze_Type(t->parent_type);
    }

    /* Freeze component types for composites */
    switch (t->kind) {
        case TYPE_ARRAY:
        case TYPE_STRING:
            /* Freeze element type */
            if (t->array.element_type) {
                Freeze_Type(t->array.element_type);
            }
            /* Freeze index types */
            for (uint32_t i = 0; i < t->array.index_count; i++) {
                if (t->array.indices[i].index_type) {
                    Freeze_Type(t->array.indices[i].index_type);
                }
            }
            break;

        case TYPE_RECORD:
            /* Freeze all component types */
            for (uint32_t i = 0; i < t->record.component_count; i++) {
                if (t->record.components[i].component_type) {
                    Freeze_Type(t->record.components[i].component_type);
                }
            }
            break;

        case TYPE_ACCESS:
            /* Access type freezing does NOT freeze designated type */
            /* Per RM 13.14: "Freezing an access type does not freeze
               its designated subtype" */
            break;

        default:
            break;
    }

    /* Register composite types for implicit equality function generation
     * Per RM 4.5.2: Equality is predefined for all non-limited types */
    if (Type_Is_Composite(t) && Frozen_Composite_Count < 256) {
        Frozen_Composite_Types[Frozen_Composite_Count++] = t;

        /* Generate a unique function name for this type's equality */
        char *name_buf = Arena_Allocate(64);
        snprintf(name_buf, 64, "_ada_eq_%.*s_%u",
                 (int)(t->name.length > 20 ? 20 : t->name.length),
                 t->name.data,
                 Frozen_Composite_Count);
        t->equality_func_name = name_buf;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §10.7 LLVM Type Mapping
 * ───────────────────────────────────────────────────────────────────────── */

/* Forward declarations for array helpers (defined after Type_Bound_Value) */
static int64_t Type_Bound_Value(Type_Bound b);
static int64_t Array_Element_Count(Type_Info *t);
static int64_t Array_Low_Bound(Type_Info *t);

static const char *Type_To_Llvm(Type_Info *t) {
    if (!t) return "i64";

    switch (t->kind) {
        case TYPE_BOOLEAN:    return "i1";
        case TYPE_CHARACTER:  return "i8";
        case TYPE_INTEGER:
        case TYPE_MODULAR:
        case TYPE_ENUMERATION:
        case TYPE_UNIVERSAL_INTEGER:
        case TYPE_FIXED:  /* Fixed-point uses scaled integer representation */
            return Llvm_Int_Type((uint32_t)To_Bits(t->size));
        case TYPE_FLOAT:
        case TYPE_UNIVERSAL_REAL:
            return Llvm_Float_Type((uint32_t)To_Bits(t->size));
        case TYPE_ACCESS:
        case TYPE_RECORD:
        case TYPE_TASK:
            return "ptr";
        case TYPE_ARRAY:
            /* Unconstrained arrays use fat pointers, constrained use ptr */
            return (t->array.is_constrained) ? "ptr" : "{ ptr, { i64, i64 } }";
        case TYPE_STRING:
            /* STRING is always unconstrained array of CHARACTER */
            return "{ ptr, { i64, i64 } }";
        default:
            return "i64";
    }
}


/* ═══════════════════════════════════════════════════════════════════════════
 * §11. SYMBOL TABLE — Scoped Name Resolution
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The symbol table implements Ada's visibility and overloading rules:
 *
 * - Hierarchical scopes (packages can nest, blocks create new scopes)
 * - Overloading: same name, different parameter profiles
 * - Use clauses: make names directly visible without qualification
 * - Visibility: immediately visible, use-visible, directly visible
 *
 * Design: Hash table with chaining, scope stack for nested contexts.
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §11.1 Symbol Kinds
 * ───────────────────────────────────────────────────────────────────────── */

typedef enum {
    SYMBOL_UNKNOWN = 0,
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_TYPE,
    SYMBOL_SUBTYPE,
    SYMBOL_PROCEDURE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER,
    SYMBOL_PACKAGE,
    SYMBOL_EXCEPTION,
    SYMBOL_LABEL,
    SYMBOL_LOOP,
    SYMBOL_ENTRY,
    SYMBOL_COMPONENT,
    SYMBOL_DISCRIMINANT,
    SYMBOL_LITERAL,      /* Enumeration literal */
    SYMBOL_GENERIC,
    SYMBOL_GENERIC_INSTANCE,
    SYMBOL_COUNT
} Symbol_Kind;

/* ─────────────────────────────────────────────────────────────────────────
 * §11.2 Symbol Structure
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct Symbol Symbol;
typedef struct Scope Scope;

/* Parameter mode */
typedef enum {
    PARAM_IN = 0,
    PARAM_OUT,
    PARAM_IN_OUT
} Parameter_Mode;

/* Parameter information for subprograms */
typedef struct {
    String_Slice    name;
    Type_Info      *param_type;
    Parameter_Mode  mode;
    Syntax_Node    *default_value;
    struct Symbol  *param_sym;    /* Symbol for this parameter in function body */
} Parameter_Info;

/* Check if parameter mode requires pass-by-reference (OUT or IN OUT) */
static inline bool Param_Is_By_Reference(Parameter_Mode mode) {
    return mode == PARAM_OUT || mode == PARAM_IN_OUT;
}

struct Symbol {
    Symbol_Kind     kind;
    String_Slice    name;
    Source_Location location;

    /* Type information */
    Type_Info      *type;

    /* Scope membership */
    Scope          *defining_scope;
    Symbol         *parent;         /* Enclosing package/subprogram symbol */

    /* Overloading chain */
    Symbol         *next_overload;

    /* Hash table chaining */
    Symbol         *next_in_bucket;

    /* Visibility */
    enum {
        VIS_HIDDEN = 0,
        VIS_IMMEDIATELY_VISIBLE = 1,
        VIS_USE_VISIBLE = 2,
        VIS_DIRECTLY_VISIBLE = 3
    } visibility;

    /* Declaration reference */
    Syntax_Node    *declaration;

    /* Subprogram-specific */
    Parameter_Info *parameters;
    uint32_t        parameter_count;
    Type_Info      *return_type;    /* NULL for procedures */

    /* Package-specific */
    Symbol        **exported;       /* Visible part symbols */
    uint32_t        exported_count;

    /* Unique identifier for mangling */
    uint32_t        unique_id;

    /* Nesting level for static link computation */
    uint32_t        nesting_level;

    /* Frame offset for static link variable access */
    int64_t         frame_offset;

    /* Scope created by this symbol (for functions/procedures) */
    Scope          *scope;

    /* ─────────────────────────────────────────────────────────────────────
     * Pragma Effects
     * ───────────────────────────────────────────────────────────────────── */

    /* pragma Inline */
    bool            is_inline;

    /* pragma Import / Export */
    bool            is_imported;
    bool            is_exported;
    String_Slice    external_name;       /* External linker name */
    String_Slice    link_name;           /* Link section name */
    enum {
        CONVENTION_ADA = 0,
        CONVENTION_C,
        CONVENTION_STDCALL,
        CONVENTION_INTRINSIC,
        CONVENTION_ASSEMBLER
    } convention;

    /* pragma Suppress checks */
    uint32_t        suppressed_checks;   /* Bitmask of suppressed checks */

    /* pragma Unreferenced */
    bool            is_unreferenced;

    /* Code generation flags */
    bool            extern_emitted;      /* Extern declaration already emitted */

    /* ─────────────────────────────────────────────────────────────────────
     * Generic Support
     * ───────────────────────────────────────────────────────────────────── */

    /* For SYMBOL_GENERIC: the generic template */
    Syntax_Node    *generic_formals;     /* List of NK_GENERIC_*_PARAM nodes */
    Syntax_Node    *generic_unit;        /* The procedure/function/package spec */
    Syntax_Node    *generic_body;        /* Associated body (if found) */

    /* For SYMBOL_GENERIC_INSTANCE: instantiation info */
    Symbol         *generic_template;    /* The SYMBOL_GENERIC being instantiated */
    Symbol         *instantiated_subprogram;  /* The resolved subprogram instance */

    /* Generic formal->actual mapping (array parallel to generic_formals) */
    struct {
        String_Slice formal_name;
        Type_Info   *actual_type;        /* For type formals */
        Symbol      *actual_subprogram;  /* For subprogram formals */
        Syntax_Node *actual_expr;        /* For object formals */
    } *generic_actuals;
    uint32_t        generic_actual_count;
};

/* ─────────────────────────────────────────────────────────────────────────
 * §11.3 Scope Structure
 * ───────────────────────────────────────────────────────────────────────── */

#define SYMBOL_TABLE_SIZE 1024

struct Scope {
    Symbol  *buckets[SYMBOL_TABLE_SIZE];
    Scope   *parent;
    Symbol  *owner;             /* Package/subprogram owning this scope */
    uint32_t nesting_level;

    /* Linear list of all symbols for enumeration (static link support) */
    Symbol **symbols;
    uint32_t symbol_count;
    uint32_t symbol_capacity;
    int64_t  frame_size;        /* Total size of frame for this scope */
};

typedef struct {
    Scope   *current_scope;
    Scope   *global_scope;

    /* Predefined types */
    Type_Info *type_boolean;
    Type_Info *type_integer;
    Type_Info *type_float;
    Type_Info *type_character;
    Type_Info *type_string;
    Type_Info *type_duration;
    Type_Info *type_universal_integer;
    Type_Info *type_universal_real;

    /* Unique ID counter for symbol mangling */
    uint32_t   next_unique_id;
} Symbol_Manager;

/* ─────────────────────────────────────────────────────────────────────────
 * §11.4 Scope Operations
 * ───────────────────────────────────────────────────────────────────────── */

static Scope *Scope_New(Scope *parent) {
    Scope *scope = Arena_Allocate(sizeof(Scope));
    scope->parent = parent;
    scope->nesting_level = parent ? parent->nesting_level + 1 : 0;
    return scope;
}

static void Symbol_Manager_Push_Scope(Symbol_Manager *sm, Symbol *owner) {
    Scope *scope = Scope_New(sm->current_scope);
    scope->owner = owner;
    sm->current_scope = scope;
}

static void Symbol_Manager_Pop_Scope(Symbol_Manager *sm) {
    if (sm->current_scope->parent) {
        sm->current_scope = sm->current_scope->parent;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §11.5 Symbol Table Operations
 * ───────────────────────────────────────────────────────────────────────── */

static uint32_t Symbol_Hash_Name(String_Slice name) {
    return (uint32_t)(Slice_Hash(name) % SYMBOL_TABLE_SIZE);
}

static Symbol *Symbol_New(Symbol_Kind kind, String_Slice name, Source_Location loc) {
    Symbol *sym = Arena_Allocate(sizeof(Symbol));
    sym->kind = kind;
    sym->name = name;
    sym->location = loc;
    sym->visibility = VIS_IMMEDIATELY_VISIBLE;
    return sym;
}

static void Symbol_Add(Symbol_Manager *sm, Symbol *sym) {
    Scope *scope = sm->current_scope;
    sym->unique_id = sm->next_unique_id++;
    sym->defining_scope = scope;
    sym->nesting_level = scope->nesting_level;

    uint32_t hash = Symbol_Hash_Name(sym->name);
    Symbol *existing = scope->buckets[hash];

    /* Check for existing symbol with same name at this scope level */
    while (existing) {
        if (existing->defining_scope == scope &&
            Slice_Equal_Ignore_Case(existing->name, sym->name)) {
            /* Overloading: add to chain if subprograms */
            if ((existing->kind == SYMBOL_PROCEDURE || existing->kind == SYMBOL_FUNCTION) &&
                (sym->kind == SYMBOL_PROCEDURE || sym->kind == SYMBOL_FUNCTION)) {
                sym->next_overload = existing->next_overload;
                existing->next_overload = sym;
                return;
            }
            /* Otherwise: redefinition error (would report here) */
        }
        existing = existing->next_in_bucket;
    }

    sym->next_in_bucket = scope->buckets[hash];
    scope->buckets[hash] = sym;

    /* Set parent to enclosing package/subprogram for nested symbol support */
    sym->parent = scope->owner;

    /* Add to linear symbol list for enumeration (static link support) */
    if (scope->symbol_count >= scope->symbol_capacity) {
        uint32_t new_cap = scope->symbol_capacity ? scope->symbol_capacity * 2 : 16;
        Symbol **new_syms = Arena_Allocate(new_cap * sizeof(Symbol*));
        if (scope->symbols) memcpy(new_syms, scope->symbols, scope->symbol_count * sizeof(Symbol*));
        scope->symbols = new_syms;
        scope->symbol_capacity = new_cap;
    }
    scope->symbols[scope->symbol_count++] = sym;

    /* Track frame offset for variables/parameters */
    if (sym->kind == SYMBOL_VARIABLE || sym->kind == SYMBOL_PARAMETER) {
        sym->frame_offset = scope->frame_size;
        uint32_t var_size = sym->type ? sym->type->size : 8;
        if (var_size == 0) var_size = 8;
        scope->frame_size += var_size;
    }
}

/* Find symbol by name, searching enclosing scopes */
static Symbol *Symbol_Find(Symbol_Manager *sm, String_Slice name) {
    uint32_t hash = Symbol_Hash_Name(name);

    for (Scope *scope = sm->current_scope; scope; scope = scope->parent) {
        for (Symbol *sym = scope->buckets[hash]; sym; sym = sym->next_in_bucket) {
            if (Slice_Equal_Ignore_Case(sym->name, name) &&
                sym->visibility >= VIS_IMMEDIATELY_VISIBLE) {
                return sym;
            }
        }
    }

    return NULL;
}

/* Find symbol with specific arity (for overload resolution) */
__attribute__((unused))
static Symbol *Symbol_Find_With_Arity(Symbol_Manager *sm, String_Slice name, uint32_t arity) {
    Symbol *sym = Symbol_Find(sm, name);

    while (sym) {
        if (sym->parameter_count == arity) return sym;
        sym = sym->next_overload;
    }

    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §11.6 OVERLOAD RESOLUTION — Following GNAT sem_type.ads Design
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Ada's overload resolution is a two-pass process:
 *
 * 1. Bottom-up pass: Collect all possible interpretations of each identifier
 *    based on visibility rules. Each interpretation is a (Symbol, Type) pair.
 *
 * 2. Top-down pass: Given context type expectations, select the unique valid
 *    interpretation using disambiguation rules.
 *
 * Key GNAT concepts implemented here:
 * - Interp: Record of (Nam, Typ, Opnd_Typ) representing one interpretation
 * - Covers: Type compatibility test (T1 covers T2 if T2's values are legal for T1)
 * - Disambiguate: Select best interpretation when multiple are valid
 *
 * Per RM 8.6: Overload resolution identifies the unique declaration for each
 * identifier. It fails if no interpretation is valid or if multiple are valid.
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §11.6.1 Interpretation Structure
 *
 * Following GNAT's sem_type.ads: "type Interp is record Nam, Typ, Opnd_Typ..."
 * We store interpretations in a contiguous array during resolution.
 * ───────────────────────────────────────────────────────────────────────── */

#define MAX_INTERPRETATIONS 64

typedef struct {
    Symbol    *nam;           /* The entity (function, procedure, operator) */
    Type_Info *typ;           /* The result type */
    Type_Info *opnd_typ;      /* For comparison ops: operand type for visibility */
    bool       is_universal;  /* True if operands are universal types */
    uint32_t   scope_depth;   /* Nesting level for hiding rules */
} Interpretation;

typedef struct {
    Interpretation items[MAX_INTERPRETATIONS];
    uint32_t       count;
} Interp_List;

/* ─────────────────────────────────────────────────────────────────────────
 * §11.6.2 Type Covering (Compatibility)
 *
 * Following GNAT's Covers function from sem_type.adb:
 * T1 covers T2 if values of T2 are legal where T1 is expected.
 *
 * Key rules from RM 8.6:
 * - Same type: always covers
 * - Subtypes of same base type: cover each other
 * - Universal types: Universal_Integer covers any integer type, etc.
 * - Class-wide types: Cover all types in the class (Ada 95+, not in Ada 83)
 * ───────────────────────────────────────────────────────────────────────── */

static bool Type_Covers(Type_Info *expected, Type_Info *actual) {
    /* Null types are permissive (incomplete analysis) */
    if (!expected || !actual) return true;

    /* Same type always covers */
    if (expected == actual) return true;

    /* Universal_Integer covers any discrete type */
    if (expected->kind == TYPE_UNIVERSAL_INTEGER) {
        return Type_Is_Discrete(actual);
    }
    if (actual->kind == TYPE_UNIVERSAL_INTEGER) {
        return Type_Is_Discrete(expected);
    }

    /* Universal_Real covers any real type */
    if (expected->kind == TYPE_UNIVERSAL_REAL) {
        return Type_Is_Real(actual);
    }
    if (actual->kind == TYPE_UNIVERSAL_REAL) {
        return Type_Is_Real(expected);
    }

    /* Same base type covers */
    Type_Info *base_exp = Type_Base(expected);
    Type_Info *base_act = Type_Base(actual);
    if (base_exp == base_act) return true;
    if (base_exp == actual || expected == base_act) return true;

    /* Array/string compatibility: same structure */
    if ((expected->kind == TYPE_ARRAY || expected->kind == TYPE_STRING) &&
        (actual->kind == TYPE_ARRAY || actual->kind == TYPE_STRING)) {
        /* STRING is compatible with CHARACTER arrays */
        if (expected->kind == TYPE_STRING || actual->kind == TYPE_STRING) {
            return true;
        }
        /* Arrays with same element type */
        if (expected->array.element_type && actual->array.element_type) {
            return Type_Covers(expected->array.element_type,
                              actual->array.element_type);
        }
        return true;
    }

    /* Access types: check designated type compatibility */
    if (expected->kind == TYPE_ACCESS && actual->kind == TYPE_ACCESS) {
        if (expected->access.designated_type && actual->access.designated_type) {
            return Type_Covers(expected->access.designated_type,
                              actual->access.designated_type);
        }
        return true;
    }

    /* NULL literal covers any access type */
    if (expected->kind == TYPE_ACCESS && !actual) {
        return true;
    }

    return false;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §11.6.3 Parameter Conformance
 *
 * Check if an argument list matches a subprogram's parameter profile.
 * Per RM 6.4.1: actual parameters must be type conformant with formals.
 * ───────────────────────────────────────────────────────────────────────── */

/* Forward declaration for Resolve_Expression - needed for argument resolution */
static Type_Info *Resolve_Expression(Symbol_Manager *sm, Syntax_Node *node);

typedef struct {
    Type_Info **types;       /* Array of argument types */
    uint32_t    count;       /* Number of arguments */
    String_Slice *names;     /* Named association names (NULL for positional) */
} Argument_Info;

/* Check if arguments match a symbol's parameter profile */
static bool Arguments_Match_Profile(Symbol *sym, Argument_Info *args) {
    if (!sym) return false;

    /* Check arity first */
    uint32_t required_params = 0;
    uint32_t optional_params = 0;

    for (uint32_t i = 0; i < sym->parameter_count; i++) {
        if (sym->parameters[i].default_value) {
            optional_params++;
        } else {
            required_params++;
        }
    }

    if (args->count < required_params ||
        args->count > sym->parameter_count) {
        return false;
    }

    /* Check type compatibility for each argument */
    for (uint32_t i = 0; i < args->count; i++) {
        Type_Info *arg_type = args->types[i];
        Type_Info *param_type = NULL;

        /* Handle named association */
        if (args->names && args->names[i].data) {
            bool found = false;
            for (uint32_t j = 0; j < sym->parameter_count; j++) {
                if (Slice_Equal_Ignore_Case(sym->parameters[j].name, args->names[i])) {
                    param_type = sym->parameters[j].param_type;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        } else {
            /* Positional: use i-th parameter */
            if (i >= sym->parameter_count) return false;
            param_type = sym->parameters[i].param_type;
        }

        if (!Type_Covers(param_type, arg_type)) {
            return false;
        }
    }

    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §11.6.4 Interpretation Collection
 *
 * Following GNAT's Collect_Interps: gather all visible interpretations
 * of an overloaded name. This includes:
 * - Immediately visible entities
 * - Use-visible entities (from USE clauses)
 * - Predefined operators for the types involved
 * ───────────────────────────────────────────────────────────────────────── */

/* Collect all visible interpretations of a name */
static void Collect_Interpretations(Symbol_Manager *sm, String_Slice name,
                                    Interp_List *interps) {
    interps->count = 0;
    uint32_t hash = Symbol_Hash_Name(name);

    /* Search all enclosing scopes */
    for (Scope *scope = sm->current_scope; scope; scope = scope->parent) {
        for (Symbol *sym = scope->buckets[hash]; sym; sym = sym->next_in_bucket) {
            if (!Slice_Equal_Ignore_Case(sym->name, name)) continue;
            if (sym->visibility < VIS_IMMEDIATELY_VISIBLE) continue;

            /* Add this interpretation and all overloads */
            Symbol *s = sym;
            while (s && interps->count < MAX_INTERPRETATIONS) {
                /* Check if we already have this interpretation */
                bool duplicate = false;
                for (uint32_t i = 0; i < interps->count; i++) {
                    if (interps->items[i].nam == s) {
                        duplicate = true;
                        break;
                    }
                }

                if (!duplicate) {
                    interps->items[interps->count++] = (Interpretation){
                        .nam = s,
                        .typ = (s->kind == SYMBOL_FUNCTION) ? s->return_type : s->type,
                        .opnd_typ = NULL,
                        .is_universal = false,
                        .scope_depth = scope->nesting_level
                    };
                }

                s = s->next_overload;
            }
        }
    }
}

/* Filter interpretations by argument compatibility */
static void Filter_By_Arguments(Interp_List *interps, Argument_Info *args) {
    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < interps->count; i++) {
        Symbol *sym = interps->items[i].nam;

        /* Non-callable symbols don't filter by arguments */
        if (sym->kind != SYMBOL_FUNCTION && sym->kind != SYMBOL_PROCEDURE) {
            interps->items[write_idx++] = interps->items[i];
            continue;
        }

        /* Keep if arguments match */
        if (Arguments_Match_Profile(sym, args)) {
            interps->items[write_idx++] = interps->items[i];
        }
    }

    interps->count = write_idx;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §11.6.5 Disambiguation
 *
 * Following GNAT's Disambiguate function from sem_type.adb:
 * When multiple interpretations remain, apply preference rules:
 *
 * 1. Prefer non-universal interpretations over universal
 * 2. Prefer interpretations in inner scopes over outer scopes
 * 3. User-defined operators can hide predefined operators if:
 *    - They have the same signature
 *    - They are visible in the current scope
 * 4. For operators: prefer interpretation where operand types match exactly
 *
 * Per RM 8.6: Resolution fails if still ambiguous after preferences.
 * ───────────────────────────────────────────────────────────────────────── */

/* Check if sym1 hides sym2 (user-defined hiding predefined, or inner scope) */
static bool Symbol_Hides(Symbol *sym1, Symbol *sym2) {
    if (!sym1 || !sym2) return false;

    /* User-defined function can hide predefined operator */
    if ((sym1->kind == SYMBOL_FUNCTION || sym1->kind == SYMBOL_PROCEDURE) &&
        sym2->nesting_level == 0) {  /* Predefined are at level 0 */
        return true;
    }

    /* Inner scope hides outer scope */
    if (sym1->nesting_level > sym2->nesting_level) {
        return true;
    }

    return false;
}

/* Check if type is a universal type */
static bool Type_Is_Universal(Type_Info *t) {
    return t && (t->kind == TYPE_UNIVERSAL_INTEGER ||
                 t->kind == TYPE_UNIVERSAL_REAL);
}

/* Score an interpretation for preference ranking (higher = better) */
static int32_t Score_Interpretation(Interpretation *interp,
                                    Type_Info *context_type,
                                    Argument_Info *args) {
    int32_t score = 0;
    Symbol *sym = interp->nam;

    /* Prefer non-universal interpretations */
    if (!Type_Is_Universal(interp->typ)) {
        score += 1000;
    }

    /* Prefer exact context type match */
    if (context_type && interp->typ == context_type) {
        score += 500;
    }

    /* Prefer inner scopes (user-defined over predefined) */
    score += (int32_t)(interp->scope_depth * 10);

    /* For functions: prefer exact argument type matches */
    if (sym && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_PROCEDURE) && args) {
        for (uint32_t i = 0; i < args->count && i < sym->parameter_count; i++) {
            Type_Info *arg_type = args->types[i];
            Type_Info *param_type = sym->parameters[i].param_type;

            /* Exact match is better than just coverage */
            if (arg_type == param_type) {
                score += 100;
            } else if (Type_Base(arg_type) == Type_Base(param_type)) {
                score += 50;
            }
        }
    }

    return score;
}

/* Select the best interpretation from a list */
static Symbol *Disambiguate(Interp_List *interps, Type_Info *context_type,
                           Argument_Info *args) {
    if (interps->count == 0) return NULL;
    if (interps->count == 1) return interps->items[0].nam;

    /* Score all interpretations */
    int32_t best_score = INT32_MIN;
    Symbol *best = NULL;
    int tied_count = 0;

    for (uint32_t i = 0; i < interps->count; i++) {
        int32_t score = Score_Interpretation(&interps->items[i], context_type, args);

        if (score > best_score) {
            best_score = score;
            best = interps->items[i].nam;
            tied_count = 1;
        } else if (score == best_score) {
            /* Check hiding rules */
            if (Symbol_Hides(interps->items[i].nam, best)) {
                best = interps->items[i].nam;
            } else if (!Symbol_Hides(best, interps->items[i].nam)) {
                tied_count++;
            }
        }
    }

    /* If still tied, check for universal vs specific preference */
    if (tied_count > 1 && context_type) {
        /* Prefer interpretation matching context exactly */
        for (uint32_t i = 0; i < interps->count; i++) {
            if (interps->items[i].typ == context_type) {
                return interps->items[i].nam;
            }
        }

        /* Prefer non-universal */
        for (uint32_t i = 0; i < interps->count; i++) {
            if (!Type_Is_Universal(interps->items[i].typ)) {
                return interps->items[i].nam;
            }
        }
    }

    return best;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §11.6.6 Unified Overload Resolution Entry Point
 *
 * Resolve_Overloaded_Call: main entry for call/indexed/conversion resolution
 * This unifies the previous symbol_find_with_arity and symbol_find_with_args
 * into a single, correct implementation following GNAT patterns.
 * ───────────────────────────────────────────────────────────────────────── */

static Symbol *Resolve_Overloaded_Call(Symbol_Manager *sm,
                                       String_Slice name,
                                       Argument_Info *args,
                                       Type_Info *context_type) {
    Interp_List interps;

    /* Phase 1: Collect all visible interpretations */
    Collect_Interpretations(sm, name, &interps);

    if (interps.count == 0) {
        return NULL;  /* No visible interpretation */
    }

    /* Phase 2: Filter by argument compatibility */
    if (args && args->count > 0) {
        Filter_By_Arguments(&interps, args);

        if (interps.count == 0) {
            return NULL;  /* No matching profile */
        }
    }

    /* Phase 3: Apply context type filtering if provided */
    if (context_type && interps.count > 1) {
        uint32_t write_idx = 0;
        for (uint32_t i = 0; i < interps.count; i++) {
            if (Type_Covers(context_type, interps.items[i].typ)) {
                interps.items[write_idx++] = interps.items[i];
            }
        }
        if (write_idx > 0) {
            interps.count = write_idx;
        }
        /* If no matches, keep all for better error reporting */
    }

    /* Phase 4: Disambiguate if multiple interpretations remain */
    return Disambiguate(&interps, context_type, args);
}

/* ─────────────────────────────────────────────────────────────────────────
 * §11.7 Symbol Manager Initialization
 * ───────────────────────────────────────────────────────────────────────── */

static void Symbol_Manager_Init_Predefined(Symbol_Manager *sm) {
    /* Create predefined types */
    sm->type_boolean = Type_New(TYPE_BOOLEAN, S("BOOLEAN"));
    sm->type_boolean->size = 1;
    sm->type_boolean->low_bound = (Type_Bound){BOUND_INTEGER, {.int_value = 0}};
    sm->type_boolean->high_bound = (Type_Bound){BOUND_INTEGER, {.int_value = 1}};

    sm->type_integer = Type_New(TYPE_INTEGER, S("INTEGER"));
    sm->type_integer->size = 8;  /* 64-bit INTEGER to match i64 usage throughout codegen */
    sm->type_integer->low_bound = (Type_Bound){BOUND_INTEGER, {.int_value = INT32_MIN}};
    sm->type_integer->high_bound = (Type_Bound){BOUND_INTEGER, {.int_value = INT32_MAX}};

    sm->type_float = Type_New(TYPE_FLOAT, S("FLOAT"));
    sm->type_float->size = 8;  /* double precision */

    sm->type_character = Type_New(TYPE_CHARACTER, S("CHARACTER"));
    sm->type_character->size = 1;

    sm->type_string = Type_New(TYPE_STRING, S("STRING"));
    sm->type_string->size = 16;  /* Fat pointer: ptr + length */
    sm->type_string->array.element_type = sm->type_character;  /* STRING is array of CHARACTER */

    /* DURATION is a predefined fixed-point type for time intervals */
    sm->type_duration = Type_New(TYPE_FIXED, S("DURATION"));
    sm->type_duration->size = 8;  /* 64-bit for high precision */
    sm->type_duration->fixed.delta = 0.00001;  /* 10 microsecond resolution */

    sm->type_universal_integer = Type_New(TYPE_UNIVERSAL_INTEGER, S("universal_integer"));
    sm->type_universal_real = Type_New(TYPE_UNIVERSAL_REAL, S("universal_real"));

    /* Add predefined type symbols to global scope */
    Symbol *sym_boolean = Symbol_New(SYMBOL_TYPE, S("BOOLEAN"), No_Location);
    sym_boolean->type = sm->type_boolean;
    Symbol_Add(sm, sym_boolean);

    Symbol *sym_integer = Symbol_New(SYMBOL_TYPE, S("INTEGER"), No_Location);
    sym_integer->type = sm->type_integer;
    Symbol_Add(sm, sym_integer);

    /* NATURAL is subtype INTEGER range 0..INTEGER'LAST - for Ada 83 compatibility */
    Symbol *sym_natural = Symbol_New(SYMBOL_TYPE, S("NATURAL"), No_Location);
    sym_natural->type = sm->type_integer;  /* Alias to INTEGER for now */
    Symbol_Add(sm, sym_natural);

    /* POSITIVE is subtype INTEGER range 1..INTEGER'LAST */
    Symbol *sym_positive = Symbol_New(SYMBOL_TYPE, S("POSITIVE"), No_Location);
    sym_positive->type = sm->type_integer;  /* Alias to INTEGER for now */
    Symbol_Add(sm, sym_positive);

    Symbol *sym_float = Symbol_New(SYMBOL_TYPE, S("FLOAT"), No_Location);
    sym_float->type = sm->type_float;
    Symbol_Add(sm, sym_float);

    Symbol *sym_duration = Symbol_New(SYMBOL_TYPE, S("DURATION"), No_Location);
    sym_duration->type = sm->type_duration;
    Symbol_Add(sm, sym_duration);

    Symbol *sym_character = Symbol_New(SYMBOL_TYPE, S("CHARACTER"), No_Location);
    sym_character->type = sm->type_character;
    Symbol_Add(sm, sym_character);

    Symbol *sym_string = Symbol_New(SYMBOL_TYPE, S("STRING"), No_Location);
    sym_string->type = sm->type_string;
    Symbol_Add(sm, sym_string);

    /* Boolean literals */
    Symbol *sym_false = Symbol_New(SYMBOL_LITERAL, S("FALSE"), No_Location);
    sym_false->type = sm->type_boolean;
    Symbol_Add(sm, sym_false);

    Symbol *sym_true = Symbol_New(SYMBOL_LITERAL, S("TRUE"), No_Location);
    sym_true->type = sm->type_boolean;
    Symbol_Add(sm, sym_true);
}

static Symbol_Manager *Symbol_Manager_New(void) {
    Symbol_Manager *sm = Arena_Allocate(sizeof(Symbol_Manager));
    sm->global_scope = Scope_New(NULL);
    sm->current_scope = sm->global_scope;
    sm->next_unique_id = 1;
    Symbol_Manager_Init_Predefined(sm);
    return sm;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §12. SEMANTIC ANALYSIS — Type Checking and Resolution
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Semantic analysis performs:
 * - Name resolution: bind identifiers to symbols
 * - Type checking: verify type compatibility of operations
 * - Overload resolution: select correct subprogram
 * - Constraint checking: verify bounds, indices, etc.
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §12.1 Expression Resolution
 * ───────────────────────────────────────────────────────────────────────── */

static Type_Info *Resolve_Expression(Symbol_Manager *sm, Syntax_Node *node);
static void Resolve_Statement(Symbol_Manager *sm, Syntax_Node *node);

static Type_Info *Resolve_Identifier(Symbol_Manager *sm, Syntax_Node *node) {
    Symbol *sym = Symbol_Find(sm, node->string_val.text);

    if (!sym) {
        Report_Error(node->location, "undefined identifier '%.*s'",
                    node->string_val.text.length, node->string_val.text.data);
        return sm->type_integer;  /* Error recovery */
    }

    node->symbol = sym;
    node->type = sym->type;
    return sym->type;
}

static Type_Info *Resolve_Selected(Symbol_Manager *sm, Syntax_Node *node) {
    /* Resolve prefix first */
    Type_Info *prefix_type = Resolve_Expression(sm, node->selected.prefix);

    if (prefix_type && prefix_type->kind == TYPE_RECORD) {
        /* Look up component */
        for (uint32_t i = 0; i < prefix_type->record.component_count; i++) {
            if (Slice_Equal_Ignore_Case(prefix_type->record.components[i].name,
                                        node->selected.selector)) {
                node->type = prefix_type->record.components[i].component_type;
                return node->type;
            }
        }
        Report_Error(node->location, "no component '%.*s' in record type",
                    node->selected.selector.length, node->selected.selector.data);
    } else {
        /* Could be package selection - look up in prefix's exported symbols */
        Symbol *prefix_sym = node->selected.prefix->symbol;
        if (prefix_sym && prefix_sym->kind == SYMBOL_PACKAGE) {
            /* Search package's exported symbols */
            for (uint32_t i = 0; i < prefix_sym->exported_count; i++) {
                if (Slice_Equal_Ignore_Case(prefix_sym->exported[i]->name,
                                           node->selected.selector)) {
                    node->symbol = prefix_sym->exported[i];
                    node->type = prefix_sym->exported[i]->type;
                    return node->type;
                }
            }
        }
    }

    return sm->type_integer;  /* Error recovery */
}

/* Get the operator name string for a token kind */
static String_Slice Operator_Name(Token_Kind op) {
    switch (op) {
        case TK_PLUS:      return S("\"+\"");
        case TK_MINUS:     return S("\"-\"");
        case TK_STAR:      return S("\"*\"");
        case TK_SLASH:     return S("\"/\"");
        case TK_MOD:       return S("\"mod\"");
        case TK_REM:       return S("\"rem\"");
        case TK_EXPON:     return S("\"**\"");
        case TK_AMPERSAND: return S("\"&\"");
        case TK_AND:       return S("\"and\"");
        case TK_OR:        return S("\"or\"");
        case TK_XOR:       return S("\"xor\"");
        case TK_EQ:        return S("\"=\"");
        case TK_NE:        return S("\"/=\"");
        case TK_LT:        return S("\"<\"");
        case TK_LE:        return S("\"<=\"");
        case TK_GT:        return S("\">\"");
        case TK_GE:        return S("\">=\"");
        case TK_NOT:       return S("\"not\"");
        case TK_ABS:       return S("\"abs\"");
        default:           return S("");
    }
}

static Type_Info *Resolve_Binary_Op(Symbol_Manager *sm, Syntax_Node *node) {
    Type_Info *left_type = Resolve_Expression(sm, node->binary.left);
    Type_Info *right_type = Resolve_Expression(sm, node->binary.right);

    Token_Kind op = node->binary.op;

    /*
     * Per RM 4.5: Binary operators can be user-defined. We first check for
     * user-defined operators, then fall back to predefined semantics.
     *
     * User-defined operators are functions with designator names like "+" that
     * take two parameters of the appropriate types.
     */

    /* Try to find a user-defined operator */
    String_Slice op_name = Operator_Name(op);
    if (op_name.length > 0) {
        Type_Info *arg_types[2] = { left_type, right_type };
        Argument_Info args = {
            .types = arg_types,
            .count = 2,
            .names = NULL
        };

        Symbol *user_op = Resolve_Overloaded_Call(sm, op_name, &args, NULL);
        if (user_op && user_op->kind == SYMBOL_FUNCTION) {
            node->symbol = user_op;
            node->type = user_op->return_type;
            return node->type;
        }
    }

    /* Fall back to predefined operator semantics */
    switch (op) {
        case TK_PLUS: case TK_MINUS: case TK_STAR: case TK_SLASH:
        case TK_MOD: case TK_REM: case TK_EXPON:
            /* Numeric operators */
            if (!Type_Is_Numeric(left_type) || !Type_Is_Numeric(right_type)) {
                Report_Error(node->location, "numeric operands required for %s",
                            Token_Name[op]);
            }
            /* Result type: prefer non-universal (per RM 4.5.5) */
            if (Type_Is_Universal(left_type) && !Type_Is_Universal(right_type)) {
                node->type = right_type;
            } else if (!Type_Is_Universal(left_type)) {
                node->type = left_type;
            } else {
                /* Both universal - result is universal */
                node->type = left_type;
            }
            break;

        case TK_AMPERSAND:
            /* String/array concatenation */
            if (left_type && left_type->kind != TYPE_STRING && left_type->kind != TYPE_ARRAY) {
                Report_Error(node->location, "concatenation requires string or array");
            }
            node->type = left_type;
            break;

        case TK_AND: case TK_OR: case TK_XOR:
        case TK_AND_THEN: case TK_OR_ELSE:
            /* Boolean operators - can also operate on arrays of Boolean */
            if (left_type && left_type->kind != TYPE_BOOLEAN) {
                if (left_type->kind != TYPE_ARRAY ||
                    !left_type->array.element_type ||
                    left_type->array.element_type->kind != TYPE_BOOLEAN) {
                    Report_Error(node->location, "Boolean operands required");
                }
            }
            node->type = left_type ? left_type : sm->type_boolean;
            break;

        case TK_EQ: case TK_NE: case TK_LT: case TK_LE: case TK_GT: case TK_GE:
            /* Comparison operators */
            if (!Type_Covers(left_type, right_type) && !Type_Covers(right_type, left_type)) {
                Report_Error(node->location, "incompatible types for comparison");
            }
            node->type = sm->type_boolean;
            break;

        case TK_IN:
            /* Membership test */
            node->type = sm->type_boolean;
            break;

        default:
            node->type = sm->type_integer;
    }

    return node->type;
}

static Type_Info *Resolve_Apply(Symbol_Manager *sm, Syntax_Node *node) {
    /*
     * Apply node resolution - handles multiple Ada constructs:
     * 1. Function/procedure calls: Put(X), Process(A, B)
     * 2. Array indexing: Arr(I), Matrix(I, J)
     * 3. Type conversions: Integer(X), Float(Y)
     * 4. Constrained subtype indications: String(1..10)
     *
     * For calls, we use the overload resolution engine to handle:
     * - Overloaded subprogram names
     * - Named parameter associations
     * - Default parameter values
     */

    /* First, resolve all arguments to get their types */
    uint32_t arg_count = (uint32_t)node->apply.arguments.count;
    Type_Info **arg_types = NULL;
    String_Slice *arg_names = NULL;

    if (arg_count > 0) {
        arg_types = Arena_Allocate(arg_count * sizeof(Type_Info*));
        arg_names = Arena_Allocate(arg_count * sizeof(String_Slice));

        for (uint32_t i = 0; i < arg_count; i++) {
            Syntax_Node *arg = node->apply.arguments.items[i];

            /* Handle named associations: Name => Value */
            if (arg->kind == NK_ASSOCIATION && arg->association.choices.count == 1) {
                Syntax_Node *name_node = arg->association.choices.items[0];
                if (name_node->kind == NK_IDENTIFIER) {
                    arg_names[i] = name_node->string_val.text;
                }
                /* Resolve the value expression */
                if (arg->association.expression) {
                    arg_types[i] = Resolve_Expression(sm, arg->association.expression);
                }
            } else {
                arg_names[i] = (String_Slice){0};  /* Positional */
                arg_types[i] = Resolve_Expression(sm, arg);
            }
        }
    }

    Argument_Info args = {
        .types = arg_types,
        .count = arg_count,
        .names = arg_names
    };

    /* Resolve the prefix */
    Syntax_Node *prefix = node->apply.prefix;
    Symbol *prefix_sym = NULL;

    /* For identifier prefix, use overload resolution */
    if (prefix->kind == NK_IDENTIFIER) {
        prefix_sym = Resolve_Overloaded_Call(sm, prefix->string_val.text, &args, NULL);
        if (prefix_sym) {
            prefix->symbol = prefix_sym;
            prefix->type = (prefix_sym->kind == SYMBOL_FUNCTION) ?
                           prefix_sym->return_type : prefix_sym->type;
        } else {
            /* Fall back to simple lookup for non-callable names */
            prefix_sym = Symbol_Find(sm, prefix->string_val.text);
            if (prefix_sym) {
                prefix->symbol = prefix_sym;
                prefix->type = prefix_sym->type;
            }
        }
    } else {
        /* For complex prefix (selected, etc.), resolve normally */
        Resolve_Expression(sm, prefix);
        prefix_sym = prefix->symbol;
    }

    Type_Info *prefix_type = prefix->type;

    /* Handle based on what the prefix resolves to */
    if (prefix_sym) {
        /* ─── Case 1: Function/Procedure Call ─── */
        if (prefix_sym->kind == SYMBOL_FUNCTION || prefix_sym->kind == SYMBOL_PROCEDURE) {
            node->symbol = prefix_sym;
            node->type = prefix_sym->return_type;  /* NULL for procedures */
            return node->type;
        }

        /* ─── Case 2: Type Conversion or Constrained Subtype ─── */
        if (prefix_sym->kind == SYMBOL_TYPE) {
            Type_Info *base_type = prefix_sym->type;

            /* Check for constrained subtype indication: STRING(1..5) */
            if (base_type && (base_type->kind == TYPE_STRING || base_type->kind == TYPE_ARRAY)) {
                /* Check if arguments are ranges (subtype indication) vs values (indexing) */
                bool has_range = false;
                for (uint32_t i = 0; i < arg_count; i++) {
                    Syntax_Node *arg = node->apply.arguments.items[i];
                    if (arg->kind == NK_RANGE) {
                        has_range = true;
                        break;
                    }
                }

                if (has_range) {
                    /* Constrained array/string subtype indication */
                    Type_Info *constrained = Type_New(TYPE_ARRAY, base_type->name);
                    constrained->array.is_constrained = true;
                    constrained->array.index_count = arg_count;

                    /* Element type */
                    if (base_type->kind == TYPE_STRING) {
                        constrained->array.element_type = sm->type_character;
                    } else if (base_type->array.element_type) {
                        constrained->array.element_type = base_type->array.element_type;
                    }

                    /* Process index constraints */
                    if (arg_count > 0) {
                        constrained->array.indices = Arena_Allocate(
                            arg_count * sizeof(Index_Info));

                        for (uint32_t i = 0; i < arg_count; i++) {
                            Syntax_Node *arg = node->apply.arguments.items[i];
                            Index_Info *info = &constrained->array.indices[i];
                            info->index_type = sm->type_integer;

                            if (arg->kind == NK_RANGE) {
                                if (arg->range.low && arg->range.low->kind == NK_INTEGER) {
                                    info->low_bound = (Type_Bound){
                                        .kind = BOUND_INTEGER,
                                        .int_value = arg->range.low->integer_lit.value
                                    };
                                }
                                if (arg->range.high && arg->range.high->kind == NK_INTEGER) {
                                    info->high_bound = (Type_Bound){
                                        .kind = BOUND_INTEGER,
                                        .int_value = arg->range.high->integer_lit.value
                                    };
                                }
                            }
                        }

                        /* Compute size */
                        int64_t count = 1;
                        for (uint32_t i = 0; i < arg_count; i++) {
                            int64_t lo = Type_Bound_Value(constrained->array.indices[i].low_bound);
                            int64_t hi = Type_Bound_Value(constrained->array.indices[i].high_bound);
                            count *= (hi - lo + 1);
                        }
                        uint32_t elem_size = constrained->array.element_type ?
                                             constrained->array.element_type->size : 1;
                        constrained->size = (uint32_t)(count * elem_size);
                    }

                    node->type = constrained;
                    return constrained;
                }
            }

            /* Regular type conversion: Integer(X) */
            if (arg_count == 1) {
                node->type = prefix_sym->type;
                return node->type;
            }
        }
    }

    /* ─── Case 3: Array Indexing ─── */
    if (prefix_type && (prefix_type->kind == TYPE_ARRAY || prefix_type->kind == TYPE_STRING)) {
        node->type = prefix_type->array.element_type;
        if (!node->type && prefix_type->kind == TYPE_STRING) {
            node->type = sm->type_character;
        }
        return node->type;
    }

    /* ─── Case 4: Unresolved - report error and recover ─── */
    if (prefix->kind == NK_IDENTIFIER) {
        Report_Error(node->location, "cannot resolve '%.*s' as callable or indexable",
                    (int)prefix->string_val.text.length, prefix->string_val.text.data);
    }

    return sm->type_integer;  /* Error recovery */
}

static Type_Info *Resolve_Expression(Symbol_Manager *sm, Syntax_Node *node) {
    if (!node) return NULL;

    switch (node->kind) {
        case NK_INTEGER:
            node->type = sm->type_universal_integer;
            return node->type;

        case NK_REAL:
            node->type = sm->type_universal_real;
            return node->type;

        case NK_CHARACTER:
            node->type = sm->type_character;
            return node->type;

        case NK_STRING:
            node->type = sm->type_string;
            return node->type;

        case NK_NULL:
            node->type = NULL;  /* Matches any access type */
            return NULL;

        case NK_IDENTIFIER:
            return Resolve_Identifier(sm, node);

        case NK_SELECTED:
            return Resolve_Selected(sm, node);

        case NK_BINARY_OP:
            return Resolve_Binary_Op(sm, node);

        case NK_UNARY_OP:
            node->type = Resolve_Expression(sm, node->unary.operand);
            if (node->unary.op == TK_NOT) {
                node->type = sm->type_boolean;
            }
            return node->type;

        case NK_APPLY:
            return Resolve_Apply(sm, node);

        case NK_ATTRIBUTE:
            Resolve_Expression(sm, node->attribute.prefix);
            if (node->attribute.argument) {
                Resolve_Expression(sm, node->attribute.argument);
            }
            /* Attribute type depends on attribute name */
            node->type = sm->type_integer;  /* Simplified */
            return node->type;

        case NK_QUALIFIED:
            Resolve_Expression(sm, node->qualified.subtype_mark);
            Resolve_Expression(sm, node->qualified.expression);
            node->type = node->qualified.subtype_mark->type;
            return node->type;

        case NK_AGGREGATE:
            {
                Type_Info *agg_type = node->type;
                bool is_record_agg = agg_type && agg_type->kind == TYPE_RECORD;

                for (uint32_t i = 0; i < node->aggregate.items.count; i++) {
                    Syntax_Node *item = node->aggregate.items.items[i];

                    if (is_record_agg && item->kind == NK_ASSOCIATION) {
                        /* For record aggregates, choices are field names - don't resolve as variables */
                        /* Just resolve the value expression */
                        if (item->association.expression) {
                            Resolve_Expression(sm, item->association.expression);
                        }
                    } else {
                        Resolve_Expression(sm, item);
                    }
                }
                return node->type;  /* Type from context */
            }

        case NK_ALLOCATOR:
            Resolve_Expression(sm, node->allocator.subtype_mark);
            if (node->allocator.expression) {
                Resolve_Expression(sm, node->allocator.expression);
            }
            /* Create access type to subtype_mark's type */
            node->type = Type_New(TYPE_ACCESS, S(""));
            return node->type;

        case NK_RANGE:
            if (node->range.low) Resolve_Expression(sm, node->range.low);
            if (node->range.high) Resolve_Expression(sm, node->range.high);
            return node->range.low ? node->range.low->type : NULL;

        case NK_ASSOCIATION:
            for (uint32_t i = 0; i < node->association.choices.count; i++) {
                Resolve_Expression(sm, node->association.choices.items[i]);
            }
            if (node->association.expression) {
                /* For case alternatives, expression is a block with statements */
                if (node->association.expression->kind == NK_BLOCK) {
                    Resolve_Statement(sm, node->association.expression);
                } else {
                    Resolve_Expression(sm, node->association.expression);
                }
            }
            return node->association.expression ? node->association.expression->type : NULL;

        case NK_ARRAY_TYPE:
            {
                /* Create array type info from syntax node */
                Type_Info *array_type = Type_New(TYPE_ARRAY, S(""));
                array_type->array.is_constrained = node->array_type.is_constrained;
                array_type->array.index_count = (uint32_t)node->array_type.indices.count;

                /* Allocate index info */
                if (array_type->array.index_count > 0) {
                    array_type->array.indices = Arena_Allocate(
                        array_type->array.index_count * sizeof(Index_Info));

                    for (uint32_t i = 0; i < array_type->array.index_count; i++) {
                        Syntax_Node *idx = node->array_type.indices.items[i];
                        Resolve_Expression(sm, idx);

                        Index_Info *info = &array_type->array.indices[i];
                        info->index_type = sm->type_integer;

                        /* Extract bounds from range node */
                        if (idx->kind == NK_RANGE && idx->range.low && idx->range.high) {
                            if (idx->range.low->kind == NK_INTEGER) {
                                info->low_bound = (Type_Bound){
                                    .kind = BOUND_INTEGER,
                                    .int_value = idx->range.low->integer_lit.value
                                };
                            }
                            if (idx->range.high->kind == NK_INTEGER) {
                                info->high_bound = (Type_Bound){
                                    .kind = BOUND_INTEGER,
                                    .int_value = idx->range.high->integer_lit.value
                                };
                            }
                        }
                    }
                }

                /* Resolve component type */
                if (node->array_type.component_type) {
                    Resolve_Expression(sm, node->array_type.component_type);
                    array_type->array.element_type = node->array_type.component_type->type;
                } else {
                    array_type->array.element_type = sm->type_integer;
                }

                /* Compute size */
                if (array_type->array.is_constrained && array_type->array.index_count > 0) {
                    int64_t count = 1;
                    for (uint32_t i = 0; i < array_type->array.index_count; i++) {
                        int64_t lo = Type_Bound_Value(array_type->array.indices[i].low_bound);
                        int64_t hi = Type_Bound_Value(array_type->array.indices[i].high_bound);
                        count *= (hi - lo + 1);
                    }
                    uint32_t elem_size = array_type->array.element_type ?
                                         array_type->array.element_type->size : 8;
                    array_type->size = (uint32_t)(count * elem_size);
                }

                node->type = array_type;
                return array_type;
            }

        case NK_ENUMERATION_TYPE:
            {
                /* Create enumeration type info from syntax node
                 * Note: Literal symbols are created later in NK_TYPE_DECL processing
                 * so they reference the named type, not this anonymous type */
                Type_Info *enum_type = Type_New(TYPE_ENUMERATION, S(""));
                uint32_t lit_count = (uint32_t)node->enum_type.literals.count;

                enum_type->enumeration.literal_count = lit_count;
                if (lit_count > 0) {
                    enum_type->enumeration.literals = Arena_Allocate(
                        lit_count * sizeof(String_Slice));

                    for (uint32_t i = 0; i < lit_count; i++) {
                        Syntax_Node *lit = node->enum_type.literals.items[i];
                        enum_type->enumeration.literals[i] = lit->string_val.text;
                    }
                }

                /* Size based on number of literals */
                if (lit_count <= 256) {
                    enum_type->size = 1;  /* Fits in 1 byte */
                } else if (lit_count <= 65536) {
                    enum_type->size = 2;  /* Fits in 2 bytes */
                } else {
                    enum_type->size = 4;  /* 4 bytes for large enums */
                }
                enum_type->alignment = enum_type->size;
                enum_type->low_bound = (Type_Bound){.kind = BOUND_INTEGER, .int_value = 0};
                enum_type->high_bound = (Type_Bound){.kind = BOUND_INTEGER, .int_value = lit_count - 1};

                node->type = enum_type;
                return enum_type;
            }

        case NK_RECORD_TYPE:
            {
                /* Create record type info from syntax node */
                Type_Info *record_type = Type_New(TYPE_RECORD, S(""));
                uint32_t comp_count = (uint32_t)node->record_type.components.count;

                record_type->record.component_count = comp_count;
                if (comp_count > 0) {
                    record_type->record.components = Arena_Allocate(
                        comp_count * sizeof(Component_Info));

                    uint32_t offset = 0;
                    for (uint32_t i = 0; i < comp_count; i++) {
                        Syntax_Node *comp = node->record_type.components.items[i];
                        Component_Info *info = &record_type->record.components[i];

                        /* Resolve component type */
                        if (comp->kind == NK_COMPONENT_DECL) {
                            Resolve_Expression(sm, comp->component.component_type);
                            Type_Info *comp_type = comp->component.component_type ?
                                                   comp->component.component_type->type : sm->type_integer;

                            info->name = comp->component.names.count > 0 ?
                                        comp->component.names.items[0]->string_val.text : S("");
                            info->component_type = comp_type;
                            info->byte_offset = offset;
                            info->bit_offset = 0;
                            info->bit_size = comp_type ? comp_type->size * 8 : 64;

                            /* Align to component size */
                            uint32_t comp_size = comp_type ? comp_type->size : 8;
                            offset += comp_size;
                        }
                    }
                    record_type->size = offset;
                    record_type->alignment = 8;  /* Conservative alignment */
                }

                node->type = record_type;
                return record_type;
            }

        case NK_SUBTYPE_INDICATION:
            {
                /* Resolve the base type */
                Resolve_Expression(sm, node->subtype_ind.subtype_mark);
                Type_Info *base_type = node->subtype_ind.subtype_mark->type;

                if (!base_type) {
                    return NULL;
                }

                /* Check for index constraint (STRING(1..5) style) */
                Syntax_Node *constraint = node->subtype_ind.constraint;
                if (constraint && constraint->kind == NK_INDEX_CONSTRAINT &&
                    (base_type->kind == TYPE_STRING || base_type->kind == TYPE_ARRAY)) {
                    /* Create constrained array type */
                    Type_Info *constrained = Type_New(TYPE_ARRAY, base_type->name);
                    constrained->array.is_constrained = true;
                    constrained->array.index_count = (uint32_t)constraint->index_constraint.ranges.count;

                    /* For STRING, element type is CHARACTER */
                    if (base_type->kind == TYPE_STRING) {
                        constrained->array.element_type = sm->type_character;
                    } else if (base_type->array.element_type) {
                        constrained->array.element_type = base_type->array.element_type;
                    }

                    /* Process index constraints */
                    if (constrained->array.index_count > 0) {
                        constrained->array.indices = Arena_Allocate(
                            constrained->array.index_count * sizeof(Index_Info));

                        for (uint32_t i = 0; i < constrained->array.index_count; i++) {
                            Syntax_Node *range = constraint->index_constraint.ranges.items[i];
                            Resolve_Expression(sm, range);

                            Index_Info *info = &constrained->array.indices[i];
                            info->index_type = sm->type_integer;

                            if (range->kind == NK_RANGE) {
                                if (range->range.low && range->range.low->kind == NK_INTEGER) {
                                    info->low_bound = (Type_Bound){
                                        .kind = BOUND_INTEGER,
                                        .int_value = range->range.low->integer_lit.value
                                    };
                                }
                                if (range->range.high && range->range.high->kind == NK_INTEGER) {
                                    info->high_bound = (Type_Bound){
                                        .kind = BOUND_INTEGER,
                                        .int_value = range->range.high->integer_lit.value
                                    };
                                }
                            }
                        }

                        /* Compute size */
                        int64_t count = 1;
                        for (uint32_t i = 0; i < constrained->array.index_count; i++) {
                            int64_t lo = Type_Bound_Value(constrained->array.indices[i].low_bound);
                            int64_t hi = Type_Bound_Value(constrained->array.indices[i].high_bound);
                            count *= (hi - lo + 1);
                        }
                        uint32_t elem_size = constrained->array.element_type ?
                                             constrained->array.element_type->size : 1;
                        constrained->size = (uint32_t)(count * elem_size);
                    }

                    node->type = constrained;
                    return constrained;
                }

                /* Otherwise just use base type */
                node->type = base_type;
                return base_type;
            }

        case NK_INTEGER_TYPE:
            {
                /* Integer type definition: range L .. H or mod M */
                Type_Info *int_type = Type_New(TYPE_INTEGER, S(""));

                /* Resolve range bounds if present */
                if (node->real_type.range) {
                    Resolve_Expression(sm, node->real_type.range);
                    Syntax_Node *range = node->real_type.range;
                    if (range->kind == NK_RANGE && range->range.low && range->range.high) {
                        if (range->range.low->kind == NK_INTEGER) {
                            int_type->low_bound = (Type_Bound){
                                .kind = BOUND_INTEGER,
                                .int_value = range->range.low->integer_lit.value
                            };
                        }
                        if (range->range.high->kind == NK_INTEGER) {
                            int_type->high_bound = (Type_Bound){
                                .kind = BOUND_INTEGER,
                                .int_value = range->range.high->integer_lit.value
                            };
                        }
                    }
                }

                /* Compute appropriate size */
                int_type->size = 8;  /* Default to 64-bit */
                int_type->alignment = 8;
                node->type = int_type;
                return int_type;
            }

        case NK_REAL_TYPE:
            {
                /* Real type definition: digits D or delta D */
                if (node->real_type.delta) {
                    /* Fixed-point type: TYPE_FIXED with delta */
                    Type_Info *fixed_type = Type_New(TYPE_FIXED, S(""));
                    Resolve_Expression(sm, node->real_type.delta);

                    /* Extract delta value */
                    double delta = 0.0;
                    if (node->real_type.delta->kind == NK_REAL) {
                        delta = node->real_type.delta->real_lit.value;
                    } else if (node->real_type.delta->kind == NK_INTEGER) {
                        delta = (double)node->real_type.delta->integer_lit.value;
                    } else {
                        delta = 0.001;  /* Default fallback */
                    }

                    /* Compute small as largest power of 2 <= delta (per RM 3.5.9) */
                    double small = 1.0;
                    if (delta > 0.0) {
                        /* Find largest 2^n <= delta */
                        while (small > delta) small /= 2.0;
                        while (small * 2.0 <= delta) small *= 2.0;
                    }

                    /* Compute scale factor: small = 2^scale */
                    int scale = 0;
                    double temp = small;
                    if (temp >= 1.0) {
                        while (temp >= 2.0) { temp /= 2.0; scale++; }
                    } else {
                        while (temp < 1.0) { temp *= 2.0; scale--; }
                    }

                    fixed_type->fixed.delta = delta;
                    fixed_type->fixed.small = small;
                    fixed_type->fixed.scale = scale;

                    /* Resolve range if present */
                    if (node->real_type.range) {
                        Resolve_Expression(sm, node->real_type.range);
                        Syntax_Node *range = node->real_type.range;
                        if (range->kind == NK_RANGE) {
                            if (range->range.low && range->range.low->kind == NK_REAL) {
                                fixed_type->low_bound = (Type_Bound){
                                    .kind = BOUND_FLOAT,
                                    .float_value = range->range.low->real_lit.value
                                };
                            }
                            if (range->range.high && range->range.high->kind == NK_REAL) {
                                fixed_type->high_bound = (Type_Bound){
                                    .kind = BOUND_FLOAT,
                                    .float_value = range->range.high->real_lit.value
                                };
                            }
                        }
                    }

                    /* Size: typically 32 or 64 bits depending on range and precision */
                    fixed_type->size = 8;  /* 64-bit for safe default */
                    fixed_type->alignment = 8;
                    node->type = fixed_type;
                    return fixed_type;
                } else {
                    /* Floating-point type: digits D */
                    Type_Info *float_type = Type_New(TYPE_FLOAT, S(""));

                    /* Resolve digits expression */
                    if (node->real_type.precision) {
                        Resolve_Expression(sm, node->real_type.precision);
                    }

                    /* Resolve range if present */
                    if (node->real_type.range) {
                        Resolve_Expression(sm, node->real_type.range);
                    }

                    /* Size based on digits: <=6 = float, >6 = double */
                    int digits = 15;  /* Default double precision */
                    if (node->real_type.precision &&
                        node->real_type.precision->kind == NK_INTEGER) {
                        digits = (int)node->real_type.precision->integer_lit.value;
                    }
                    float_type->size = (digits <= 6) ? 4 : 8;
                    float_type->alignment = float_type->size;
                    node->type = float_type;
                    return float_type;
                }
            }

        default:
            return NULL;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §12.2 Statement Resolution
 * ───────────────────────────────────────────────────────────────────────── */

static void Resolve_Statement(Symbol_Manager *sm, Syntax_Node *node);
static void Resolve_Declaration_List(Symbol_Manager *sm, Node_List *list);
static void Freeze_Declaration_List(Node_List *list);

static void Resolve_Statement_List(Symbol_Manager *sm, Node_List *list) {
    for (uint32_t i = 0; i < list->count; i++) {
        Resolve_Statement(sm, list->items[i]);
    }
}

static void Resolve_Statement(Symbol_Manager *sm, Syntax_Node *node) {
    if (!node) return;

    switch (node->kind) {
        case NK_ASSIGNMENT:
            Resolve_Expression(sm, node->assignment.target);
            /* Propagate target type to aggregate values for context-dependent typing */
            if (node->assignment.value->kind == NK_AGGREGATE &&
                node->assignment.target->type) {
                node->assignment.value->type = node->assignment.target->type;
            }
            Resolve_Expression(sm, node->assignment.value);
            /* Type check: value must be compatible with target */
            if (node->assignment.target->type && node->assignment.value->type) {
                if (!Type_Covers(node->assignment.target->type,
                                node->assignment.value->type)) {
                    Report_Error(node->location, "type mismatch in assignment");
                }
            }
            break;

        case NK_CALL_STMT:
            Resolve_Expression(sm, node->assignment.target);
            break;

        case NK_RETURN:
            if (node->return_stmt.expression) {
                Resolve_Expression(sm, node->return_stmt.expression);
            }
            break;

        case NK_IF:
            Resolve_Expression(sm, node->if_stmt.condition);
            Resolve_Statement_List(sm, &node->if_stmt.then_stmts);
            for (uint32_t i = 0; i < node->if_stmt.elsif_parts.count; i++) {
                Resolve_Statement(sm, node->if_stmt.elsif_parts.items[i]);
            }
            Resolve_Statement_List(sm, &node->if_stmt.else_stmts);
            break;

        case NK_CASE:
            Resolve_Expression(sm, node->case_stmt.expression);
            for (uint32_t i = 0; i < node->case_stmt.alternatives.count; i++) {
                Resolve_Statement(sm, node->case_stmt.alternatives.items[i]);
            }
            break;

        case NK_LOOP:
            {
                Syntax_Node *iter = node->loop_stmt.iteration_scheme;
                bool is_for_loop = iter && iter->kind == NK_BINARY_OP &&
                                   iter->binary.op == TK_IN;
                if (is_for_loop) {
                    /* FOR loop - create loop variable in new scope.
                     * Inherit owner from enclosing scope for proper name mangling */
                    Symbol *enclosing_owner = sm->current_scope->owner;
                    Symbol_Manager_Push_Scope(sm, enclosing_owner);
                    Syntax_Node *loop_id = iter->binary.left;
                    if (loop_id && loop_id->kind == NK_IDENTIFIER) {
                        Symbol *loop_var = Symbol_New(SYMBOL_VARIABLE,
                                                      loop_id->string_val.text,
                                                      loop_id->location);
                        loop_var->type = sm->type_integer;
                        Symbol_Add(sm, loop_var);
                        loop_id->symbol = loop_var;
                    }
                    /* Resolve range expression */
                    Resolve_Expression(sm, iter->binary.right);
                    Resolve_Statement_List(sm, &node->loop_stmt.statements);
                    Symbol_Manager_Pop_Scope(sm);
                } else {
                    /* WHILE or bare LOOP */
                    if (iter) Resolve_Expression(sm, iter);
                    Resolve_Statement_List(sm, &node->loop_stmt.statements);
                }
            }
            break;

        case NK_BLOCK:
            /* Inherit owner from enclosing scope for proper symbol parenting */
            Symbol_Manager_Push_Scope(sm, sm->current_scope->owner);
            /* Resolve declarations first (adds symbols to scope) */
            Resolve_Declaration_List(sm, &node->block_stmt.declarations);
            /* Freeze all types at end of declarative part (RM 13.14) */
            Freeze_Declaration_List(&node->block_stmt.declarations);
            /* Then resolve statements that use those symbols */
            Resolve_Statement_List(sm, &node->block_stmt.statements);
            for (uint32_t i = 0; i < node->block_stmt.handlers.count; i++) {
                Resolve_Statement(sm, node->block_stmt.handlers.items[i]);
            }
            Symbol_Manager_Pop_Scope(sm);
            break;

        case NK_EXIT:
            if (node->exit_stmt.condition) {
                Resolve_Expression(sm, node->exit_stmt.condition);
            }
            break;

        case NK_RAISE:
            if (node->raise_stmt.exception_name) {
                Resolve_Expression(sm, node->raise_stmt.exception_name);
            }
            break;

        case NK_EXCEPTION_HANDLER:
            /* Resolve exception names */
            for (uint32_t i = 0; i < node->handler.exceptions.count; i++) {
                Syntax_Node *exc = node->handler.exceptions.items[i];
                if (exc && exc->kind != NK_OTHERS) {
                    Resolve_Expression(sm, exc);
                }
            }
            Resolve_Statement_List(sm, &node->handler.statements);
            break;

        case NK_ASSOCIATION:
            /* Case alternative - resolve choices and body */
            for (uint32_t i = 0; i < node->association.choices.count; i++) {
                Resolve_Expression(sm, node->association.choices.items[i]);
            }
            if (node->association.expression) {
                if (node->association.expression->kind == NK_BLOCK) {
                    Resolve_Statement(sm, node->association.expression);
                } else {
                    Resolve_Expression(sm, node->association.expression);
                }
            }
            break;

        default:
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §12.3 Declaration Resolution
 * ───────────────────────────────────────────────────────────────────────── */

static void Resolve_Declaration(Symbol_Manager *sm, Syntax_Node *node);
static char *Lookup_Path(String_Slice name);
static void Load_Package_Spec(Symbol_Manager *sm, String_Slice name, char *src);

static void Resolve_Declaration_List(Symbol_Manager *sm, Node_List *list) {
    for (uint32_t i = 0; i < list->count; i++) {
        Resolve_Declaration(sm, list->items[i]);
    }
}

/* Freeze all types declared in a list
 * Per RM 13.14: At the end of a declarative part, all entities are frozen */
static void Freeze_Declaration_List(Node_List *list) {
    for (uint32_t i = 0; i < list->count; i++) {
        Syntax_Node *node = list->items[i];
        if (!node) continue;

        switch (node->kind) {
            case NK_TYPE_DECL:
            case NK_SUBTYPE_DECL:
                if (node->symbol && node->symbol->type) {
                    Freeze_Type(node->symbol->type);
                }
                break;

            case NK_OBJECT_DECL:
                /* Objects already freeze their type at declaration */
                break;

            case NK_PROCEDURE_BODY:
            case NK_FUNCTION_BODY:
                /* Subprogram bodies freeze all visible entities */
                break;

            default:
                break;
        }
    }
}

static void Resolve_Declaration(Symbol_Manager *sm, Syntax_Node *node) {
    if (!node) return;

    switch (node->kind) {
        case NK_OBJECT_DECL:
            /* Resolve type */
            if (node->object_decl.object_type) {
                Resolve_Expression(sm, node->object_decl.object_type);
                /* Object declarations freeze the type (RM 13.14) */
                if (node->object_decl.object_type->type) {
                    Freeze_Type(node->object_decl.object_type->type);
                }
            }
            /* Resolve initializer */
            if (node->object_decl.init) {
                Resolve_Expression(sm, node->object_decl.init);
            }
            /* Add symbols for each name */
            for (uint32_t i = 0; i < node->object_decl.names.count; i++) {
                Syntax_Node *name_node = node->object_decl.names.items[i];
                Symbol *sym = Symbol_New(
                    node->object_decl.is_constant ? SYMBOL_CONSTANT : SYMBOL_VARIABLE,
                    name_node->string_val.text,
                    name_node->location);
                sym->type = node->object_decl.object_type ? node->object_decl.object_type->type : NULL;
                sym->declaration = node;
                Symbol_Add(sm, sym);
                name_node->symbol = sym;
            }
            break;

        case NK_TYPE_DECL:
            {
                Symbol *sym = Symbol_New(SYMBOL_TYPE, node->type_decl.name, node->location);
                Type_Info *type = Type_New(TYPE_UNKNOWN, node->type_decl.name);
                sym->type = type;
                type->defining_symbol = sym;
                Symbol_Add(sm, sym);
                node->symbol = sym;

                /* Resolve definition and copy type info */
                if (node->type_decl.definition) {
                    Type_Info *def_type = Resolve_Expression(sm, node->type_decl.definition);
                    if (def_type) {
                        /* Copy type info from definition to named type */
                        type->kind = def_type->kind;
                        type->size = def_type->size;
                        type->alignment = def_type->alignment;
                        type->low_bound = def_type->low_bound;
                        type->high_bound = def_type->high_bound;

                        if (def_type->kind == TYPE_ARRAY) {
                            type->array = def_type->array;
                        } else if (def_type->kind == TYPE_RECORD) {
                            type->record = def_type->record;
                        } else if (def_type->kind == TYPE_ACCESS) {
                            type->access = def_type->access;
                        } else if (def_type->kind == TYPE_ENUMERATION) {
                            type->enumeration = def_type->enumeration;

                            /* Create symbols for enumeration literals
                             * They must reference the named type (type), not the anonymous def_type */
                            if (node->type_decl.definition &&
                                node->type_decl.definition->kind == NK_ENUMERATION_TYPE) {
                                Node_List *lits = &node->type_decl.definition->enum_type.literals;
                                for (uint32_t i = 0; i < lits->count; i++) {
                                    Syntax_Node *lit = lits->items[i];
                                    Symbol *lit_sym = Symbol_New(SYMBOL_LITERAL, lit->string_val.text, lit->location);
                                    lit_sym->type = type;  /* Reference the named type */
                                    lit_sym->frame_offset = (int64_t)i;  /* Store ordinal position */
                                    Symbol_Add(sm, lit_sym);
                                    lit->symbol = lit_sym;
                                }
                            }
                        }
                    }
                }
            }
            break;

        case NK_SUBTYPE_DECL:
            {
                Symbol *sym = Symbol_New(SYMBOL_SUBTYPE, node->type_decl.name, node->location);
                Symbol_Add(sm, sym);
                node->symbol = sym;

                if (node->type_decl.definition) {
                    Resolve_Expression(sm, node->type_decl.definition);
                    sym->type = node->type_decl.definition->type;
                }
            }
            break;

        case NK_PROCEDURE_SPEC:
        case NK_FUNCTION_SPEC:
            {
                Symbol *sym = Symbol_New(
                    node->kind == NK_PROCEDURE_SPEC ? SYMBOL_PROCEDURE : SYMBOL_FUNCTION,
                    node->subprogram_spec.name, node->location);

                /* Count total parameters (each param_spec can have multiple names) */
                Node_List *param_list = &node->subprogram_spec.parameters;
                uint32_t total_params = 0;
                for (uint32_t i = 0; i < param_list->count; i++) {
                    Syntax_Node *ps = param_list->items[i];
                    if (ps->kind == NK_PARAM_SPEC) {
                        total_params += ps->param_spec.names.count;
                    }
                }

                sym->parameter_count = total_params;
                if (total_params > 0) {
                    sym->parameters = Arena_Allocate(total_params * sizeof(Parameter_Info));
                    uint32_t param_idx = 0;
                    for (uint32_t i = 0; i < param_list->count; i++) {
                        Syntax_Node *ps = param_list->items[i];
                        if (ps->kind == NK_PARAM_SPEC) {
                            /* Resolve parameter type */
                            if (ps->param_spec.param_type) {
                                Resolve_Expression(sm, ps->param_spec.param_type);
                            }
                            Type_Info *pt = ps->param_spec.param_type ?
                                          ps->param_spec.param_type->type : NULL;
                            for (uint32_t j = 0; j < ps->param_spec.names.count; j++) {
                                Syntax_Node *name = ps->param_spec.names.items[j];
                                sym->parameters[param_idx].name = name->string_val.text;
                                sym->parameters[param_idx].param_type = pt;
                                sym->parameters[param_idx].mode = (Parameter_Mode)ps->param_spec.mode;
                                sym->parameters[param_idx].default_value = ps->param_spec.default_expr;
                                param_idx++;
                            }
                        }
                    }
                }

                if (node->subprogram_spec.return_type) {
                    Resolve_Expression(sm, node->subprogram_spec.return_type);
                    sym->return_type = node->subprogram_spec.return_type->type;
                }
                Symbol_Add(sm, sym);
                node->symbol = sym;
            }
            break;

        case NK_PROCEDURE_BODY:
        case NK_FUNCTION_BODY:
            {
                /* Check if this body completes a generic declaration */
                Syntax_Node *spec = node->subprogram_body.specification;
                String_Slice body_name = spec ? spec->subprogram_spec.name : (String_Slice){0};

                Symbol *matching_generic = Symbol_Find(sm, body_name);
                if (matching_generic && matching_generic->kind == SYMBOL_GENERIC) {
                    /* This body completes a generic - store it and don't process further */
                    matching_generic->generic_body = node;
                    node->symbol = matching_generic;
                    break;  /* Don't resolve body now - will be resolved during instantiation */
                }

                /* Resolve spec if present */
                if (spec) {
                    Resolve_Declaration(sm, spec);
                    node->symbol = spec->symbol;
                }

                /* Push scope for body */
                Symbol_Manager_Push_Scope(sm, node->symbol);

                /* Link the scope to the symbol for static link access */
                if (node->symbol) {
                    node->symbol->scope = sm->current_scope;
                }

                /* Add parameters to scope and link to Parameter_Info */
                if (spec) {
                    Symbol *func_sym = node->symbol;
                    uint32_t param_idx = 0;
                    Node_List *params = &spec->subprogram_spec.parameters;
                    for (uint32_t i = 0; i < params->count; i++) {
                        Syntax_Node *param = params->items[i];
                        if (param->kind == NK_PARAM_SPEC) {
                            /* Resolve parameter type */
                            if (param->param_spec.param_type) {
                                Resolve_Expression(sm, param->param_spec.param_type);
                            }
                            /* Add each parameter name as a symbol */
                            for (uint32_t j = 0; j < param->param_spec.names.count; j++) {
                                Syntax_Node *name = param->param_spec.names.items[j];
                                Symbol *param_sym = Symbol_New(SYMBOL_PARAMETER,
                                    name->string_val.text, name->location);
                                if (param->param_spec.param_type) {
                                    param_sym->type = param->param_spec.param_type->type;
                                }
                                Symbol_Add(sm, param_sym);
                                name->symbol = param_sym;
                                /* Link to Parameter_Info for code generation */
                                if (func_sym && param_idx < func_sym->parameter_count) {
                                    func_sym->parameters[param_idx].param_sym = param_sym;
                                }
                                param_idx++;
                            }
                        }
                    }
                }

                /* Resolve declarations and statements */
                Resolve_Declaration_List(sm, &node->subprogram_body.declarations);
                /* Freeze all types at end of declarative part (RM 13.14) */
                Freeze_Declaration_List(&node->subprogram_body.declarations);
                Resolve_Statement_List(sm, &node->subprogram_body.statements);

                /* Resolve exception handlers */
                for (uint32_t i = 0; i < node->subprogram_body.handlers.count; i++) {
                    Resolve_Statement(sm, node->subprogram_body.handlers.items[i]);
                }

                Symbol_Manager_Pop_Scope(sm);
            }
            break;

        case NK_PACKAGE_SPEC:
            {
                Symbol *sym = Symbol_New(SYMBOL_PACKAGE, node->package_spec.name, node->location);
                Symbol_Add(sm, sym);
                node->symbol = sym;

                Symbol_Manager_Push_Scope(sm, sym);
                Resolve_Declaration_List(sm, &node->package_spec.visible_decls);
                Resolve_Declaration_List(sm, &node->package_spec.private_decls);
                /* End of package spec freezes all declared entities (RM 13.14) */
                Freeze_Declaration_List(&node->package_spec.visible_decls);
                Freeze_Declaration_List(&node->package_spec.private_decls);
                Symbol_Manager_Pop_Scope(sm);
            }
            break;

        case NK_PACKAGE_BODY:
            {
                /* Find or create package symbol for proper name mangling */
                Symbol *pkg_sym = NULL;
                String_Slice pkg_name = node->package_body.name;
                if (pkg_name.length > 0) {
                    pkg_sym = Symbol_Find(sm, pkg_name);
                    if (!pkg_sym) {
                        /* Try to load corresponding package spec first.
                         * This ensures body uses same symbol IDs as spec */
                        char *spec_src = Lookup_Path(pkg_name);
                        if (spec_src) {
                            Load_Package_Spec(sm, pkg_name, spec_src);
                            pkg_sym = Symbol_Find(sm, pkg_name);
                        }
                        if (!pkg_sym) {
                            /* Create package symbol if spec not found */
                            pkg_sym = Symbol_New(SYMBOL_PACKAGE, pkg_name, node->location);
                            Symbol_Add(sm, pkg_sym);
                        }
                    }
                    node->symbol = pkg_sym;
                }
                Symbol_Manager_Push_Scope(sm, pkg_sym);
                Resolve_Declaration_List(sm, &node->package_body.declarations);
                /* Freeze all types at end of declarative part (RM 13.14) */
                Freeze_Declaration_List(&node->package_body.declarations);
                Resolve_Statement_List(sm, &node->package_body.statements);
                Symbol_Manager_Pop_Scope(sm);
            }
            break;

        case NK_USE_CLAUSE:
            /* Make package contents directly visible (Ada 83 8.4)
             * "A use clause achieves direct visibility of declarations
             *  that appear in the visible parts of the named packages" */
            for (uint32_t i = 0; i < node->use_clause.names.count; i++) {
                Syntax_Node *pkg_name_node = node->use_clause.names.items[i];
                Resolve_Expression(sm, pkg_name_node);

                /* Find the package symbol */
                Symbol *pkg_sym = NULL;
                if (pkg_name_node->kind == NK_IDENTIFIER) {
                    pkg_sym = Symbol_Find(sm, pkg_name_node->string_val.text);
                } else if (pkg_name_node->symbol) {
                    pkg_sym = pkg_name_node->symbol;
                }

                /* Make visible declarations from package accessible */
                if (pkg_sym && pkg_sym->kind == SYMBOL_PACKAGE && pkg_sym->declaration) {
                    Syntax_Node *pkg_decl = pkg_sym->declaration;
                    if (pkg_decl->kind == NK_PACKAGE_SPEC) {
                        /* Iterate visible declarations and add to current scope */
                        for (uint32_t j = 0; j < pkg_decl->package_spec.visible_decls.count; j++) {
                            Syntax_Node *decl = pkg_decl->package_spec.visible_decls.items[j];
                            if (decl && decl->symbol) {
                                /* Create a use-visible alias in current scope */
                                Symbol *alias = Symbol_New(decl->symbol->kind,
                                                           decl->symbol->name,
                                                           decl->symbol->location);
                                alias->type = decl->symbol->type;
                                alias->declaration = decl->symbol->declaration;
                                alias->visibility = VIS_USE_VISIBLE;
                                alias->parameter_count = decl->symbol->parameter_count;
                                alias->parameters = decl->symbol->parameters;
                                alias->return_type = decl->symbol->return_type;
                                Symbol_Add(sm, alias);
                                /* Restore original symbol's identity for name mangling.
                                 * Symbol_Add sets parent/unique_id to local scope values,
                                 * but USE-visible symbols must retain original identity
                                 * so @REPORT__TEST_S5 not @A21001A__TEST_S99 is emitted */
                                alias->parent = decl->symbol->parent;
                                alias->unique_id = decl->symbol->unique_id;
                            }
                        }
                    }
                }
            }
            break;

        case NK_PRAGMA:
            /* Process pragma effects */
            {
                String_Slice pragma_name = node->pragma_node.name;

                /* pragma Inline(subprogram_name, ...) */
                if (Slice_Equal_Ignore_Case(pragma_name, S("INLINE"))) {
                    for (uint32_t i = 0; i < node->pragma_node.arguments.count; i++) {
                        Syntax_Node *arg = node->pragma_node.arguments.items[i];
                        Syntax_Node *name_node = (arg->kind == NK_ASSOCIATION) ?
                                                  arg->association.expression : arg;
                        if (name_node && name_node->kind == NK_IDENTIFIER) {
                            Symbol *sym = Symbol_Find(sm, name_node->string_val.text);
                            if (sym && (sym->kind == SYMBOL_PROCEDURE ||
                                        sym->kind == SYMBOL_FUNCTION)) {
                                sym->is_inline = true;
                            }
                        }
                    }
                }

                /* pragma Pack(type_name) */
                else if (Slice_Equal_Ignore_Case(pragma_name, S("PACK"))) {
                    if (node->pragma_node.arguments.count > 0) {
                        Syntax_Node *arg = node->pragma_node.arguments.items[0];
                        Syntax_Node *name_node = (arg->kind == NK_ASSOCIATION) ?
                                                  arg->association.expression : arg;
                        if (name_node && name_node->kind == NK_IDENTIFIER) {
                            Symbol *sym = Symbol_Find(sm, name_node->string_val.text);
                            if (sym && sym->type) {
                                sym->type->is_packed = true;
                            }
                        }
                    }
                }

                /* pragma Suppress(check_name) or pragma Suppress(check_name, entity_name) */
                else if (Slice_Equal_Ignore_Case(pragma_name, S("SUPPRESS"))) {
                    uint32_t check_bit = 0;
                    if (node->pragma_node.arguments.count > 0) {
                        Syntax_Node *arg = node->pragma_node.arguments.items[0];
                        Syntax_Node *check_node = (arg->kind == NK_ASSOCIATION) ?
                                                   arg->association.expression : arg;
                        if (check_node && check_node->kind == NK_IDENTIFIER) {
                            String_Slice check = check_node->string_val.text;
                            if (Slice_Equal_Ignore_Case(check, S("RANGE_CHECK")))
                                check_bit = 1;
                            else if (Slice_Equal_Ignore_Case(check, S("OVERFLOW_CHECK")))
                                check_bit = 2;
                            else if (Slice_Equal_Ignore_Case(check, S("INDEX_CHECK")))
                                check_bit = 4;
                            else if (Slice_Equal_Ignore_Case(check, S("LENGTH_CHECK")))
                                check_bit = 8;
                            else if (Slice_Equal_Ignore_Case(check, S("ALL_CHECKS")))
                                check_bit = 0xFFFFFFFF;
                        }
                    }

                    /* Apply to specific entity or current scope */
                    if (node->pragma_node.arguments.count > 1) {
                        Syntax_Node *arg = node->pragma_node.arguments.items[1];
                        Syntax_Node *entity = (arg->kind == NK_ASSOCIATION) ?
                                               arg->association.expression : arg;
                        if (entity && entity->kind == NK_IDENTIFIER) {
                            Symbol *sym = Symbol_Find(sm, entity->string_val.text);
                            if (sym) sym->suppressed_checks |= check_bit;
                        }
                    }
                    /* else: would apply to enclosing scope */
                }

                /* pragma Import(Convention, Entity, External_Name, Link_Name) */
                else if (Slice_Equal_Ignore_Case(pragma_name, S("IMPORT"))) {
                    if (node->pragma_node.arguments.count >= 2) {
                        /* Get convention */
                        Syntax_Node *conv_arg = node->pragma_node.arguments.items[0];
                        Syntax_Node *conv_node = (conv_arg->kind == NK_ASSOCIATION) ?
                                                  conv_arg->association.expression : conv_arg;

                        /* Get entity */
                        Syntax_Node *ent_arg = node->pragma_node.arguments.items[1];
                        Syntax_Node *ent_node = (ent_arg->kind == NK_ASSOCIATION) ?
                                                 ent_arg->association.expression : ent_arg;

                        if (ent_node && ent_node->kind == NK_IDENTIFIER) {
                            Symbol *sym = Symbol_Find(sm, ent_node->string_val.text);
                            if (sym) {
                                sym->is_imported = true;

                                /* Set convention */
                                if (conv_node && conv_node->kind == NK_IDENTIFIER) {
                                    String_Slice conv = conv_node->string_val.text;
                                    if (Slice_Equal_Ignore_Case(conv, S("C")))
                                        sym->convention = CONVENTION_C;
                                    else if (Slice_Equal_Ignore_Case(conv, S("STDCALL")))
                                        sym->convention = CONVENTION_STDCALL;
                                    else if (Slice_Equal_Ignore_Case(conv, S("INTRINSIC")))
                                        sym->convention = CONVENTION_INTRINSIC;
                                }

                                /* Get external name if provided */
                                if (node->pragma_node.arguments.count >= 3) {
                                    Syntax_Node *name_arg = node->pragma_node.arguments.items[2];
                                    Syntax_Node *name_node = (name_arg->kind == NK_ASSOCIATION) ?
                                                              name_arg->association.expression : name_arg;
                                    if (name_node && name_node->kind == NK_STRING) {
                                        sym->external_name = name_node->string_val.text;
                                    }
                                }
                            }
                        }
                    }
                }

                /* pragma Export(Convention, Entity, External_Name) */
                else if (Slice_Equal_Ignore_Case(pragma_name, S("EXPORT"))) {
                    if (node->pragma_node.arguments.count >= 2) {
                        Syntax_Node *conv_arg = node->pragma_node.arguments.items[0];
                        Syntax_Node *conv_node = (conv_arg->kind == NK_ASSOCIATION) ?
                                                  conv_arg->association.expression : conv_arg;

                        Syntax_Node *ent_arg = node->pragma_node.arguments.items[1];
                        Syntax_Node *ent_node = (ent_arg->kind == NK_ASSOCIATION) ?
                                                 ent_arg->association.expression : ent_arg;

                        if (ent_node && ent_node->kind == NK_IDENTIFIER) {
                            Symbol *sym = Symbol_Find(sm, ent_node->string_val.text);
                            if (sym) {
                                sym->is_exported = true;

                                if (conv_node && conv_node->kind == NK_IDENTIFIER) {
                                    String_Slice conv = conv_node->string_val.text;
                                    if (Slice_Equal_Ignore_Case(conv, S("C")))
                                        sym->convention = CONVENTION_C;
                                }

                                if (node->pragma_node.arguments.count >= 3) {
                                    Syntax_Node *name_arg = node->pragma_node.arguments.items[2];
                                    Syntax_Node *name_node = (name_arg->kind == NK_ASSOCIATION) ?
                                                              name_arg->association.expression : name_arg;
                                    if (name_node && name_node->kind == NK_STRING) {
                                        sym->external_name = name_node->string_val.text;
                                    }
                                }
                            }
                        }
                    }
                }

                /* pragma Unreferenced(name, ...) */
                else if (Slice_Equal_Ignore_Case(pragma_name, S("UNREFERENCED"))) {
                    for (uint32_t i = 0; i < node->pragma_node.arguments.count; i++) {
                        Syntax_Node *arg = node->pragma_node.arguments.items[i];
                        Syntax_Node *name_node = (arg->kind == NK_ASSOCIATION) ?
                                                  arg->association.expression : arg;
                        if (name_node && name_node->kind == NK_IDENTIFIER) {
                            Symbol *sym = Symbol_Find(sm, name_node->string_val.text);
                            if (sym) sym->is_unreferenced = true;
                        }
                    }
                }

                /* pragma Convention(convention, entity) */
                else if (Slice_Equal_Ignore_Case(pragma_name, S("CONVENTION"))) {
                    if (node->pragma_node.arguments.count >= 2) {
                        Syntax_Node *conv_arg = node->pragma_node.arguments.items[0];
                        Syntax_Node *conv_node = (conv_arg->kind == NK_ASSOCIATION) ?
                                                  conv_arg->association.expression : conv_arg;

                        Syntax_Node *ent_arg = node->pragma_node.arguments.items[1];
                        Syntax_Node *ent_node = (ent_arg->kind == NK_ASSOCIATION) ?
                                                 ent_arg->association.expression : ent_arg;

                        if (ent_node && ent_node->kind == NK_IDENTIFIER) {
                            Symbol *sym = Symbol_Find(sm, ent_node->string_val.text);
                            if (sym && conv_node && conv_node->kind == NK_IDENTIFIER) {
                                String_Slice conv = conv_node->string_val.text;
                                if (Slice_Equal_Ignore_Case(conv, S("C")))
                                    sym->convention = CONVENTION_C;
                                else if (Slice_Equal_Ignore_Case(conv, S("STDCALL")))
                                    sym->convention = CONVENTION_STDCALL;
                            }
                        }
                    }
                }

                /* pragma Pure, pragma Preelaborate, etc. - informational only */
                /* pragma Elaborate, pragma Elaborate_All - handled at link time */
                /* pragma Restrictions - not implemented */
            }
            break;

        case NK_EXCEPTION_DECL:
            /* Exception declaration: E : exception; */
            for (uint32_t i = 0; i < node->exception_decl.names.count; i++) {
                Syntax_Node *name_node = node->exception_decl.names.items[i];
                if (name_node && name_node->kind == NK_IDENTIFIER) {
                    Symbol *sym = Symbol_New(SYMBOL_EXCEPTION,
                                             name_node->string_val.text,
                                             name_node->location);
                    Symbol_Add(sm, sym);
                    name_node->symbol = sym;
                    /* Add to global exception list for codegen */
                    if (Exception_Symbol_Count < 256) {
                        Exception_Symbols[Exception_Symbol_Count++] = sym;
                    }
                }
            }
            break;

        case NK_GENERIC_DECL:
            /* Generic declaration: generic ... procedure/function/package spec */
            {
                Syntax_Node *unit = node->generic_decl.unit;
                if (!unit) break;

                /* Get name from the unit */
                String_Slice name = {0};
                if (unit->kind == NK_PROCEDURE_SPEC || unit->kind == NK_FUNCTION_SPEC) {
                    name = unit->subprogram_spec.name;
                } else if (unit->kind == NK_PACKAGE_SPEC) {
                    name = unit->package_spec.name;
                }

                /* Create generic symbol */
                Symbol *sym = Symbol_New(SYMBOL_GENERIC, name, node->location);
                sym->declaration = node;
                sym->generic_unit = unit;

                /* Store formals list for later */
                if (node->generic_decl.formals.count > 0) {
                    sym->generic_formals = node->generic_decl.formals.items[0];
                }

                Symbol_Add(sm, sym);
                node->symbol = sym;

                /* DON'T resolve the unit here - it contains generic formals
                   that need to be substituted during instantiation */
            }
            break;

        case NK_GENERIC_INST:
            /* Generic instantiation: procedure/function X is new GENERIC_NAME(actuals) */
            {
                /* Find the generic template */
                Syntax_Node *gen_name = node->generic_inst.generic_name;
                if (!gen_name) {
                    Report_Error(node->location, "expected a generic unit name");
                    break;
                }

                /* Resolve the generic name to find template */
                Resolve_Expression(sm, gen_name);
                Symbol *template = NULL;
                if (gen_name->kind == NK_IDENTIFIER) {
                    template = Symbol_Find(sm, gen_name->string_val.text);
                }

                if (!template || template->kind != SYMBOL_GENERIC) {
                    Report_Error(node->location, "expected a generic unit name");
                    break;
                }

                /* Create instance symbol */
                Symbol_Kind inst_kind = (node->generic_inst.unit_kind == TK_FUNCTION) ?
                                        SYMBOL_FUNCTION : SYMBOL_PROCEDURE;
                Symbol *inst_sym = Symbol_New(inst_kind,
                                              node->generic_inst.instance_name,
                                              node->location);
                inst_sym->declaration = node;
                inst_sym->generic_template = template;

                /* Process generic actuals and build mapping */
                Node_List *formals = &template->declaration->generic_decl.formals;
                Node_List *actuals = &node->generic_inst.actuals;

                inst_sym->generic_actual_count = formals->count;
                if (formals->count > 0) {
                    inst_sym->generic_actuals = Arena_Allocate(
                        formals->count * sizeof(*inst_sym->generic_actuals));

                    for (uint32_t i = 0; i < formals->count; i++) {
                        Syntax_Node *formal = formals->items[i];
                        Syntax_Node *actual = (i < actuals->count) ? actuals->items[i] : NULL;

                        /* Get formal name */
                        if (formal->kind == NK_GENERIC_TYPE_PARAM) {
                            inst_sym->generic_actuals[i].formal_name =
                                formal->generic_type_param.name;

                            /* Resolve actual type */
                            if (actual) {
                                Syntax_Node *type_node = actual;
                                if (actual->kind == NK_ASSOCIATION) {
                                    type_node = actual->association.expression;
                                }
                                Resolve_Expression(sm, type_node);
                                inst_sym->generic_actuals[i].actual_type = type_node->type;
                            }
                        }
                    }
                }

                /* Copy parameter info from template unit */
                Syntax_Node *unit = template->generic_unit;
                if (unit && (unit->kind == NK_FUNCTION_SPEC || unit->kind == NK_PROCEDURE_SPEC)) {
                    Node_List *params = &unit->subprogram_spec.parameters;
                    uint32_t total_params = 0;
                    for (uint32_t i = 0; i < params->count; i++) {
                        Syntax_Node *ps = params->items[i];
                        if (ps->kind == NK_PARAM_SPEC) {
                            total_params += ps->param_spec.names.count;
                        }
                    }

                    inst_sym->parameter_count = total_params;
                    if (total_params > 0) {
                        inst_sym->parameters = Arena_Allocate(total_params * sizeof(Parameter_Info));
                        uint32_t idx = 0;
                        for (uint32_t i = 0; i < params->count; i++) {
                            Syntax_Node *ps = params->items[i];
                            if (ps->kind == NK_PARAM_SPEC) {
                                /* Resolve param type and substitute */
                                Type_Info *param_type = NULL;
                                if (ps->param_spec.param_type) {
                                    Syntax_Node *pt = ps->param_spec.param_type;
                                    if (pt->kind == NK_IDENTIFIER) {
                                        /* Check if this is a formal type parameter */
                                        for (uint32_t k = 0; k < inst_sym->generic_actual_count; k++) {
                                            if (Slice_Equal_Ignore_Case(pt->string_val.text,
                                                          inst_sym->generic_actuals[k].formal_name)) {
                                                param_type = inst_sym->generic_actuals[k].actual_type;
                                                break;
                                            }
                                        }
                                    }
                                }

                                for (uint32_t j = 0; j < ps->param_spec.names.count; j++) {
                                    Syntax_Node *name = ps->param_spec.names.items[j];
                                    inst_sym->parameters[idx].name = name->string_val.text;
                                    inst_sym->parameters[idx].param_type = param_type;
                                    inst_sym->parameters[idx].mode = (Parameter_Mode)ps->param_spec.mode;
                                    idx++;
                                }
                            }
                        }
                    }

                    /* Handle return type for functions */
                    if (unit->kind == NK_FUNCTION_SPEC && unit->subprogram_spec.return_type) {
                        Syntax_Node *rt = unit->subprogram_spec.return_type;
                        if (rt->kind == NK_IDENTIFIER) {
                            /* Check if return type is a formal type parameter */
                            for (uint32_t k = 0; k < inst_sym->generic_actual_count; k++) {
                                if (Slice_Equal_Ignore_Case(rt->string_val.text,
                                              inst_sym->generic_actuals[k].formal_name)) {
                                    inst_sym->return_type = inst_sym->generic_actuals[k].actual_type;
                                    break;
                                }
                            }
                        }
                    }
                }

                Symbol_Add(sm, inst_sym);
                node->symbol = inst_sym;

                /* Resolve the generic body with instance context */
                Syntax_Node *gen_body = template->generic_body;
                if (gen_body && (gen_body->kind == NK_FUNCTION_BODY ||
                                 gen_body->kind == NK_PROCEDURE_BODY)) {
                    /* Push scope for instance body resolution */
                    Symbol_Manager_Push_Scope(sm, inst_sym);
                    inst_sym->scope = sm->current_scope;

                    /* Add generic formal types to scope with actual types */
                    for (uint32_t k = 0; k < inst_sym->generic_actual_count; k++) {
                        if (inst_sym->generic_actuals[k].actual_type) {
                            Symbol *type_sym = Symbol_New(SYMBOL_TYPE,
                                inst_sym->generic_actuals[k].formal_name,
                                node->location);
                            type_sym->type = inst_sym->generic_actuals[k].actual_type;
                            Symbol_Add(sm, type_sym);
                        }
                    }

                    /* Add instance parameters to scope */
                    Syntax_Node *body_spec = gen_body->subprogram_body.specification;
                    if (body_spec) {
                        Node_List *params = &body_spec->subprogram_spec.parameters;
                        uint32_t param_idx = 0;
                        for (uint32_t i = 0; i < params->count && param_idx < inst_sym->parameter_count; i++) {
                            Syntax_Node *ps = params->items[i];
                            if (ps->kind == NK_PARAM_SPEC) {
                                for (uint32_t j = 0; j < ps->param_spec.names.count; j++) {
                                    Syntax_Node *name = ps->param_spec.names.items[j];
                                    Symbol *param_sym = Symbol_New(SYMBOL_PARAMETER,
                                        name->string_val.text, name->location);
                                    param_sym->type = inst_sym->parameters[param_idx].param_type;
                                    Symbol_Add(sm, param_sym);
                                    name->symbol = param_sym;
                                    inst_sym->parameters[param_idx].param_sym = param_sym;
                                    param_idx++;
                                }
                            }
                        }
                    }

                    /* Resolve body declarations (local variables) */
                    Resolve_Declaration_List(sm, &gen_body->subprogram_body.declarations);

                    /* Resolve body statements */
                    Resolve_Statement_List(sm, &gen_body->subprogram_body.statements);

                    Symbol_Manager_Pop_Scope(sm);
                }
            }
            break;

        case NK_REPRESENTATION_CLAUSE:
            /* Representation clause: FOR T'SIZE USE 32; or FOR T USE RECORD ... */
            {
                /* Resolve entity name */
                if (node->rep_clause.entity_name) {
                    Resolve_Expression(sm, node->rep_clause.entity_name);

                    /* Get target type or symbol */
                    Symbol *target_sym = NULL;
                    if (node->rep_clause.entity_name->kind == NK_IDENTIFIER) {
                        target_sym = Symbol_Find(sm, node->rep_clause.entity_name->string_val.text);
                    } else if (node->rep_clause.entity_name->symbol) {
                        target_sym = node->rep_clause.entity_name->symbol;
                    }

                    Type_Info *target_type = target_sym ? target_sym->type : NULL;

                    /* Process attribute clauses: FOR T'SIZE USE 32; */
                    if (node->rep_clause.attribute.data && target_type) {
                        String_Slice attr = node->rep_clause.attribute;

                        if (node->rep_clause.expression) {
                            Resolve_Expression(sm, node->rep_clause.expression);
                            int64_t value = 0;

                            /* Extract constant value */
                            if (node->rep_clause.expression->kind == NK_INTEGER) {
                                value = node->rep_clause.expression->integer_lit.value;
                            }

                            /* Apply representation */
                            if (Slice_Equal_Ignore_Case(attr, S("SIZE"))) {
                                /* Size in bits - convert to bytes */
                                target_type->size = (uint32_t)((value + 7) / 8);
                            } else if (Slice_Equal_Ignore_Case(attr, S("ALIGNMENT"))) {
                                target_type->alignment = (uint32_t)value;
                            } else if (Slice_Equal_Ignore_Case(attr, S("STORAGE_SIZE"))) {
                                /* For task types: storage for task stack */
                                /* Would store in target_sym or target_type */
                            } else if (Slice_Equal_Ignore_Case(attr, S("SMALL"))) {
                                /* For fixed-point: set small value */
                                if (target_type->kind == TYPE_FIXED &&
                                    node->rep_clause.expression->kind == NK_REAL) {
                                    target_type->fixed.small =
                                        node->rep_clause.expression->real_lit.value;
                                }
                            }
                        }
                    }

                    /* Process record representation: FOR T USE RECORD ... */
                    if (node->rep_clause.is_record_rep && target_type &&
                        target_type->kind == TYPE_RECORD) {
                        /* Process alignment clause */
                        if (node->rep_clause.expression) {
                            Resolve_Expression(sm, node->rep_clause.expression);
                            if (node->rep_clause.expression->kind == NK_INTEGER) {
                                target_type->alignment =
                                    (uint32_t)node->rep_clause.expression->integer_lit.value;
                            }
                        }

                        /* Process component clauses (record layout)
                         * Each clause: component_name AT byte_position [RANGE bits]; */
                        for (uint32_t i = 0; i < node->rep_clause.component_clauses.count; i++) {
                            Syntax_Node *cc = node->rep_clause.component_clauses.items[i];
                            if (cc->kind == NK_ASSOCIATION && cc->association.choices.count > 0) {
                                Syntax_Node *name_node = cc->association.choices.items[0];
                                if (name_node && name_node->kind == NK_IDENTIFIER) {
                                    String_Slice comp_name = name_node->string_val.text;
                                    /* Find matching component in record type */
                                    for (uint32_t j = 0; j < target_type->record.component_count; j++) {
                                        Component_Info *comp = &target_type->record.components[j];
                                        if (Slice_Equal_Ignore_Case(comp->name, comp_name)) {
                                            /* Get byte offset from expression */
                                            Syntax_Node *pos_expr = cc->association.expression;
                                            if (pos_expr && pos_expr->kind == NK_INTEGER) {
                                                comp->byte_offset = (uint32_t)pos_expr->integer_lit.value;
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    /* Process enumeration representation: FOR T USE (val0, val1, ...); */
                    if (node->rep_clause.is_enum_rep && target_type &&
                        target_type->kind == TYPE_ENUMERATION) {
                        /* Store internal representation values for enum literals
                         * The component_clauses list contains the values in order */
                        uint32_t val_count = node->rep_clause.component_clauses.count;
                        if (val_count > 0 && val_count <= target_type->enumeration.literal_count) {
                            /* Allocate array for representation values */
                            target_type->enumeration.rep_values =
                                Arena_Allocate(val_count * sizeof(int64_t));
                            for (uint32_t i = 0; i < val_count; i++) {
                                Syntax_Node *val_node = node->rep_clause.component_clauses.items[i];
                                if (val_node && val_node->kind == NK_INTEGER) {
                                    target_type->enumeration.rep_values[i] =
                                        val_node->integer_lit.value;
                                } else {
                                    /* Default to position value */
                                    target_type->enumeration.rep_values[i] = (int64_t)i;
                                }
                            }
                        }
                    }
                }
            }
            break;

        default:
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §12.4 Compilation Unit Resolution
 * ───────────────────────────────────────────────────────────────────────── */

/* Forward declarations for WITH clause loading */
static char *Lookup_Path(String_Slice name);
static void Load_Package_Spec(Symbol_Manager *sm, String_Slice name, char *src);

static void Resolve_Compilation_Unit(Symbol_Manager *sm, Syntax_Node *node) {
    if (!node) return;

    /* Load WITH'd packages from include paths */
    if (node->compilation_unit.context) {
        Syntax_Node *ctx = node->compilation_unit.context;
        for (uint32_t i = 0; i < ctx->context.with_clauses.count; i++) {
            Syntax_Node *with_node = ctx->context.with_clauses.items[i];
            /* WITH clause contains a list of package names */
            for (uint32_t j = 0; j < with_node->use_clause.names.count; j++) {
                Syntax_Node *pkg_name = with_node->use_clause.names.items[j];
                if (pkg_name->kind == NK_IDENTIFIER) {
                    char *pkg_src = Lookup_Path(pkg_name->string_val.text);
                    if (pkg_src) {
                        Load_Package_Spec(sm, pkg_name->string_val.text, pkg_src);
                    }
                    /* Resolve the identifier to the package symbol */
                    Resolve_Expression(sm, pkg_name);
                }
            }
        }
        /* Resolve USE clauses (make package contents visible) */
        for (uint32_t i = 0; i < ctx->context.use_clauses.count; i++) {
            Resolve_Declaration(sm, ctx->context.use_clauses.items[i]);
        }
    }

    /* Resolve main unit */
    if (node->compilation_unit.unit) {
        Resolve_Declaration(sm, node->compilation_unit.unit);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §13. LLVM IR CODE GENERATION
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Generate LLVM IR from the resolved AST. Key principles:
 *
 * 1. Widen to i64 for computation, truncate for storage (GNAT LLVM style)
 * 2. All pointer types use opaque 'ptr' (LLVM 15+)
 * 3. Static links for nested subprogram access
 * 4. Fat pointers for unconstrained arrays (ptr + bounds)
 */

/* ─────────────────────────────────────────────────────────────────────────
 * §13.1 Code Generator State
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct {
    FILE         *output;
    Symbol_Manager *sm;

    /* ID counters */
    uint32_t      temp_id;
    uint32_t      label_id;
    uint32_t      global_id;
    uint32_t      string_id;

    /* Current function context */
    Symbol       *current_function;
    uint32_t      current_nesting_level;

    /* Generic instance context for type substitution */
    Symbol       *current_instance;

    /* Loop/exit context */
    uint32_t      loop_exit_label;
    uint32_t      loop_continue_label;

    /* Function exit tracking */
    bool          has_return;

    /* Deferred nested subprogram bodies */
    Syntax_Node  *deferred_bodies[64];
    uint32_t      deferred_count;

    /* Static link support for nested functions */
    Symbol       *enclosing_function;    /* Function containing current nested function */
    bool          is_nested;              /* True if current function is nested */

    /* Exception handling support */
    uint32_t      exception_handler_label;  /* Label of current exception handler */
    uint32_t      exception_jmp_buf;        /* Current setjmp buffer temp */
    bool          in_exception_region;      /* True if inside exception-handled block */

    /* String constant buffer (emitted at module level) */
    char         *string_const_buffer;
    size_t        string_const_size;
    size_t        string_const_capacity;
} Code_Generator;

static Code_Generator *Code_Generator_New(FILE *output, Symbol_Manager *sm) {
    Code_Generator *cg = Arena_Allocate(sizeof(Code_Generator));
    cg->output = output;
    cg->sm = sm;
    cg->temp_id = 1;
    cg->label_id = 1;
    cg->global_id = 1;
    cg->string_id = 1;
    cg->deferred_count = 0;
    /* Initialize string constant buffer */
    cg->string_const_capacity = 4096;
    cg->string_const_buffer = Arena_Allocate(cg->string_const_capacity);
    cg->string_const_size = 0;
    return cg;
}

/* Emit to string constant buffer instead of main output */
static void Emit_String_Const(Code_Generator *cg, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char temp[1024];
    int len = vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    if (len < 0) return;  /* Format error */
    size_t slen = (size_t)len;

    /* Expand buffer if needed */
    while (cg->string_const_size + slen + 1 > cg->string_const_capacity) {
        size_t new_cap = cg->string_const_capacity * 2;
        char *new_buf = Arena_Allocate(new_cap);
        memcpy(new_buf, cg->string_const_buffer, cg->string_const_size);
        cg->string_const_buffer = new_buf;
        cg->string_const_capacity = new_cap;
    }
    memcpy(cg->string_const_buffer + cg->string_const_size, temp, slen);
    cg->string_const_size += slen;
    cg->string_const_buffer[cg->string_const_size] = '\0';
}

/* Emit a single char to string constant buffer */
static void Emit_String_Const_Char(Code_Generator *cg, char c) {
    if (cg->string_const_size + 2 > cg->string_const_capacity) {
        size_t new_cap = cg->string_const_capacity * 2;
        char *new_buf = Arena_Allocate(new_cap);
        memcpy(new_buf, cg->string_const_buffer, cg->string_const_size);
        cg->string_const_buffer = new_buf;
        cg->string_const_capacity = new_cap;
    }
    cg->string_const_buffer[cg->string_const_size++] = c;
    cg->string_const_buffer[cg->string_const_size] = '\0';
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.2 IR Emission Helpers
 * ───────────────────────────────────────────────────────────────────────── */

static uint32_t Emit_Temp(Code_Generator *cg) {
    return cg->temp_id++;
}

static uint32_t Emit_Label(Code_Generator *cg) {
    return cg->label_id++;
}

static void Emit(Code_Generator *cg, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(cg->output, format, args);
    va_end(args);
}

/* Encode symbol name for LLVM identifier */
static void Emit_Symbol_Name(Code_Generator *cg, Symbol *sym) {
    if (!sym) {
        Emit(cg, "unknown");
        return;
    }

    /* For imported symbols with external name, use that directly */
    if (sym->is_imported && sym->external_name.length > 0) {
        /* External name is a string literal, strip quotes if present */
        String_Slice name = sym->external_name;
        if (name.length >= 2 && name.data[0] == '"' && name.data[name.length-1] == '"') {
            name.data++;
            name.length -= 2;
        }
        for (uint32_t i = 0; i < name.length; i++) {
            fputc(name.data[i], cg->output);
        }
        return;
    }

    /* Build mangled name: parent_scope__name[__unique_id] */
    if (sym->parent) {
        Emit_Symbol_Name(cg, sym->parent);
        Emit(cg, "__");
    }

    /* Emit name, replacing non-alphanumeric */
    for (uint32_t i = 0; i < sym->name.length; i++) {
        char c = sym->name.data[i];
        if (Is_Alnum(c) || c == '_') {
            fputc(c, cg->output);
        } else if (c == '"') {
            Emit(cg, "_op_");
        } else {
            Emit(cg, "_%02x", (unsigned char)c);
        }
    }

    /* Only add unique_id suffix for local/nested symbols, not package-level.
     * Package-level symbols (parent is a package or no parent) use just the
     * qualified name for cross-compilation linking. Local symbols need the
     * unique_id to disambiguate nested scopes with same names. */
    bool is_package_level = !sym->parent || sym->parent->kind == SYMBOL_PACKAGE;
    if (!is_package_level) {
        Emit(cg, "_S%u", sym->unique_id);
    }
}

/* Emit type conversion if needed (sext/trunc for integers) */
static uint32_t Emit_Convert(Code_Generator *cg, uint32_t src, const char *src_type,
                            const char *dst_type) {
    if (strcmp(src_type, dst_type) == 0) return src;

    uint32_t t = Emit_Temp(cg);
    /* Determine source and dest bit widths */
    int src_bits = 64, dst_bits = 64;
    if (strcmp(src_type, "i32") == 0) src_bits = 32;
    else if (strcmp(src_type, "i16") == 0) src_bits = 16;
    else if (strcmp(src_type, "i8") == 0) src_bits = 8;
    if (strcmp(dst_type, "i32") == 0) dst_bits = 32;
    else if (strcmp(dst_type, "i16") == 0) dst_bits = 16;
    else if (strcmp(dst_type, "i8") == 0) dst_bits = 8;

    if (dst_bits > src_bits) {
        Emit(cg, "  %%t%u = sext %s %%t%u to %s\n", t, src_type, src, dst_type);
    } else if (dst_bits < src_bits) {
        Emit(cg, "  %%t%u = trunc %s %%t%u to %s\n", t, src_type, src, dst_type);
    } else {
        return src;  /* Same size, no conversion */
    }
    return t;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.2.1 Fat Pointer Support for Unconstrained Arrays
 *
 * Unconstrained arrays use a "fat pointer" representation:
 *   %fat_ptr = type { ptr, { i64, i64 } }
 *
 * Where:
 *   - Field 0: pointer to array data
 *   - Field 1: bounds struct { low_bound, high_bound }
 *
 * This allows passing arrays without knowing their bounds at compile time.
 * ───────────────────────────────────────────────────────────────────────── */

/* Fat pointer type name */
#define FAT_PTR_TYPE "{ ptr, { i64, i64 } }"

/* Create a fat pointer from data pointer and bounds
 * Returns the temp ID of the fat pointer struct */
static uint32_t Emit_Fat_Pointer(Code_Generator *cg, uint32_t data_ptr,
                                  int64_t low, int64_t high) {
    /* Allocate fat pointer struct on stack */
    uint32_t fat_alloca = Emit_Temp(cg);
    Emit(cg, "  %%t%u = alloca " FAT_PTR_TYPE "\n", fat_alloca);

    /* Store data pointer */
    uint32_t data_gep = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr " FAT_PTR_TYPE ", ptr %%t%u, i32 0, i32 0\n",
         data_gep, fat_alloca);
    Emit(cg, "  store ptr %%t%u, ptr %%t%u\n", data_ptr, data_gep);

    /* Store low bound */
    uint32_t low_gep = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr " FAT_PTR_TYPE ", ptr %%t%u, i32 0, i32 1, i32 0\n",
         low_gep, fat_alloca);
    Emit(cg, "  store i64 %lld, ptr %%t%u\n", (long long)low, low_gep);

    /* Store high bound */
    uint32_t high_gep = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr " FAT_PTR_TYPE ", ptr %%t%u, i32 0, i32 1, i32 1\n",
         high_gep, fat_alloca);
    Emit(cg, "  store i64 %lld, ptr %%t%u\n", (long long)high, high_gep);

    /* Load and return the fat pointer struct */
    uint32_t fat_val = Emit_Temp(cg);
    Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%t%u\n", fat_val, fat_alloca);
    return fat_val;
}

/* Extract data pointer from fat pointer */
static uint32_t Emit_Fat_Pointer_Data(Code_Generator *cg, uint32_t fat_ptr) {
    uint32_t t = Emit_Temp(cg);
    Emit(cg, "  %%t%u = extractvalue " FAT_PTR_TYPE " %%t%u, 0\n", t, fat_ptr);
    return t;
}

/* Extract low bound from fat pointer */
static uint32_t Emit_Fat_Pointer_Low(Code_Generator *cg, uint32_t fat_ptr) {
    uint32_t t = Emit_Temp(cg);
    Emit(cg, "  %%t%u = extractvalue " FAT_PTR_TYPE " %%t%u, 1, 0\n", t, fat_ptr);
    return t;
}

/* Extract high bound from fat pointer */
static uint32_t Emit_Fat_Pointer_High(Code_Generator *cg, uint32_t fat_ptr) {
    uint32_t t = Emit_Temp(cg);
    Emit(cg, "  %%t%u = extractvalue " FAT_PTR_TYPE " %%t%u, 1, 1\n", t, fat_ptr);
    return t;
}

/* Create a fat pointer from data pointer and dynamic bounds (temp IDs)
 * Returns the temp ID of the fat pointer struct */
static uint32_t Emit_Fat_Pointer_Dynamic(Code_Generator *cg, uint32_t data_ptr,
                                          uint32_t low_temp, uint32_t high_temp) {
    /* Allocate fat pointer struct on stack */
    uint32_t fat_alloca = Emit_Temp(cg);
    Emit(cg, "  %%t%u = alloca " FAT_PTR_TYPE "\n", fat_alloca);

    /* Store data pointer */
    uint32_t data_gep = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr " FAT_PTR_TYPE ", ptr %%t%u, i32 0, i32 0\n",
         data_gep, fat_alloca);
    Emit(cg, "  store ptr %%t%u, ptr %%t%u\n", data_ptr, data_gep);

    /* Store low bound */
    uint32_t low_gep = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr " FAT_PTR_TYPE ", ptr %%t%u, i32 0, i32 1, i32 0\n",
         low_gep, fat_alloca);
    Emit(cg, "  store i64 %%t%u, ptr %%t%u\n", low_temp, low_gep);

    /* Store high bound */
    uint32_t high_gep = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr " FAT_PTR_TYPE ", ptr %%t%u, i32 0, i32 1, i32 1\n",
         high_gep, fat_alloca);
    Emit(cg, "  store i64 %%t%u, ptr %%t%u\n", high_temp, high_gep);

    /* Load and return the fat pointer struct */
    uint32_t fat_val = Emit_Temp(cg);
    Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%t%u\n", fat_val, fat_alloca);
    return fat_val;
}

/* Compute length from fat pointer bounds: high - low + 1
 * Returns temp ID holding the i64 length */
static uint32_t Emit_Fat_Pointer_Length(Code_Generator *cg, uint32_t fat_ptr) {
    uint32_t low = Emit_Fat_Pointer_Low(cg, fat_ptr);
    uint32_t high = Emit_Fat_Pointer_High(cg, fat_ptr);
    uint32_t diff = Emit_Temp(cg);
    Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u\n", diff, high, low);
    uint32_t len = Emit_Temp(cg);
    Emit(cg, "  %%t%u = add i64 %%t%u, 1\n", len, diff);
    return len;
}

/* Copy data from fat pointer to a named destination
 * Emits: memcpy(dst, src_data, length) */
static void Emit_Fat_Pointer_Copy_To_Name(Code_Generator *cg, uint32_t fat_ptr, Symbol *dst) {
    uint32_t src_ptr = Emit_Fat_Pointer_Data(cg, fat_ptr);
    uint32_t len = Emit_Fat_Pointer_Length(cg, fat_ptr);
    Emit(cg, "  call void @llvm.memcpy.p0.p0.i64(ptr %%");
    Emit_Symbol_Name(cg, dst);
    Emit(cg, ", ptr %%t%u, i64 %%t%u, i1 false)\n", src_ptr, len);
}

/* Copy data from fat pointer to a temp pointer destination
 * Emits: memcpy(dst_ptr, src_data, length) */
__attribute__((unused))
static void Emit_Fat_Pointer_Copy_To_Ptr(Code_Generator *cg, uint32_t fat_ptr, uint32_t dst_ptr) {
    uint32_t src_ptr = Emit_Fat_Pointer_Data(cg, fat_ptr);
    uint32_t len = Emit_Fat_Pointer_Length(cg, fat_ptr);
    Emit(cg, "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%u, ptr %%t%u, i64 %%t%u, i1 false)\n",
         dst_ptr, src_ptr, len);
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.3 Expression Code Generation
 *
 * Returns the LLVM SSA value ID holding the expression result.
 * ───────────────────────────────────────────────────────────────────────── */

static uint32_t Generate_Expression(Code_Generator *cg, Syntax_Node *node);

static uint32_t Generate_Integer_Literal(Code_Generator *cg, Syntax_Node *node) {
    uint32_t t = Emit_Temp(cg);
    Emit(cg, "  %%t%u = add i64 0, %lld\n", t, (long long)node->integer_lit.value);
    return t;
}

static uint32_t Generate_Real_Literal(Code_Generator *cg, Syntax_Node *node) {
    uint32_t t = Emit_Temp(cg);
    Emit(cg, "  %%t%u = fadd double 0.0, %f\n", t, node->real_lit.value);
    return t;
}

static uint32_t Generate_String_Literal(Code_Generator *cg, Syntax_Node *node) {
    /* Allocate string constant */
    uint32_t str_id = cg->string_id++;
    uint32_t len = node->string_val.text.length;

    /* Generate global constant to buffer (without null terminator for Ada strings) */
    Emit_String_Const(cg, "@.str%u = private unnamed_addr constant [%u x i8] c\"", str_id, len);
    for (uint32_t i = 0; i < len; i++) {
        char c = node->string_val.text.data[i];
        if (c >= 32 && c < 127 && c != '"' && c != '\\') {
            Emit_String_Const_Char(cg, c);
        } else {
            Emit_String_Const(cg, "\\%02X", (unsigned char)c);
        }
    }
    Emit_String_Const(cg, "\"\n");

    /* Get pointer to string data */
    uint32_t data_ptr = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr [%u x i8], ptr @.str%u, i64 0, i64 0\n",
         data_ptr, len, str_id);

    /* Return fat pointer with Ada STRING bounds (1..length) */
    return Emit_Fat_Pointer(cg, data_ptr, 1, (int64_t)len);
}

static uint32_t Generate_Identifier(Code_Generator *cg, Syntax_Node *node) {
    Symbol *sym = node->symbol;
    if (!sym) {
        Report_Error(node->location, "unresolved identifier in codegen");
        return 0;
    }

    uint32_t t = Emit_Temp(cg);
    Type_Info *ty = sym->type;

    switch (sym->kind) {
        case SYMBOL_VARIABLE:
        case SYMBOL_PARAMETER: {
            /* Check for constrained string/array - return fat pointer */
            if (ty && ty->kind == TYPE_ARRAY && ty->array.is_constrained &&
                ty->array.element_type && ty->array.element_type->kind == TYPE_CHARACTER) {

                /* Get bounds from type */
                int64_t low = 1, high = 0;
                if (ty->array.index_count > 0) {
                    low = Type_Bound_Value(ty->array.indices[0].low_bound);
                    high = Type_Bound_Value(ty->array.indices[0].high_bound);
                }

                /* Get pointer to array data */
                uint32_t data_ptr = Emit_Temp(cg);
                Emit(cg, "  %%t%u = getelementptr i8, ptr %%", data_ptr);
                Emit_Symbol_Name(cg, sym);
                Emit(cg, ", i64 0\n");

                /* Create fat pointer with bounds */
                return Emit_Fat_Pointer(cg, data_ptr, low, high);
            }

            /* Check if this is an uplevel (outer scope) variable access
             * A variable is "uplevel" if its defining scope's owner is different
             * from the current function */
            Symbol *var_owner = sym->defining_scope ? sym->defining_scope->owner : NULL;
            bool is_uplevel = cg->current_function && var_owner &&
                              var_owner != cg->current_function;

            /* Check if package-level global (parent is NULL or SYMBOL_PACKAGE) */
            bool is_global = !sym->parent || sym->parent->kind == SYMBOL_PACKAGE;

            const char *type_str = Type_To_Llvm(ty);
            if (is_uplevel && cg->is_nested) {
                /* Uplevel access through frame pointer parameter
                 * The frame pointer points to the variable in the enclosing scope */
                Emit(cg, "  ; UPLEVEL ACCESS: %.*s via frame pointer\n",
                     (int)sym->name.length, sym->name.data);
                Emit(cg, "  %%t%u = load %s, ptr %%__frame.", t, type_str);
                Emit_Symbol_Name(cg, sym);
                Emit(cg, "\n");
            } else if (is_global) {
                /* Global variable - use @ prefix */
                Emit(cg, "  %%t%u = load %s, ptr @", t, type_str);
                Emit_Symbol_Name(cg, sym);
                Emit(cg, "\n");
            } else {
                Emit(cg, "  %%t%u = load %s, ptr %%", t, type_str);
                Emit_Symbol_Name(cg, sym);
                Emit(cg, "\n");
            }
            /* Widen to i64 for computation if narrower type */
            t = Emit_Convert(cg, t, type_str, "i64");
        } break;

        case SYMBOL_CONSTANT:
        case SYMBOL_LITERAL:
            /* Enumeration literal or constant */
            if (sym->type && sym->type->kind == TYPE_ENUMERATION) {
                /* Find position in enumeration */
                int64_t pos = 0;
                for (uint32_t i = 0; i < sym->type->enumeration.literal_count; i++) {
                    if (Slice_Equal_Ignore_Case(sym->type->enumeration.literals[i], sym->name)) {
                        pos = i;
                        break;
                    }
                }
                Emit(cg, "  %%t%u = add i64 0, %lld\n", t, (long long)pos);
            } else {
                Emit(cg, "  %%t%u = add i64 0, 0  ; constant\n", t);
            }
            break;

        default:
            Emit(cg, "  %%t%u = add i64 0, 0  ; unhandled symbol kind\n", t);
    }

    return t;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.3.1 Implicit Operators for Composite Types
 *
 * Ada requires equality operators for all non-limited types. For composite
 * types (records, arrays), equality is defined component-wise.
 * ───────────────────────────────────────────────────────────────────────── */

/* Generate equality comparison for record types (component-by-component) */
static uint32_t Generate_Record_Equality(Code_Generator *cg, uint32_t left_ptr,
                                         uint32_t right_ptr, Type_Info *record_type) {
    if (!record_type || record_type->kind != TYPE_RECORD ||
        record_type->record.component_count == 0) {
        /* Empty record or invalid - always equal */
        uint32_t t = Emit_Temp(cg);
        Emit(cg, "  %%t%u = add i1 0, 1  ; empty record equality\n", t);
        return t;
    }

    uint32_t result = 0;
    for (uint32_t i = 0; i < record_type->record.component_count; i++) {
        Component_Info *comp = &record_type->record.components[i];
        const char *comp_llvm_type = Type_To_Llvm(comp->component_type);

        /* Get pointers to components */
        uint32_t left_gep = Emit_Temp(cg);
        uint32_t right_gep = Emit_Temp(cg);
        Emit(cg, "  %%t%u = getelementptr i8, ptr %%t%u, i64 %u\n",
             left_gep, left_ptr, comp->byte_offset);
        Emit(cg, "  %%t%u = getelementptr i8, ptr %%t%u, i64 %u\n",
             right_gep, right_ptr, comp->byte_offset);

        /* Load component values */
        uint32_t left_val = Emit_Temp(cg);
        uint32_t right_val = Emit_Temp(cg);
        Emit(cg, "  %%t%u = load %s, ptr %%t%u\n", left_val, comp_llvm_type, left_gep);
        Emit(cg, "  %%t%u = load %s, ptr %%t%u\n", right_val, comp_llvm_type, right_gep);

        /* Compare */
        uint32_t cmp = Emit_Temp(cg);
        if (Type_Is_Real(comp->component_type)) {
            Emit(cg, "  %%t%u = fcmp oeq %s %%t%u, %%t%u\n",
                 cmp, comp_llvm_type, left_val, right_val);
        } else {
            Emit(cg, "  %%t%u = icmp eq %s %%t%u, %%t%u\n",
                 cmp, comp_llvm_type, left_val, right_val);
        }

        /* AND with previous results */
        if (i == 0) {
            result = cmp;
        } else {
            uint32_t and_result = Emit_Temp(cg);
            Emit(cg, "  %%t%u = and i1 %%t%u, %%t%u\n", and_result, result, cmp);
            result = and_result;
        }
    }

    return result;
}

/* Generate equality comparison for constrained array types (element-by-element) */
static uint32_t Generate_Array_Equality(Code_Generator *cg, uint32_t left_ptr,
                                        uint32_t right_ptr, Type_Info *array_type) {
    if (!array_type || (array_type->kind != TYPE_ARRAY && array_type->kind != TYPE_STRING)) {
        uint32_t t = Emit_Temp(cg);
        Emit(cg, "  %%t%u = add i1 0, 1  ; invalid array equality\n", t);
        return t;
    }

    /* For constrained arrays, use memcmp */
    if (array_type->array.is_constrained) {
        int64_t count = Array_Element_Count(array_type);
        uint32_t elem_size = array_type->array.element_type ?
                             array_type->array.element_type->size : 4;
        int64_t total_size = count * elem_size;

        /* Call llvm.memcmp or compare byte-by-byte */
        uint32_t result = Emit_Temp(cg);
        uint32_t cmp_result = Emit_Temp(cg);

        /* Use LLVM's memcmp intrinsic equivalent */
        Emit(cg, "  %%t%u = call i32 @memcmp(ptr %%t%u, ptr %%t%u, i64 %lld)\n",
             result, left_ptr, right_ptr, (long long)total_size);
        Emit(cg, "  %%t%u = icmp eq i32 %%t%u, 0\n", cmp_result, result);
        return cmp_result;
    }

    /*
     * Unconstrained array equality (per RM 4.5.2):
     * Two arrays are equal iff they have the same length and matching components.
     * Bounds themselves need not match—only length and content.
     *
     * For fat pointers: compare lengths, then data if lengths match.
     * Use select instead of phi to avoid block label complications.
     */

    /* Extract bounds from fat pointer structures */
    uint32_t left_low = Emit_Fat_Pointer_Low(cg, left_ptr);
    uint32_t left_high = Emit_Fat_Pointer_High(cg, left_ptr);
    uint32_t right_low = Emit_Fat_Pointer_Low(cg, right_ptr);
    uint32_t right_high = Emit_Fat_Pointer_High(cg, right_ptr);

    /* Compute lengths: high - low + 1 */
    uint32_t left_len = Emit_Temp(cg);
    Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u\n", left_len, left_high, left_low);
    uint32_t left_len1 = Emit_Temp(cg);
    Emit(cg, "  %%t%u = add i64 %%t%u, 1\n", left_len1, left_len);

    uint32_t right_len = Emit_Temp(cg);
    Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u\n", right_len, right_high, right_low);
    uint32_t right_len1 = Emit_Temp(cg);
    Emit(cg, "  %%t%u = add i64 %%t%u, 1\n", right_len1, right_len);

    /* Compare lengths */
    uint32_t len_eq = Emit_Temp(cg);
    Emit(cg, "  %%t%u = icmp eq i64 %%t%u, %%t%u\n", len_eq, left_len1, right_len1);

    /* Extract data pointers */
    uint32_t left_data = Emit_Fat_Pointer_Data(cg, left_ptr);
    uint32_t right_data = Emit_Fat_Pointer_Data(cg, right_ptr);

    /* Compute byte size for memcmp */
    uint32_t elem_size = array_type->array.element_type ?
                         array_type->array.element_type->size : 1;
    uint32_t byte_size = Emit_Temp(cg);
    Emit(cg, "  %%t%u = mul i64 %%t%u, %u\n", byte_size, left_len1, elem_size);

    /* Call memcmp */
    uint32_t memcmp_result = Emit_Temp(cg);
    Emit(cg, "  %%t%u = call i32 @memcmp(ptr %%t%u, ptr %%t%u, i64 %%t%u)\n",
         memcmp_result, left_data, right_data, byte_size);
    uint32_t data_eq = Emit_Temp(cg);
    Emit(cg, "  %%t%u = icmp eq i32 %%t%u, 0\n", data_eq, memcmp_result);

    /* Result: lengths match AND data matches */
    uint32_t result = Emit_Temp(cg);
    Emit(cg, "  %%t%u = and i1 %%t%u, %%t%u\n", result, len_eq, data_eq);

    return result;
}

/* Generate the address of a composite type expression (for equality comparison) */
static uint32_t Generate_Composite_Address(Code_Generator *cg, Syntax_Node *node) {
    if (node->kind == NK_IDENTIFIER) {
        Symbol *sym = node->symbol;
        if (sym) {
            /* For composite variables, the symbol's address IS the pointer to the data */
            uint32_t t = Emit_Temp(cg);
            Emit(cg, "  %%t%u = getelementptr i8, ptr %%", t);
            Emit_Symbol_Name(cg, sym);
            Emit(cg, ", i64 0\n");
            return t;
        }
    }
    /* Fallback: generate as expression (may be incorrect for some cases) */
    return Generate_Expression(cg, node);
}

static uint32_t Generate_Binary_Op(Code_Generator *cg, Syntax_Node *node) {
    /* Check if this is equality/inequality on composite types */
    Type_Info *left_type = node->binary.left ? node->binary.left->type : NULL;

    if ((node->binary.op == TK_EQ || node->binary.op == TK_NE) &&
        left_type && Type_Is_Composite(left_type)) {
        /* Composite type comparison - get addresses, not loaded values */
        uint32_t left_ptr = Generate_Composite_Address(cg, node->binary.left);
        uint32_t right_ptr = Generate_Composite_Address(cg, node->binary.right);
        uint32_t eq_result = Emit_Temp(cg);

        if (left_type->equality_func_name) {
            /* Call the generated equality function */
            Emit(cg, "  %%t%u = call i1 @%s(ptr %%t%u, ptr %%t%u)\n",
                 eq_result, left_type->equality_func_name, left_ptr, right_ptr);
        } else {
            /* Fallback: inline comparison (type wasn't frozen properly) */
            if (left_type->kind == TYPE_RECORD) {
                eq_result = Generate_Record_Equality(cg, left_ptr, right_ptr, left_type);
            } else {
                eq_result = Generate_Array_Equality(cg, left_ptr, right_ptr, left_type);
            }
        }

        /* For /= operator, negate the result */
        if (node->binary.op == TK_NE) {
            uint32_t ne_result = Emit_Temp(cg);
            Emit(cg, "  %%t%u = xor i1 %%t%u, 1\n", ne_result, eq_result);
            return ne_result;
        }
        return eq_result;
    }

    /* String/array concatenation */
    if (node->binary.op == TK_AMPERSAND &&
        left_type && (left_type->kind == TYPE_STRING || left_type->kind == TYPE_ARRAY)) {

        /* Generate both operands - they return fat pointers */
        uint32_t left_fat = Generate_Expression(cg, node->binary.left);
        uint32_t right_fat = Generate_Expression(cg, node->binary.right);

        /* Extract data pointers and bounds */
        uint32_t left_data = Emit_Fat_Pointer_Data(cg, left_fat);
        uint32_t left_low = Emit_Fat_Pointer_Low(cg, left_fat);
        uint32_t left_high = Emit_Fat_Pointer_High(cg, left_fat);

        uint32_t right_data = Emit_Fat_Pointer_Data(cg, right_fat);
        uint32_t right_low = Emit_Fat_Pointer_Low(cg, right_fat);
        uint32_t right_high = Emit_Fat_Pointer_High(cg, right_fat);

        /* Calculate lengths: high - low + 1 */
        uint32_t left_len = Emit_Temp(cg);
        Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u\n", left_len, left_high, left_low);
        uint32_t left_len1 = Emit_Temp(cg);
        Emit(cg, "  %%t%u = add i64 %%t%u, 1\n", left_len1, left_len);

        uint32_t right_len = Emit_Temp(cg);
        Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u\n", right_len, right_high, right_low);
        uint32_t right_len1 = Emit_Temp(cg);
        Emit(cg, "  %%t%u = add i64 %%t%u, 1\n", right_len1, right_len);

        /* Total length */
        uint32_t total_len = Emit_Temp(cg);
        Emit(cg, "  %%t%u = add i64 %%t%u, %%t%u\n", total_len, left_len1, right_len1);

        /* Allocate space on secondary stack */
        uint32_t result_data = Emit_Temp(cg);
        Emit(cg, "  %%t%u = call ptr @__ada_sec_stack_alloc(i64 %%t%u)\n",
             result_data, total_len);

        /* Copy left string using llvm.memcpy */
        Emit(cg, "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%u, ptr %%t%u, i64 %%t%u, i1 false)\n",
             result_data, left_data, left_len1);

        /* Calculate destination for right string */
        uint32_t right_dest = Emit_Temp(cg);
        Emit(cg, "  %%t%u = getelementptr i8, ptr %%t%u, i64 %%t%u\n",
             right_dest, result_data, left_len1);

        /* Copy right string */
        Emit(cg, "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%u, ptr %%t%u, i64 %%t%u, i1 false)\n",
             right_dest, right_data, right_len1);

        /* Result bounds: 1..total_len (Ada STRING convention) */
        uint32_t one = Emit_Temp(cg);
        Emit(cg, "  %%t%u = add i64 0, 1\n", one);

        /* Return fat pointer to result */
        return Emit_Fat_Pointer_Dynamic(cg, result_data, one, total_len);
    }

    uint32_t left = Generate_Expression(cg, node->binary.left);
    uint32_t right = Generate_Expression(cg, node->binary.right);
    uint32_t t = Emit_Temp(cg);

    const char *op;
    Type_Info *result_type = node->type;
    bool is_float = result_type && (result_type->kind == TYPE_FLOAT ||
                                     result_type->kind == TYPE_UNIVERSAL_REAL);
    bool is_fixed = result_type && result_type->kind == TYPE_FIXED;

    /* Fixed-point multiplication/division needs scaling (RM 4.5.5) */
    if (is_fixed && (node->binary.op == TK_STAR || node->binary.op == TK_SLASH)) {
        int scale = result_type->fixed.scale;
        if (node->binary.op == TK_STAR) {
            /* Fixed * Fixed: result = (a * b) >> abs(scale) */
            uint32_t mul = Emit_Temp(cg);
            Emit(cg, "  %%t%u = mul i64 %%t%u, %%t%u\n", mul, left, right);
            if (scale < 0) {
                /* Negative scale = right shift by |scale| */
                Emit(cg, "  %%t%u = ashr i64 %%t%u, %d\n", t, mul, -scale);
            } else if (scale > 0) {
                /* Positive scale = left shift (uncommon) */
                Emit(cg, "  %%t%u = shl i64 %%t%u, %d\n", t, mul, scale);
            } else {
                t = mul;
            }
            return t;
        } else {
            /* Fixed / Fixed: result = (a << abs(scale)) / b */
            uint32_t shifted = Emit_Temp(cg);
            if (scale < 0) {
                Emit(cg, "  %%t%u = shl i64 %%t%u, %d\n", shifted, left, -scale);
            } else if (scale > 0) {
                Emit(cg, "  %%t%u = ashr i64 %%t%u, %d\n", shifted, left, scale);
            } else {
                shifted = left;
            }
            Emit(cg, "  %%t%u = sdiv i64 %%t%u, %%t%u\n", t, shifted, right);
            return t;
        }
    }

    switch (node->binary.op) {
        case TK_PLUS:  op = is_float ? "fadd" : "add"; break;
        case TK_MINUS: op = is_float ? "fsub" : "sub"; break;
        case TK_STAR:  op = is_float ? "fmul" : "mul"; break;
        case TK_SLASH: op = is_float ? "fdiv" : "sdiv"; break;
        case TK_MOD:   op = "srem"; break;
        case TK_REM:   op = "srem"; break;

        case TK_AND:
        case TK_AND_THEN:
            /* Boolean AND: operands should be i1 (from comparisons) */
            Emit(cg, "  %%t%u = and i1 %%t%u, %%t%u\n", t, left, right);
            return t;
        case TK_OR:
        case TK_OR_ELSE:
            /* Boolean OR: operands should be i1 */
            Emit(cg, "  %%t%u = or i1 %%t%u, %%t%u\n", t, left, right);
            return t;
        case TK_XOR:
            /* Boolean XOR: operands should be i1 */
            Emit(cg, "  %%t%u = xor i1 %%t%u, %%t%u\n", t, left, right);
            return t;

        case TK_EQ:  Emit(cg, "  %%t%u = icmp eq i64 %%t%u, %%t%u\n", t, left, right); return t;
        case TK_NE:  Emit(cg, "  %%t%u = icmp ne i64 %%t%u, %%t%u\n", t, left, right); return t;
        case TK_LT:  Emit(cg, "  %%t%u = icmp slt i64 %%t%u, %%t%u\n", t, left, right); return t;
        case TK_LE:  Emit(cg, "  %%t%u = icmp sle i64 %%t%u, %%t%u\n", t, left, right); return t;
        case TK_GT:  Emit(cg, "  %%t%u = icmp sgt i64 %%t%u, %%t%u\n", t, left, right); return t;
        case TK_GE:  Emit(cg, "  %%t%u = icmp sge i64 %%t%u, %%t%u\n", t, left, right); return t;

        default: op = "add"; break;
    }

    Emit(cg, "  %%t%u = %s %s %%t%u, %%t%u\n", t, op,
         is_float ? "double" : "i64", left, right);
    return t;
}

static uint32_t Generate_Unary_Op(Code_Generator *cg, Syntax_Node *node) {
    uint32_t operand = Generate_Expression(cg, node->unary.operand);
    uint32_t t = Emit_Temp(cg);

    switch (node->unary.op) {
        case TK_MINUS:
            Emit(cg, "  %%t%u = sub i64 0, %%t%u\n", t, operand);
            break;
        case TK_PLUS:
            return operand;
        case TK_NOT:
            Emit(cg, "  %%t%u = xor i1 %%t%u, 1\n", t, operand);
            break;
        case TK_ABS:
            {
                uint32_t neg = Emit_Temp(cg);
                uint32_t cmp = Emit_Temp(cg);
                Emit(cg, "  %%t%u = sub i64 0, %%t%u\n", neg, operand);
                Emit(cg, "  %%t%u = icmp slt i64 %%t%u, 0\n", cmp, operand);
                Emit(cg, "  %%t%u = select i1 %%t%u, i64 %%t%u, i64 %%t%u\n",
                     t, cmp, neg, operand);
            }
            break;
        default:
            return operand;
    }

    return t;
}

static uint32_t Generate_Apply(Code_Generator *cg, Syntax_Node *node) {
    Symbol *sym = node->apply.prefix->symbol;

    if (sym && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_PROCEDURE)) {
        /* Function call - generate arguments
         * For OUT/IN OUT parameters, we need to pass the ADDRESS, not the value */
        uint32_t *args = Arena_Allocate(node->apply.arguments.count * sizeof(uint32_t));
        bool *is_byref = Arena_Allocate(node->apply.arguments.count * sizeof(bool));

        for (uint32_t i = 0; i < node->apply.arguments.count; i++) {
            bool byref = i < sym->parameter_count &&
                         Param_Is_By_Reference(sym->parameters[i].mode);
            is_byref[i] = byref;

            if (byref) {
                /* For OUT/IN OUT: pass the address of the variable
                 * The argument must be an lvalue (identifier, selected, indexed) */
                Syntax_Node *arg = node->apply.arguments.items[i];
                if (arg->kind == NK_IDENTIFIER && arg->symbol) {
                    /* Simple variable - emit address */
                    args[i] = Emit_Temp(cg);
                    Emit(cg, "  %%t%u = getelementptr i8, ptr %%", args[i]);
                    Emit_Symbol_Name(cg, arg->symbol);
                    Emit(cg, ", i64 0  ; address for OUT/IN OUT\n");
                } else {
                    /* Complex lvalue - generate address (fallback) */
                    args[i] = Generate_Composite_Address(cg, arg);
                }
            } else {
                args[i] = Generate_Expression(cg, node->apply.arguments.items[i]);
                /* Truncate to actual parameter type */
                if (i < sym->parameter_count && sym->parameters[i].param_type) {
                    const char *param_type = Type_To_Llvm(sym->parameters[i].param_type);
                    args[i] = Emit_Convert(cg, args[i], "i64", param_type);
                }
            }
        }

        /* Check if calling a nested function of current scope */
        bool callee_is_nested = sym->parent &&
            (sym->parent->kind == SYMBOL_FUNCTION ||
             sym->parent->kind == SYMBOL_PROCEDURE);

        uint32_t t = Emit_Temp(cg);

        if (sym->return_type) {
            Emit(cg, "  %%t%u = call %s @", t, Type_To_Llvm(sym->return_type));
        } else {
            Emit(cg, "  call void @");
        }

        Emit_Symbol_Name(cg, sym);
        Emit(cg, "(");

        /* If calling nested function from its parent, pass frame pointer first */
        if (callee_is_nested && cg->current_function == sym->parent) {
            Emit(cg, "ptr %%__frame_base");
            if (node->apply.arguments.count > 0) Emit(cg, ", ");
        }

        for (uint32_t i = 0; i < node->apply.arguments.count; i++) {
            if (i > 0) Emit(cg, ", ");
            if (is_byref[i]) {
                /* OUT/IN OUT: pass as pointer */
                Emit(cg, "ptr %%t%u", args[i]);
            } else {
                const char *param_type = (i < sym->parameter_count && sym->parameters[i].param_type)
                    ? Type_To_Llvm(sym->parameters[i].param_type) : "i64";
                Emit(cg, "%s %%t%u", param_type, args[i]);
            }
        }

        Emit(cg, ")\n");

        /* Widen return value to i64 for computation */
        if (sym->return_type) {
            t = Emit_Convert(cg, t, Type_To_Llvm(sym->return_type), "i64");
            return t;
        }
        return 0;
    }

    /* Array indexing */
    Type_Info *prefix_type = node->apply.prefix->type;
    if (prefix_type && (prefix_type->kind == TYPE_ARRAY || prefix_type->kind == TYPE_STRING)) {
        Symbol *array_sym = node->apply.prefix->symbol;
        uint32_t base;
        uint32_t low_bound_val = 0;
        bool has_dynamic_low = false;

        /* Check if unconstrained array needing fat pointer handling */
        if (Type_Is_Unconstrained_Array(prefix_type) && array_sym &&
            (array_sym->kind == SYMBOL_PARAMETER || array_sym->kind == SYMBOL_VARIABLE)) {
            /* Load fat pointer and extract data pointer and low bound */
            uint32_t fat = Emit_Temp(cg);
            Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%", fat);
            Emit_Symbol_Name(cg, array_sym);
            Emit(cg, "  ; load fat pointer for indexing\n");
            base = Emit_Fat_Pointer_Data(cg, fat);
            low_bound_val = Emit_Fat_Pointer_Low(cg, fat);
            has_dynamic_low = true;
        } else if (array_sym) {
            /* Constrained array - get direct pointer to data */
            base = Emit_Temp(cg);
            Emit(cg, "  %%t%u = getelementptr i8, ptr %%", base);
            Emit_Symbol_Name(cg, array_sym);
            Emit(cg, ", i64 0\n");
        } else {
            /* Fallback for complex expressions - shouldn't happen often */
            base = Generate_Expression(cg, node->apply.prefix);
        }

        /* Generate index expression */
        uint32_t idx = Generate_Expression(cg, node->apply.arguments.items[0]);

        /* Adjust for array low bound (Ada arrays can start at any index) */
        if (has_dynamic_low) {
            /* Dynamic low bound from fat pointer */
            uint32_t adj = Emit_Temp(cg);
            Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u  ; adjust for dynamic low bound\n",
                 adj, idx, low_bound_val);
            idx = adj;
        } else {
            int64_t low_bound = Array_Low_Bound(prefix_type);
            if (low_bound != 0) {
                uint32_t adj = Emit_Temp(cg);
                Emit(cg, "  %%t%u = sub i64 %%t%u, %lld\n", adj, idx, (long long)low_bound);
                idx = adj;
            }
        }

        /* Get pointer to element and load */
        const char *elem_type = Type_To_Llvm(prefix_type->array.element_type);
        uint32_t ptr = Emit_Temp(cg);
        uint32_t t = Emit_Temp(cg);

        Emit(cg, "  %%t%u = getelementptr %s, ptr %%t%u, i64 %%t%u\n",
             ptr, elem_type, base, idx);
        Emit(cg, "  %%t%u = load %s, ptr %%t%u\n", t, elem_type, ptr);

        /* Widen to i64 for computation */
        t = Emit_Convert(cg, t, elem_type, "i64");
        return t;
    }

    return 0;
}

static uint32_t Generate_Selected(Code_Generator *cg, Syntax_Node *node) {
    /* Generate code for A.B (selected component) */
    Type_Info *prefix_type = node->selected.prefix->type;

    if (!prefix_type || prefix_type->kind != TYPE_RECORD) {
        /* Package-qualified name - just generate using the resolved symbol */
        Symbol *sym = node->symbol;
        if (sym) {
            uint32_t t = Emit_Temp(cg);
            Emit(cg, "  %%t%u = load i64, ptr %%", t);
            Emit_Symbol_Name(cg, sym);
            Emit(cg, "\n");
            return t;
        }
        return 0;
    }

    /* Record field access - find component by name */
    uint32_t byte_offset = 0;
    Type_Info *field_type = NULL;
    for (uint32_t i = 0; i < prefix_type->record.component_count; i++) {
        if (Slice_Equal_Ignore_Case(
                prefix_type->record.components[i].name, node->selected.selector)) {
            byte_offset = prefix_type->record.components[i].byte_offset;
            field_type = prefix_type->record.components[i].component_type;
            break;
        }
    }

    /* Get base address of record variable */
    Symbol *record_sym = node->selected.prefix->symbol;
    if (!record_sym) {
        /* Handle nested selection (e.g., A.B.C) by generating prefix expression */
        uint32_t base_ptr = Generate_Expression(cg, node->selected.prefix);
        uint32_t ptr = Emit_Temp(cg);
        uint32_t t = Emit_Temp(cg);
        Emit(cg, "  %%t%u = getelementptr i8, ptr %%t%u, i64 %u\n",
             ptr, base_ptr, byte_offset);
        Emit(cg, "  %%t%u = load %s, ptr %%t%u\n",
             t, Type_To_Llvm(field_type), ptr);
        return t;
    }

    /* Calculate address of field */
    uint32_t ptr = Emit_Temp(cg);
    uint32_t t = Emit_Temp(cg);
    Emit(cg, "  %%t%u = getelementptr i8, ptr %%", ptr);
    Emit_Symbol_Name(cg, record_sym);
    Emit(cg, ", i64 %u\n", byte_offset);
    Emit(cg, "  %%t%u = load %s, ptr %%t%u\n",
         t, Type_To_Llvm(field_type), ptr);

    return t;
}

static int64_t Type_Bound_Value(Type_Bound b) {
    if (b.kind == BOUND_INTEGER) return b.int_value;
    return 0;  /* Handle other bound kinds as needed */
}

/* Get array element count for constrained arrays, 0 for unconstrained */
static int64_t Array_Element_Count(Type_Info *t) {
    if (!t || t->kind != TYPE_ARRAY || !t->array.is_constrained)
        return 0;
    if (t->array.index_count == 0)
        return 0;
    int64_t low = Type_Bound_Value(t->array.indices[0].low_bound);
    int64_t high = Type_Bound_Value(t->array.indices[0].high_bound);
    return high - low + 1;
}

/* Get array low bound for index adjustment */
static int64_t Array_Low_Bound(Type_Info *t) {
    if (!t || t->kind != TYPE_ARRAY || t->array.index_count == 0)
        return 0;
    return Type_Bound_Value(t->array.indices[0].low_bound);
}

/* Get dimension index from attribute argument (1-based, default 1) */
static uint32_t Get_Dimension_Index(Syntax_Node *arg) {
    if (!arg) return 0;  /* Default to first dimension */
    if (arg->kind == NK_INTEGER) return (uint32_t)(arg->integer_lit.value - 1);
    return 0;
}

static uint32_t Generate_Attribute(Code_Generator *cg, Syntax_Node *node) {
    /* Generate code for X'FIRST, X'LAST, X'LENGTH, X'SIZE, X'ADDRESS, etc. */
    Type_Info *prefix_type = node->attribute.prefix->type;
    String_Slice attr = node->attribute.name;
    uint32_t t = Emit_Temp(cg);
    uint32_t dim = Get_Dimension_Index(node->attribute.argument);

    /* ─────────────────────────────────────────────────────────────────────
     * Check if prefix is an unconstrained array that needs runtime bounds
     * Unconstrained arrays (STRING, unconstrained ARRAY) are passed as
     * fat pointers { ptr, { i64, i64 } } containing data pointer and bounds.
     * ───────────────────────────────────────────────────────────────────── */

    bool needs_runtime_bounds = false;
    Symbol *prefix_sym = node->attribute.prefix->symbol;
    if (prefix_type && Type_Is_Unconstrained_Array(prefix_type) &&
        prefix_sym && (prefix_sym->kind == SYMBOL_PARAMETER ||
                       prefix_sym->kind == SYMBOL_VARIABLE)) {
        needs_runtime_bounds = true;
    }

    /* ─────────────────────────────────────────────────────────────────────
     * Array/Scalar Bound Attributes
     * ───────────────────────────────────────────────────────────────────── */

    if (Slice_Equal_Ignore_Case(attr, S("FIRST"))) {
        if (prefix_type && (prefix_type->kind == TYPE_ARRAY || prefix_type->kind == TYPE_STRING)) {
            if (needs_runtime_bounds && dim == 0) {
                /* Load fat pointer and extract low bound */
                uint32_t fat = Emit_Temp(cg);
                Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%", fat);
                Emit_Symbol_Name(cg, prefix_sym);
                Emit(cg, "  ; load fat pointer for 'FIRST\n");
                uint32_t low = Emit_Fat_Pointer_Low(cg, fat);
                return low;
            } else if (dim < prefix_type->array.index_count) {
                Emit(cg, "  %%t%u = add i64 0, %lld  ; %.*s'FIRST(%u)\n", t,
                     (long long)Type_Bound_Value(prefix_type->array.indices[dim].low_bound),
                     (int)attr.length, attr.data, dim + 1);
            }
        } else if (prefix_type) {
            Emit(cg, "  %%t%u = add i64 0, %lld  ; %.*s'FIRST\n", t,
                 (long long)Type_Bound_Value(prefix_type->low_bound),
                 (int)attr.length, attr.data);
        }
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("LAST"))) {
        if (prefix_type && (prefix_type->kind == TYPE_ARRAY || prefix_type->kind == TYPE_STRING)) {
            if (needs_runtime_bounds && dim == 0) {
                /* Load fat pointer and extract high bound */
                uint32_t fat = Emit_Temp(cg);
                Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%", fat);
                Emit_Symbol_Name(cg, prefix_sym);
                Emit(cg, "  ; load fat pointer for 'LAST\n");
                uint32_t high = Emit_Fat_Pointer_High(cg, fat);
                return high;
            } else if (dim < prefix_type->array.index_count) {
                Emit(cg, "  %%t%u = add i64 0, %lld  ; %.*s'LAST(%u)\n", t,
                     (long long)Type_Bound_Value(prefix_type->array.indices[dim].high_bound),
                     (int)attr.length, attr.data, dim + 1);
            }
        } else if (prefix_type) {
            Emit(cg, "  %%t%u = add i64 0, %lld  ; %.*s'LAST\n", t,
                 (long long)Type_Bound_Value(prefix_type->high_bound),
                 (int)attr.length, attr.data);
        }
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("LENGTH"))) {
        if (prefix_type && (prefix_type->kind == TYPE_ARRAY || prefix_type->kind == TYPE_STRING)) {
            if (needs_runtime_bounds && dim == 0) {
                /* Load fat pointer and compute length from bounds */
                uint32_t fat = Emit_Temp(cg);
                Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%", fat);
                Emit_Symbol_Name(cg, prefix_sym);
                Emit(cg, "  ; load fat pointer for 'LENGTH\n");
                uint32_t low = Emit_Fat_Pointer_Low(cg, fat);
                uint32_t high = Emit_Fat_Pointer_High(cg, fat);
                uint32_t diff = Emit_Temp(cg);
                Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u\n", diff, high, low);
                uint32_t len = Emit_Temp(cg);
                Emit(cg, "  %%t%u = add i64 %%t%u, 1  ; 'LENGTH\n", len, diff);
                return len;
            } else if (dim < prefix_type->array.index_count) {
                int64_t low = Type_Bound_Value(prefix_type->array.indices[dim].low_bound);
                int64_t high = Type_Bound_Value(prefix_type->array.indices[dim].high_bound);
                Emit(cg, "  %%t%u = add i64 0, %lld  ; 'LENGTH(%u)\n", t,
                     (long long)(high - low + 1), dim + 1);
            }
        }
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("RANGE"))) {
        /* Range attribute - typically used in for loops
         * For unconstrained arrays, this is handled specially in Generate_For_Loop
         * Here we just return the low bound for general expression contexts */
        if (prefix_type && (prefix_type->kind == TYPE_ARRAY || prefix_type->kind == TYPE_STRING)) {
            if (needs_runtime_bounds && dim == 0) {
                /* Load fat pointer and extract low bound */
                uint32_t fat = Emit_Temp(cg);
                Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%", fat);
                Emit_Symbol_Name(cg, prefix_sym);
                Emit(cg, "  ; load fat pointer for 'RANGE\n");
                uint32_t low = Emit_Fat_Pointer_Low(cg, fat);
                return low;
            } else if (dim < prefix_type->array.index_count) {
                Emit(cg, "  %%t%u = add i64 0, %lld  ; 'RANGE(%u) low\n", t,
                     (long long)Type_Bound_Value(prefix_type->array.indices[dim].low_bound),
                     dim + 1);
            }
        }
        return t;
    }

    /* ─────────────────────────────────────────────────────────────────────
     * Size and Representation Attributes
     * ───────────────────────────────────────────────────────────────────── */

    if (Slice_Equal_Ignore_Case(attr, S("SIZE"))) {
        /* 'SIZE returns size in bits */
        Emit(cg, "  %%t%u = add i64 0, %lld  ; 'SIZE in bits\n", t,
             (long long)(prefix_type ? prefix_type->size * 8 : 0));
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("ALIGNMENT"))) {
        Emit(cg, "  %%t%u = add i64 0, %lld  ; 'ALIGNMENT\n", t,
             (long long)(prefix_type ? prefix_type->alignment : 8));
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("COMPONENT_SIZE"))) {
        if (prefix_type && prefix_type->kind == TYPE_ARRAY && prefix_type->array.element_type) {
            Emit(cg, "  %%t%u = add i64 0, %lld  ; 'COMPONENT_SIZE\n", t,
                 (long long)(prefix_type->array.element_type->size * 8));
        } else {
            Emit(cg, "  %%t%u = add i64 0, 0\n", t);
        }
        return t;
    }

    /* ─────────────────────────────────────────────────────────────────────
     * Address Attribute
     * ───────────────────────────────────────────────────────────────────── */

    if (Slice_Equal_Ignore_Case(attr, S("ADDRESS"))) {
        /* Generate address of prefix object */
        Symbol *sym = node->attribute.prefix->symbol;
        if (sym) {
            Emit(cg, "  %%t%u = ptrtoint ptr %%", t);
            Emit_Symbol_Name(cg, sym);
            Emit(cg, " to i64  ; 'ADDRESS\n");
        } else {
            Emit(cg, "  %%t%u = add i64 0, 0  ; 'ADDRESS (no symbol)\n", t);
        }
        return t;
    }

    /* ─────────────────────────────────────────────────────────────────────
     * Enumeration Attributes
     * ───────────────────────────────────────────────────────────────────── */

    if (Slice_Equal_Ignore_Case(attr, S("POS"))) {
        /* T'POS(x) - position of enumeration value */
        if (node->attribute.argument) {
            return Generate_Expression(cg, node->attribute.argument);
        }
        return 0;
    }

    if (Slice_Equal_Ignore_Case(attr, S("VAL"))) {
        /* T'VAL(n) - enumeration value at position n */
        if (node->attribute.argument) {
            return Generate_Expression(cg, node->attribute.argument);
        }
        return 0;
    }

    if (Slice_Equal_Ignore_Case(attr, S("SUCC"))) {
        if (node->attribute.argument) {
            uint32_t val = Generate_Expression(cg, node->attribute.argument);
            Emit(cg, "  %%t%u = add i64 %%t%u, 1  ; 'SUCC\n", t, val);
            return t;
        }
    }

    if (Slice_Equal_Ignore_Case(attr, S("PRED"))) {
        if (node->attribute.argument) {
            uint32_t val = Generate_Expression(cg, node->attribute.argument);
            Emit(cg, "  %%t%u = sub i64 %%t%u, 1  ; 'PRED\n", t, val);
            return t;
        }
    }

    /* ─────────────────────────────────────────────────────────────────────
     * Scalar Type Attributes
     * ───────────────────────────────────────────────────────────────────── */

    if (Slice_Equal_Ignore_Case(attr, S("MIN"))) {
        /* T'MIN(a, b) - minimum of two values */
        /* For now, just return first argument; proper impl needs both */
        if (node->attribute.argument) {
            return Generate_Expression(cg, node->attribute.argument);
        }
        return 0;
    }

    if (Slice_Equal_Ignore_Case(attr, S("MAX"))) {
        /* T'MAX(a, b) - maximum of two values */
        if (node->attribute.argument) {
            return Generate_Expression(cg, node->attribute.argument);
        }
        return 0;
    }

    if (Slice_Equal_Ignore_Case(attr, S("ABS"))) {
        /* T'ABS(x) or just abs function */
        if (node->attribute.argument) {
            uint32_t val = Generate_Expression(cg, node->attribute.argument);
            /* Compute abs: (x ^ (x >> 63)) - (x >> 63) for signed */
            uint32_t shift = Emit_Temp(cg);
            uint32_t xored = Emit_Temp(cg);
            Emit(cg, "  %%t%u = ashr i64 %%t%u, 63  ; sign bit\n", shift, val);
            Emit(cg, "  %%t%u = xor i64 %%t%u, %%t%u\n", xored, val, shift);
            Emit(cg, "  %%t%u = sub i64 %%t%u, %%t%u  ; 'ABS\n", t, xored, shift);
            return t;
        }
    }

    if (Slice_Equal_Ignore_Case(attr, S("MOD"))) {
        /* Modular arithmetic attribute */
        if (prefix_type && prefix_type->modulus > 0) {
            Emit(cg, "  %%t%u = add i64 0, %lld  ; 'MOD\n", t,
                 (long long)prefix_type->modulus);
            return t;
        }
    }

    /* ─────────────────────────────────────────────────────────────────────
     * String/Image Attributes (simplified - return 0 for now)
     * Full implementation would need runtime support
     * ───────────────────────────────────────────────────────────────────── */

    if (Slice_Equal_Ignore_Case(attr, S("IMAGE"))) {
        /* T'IMAGE(x) - string representation (RM 3.5.5) */
        if (node->attribute.argument) {
            uint32_t arg_val = Generate_Expression(cg, node->attribute.argument);

            if (prefix_type && (prefix_type->kind == TYPE_INTEGER ||
                                prefix_type->kind == TYPE_MODULAR ||
                                prefix_type->kind == TYPE_UNIVERSAL_INTEGER)) {
                /* Integer'IMAGE */
                Emit(cg, "  %%t%u = call { ptr, { i64, i64 } } @__ada_integer_image(i64 %%t%u)\n",
                     t, arg_val);
            } else if (prefix_type && prefix_type->kind == TYPE_CHARACTER) {
                /* Character'IMAGE */
                uint32_t char_val = Emit_Temp(cg);
                Emit(cg, "  %%t%u = trunc i64 %%t%u to i8\n", char_val, arg_val);
                Emit(cg, "  %%t%u = call { ptr, { i64, i64 } } @__ada_character_image(i8 %%t%u)\n",
                     t, char_val);
            } else if (prefix_type && (prefix_type->kind == TYPE_FLOAT ||
                                       prefix_type->kind == TYPE_UNIVERSAL_REAL)) {
                /* Float'IMAGE */
                Emit(cg, "  %%t%u = call { ptr, { i64, i64 } } @__ada_float_image(double %%t%u)\n",
                     t, arg_val);
            } else {
                /* Default: treat as integer */
                Emit(cg, "  %%t%u = call { ptr, { i64, i64 } } @__ada_integer_image(i64 %%t%u)\n",
                     t, arg_val);
            }
            return t;
        }
        Emit(cg, "  %%t%u = insertvalue { ptr, { i64, i64 } } undef, ptr null, 0  ; 'IMAGE no arg\n", t);
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("VALUE"))) {
        /* T'VALUE(s) - parse string to type (RM 3.5.5) */
        /* For now, placeholder - full implementation needs string parsing runtime */
        Emit(cg, "  %%t%u = add i64 0, 0  ; 'VALUE (needs runtime implementation)\n", t);
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("WIDTH"))) {
        /* T'WIDTH - maximum image width for type */
        if (prefix_type) {
            /* Estimate based on type size */
            int64_t width = prefix_type->size <= 4 ? 11 : 20;  /* digits + sign */
            Emit(cg, "  %%t%u = add i64 0, %lld  ; 'WIDTH\n", t, (long long)width);
            return t;
        }
    }

    /* ─────────────────────────────────────────────────────────────────────
     * Access Type Attributes
     * ───────────────────────────────────────────────────────────────────── */

    if (Slice_Equal_Ignore_Case(attr, S("ACCESS"))) {
        /* X'ACCESS - access to X (address) */
        Symbol *sym = node->attribute.prefix->symbol;
        if (sym) {
            Emit(cg, "  %%t%u = getelementptr i8, ptr %%", t);
            Emit_Symbol_Name(cg, sym);
            Emit(cg, ", i64 0  ; 'ACCESS\n");
        } else {
            Emit(cg, "  %%t%u = add i64 0, 0\n", t);
        }
        return t;
    }

    if (Slice_Equal_Ignore_Case(attr, S("UNCHECKED_ACCESS"))) {
        /* X'UNCHECKED_ACCESS - unchecked access to X */
        Symbol *sym = node->attribute.prefix->symbol;
        if (sym) {
            Emit(cg, "  %%t%u = getelementptr i8, ptr %%", t);
            Emit_Symbol_Name(cg, sym);
            Emit(cg, ", i64 0  ; 'UNCHECKED_ACCESS\n");
        } else {
            Emit(cg, "  %%t%u = add i64 0, 0\n", t);
        }
        return t;
    }

    /* Unhandled attribute */
    Emit(cg, "  %%t%u = add i64 0, 0  ; unhandled '%.*s\n", t,
         (int)attr.length, attr.data);
    return t;
}

/* Helper: Find component index by name in record type */
static int32_t Find_Record_Component(Type_Info *record_type, String_Slice name) {
    if (!record_type || record_type->kind != TYPE_RECORD) return -1;
    for (uint32_t i = 0; i < record_type->record.component_count; i++) {
        if (Slice_Equal_Ignore_Case(record_type->record.components[i].name, name)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Check if a choice is "others" */
static bool Is_Others_Choice(Syntax_Node *choice) {
    return choice && choice->kind == NK_IDENTIFIER &&
           Slice_Equal_Ignore_Case(choice->string_val.text, S("others"));
}

static uint32_t Generate_Aggregate(Code_Generator *cg, Syntax_Node *node) {
    /* Generate code for record/array aggregates
     * Supports: positional, named associations, others clause, ranges */
    Type_Info *agg_type = node->type;

    if (!agg_type) {
        Report_Error(node->location, "untyped aggregate in codegen");
        return 0;
    }

    if (agg_type->kind == TYPE_ARRAY && agg_type->array.index_count > 0) {
        /* Array aggregate - allocate on stack and initialize */
        uint32_t base = Emit_Temp(cg);
        int64_t low = Type_Bound_Value(agg_type->array.indices[0].low_bound);
        int64_t high = Type_Bound_Value(agg_type->array.indices[0].high_bound);
        int64_t count = high - low + 1;
        const char *elem_type = Type_To_Llvm(agg_type->array.element_type);

        Emit(cg, "  %%t%u = alloca [%lld x %s]  ; array aggregate\n",
             base, (long long)count, elem_type);

        /* Track which elements are initialized (for others clause) */
        bool *initialized = Arena_Allocate((size_t)count * sizeof(bool));
        for (int64_t i = 0; i < count; i++) initialized[i] = false;

        /* Default value for "others" clause (if any) */
        uint32_t others_val = 0;
        bool has_others = false;

        /* First pass: find "others" clause */
        for (uint32_t i = 0; i < node->aggregate.items.count; i++) {
            Syntax_Node *item = node->aggregate.items.items[i];
            if (item->kind == NK_ASSOCIATION && item->association.choices.count > 0) {
                if (Is_Others_Choice(item->association.choices.items[0])) {
                    others_val = Generate_Expression(cg, item->association.expression);
                    others_val = Emit_Convert(cg, others_val, "i64", elem_type);
                    has_others = true;
                    break;
                }
            }
        }

        /* Second pass: initialize elements */
        uint32_t positional_idx = 0;
        for (uint32_t i = 0; i < node->aggregate.items.count; i++) {
            Syntax_Node *item = node->aggregate.items.items[i];

            if (item->kind == NK_ASSOCIATION) {
                /* Named association: handle each choice */
                for (uint32_t c = 0; c < item->association.choices.count; c++) {
                    Syntax_Node *choice = item->association.choices.items[c];

                    if (Is_Others_Choice(choice)) {
                        continue;  /* Handle in third pass */
                    }

                    if (choice->kind == NK_RANGE) {
                        /* Range choice: 1..5 => value */
                        int64_t rng_low = choice->range.low->kind == NK_INTEGER ?
                                          choice->range.low->integer_lit.value : low;
                        int64_t rng_high = choice->range.high->kind == NK_INTEGER ?
                                           choice->range.high->integer_lit.value : high;
                        uint32_t val = Generate_Expression(cg, item->association.expression);
                        val = Emit_Convert(cg, val, "i64", elem_type);

                        for (int64_t idx = rng_low; idx <= rng_high; idx++) {
                            int64_t arr_idx = idx - low;
                            if (arr_idx >= 0 && arr_idx < count) {
                                uint32_t ptr = Emit_Temp(cg);
                                Emit(cg, "  %%t%u = getelementptr %s, ptr %%t%u, i64 %lld\n",
                                     ptr, elem_type, base, (long long)arr_idx);
                                Emit(cg, "  store %s %%t%u, ptr %%t%u\n", elem_type, val, ptr);
                                initialized[arr_idx] = true;
                            }
                        }
                    } else if (choice->kind == NK_INTEGER) {
                        /* Single index: 3 => value */
                        int64_t idx = choice->integer_lit.value - low;
                        if (idx >= 0 && idx < count) {
                            uint32_t val = Generate_Expression(cg, item->association.expression);
                            val = Emit_Convert(cg, val, "i64", elem_type);
                            uint32_t ptr = Emit_Temp(cg);
                            Emit(cg, "  %%t%u = getelementptr %s, ptr %%t%u, i64 %lld\n",
                                 ptr, elem_type, base, (long long)idx);
                            Emit(cg, "  store %s %%t%u, ptr %%t%u\n", elem_type, val, ptr);
                            initialized[idx] = true;
                        }
                    }
                }
            } else {
                /* Positional association */
                if (positional_idx < (uint32_t)count) {
                    uint32_t val = Generate_Expression(cg, item);
                    val = Emit_Convert(cg, val, "i64", elem_type);
                    uint32_t ptr = Emit_Temp(cg);
                    Emit(cg, "  %%t%u = getelementptr %s, ptr %%t%u, i64 %u\n",
                         ptr, elem_type, base, positional_idx);
                    Emit(cg, "  store %s %%t%u, ptr %%t%u\n", elem_type, val, ptr);
                    initialized[positional_idx] = true;
                    positional_idx++;
                }
            }
        }

        /* Third pass: fill uninitialized with "others" value */
        if (has_others) {
            for (int64_t idx = 0; idx < count; idx++) {
                if (!initialized[idx]) {
                    uint32_t ptr = Emit_Temp(cg);
                    Emit(cg, "  %%t%u = getelementptr %s, ptr %%t%u, i64 %lld\n",
                         ptr, elem_type, base, (long long)idx);
                    Emit(cg, "  store %s %%t%u, ptr %%t%u\n", elem_type, others_val, ptr);
                }
            }
        }

        return base;
    }

    if (agg_type->kind == TYPE_RECORD) {
        /* Record aggregate - allocate [N x i8] and fill fields by offset */
        uint32_t base = Emit_Temp(cg);
        uint32_t record_size = agg_type->size > 0 ? agg_type->size : 8;

        Emit(cg, "  %%t%u = alloca [%u x i8]  ; record aggregate\n", base, record_size);

        /* Track initialized components for others clause */
        uint32_t comp_count = agg_type->record.component_count;
        bool *initialized = Arena_Allocate(comp_count * sizeof(bool));
        for (uint32_t i = 0; i < comp_count; i++) initialized[i] = false;

        /* Default value for "others" clause */
        uint32_t others_val = 0;
        bool has_others = false;

        /* First pass: find "others" clause */
        for (uint32_t i = 0; i < node->aggregate.items.count; i++) {
            Syntax_Node *item = node->aggregate.items.items[i];
            if (item->kind == NK_ASSOCIATION && item->association.choices.count > 0) {
                if (Is_Others_Choice(item->association.choices.items[0])) {
                    others_val = Generate_Expression(cg, item->association.expression);
                    has_others = true;
                    break;
                }
            }
        }

        /* Second pass: initialize fields */
        uint32_t positional_idx = 0;
        for (uint32_t i = 0; i < node->aggregate.items.count; i++) {
            Syntax_Node *item = node->aggregate.items.items[i];

            if (item->kind == NK_ASSOCIATION) {
                /* Named association: field_name => value */
                for (uint32_t c = 0; c < item->association.choices.count; c++) {
                    Syntax_Node *choice = item->association.choices.items[c];

                    if (Is_Others_Choice(choice)) {
                        continue;  /* Handle in third pass */
                    }

                    if (choice->kind == NK_IDENTIFIER) {
                        int32_t comp_idx = Find_Record_Component(agg_type, choice->string_val.text);
                        if (comp_idx >= 0) {
                            Component_Info *comp = &agg_type->record.components[comp_idx];
                            const char *comp_type = Type_To_Llvm(comp->component_type);
                            uint32_t val = Generate_Expression(cg, item->association.expression);
                            val = Emit_Convert(cg, val, "i64", comp_type);

                            uint32_t ptr = Emit_Temp(cg);
                            Emit(cg, "  %%t%u = getelementptr i8, ptr %%t%u, i64 %u\n",
                                 ptr, base, comp->byte_offset);
                            Emit(cg, "  store %s %%t%u, ptr %%t%u\n", comp_type, val, ptr);
                            initialized[comp_idx] = true;
                        }
                    }
                }
            } else {
                /* Positional: initialize component by position */
                if (positional_idx < comp_count) {
                    Component_Info *comp = &agg_type->record.components[positional_idx];
                    const char *comp_type = Type_To_Llvm(comp->component_type);
                    uint32_t val = Generate_Expression(cg, item);
                    val = Emit_Convert(cg, val, "i64", comp_type);

                    uint32_t ptr = Emit_Temp(cg);
                    Emit(cg, "  %%t%u = getelementptr i8, ptr %%t%u, i64 %u\n",
                         ptr, base, comp->byte_offset);
                    Emit(cg, "  store %s %%t%u, ptr %%t%u\n", comp_type, val, ptr);
                    initialized[positional_idx] = true;
                    positional_idx++;
                }
            }
        }

        /* Third pass: fill uninitialized with "others" value (uncommon for records) */
        if (has_others) {
            for (uint32_t idx = 0; idx < comp_count; idx++) {
                if (!initialized[idx]) {
                    Component_Info *comp = &agg_type->record.components[idx];
                    const char *comp_type = Type_To_Llvm(comp->component_type);
                    uint32_t converted = Emit_Convert(cg, others_val, "i64", comp_type);

                    uint32_t ptr = Emit_Temp(cg);
                    Emit(cg, "  %%t%u = getelementptr i8, ptr %%t%u, i64 %u\n",
                         ptr, base, comp->byte_offset);
                    Emit(cg, "  store %s %%t%u, ptr %%t%u\n", comp_type, converted, ptr);
                }
            }
        }

        return base;
    }

    return 0;
}

static uint32_t Generate_Qualified(Code_Generator *cg, Syntax_Node *node) {
    /* Type'(expression) - just generate the expression, no runtime conversion */
    return Generate_Expression(cg, node->qualified.expression);
}

static uint32_t Generate_Allocator(Code_Generator *cg, Syntax_Node *node) {
    /* new T or new T'(value) */
    Type_Info *alloc_type = node->type;
    uint32_t t = Emit_Temp(cg);

    if (!alloc_type) {
        Emit(cg, "  %%t%u = call ptr @malloc(i64 8)\n", t);
        return t;
    }

    uint64_t size = alloc_type->size > 0 ? alloc_type->size : 8;
    Emit(cg, "  %%t%u = call ptr @malloc(i64 %llu)\n", t, (unsigned long long)size);

    /* If there's an initializer, store it */
    if (node->allocator.expression) {
        uint32_t val = Generate_Expression(cg, node->allocator.expression);
        Emit(cg, "  store %s %%t%u, ptr %%t%u\n", Type_To_Llvm(alloc_type), val, t);
    }

    return t;
}

static uint32_t Generate_Expression(Code_Generator *cg, Syntax_Node *node) {
    if (!node) return 0;

    switch (node->kind) {
        case NK_INTEGER:    return Generate_Integer_Literal(cg, node);
        case NK_REAL:       return Generate_Real_Literal(cg, node);
        case NK_STRING:     return Generate_String_Literal(cg, node);
        case NK_CHARACTER:  return Generate_Integer_Literal(cg, node);  /* Char as int */
        case NK_NULL:       { uint32_t t = Emit_Temp(cg);
                             Emit(cg, "  %%t%u = inttoptr i64 0 to ptr\n", t);
                             return t; }
        case NK_IDENTIFIER: return Generate_Identifier(cg, node);
        case NK_SELECTED:   return Generate_Selected(cg, node);
        case NK_ATTRIBUTE:  return Generate_Attribute(cg, node);
        case NK_BINARY_OP:  return Generate_Binary_Op(cg, node);
        case NK_UNARY_OP:   return Generate_Unary_Op(cg, node);
        case NK_APPLY:      return Generate_Apply(cg, node);
        case NK_AGGREGATE:  return Generate_Aggregate(cg, node);
        case NK_QUALIFIED:  return Generate_Qualified(cg, node);
        case NK_ALLOCATOR:  return Generate_Allocator(cg, node);

        default:
            Report_Error(node->location, "unsupported expression kind in codegen");
            return 0;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.4 Statement Code Generation
 * ───────────────────────────────────────────────────────────────────────── */

static void Generate_Statement(Code_Generator *cg, Syntax_Node *node);

static void Generate_Statement_List(Code_Generator *cg, Node_List *list) {
    for (uint32_t i = 0; i < list->count; i++) {
        Generate_Statement(cg, list->items[i]);
    }
}

static void Generate_Assignment(Code_Generator *cg, Syntax_Node *node) {
    Syntax_Node *target = node->assignment.target;

    /* Handle indexed component target (array element assignment) */
    if (target->kind == NK_APPLY) {
        Type_Info *prefix_type = target->apply.prefix->type;
        if (prefix_type && prefix_type->kind == TYPE_ARRAY) {
            /* Array element assignment: DATA(I) := value */
            Symbol *array_sym = target->apply.prefix->symbol;
            if (!array_sym) return;

            /* Get array base address (not loaded value) */
            uint32_t base = Emit_Temp(cg);
            Emit(cg, "  %%t%u = getelementptr i8, ptr %%", base);
            Emit_Symbol_Name(cg, array_sym);
            Emit(cg, ", i64 0\n");

            /* Generate index expression */
            uint32_t idx = Generate_Expression(cg, target->apply.arguments.items[0]);

            /* Adjust for array low bound (Ada arrays can start at any index) */
            int64_t low_bound = Array_Low_Bound(prefix_type);
            if (low_bound != 0) {
                uint32_t adj = Emit_Temp(cg);
                Emit(cg, "  %%t%u = sub i64 %%t%u, %lld\n", adj, idx, (long long)low_bound);
                idx = adj;
            }

            /* Get pointer to element */
            const char *elem_type = Type_To_Llvm(prefix_type->array.element_type);
            uint32_t ptr = Emit_Temp(cg);
            Emit(cg, "  %%t%u = getelementptr %s, ptr %%t%u, i64 %%t%u\n",
                 ptr, elem_type, base, idx);

            /* Generate value and store */
            uint32_t value = Generate_Expression(cg, node->assignment.value);
            value = Emit_Convert(cg, value, "i64", elem_type);
            Emit(cg, "  store %s %%t%u, ptr %%t%u\n", elem_type, value, ptr);
            return;
        }
    }

    /* Handle selected component target (record field assignment) */
    if (target->kind == NK_SELECTED) {
        Syntax_Node *prefix = target->selected.prefix;
        Type_Info *prefix_type = prefix->type;

        if (prefix_type && prefix_type->kind == TYPE_RECORD) {
            /* Find component offset */
            uint32_t offset = 0;
            Type_Info *comp_type = NULL;
            for (uint32_t i = 0; i < prefix_type->record.component_count; i++) {
                if (Slice_Equal_Ignore_Case(prefix_type->record.components[i].name,
                                            target->selected.selector)) {
                    offset = prefix_type->record.components[i].byte_offset;
                    comp_type = prefix_type->record.components[i].component_type;
                    break;
                }
            }

            const char *comp_llvm_type = Type_To_Llvm(comp_type);

            /* Get base address of record variable */
            Symbol *record_sym = prefix->symbol;
            if (!record_sym) return;

            /* Calculate address of field */
            uint32_t field_ptr = Emit_Temp(cg);
            Emit(cg, "  %%t%u = getelementptr i8, ptr %%", field_ptr);
            Emit_Symbol_Name(cg, record_sym);
            Emit(cg, ", i64 %u\n", offset);

            /* Generate value and store */
            uint32_t value = Generate_Expression(cg, node->assignment.value);
            value = Emit_Convert(cg, value, "i64", comp_llvm_type);
            Emit(cg, "  store %s %%t%u, ptr %%t%u\n", comp_llvm_type, value, field_ptr);
            return;
        }
    }

    /* Simple variable target */
    Symbol *target_sym = target->symbol;
    if (!target_sym) return;

    Type_Info *ty = target_sym->type;

    /* Handle constrained string/character array assignment */
    if (ty && ty->kind == TYPE_ARRAY && ty->array.is_constrained &&
        ty->array.element_type && ty->array.element_type->kind == TYPE_CHARACTER) {
        /* The value is a fat pointer - copy data to target variable */
        uint32_t fat_ptr = Generate_Expression(cg, node->assignment.value);
        Emit_Fat_Pointer_Copy_To_Name(cg, fat_ptr, target_sym);
        return;
    }

    uint32_t value = Generate_Expression(cg, node->assignment.value);

    /* Check if this is an uplevel (outer scope) variable access */
    Symbol *var_owner = target_sym->defining_scope ? target_sym->defining_scope->owner : NULL;
    bool is_uplevel = cg->current_function && var_owner &&
                      var_owner != cg->current_function;

    const char *type_str = Type_To_Llvm(ty);
    /* Truncate from i64 computation to actual storage type */
    value = Emit_Convert(cg, value, "i64", type_str);

    /* Check if package-level global (parent is NULL or SYMBOL_PACKAGE) */
    bool is_global = !target_sym->parent || target_sym->parent->kind == SYMBOL_PACKAGE;

    if (is_uplevel && cg->is_nested) {
        /* Uplevel store through frame pointer */
        Emit(cg, "  ; UPLEVEL STORE: %.*s via frame pointer\n",
             (int)target_sym->name.length, target_sym->name.data);
        Emit(cg, "  store %s %%t%u, ptr %%__frame.", type_str, value);
        Emit_Symbol_Name(cg, target_sym);
        Emit(cg, "\n");
    } else if (is_global) {
        /* Global variable - use @ prefix */
        Emit(cg, "  store %s %%t%u, ptr @", type_str, value);
        Emit_Symbol_Name(cg, target_sym);
        Emit(cg, "\n");
    } else {
        Emit(cg, "  store %s %%t%u, ptr %%", type_str, value);
        Emit_Symbol_Name(cg, target_sym);
        Emit(cg, "\n");
    }
}

static void Generate_If_Statement(Code_Generator *cg, Syntax_Node *node) {
    uint32_t cond = Generate_Expression(cg, node->if_stmt.condition);
    uint32_t then_label = Emit_Label(cg);
    uint32_t else_label = Emit_Label(cg);
    uint32_t end_label = Emit_Label(cg);

    Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", cond, then_label, else_label);

    Emit(cg, "L%u:\n", then_label);
    Generate_Statement_List(cg, &node->if_stmt.then_stmts);
    Emit(cg, "  br label %%L%u\n", end_label);

    Emit(cg, "L%u:\n", else_label);
    if (node->if_stmt.else_stmts.count > 0) {
        Generate_Statement_List(cg, &node->if_stmt.else_stmts);
    }
    Emit(cg, "  br label %%L%u\n", end_label);

    Emit(cg, "L%u:\n", end_label);
}

static void Generate_Loop_Statement(Code_Generator *cg, Syntax_Node *node) {
    uint32_t loop_start = Emit_Label(cg);
    uint32_t loop_body = Emit_Label(cg);
    uint32_t loop_end = Emit_Label(cg);

    uint32_t saved_exit = cg->loop_exit_label;
    uint32_t saved_cont = cg->loop_continue_label;
    cg->loop_exit_label = loop_end;
    cg->loop_continue_label = loop_start;

    Emit(cg, "  br label %%L%u\n", loop_start);
    Emit(cg, "L%u:\n", loop_start);

    /* Condition check for WHILE loops */
    if (node->loop_stmt.iteration_scheme &&
        node->loop_stmt.iteration_scheme->kind != NK_BINARY_OP) {
        /* WHILE loop */
        uint32_t cond = Generate_Expression(cg, node->loop_stmt.iteration_scheme);
        Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", cond, loop_body, loop_end);
    } else {
        Emit(cg, "  br label %%L%u\n", loop_body);
    }

    Emit(cg, "L%u:\n", loop_body);
    Generate_Statement_List(cg, &node->loop_stmt.statements);
    Emit(cg, "  br label %%L%u\n", loop_start);

    Emit(cg, "L%u:\n", loop_end);

    cg->loop_exit_label = saved_exit;
    cg->loop_continue_label = saved_cont;
}

static void Generate_Return_Statement(Code_Generator *cg, Syntax_Node *node) {
    cg->has_return = true;
    if (node->return_stmt.expression) {
        uint32_t value = Generate_Expression(cg, node->return_stmt.expression);
        const char *type_str = cg->current_function && cg->current_function->return_type
            ? Type_To_Llvm(cg->current_function->return_type) : "i64";
        /* Truncate from i64 computation to actual return type */
        value = Emit_Convert(cg, value, "i64", type_str);
        Emit(cg, "  ret %s %%t%u\n", type_str, value);
    } else {
        Emit(cg, "  ret void\n");
    }
}

static void Generate_Case_Statement(Code_Generator *cg, Syntax_Node *node) {
    /* CASE expr IS WHEN choice => stmts; ... END CASE; */
    uint32_t selector = Generate_Expression(cg, node->case_stmt.expression);
    uint32_t end_label = Emit_Label(cg);

    /* Generate switch-like structure using branches */
    uint32_t num_alts = node->case_stmt.alternatives.count;
    uint32_t *alt_labels = Arena_Allocate(num_alts * sizeof(uint32_t));

    /* Allocate labels for each alternative */
    for (uint32_t i = 0; i < num_alts; i++) {
        alt_labels[i] = Emit_Label(cg);
    }

    /* Generate branching logic */
    for (uint32_t i = 0; i < num_alts; i++) {
        Syntax_Node *alt = node->case_stmt.alternatives.items[i];
        uint32_t next_check = (i + 1 < num_alts) ? Emit_Label(cg) : end_label;

        /* Check each choice in this alternative */
        for (uint32_t j = 0; j < alt->association.choices.count; j++) {
            Syntax_Node *choice = alt->association.choices.items[j];

            if (choice->kind == NK_OTHERS) {
                /* OTHERS matches everything - jump to alternative */
                Emit(cg, "  br label %%L%u\n", alt_labels[i]);
            } else if (choice->kind == NK_RANGE) {
                /* Range check: low <= selector <= high */
                uint32_t low = Generate_Expression(cg, choice->range.low);
                uint32_t high = Generate_Expression(cg, choice->range.high);
                uint32_t cmp1 = Emit_Temp(cg);
                uint32_t cmp2 = Emit_Temp(cg);
                uint32_t both = Emit_Temp(cg);

                Emit(cg, "  %%t%u = icmp sle i64 %%t%u, %%t%u\n", cmp1, low, selector);
                Emit(cg, "  %%t%u = icmp sle i64 %%t%u, %%t%u\n", cmp2, selector, high);
                Emit(cg, "  %%t%u = and i1 %%t%u, %%t%u\n", both, cmp1, cmp2);

                uint32_t next_choice = (j + 1 < alt->association.choices.count) ?
                                       Emit_Label(cg) : next_check;
                Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
                     both, alt_labels[i], next_choice);

                if (j + 1 < alt->association.choices.count) {
                    Emit(cg, "L%u:\n", next_choice);
                }
            } else {
                /* Single value check */
                uint32_t val = Generate_Expression(cg, choice);
                uint32_t cmp = Emit_Temp(cg);

                Emit(cg, "  %%t%u = icmp eq i64 %%t%u, %%t%u\n", cmp, selector, val);

                uint32_t next_choice = (j + 1 < alt->association.choices.count) ?
                                       Emit_Label(cg) : next_check;
                Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
                     cmp, alt_labels[i], next_choice);

                if (j + 1 < alt->association.choices.count) {
                    Emit(cg, "L%u:\n", next_choice);
                }
            }
        }

        if (i + 1 < num_alts) {
            Emit(cg, "L%u:\n", next_check);
        }
    }

    /* Generate alternative bodies - expression is a block with statements */
    for (uint32_t i = 0; i < num_alts; i++) {
        Syntax_Node *alt = node->case_stmt.alternatives.items[i];
        Emit(cg, "L%u:\n", alt_labels[i]);
        if (alt->association.expression &&
            alt->association.expression->kind == NK_BLOCK) {
            Generate_Statement_List(cg, &alt->association.expression->block_stmt.statements);
        }
        Emit(cg, "  br label %%L%u\n", end_label);
    }

    Emit(cg, "L%u:\n", end_label);
}

static void Generate_For_Loop(Code_Generator *cg, Syntax_Node *node) {
    /* FOR loop with iteration variable - iteration_scheme is NK_BINARY_OP TK_IN */
    Syntax_Node *iter = node->loop_stmt.iteration_scheme;
    if (!iter || iter->kind != NK_BINARY_OP || iter->binary.op != TK_IN) {
        /* Not a FOR loop - fall back to simple loop */
        return;
    }

    Syntax_Node *loop_id = iter->binary.left;
    Syntax_Node *range = iter->binary.right;
    Symbol *loop_var = loop_id->symbol;
    bool is_reverse = node->loop_stmt.is_reverse;

    uint32_t loop_start = Emit_Label(cg);
    uint32_t loop_body = Emit_Label(cg);
    uint32_t loop_end = Emit_Label(cg);

    uint32_t saved_exit = cg->loop_exit_label;
    cg->loop_exit_label = loop_end;

    /* Allocate loop variable if we have a symbol */
    if (loop_var) {
        Emit(cg, "  %%");
        Emit_Symbol_Name(cg, loop_var);
        Emit(cg, " = alloca i64\n");
    }

    /* Get range bounds */
    uint32_t low_val, high_val;
    if (range && range->kind == NK_RANGE) {
        low_val = Generate_Expression(cg, range->range.low);
        high_val = Generate_Expression(cg, range->range.high);
    } else if (range && range->kind == NK_ATTRIBUTE &&
               Slice_Equal_Ignore_Case(range->attribute.name, S("RANGE"))) {
        /* X'RANGE attribute - need to generate both 'FIRST and 'LAST */
        Type_Info *prefix_type = range->attribute.prefix->type;
        Symbol *prefix_sym = range->attribute.prefix->symbol;

        /* Check if this is an unconstrained array needing runtime bounds */
        if (prefix_type && Type_Is_Unconstrained_Array(prefix_type) &&
            prefix_sym && (prefix_sym->kind == SYMBOL_PARAMETER ||
                           prefix_sym->kind == SYMBOL_VARIABLE)) {
            /* Load fat pointer and extract both bounds */
            uint32_t fat = Emit_Temp(cg);
            Emit(cg, "  %%t%u = load " FAT_PTR_TYPE ", ptr %%", fat);
            Emit_Symbol_Name(cg, prefix_sym);
            Emit(cg, "  ; load fat pointer for 'RANGE\n");
            low_val = Emit_Fat_Pointer_Low(cg, fat);
            high_val = Emit_Fat_Pointer_High(cg, fat);
        } else if (prefix_type && (prefix_type->kind == TYPE_ARRAY ||
                                   prefix_type->kind == TYPE_STRING)) {
            /* Constrained array - use compile-time bounds */
            uint32_t dim = Get_Dimension_Index(range->attribute.argument);
            if (dim < prefix_type->array.index_count) {
                low_val = Emit_Temp(cg);
                Emit(cg, "  %%t%u = add i64 0, %lld  ; 'RANGE low\n", low_val,
                     (long long)Type_Bound_Value(prefix_type->array.indices[dim].low_bound));
                high_val = Emit_Temp(cg);
                Emit(cg, "  %%t%u = add i64 0, %lld  ; 'RANGE high\n", high_val,
                     (long long)Type_Bound_Value(prefix_type->array.indices[dim].high_bound));
            } else {
                low_val = high_val = 0;
            }
        } else {
            /* Fallback */
            low_val = Generate_Expression(cg, range);
            high_val = low_val;
        }
    } else {
        /* Other expression - evaluate as low bound, assume scalar with same high */
        low_val = Generate_Expression(cg, range);
        high_val = low_val;
    }

    /* Initialize loop variable */
    if (loop_var) {
        Emit(cg, "  store i64 %%t%u, ptr %%", is_reverse ? high_val : low_val);
        Emit_Symbol_Name(cg, loop_var);
        Emit(cg, "\n");
    }

    /* Loop start - check condition */
    Emit(cg, "  br label %%L%u\n", loop_start);
    Emit(cg, "L%u:\n", loop_start);

    uint32_t cur = Emit_Temp(cg);
    if (loop_var) {
        Emit(cg, "  %%t%u = load i64, ptr %%", cur);
        Emit_Symbol_Name(cg, loop_var);
        Emit(cg, "\n");
    }

    uint32_t cond = Emit_Temp(cg);
    if (is_reverse) {
        Emit(cg, "  %%t%u = icmp sge i64 %%t%u, %%t%u\n", cond, cur, low_val);
    } else {
        Emit(cg, "  %%t%u = icmp sle i64 %%t%u, %%t%u\n", cond, cur, high_val);
    }
    Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", cond, loop_body, loop_end);

    /* Loop body */
    Emit(cg, "L%u:\n", loop_body);
    Generate_Statement_List(cg, &node->loop_stmt.statements);

    /* Increment/decrement loop variable */
    if (loop_var) {
        uint32_t next = Emit_Temp(cg);
        if (is_reverse) {
            Emit(cg, "  %%t%u = sub i64 %%t%u, 1\n", next, cur);
        } else {
            Emit(cg, "  %%t%u = add i64 %%t%u, 1\n", next, cur);
        }
        Emit(cg, "  store i64 %%t%u, ptr %%", next);
        Emit_Symbol_Name(cg, loop_var);
        Emit(cg, "\n");
    }

    Emit(cg, "  br label %%L%u\n", loop_start);
    Emit(cg, "L%u:\n", loop_end);

    cg->loop_exit_label = saved_exit;
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.4.8 Exception Handling
 * ───────────────────────────────────────────────────────────────────────── */

/* Forward declaration for Generate_Declaration_List (used in blocks) */
static void Generate_Declaration_List(Code_Generator *cg, Node_List *list);
static void Generate_Generic_Instance_Body(Code_Generator *cg, Symbol *inst_sym, Syntax_Node *template_body);

static void Generate_Raise_Statement(Code_Generator *cg, Syntax_Node *node) {
    /* RAISE E; or RAISE; (reraise) */
    if (node->raise_stmt.exception_name) {
        Symbol *exc = node->raise_stmt.exception_name->symbol;
        if (exc) {
            Emit(cg, "  ; RAISE ");
            Emit_Symbol_Name(cg, exc);
            Emit(cg, "\n");

            /* Store exception identity and call __ada_raise */
            uint32_t exc_addr = Emit_Temp(cg);
            Emit(cg, "  %%t%u = ptrtoint ptr @__exc.", exc_addr);
            Emit_Symbol_Name(cg, exc);
            Emit(cg, " to i64\n");
            Emit(cg, "  call void @__ada_raise(i64 %%t%u)\n", exc_addr);
        }
    } else {
        /* Reraise current exception */
        Emit(cg, "  ; RAISE (reraise)\n");
        Emit(cg, "  call void @__ada_reraise()\n");
    }
    Emit(cg, "  unreachable\n");
}

static void Generate_Block_Statement(Code_Generator *cg, Syntax_Node *node) {
    /* Block with optional declarations and exception handlers */
    bool has_handlers = node->block_stmt.handlers.count > 0;

    if (has_handlers) {
        /* Setup exception handling using setjmp/longjmp */
        uint32_t jmp_buf = Emit_Temp(cg);
        uint32_t handler_label = Emit_Label(cg);
        uint32_t normal_label = Emit_Label(cg);
        uint32_t end_label = Emit_Label(cg);

        /* Allocate jmp_buf (200 bytes for safety) */
        Emit(cg, "  %%t%u = alloca [200 x i8], align 16  ; jmp_buf\n", jmp_buf);

        /* Push exception handler */
        Emit(cg, "  call void @__ada_push_handler(ptr %%t%u)\n", jmp_buf);

        /* Call setjmp */
        uint32_t setjmp_result = Emit_Temp(cg);
        Emit(cg, "  %%t%u = call i32 @setjmp(ptr %%t%u)\n", setjmp_result, jmp_buf);

        /* Branch based on setjmp return */
        uint32_t is_normal = Emit_Temp(cg);
        Emit(cg, "  %%t%u = icmp eq i32 %%t%u, 0\n", is_normal, setjmp_result);
        Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
             is_normal, normal_label, handler_label);

        /* Normal execution path */
        Emit(cg, "L%u:\n", normal_label);

        /* Save and set exception context */
        uint32_t saved_handler = cg->exception_handler_label;
        uint32_t saved_jmp_buf = cg->exception_jmp_buf;
        bool saved_in_region = cg->in_exception_region;

        cg->exception_handler_label = handler_label;
        cg->exception_jmp_buf = jmp_buf;
        cg->in_exception_region = true;

        /* Generate block declarations */
        Generate_Declaration_List(cg, &node->block_stmt.declarations);

        /* Generate block statements */
        Generate_Statement_List(cg, &node->block_stmt.statements);

        /* Pop handler on normal exit */
        Emit(cg, "  call void @__ada_pop_handler()\n");
        Emit(cg, "  br label %%L%u\n", end_label);

        /* Exception handler entry */
        Emit(cg, "L%u:\n", handler_label);
        Emit(cg, "  call void @__ada_pop_handler()\n");

        /* Get current exception identity */
        uint32_t exc_id = Emit_Temp(cg);
        Emit(cg, "  %%t%u = call i64 @__ada_current_exception()\n", exc_id);

        /* Generate exception handlers */
        uint32_t next_handler = 0;
        for (uint32_t i = 0; i < node->block_stmt.handlers.count; i++) {
            Syntax_Node *handler = node->block_stmt.handlers.items[i];
            if (!handler) continue;

            if (next_handler != 0) {
                Emit(cg, "L%u:\n", next_handler);
            }
            next_handler = Emit_Label(cg);
            uint32_t handler_body = Emit_Label(cg);

            /* Check each exception name in the handler */
            bool has_others = false;
            for (uint32_t j = 0; j < handler->handler.exceptions.count; j++) {
                Syntax_Node *exc_name = handler->handler.exceptions.items[j];
                if (exc_name->kind == NK_OTHERS) {
                    has_others = true;
                    break;
                }
            }

            if (has_others) {
                /* WHEN OTHERS => catches all */
                Emit(cg, "  br label %%L%u\n", handler_body);
            } else {
                /* Check against specific exceptions */
                for (uint32_t j = 0; j < handler->handler.exceptions.count; j++) {
                    Syntax_Node *exc_name = handler->handler.exceptions.items[j];
                    if (exc_name->symbol) {
                        uint32_t exc_ptr = Emit_Temp(cg);
                        Emit(cg, "  %%t%u = ptrtoint ptr @__exc.", exc_ptr);
                        Emit_Symbol_Name(cg, exc_name->symbol);
                        Emit(cg, " to i64\n");
                        uint32_t match = Emit_Temp(cg);
                        Emit(cg, "  %%t%u = icmp eq i64 %%t%u, %%t%u\n",
                             match, exc_id, exc_ptr);
                        Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
                             match, handler_body, next_handler);
                    }
                }
            }

            /* Handler body */
            Emit(cg, "L%u:\n", handler_body);
            Generate_Statement_List(cg, &handler->handler.statements);
            Emit(cg, "  br label %%L%u\n", end_label);
        }

        /* If no handler matched, reraise */
        if (next_handler != 0) {
            Emit(cg, "L%u:\n", next_handler);
            Emit(cg, "  call void @__ada_reraise()\n");
            Emit(cg, "  unreachable\n");
        }

        /* End of block */
        Emit(cg, "L%u:\n", end_label);

        /* Restore exception context */
        cg->exception_handler_label = saved_handler;
        cg->exception_jmp_buf = saved_jmp_buf;
        cg->in_exception_region = saved_in_region;
    } else {
        /* Simple block without exception handlers */
        Generate_Declaration_List(cg, &node->block_stmt.declarations);
        Generate_Statement_List(cg, &node->block_stmt.statements);
    }
}

static void Generate_Statement(Code_Generator *cg, Syntax_Node *node) {
    if (!node) return;

    switch (node->kind) {
        case NK_ASSIGNMENT:
            Generate_Assignment(cg, node);
            break;

        case NK_CALL_STMT: {
            /* Procedure call - might be NK_APPLY or NK_IDENTIFIER (no args) */
            Syntax_Node *target = node->assignment.target;
            if (target->kind == NK_APPLY) {
                Generate_Expression(cg, target);
            } else if (target->kind == NK_IDENTIFIER) {
                /* Parameterless procedure/function call */
                Symbol *proc = target->symbol;
                if (proc && (proc->kind == SYMBOL_PROCEDURE || proc->kind == SYMBOL_FUNCTION)) {
                    /* Check if calling a nested function of current scope */
                    bool callee_is_nested = proc->parent &&
                        (proc->parent->kind == SYMBOL_FUNCTION ||
                         proc->parent->kind == SYMBOL_PROCEDURE);

                    if (proc->return_type) {
                        Emit(cg, "  call %s @", Type_To_Llvm(proc->return_type));
                    } else {
                        Emit(cg, "  call void @");
                    }
                    Emit_Symbol_Name(cg, proc);

                    if (callee_is_nested && cg->current_function == proc->parent) {
                        /* Nested call from parent - pass address of first local variable as frame */
                        Emit(cg, "(ptr %%__frame_base)\n");
                    } else {
                        Emit(cg, "()\n");
                    }
                }
            }
        } break;

        case NK_RETURN:
            Generate_Return_Statement(cg, node);
            break;

        case NK_IF:
            Generate_If_Statement(cg, node);
            break;

        case NK_LOOP:
            if (node->loop_stmt.iteration_scheme &&
                node->loop_stmt.iteration_scheme->kind == NK_BINARY_OP &&
                node->loop_stmt.iteration_scheme->binary.op == TK_IN) {
                Generate_For_Loop(cg, node);
            } else {
                Generate_Loop_Statement(cg, node);
            }
            break;

        case NK_CASE:
            Generate_Case_Statement(cg, node);
            break;

        case NK_EXIT:
            if (node->exit_stmt.condition) {
                uint32_t cond = Generate_Expression(cg, node->exit_stmt.condition);
                uint32_t cont = Emit_Label(cg);
                Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
                     cond, cg->loop_exit_label, cont);
                Emit(cg, "L%u:\n", cont);
            } else {
                Emit(cg, "  br label %%L%u\n", cg->loop_exit_label);
            }
            break;

        case NK_NULL_STMT:
            /* No code needed */
            break;

        case NK_BLOCK:
            Generate_Block_Statement(cg, node);
            break;

        case NK_RAISE:
            Generate_Raise_Statement(cg, node);
            break;

        case NK_DELAY:
            {
                /* DELAY expression — sleep for specified duration */
                /* Expression should be in seconds, convert to microseconds */
                uint32_t val = Generate_Expression(cg, node->delay_stmt.expression);
                /* Convert to microseconds (assuming Duration in seconds) */
                uint32_t us = Emit_Temp(cg);
                Emit(cg, "  %%t%u = fmul double %%t%u, 1.0e6\n", us, val);
                uint32_t us_int = Emit_Temp(cg);
                Emit(cg, "  %%t%u = fptoui double %%t%u to i64\n", us_int, us);
                Emit(cg, "  call void @__ada_delay(i64 %%t%u)\n", us_int);
            }
            break;

        case NK_ACCEPT:
            {
                /* ACCEPT statement — rendezvous with caller */
                /* For now, just execute the body (full implementation needs runtime) */
                Emit(cg, "  ; ACCEPT %.*s (simplified)\n",
                     (int)node->accept_stmt.entry_name.length,
                     node->accept_stmt.entry_name.data);
                Generate_Statement_List(cg, &node->accept_stmt.statements);
            }
            break;

        case NK_SELECT:
            /* SELECT statement — selective wait (Ada 83 9.7)
             * Forms: selective_wait, conditional_entry_call, timed_entry_call
             * Runtime: check open alternatives, wait or execute else */
            {
                uint32_t done_label = cg->label_id++;
                bool has_else = (node->select_stmt.else_part != NULL);
                bool has_delay = false;
                uint32_t delay_label = 0;

                /* Check for delay alternative */
                for (uint32_t i = 0; i < node->select_stmt.alternatives.count; i++) {
                    if (node->select_stmt.alternatives.items[i]->kind == NK_DELAY) {
                        has_delay = true;
                        delay_label = cg->label_id++;
                        break;
                    }
                }

                /* Generate alternatives */
                for (uint32_t i = 0; i < node->select_stmt.alternatives.count; i++) {
                    Syntax_Node *alt = node->select_stmt.alternatives.items[i];
                    uint32_t next_label = cg->label_id++;

                    switch (alt->kind) {
                        case NK_ASSOCIATION:
                            /* Guarded alternative: WHEN cond => stmt */
                            {
                                uint32_t guard = Generate_Expression(cg,
                                    alt->association.choices.items[0]);
                                Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
                                     guard, cg->label_id, next_label);
                                Emit(cg, "L%u:\n", cg->label_id++);
                                if (alt->association.expression)
                                    Generate_Statement(cg, alt->association.expression);
                                Emit(cg, "  br label %%L%u\n", done_label);
                            }
                            break;

                        case NK_ACCEPT:
                            /* Accept alternative */
                            Emit(cg, "  ; accept alternative: %.*s\n",
                                 (int)alt->accept_stmt.entry_name.length,
                                 alt->accept_stmt.entry_name.data);
                            Generate_Statement_List(cg, &alt->accept_stmt.statements);
                            Emit(cg, "  br label %%L%u\n", done_label);
                            break;

                        case NK_DELAY:
                            /* Delay alternative */
                            Emit(cg, "L%u:  ; delay alternative\n", delay_label);
                            {
                                uint32_t dur = Generate_Expression(cg, alt->delay_stmt.expression);
                                uint32_t us = Emit_Temp(cg);
                                Emit(cg, "  %%t%u = fmul double %%t%u, 1.0e6\n", us, dur);
                                uint32_t us_int = Emit_Temp(cg);
                                Emit(cg, "  %%t%u = fptoui double %%t%u to i64\n", us_int, us);
                                Emit(cg, "  call void @__ada_delay(i64 %%t%u)\n", us_int);
                            }
                            Emit(cg, "  br label %%L%u\n", done_label);
                            break;

                        case NK_NULL_STMT:
                            /* Terminate alternative */
                            Emit(cg, "  ; terminate alternative\n");
                            Emit(cg, "  call void @__ada_task_terminate()\n");
                            Emit(cg, "  br label %%L%u\n", done_label);
                            break;

                        default:
                            break;
                    }
                    Emit(cg, "L%u:\n", next_label);
                }

                /* Else clause or fall through to delay */
                if (has_else) {
                    Generate_Statement(cg, node->select_stmt.else_part);
                } else if (has_delay) {
                    Emit(cg, "  br label %%L%u\n", delay_label);
                }
                Emit(cg, "  br label %%L%u\n", done_label);
                Emit(cg, "L%u:\n", done_label);
            }
            break;

        case NK_ABORT:
            /* ABORT statement — abort named tasks (Ada 83 9.10) */
            for (uint32_t i = 0; i < node->abort_stmt.task_names.count; i++) {
                Syntax_Node *task_name = node->abort_stmt.task_names.items[i];
                uint32_t task_ptr = Generate_Expression(cg, task_name);
                Emit(cg, "  call void @__ada_task_abort(ptr %%t%u)\n", task_ptr);
            }
            break;

        default:
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.5 Declaration Code Generation
 * ───────────────────────────────────────────────────────────────────────── */

static void Generate_Declaration(Code_Generator *cg, Syntax_Node *node);
static void Emit_Extern_Subprogram(Code_Generator *cg, Symbol *sym);

static void Generate_Declaration_List(Code_Generator *cg, Node_List *list) {
    for (uint32_t i = 0; i < list->count; i++) {
        Generate_Declaration(cg, list->items[i]);
    }
}

static void Generate_Object_Declaration(Code_Generator *cg, Syntax_Node *node) {
    /* cg->current_nesting_level is repurposed: 1 = has nested functions, use frame */
    bool use_frame = cg->current_nesting_level > 0;
    bool is_package_level = (cg->current_function == NULL);

    for (uint32_t i = 0; i < node->object_decl.names.count; i++) {
        Syntax_Node *name = node->object_decl.names.items[i];
        Symbol *sym = name->symbol;
        if (!sym) continue;

        Type_Info *ty = sym->type;
        const char *type_str = Type_To_Llvm(ty);

        /* Check if this is a constrained array */
        bool is_array = ty && ty->kind == TYPE_ARRAY && ty->array.is_constrained;
        int64_t array_count = is_array ? Array_Element_Count(ty) : 0;
        const char *elem_type = is_array ? Type_To_Llvm(ty->array.element_type) : NULL;

        /* Check if this is a record type */
        bool is_record = ty && ty->kind == TYPE_RECORD;
        uint32_t record_size = is_record ? ty->size : 0;

        /* Package-level variables are globals, local variables use alloca */
        if (is_package_level) {
            /* Global variable at package level */
            Emit(cg, "@");
            Emit_Symbol_Name(cg, sym);
            if (node->object_decl.is_constant && node->object_decl.init) {
                /* Constant with initializer - emit as constant */
                if (node->object_decl.init->kind == NK_INTEGER) {
                    Emit(cg, " = linkonce_odr constant %s %lld\n", type_str,
                         (long long)node->object_decl.init->integer_lit.value);
                    continue;
                }
            }
            /* Variable - emit as global with default init */
            if (is_array && array_count > 0) {
                Emit(cg, " = linkonce_odr global [%lld x %s] zeroinitializer\n",
                     (long long)array_count, elem_type);
            } else if (is_record && record_size > 0) {
                Emit(cg, " = linkonce_odr global [%u x i8] zeroinitializer\n", record_size);
            } else {
                Emit(cg, " = linkonce_odr global %s 0\n", type_str);
            }
            continue;
        }

        /* Local variable allocation */
        if (use_frame) {
            Emit(cg, "  %%");
            Emit_Symbol_Name(cg, sym);
            Emit(cg, " = getelementptr i8, ptr %%__frame_base, i64 %lld\n",
                 (long long)sym->frame_offset);
        } else if (is_array && array_count > 0) {
            /* Constrained array: allocate [N x element_type] */
            Emit(cg, "  %%");
            Emit_Symbol_Name(cg, sym);
            Emit(cg, " = alloca [%lld x %s]\n", (long long)array_count, elem_type);
        } else if (is_record && record_size > 0) {
            /* Record type: allocate [N x i8] for the record size */
            Emit(cg, "  %%");
            Emit_Symbol_Name(cg, sym);
            Emit(cg, " = alloca [%u x i8]  ; record type\n", record_size);
        } else {
            Emit(cg, "  %%");
            Emit_Symbol_Name(cg, sym);
            Emit(cg, " = alloca %s\n", type_str);
        }

        /* Initialize if provided */
        if (node->object_decl.init) {
            if (is_array && ty->array.element_type == cg->sm->type_character) {
                /* String/character array initialization - copy fat pointer data */
                uint32_t fat_ptr = Generate_Expression(cg, node->object_decl.init);
                Emit_Fat_Pointer_Copy_To_Name(cg, fat_ptr, sym);
            } else if (!is_array && !is_record) {
                uint32_t init = Generate_Expression(cg, node->object_decl.init);
                /* Truncate from i64 computation to storage type */
                init = Emit_Convert(cg, init, "i64", type_str);
                Emit(cg, "  store %s %%t%u, ptr %%", type_str, init);
                Emit_Symbol_Name(cg, sym);
                Emit(cg, "\n");
            }
        }
    }
}

static void Generate_Subprogram_Body(Code_Generator *cg, Syntax_Node *node) {
    Syntax_Node *spec = node->subprogram_body.specification;
    Symbol *sym = spec ? spec->symbol : NULL;
    if (!sym) return;

    bool is_function = sym->kind == SYMBOL_FUNCTION;
    uint32_t saved_deferred_count = cg->deferred_count;

    /* Check if this is a nested function (has enclosing function) */
    Symbol *saved_enclosing = cg->enclosing_function;
    bool saved_is_nested = cg->is_nested;
    Symbol *parent_owner = sym->parent;

    /* Determine if nested: parent is a function/procedure */
    bool is_nested = parent_owner &&
                     (parent_owner->kind == SYMBOL_FUNCTION ||
                      parent_owner->kind == SYMBOL_PROCEDURE);
    cg->is_nested = is_nested;
    cg->enclosing_function = is_nested ? parent_owner : NULL;

    /* Function header */
    Emit(cg, "define %s @", is_function ? Type_To_Llvm(sym->return_type) : "void");
    Emit_Symbol_Name(cg, sym);
    Emit(cg, "(");

    /* If nested, first parameter is the frame pointer */
    if (is_nested) {
        Emit(cg, "ptr %%__parent_frame");
        if (sym->parameter_count > 0) Emit(cg, ", ");
    }

    /* Parameters */
    for (uint32_t i = 0; i < sym->parameter_count; i++) {
        if (i > 0) Emit(cg, ", ");
        /* OUT and IN OUT parameters are passed by reference */
        if (Param_Is_By_Reference(sym->parameters[i].mode)) {
            Emit(cg, "ptr %%p%u", i);
        } else {
            Emit(cg, "%s %%p%u", Type_To_Llvm(sym->parameters[i].param_type), i);
        }
    }

    Emit(cg, ") {\n");
    Emit(cg, "entry:\n");

    Symbol *saved_current_function = cg->current_function;
    cg->current_function = sym;
    cg->has_return = false;

    /* Check if this function has nested subprograms (by looking at declarations) */
    bool has_nested = false;
    for (uint32_t i = 0; i < node->subprogram_body.declarations.count; i++) {
        Syntax_Node *decl = node->subprogram_body.declarations.items[i];
        if (decl && (decl->kind == NK_PROCEDURE_BODY || decl->kind == NK_FUNCTION_BODY)) {
            has_nested = true;
            break;
        }
    }

    /* If this function has nested subprograms, allocate a frame base
     * This is the address that will be passed to nested functions */
    if (has_nested && sym->scope && sym->scope->frame_size > 0) {
        Emit(cg, "  ; Frame for nested function access\n");
        Emit(cg, "  %%__frame_base = alloca i8, i64 %lld\n",
             (long long)sym->scope->frame_size);
    }

    /* If nested, create aliases for accessing enclosing scope variables via frame */
    if (is_nested && parent_owner && parent_owner->scope) {
        /* Create pointer aliases to parent scope variables */
        Scope *parent_scope = parent_owner->scope;
        for (uint32_t i = 0; i < parent_scope->symbol_count; i++) {
            Symbol *var = parent_scope->symbols[i];
            if (var && (var->kind == SYMBOL_VARIABLE || var->kind == SYMBOL_PARAMETER)) {
                /* Create a GEP alias:  %__frame.VAR = getelementptr i8, ptr %__parent_frame, i64 offset */
                Emit(cg, "  %%__frame.");
                Emit_Symbol_Name(cg, var);
                Emit(cg, " = getelementptr i8, ptr %%__parent_frame, i64 %lld\n",
                     (long long)(var->frame_offset));
            }
        }
    }

    /* Allocate and store parameters to local stack slots
     * For OUT/IN OUT: param is already a pointer, use directly
     * For IN: allocate local slot and copy value */
    for (uint32_t i = 0; i < sym->parameter_count; i++) {
        Symbol *param_sym = sym->parameters[i].param_sym;
        if (param_sym) {
            const char *type_str = Type_To_Llvm(sym->parameters[i].param_type);
            Parameter_Mode mode = sym->parameters[i].mode;

            if (Param_Is_By_Reference(mode)) {
                /* OUT/IN OUT: %p is already a pointer to caller's variable
                 * Create an alias so the parameter name points to caller's storage */
                Emit(cg, "  %%");
                Emit_Symbol_Name(cg, param_sym);
                Emit(cg, " = getelementptr i8, ptr %%p%u, i64 0  ; by-ref param\n", i);
            } else if (has_nested && sym->scope) {
                /* IN param with nested functions: allocate in frame */
                Emit(cg, "  %%");
                Emit_Symbol_Name(cg, param_sym);
                Emit(cg, " = getelementptr i8, ptr %%__frame_base, i64 %lld\n",
                     (long long)param_sym->frame_offset);
                Emit(cg, "  store %s %%p%u, ptr %%", type_str, i);
                Emit_Symbol_Name(cg, param_sym);
                Emit(cg, "\n");
            } else {
                /* IN param: allocate local and copy value */
                Emit(cg, "  %%");
                Emit_Symbol_Name(cg, param_sym);
                Emit(cg, " = alloca %s\n", type_str);
                Emit(cg, "  store %s %%p%u, ptr %%", type_str, i);
                Emit_Symbol_Name(cg, param_sym);
                Emit(cg, "\n");
            }
        }
    }

    /* Generate local declarations, passing has_nested flag via cg for frame allocation */
    bool saved_has_nested = cg->current_nesting_level > 0;  /* Repurpose field temporarily */
    cg->current_nesting_level = has_nested ? 1 : 0;
    Generate_Declaration_List(cg, &node->subprogram_body.declarations);
    cg->current_nesting_level = saved_has_nested;

    /* Check if subprogram has exception handlers */
    bool has_exc_handlers = node->subprogram_body.handlers.count > 0;

    if (has_exc_handlers) {
        /* Setup exception handling using setjmp/longjmp */
        uint32_t jmp_buf = Emit_Temp(cg);
        uint32_t handler_label = Emit_Label(cg);
        uint32_t normal_label = Emit_Label(cg);
        uint32_t end_label = Emit_Label(cg);

        /* Allocate jmp_buf (200 bytes for safety) */
        Emit(cg, "  %%t%u = alloca [200 x i8], align 16  ; jmp_buf\n", jmp_buf);

        /* Push exception handler */
        Emit(cg, "  call void @__ada_push_handler(ptr %%t%u)\n", jmp_buf);

        /* Call setjmp */
        uint32_t setjmp_result = Emit_Temp(cg);
        Emit(cg, "  %%t%u = call i32 @setjmp(ptr %%t%u)\n", setjmp_result, jmp_buf);

        /* Branch based on setjmp return */
        uint32_t is_normal = Emit_Temp(cg);
        Emit(cg, "  %%t%u = icmp eq i32 %%t%u, 0\n", is_normal, setjmp_result);
        Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
             is_normal, normal_label, handler_label);

        /* Normal execution path */
        Emit(cg, "L%u:\n", normal_label);

        /* Generate statements */
        Generate_Statement_List(cg, &node->subprogram_body.statements);

        /* Pop handler on normal exit */
        Emit(cg, "  call void @__ada_pop_handler()\n");
        Emit(cg, "  br label %%L%u\n", end_label);

        /* Exception handler entry */
        Emit(cg, "L%u:\n", handler_label);
        Emit(cg, "  call void @__ada_pop_handler()\n");

        /* Get current exception identity */
        uint32_t exc_id = Emit_Temp(cg);
        Emit(cg, "  %%t%u = call i64 @__ada_current_exception()\n", exc_id);

        /* Generate exception handlers */
        uint32_t next_handler = 0;
        for (uint32_t i = 0; i < node->subprogram_body.handlers.count; i++) {
            Syntax_Node *handler = node->subprogram_body.handlers.items[i];
            if (!handler) continue;

            if (next_handler != 0) {
                Emit(cg, "L%u:\n", next_handler);
            }
            next_handler = Emit_Label(cg);
            uint32_t handler_body = Emit_Label(cg);

            /* Check each exception name in the handler */
            bool has_others = false;
            for (uint32_t j = 0; j < handler->handler.exceptions.count; j++) {
                Syntax_Node *exc_name = handler->handler.exceptions.items[j];
                if (exc_name->kind == NK_OTHERS) {
                    has_others = true;
                    break;
                }
            }

            if (has_others) {
                /* WHEN OTHERS => catches all */
                Emit(cg, "  br label %%L%u\n", handler_body);
            } else {
                /* Check against specific exceptions */
                for (uint32_t j = 0; j < handler->handler.exceptions.count; j++) {
                    Syntax_Node *exc_name = handler->handler.exceptions.items[j];
                    if (exc_name->symbol) {
                        uint32_t exc_ptr = Emit_Temp(cg);
                        Emit(cg, "  %%t%u = ptrtoint ptr @__exc.", exc_ptr);
                        Emit_Symbol_Name(cg, exc_name->symbol);
                        Emit(cg, " to i64\n");
                        uint32_t match = Emit_Temp(cg);
                        Emit(cg, "  %%t%u = icmp eq i64 %%t%u, %%t%u\n",
                             match, exc_id, exc_ptr);
                        Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
                             match, handler_body, next_handler);
                    }
                }
            }

            /* Handler body */
            Emit(cg, "L%u:\n", handler_body);
            Generate_Statement_List(cg, &handler->handler.statements);
            Emit(cg, "  br label %%L%u\n", end_label);
        }

        /* If no handler matched, reraise */
        if (next_handler != 0) {
            Emit(cg, "L%u:\n", next_handler);
            Emit(cg, "  call void @__ada_reraise()\n");
            Emit(cg, "  unreachable\n");
        }

        /* End label - normal return point */
        Emit(cg, "L%u:\n", end_label);
    } else {
        /* Generate statements without exception handling */
        Generate_Statement_List(cg, &node->subprogram_body.statements);
    }

    /* Default return if no explicit return was emitted */
    if (!cg->has_return) {
        if (is_function) {
            Emit(cg, "  ret %s 0\n", Type_To_Llvm(sym->return_type));
        } else {
            Emit(cg, "  ret void\n");
        }
    }

    Emit(cg, "}\n\n");
    cg->current_function = saved_current_function;
    cg->is_nested = saved_is_nested;
    cg->enclosing_function = saved_enclosing;

    /* Emit deferred nested subprogram bodies */
    while (cg->deferred_count > saved_deferred_count) {
        Syntax_Node *deferred = cg->deferred_bodies[--cg->deferred_count];
        if (deferred->kind == NK_GENERIC_INST) {
            /* Handle deferred generic instance */
            Symbol *inst = deferred->symbol;
            if (inst && inst->generic_template && inst->generic_template->generic_body) {
                Symbol *saved = cg->current_instance;
                cg->current_instance = inst;
                Generate_Generic_Instance_Body(cg, inst, inst->generic_template->generic_body);
                cg->current_instance = saved;
            }
        } else {
            Generate_Subprogram_Body(cg, deferred);
        }
    }
}

/* Generate code for a generic instance body */
static void Generate_Generic_Instance_Body(Code_Generator *cg, Symbol *inst_sym,
                                           Syntax_Node *template_body) {
    if (!inst_sym || !template_body) return;

    bool is_function = inst_sym->kind == SYMBOL_FUNCTION;
    uint32_t saved_deferred_count = cg->deferred_count;

    /* For generic instances, determine nesting from instance parent */
    Symbol *saved_enclosing = cg->enclosing_function;
    bool saved_is_nested = cg->is_nested;
    Symbol *parent_owner = inst_sym->parent;

    bool is_nested = parent_owner &&
                     (parent_owner->kind == SYMBOL_FUNCTION ||
                      parent_owner->kind == SYMBOL_PROCEDURE);
    cg->is_nested = is_nested;
    cg->enclosing_function = is_nested ? parent_owner : NULL;

    /* Function header */
    Emit(cg, "define %s @", is_function ? Type_To_Llvm(inst_sym->return_type) : "void");
    Emit_Symbol_Name(cg, inst_sym);
    Emit(cg, "(");

    /* If nested, first parameter is the frame pointer */
    if (is_nested) {
        Emit(cg, "ptr %%__parent_frame");
        if (inst_sym->parameter_count > 0) Emit(cg, ", ");
    }

    /* Parameters - use instance symbol's types (already substituted) */
    for (uint32_t i = 0; i < inst_sym->parameter_count; i++) {
        if (i > 0) Emit(cg, ", ");
        Emit(cg, "%s %%p%u", Type_To_Llvm(inst_sym->parameters[i].param_type), i);
    }

    Emit(cg, ") {\n");
    Emit(cg, "entry:\n");

    Symbol *saved_current_function = cg->current_function;
    cg->current_function = inst_sym;
    cg->has_return = false;

    /* Allocate parameters using symbol names for consistent access */
    for (uint32_t i = 0; i < inst_sym->parameter_count; i++) {
        Symbol *param_sym = inst_sym->parameters[i].param_sym;
        if (param_sym) {
            const char *type_str = Type_To_Llvm(inst_sym->parameters[i].param_type);
            /* Allocate parameter with its symbol name */
            Emit(cg, "  %%");
            Emit_Symbol_Name(cg, param_sym);
            Emit(cg, " = alloca %s\n", type_str);
            Emit(cg, "  store %s %%p%u, ptr %%", type_str, i);
            Emit_Symbol_Name(cg, param_sym);
            Emit(cg, "\n");
        }
    }

    /* Generate the body statements with type substitution */
    Syntax_Node *body = template_body;

    /* Generate local declarations */
    Generate_Declaration_List(cg, &body->subprogram_body.declarations);

    /* Generate statements from template body */
    for (uint32_t i = 0; i < body->subprogram_body.statements.count; i++) {
        Syntax_Node *stmt = body->subprogram_body.statements.items[i];
        if (stmt) {
            Generate_Statement(cg, stmt);
        }
    }

    /* Default return if no explicit return */
    if (!cg->has_return) {
        if (is_function) {
            /* For function, return parameter 0 (this is simplified - real implementation
               would need to evaluate the return expression with substitution) */
            const char *ret_type = Type_To_Llvm(inst_sym->return_type);
            uint32_t t = Emit_Temp(cg);
            Emit(cg, "  %%t%u = load %s, ptr %%param0\n", t, ret_type);
            Emit(cg, "  ret %s %%t%u\n", ret_type, t);
        } else {
            Emit(cg, "  ret void\n");
        }
    }

    Emit(cg, "}\n\n");
    cg->current_function = saved_current_function;
    cg->is_nested = saved_is_nested;
    cg->enclosing_function = saved_enclosing;

    /* Process deferred bodies */
    while (cg->deferred_count > saved_deferred_count) {
        Syntax_Node *deferred = cg->deferred_bodies[--cg->deferred_count];
        if (deferred->kind == NK_GENERIC_INST) {
            Symbol *inst = deferred->symbol;
            if (inst && inst->generic_template && inst->generic_template->generic_body) {
                Symbol *saved = cg->current_instance;
                cg->current_instance = inst;
                Generate_Generic_Instance_Body(cg, inst, inst->generic_template->generic_body);
                cg->current_instance = saved;
            }
        } else {
            Generate_Subprogram_Body(cg, deferred);
        }
    }
}

static void Generate_Declaration(Code_Generator *cg, Syntax_Node *node) {
    if (!node) return;

    switch (node->kind) {
        case NK_OBJECT_DECL:
            Generate_Object_Declaration(cg, node);
            break;

        case NK_PROCEDURE_SPEC:
        case NK_FUNCTION_SPEC:
            /* Forward declaration - if imported, emit extern declaration */
            if (node->symbol && node->symbol->is_imported) {
                Emit_Extern_Subprogram(cg, node->symbol);
            }
            break;

        case NK_PROCEDURE_BODY:
        case NK_FUNCTION_BODY:
            /* Defer nested subprogram bodies - emit after enclosing function */
            if (cg->current_function && cg->deferred_count < 64) {
                cg->deferred_bodies[cg->deferred_count++] = node;
            } else {
                Generate_Subprogram_Body(cg, node);
            }
            break;

        case NK_PACKAGE_BODY:
            Generate_Declaration_List(cg, &node->package_body.declarations);
            /* Generate initialization sequence if present */
            if (node->package_body.statements.count > 0) {
                Generate_Statement_List(cg, &node->package_body.statements);
            }
            break;

        case NK_GENERIC_INST:
            /* Generate the instantiated generic body */
            {
                Symbol *inst_sym = node->symbol;
                if (!inst_sym || !inst_sym->generic_template) break;

                Symbol *template = inst_sym->generic_template;
                Syntax_Node *generic_body = template->generic_body;
                if (!generic_body) break;

                /* Store current instance for type substitution during codegen */
                Symbol *saved_instance = cg->current_instance;
                cg->current_instance = inst_sym;

                /* Generate instantiated body using the instance's symbol */
                if (cg->current_function && cg->deferred_count < 64) {
                    /* Defer nested generic instance - emit after enclosing function */
                    cg->deferred_bodies[cg->deferred_count++] = node;
                } else {
                    Generate_Generic_Instance_Body(cg, inst_sym, generic_body);
                }

                cg->current_instance = saved_instance;
            }
            break;

        case NK_GENERIC_DECL:
            /* Generic declarations don't generate code - only instances do */
            break;

        case NK_TASK_SPEC:
            /* Task type/object specification - record entry points */
            Emit(cg, "; Task spec: %.*s (entries registered at runtime)\n",
                 (int)node->task_spec.name.length, node->task_spec.name.data);
            break;

        case NK_TASK_BODY:
            /* Task body - generate as a void function with trampoline wrapper */
            {
                Emit(cg, "\n; Task body: %.*s\n",
                     (int)node->task_body.name.length, node->task_body.name.data);

                /* Generate task entry point function */
                Emit(cg, "define void @task_%.*s(ptr %%_unused_arg) {\n",
                     (int)node->task_body.name.length, node->task_body.name.data);
                Emit(cg, "entry:\n");

                /* Push exception handler for task */
                uint32_t jmp_buf = Emit_Temp(cg);
                Emit(cg, "  %%t%u = alloca [200 x i8]\n", jmp_buf);
                uint32_t jmp_ptr = Emit_Temp(cg);
                Emit(cg, "  %%t%u = getelementptr [200 x i8], ptr %%t%u, i64 0, i64 0\n",
                     jmp_ptr, jmp_buf);
                Emit(cg, "  call void @__ada_push_handler(ptr %%t%u)\n", jmp_ptr);
                uint32_t setjmp_result = Emit_Temp(cg);
                Emit(cg, "  %%t%u = call i32 @setjmp(ptr %%t%u)\n", setjmp_result, jmp_ptr);
                uint32_t is_zero = Emit_Temp(cg);
                Emit(cg, "  %%t%u = icmp eq i32 %%t%u, 0\n", is_zero, setjmp_result);
                uint32_t body_label = Emit_Label(cg);
                uint32_t exit_label = Emit_Label(cg);
                Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n",
                     is_zero, body_label, exit_label);

                /* Normal execution path */
                Emit(cg, "L%u:\n", body_label);
                Generate_Declaration_List(cg, &node->task_body.declarations);
                Generate_Statement_List(cg, &node->task_body.statements);
                Emit(cg, "  call void @__ada_pop_handler()\n");
                Emit(cg, "  ret void\n");

                /* Exception handler path */
                Emit(cg, "L%u:\n", exit_label);
                Emit(cg, "  call void @__ada_pop_handler()\n");
                /* Task terminates silently on unhandled exception */
                Emit(cg, "  ret void\n");

                Emit(cg, "}\n\n");
            }
            break;

        default:
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.6 Implicit Equality Function Generation
 *
 * Generate equality functions for composite types at freeze points.
 * Per RM 4.5.2, equality is predefined for all non-limited types.
 * ───────────────────────────────────────────────────────────────────────── */

static void Generate_Type_Equality_Function(Code_Generator *cg, Type_Info *t) {
    if (!t || !t->equality_func_name) return;

    const char *func_name = t->equality_func_name;

    /* Determine parameter type based on array constrained-ness */
    bool is_unconstrained = (t->kind == TYPE_ARRAY || t->kind == TYPE_STRING) &&
                            !t->array.is_constrained;
    const char *param_type = is_unconstrained ? FAT_PTR_TYPE : "ptr";

    /* Emit function definition with linkonce_odr for linker deduplication */
    Emit(cg, "\n; Implicit equality for type %.*s\n",
         (int)t->name.length, t->name.data);
    Emit(cg, "define linkonce_odr i1 @%s(%s %%0, %s %%1) {\n", func_name, param_type, param_type);
    Emit(cg, "entry:\n");

    /* Save and reset temp counter for this function */
    uint32_t saved_temp = cg->temp_id;
    cg->temp_id = 2;  /* Start after %0 and %1 */

    if (t->kind == TYPE_RECORD) {
        if (t->record.component_count == 0) {
            /* Empty record - always equal */
            Emit(cg, "  ret i1 1\n");
        } else {
            uint32_t result = 0;
            for (uint32_t i = 0; i < t->record.component_count; i++) {
                Component_Info *comp = &t->record.components[i];
                const char *comp_llvm_type = Type_To_Llvm(comp->component_type);

                /* Get pointers to components */
                uint32_t left_gep = Emit_Temp(cg);
                uint32_t right_gep = Emit_Temp(cg);
                Emit(cg, "  %%t%u = getelementptr i8, ptr %%0, i64 %u\n",
                     left_gep, comp->byte_offset);
                Emit(cg, "  %%t%u = getelementptr i8, ptr %%1, i64 %u\n",
                     right_gep, comp->byte_offset);

                /* Load component values */
                uint32_t left_val = Emit_Temp(cg);
                uint32_t right_val = Emit_Temp(cg);
                Emit(cg, "  %%t%u = load %s, ptr %%t%u\n",
                     left_val, comp_llvm_type, left_gep);
                Emit(cg, "  %%t%u = load %s, ptr %%t%u\n",
                     right_val, comp_llvm_type, right_gep);

                /* Compare */
                uint32_t cmp = Emit_Temp(cg);
                if (Type_Is_Real(comp->component_type)) {
                    Emit(cg, "  %%t%u = fcmp oeq %s %%t%u, %%t%u\n",
                         cmp, comp_llvm_type, left_val, right_val);
                } else {
                    Emit(cg, "  %%t%u = icmp eq %s %%t%u, %%t%u\n",
                         cmp, comp_llvm_type, left_val, right_val);
                }

                /* AND with previous results */
                if (i == 0) {
                    result = cmp;
                } else {
                    uint32_t and_result = Emit_Temp(cg);
                    Emit(cg, "  %%t%u = and i1 %%t%u, %%t%u\n",
                         and_result, result, cmp);
                    result = and_result;
                }
            }
            Emit(cg, "  ret i1 %%t%u\n", result);
        }
    } else if (t->kind == TYPE_ARRAY || t->kind == TYPE_STRING) {
        if (t->array.is_constrained) {
            /* Constrained array - use memcmp */
            int64_t count = Array_Element_Count(t);
            uint32_t elem_size = t->array.element_type ?
                                 t->array.element_type->size : 4;
            int64_t total_size = count * elem_size;

            uint32_t result = Emit_Temp(cg);
            uint32_t cmp = Emit_Temp(cg);
            Emit(cg, "  %%t%u = call i32 @memcmp(ptr %%0, ptr %%1, i64 %lld)\n",
                 result, (long long)total_size);
            Emit(cg, "  %%t%u = icmp eq i32 %%t%u, 0\n", cmp, result);
            Emit(cg, "  ret i1 %%t%u\n", cmp);
        } else {
            /*
             * Unconstrained array equality (per RM 4.5.2):
             * Fat pointer layout: { ptr data, { i64 low, i64 high } }
             * Compare lengths first, then data if lengths match.
             */
            uint32_t elem_size = t->array.element_type ?
                                 t->array.element_type->size : 1;

            /* Extract bounds from first fat pointer (%0) */
            Emit(cg, "  %%left_low = extractvalue " FAT_PTR_TYPE " %%0, 1, 0\n");
            Emit(cg, "  %%left_high = extractvalue " FAT_PTR_TYPE " %%0, 1, 1\n");
            Emit(cg, "  %%left_len = sub i64 %%left_high, %%left_low\n");
            Emit(cg, "  %%left_len1 = add i64 %%left_len, 1\n");

            /* Extract bounds from second fat pointer (%1) */
            Emit(cg, "  %%right_low = extractvalue " FAT_PTR_TYPE " %%1, 1, 0\n");
            Emit(cg, "  %%right_high = extractvalue " FAT_PTR_TYPE " %%1, 1, 1\n");
            Emit(cg, "  %%right_len = sub i64 %%right_high, %%right_low\n");
            Emit(cg, "  %%right_len1 = add i64 %%right_len, 1\n");

            /* Compare lengths */
            Emit(cg, "  %%len_eq = icmp eq i64 %%left_len1, %%right_len1\n");

            /* Extract data pointers */
            Emit(cg, "  %%left_data = extractvalue " FAT_PTR_TYPE " %%0, 0\n");
            Emit(cg, "  %%right_data = extractvalue " FAT_PTR_TYPE " %%1, 0\n");

            /* Compute byte size and call memcmp */
            Emit(cg, "  %%byte_size = mul i64 %%left_len1, %u\n", elem_size);
            Emit(cg, "  %%memcmp_res = call i32 @memcmp(ptr %%left_data, ptr %%right_data, i64 %%byte_size)\n");
            Emit(cg, "  %%data_eq = icmp eq i32 %%memcmp_res, 0\n");

            /* Result: lengths match AND data matches */
            Emit(cg, "  %%result = and i1 %%len_eq, %%data_eq\n");
            Emit(cg, "  ret i1 %%result\n");
        }
    } else {
        /* Unknown composite type - return true */
        Emit(cg, "  ret i1 1\n");
    }

    Emit(cg, "}\n");
    cg->temp_id = saved_temp;  /* Restore temp counter */
}

static void Generate_Implicit_Operators(Code_Generator *cg) {
    /* Generate equality functions for all frozen composite types */
    for (uint32_t i = 0; i < Frozen_Composite_Count; i++) {
        Generate_Type_Equality_Function(cg, Frozen_Composite_Types[i]);
    }
}

/* Generate global constants for exception identities */
static void Generate_Exception_Globals(Code_Generator *cg) {
    /* Generate globals for all registered exceptions */
    if (Exception_Symbol_Count > 0) {
        Emit(cg, "; Exception identity globals\n");
        for (uint32_t i = 0; i < Exception_Symbol_Count; i++) {
            Symbol *sym = Exception_Symbols[i];
            Emit(cg, "@__exc.");
            Emit_Symbol_Name(cg, sym);
            Emit(cg, " = private constant i8 0\n");
        }
        Emit(cg, "\n");
    }
}

/* Check if external name is a builtin text_io function (already defined) */
static bool Is_Builtin_Function(String_Slice name) {
    /* Strip quotes if present */
    if (name.length >= 2 && name.data[0] == '"' && name.data[name.length-1] == '"') {
        name.data++;
        name.length -= 2;
    }
    const char *builtins[] = {
        "__text_io_new_line", "__text_io_put_char", "__text_io_put",
        "__text_io_put_line", "__text_io_put_int", "__text_io_put_float",
        "__text_io_get_char", "__text_io_get_line", NULL
    };
    for (int i = 0; builtins[i]; i++) {
        if (name.length == strlen(builtins[i]) &&
            memcmp(name.data, builtins[i], name.length) == 0) {
            return true;
        }
    }
    return false;
}

/* Emit function signature for extern declaration */
static void Emit_Extern_Subprogram(Code_Generator *cg, Symbol *sym) {
    if (!sym) return;
    if (sym->kind != SYMBOL_FUNCTION && sym->kind != SYMBOL_PROCEDURE) return;

    /* Skip if already emitted */
    if (sym->extern_emitted) return;
    sym->extern_emitted = true;

    /* Skip if this is a builtin function that we've already defined */
    if (sym->is_imported && sym->external_name.length > 0) {
        if (Is_Builtin_Function(sym->external_name)) {
            return;
        }
    }

    /* Get return type */
    const char *ret_type = sym->return_type ? Type_To_Llvm(sym->return_type) : "void";

    Emit(cg, "declare %s @", ret_type);
    Emit_Symbol_Name(cg, sym);
    Emit(cg, "(");

    /* Emit parameter types */
    for (uint32_t i = 0; i < sym->parameter_count; i++) {
        if (i > 0) Emit(cg, ", ");
        Type_Info *ty = sym->parameters[i].param_type;
        if (ty) {
            /* Unconstrained arrays pass as fat pointers */
            if ((ty->kind == TYPE_ARRAY && !ty->array.is_constrained) ||
                ty->kind == TYPE_STRING) {
                Emit(cg, FAT_PTR_TYPE);
            } else {
                Emit(cg, "%s", Type_To_Llvm(ty));
            }
        } else {
            Emit(cg, "i64");
        }
    }
    Emit(cg, ")\n");
}

/* Generate extern declarations for all loaded package specs */
static void Generate_Extern_Declarations(Code_Generator *cg, Syntax_Node *node) {
    if (!node || !node->compilation_unit.context) return;

    Syntax_Node *ctx = node->compilation_unit.context;
    bool emitted_header = false;

    /* Iterate through WITH'd packages */
    for (uint32_t i = 0; i < ctx->context.with_clauses.count; i++) {
        Syntax_Node *with_node = ctx->context.with_clauses.items[i];
        for (uint32_t j = 0; j < with_node->use_clause.names.count; j++) {
            Syntax_Node *pkg_name = with_node->use_clause.names.items[j];
            if (pkg_name->kind != NK_IDENTIFIER) continue;

            Symbol *pkg_sym = pkg_name->symbol;
            if (!pkg_sym || pkg_sym->kind != SYMBOL_PACKAGE) continue;
            if (!pkg_sym->declaration) continue;

            Syntax_Node *pkg_decl = pkg_sym->declaration;
            if (pkg_decl->kind != NK_PACKAGE_SPEC) continue;

            /* Emit extern for each subprogram in the package */
            for (uint32_t k = 0; k < pkg_decl->package_spec.visible_decls.count; k++) {
                Syntax_Node *decl = pkg_decl->package_spec.visible_decls.items[k];
                if (!decl) continue;

                if (decl->kind == NK_PROCEDURE_SPEC || decl->kind == NK_FUNCTION_SPEC) {
                    if (!emitted_header) {
                        Emit(cg, "\n; External Ada subprogram declarations\n");
                        emitted_header = true;
                    }
                    Emit_Extern_Subprogram(cg, decl->symbol);
                }
            }
        }
    }
    if (emitted_header) Emit(cg, "\n");
}

/* ─────────────────────────────────────────────────────────────────────────
 * §13.7 Compilation Unit Code Generation
 * ───────────────────────────────────────────────────────────────────────── */

static void Generate_Compilation_Unit(Code_Generator *cg, Syntax_Node *node) {
    if (!node) return;

    /* Generate LLVM module header */
    Emit(cg, "; Ada83 Compiler Output\n");
    Emit(cg, "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n");
    Emit(cg, "target triple = \"x86_64-pc-linux-gnu\"\n\n");

    /* External C library and LLVM intrinsic declarations */
    Emit(cg, "; External declarations\n");
    Emit(cg, "declare i32 @memcmp(ptr, ptr, i64)\n");
    Emit(cg, "declare i32 @setjmp(ptr)\n");
    Emit(cg, "declare void @longjmp(ptr, i32)\n");
    Emit(cg, "declare void @exit(i32)\n");
    Emit(cg, "declare ptr @malloc(i64)\n");
    Emit(cg, "declare ptr @realloc(ptr, i64)\n");
    Emit(cg, "declare void @free(ptr)\n");
    Emit(cg, "declare i32 @usleep(i32)\n");
    Emit(cg, "declare i32 @printf(ptr, ...)\n");
    Emit(cg, "declare i32 @putchar(i32)\n");
    Emit(cg, "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)\n\n");

    /* Runtime globals */
    Emit(cg, "; Runtime globals\n");
    Emit(cg, "@__ss_base = linkonce_odr global ptr null\n");
    Emit(cg, "@__ss_ptr = linkonce_odr global i64 0\n");
    Emit(cg, "@__ss_size = linkonce_odr global i64 0\n");
    Emit(cg, "@__eh_cur = linkonce_odr global ptr null\n");
    Emit(cg, "@__ex_cur = linkonce_odr global i64 0\n");
    Emit(cg, "@__fin_list = linkonce_odr global ptr null\n");
    Emit(cg, "@.fmt_ue = linkonce_odr constant [27 x i8] c\"Unhandled exception: %%lld\\0A\\00\"\n\n");

    /* Standard exceptions (RM 11.1) */
    Emit(cg, "; Standard exception identities\n");
    Emit(cg, "@.ex.CONSTRAINT_ERROR = linkonce_odr constant i64 1\n");
    Emit(cg, "@.ex.NUMERIC_ERROR = linkonce_odr constant i64 2\n");
    Emit(cg, "@.ex.PROGRAM_ERROR = linkonce_odr constant i64 3\n");
    Emit(cg, "@.ex.STORAGE_ERROR = linkonce_odr constant i64 4\n");
    Emit(cg, "@.ex.TASKING_ERROR = linkonce_odr constant i64 5\n\n");

    /* Secondary stack initialization */
    Emit(cg, "; Secondary stack runtime\n");
    Emit(cg, "define linkonce_odr void @__ada_ss_init() {\n");
    Emit(cg, "  %%p = call ptr @malloc(i64 1048576)\n");
    Emit(cg, "  store ptr %%p, ptr @__ss_base\n");
    Emit(cg, "  store i64 1048576, ptr @__ss_size\n");
    Emit(cg, "  store i64 0, ptr @__ss_ptr\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* Secondary stack mark */
    Emit(cg, "define linkonce_odr i64 @__ada_sec_stack_mark() {\n");
    Emit(cg, "  %%m = load i64, ptr @__ss_ptr\n");
    Emit(cg, "  ret i64 %%m\n");
    Emit(cg, "}\n\n");

    /* Secondary stack release */
    Emit(cg, "define linkonce_odr void @__ada_sec_stack_release(i64 %%m) {\n");
    Emit(cg, "  store i64 %%m, ptr @__ss_ptr\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* Secondary stack allocate */
    Emit(cg, "define linkonce_odr ptr @__ada_sec_stack_alloc(i64 %%sz) {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%1 = load ptr, ptr @__ss_base\n");
    Emit(cg, "  %%2 = icmp eq ptr %%1, null\n");
    Emit(cg, "  br i1 %%2, label %%init, label %%alloc\n");
    Emit(cg, "init:\n");
    Emit(cg, "  call void @__ada_ss_init()\n");
    Emit(cg, "  %%3 = load ptr, ptr @__ss_base\n");
    Emit(cg, "  br label %%alloc\n");
    Emit(cg, "alloc:\n");
    Emit(cg, "  %%p = phi ptr [%%1, %%entry], [%%3, %%init]\n");
    Emit(cg, "  %%4 = load i64, ptr @__ss_ptr\n");
    Emit(cg, "  %%5 = add i64 %%sz, 7\n");
    Emit(cg, "  %%6 = and i64 %%5, -8\n");
    Emit(cg, "  %%7 = add i64 %%4, %%6\n");
    Emit(cg, "  %%8 = load i64, ptr @__ss_size\n");
    Emit(cg, "  %%9 = icmp ult i64 %%7, %%8\n");
    Emit(cg, "  br i1 %%9, label %%ok, label %%grow\n");
    Emit(cg, "grow:\n");
    Emit(cg, "  %%10 = mul i64 %%8, 2\n");
    Emit(cg, "  store i64 %%10, ptr @__ss_size\n");
    Emit(cg, "  %%11 = call ptr @realloc(ptr %%p, i64 %%10)\n");
    Emit(cg, "  store ptr %%11, ptr @__ss_base\n");
    Emit(cg, "  br label %%ok\n");
    Emit(cg, "ok:\n");
    Emit(cg, "  %%12 = phi ptr [%%p, %%alloc], [%%11, %%grow]\n");
    Emit(cg, "  %%13 = getelementptr i8, ptr %%12, i64 %%4\n");
    Emit(cg, "  store i64 %%7, ptr @__ss_ptr\n");
    Emit(cg, "  ret ptr %%13\n");
    Emit(cg, "}\n\n");

    /* Exception handling: push handler */
    Emit(cg, "; Exception handling runtime\n");
    Emit(cg, "define linkonce_odr void @__ada_push_handler(ptr %%h) {\n");
    Emit(cg, "  %%1 = load ptr, ptr @__eh_cur\n");
    Emit(cg, "  store ptr %%1, ptr %%h\n");
    Emit(cg, "  store ptr %%h, ptr @__eh_cur\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* Exception handling: pop handler */
    Emit(cg, "define linkonce_odr void @__ada_pop_handler() {\n");
    Emit(cg, "  %%1 = load ptr, ptr @__eh_cur\n");
    Emit(cg, "  %%2 = icmp eq ptr %%1, null\n");
    Emit(cg, "  br i1 %%2, label %%done, label %%pop\n");
    Emit(cg, "pop:\n");
    Emit(cg, "  %%3 = load ptr, ptr %%1\n");
    Emit(cg, "  store ptr %%3, ptr @__eh_cur\n");
    Emit(cg, "  br label %%done\n");
    Emit(cg, "done:\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* Exception handling: raise */
    Emit(cg, "define linkonce_odr void @__ada_raise(i64 %%exc_id) {\n");
    Emit(cg, "  store i64 %%exc_id, ptr @__ex_cur\n");
    Emit(cg, "  %%jb = load ptr, ptr @__eh_cur\n");
    Emit(cg, "  %%is_null = icmp eq ptr %%jb, null\n");
    Emit(cg, "  br i1 %%is_null, label %%unhandled, label %%jump\n");
    Emit(cg, "jump:\n");
    Emit(cg, "  call void @longjmp(ptr %%jb, i32 1)\n");
    Emit(cg, "  unreachable\n");
    Emit(cg, "unhandled:\n");
    Emit(cg, "  call i32 (ptr, ...) @printf(ptr @.fmt_ue, i64 %%exc_id)\n");
    Emit(cg, "  call void @longjmp(ptr null, i32 1)\n");
    Emit(cg, "  unreachable\n");
    Emit(cg, "}\n\n");

    /* Exception handling: reraise */
    Emit(cg, "define linkonce_odr void @__ada_reraise() {\n");
    Emit(cg, "  %%exc = load i64, ptr @__ex_cur\n");
    Emit(cg, "  call void @__ada_raise(i64 %%exc)\n");
    Emit(cg, "  unreachable\n");
    Emit(cg, "}\n\n");

    /* Exception handling: get current exception */
    Emit(cg, "define linkonce_odr i64 @__ada_current_exception() {\n");
    Emit(cg, "  %%exc = load i64, ptr @__ex_cur\n");
    Emit(cg, "  ret i64 %%exc\n");
    Emit(cg, "}\n\n");

    /* Integer power function */
    Emit(cg, "; Arithmetic runtime\n");
    Emit(cg, "define linkonce_odr i64 @__ada_powi(i64 %%base, i64 %%exp) {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%result = alloca i64\n");
    Emit(cg, "  store i64 1, ptr %%result\n");
    Emit(cg, "  %%e = alloca i64\n");
    Emit(cg, "  store i64 %%exp, ptr %%e\n");
    Emit(cg, "  br label %%loop\n");
    Emit(cg, "loop:\n");
    Emit(cg, "  %%ev = load i64, ptr %%e\n");
    Emit(cg, "  %%cmp = icmp sgt i64 %%ev, 0\n");
    Emit(cg, "  br i1 %%cmp, label %%body, label %%done\n");
    Emit(cg, "body:\n");
    Emit(cg, "  %%rv = load i64, ptr %%result\n");
    Emit(cg, "  %%nv = mul i64 %%rv, %%base\n");
    Emit(cg, "  store i64 %%nv, ptr %%result\n");
    Emit(cg, "  %%ev2 = load i64, ptr %%e\n");
    Emit(cg, "  %%ev3 = sub i64 %%ev2, 1\n");
    Emit(cg, "  store i64 %%ev3, ptr %%e\n");
    Emit(cg, "  br label %%loop\n");
    Emit(cg, "done:\n");
    Emit(cg, "  %%final = load i64, ptr %%result\n");
    Emit(cg, "  ret i64 %%final\n");
    Emit(cg, "}\n\n");

    /* Delay statement support */
    Emit(cg, "; Tasking runtime\n");
    Emit(cg, "define linkonce_odr void @__ada_delay(i64 %%us) {\n");
    Emit(cg, "  %%t = trunc i64 %%us to i32\n");
    Emit(cg, "  call i32 @usleep(i32 %%t)\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* Task abort: signal task to terminate (simplified: just sets flag) */
    Emit(cg, "define linkonce_odr void @__ada_task_abort(ptr %%task) {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%1 = icmp eq ptr %%task, null\n");
    Emit(cg, "  br i1 %%1, label %%done, label %%abort\n");
    Emit(cg, "abort:\n");
    Emit(cg, "  ; In full impl: set abort flag, signal condition\n");
    Emit(cg, "  store i8 1, ptr %%task  ; Mark abort pending\n");
    Emit(cg, "  br label %%done\n");
    Emit(cg, "done:\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* Task terminate: graceful task termination (for terminate alternative) */
    Emit(cg, "define linkonce_odr void @__ada_task_terminate() {\n");
    Emit(cg, "  ; Check if master task is complete, if so exit\n");
    Emit(cg, "  call void @exit(i32 0)\n");
    Emit(cg, "  unreachable\n");
    Emit(cg, "}\n\n");

    /* Finalization support */
    Emit(cg, "; Finalization runtime\n");
    Emit(cg, "define linkonce_odr void @__ada_finalize(ptr %%obj, ptr %%fn) {\n");
    Emit(cg, "  %%1 = call ptr @malloc(i64 24)\n");
    Emit(cg, "  %%2 = getelementptr ptr, ptr %%1, i64 0\n");
    Emit(cg, "  store ptr %%obj, ptr %%2\n");
    Emit(cg, "  %%3 = getelementptr ptr, ptr %%1, i64 1\n");
    Emit(cg, "  store ptr %%fn, ptr %%3\n");
    Emit(cg, "  %%4 = load ptr, ptr @__fin_list\n");
    Emit(cg, "  %%5 = getelementptr ptr, ptr %%1, i64 2\n");
    Emit(cg, "  store ptr %%4, ptr %%5\n");
    Emit(cg, "  store ptr %%1, ptr @__fin_list\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    Emit(cg, "define linkonce_odr void @__ada_finalize_all() {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%1 = load ptr, ptr @__fin_list\n");
    Emit(cg, "  br label %%loop\n");
    Emit(cg, "loop:\n");
    Emit(cg, "  %%p = phi ptr [%%1, %%entry], [%%9, %%fin]\n");
    Emit(cg, "  %%2 = icmp eq ptr %%p, null\n");
    Emit(cg, "  br i1 %%2, label %%done, label %%fin\n");
    Emit(cg, "fin:\n");
    Emit(cg, "  %%3 = getelementptr ptr, ptr %%p, i64 0\n");
    Emit(cg, "  %%4 = load ptr, ptr %%3\n");
    Emit(cg, "  %%5 = getelementptr ptr, ptr %%p, i64 1\n");
    Emit(cg, "  %%6 = load ptr, ptr %%5\n");
    Emit(cg, "  call void %%6(ptr %%4)\n");
    Emit(cg, "  %%8 = getelementptr ptr, ptr %%p, i64 2\n");
    Emit(cg, "  %%9 = load ptr, ptr %%8\n");
    Emit(cg, "  call void @free(ptr %%p)\n");
    Emit(cg, "  br label %%loop\n");
    Emit(cg, "done:\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* TEXT_IO inline implementation (Ada 83 Chapter 14) */
    Emit(cg, "; TEXT_IO runtime\n");
    Emit(cg, "@stdin = external global ptr\n");
    Emit(cg, "@stdout = external global ptr\n");
    Emit(cg, "@stderr = external global ptr\n");
    Emit(cg, "declare i32 @fputc(i32, ptr)\n");
    Emit(cg, "declare i32 @fputs(ptr, ptr)\n");
    Emit(cg, "declare i32 @fgetc(ptr)\n");
    Emit(cg, "declare ptr @fgets(ptr, i32, ptr)\n");
    Emit(cg, "declare i32 @fprintf(ptr, ptr, ...)\n");
    Emit(cg, "@.fmt_d = linkonce_odr constant [5 x i8] c\"%%lld\\00\"\n");
    Emit(cg, "@.fmt_s = linkonce_odr constant [3 x i8] c\"%%s\\00\"\n");
    Emit(cg, "@.fmt_f = linkonce_odr constant [3 x i8] c\"%%g\\00\"\n");
    Emit(cg, "@.fmt_c = linkonce_odr constant [3 x i8] c\"%%c\\00\"\n\n");

    /* NEW_LINE: output line terminator */
    Emit(cg, "define linkonce_odr void @__text_io_new_line() {\n");
    Emit(cg, "  %%out = load ptr, ptr @stdout\n");
    Emit(cg, "  call i32 @fputc(i32 10, ptr %%out)\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* PUT_CHAR: output single character */
    Emit(cg, "define linkonce_odr void @__text_io_put_char(i8 %%c) {\n");
    Emit(cg, "  %%out = load ptr, ptr @stdout\n");
    Emit(cg, "  %%ci = zext i8 %%c to i32\n");
    Emit(cg, "  call i32 @fputc(i32 %%ci, ptr %%out)\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* PUT: output string (ptr + bounds) */
    Emit(cg, "define linkonce_odr void @__text_io_put(ptr %%data, i64 %%lo, i64 %%hi) {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%out = load ptr, ptr @stdout\n");
    Emit(cg, "  %%i.init = sub i64 %%lo, 1\n");
    Emit(cg, "  br label %%loop\n");
    Emit(cg, "loop:\n");
    Emit(cg, "  %%i = phi i64 [ %%i.init, %%entry ], [ %%i.next, %%body ]\n");
    Emit(cg, "  %%i.next = add i64 %%i, 1\n");
    Emit(cg, "  %%done = icmp sgt i64 %%i.next, %%hi\n");
    Emit(cg, "  br i1 %%done, label %%exit, label %%body\n");
    Emit(cg, "body:\n");
    Emit(cg, "  %%idx = sub i64 %%i.next, %%lo\n");
    Emit(cg, "  %%ptr = getelementptr i8, ptr %%data, i64 %%idx\n");
    Emit(cg, "  %%ch = load i8, ptr %%ptr\n");
    Emit(cg, "  %%chi = zext i8 %%ch to i32\n");
    Emit(cg, "  call i32 @fputc(i32 %%chi, ptr %%out)\n");
    Emit(cg, "  br label %%loop\n");
    Emit(cg, "exit:\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* PUT_LINE: output string followed by newline */
    Emit(cg, "define linkonce_odr void @__text_io_put_line(ptr %%data, i64 %%lo, i64 %%hi) {\n");
    Emit(cg, "  call void @__text_io_put(ptr %%data, i64 %%lo, i64 %%hi)\n");
    Emit(cg, "  call void @__text_io_new_line()\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* PUT_INTEGER: output integer with optional width */
    Emit(cg, "define linkonce_odr void @__text_io_put_int(i64 %%val, i32 %%width) {\n");
    Emit(cg, "  %%out = load ptr, ptr @stdout\n");
    Emit(cg, "  call i32 (ptr, ptr, ...) @fprintf(ptr %%out, ptr @.fmt_d, i64 %%val)\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* PUT_FLOAT: output float value */
    Emit(cg, "define linkonce_odr void @__text_io_put_float(double %%val) {\n");
    Emit(cg, "  %%out = load ptr, ptr @stdout\n");
    Emit(cg, "  call i32 (ptr, ptr, ...) @fprintf(ptr %%out, ptr @.fmt_f, double %%val)\n");
    Emit(cg, "  ret void\n");
    Emit(cg, "}\n\n");

    /* GET_CHAR: read single character */
    Emit(cg, "define linkonce_odr i8 @__text_io_get_char() {\n");
    Emit(cg, "  %%inp = load ptr, ptr @stdin\n");
    Emit(cg, "  %%c = call i32 @fgetc(ptr %%inp)\n");
    Emit(cg, "  %%c8 = trunc i32 %%c to i8\n");
    Emit(cg, "  ret i8 %%c8\n");
    Emit(cg, "}\n\n");

    /* GET_LINE: read line into buffer, return fat pointer */
    Emit(cg, "define linkonce_odr { ptr, { i64, i64 } } @__text_io_get_line() {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%buf = call ptr @__ada_sec_stack_alloc(i64 256)\n");
    Emit(cg, "  %%inp = load ptr, ptr @stdin\n");
    Emit(cg, "  %%res = call ptr @fgets(ptr %%buf, i32 255, ptr %%inp)\n");
    Emit(cg, "  %%iseof = icmp eq ptr %%res, null\n");
    Emit(cg, "  br i1 %%iseof, label %%empty, label %%gotline\n");
    Emit(cg, "empty:\n");
    Emit(cg, "  %%e1 = insertvalue { ptr, { i64, i64 } } undef, ptr %%buf, 0\n");
    Emit(cg, "  %%e2 = insertvalue { ptr, { i64, i64 } } %%e1, i64 1, 1, 0\n");
    Emit(cg, "  %%e3 = insertvalue { ptr, { i64, i64 } } %%e2, i64 0, 1, 1\n");
    Emit(cg, "  ret { ptr, { i64, i64 } } %%e3\n");
    Emit(cg, "gotline:\n");
    Emit(cg, "  %%len = call i64 @strlen(ptr %%buf)\n");
    Emit(cg, "  ; Strip trailing newline if present\n");
    Emit(cg, "  %%lastidx = sub i64 %%len, 1\n");
    Emit(cg, "  %%lastptr = getelementptr i8, ptr %%buf, i64 %%lastidx\n");
    Emit(cg, "  %%lastch = load i8, ptr %%lastptr\n");
    Emit(cg, "  %%isnl = icmp eq i8 %%lastch, 10\n");
    Emit(cg, "  %%adjlen = select i1 %%isnl, i64 %%lastidx, i64 %%len\n");
    Emit(cg, "  %%f1 = insertvalue { ptr, { i64, i64 } } undef, ptr %%buf, 0\n");
    Emit(cg, "  %%f2 = insertvalue { ptr, { i64, i64 } } %%f1, i64 1, 1, 0\n");
    Emit(cg, "  %%f3 = insertvalue { ptr, { i64, i64 } } %%f2, i64 %%adjlen, 1, 1\n");
    Emit(cg, "  ret { ptr, { i64, i64 } } %%f3\n");
    Emit(cg, "}\n\n");

    /* 'IMAGE runtime: Integer'IMAGE(x) returns string representation */
    Emit(cg, "; Attribute runtime support\n");
    Emit(cg, "declare i32 @snprintf(ptr, i64, ptr, ...)\n");
    Emit(cg, "declare i64 @strlen(ptr)\n");
    Emit(cg, "@.img_fmt_d = linkonce_odr constant [5 x i8] c\"%%lld\\00\"\n");
    Emit(cg, "@.img_fmt_c = linkonce_odr constant [3 x i8] c\"%%c\\00\"\n");
    Emit(cg, "@.img_fmt_f = linkonce_odr constant [5 x i8] c\"%%.6g\\00\"\n\n");

    /* Integer'IMAGE - convert integer to string fat pointer */
    Emit(cg, "define linkonce_odr { ptr, { i64, i64 } } @__ada_integer_image(i64 %%val) {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%buf = call ptr @__ada_sec_stack_alloc(i64 24)\n");
    Emit(cg, "  %%len = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %%buf, i64 24, ptr @.img_fmt_d, i64 %%val)\n");
    Emit(cg, "  %%len64 = sext i32 %%len to i64\n");
    Emit(cg, "  %%high = sub i64 %%len64, 1\n");
    Emit(cg, "  %%fat1 = insertvalue { ptr, { i64, i64 } } undef, ptr %%buf, 0\n");
    Emit(cg, "  %%fat2 = insertvalue { ptr, { i64, i64 } } %%fat1, i64 1, 1, 0\n");
    Emit(cg, "  %%fat3 = insertvalue { ptr, { i64, i64 } } %%fat2, i64 %%len64, 1, 1\n");
    Emit(cg, "  ret { ptr, { i64, i64 } } %%fat3\n");
    Emit(cg, "}\n\n");

    /* Character'IMAGE - single char to string (3 chars: 'x') */
    Emit(cg, "define linkonce_odr { ptr, { i64, i64 } } @__ada_character_image(i8 %%val) {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%buf = call ptr @__ada_sec_stack_alloc(i64 4)\n");
    Emit(cg, "  %%p0 = getelementptr i8, ptr %%buf, i64 0\n");
    Emit(cg, "  store i8 39, ptr %%p0  ; single quote\n");
    Emit(cg, "  %%p1 = getelementptr i8, ptr %%buf, i64 1\n");
    Emit(cg, "  store i8 %%val, ptr %%p1\n");
    Emit(cg, "  %%p2 = getelementptr i8, ptr %%buf, i64 2\n");
    Emit(cg, "  store i8 39, ptr %%p2  ; single quote\n");
    Emit(cg, "  %%fat1 = insertvalue { ptr, { i64, i64 } } undef, ptr %%buf, 0\n");
    Emit(cg, "  %%fat2 = insertvalue { ptr, { i64, i64 } } %%fat1, i64 1, 1, 0\n");
    Emit(cg, "  %%fat3 = insertvalue { ptr, { i64, i64 } } %%fat2, i64 3, 1, 1\n");
    Emit(cg, "  ret { ptr, { i64, i64 } } %%fat3\n");
    Emit(cg, "}\n\n");

    /* Float'IMAGE - convert float to string */
    Emit(cg, "define linkonce_odr { ptr, { i64, i64 } } @__ada_float_image(double %%val) {\n");
    Emit(cg, "entry:\n");
    Emit(cg, "  %%buf = call ptr @__ada_sec_stack_alloc(i64 32)\n");
    Emit(cg, "  %%len = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %%buf, i64 32, ptr @.img_fmt_f, double %%val)\n");
    Emit(cg, "  %%len64 = sext i32 %%len to i64\n");
    Emit(cg, "  %%fat1 = insertvalue { ptr, { i64, i64 } } undef, ptr %%buf, 0\n");
    Emit(cg, "  %%fat2 = insertvalue { ptr, { i64, i64 } } %%fat1, i64 1, 1, 0\n");
    Emit(cg, "  %%fat3 = insertvalue { ptr, { i64, i64 } } %%fat2, i64 %%len64, 1, 1\n");
    Emit(cg, "  ret { ptr, { i64, i64 } } %%fat3\n");
    Emit(cg, "}\n\n");

    /* Generate exception identity globals */
    Generate_Exception_Globals(cg);

    /* Generate implicit operators for frozen composite types */
    Generate_Implicit_Operators(cg);
    Emit(cg, "\n");

    /* Generate extern declarations for WITH'd packages */
    Generate_Extern_Declarations(cg, node);

    /* Generate declarations */
    if (node->compilation_unit.unit) {
        Generate_Declaration(cg, node->compilation_unit.unit);
    }

    /* Emit buffered string constants at module level */
    if (cg->string_const_size > 0) {
        Emit(cg, "\n; String constants\n");
        fprintf(cg->output, "%s", cg->string_const_buffer);
        Emit(cg, "\n");
    }

    /* Generate main function if this is a main program (library-level procedure) */
    Syntax_Node *unit = node->compilation_unit.unit;
    if (unit && unit->kind == NK_PROCEDURE_BODY && unit->symbol) {
        Symbol *main_sym = unit->symbol;
        /* Check if this is a library-level procedure (no parameters, no parent package) */
        if (main_sym->parameter_count == 0) {
            Emit(cg, "\n; C main entry point\n");
            Emit(cg, "define i32 @main() {\n");
            Emit(cg, "  call void @");
            Emit_Symbol_Name(cg, main_sym);
            Emit(cg, "()\n");
            Emit(cg, "  ret i32 0\n");
            Emit(cg, "}\n");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §14. INCLUDE PATH & PACKAGE LOADING
 * ═══════════════════════════════════════════════════════════════════════════
 */

static const char *Include_Paths[32];
static int Include_Path_Count = 0;

/* Track packages currently being loaded to detect cycles */
typedef struct {
    String_Slice names[64];
    int count;
} Loading_Set;

static Loading_Set Loading_Packages = {0};

static bool Loading_Set_Contains(String_Slice name) {
    for (int i = 0; i < Loading_Packages.count; i++) {
        if (Loading_Packages.names[i].length == name.length &&
            strncasecmp(Loading_Packages.names[i].data, name.data, name.length) == 0) {
            return true;
        }
    }
    return false;
}

static void Loading_Set_Add(String_Slice name) {
    if (Loading_Packages.count < 64) {
        Loading_Packages.names[Loading_Packages.count++] = name;
    }
}

static void Loading_Set_Remove(String_Slice name) {
    for (int i = 0; i < Loading_Packages.count; i++) {
        if (Loading_Packages.names[i].length == name.length &&
            strncasecmp(Loading_Packages.names[i].data, name.data, name.length) == 0) {
            /* Swap with last and decrement count */
            Loading_Packages.names[i] = Loading_Packages.names[--Loading_Packages.count];
            return;
        }
    }
}

static char *Read_File_Simple(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize < 0) { fclose(f); return NULL; }
    size_t size = (size_t)fsize;
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) { fclose(f); return NULL; }

    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);
    buffer[read_size] = '\0';
    return buffer;
}

/* Find a package source file in include paths */
static char *Lookup_Path(String_Slice name) {
    char path[512], full_path[520];  /* full_path larger for .ads extension */

    for (int i = 0; i < Include_Path_Count; i++) {
        /* Build lowercase filename */
        size_t base_len = strlen(Include_Paths[i]);
        snprintf(path, sizeof(path), "%s%s%.*s",
                 Include_Paths[i],
                 (base_len > 0 && Include_Paths[i][base_len-1] != '/') ? "/" : "",
                 (int)name.length, name.data);

        /* Lowercase the filename part */
        for (char *p = path + base_len; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
        }

        /* Try .ads extension */
        snprintf(full_path, sizeof(full_path), "%s.ads", path);
        char *src = Read_File_Simple(full_path);
        if (src) return src;
    }
    return NULL;
}

/* Forward declarations for package loading */
static Syntax_Node *Parse_Compilation_Unit(Parser *p);
static void Resolve_Declaration(Symbol_Manager *sm, Syntax_Node *node);

/* Load and resolve a package specification */
static void Load_Package_Spec(Symbol_Manager *sm, String_Slice name, char *src) {
    if (!src) return;

    /* Check if already loaded */
    Symbol *existing = Symbol_Find(sm, name);
    if (existing && existing->kind == SYMBOL_PACKAGE && existing->declaration) {
        return;  /* Already loaded */
    }

    /* Check for circular dependency (package currently being loaded) */
    if (Loading_Set_Contains(name)) {
        return;  /* Break cycle - package will be available when outer load completes */
    }

    /* Mark package as loading to detect cycles */
    Loading_Set_Add(name);

    /* Parse the package */
    char filename[256];
    snprintf(filename, sizeof(filename), "%.*s.ads", (int)name.length, name.data);
    Parser p = Parser_New(src, strlen(src), filename);
    Syntax_Node *cu = Parse_Compilation_Unit(&p);

    if (!cu) {
        Loading_Set_Remove(name);
        return;
    }

    /* Recursively load WITH'd packages */
    if (cu->compilation_unit.context) {
        Node_List *withs = &cu->compilation_unit.context->context.with_clauses;
        for (uint32_t i = 0; i < withs->count; i++) {
            Syntax_Node *with_node = withs->items[i];
            for (uint32_t j = 0; j < with_node->use_clause.names.count; j++) {
                Syntax_Node *pkg_name = with_node->use_clause.names.items[j];
                if (pkg_name->kind == NK_IDENTIFIER) {
                    char *pkg_src = Lookup_Path(pkg_name->string_val.text);
                    if (pkg_src) {
                        Load_Package_Spec(sm, pkg_name->string_val.text, pkg_src);
                    }
                }
            }
        }
    }

    /* Resolve the package declarations */
    if (cu->compilation_unit.unit &&
        cu->compilation_unit.unit->kind == NK_PACKAGE_SPEC) {

        Syntax_Node *pkg = cu->compilation_unit.unit;

        /* Create package symbol */
        Symbol *pkg_sym = Symbol_New(SYMBOL_PACKAGE, pkg->package_spec.name,
                                     pkg->location);
        Type_Info *pkg_type = Type_New(TYPE_PACKAGE, pkg->package_spec.name);
        pkg_sym->type = pkg_type;
        pkg_sym->declaration = pkg;
        Symbol_Add(sm, pkg_sym);
        pkg->symbol = pkg_sym;

        /* Push package scope */
        Symbol_Manager_Push_Scope(sm, pkg_sym);

        /* Resolve visible declarations */
        Resolve_Declaration_List(sm, &pkg->package_spec.visible_decls);

        /* Resolve private declarations */
        Resolve_Declaration_List(sm, &pkg->package_spec.private_decls);

        Symbol_Manager_Pop_Scope(sm);
    }

    /* Done loading this package */
    Loading_Set_Remove(name);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * §15. MAIN DRIVER
 * ═══════════════════════════════════════════════════════════════════════════
 */

static char *Read_File(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize < 0) { fclose(f); return NULL; }
    size_t size = (size_t)fsize;
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) { fclose(f); return NULL; }

    size_t read = fread(buffer, 1, size, f);
    fclose(f);

    buffer[read] = '\0';
    *out_size = read;
    return buffer;
}

static void Compile_File(const char *input_path, const char *output_path) {
    size_t source_size;
    char *source = Read_File(input_path, &source_size);

    if (!source) {
        fprintf(stderr, "Error: cannot read file '%s'\n", input_path);
        return;
    }

    /* Parse */
    Parser parser = Parser_New(source, source_size, input_path);
    Syntax_Node *unit = Parse_Compilation_Unit(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Parsing failed with %d error(s)\n", Error_Count);
        free(source);
        return;
    }

    /* Semantic analysis */
    Symbol_Manager *sm = Symbol_Manager_New();
    Resolve_Compilation_Unit(sm, unit);

    if (Error_Count > 0) {
        fprintf(stderr, "Semantic analysis failed with %d error(s)\n", Error_Count);
        free(source);
        return;
    }

    /* Code generation */
    FILE *out_file;
    bool close_output = false;

    if (output_path) {
        out_file = fopen(output_path, "w");
        if (!out_file) {
            fprintf(stderr, "Error: cannot open output file '%s'\n", output_path);
            free(source);
            return;
        }
        close_output = true;
    } else {
        out_file = stdout;  /* Output to stdout if no -o specified */
    }

    Code_Generator *cg = Code_Generator_New(out_file, sm);
    Generate_Compilation_Unit(cg, unit);

    if (close_output) {
        fclose(out_file);
        fprintf(stderr, "Compiled '%s' -> '%s'\n", input_path, output_path);
    }
    free(source);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-I path] <input.ada> [-o output.ll]\n", argv[0]);
        return 1;
    }

    const char *input = NULL;
    const char *output = NULL;  /* NULL means stdout */

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            /* Add include path */
            if (Include_Path_Count < 32) {
                Include_Paths[Include_Path_Count++] = argv[++i];
            }
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            /* -Ipath format (no space) */
            if (Include_Path_Count < 32) {
                Include_Paths[Include_Path_Count++] = argv[i] + 2;
            }
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (argv[i][0] != '-') {
            input = argv[i];
        }
    }

    if (!input) {
        fprintf(stderr, "Error: no input file specified\n");
        return 1;
    }

    /* Add current directory to include paths by default */
    if (Include_Path_Count < 32) {
        Include_Paths[Include_Path_Count++] = ".";
    }

    Compile_File(input, output);

    Arena_Free_All();
    return Error_Count > 0 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * END OF ada83new.c — A Literate Ada 83 Compiler
 *
 * Key improvements:
 * ═══════════════════════════════════════════════════════════════════════════
 */
