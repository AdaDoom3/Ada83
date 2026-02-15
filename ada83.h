/* ada83.h -- Package specification for the Ada83 compiler
 * ════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * An Ada 1983 (ANSI/MIL-STD-1815A) compiler targeting LLVM IR, presented as a literate
 * program in the tradition of Knuth's WEB.  The header is arranged like a book: we begin
 * with the tuneable constants that govern the compiler's behaviour, proceed through the
 * primitive data structures every subsystem shares, and build chapter by chapter toward
 * the full compilation pipeline.
 *
 * The reader who wishes to understand this compiler should read sequentially; the reader
 * who wishes to modify it should begin at §1 (Settings), where every hard-coded limit
 * and platform assumption is gathered in one place.
 *
 *   §1   Settings                Tuneable limits, platform constants, magic numbers
 *   §2   Scalar primitives       128-bit integers, safe ctype, identifier characters
 *   §3   String slices           Non-owning views with case-insensitive comparison
 *   §4   Memory arena            Bump allocator for the compilation session
 *   §5   Source locations         File / line / column anchors for diagnostics
 *   §6   Diagnostics             Error accumulation and fatal-error reporting
 *   §7   Big integers            Arbitrary-precision integers for Ada literals
 *   §8   Big reals               Arbitrary-precision reals and exact rationals
 *   §9   Tokens                  The vocabulary of Ada 83
 *   §10  Lexer                   Character stream to token stream
 *   §11  Syntax tree             Node kinds, the universal syntax-node record
 *   §12  Parser                  Recursive-descent state and grammar productions
 *   §13  Type system             Kind lattice, bounds, components, type descriptors
 *   §14  Symbol table            Names, scopes, visibility, overload resolution
 *   §15  Semantic analysis       Type checking, constant folding, declaration processing
 *   §16  Code generator          LLVM IR emission for all Ada constructs
 *   §17  Library information     ALI files, CRC32, dependency tracking
 *   §18  Elaboration model       Dependency graphs, Tarjan's SCC, topological ordering
 *   §19  Build-in-place          Return protocol for limited types (RM 7.5)
 *   §20  Generics                Macro-style instantiation and deep cloning
 *   §21  Include paths           Package file loading and search-path management
 *   §22  SIMD fast paths         Vectorised scanning for x86-64, ARM64, and scalar
 *   §23  Driver                  File compilation, parallel workers, entry point
 *
 * ════════════════════════════════════════════════════════════════════════════════════════════════
 */

#ifndef ADA83_H
#define ADA83_H

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
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §1. Settings
 *
 * Every tuneable limit, capacity bound, and platform-dependent constant lives here.
 * When porting to a new target, adapting to a larger program, or changing the LLVM IR
 * layout conventions, this is the only section that should need attention.
 *
 * The constants are grouped by subsystem.  A grep for the §-number of any later
 * chapter will show which settings govern it.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* §1.1  Target data model ── assumed 64-bit LP64 throughout. */
enum { Bits_Per_Unit = 8 };

typedef enum {
  Width_1   = 1,    Width_8   = 8,    Width_16  = 16,
  Width_32  = 32,   Width_64  = 64,   Width_128 = 128,
  Width_Ptr = 64,   Width_Float = 32, Width_Double = 64
} Bit_Width;

typedef enum {
  Ada_Short_Short_Integer_Bits    = Width_8,
  Ada_Short_Integer_Bits          = Width_16,
  Ada_Integer_Bits                = Width_32,
  Ada_Long_Integer_Bits           = Width_64,
  Ada_Long_Long_Integer_Bits      = Width_64,
  Ada_Long_Long_Long_Integer_Bits = Width_128            /* Ada 2022 extension */
} Ada_Integer_Width;

enum {
  Default_Size_Bits   = Ada_Integer_Bits,
  Default_Size_Bytes  = Ada_Integer_Bits / Bits_Per_Unit,
  Default_Align_Bytes = Default_Size_Bytes
};

/* §1.2  LLVM fat-pointer layout ── GNAT convention (gnatllvm-arrays-create.adb §684-707).
 * A fat pointer is { data_ptr, bounds_ptr } = 16 bytes on 64-bit targets.  STRING
 * bounds are a pair of i32 indices. */
#define FAT_PTR_TYPE          "{ ptr, ptr }"
#define FAT_PTR_ALLOC_SIZE    16
#define STRING_BOUND_TYPE     "i32"
#define STRING_BOUND_WIDTH    32
#define STRING_BOUNDS_STRUCT  "{ i32, i32 }"
#define STRING_BOUNDS_ALLOC   8

/* §1.3  IEEE floating-point model parameters (RM §3.5.8). */
#define IEEE_FLOAT_DIGITS        6
#define IEEE_DOUBLE_DIGITS       15
#define IEEE_FLOAT_MANTISSA      24
#define IEEE_DOUBLE_MANTISSA     53
#define IEEE_FLOAT_EMAX          128
#define IEEE_DOUBLE_EMAX         1024
#define IEEE_FLOAT_EMIN          (-125)
#define IEEE_DOUBLE_EMIN         (-1021)
#define IEEE_MACHINE_RADIX       2
#define IEEE_DOUBLE_MIN_NORMAL   2.2250738585072014e-308  /* 2^(-1022) */
#define IEEE_FLOAT_MIN_NORMAL    1.1754943508222875e-38   /* 2^(-126)  */
#define LOG2_OF_10               3.321928094887362

/* §1.4  Memory arena ── 16 MiB chunks keep the number of malloc calls low. */
enum { Default_Chunk_Size = 1 << 24 };

/* §1.5  Symbol table ── hash table width.  1024 buckets covers most programs. */
#define SYMBOL_TABLE_SIZE        1024

/* §1.6  Overload resolution ── sixty-four interpretations suffices; deeper
 * ambiguity signals a pathological program. */
#define MAX_INTERPRETATIONS      64

/* §1.7  ALI file format version string (§17). */
#define ALI_VERSION              "Ada83 1.0 built " __DATE__ " " __TIME__

/* §1.8  Elaboration graph capacities (§18). */
#define ELAB_MAX_VERTICES        512
#define ELAB_MAX_EDGES           2048
#define ELAB_MAX_COMPONENTS      256

/* §1.9  Build-in-place extra formal names (§19). */
#define BIP_ALLOC_NAME           "__BIPalloc"
#define BIP_ACCESS_NAME          "__BIPaccess"
#define BIP_MASTER_NAME          "__BIPmaster"
#define BIP_CHAIN_NAME           "__BIPchain"
#define BIP_FINAL_NAME           "__BIPfinal"

/* §1.10 Code generator ring-buffer and capacity limits (§16). */
#define TEMP_TYPE_CAPACITY       4096
#define EXC_REF_CAPACITY         512
#define MAX_AGG_DIMS             8
#define MAX_DISC_CACHE           16

/* §1.11 Runtime check bit-flags (GNAT-style, RM 11.5).
 * Each bit controls a category suppressible via pragma Suppress. */
#define CHK_RANGE                ((uint32_t)1)
#define CHK_OVERFLOW             ((uint32_t)2)
#define CHK_INDEX                ((uint32_t)4)
#define CHK_LENGTH               ((uint32_t)8)
#define CHK_DIVISION             ((uint32_t)16)
#define CHK_ACCESS               ((uint32_t)32)
#define CHK_DISCRIMINANT         ((uint32_t)64)
#define CHK_ELABORATION          ((uint32_t)128)
#define CHK_STORAGE              ((uint32_t)256)
#define CHK_ALL                  ((uint32_t)0xFFFFFFFF)

/* §1.12 Platform detection ── selects SIMD paths at compile time (§22). */
#if defined (__x86_64__) || defined (_M_X64)
  #define SIMD_X86_64 1
#elif defined (__aarch64__) || defined (_M_ARM64)
  #define SIMD_ARM64 1
#else
  #define SIMD_GENERIC 1
#endif

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §2. Scalar primitives
 *
 * 128-bit integers are a GCC/Clang extension required by Ada's mod 2**128 modular types
 * and the Ada 2022 Long_Long_Long_Integer.  The safe ctype wrappers avoid undefined
 * behaviour when plain char is signed.  The identifier-character table encodes Ada
 * LRM §2.3's rules as a 256-byte bitmap.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef __int128          int128_t;
typedef unsigned __int128 uint128_t;

int  Is_Alpha  (char ch);
int  Is_Digit  (char ch);
int  Is_Xdigit (char ch);
int  Is_Space  (char ch);
char To_Lower  (char ch);

extern const uint8_t Id_Char_Table[256];
#define Is_Id_Char(ch) (Id_Char_Table[(uint8_t)(ch)])

/* Size morphisms.  To_Bits is multiplicative and total; To_Bytes rounds up. */
uint64_t    To_Bits              (uint64_t bytes);
uint64_t    To_Bytes             (uint64_t bits);
uint64_t    Byte_Align           (uint64_t bits);
size_t      Align_To             (size_t size, size_t alignment);

/* LLVM type selection ── return the smallest enclosing iN or float type. */
const char *Llvm_Int_Type        (uint32_t bits);
const char *Llvm_Float_Type      (uint32_t bits);

/* Fat-pointer predicates. */
bool Llvm_Type_Is_Pointer        (const char *llvm_type);
bool Llvm_Type_Is_Fat_Pointer    (const char *llvm_type);

/* Range predicates ── minimum bit width for an integer range or modulus.  All bounds
 * are int128_t/uint128_t so the full Ada 2022 range is representable. */
bool        Fits_In_Signed       (int128_t lo, int128_t hi, uint32_t bits);
bool        Fits_In_Unsigned     (int128_t lo, int128_t hi, uint32_t bits);
uint32_t    Bits_For_Range       (int128_t lo, int128_t hi);
uint32_t    Bits_For_Modulus     (uint128_t modulus);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §3. String slices
 *
 * A String_Slice is a (pointer, length) pair borrowed from the source buffer or arena.
 * No null terminator is required.  Ada identifiers are case-insensitive, so comparison
 * and hashing fold to lower case.  The S() macro builds a slice from a C literal.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  const char *data;
  uint32_t    length;
} String_Slice;

#define S(literal) \
  ((String_Slice){ .data = (literal), .length = sizeof (literal) - 1 })

extern const String_Slice Empty_Slice;

String_Slice Slice_From_Cstring      (const char *source);
String_Slice Slice_Duplicate         (String_Slice slice);
bool         Slice_Equal             (String_Slice left, String_Slice right);
bool         Slice_Equal_Ignore_Case (String_Slice left, String_Slice right);
uint64_t     Slice_Hash              (String_Slice slice);
int          Edit_Distance           (String_Slice left, String_Slice right);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §4. Memory arena
 *
 * A bump allocator whose lifetime spans the entire compilation.  All memory is freed
 * in one shot by Arena_Free_All.  Individual frees are neither needed nor supported.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

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

extern Memory_Arena Global_Arena;

void *Arena_Allocate (size_t size);
void  Arena_Free_All (void);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §5. Source locations
 *
 * Every AST node, token, and symbol carries a Source_Location so error messages can
 * point the programmer at the exact file, line, and column of the problem.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  const char *filename;
  uint32_t    line;
  uint32_t    column;
} Source_Location;

extern const Source_Location No_Location;

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §6. Diagnostics
 *
 * Errors are accumulated rather than aborting, so the compiler reports multiple issues
 * per invocation.  Fatal_Error is reserved for internal consistency violations.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

extern int Error_Count;

void Report_Error (Source_Location location, const char *format, ...);

__attribute__ ((noreturn))
void Fatal_Error  (Source_Location location, const char *format, ...);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §7. Big integers
 *
 * Ada numeric literals can exceed the 64-bit range (mod 2**128).  Magnitudes are
 * little-endian arrays of 64-bit limbs.  The operations needed for literal parsing
 * are: construction from a based string, multiply-add by a small constant, comparison,
 * full multiplication, division with remainder, GCD, and extraction to machine types.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  uint64_t *limbs;
  uint32_t  count;
  uint32_t  capacity;
  bool      is_negative;
} Big_Integer;

Big_Integer *Big_Integer_New             (uint32_t capacity);
Big_Integer *Big_Integer_Clone           (const Big_Integer *source);
Big_Integer *Big_Integer_One             (void);
void         Big_Integer_Ensure_Capacity (Big_Integer *integer, uint32_t needed);
void         Big_Integer_Normalize       (Big_Integer *integer);
void         Big_Integer_Mul_Add_Small   (Big_Integer *integer,
                                          uint64_t factor, uint64_t addend);
int          Big_Integer_Compare         (const Big_Integer *left,
                                          const Big_Integer *right);
Big_Integer *Big_Integer_Add             (const Big_Integer *left,
                                          const Big_Integer *right);
Big_Integer *Big_Integer_Multiply        (const Big_Integer *left,
                                          const Big_Integer *right);
void         Big_Integer_Div_Rem         (const Big_Integer *dividend,
                                          const Big_Integer *divisor,
                                          Big_Integer **quotient,
                                          Big_Integer **remainder);
Big_Integer *Big_Integer_GCD             (const Big_Integer *left,
                                          const Big_Integer *right);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §8. Big reals and rationals
 *
 * Real literals are significand x 10^exponent (Ada LRM §2.4.1).  The literal
 * 3.14159_26535 becomes significand = 31415926535, exponent = -10.  Exact until the
 * code generator rounds to float or double.
 *
 * Rationals carry constant-folding intermediate results through the semantic pass
 * without loss of precision.  Reduced by GCD on construction.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  Big_Integer *significand;
  int32_t      exponent;
} Big_Real;

Big_Real *Big_Real_New         (void);
Big_Real *Big_Real_From_String (const char *text);
double    Big_Real_To_Double   (const Big_Real *real);
bool      Big_Real_Fits_Double (const Big_Real *real);
void      Big_Real_To_Hex     (const Big_Real *real,
                                char *buffer, size_t buffer_size);
int       Big_Real_Compare     (const Big_Real *left, const Big_Real *right);
Big_Real *Big_Real_Add_Sub     (const Big_Real *left,
                                const Big_Real *right, bool subtract);
Big_Real *Big_Real_Scale       (const Big_Real *real, int32_t scale);
Big_Real *Big_Real_Divide_Int  (const Big_Real *dividend, int64_t divisor);
Big_Real *Big_Real_Multiply    (const Big_Real *left, const Big_Real *right);

typedef struct {
  Big_Integer *numerator;
  Big_Integer *denominator;
} Rational;

Rational Rational_Reduce        (Big_Integer *numer, Big_Integer *denom);
Rational Rational_From_Big_Real (const Big_Real *real);
Rational Rational_From_Int      (int64_t value);
Rational Rational_Add           (Rational left, Rational right);
Rational Rational_Sub           (Rational left, Rational right);
Rational Rational_Mul           (Rational left, Rational right);
Rational Rational_Div           (Rational left, Rational right);
Rational Rational_Pow           (Rational base, int exponent);
int      Rational_Compare       (Rational left, Rational right);
double   Rational_To_Double     (Rational rational);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §9. Tokens
 *
 * A token is the smallest meaningful unit of Ada source text.  Token_Kind enumerates
 * every lexeme in the Ada 83 grammar: identifiers, numeric and string literals,
 * delimiters, operators, and the 63 reserved words (RM §2.9).
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum {
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

  /* Reserved words (Ada 83 ── RM §2.9) */
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

extern const char *Token_Name[TK_COUNT];
Token_Kind Lookup_Keyword (String_Slice name);

/* A Token carries its kind, source location, literal text, and (for numeric literals)
 * the parsed value in a discriminated union. */
typedef struct {
  Token_Kind      kind;
  Source_Location  location;
  String_Slice    text;
  union { int64_t integer_value; double float_value; };
  union { Big_Integer *big_integer; Big_Real *big_real; };
} Token;

Token Make_Token (Token_Kind kind, Source_Location location, String_Slice text);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §10. Lexer
 *
 * The lexer is a cursor over the source buffer that produces tokens on demand.  It
 * owns no allocations; all string data points into the original source text.  SIMD
 * fast paths for whitespace skipping, identifier scanning, and digit scanning are
 * defined later in §22.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  const char *source_start;
  const char *current;
  const char *source_end;
  const char *filename;
  uint32_t    line;
  uint32_t    column;
  Token_Kind  prev_token_kind;
} Lexer;

Lexer Lexer_New                          (const char *source, size_t length,
                                          const char *filename);
char  Lexer_Peek                         (const Lexer *lex, size_t offset);
char  Lexer_Advance                      (Lexer *lex);
void  Lexer_Skip_Whitespace_And_Comments (Lexer *lex);
Token Lexer_Next_Token                   (Lexer *lex);

Token Scan_Identifier        (Lexer *lex);
Token Scan_Number            (Lexer *lex);
Token Scan_Character_Literal (Lexer *lex);
Token Scan_String_Literal    (Lexer *lex);
int   Digit_Value            (char ch);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §11. Syntax tree
 *
 * The abstract syntax tree is the central data structure.  Every syntactic construct
 * in Ada 83 maps to a Node_Kind; the Syntax_Node record carries kind, location, a
 * type annotation (set by semantic analysis), a symbol link (set by name resolution),
 * and a payload union discriminated by kind.
 *
 * Forward declarations appear here because Syntax_Node, Type_Info, and Symbol are
 * mutually recursive: a syntax node can reference a type or symbol, and a type or
 * symbol can reference a syntax node.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct Syntax_Node Syntax_Node;
typedef struct Type_Info   Type_Info;
typedef struct Symbol      Symbol;
typedef struct Scope       Scope;

/* Dynamic array of syntax-node pointers. */
typedef struct {
  Syntax_Node **items;
  uint32_t      count;
  uint32_t      capacity;
} Node_List;

void Node_List_Push (Node_List *list, Syntax_Node *node);

typedef enum {

  /* Literals and primaries */
  NK_INTEGER, NK_REAL, NK_STRING, NK_CHARACTER, NK_NULL, NK_OTHERS,
  NK_IDENTIFIER, NK_SELECTED, NK_ATTRIBUTE, NK_QUALIFIED,

  /* Expressions */
  NK_BINARY_OP, NK_UNARY_OP, NK_AGGREGATE, NK_ALLOCATOR,
  NK_APPLY, NK_RANGE, NK_ASSOCIATION,

  /* Type definitions */
  NK_SUBTYPE_INDICATION, NK_RANGE_CONSTRAINT, NK_INDEX_CONSTRAINT,
  NK_DISCRIMINANT_CONSTRAINT, NK_DIGITS_CONSTRAINT, NK_DELTA_CONSTRAINT,
  NK_ARRAY_TYPE, NK_RECORD_TYPE, NK_ACCESS_TYPE, NK_DERIVED_TYPE,
  NK_ENUMERATION_TYPE, NK_INTEGER_TYPE, NK_REAL_TYPE, NK_COMPONENT_DECL,
  NK_VARIANT_PART, NK_VARIANT, NK_DISCRIMINANT_SPEC,

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

struct Syntax_Node {
  Node_Kind        kind;
  Source_Location   location;
  Type_Info       *type;
  Symbol          *symbol;

  union {
    struct { int64_t value; Big_Integer *big_value; }              integer_lit;
    struct { double value; Big_Real *big_value; }                  real_lit;
    struct { String_Slice text; }                                  string_val;
    struct { Syntax_Node *prefix; String_Slice selector; }        selected;
    struct { Syntax_Node *prefix; String_Slice name;
             Node_List arguments; }                                attribute;
    struct { Syntax_Node *subtype_mark;
             Syntax_Node *expression; }                            qualified;
    struct { Token_Kind op; Syntax_Node *left;
             Syntax_Node *right; }                                 binary;
    struct { Token_Kind op; Syntax_Node *operand; }               unary;
    struct { Node_List items; bool is_named;
             bool is_parenthesized; }                              aggregate;
    struct { Syntax_Node *subtype_mark;
             Syntax_Node *expression; }                            allocator;
    struct { Syntax_Node *prefix; Node_List arguments; }          apply;
    struct { Syntax_Node *low; Syntax_Node *high; }               range;
    struct { Node_List choices; Syntax_Node *expression; }        association;
    struct { Syntax_Node *subtype_mark;
             Syntax_Node *constraint; }                            subtype_ind;
    struct { Node_List ranges; }                                  index_constraint;
    struct { Syntax_Node *range; }                                range_constraint;
    struct { Node_List associations; }                            discriminant_constraint;
    struct { Syntax_Node *digits_expr;
             Syntax_Node *range; }                                 digits_constraint;
    struct { Syntax_Node *delta_expr;
             Syntax_Node *range; }                                 delta_constraint;
    struct { Node_List indices; Syntax_Node *component_type;
             bool is_constrained; }                                array_type;
    struct { Node_List discriminants; Node_List components;
             Syntax_Node *variant_part; bool is_null; }           record_type;
    struct { Syntax_Node *designated; bool is_constant; }         access_type;
    struct { Syntax_Node *parent_type;
             Syntax_Node *constraint; }                            derived_type;
    struct { Node_List literals; }                                enum_type;
    struct { Syntax_Node *range; uint128_t modulus;
             bool is_modular; }                                    integer_type;
    struct { Syntax_Node *precision; Syntax_Node *range;
             Syntax_Node *delta; }                                 real_type;
    struct { Node_List names; Syntax_Node *component_type;
             Syntax_Node *init; }                                  component;
    struct { String_Slice discriminant; Node_List variants; }     variant_part;
    struct { Node_List choices; Node_List components;
             Syntax_Node *variant_part; }                          variant;
    struct { Node_List names; Syntax_Node *disc_type;
             Syntax_Node *default_expr; }                          discriminant;
    struct { Syntax_Node *target; Syntax_Node *value; }           assignment;
    struct { Syntax_Node *expression; }                           return_stmt;
    struct { Syntax_Node *condition; Node_List then_stmts;
             Node_List elsif_parts; Node_List else_stmts; }       if_stmt;
    struct { Syntax_Node *expression;
             Node_List alternatives; }                             case_stmt;
    struct { String_Slice label; Symbol *label_symbol;
             Syntax_Node *iteration_scheme;
             Node_List statements; bool is_reverse; }             loop_stmt;
    struct { String_Slice label; Symbol *label_symbol;
             Node_List declarations; Node_List statements;
             Node_List handlers; }                                 block_stmt;
    struct { String_Slice loop_name; Syntax_Node *condition;
             Symbol *target; }                                     exit_stmt;
    struct { String_Slice name; Symbol *target; }                 goto_stmt;
    struct { String_Slice name; Syntax_Node *statement;
             Symbol *symbol; }                                     label_node;
    struct { Syntax_Node *exception_name; }                       raise_stmt;
    struct { String_Slice entry_name; Syntax_Node *index;
             Node_List parameters; Node_List statements;
             Symbol *entry_sym; }                                  accept_stmt;
    struct { Node_List alternatives;
             Syntax_Node *else_part; }                             select_stmt;
    struct { Syntax_Node *expression; }                           delay_stmt;
    struct { Node_List task_names; }                              abort_stmt;
    struct { Node_List names; Syntax_Node *object_type;
             Syntax_Node *init; bool is_constant;
             bool is_aliased; bool is_rename; }                   object_decl;
    struct { String_Slice name; Node_List discriminants;
             Syntax_Node *definition; bool is_limited;
             bool is_private; }                                    type_decl;
    struct { Node_List names; Syntax_Node *renamed; }             exception_decl;
    struct { String_Slice name; Node_List parameters;
             Syntax_Node *return_type;
             Syntax_Node *renamed; }                               subprogram_spec;
    struct { Syntax_Node *specification; Node_List declarations;
             Node_List statements; Node_List handlers;
             bool is_separate; bool code_generated; }             subprogram_body;
    struct { String_Slice name; Node_List visible_decls;
             Node_List private_decls; }                            package_spec;
    struct { String_Slice name; Node_List declarations;
             Node_List statements; Node_List handlers;
             bool is_separate; }                                   package_body;
    struct { String_Slice new_name;
             Syntax_Node *old_name; }                              package_renaming;
    struct { String_Slice name; Node_List entries;
             bool is_type; }                                       task_spec;
    struct { String_Slice name; Node_List declarations;
             Node_List statements; Node_List handlers;
             bool is_separate; }                                   task_body;
    struct { String_Slice name; Node_List parameters;
             Node_List index_constraints; }                        entry_decl;
    struct { Node_List names; Syntax_Node *param_type;
             Syntax_Node *default_expr;
             enum { MODE_IN, MODE_OUT, MODE_IN_OUT } mode; }     param_spec;
    struct { Node_List formals; Syntax_Node *unit; }              generic_decl;
    struct { Syntax_Node *generic_name; Node_List actuals;
             String_Slice instance_name;
             Token_Kind unit_kind; }                               generic_inst;
    struct { String_Slice name;
             enum { GEN_DEF_PRIVATE = 0, GEN_DEF_LIMITED_PRIVATE,
                    GEN_DEF_DISCRETE, GEN_DEF_INTEGER, GEN_DEF_FLOAT,
                    GEN_DEF_FIXED, GEN_DEF_ARRAY, GEN_DEF_ACCESS,
                    GEN_DEF_DERIVED } def_kind;
             Syntax_Node *def_detail;
             Node_List discriminants; }                            generic_type_param;
    struct { Node_List names; Syntax_Node *object_type;
             Syntax_Node *default_expr;
             enum { GEN_MODE_IN = 0, GEN_MODE_OUT,
                    GEN_MODE_IN_OUT } mode; }                     generic_object_param;
    struct { String_Slice name; Node_List parameters;
             Syntax_Node *return_type; Syntax_Node *default_name;
             bool is_function; bool default_box; }                generic_subprog_param;
    struct { Node_List names; }                                   use_clause;
    struct { String_Slice name; Node_List arguments; }            pragma_node;
    struct { Node_List exceptions; Node_List statements; }        handler;
    struct { Syntax_Node *entity_name; String_Slice attribute;
             Syntax_Node *expression; Node_List component_clauses;
             bool is_record_rep; bool is_enum_rep; }              rep_clause;
    struct { Node_List with_clauses;
             Node_List use_clauses; }                              context;
    struct { Syntax_Node *context; Syntax_Node *unit;
             Syntax_Node *separate_parent; }                       compilation_unit;
  };
};

Syntax_Node *Node_New (Node_Kind kind, Source_Location location);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §12. Parser
 *
 * Recursive descent mirrors the grammar: each nonterminal becomes a function, each
 * alternative becomes a branch.  Three unifying principles keep the parser small:
 *
 *   1. All X(...) forms parse as NK_APPLY.  Semantic analysis later distinguishes
 *      calls, indexing, slicing, and type conversions.
 *   2. One helper handles positional, named, and choice associations.
 *   3. One postfix loop handles .selector, 'attribute, and (args).
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  Lexer        lexer;
  Token        current_token;
  Token        previous_token;
  bool         had_error;
  bool         panic_mode;
  uint32_t     last_line;
  uint32_t     last_column;
  Token_Kind   last_kind;
} Parser;

typedef enum {
  PREC_NONE = 0, PREC_LOGICAL, PREC_RELATIONAL, PREC_ADDITIVE,
  PREC_MULTIPLICATIVE, PREC_EXPONENTIAL, PREC_UNARY, PREC_PRIMARY
} Precedence;

Parser           Parser_New               (const char *source, size_t length,
                                           const char *filename);
bool             Parser_At                (Parser *p, Token_Kind kind);
bool             Parser_At_Any            (Parser *p, Token_Kind k1,
                                           Token_Kind k2);
bool             Parser_Peek_At           (Parser *p, Token_Kind kind);
Token            Parser_Advance           (Parser *p);
bool             Parser_Match             (Parser *p, Token_Kind kind);
bool             Parser_Expect            (Parser *p, Token_Kind kind);
Source_Location   Parser_Location          (Parser *p);
String_Slice     Parser_Identifier        (Parser *p);
void             Parser_Error             (Parser *p, const char *message);
void             Parser_Error_At_Current  (Parser *p, const char *expected);
void             Parser_Synchronize       (Parser *p);
bool             Parser_Check_Progress    (Parser *p);
void             Parser_Check_End_Name    (Parser *p, String_Slice expected);

Precedence       Get_Infix_Precedence     (Token_Kind kind);
bool             Is_Right_Associative     (Token_Kind kind);

/* §12.1 Grammar productions ── expressions and names */
Syntax_Node *Parse_Expression             (Parser *p);
Syntax_Node *Parse_Expression_Precedence  (Parser *p, Precedence min_prec);
Syntax_Node *Parse_Unary                  (Parser *p);
Syntax_Node *Parse_Primary               (Parser *p);
Syntax_Node *Parse_Name                  (Parser *p);
Syntax_Node *Parse_Simple_Name           (Parser *p);
Syntax_Node *Parse_Choice                (Parser *p);
Syntax_Node *Parse_Range                 (Parser *p);
void         Parse_Association_List      (Parser *p, Node_List *list);
void         Parse_Parameter_List        (Parser *p, Node_List *params);

/* §12.2 Grammar productions ── type definitions */
Syntax_Node *Parse_Subtype_Indication    (Parser *p);
Syntax_Node *Parse_Type_Definition       (Parser *p);
Syntax_Node *Parse_Enumeration_Type      (Parser *p);
Syntax_Node *Parse_Array_Type            (Parser *p);
Syntax_Node *Parse_Record_Type           (Parser *p);
Syntax_Node *Parse_Access_Type           (Parser *p);
Syntax_Node *Parse_Derived_Type          (Parser *p);
Syntax_Node *Parse_Discrete_Range        (Parser *p);
Syntax_Node *Parse_Variant_Part          (Parser *p);
Syntax_Node *Parse_Discriminant_Part     (Parser *p);

/* §12.3 Grammar productions ── statements */
Syntax_Node *Parse_Statement             (Parser *p);
void         Parse_Statement_Sequence    (Parser *p, Node_List *list);
Syntax_Node *Parse_Assignment_Or_Call    (Parser *p);
Syntax_Node *Parse_Return_Statement      (Parser *p);
Syntax_Node *Parse_If_Statement          (Parser *p);
Syntax_Node *Parse_Case_Statement        (Parser *p);
Syntax_Node *Parse_Loop_Statement        (Parser *p, String_Slice label);
Syntax_Node *Parse_Block_Statement       (Parser *p, String_Slice label);
Syntax_Node *Parse_Exit_Statement        (Parser *p);
Syntax_Node *Parse_Goto_Statement        (Parser *p);
Syntax_Node *Parse_Raise_Statement       (Parser *p);
Syntax_Node *Parse_Delay_Statement       (Parser *p);
Syntax_Node *Parse_Abort_Statement       (Parser *p);
Syntax_Node *Parse_Accept_Statement      (Parser *p);
Syntax_Node *Parse_Select_Statement      (Parser *p);
Syntax_Node *Parse_Pragma                (Parser *p);

/* §12.4 Grammar productions ── declarations, packages, generics */
Syntax_Node *Parse_Declaration           (Parser *p);
void         Parse_Declarative_Part      (Parser *p, Node_List *list);
Syntax_Node *Parse_Object_Declaration    (Parser *p);
Syntax_Node *Parse_Type_Declaration      (Parser *p);
Syntax_Node *Parse_Subtype_Declaration   (Parser *p);
Syntax_Node *Parse_Procedure_Specification (Parser *p);
Syntax_Node *Parse_Function_Specification  (Parser *p);
Syntax_Node *Parse_Subprogram_Body       (Parser *p, Syntax_Node *spec);
Syntax_Node *Parse_Package_Specification (Parser *p);
Syntax_Node *Parse_Package_Body          (Parser *p);
void         Parse_Generic_Formal_Part   (Parser *p, Node_List *formals);
Syntax_Node *Parse_Generic_Declaration   (Parser *p);
Syntax_Node *Parse_Use_Clause            (Parser *p);
Syntax_Node *Parse_With_Clause           (Parser *p);
Syntax_Node *Parse_Representation_Clause (Parser *p);
Syntax_Node *Parse_Context_Clause        (Parser *p);
Syntax_Node *Parse_Compilation_Unit      (Parser *p);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §13. Type system
 *
 * Ada's types form a lattice: Boolean and Character are enumerations; Integer and
 * Modular are discrete; Fixed and Float are real; String is an array.  The type
 * descriptor carries kind, bounds, representation, and composite structure.
 *
 * INVARIANT: sizes in Type_Info are always BYTES, matching LLVM DataLayout.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum {
  TYPE_UNKNOWN = 0,
  TYPE_BOOLEAN, TYPE_CHARACTER, TYPE_INTEGER, TYPE_MODULAR,
  TYPE_ENUMERATION, TYPE_FLOAT, TYPE_FIXED,
  TYPE_ARRAY, TYPE_RECORD, TYPE_STRING, TYPE_ACCESS,
  TYPE_UNIVERSAL_INTEGER, TYPE_UNIVERSAL_REAL,
  TYPE_TASK, TYPE_SUBPROGRAM, TYPE_PRIVATE, TYPE_LIMITED_PRIVATE,
  TYPE_INCOMPLETE, TYPE_PACKAGE,
  TYPE_COUNT
} Type_Kind;

/* Scalar bounds: explicit tagged union.  int128_t supports the full Ada 2022 range. */
typedef struct {
  enum { BOUND_NONE, BOUND_INTEGER, BOUND_FLOAT, BOUND_EXPR } kind;
  union { int128_t int_value; double float_value; Syntax_Node *expr; };
  uint32_t cached_temp;
} Type_Bound;

/* Variant information for discriminated records (RM 3.7.3). */
typedef struct {
  int64_t  disc_value_low;
  int64_t  disc_value_high;
  bool     is_others;
  uint32_t first_component;
  uint32_t component_count;
  uint32_t variant_size;
} Variant_Info;

/* Record component descriptor. */
typedef struct {
  String_Slice  name;
  Type_Info    *component_type;
  uint32_t      byte_offset;
  uint32_t      bit_offset;
  uint32_t      bit_size;
  Syntax_Node  *default_expr;
  bool          is_discriminant;
  int32_t       variant_index;
} Component_Info;

/* Array dimension descriptor. */
typedef struct {
  Type_Info *index_type;
  Type_Bound low_bound;
  Type_Bound high_bound;
} Index_Info;

struct Type_Info {
  Type_Kind    kind;
  String_Slice name;
  Symbol      *defining_symbol;
  uint32_t     size;
  uint32_t     alignment;
  uint32_t     specified_bit_size;
  Type_Bound   low_bound;
  Type_Bound   high_bound;
  uint128_t    modulus;
  Type_Info   *base_type;
  Type_Info   *parent_type;

  union {
    struct { Index_Info *indices; uint32_t index_count;
             Type_Info *element_type; bool is_constrained; }      array;
    struct { Component_Info *components; uint32_t component_count;
             uint32_t discriminant_count; bool has_discriminants;
             bool all_defaults; bool is_constrained;
             Variant_Info *variants; uint32_t variant_count;
             uint32_t variant_offset; uint32_t max_variant_size;
             Syntax_Node *variant_part_node;
             int64_t *disc_constraint_values;
             Syntax_Node **disc_constraint_exprs;
             uint32_t *disc_constraint_preeval;
             bool has_disc_constraints; }                          record;
    struct { Type_Info *designated_type;
             bool is_access_constant; }                            access;
    struct { String_Slice *literals; uint32_t literal_count;
             int64_t *rep_values; }                                enumeration;
    struct { double delta; double small; int scale; }             fixed;
    struct { int digits; }                                        flt;
  };

  uint32_t     suppressed_checks;
  bool         is_packed;
  bool         is_limited;
  bool         is_frozen;
  int64_t      storage_size;
  const char  *equality_func_name;
  uint32_t     rt_global_id;
};

extern Type_Info *Frozen_Composite_Types[256];
extern uint32_t   Frozen_Composite_Count;
extern Symbol    *Exception_Symbols[256];
extern uint32_t   Exception_Symbol_Count;

/* §13.1 Type construction and freezing */
Type_Info  *Type_New             (Type_Kind kind, String_Slice name);
void        Freeze_Type          (Type_Info *type_info);

/* §13.2 Classification predicates */
bool Type_Is_Scalar              (const Type_Info *t);
bool Type_Is_Discrete            (const Type_Info *t);
bool Type_Is_Numeric             (const Type_Info *t);
bool Type_Is_Real                (const Type_Info *t);
bool Type_Is_Float_Representation(const Type_Info *t);
bool Type_Is_Array_Like          (const Type_Info *t);
bool Type_Is_Composite           (const Type_Info *t);
bool Type_Is_Access              (const Type_Info *t);
bool Type_Is_Record              (const Type_Info *t);
bool Type_Is_Task                (const Type_Info *t);
bool Type_Is_Float               (const Type_Info *t);
bool Type_Is_Fixed_Point         (const Type_Info *t);
bool Type_Is_Private             (const Type_Info *t);
bool Type_Is_Limited             (const Type_Info *t);
bool Type_Is_Integer_Like        (const Type_Info *t);
bool Type_Is_Unsigned            (const Type_Info *t);
bool Type_Is_Enumeration         (const Type_Info *t);
bool Type_Is_Boolean             (const Type_Info *t);
bool Type_Is_Character           (const Type_Info *t);
bool Type_Is_String              (const Type_Info *t);
bool Type_Is_Universal_Integer   (const Type_Info *t);
bool Type_Is_Universal_Real      (const Type_Info *t);
bool Type_Is_Universal           (const Type_Info *t);
bool Type_Is_Unconstrained_Array (const Type_Info *t);
bool Type_Is_Constrained_Array   (const Type_Info *t);
bool Type_Is_Bool_Array          (const Type_Info *t);
bool Type_Has_Dynamic_Bounds     (const Type_Info *t);
bool Type_Needs_Fat_Pointer      (const Type_Info *t);
bool Type_Needs_Fat_Pointer_Load (const Type_Info *t);

/* §13.3 Type chain navigation and bounds */
Type_Info  *Type_Base                    (Type_Info *t);
Type_Info  *Type_Root                    (Type_Info *t);
int128_t    Type_Bound_Value             (Type_Bound bound);
bool        Type_Bound_Is_Compile_Time_Known (Type_Bound b);
bool        Type_Bound_Is_Set            (Type_Bound b);
double      Type_Bound_Float_Value       (Type_Bound b);
int128_t    Array_Element_Count          (Type_Info *t);
int128_t    Array_Low_Bound              (Type_Info *t);

/* §13.4 LLVM type mapping */
const char *Type_To_Llvm                 (Type_Info *t);
const char *Type_To_Llvm_Sig             (Type_Info *t);
const char *Array_Bound_Llvm_Type        (const Type_Info *t);
const char *Bounds_Type_For              (const char *bound_type);
int         Bounds_Alloc_Size            (const char *bound_type);
const char *Float_Llvm_Type_Of           (const Type_Info *t);
bool        Float_Is_Single              (const Type_Info *t);
int         Float_Effective_Digits       (const Type_Info *t);
void        Float_Model_Parameters       (const Type_Info *t,
                                          int *mantissa, int *emin, int *emax);

/* §13.5 Expression type queries */
bool        Expression_Is_Slice              (const Syntax_Node *node);
bool        Expression_Produces_Fat_Pointer  (const Syntax_Node *node,
                                              const Type_Info *type);
bool        Expression_Is_Boolean            (Syntax_Node *node);
bool        Expression_Is_Float              (Syntax_Node *node);
const char *Expression_Llvm_Type             (Syntax_Node *node);

/* §13.6 Derived type operations (RM 3.4) */
bool        Types_Same_Named         (Type_Info *t1, Type_Info *t2);
bool        Subprogram_Is_Primitive_Of (Symbol *sub, Type_Info *type);
void        Create_Derived_Operation (Symbol *sub, Type_Info *derived_type,
                                      Type_Info *parent_type);
void        Derive_Subprograms      (Type_Info *derived_type,
                                      Type_Info *parent_type);

/* §13.7 Generic type map and check suppression */
extern struct {
  uint32_t count;
  struct { String_Slice formal_name; Type_Info *actual_type; } mappings[32];
} g_generic_type_map;

void        Set_Generic_Type_Map        (Symbol *inst);
Type_Info  *Resolve_Generic_Actual_Type (Type_Info *type);
bool        Check_Is_Suppressed         (Type_Info *type, Symbol *sym,
                                         uint32_t check_bit);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §14. Symbol table
 *
 * The symbol table maps names to meanings while the scope stack provides context.
 * Lexical scoping is a tree; visibility rules turn it into a forest.  Ada's overloading
 * rules require collecting all visible interpretations and disambiguating by type.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum {
  SYMBOL_UNKNOWN = 0, SYMBOL_VARIABLE, SYMBOL_CONSTANT,
  SYMBOL_TYPE, SYMBOL_SUBTYPE, SYMBOL_PROCEDURE, SYMBOL_FUNCTION,
  SYMBOL_PARAMETER, SYMBOL_PACKAGE, SYMBOL_EXCEPTION, SYMBOL_LABEL,
  SYMBOL_LOOP, SYMBOL_ENTRY, SYMBOL_COMPONENT, SYMBOL_DISCRIMINANT,
  SYMBOL_LITERAL, SYMBOL_GENERIC, SYMBOL_GENERIC_INSTANCE,
  SYMBOL_COUNT
} Symbol_Kind;

typedef enum { PARAM_IN = 0, PARAM_OUT, PARAM_IN_OUT } Parameter_Mode;

typedef struct {
  String_Slice    name;
  Type_Info      *param_type;
  Parameter_Mode  mode;
  Syntax_Node    *default_value;
  struct Symbol  *param_sym;
} Parameter_Info;

bool Param_Is_By_Reference (Parameter_Mode mode);

struct Symbol {
  Symbol_Kind     kind;
  String_Slice    name;
  Source_Location  location;
  Type_Info      *type;
  Scope          *defining_scope;
  Symbol         *parent;
  Symbol         *next_overload;
  Symbol         *next_in_bucket;
  enum { VIS_HIDDEN = 0, VIS_IMMEDIATELY_VISIBLE = 1,
         VIS_USE_VISIBLE = 2, VIS_DIRECTLY_VISIBLE = 3 } visibility;
  Syntax_Node    *declaration;

  /* Subprogram-specific */
  Parameter_Info *parameters;
  uint32_t        parameter_count;
  Type_Info      *return_type;

  /* Package-specific */
  Symbol        **exported;
  uint32_t        exported_count;

  /* Code generation */
  uint32_t        unique_id;
  uint32_t        nesting_level;
  int64_t         frame_offset;
  Scope          *scope;

  /* Pragma effects */
  bool            is_inline;
  bool            is_imported;
  bool            is_exported;
  String_Slice    external_name;
  String_Slice    link_name;
  enum { CONVENTION_ADA = 0, CONVENTION_C, CONVENTION_STDCALL,
         CONVENTION_INTRINSIC, CONVENTION_ASSEMBLER } convention;
  uint32_t        suppressed_checks;
  bool            is_unreferenced;

  /* Code-generation flags */
  bool            extern_emitted;
  bool            body_emitted;
  bool            is_named_number;
  bool            is_overloaded;
  bool            body_claimed;
  bool            is_predefined;
  bool            needs_address_marker;
  bool            is_identity_function;
  uint32_t        disc_agg_temp;
  bool            is_disc_constrained;
  bool            needs_fat_ptr_storage;

  /* Derived type operations (RM 3.4) */
  Symbol         *parent_operation;
  Type_Info      *derived_from_type;

  /* Labels and entries */
  uint32_t        llvm_label_id;
  uint32_t        loop_exit_label_id;
  uint32_t        entry_index;
  Syntax_Node    *renamed_object;

  /* Generic support */
  Syntax_Node    *generic_formals;
  Syntax_Node    *generic_unit;
  Syntax_Node    *generic_body;
  Symbol         *generic_template;
  Symbol         *instantiated_subprogram;
  struct {
    String_Slice  formal_name;
    Type_Info    *actual_type;
    Symbol       *actual_subprogram;
    Syntax_Node  *actual_expr;
    Token_Kind    builtin_operator;
  } *generic_actuals;
  uint32_t        generic_actual_count;
  Syntax_Node    *expanded_spec;
  Syntax_Node    *expanded_body;
};

struct Scope {
  Symbol  *buckets[SYMBOL_TABLE_SIZE];
  Scope   *parent;
  Symbol  *owner;
  uint32_t nesting_level;
  Symbol **symbols;
  uint32_t symbol_count;
  uint32_t symbol_capacity;
  int64_t  frame_size;
  Symbol **frame_vars;
  uint32_t frame_var_count;
  uint32_t frame_var_capacity;
};

typedef struct {
  Scope   *current_scope;
  Scope   *global_scope;
  Type_Info *type_boolean;
  Type_Info *type_integer;
  Type_Info *type_float;
  Type_Info *type_character;
  Type_Info *type_string;
  Type_Info *type_duration;
  Type_Info *type_universal_integer;
  Type_Info *type_universal_real;
  Type_Info *type_address;
  uint32_t   next_unique_id;
} Symbol_Manager;

extern Symbol_Manager *sm;

/* §14.1 Scope and symbol lifecycle */
Scope   *Scope_New                       (Scope *parent);
void     Symbol_Manager_Push_Scope       (Symbol *owner);
void     Symbol_Manager_Pop_Scope        (void);
void     Symbol_Manager_Push_Existing_Scope (Scope *scope);
uint32_t Symbol_Hash_Name               (String_Slice name);
Symbol  *Symbol_New                      (Symbol_Kind kind, String_Slice name,
                                          Source_Location location);
void     Symbol_Add                      (Symbol *sym);
Symbol  *Symbol_Find                     (String_Slice name);
Symbol  *Symbol_Find_By_Type             (String_Slice name,
                                          Type_Info *expected_type);
void     Symbol_Manager_Init_Predefined  (void);
void     Symbol_Manager_Init             (void);

/* §14.2 Overload resolution */
typedef struct {
  Type_Info **types;
  uint32_t    count;
  String_Slice *names;
} Argument_Info;

typedef struct {
  Symbol    *nam;
  Type_Info *typ;
  Type_Info *opnd_typ;
  bool       is_universal;
  uint32_t   scope_depth;
} Interpretation;

typedef struct {
  Interpretation items[MAX_INTERPRETATIONS];
  uint32_t       count;
} Interp_List;

bool     Type_Covers                     (Type_Info *expected,
                                          Type_Info *actual);
bool     Arguments_Match_Profile         (Symbol *sym, Argument_Info *args);
void     Collect_Interpretations         (String_Slice name,
                                          Interp_List *list);
void     Filter_By_Arguments             (Interp_List *interps,
                                          Argument_Info *args);
bool     Symbol_Hides                    (Symbol *sym1, Symbol *sym2);
int32_t  Score_Interpretation            (Interpretation *interp,
                                          Type_Info *context_type,
                                          Argument_Info *args);
Symbol  *Disambiguate                    (Interp_List *interps,
                                          Type_Info *context_type,
                                          Argument_Info *args);
Symbol  *Resolve_Overloaded_Call         (String_Slice name,
                                          Argument_Info *args,
                                          Type_Info *context_type);
String_Slice Operator_Name               (Token_Kind op);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §15. Semantic analysis
 *
 * The semantic pass walks the AST to resolve names, check types, fold constants,
 * and freeze type representations.  It is the bridge between parsing (which knows
 * only syntax) and code generation (which needs fully-resolved types and symbols).
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

Type_Info   *Resolve_Expression          (Syntax_Node *node);
Type_Info   *Resolve_Identifier          (Syntax_Node *node);
Type_Info   *Resolve_Selected            (Syntax_Node *node);
Type_Info   *Resolve_Binary_Op           (Syntax_Node *node);
Type_Info   *Resolve_Apply               (Syntax_Node *node);
bool         Resolve_Char_As_Enum        (Syntax_Node *char_node,
                                          Type_Info *enum_type);
void         Resolve_Statement           (Syntax_Node *node);
void         Resolve_Declaration         (Syntax_Node *node);
void         Resolve_Declaration_List    (Node_List *list);
void         Resolve_Statement_List      (Node_List *list);
void         Resolve_Compilation_Unit    (Syntax_Node *node);
void         Freeze_Declaration_List     (Node_List *list);
void         Populate_Package_Exports    (Symbol *pkg_sym,
                                          Syntax_Node *pkg_spec);
void         Preregister_Labels          (Node_List *list);
void         Install_Declaration_Symbols (Node_List *decls);

/* Constant folding */
bool         Is_Integer_Expr             (Syntax_Node *node);
double       Eval_Const_Numeric          (Syntax_Node *node);
bool         Eval_Const_Rational         (Syntax_Node *node, Rational *out);
const char  *I128_Decimal                (int128_t value);
const char  *U128_Decimal                (uint128_t value);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §16. Code generator
 *
 * The code generator walks the resolved AST and emits LLVM IR to a FILE*.  Every
 * Ada construct maps to a sequence of LLVM instructions; the generator tracks temp
 * registers, labels, string constants, and deferred nested bodies.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  FILE         *output;
  uint32_t      temp_id;
  uint32_t      label_id;
  uint32_t      global_id;
  uint32_t      string_id;
  Symbol       *current_function;
  uint32_t      current_nesting_level;
  Symbol       *current_instance;
  uint32_t      loop_exit_label;
  uint32_t      loop_continue_label;
  bool          has_return;
  bool          block_terminated;
  bool          header_emitted;
  Symbol       *main_candidate;
  Syntax_Node  *deferred_bodies[64];
  uint32_t      deferred_count;
  Symbol       *enclosing_function;
  bool          is_nested;
  uint32_t      exception_handler_label;
  uint32_t      exception_jmp_buf;
  bool          in_exception_region;
  char         *string_const_buffer;
  size_t        string_const_size;
  size_t        string_const_capacity;
  Symbol       *address_markers[256];
  uint32_t      address_marker_count;
  uint32_t      emitted_func_ids[1024];
  uint32_t      emitted_func_count;
  bool          in_task_body;
  Symbol       *elab_funcs[64];
  uint32_t      elab_func_count;
  uint32_t      temp_type_keys[TEMP_TYPE_CAPACITY];
  const char   *temp_types[TEMP_TYPE_CAPACITY];
  uint8_t       temp_is_fat_alloca[TEMP_TYPE_CAPACITY];
  char         *exc_refs[EXC_REF_CAPACITY];
  uint32_t      exc_ref_count;
  bool          needs_trim_helpers;
  uint32_t      rt_type_counter;
  uint32_t      in_agg_component;
  uint32_t      inner_agg_bnd_lo[MAX_AGG_DIMS];
  uint32_t      inner_agg_bnd_hi[MAX_AGG_DIMS];
  int           inner_agg_bnd_n;
  uint32_t      disc_cache[MAX_DISC_CACHE];
  uint32_t      disc_cache_count;
  Type_Info    *disc_cache_type;
} Code_Generator;

extern Code_Generator *cg;

/* §16.1 Generator lifecycle */
void Code_Generator_Init (FILE *output);

/* §16.2 Temp register and label management */
uint32_t    Emit_Temp                    (void);
uint32_t    Emit_Label                   (void);
void        Temp_Set_Type                (uint32_t temp_id,
                                          const char *llvm_type);
const char *Temp_Get_Type                (uint32_t temp_id);
void        Temp_Mark_Fat_Alloca         (uint32_t temp_id);
bool        Temp_Is_Fat_Alloca           (uint32_t temp_id);

/* §16.3 IR emission primitives */
void        Emit                         (const char *format, ...);
void        Emit_Location                (Source_Location location);
void        Emit_Label_Here              (uint32_t label);
void        Emit_Branch_If_Needed        (uint32_t label);
void        Emit_String_Const            (const char *format, ...);
void        Emit_String_Const_Char       (char ch);
void        Emit_Float_Constant          (uint32_t result,
                                          const char *type, double value);

/* §16.4 String and type helpers */
const char *String_Bound_Type            (void);
const char *String_Bounds_Struct         (void);
int         String_Bounds_Alloc          (void);
const char *Integer_Arith_Type           (void);
int         Type_Bits                    (const char *llvm_type);
const char *Wider_Int_Type               (const char *left,
                                          const char *right);
bool        Is_Float_Type                (const char *llvm_type);
const char *Float_Cmp_Predicate          (int op);
const char *Int_Cmp_Predicate            (int op, bool is_unsigned);

/* §16.5 Symbol mangling and references */
bool         Symbol_Is_Global            (Symbol *sym);
size_t       Mangle_Slice_Into           (char *buf, size_t pos,
                                          size_t max, String_Slice name);
size_t       Mangle_Into_Buffer          (char *buf, size_t pos,
                                          size_t max, Symbol *sym);
String_Slice Symbol_Mangle_Name          (Symbol *sym);
String_Slice Mangle_Qualified_Name       (String_Slice parent,
                                          String_Slice name);
void         Emit_Symbol_Name            (Symbol *sym);
void         Emit_Symbol_Ref             (Symbol *sym);
void         Emit_Symbol_Storage         (Symbol *sym);
void         Emit_Exception_Ref          (Symbol *exc);
Symbol      *Find_Instance_Local         (const Symbol *template_sym);

/* §16.6 Static link support */
Symbol      *Find_Enclosing_Subprogram   (Symbol *sym);
bool         Subprogram_Needs_Static_Chain (Symbol *sym);
bool         Is_Uplevel_Access           (const Symbol *sym);
int          Nested_Frame_Depth          (Symbol *proc);
uint32_t     Precompute_Nested_Frame_Arg (Symbol *proc);
bool         Emit_Nested_Frame_Arg       (Symbol *proc, uint32_t precomp);

/* §16.7 Checks and conversions */
void         Emit_Raise_Exception        (const char *exc_name,
                                          const char *comment);
void         Emit_Check_With_Raise       (uint32_t cond,
                                          const char *exc, const char *msg);
void         Emit_Range_Check_With_Raise (uint32_t val, uint32_t lo,
                                          uint32_t hi, const char *ty,
                                          bool is_unsigned);
uint32_t     Emit_Overflow_Checked_Op    (uint32_t left, uint32_t right,
                                          const char *type, const char *op,
                                          bool is_unsigned);
void         Emit_Division_Check         (uint32_t divisor,
                                          const char *type, Type_Info *t);
void         Emit_Signed_Division_Overflow_Check (uint32_t dividend,
                                          uint32_t divisor, const char *ty);
uint32_t     Emit_Convert                (uint32_t src, const char *from,
                                          const char *to);
uint32_t     Emit_Convert_Ext            (uint32_t src, const char *src_type,
                                          const char *dst_type,
                                          bool is_unsigned);
uint32_t     Emit_Coerce                 (uint32_t temp, const char *from,
                                          const char *to);
uint32_t     Emit_Coerce_Default_Int     (uint32_t temp, const char *from);
uint32_t     Emit_Index_Check            (uint32_t index, uint32_t lo,
                                          uint32_t hi, const char *ty);
void         Emit_Length_Check           (uint32_t src_length,
                                          uint32_t dst_length,
                                          const char *ty);
void         Emit_Access_Check           (uint32_t ptr_val,
                                          Type_Info *acc_type);
void         Emit_Discriminant_Check     (uint32_t actual, uint32_t expected,
                                          const char *ty);
uint32_t     Emit_Constraint_Check       (uint32_t val, Type_Info *type);
uint32_t     Emit_Constraint_Check_With_Type (uint32_t val, Type_Info *type,
                                          const char *target_type);
void         Emit_Subtype_Constraint_Compat_Check (Type_Info *subtype);
uint32_t     Emit_Widen_For_Intrinsic    (uint32_t val, const char *from,
                                          const char *to);
uint32_t     Emit_Extend_To_I64          (uint32_t val, const char *from);

/* §16.8 Fat-pointer operations */
uint32_t     Emit_Fat_Pointer            (uint32_t data_ptr, uint32_t lo,
                                          uint32_t hi, const char *bt);
uint32_t     Emit_Fat_Pointer_Dynamic    (uint32_t data_ptr, uint32_t lo,
                                          uint32_t hi, const char *bt);
uint32_t     Emit_Fat_Pointer_Heap       (uint32_t data_ptr, uint32_t lo,
                                          uint32_t hi, const char *bt);
uint32_t     Emit_Fat_Pointer_MultiDim   (uint32_t data_ptr,
                                          uint32_t *lo, uint32_t *hi,
                                          uint32_t ndims, const char *bt);
uint32_t     Emit_Fat_Pointer_Null       (const char *bt);
uint32_t     Fat_Ptr_As_Value            (uint32_t fat_ptr);
uint32_t     Emit_Fat_Pointer_Data       (uint32_t fat_ptr, const char *bt);
uint32_t     Emit_Fat_Pointer_Bound      (uint32_t fat_ptr, const char *bt,
                                          int which);
uint32_t     Emit_Fat_Pointer_Low_Dim    (uint32_t fat_ptr, const char *bt,
                                          uint32_t dim);
uint32_t     Emit_Fat_Pointer_High_Dim   (uint32_t fat_ptr, const char *bt,
                                          uint32_t dim);
uint32_t     Emit_Fat_Pointer_Length     (uint32_t fat_ptr, const char *bt);
uint32_t     Emit_Fat_Pointer_Length_Dim (uint32_t fat_ptr, const char *bt,
                                          uint32_t dim);
uint32_t     Emit_Fat_Pointer_Compare    (uint32_t left_fat,
                                          uint32_t right_fat, const char *bt);
uint32_t     Emit_Load_Fat_Pointer       (Symbol *sym, const char *bt);
uint32_t     Emit_Load_Fat_Pointer_From_Temp (uint32_t ptr, const char *bt);
void         Emit_Store_Fat_Pointer_To_Symbol (uint32_t fat, Symbol *sym,
                                          const char *bt);
void         Emit_Store_Fat_Pointer_Fields_To_Temp (uint32_t data,
                                          uint32_t lo, uint32_t hi,
                                          uint32_t dest, const char *bt);
void         Emit_Fat_Pointer_Copy_To_Name (uint32_t fat_ptr,
                                          const char *name, const char *bt);
void         Emit_Fat_To_Array_Memcpy    (uint32_t fat_val,
                                          uint32_t dest_ptr, Type_Info *t);
uint32_t     Emit_Alloc_Bounds_Struct    (uint32_t lo, uint32_t hi,
                                          const char *bt);
uint32_t     Emit_Alloc_Bounds_MultiDim  (uint32_t *lo, uint32_t *hi,
                                          uint32_t ndims, const char *bt);
uint32_t     Emit_Heap_Bounds_Struct     (uint32_t lo, uint32_t hi,
                                          const char *bt);
void         Emit_Fat_Pointer_Insertvalue_Named (const char *prefix,
                                          const char *data_expr,
                                          const char *bounds_expr,
                                          const char *bt);
void         Emit_Fat_Pointer_Extractvalue_Named (const char *src,
                                          const char *data_name,
                                          const char *bounds_name,
                                          const char *bt);
void         Emit_Widen_Named_For_Intrinsic (const char *src,
                                          const char *dst, const char *bt);
void         Emit_Narrow_Named_From_Intrinsic (const char *src,
                                          const char *dst, const char *bt);

/* §16.9 Bound and length emission */
typedef struct {
  uint32_t    low_temp;
  uint32_t    high_temp;
  const char *bound_type;
} Bound_Temps;

uint32_t     Emit_Bound_Value            (Type_Bound *bound);
uint32_t     Emit_Bound_Value_Typed      (Type_Bound *bound, const char *ty);
uint32_t     Emit_Single_Bound           (Type_Bound *bound, const char *ty);
Bound_Temps  Emit_Bounds                 (Type_Info *type, uint32_t dim);
Bound_Temps  Emit_Bounds_From_Fat        (uint32_t fat, const char *bt);
Bound_Temps  Emit_Bounds_From_Fat_Dim    (uint32_t fat, const char *bt,
                                          uint32_t dim);
uint32_t     Emit_Length_From_Bounds     (uint32_t lo, uint32_t hi,
                                          const char *bt);
uint32_t     Emit_Length_Clamped         (uint32_t lo, uint32_t hi,
                                          const char *bt);
uint32_t     Emit_Static_Int            (int128_t value, const char *ty);
uint32_t     Emit_Type_Bound            (Type_Bound *bound, const char *ty);
uint32_t     Emit_Min_Value             (uint32_t left, uint32_t right,
                                          const char *ty);
uint32_t     Emit_Memcmp_Eq             (uint32_t left_ptr,
                                          uint32_t right_ptr,
                                          uint32_t byte_size, const char *ty);
uint32_t     Emit_Array_Lex_Compare     (uint32_t left_ptr,
                                          uint32_t right_ptr,
                                          uint32_t elem_size, const char *bt);

/* §16.10 Exception handling */
typedef struct {
  uint32_t handler_frame;
  uint32_t jmp_buf;
  uint32_t normal_label;
  uint32_t handler_label;
} Exception_Setup;

Exception_Setup Emit_Exception_Handler_Setup (void);
uint32_t     Emit_Current_Exception_Id   (void);
void         Generate_Exception_Dispatch (Node_List *handlers,
                                          uint32_t handler_label);

/* §16.11 Expression and statement generation */
uint32_t     Generate_Expression         (Syntax_Node *node);
uint32_t     Generate_Lvalue             (Syntax_Node *node);
uint32_t     Generate_Bound_Value        (Type_Bound bound, const char *ty);
uint32_t     Generate_Integer_Literal    (Syntax_Node *node);
uint32_t     Generate_Real_Literal       (Syntax_Node *node);
uint32_t     Generate_String_Literal     (Syntax_Node *node);
uint32_t     Generate_Identifier         (Syntax_Node *node);
uint32_t     Generate_Binary_Op          (Syntax_Node *node);
uint32_t     Generate_Unary_Op           (Syntax_Node *node);
uint32_t     Generate_Apply              (Syntax_Node *node);
uint32_t     Generate_Selected           (Syntax_Node *node);
uint32_t     Generate_Attribute          (Syntax_Node *node);
uint32_t     Generate_Qualified          (Syntax_Node *node);
uint32_t     Generate_Allocator          (Syntax_Node *node);
uint32_t     Generate_Aggregate          (Syntax_Node *node);
uint32_t     Generate_Array_Equality     (uint32_t left_ptr,
                                          uint32_t right_ptr, Type_Info *t);
uint32_t     Generate_Record_Equality    (uint32_t left_ptr,
                                          uint32_t right_ptr, Type_Info *t);
uint32_t     Generate_Composite_Address  (Syntax_Node *node);
uint32_t     Normalize_To_Fat_Pointer    (Syntax_Node *expr, uint32_t raw,
                                          Type_Info *type, const char *bt);
bool         Aggregate_Produces_Fat_Pointer (const Type_Info *t);
void         Ensure_Runtime_Type_Globals (Type_Info *t);
uint32_t     Wrap_Constrained_As_Fat     (Syntax_Node *expr, Type_Info *type,
                                          const char *bt);
uint32_t     Convert_Real_To_Fixed       (uint32_t val, double small,
                                          const char *fix_type);
uint32_t     Emit_Bool_Array_Binop       (uint32_t left, uint32_t right,
                                          Type_Info *result_type,
                                          const char *ir_op);
uint32_t     Emit_Bool_Array_Not         (uint32_t operand,
                                          Type_Info *result_type);
uint32_t     Get_Dimension_Index         (Syntax_Node *arg);
void         Emit_Float_Type_Limit       (uint32_t t, Type_Info *type,
                                          String_Slice attr_name);
uint32_t     Emit_Bound_Attribute        (uint32_t t, Type_Info *type,
                                          String_Slice attr_name);
uint32_t     Emit_Disc_Constraint_Value  (Type_Info *type_info,
                                          uint32_t disc_index,
                                          const char *disc_type);
void         Emit_Nested_Disc_Checks     (Type_Info *parent_type);
void         Emit_Comp_Disc_Check        (uint32_t ptr, Type_Info *type,
                                          uint32_t comp_idx);

/* §16.12 Statement generation */
void         Generate_Statement          (Syntax_Node *node);
void         Generate_Statement_List     (Node_List *list);
void         Generate_Assignment         (Syntax_Node *node);
void         Generate_If_Statement       (Syntax_Node *node);
void         Generate_Loop_Statement     (Syntax_Node *node);
void         Generate_For_Loop           (Syntax_Node *node);
void         Generate_Return_Statement   (Syntax_Node *node);
void         Generate_Case_Statement     (Syntax_Node *node);
void         Generate_Block_Statement    (Syntax_Node *node);
void         Generate_Raise_Statement    (Syntax_Node *node);

/* §16.13 Declaration and subprogram generation */
void         Generate_Declaration        (Syntax_Node *node);
void         Generate_Declaration_List   (Node_List *list);
void         Generate_Object_Declaration (Syntax_Node *node);
void         Generate_Subprogram_Body    (Syntax_Node *node);
void         Generate_Task_Body          (Syntax_Node *node);
void         Generate_Generic_Instance_Body (Symbol *inst_sym,
                                          Syntax_Node *template_body);
void         Emit_Extern_Subprogram      (Symbol *sym);
void         Emit_Function_Header        (Symbol *sym, bool is_nested);
bool         Is_Builtin_Function         (String_Slice name);
bool         Has_Nested_Subprograms      (Node_List *declarations,
                                          Node_List *statements);
bool         Has_Nested_In_Statements    (Node_List *statements);
Syntax_Node *Find_Homograph_Body         (Symbol **exports, uint32_t idx,
                                          Node_List *decls);
void         Process_Deferred_Bodies     (uint32_t saved_deferred_count);
void         Emit_Task_Function_Name     (Symbol *task_sym,
                                          String_Slice fallback_name);

/* §16.14 Aggregate helpers */
typedef struct {
  uint32_t n_positional;
  bool     has_named;
  bool     has_others;
  Syntax_Node *others_expr;
} Agg_Class;

Agg_Class    Agg_Classify                (Syntax_Node *node);
bool         Bound_Pair_Overflows        (Type_Bound low, Type_Bound high);
uint32_t     Agg_Resolve_Elem            (Syntax_Node *expr,
                                          Type_Info *elem_type);
void         Agg_Store_At_Static         (uint32_t base, uint32_t val,
                                          uint32_t index, Type_Info *t);
void         Agg_Store_At_Dynamic        (uint32_t base, uint32_t idx_temp,
                                          uint32_t val, Type_Info *t);
bool         Agg_Elem_Is_Composite       (Type_Info *elem, bool multidim);
uint32_t     Agg_Comp_Byte_Size          (Type_Info *ti, Syntax_Node *src);
void         Agg_Rec_Store               (uint32_t val, uint32_t dest_ptr,
                                          Component_Info *comp, Type_Info *t);
void         Agg_Rec_Disc_Post           (uint32_t val, Component_Info *comp,
                                          uint32_t disc_ordinal,
                                          Type_Info *type);
uint32_t     Disc_Ordinal_Before         (Type_Info *type_info,
                                          uint32_t comp_index);
int32_t      Find_Record_Component       (Type_Info *record_type,
                                          String_Slice name);
bool         Is_Others_Choice            (Syntax_Node *choice);
bool         Is_Static_Int_Node          (Syntax_Node *n);
int128_t     Static_Int_Value            (Syntax_Node *n);
void         Desugar_Aggregate_Range_Choices (Syntax_Node *agg);
void         Emit_Inner_Consistency_Track (uint32_t lo, uint32_t hi,
                                          const char *bt);
void         Collect_Disc_Symbols_In_Expr (Syntax_Node *node,
                                          Symbol **syms, uint32_t *count);

/* §16.15 Top-level unit generation */
void         Generate_Type_Equality_Function (Type_Info *t);
void         Generate_Implicit_Operators (void);
void         Generate_Exception_Globals  (void);
void         Generate_Extern_Declarations (Syntax_Node *node);
void         Generate_Compilation_Unit   (Syntax_Node *node);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §17. Library information (ALI)
 *
 * GNAT-compatible Ada Library Information files record dependencies, checksums,
 * and exported symbols so the binder can check consistency and the linker can
 * resolve cross-unit references.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  String_Slice unit_name;
  String_Slice source_name;
  uint32_t     source_checksum;
  bool         is_body;
  bool         is_generic;
  bool         is_preelaborate;
  bool         is_pure;
  bool         has_elaboration;
} Unit_Info;

typedef struct {
  String_Slice name;
  String_Slice source_file;
  String_Slice ali_file;
  bool         is_limited;
  bool         elaborate;
  bool         elaborate_all;
} With_Info;

typedef struct {
  String_Slice source_file;
  uint32_t     timestamp;
  uint32_t     checksum;
} Dependency_Info;

typedef struct {
  String_Slice name;
  String_Slice mangled_name;
  char         kind;
  uint32_t     line;
  String_Slice type_name;
  String_Slice llvm_type;
  uint32_t     param_count;
} Export_Info;

typedef struct {
  Unit_Info       units[8];
  uint32_t        unit_count;
  With_Info       withs[64];
  uint32_t        with_count;
  Dependency_Info deps[128];
  uint32_t        dep_count;
  Export_Info      exports[256];
  uint32_t        export_count;
} ALI_Info;

typedef struct {
  char         kind;
  char        *name;
  char        *mangled_name;
  char        *llvm_type;
  uint32_t     line;
  char        *type_name;
  uint32_t     param_count;
} ALI_Export;

typedef struct ALI_Cache_Entry_Forward {
  char        *unit_name;
  char        *source_file;
  char        *ali_file;
  uint32_t     checksum;
  bool         is_spec;
  bool         is_generic;
  bool         is_preelaborate;
  bool         is_pure;
  bool         loaded;
  char        *withs[64];
  uint32_t     with_count;
  ALI_Export   exports[256];
  uint32_t     export_count;
} ALI_Cache_Entry;

extern ALI_Cache_Entry ALI_Cache[256];
extern uint32_t        ALI_Cache_Count;
extern uint32_t        Crc32_Table[256];
extern bool            Crc32_Table_Initialized;

void             Crc32_Init_Table        (void);
uint32_t         Crc32                   (const char *data, size_t length);
void             Unit_Name_To_File       (String_Slice unit_name,
                                          bool is_body, char *buf, size_t sz);
void             ALI_Collect_Withs       (ALI_Info *ali, Syntax_Node *ctx);
String_Slice     Get_Subprogram_Name     (Syntax_Node *node);
String_Slice     LLVM_Type_Basic         (String_Slice ada_type);
void             ALI_Collect_Exports     (ALI_Info *ali, Syntax_Node *unit);
void             ALI_Collect_Unit        (ALI_Info *ali, Syntax_Node *cu,
                                          const char *source, size_t len);
void             ALI_Write               (FILE *out, ALI_Info *ali);
void             Generate_ALI_File       (const char *output_path,
                                          Syntax_Node *cu,
                                          const char *source, size_t len);
const char      *ALI_Skip_Ws            (const char *cursor);
const char      *ALI_Read_Token         (const char *cursor,
                                          char *buf, size_t bufsize);
uint32_t         ALI_Parse_Hex          (const char *str);
ALI_Cache_Entry *ALI_Read               (const char *ali_path);
bool             ALI_Is_Current         (const char *ali_path,
                                          const char *source_path);
void             ALI_Load_Symbols       (ALI_Cache_Entry *entry);
char            *ALI_Find               (String_Slice unit_name);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §18. Elaboration model
 *
 * Multi-unit Ada programs must elaborate package bodies in an order that satisfies
 * WITH-clause and Elaborate_All dependencies.  The elaboration model builds a
 * directed graph of compilation units, finds strongly connected components via
 * Tarjan's algorithm, and produces a topological ordering.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum {
  UNIT_SPEC, UNIT_BODY, UNIT_SPEC_ONLY, UNIT_BODY_ONLY
} Elab_Unit_Kind;

typedef enum {
  EDGE_WITH, EDGE_ELABORATE, EDGE_ELABORATE_ALL,
  EDGE_SPEC_BEFORE_BODY, EDGE_INVOCATION, EDGE_FORCED
} Elab_Edge_Kind;

typedef enum { PREC_HIGHER, PREC_EQUAL, PREC_LOWER } Elab_Precedence;

typedef enum {
  ELAB_ORDER_OK, ELAB_ORDER_HAS_CYCLE, ELAB_ORDER_HAS_ELABORATE_ALL_CYCLE
} Elab_Order_Status;

typedef struct Elab_Vertex Elab_Vertex;
typedef struct Elab_Edge   Elab_Edge;

struct Elab_Vertex {
  uint32_t         id;
  String_Slice     name;
  Elab_Unit_Kind   kind;
  Symbol          *symbol;
  uint32_t         component_id;
  uint32_t         pending_strong;
  uint32_t         pending_weak;
  bool             in_elab_order;
  bool             is_preelaborate;
  bool             is_pure;
  bool             has_elab_body;
  bool             is_predefined;
  bool             is_internal;
  bool             needs_elab_code;
  Elab_Vertex     *body_vertex;
  Elab_Vertex     *spec_vertex;
  uint32_t         first_pred_edge;
  uint32_t         first_succ_edge;
  int32_t          tarjan_index;
  int32_t          tarjan_lowlink;
  bool             tarjan_on_stack;
};

struct Elab_Edge {
  uint32_t         id;
  Elab_Edge_Kind   kind;
  bool             is_strong;
  uint32_t         pred_vertex_id;
  uint32_t         succ_vertex_id;
  uint32_t         next_pred_edge;
  uint32_t         next_succ_edge;
};

typedef struct {
  uint64_t bits[(ELAB_MAX_VERTICES + 63) / 64];
} Elab_Vertex_Set;

typedef struct {
  Elab_Vertex   vertices[ELAB_MAX_VERTICES];
  uint32_t      vertex_count;
  Elab_Edge     edges[ELAB_MAX_EDGES];
  uint32_t      edge_count;
  uint32_t      component_pending_strong[ELAB_MAX_COMPONENTS];
  uint32_t      component_pending_weak[ELAB_MAX_COMPONENTS];
  uint32_t      component_count;
  Elab_Vertex  *order[ELAB_MAX_VERTICES];
  uint32_t      order_count;
  bool          has_elaborate_all_cycle;
} Elab_Graph;

typedef struct {
  uint32_t stack[ELAB_MAX_VERTICES];
  uint32_t stack_top;
  int32_t  index;
} Tarjan_State;

typedef bool (*Elab_Vertex_Pred)(const Elab_Vertex *v);
typedef Elab_Precedence (*Elab_Vertex_Cmp)(const Elab_Graph *,
                                           const Elab_Vertex *,
                                           const Elab_Vertex *);

bool             Edge_Kind_Is_Strong      (Elab_Edge_Kind k);
Elab_Graph       Elab_Graph_New           (void);
uint32_t         Elab_Add_Vertex          (Elab_Graph *g, String_Slice name,
                                           Elab_Unit_Kind kind, Symbol *sym);
uint32_t         Elab_Find_Vertex         (const Elab_Graph *g,
                                           String_Slice name,
                                           Elab_Unit_Kind kind);
Elab_Vertex     *Elab_Get_Vertex          (Elab_Graph *g, uint32_t id);
const Elab_Vertex *Elab_Get_Vertex_Const  (const Elab_Graph *g, uint32_t id);
uint32_t         Elab_Add_Edge            (Elab_Graph *g, uint32_t pred,
                                           uint32_t succ, Elab_Edge_Kind k);
void             Tarjan_Strongconnect     (Elab_Graph *g, Tarjan_State *s,
                                           uint32_t v_id);
void             Elab_Find_Components     (Elab_Graph *g);
bool             Elab_Is_Elaborable       (const Elab_Vertex *v);
bool             Elab_Is_Weakly_Elaborable(const Elab_Vertex *v);
bool             Elab_Has_Elaborable_Body (const Elab_Graph *g,
                                           const Elab_Vertex *v);
Elab_Precedence  Elab_Compare_Vertices    (const Elab_Graph *g,
                                           const Elab_Vertex *a,
                                           const Elab_Vertex *b);
Elab_Precedence  Elab_Compare_Weak        (const Elab_Graph *g,
                                           const Elab_Vertex *a,
                                           const Elab_Vertex *b);
Elab_Vertex_Set  Elab_Set_Empty           (void);
bool             Elab_Set_Contains        (const Elab_Vertex_Set *s,
                                           uint32_t id);
void             Elab_Set_Insert          (Elab_Vertex_Set *s, uint32_t id);
void             Elab_Set_Remove          (Elab_Vertex_Set *s, uint32_t id);
uint32_t         Elab_Set_Size            (const Elab_Vertex_Set *s);
uint32_t         Elab_Find_Best_Vertex    (const Elab_Graph *g,
                                           const Elab_Vertex_Set *cands,
                                           Elab_Vertex_Pred pred,
                                           Elab_Vertex_Cmp cmp);
void             Elab_Update_Successor    (Elab_Graph *g, uint32_t edge_id,
                                           Elab_Vertex_Set *ready);
void             Elab_Elaborate_Vertex    (Elab_Graph *g, uint32_t v_id,
                                           Elab_Vertex_Set *ready);
Elab_Order_Status Elab_Elaborate_Graph    (Elab_Graph *g);
void             Elab_Pair_Specs_Bodies   (Elab_Graph *g);

extern bool g_elab_graph_initialized;
void             Elab_Init                (void);
uint32_t         Elab_Register_Unit       (String_Slice name, bool is_body,
                                           Symbol *sym);
Elab_Order_Status Elab_Compute_Order      (void);
uint32_t         Elab_Get_Order_Count     (void);
Symbol          *Elab_Get_Order_Symbol    (uint32_t index);
bool             Elab_Needs_Elab_Call     (uint32_t index);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §19. Build-in-place (BIP)
 *
 * Ada limited types cannot be copied (RM 7.5), so functions returning them must
 * build the result directly in the caller's storage.  The BIP protocol passes extra
 * implicit formals that carry the allocation strategy and destination address.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum {
  BIP_ALLOC_UNSPECIFIED = 0, BIP_ALLOC_CALLER = 1,
  BIP_ALLOC_SECONDARY = 2, BIP_ALLOC_GLOBAL_HEAP = 3,
  BIP_ALLOC_USER_POOL = 4
} BIP_Alloc_Form;

typedef enum {
  BIP_FORMAL_ALLOC_FORM, BIP_FORMAL_STORAGE_POOL,
  BIP_FORMAL_FINALIZATION, BIP_FORMAL_TASK_MASTER,
  BIP_FORMAL_ACTIVATION, BIP_FORMAL_OBJECT_ACCESS
} BIP_Formal_Kind;

typedef struct {
  Symbol *func; Type_Info *result_type; BIP_Alloc_Form alloc_form;
  uint32_t dest_ptr; bool needs_finalization; bool has_tasks;
} BIP_Context;

typedef struct {
  bool     is_bip_function;
  uint32_t bip_alloc_param;
  uint32_t bip_access_param;
  uint32_t bip_master_param;
  uint32_t bip_chain_param;
  bool     has_task_components;
} BIP_Function_State;

extern BIP_Function_State g_bip_state;

bool         BIP_Is_Explicitly_Limited       (const Type_Info *t);
bool         BIP_Is_Task_Type                (const Type_Info *t);
bool         BIP_Record_Has_Limited_Component(const Type_Info *t);
bool         BIP_Is_Limited_Type             (const Type_Info *t);
bool         BIP_Is_BIP_Function             (const Symbol *func);
bool         BIP_Type_Has_Task_Component     (const Type_Info *t);
bool         BIP_Needs_Alloc_Form            (const Symbol *func);
uint32_t     BIP_Extra_Formal_Count          (const Symbol *func);
BIP_Alloc_Form BIP_Determine_Alloc_Form     (bool is_allocator,
                                              bool has_pool);
void         BIP_Begin_Function              (const Symbol *func);
bool         BIP_In_BIP_Function             (void);
void         BIP_End_Function                (void);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §20. Generics
 *
 * Generic units are instantiated by macro-style expansion: the template AST is deep-
 * cloned with formal-to-actual substitution, then resolved and code-generated as if
 * the programmer had written the expanded text by hand.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  String_Slice  formal_name;
  Type_Info    *actual_type;
  Symbol       *actual_symbol;
  Syntax_Node  *actual_expr;
} Generic_Mapping;

typedef struct {
  Generic_Mapping  mappings[32];
  uint32_t         count;
  Symbol          *instance_sym;
  Symbol          *template_sym;
} Instantiation_Env;

Type_Info   *Env_Lookup_Type             (Instantiation_Env *env,
                                          String_Slice name);
Syntax_Node *Env_Lookup_Expr             (Instantiation_Env *env,
                                          String_Slice name);
Syntax_Node *Node_Deep_Clone             (Syntax_Node *node,
                                          Instantiation_Env *env, int depth);
void         Node_List_Clone             (Node_List *dst, Node_List *src,
                                          Instantiation_Env *env, int depth);
void         Build_Instantiation_Env     (Instantiation_Env *env,
                                          Symbol *inst, Symbol *tmpl);
void         Expand_Generic_Package      (Symbol *instance_sym);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §21. Include paths and unit loading
 *
 * WITH clauses are resolved by searching a list of include directories for the
 * corresponding source file, loading it, and recursively resolving its own WITHs.
 * A loading set detects circular dependencies.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

extern const char     *Include_Paths[32];
extern uint32_t        Include_Path_Count;
extern Syntax_Node    *Loaded_Package_Bodies[128];
extern int             Loaded_Body_Count;
extern String_Slice    Loaded_Body_Names[128];
extern int             Loaded_Body_Names_Count;

typedef struct {
  String_Slice names[64];
  int          count;
} Loading_Set;

extern Loading_Set Loading_Packages;

bool  Body_Already_Loaded    (String_Slice name);
void  Mark_Body_Loaded       (String_Slice name);
bool  Loading_Set_Contains   (String_Slice name);
void  Loading_Set_Add        (String_Slice name);
void  Loading_Set_Remove     (String_Slice name);
char *Lookup_Path            (String_Slice name);
char *Lookup_Path_Body       (String_Slice name);
void  Load_Package_Spec      (String_Slice name, char *src);
char *Read_File              (const char *path, size_t *out_size);
char *Read_File_Simple       (const char *path);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §22. SIMD fast paths
 *
 * Vectorised scanning primitives for whitespace skipping, identifier recognition,
 * digit scanning, and delimiter search.  Three implementations are selected at
 * compile time: x86-64 (AVX-512/AVX2/SSE4.2), ARM64 (NEON), and a scalar fallback.
 * All SIMD paths have an equivalent scalar fallback with loop unrolling.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

#ifdef SIMD_X86_64
  extern int Simd_Has_Avx512;
  extern int Simd_Has_Avx2;
  uint32_t Tzcnt32 (uint32_t value);
  uint64_t Tzcnt64 (uint64_t value);
  #ifdef __AVX2__
  uint32_t Simd_Parse_8_Digits_Avx2 (const char *digits);
  #endif
#endif

void        Simd_Detect_Features       (void);
const char *Simd_Skip_Whitespace       (const char *cursor, const char *limit);
const char *Simd_Find_Newline          (const char *cursor, const char *limit);
const char *Simd_Find_Quote            (const char *cursor, const char *limit);
const char *Simd_Find_Double_Quote     (const char *cursor, const char *limit);
const char *Simd_Scan_Identifier       (const char *cursor, const char *limit);
const char *Simd_Scan_Digits           (const char *cursor, const char *limit);

/* ──────────────────────────────────────────────────────────────────────────────────────────────
 * §23. Driver
 *
 * The main driver parses command-line arguments, compiles each source file to LLVM IR
 * (forking a subprocess per file for parallel compilation), and returns an exit status.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  const char *input_path;
  const char *output_path;
  int         exit_status;
} Compile_Job;

void  Compile_File       (const char *input_path, const char *output_path);
void  Derive_Output_Path (const char *input, char *out, size_t out_size);
void *Compile_Worker     (void *arg);
int   main               (int argc, char *argv[]);

#endif /* ADA83_H */

