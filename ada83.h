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

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * Ada83 — An Ada 1983 (ANSI/MIL-STD-1815A) compiler targeting LLVM IR
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * §0    Setup              — SIMD feature detection and fat-pointer layout constants
 * §1    Type_Metrics       — Size, alignment, and bit-width computations
 * §2    Memory_Arena       — Bump allocator for AST nodes and transient storage
 * §3    String_Slice       — Non-owning string views with case-insensitive comparison
 * §4    Source_Location    — File, line, and column anchors for diagnostics
 * §5    Error_Handling     — Accumulating error and fatal-error reporters
 * §6    Big_Integer        — Arbitrary-precision integers and reals for Ada literals
 * §7    Lexer              — Character stream to token stream (with SIMD fast paths)
 * §8    Abstract_Syntax    — Parse-tree node kinds and tree construction
 * §9    Parser             — Recursive-descent parser for the full Ada 83 grammar
 * §10   Type_System        — Ada type semantics: derivation, subtypes, constraints
 * §11   Symbol_Table       — Scoped name resolution with use-clause visibility
 * §12   Semantic_Pass      — Type checking, overload resolution, and constant folding
 * §13   Code_Generator     — LLVM IR emission for all Ada constructs
 * §14   Include_Path       — Package file loading and search-path management
 * §15   ALI_Writer         — GNAT-compatible Ada Library Information output
 * §15.7 Elaboration_Model  — Dependency ordering for multi-unit elaboration
 * §16   Generic_Expansion  — Macro-style instantiation of generic units
 * §17   Main_Driver        — Command-line parsing and top-level orchestration
 */

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §0. SIMD Optimizations and Fat Pointers
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Architectures:
 *   x86-64  — AVX-512BW (64-byte vectors), AVX2 (32-byte), SSE4.2 (16-byte)
 *   ARM64   — NEON/ASIMD (16-byte), SVE (128–2048 bits, detected at runtime)
 *   Generic — Scalar fallback with loop unrolling for portability
 *
 * Every SIMD path has an equivalent scalar fallback.
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 */

/* Bounds live behind the second pointer as a { bound_type, bound_type } struct where bound_type
 * is the native index type (i32 for STRING, i8 for CHARACTER, etc.).  For the GNAT layout see
 * gnatllvm-arrays-create.adb lines 684–707. */
#define FAT_PTR_TYPE "{ ptr, ptr }"

/* Fat pointer size in bytes: ptr (8) + ptr (8) = 16 on all 64-bit targets. */
#define FAT_PTR_ALLOC_SIZE   16
#define STRING_BOUND_TYPE    "i32"           /* LLVM IR type for STRING index bounds */
#define STRING_BOUND_WIDTH   32              /* Width of that type in bits */
#define STRING_BOUNDS_STRUCT "{ i32, i32 }"  /* Bounds struct for STRING: { first, last } */
#define STRING_BOUNDS_ALLOC  8               /* sizeof (STRING bounds struct) in bytes */

/* Detect the target architecture at compile time so we can select SIMD paths. */
#if defined (__x86_64__) || defined (_M_X64)
  #define SIMD_X86_64 1
#elif defined (__aarch64__) || defined (_M_ARM64)
  #define SIMD_ARM64 1
#else
  #define SIMD_GENERIC 1
#endif
int Simd_Has_Avx512 = -1; /* -1 = unchecked, 0 = absent, 1 = present */
__attribute__ ((unused))
#define Is_Id_Char(ch) (Id_Char_Table[(uint8_t)(ch)])

/* 128-bit integer typedefs — a GCC/Clang extension.  Ada modular types with mod 2**128 and the
 * Ada 2022 Long_Long_Long_Integer type both require native 128-bit arithmetic. */
typedef __int128          int128_t;
typedef unsigned __int128 uint128_t;

/* Number of bits in an addressable storage unit — universally 8 on modern targets. */
enum { Bits_Per_Unit = 8 };

/* Named constants for the LLVM integer and floating-point widths used throughout the compiler. */
typedef enum {
  Width_1   = 1,    Width_8   = 8,    Width_16  = 16,
  Width_32  = 32,   Width_64  = 64,   Width_128 = 128,
  Width_Ptr = 64,   Width_Float = 32, Width_Double = 64
} Bit_Width;

/* Ada standard integer widths per RM §3.5.4.  Long_Long_Long_Integer (128-bit) is an Ada 2022
 * extension included here so that the type system is ready for i128/u128 modular types. */
typedef enum {
  Ada_Short_Short_Integer_Bits      = Width_8,
  Ada_Short_Integer_Bits            = Width_16,
  Ada_Integer_Bits                  = Width_32,
  Ada_Long_Integer_Bits             = Width_64,
  Ada_Long_Long_Integer_Bits        = Width_64,
  Ada_Long_Long_Long_Integer_Bits   = Width_128   /* Ada 2022: 128-bit */
} Ada_Integer_Width;

/* Default metrics when the type is unspecified — falls back to Integer'Size (32 bits, 4 bytes). */
enum {
  Default_Size_Bits   = Ada_Integer_Bits,
  Default_Size_Bytes  = Ada_Integer_Bits / Bits_Per_Unit,
  Default_Align_Bytes = Default_Size_Bytes
};

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §2. MEMORY ARENA — Bump allocation for the compilation session
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * A simple bump allocator used for AST nodes, interned strings, and other objects whose lifetime
 * spans the entire compilation.  All memory is freed in one shot at the end via Arena_Free_All.
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
enum { Default_Chunk_Size = 1 << 24 };  /* 16 MiB per chunk */

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §3. STRING SLICE — Non-owning string views
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * A String_Slice is a (pointer, length) pair borrowed from the source buffer or the arena.  It
 * avoids strlen () calls and allows substring views without allocation.  Ada identifiers are
 * case-insensitive, so the comparison and hashing functions fold to lower case.
 */
typedef struct {
  const char *data;
  uint32_t    length;
} String_Slice;
#define S(literal) ((String_Slice){ .data = (literal), .length = sizeof (literal) - 1 })

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §4. SOURCE LOCATION — Anchoring diagnostics to source text
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Every AST node, token, and symbol carries a Source_Location so that error messages can point
 * the programmer at the exact file, line, and column where the problem was detected.
 */
typedef struct {
  const char *filename;
  uint32_t    line;
  uint32_t    column;
} Source_Location;

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §6. BIG INTEGER — Arbitrary-precision integers and reals for Ada literal values
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Ada numeric literals can exceed the 64-bit range (e.g. mod 2**128).  Magnitudes are stored as
 * little-endian arrays of 64-bit limbs.  The operations needed for literal parsing are:
 *   - Construction from a decimal (or based) string
 *   - Multiply by a small constant (the base)
 *   - Add a small constant (the digit value)
 *   - Sign-aware comparison and extraction to int64/int128/uint128
 */
typedef struct {
  uint64_t *limbs;
  uint32_t  count;
  uint32_t  capacity;
  bool      is_negative;
} Big_Integer;

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §6.2 BIG_REAL — Arbitrary-precision real numbers for Ada literals
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Real literals are represented as significand * 10**exponent per Ada LRM §2.4.1.  For example
 * the literal 3.14159_26535_89793 is stored as significand = 314159265358979, exponent = -14.
 * This keeps the literal value exact; rounding happens only when converting to a machine float.
 */
typedef struct {
  Big_Integer *significand;  /* All digits without decimal point */
  int32_t      exponent;     /* Power of 10 (negative for fractional) */
} Big_Real;

/* Exact rational number: numerator / denominator with denominator > 0, reduced by GCD. */
typedef struct {
  Big_Integer *numerator;
  Big_Integer *denominator;
} Rational;

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §7. LEXER — Transforming characters into tokens
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * The lexer maintains a cursor over the source buffer and produces tokens on demand.  Lexical
 * rules follow Ada RM §2.  SIMD fast paths accelerate whitespace skipping, identifier scanning,
 * and digit scanning on x86-64 (AVX-512/AVX2/SSE4.2) and ARM64 (NEON) targets.
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §7.1 Token Kinds — Every lexeme in the Ada 83 grammar
 * ───────────────────────────────────────────────────────────────────────────────────────────────── */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §7.2 Token Structure — A single lexeme with its semantic value
 * ───────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  Token_Kind      kind;
  Source_Location location;
  String_Slice    text;

  /* Semantic value (valid based on kind) */
  union {
    int64_t      integer_value;
    double       float_value;
  };
  union {
    Big_Integer *big_integer;
    Big_Real    *big_real;     /* Arbitrary precision real literal */
  };
} Token;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §7.3 Lexer State — Cursor over the source buffer
 * ───────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  const char *source_start;
  const char *current;
  const char *source_end;
  const char *filename;
  uint32_t    line;
  uint32_t    column;
  Token_Kind  prev_token_kind;  /* Track previous token for context-sensitive lexing */
} Lexer;

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §8. ABSTRACT SYNTAX TREE — Parse Tree Representation
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * The AST uses a tagged-union design: each Syntax_Node carries a Node_Kind tag and a union payload
 * specific to that kind.  The tree is a forest — one root per compilation unit, with shared
 * subtrees within a unit where the grammar allows (e.g. subtype marks referenced from multiple
 * declarations).  All nodes are arena-allocated and never individually freed.
 * ═══════════════════════════════════════════════════════════════════════════════════════════════ */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §8.1 Node Kinds — one enumerator per syntactic construct in Ada 83
 * ─────────────────────────────────────────────────────────────────────────────────────────────── */
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
  NK_DISCRIMINANT_CONSTRAINT, NK_DIGITS_CONSTRAINT, NK_DELTA_CONSTRAINT,
  NK_ARRAY_TYPE, NK_RECORD_TYPE,
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

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §8.2 Syntax Node Structure
 *
 * Each node carries its kind, source location, optional type annotation (filled by semantic
 * analysis), optional symbol link (filled by name resolution), and a payload union whose active
 * member is determined by the kind tag.
 * ─────────────────────────────────────────────────────────────────────────────────────────────── */
struct Syntax_Node {
  Node_Kind        kind;
  Source_Location  location;
  Type_Info       *type;      /* Set during semantic analysis */
  Symbol          *symbol;    /* Set during name resolution */
  union {

    /* NK_INTEGER */
    struct { int64_t value; Big_Integer *big_value; } integer_lit;

    /* NK_REAL - arbitrary precision with double for compatibility */
    struct { double value; Big_Real *big_value; } real_lit;

    /* NK_STRING, NK_CHARACTER, NK_IDENTIFIER */
    struct { String_Slice text; } string_val;

    /* NK_SELECTED: prefix.selector */
    struct { Syntax_Node *prefix; String_Slice selector; } selected;

    /* NK_ATTRIBUTE: prefix'attribute(args) */
    struct { Syntax_Node *prefix; String_Slice name; Node_List arguments; } attribute;

    /* NK_QUALIFIED: subtype_mark'(expression) */
    struct { Syntax_Node *subtype_mark; Syntax_Node *expression; } qualified;

    /* NK_BINARY_OP, NK_UNARY_OP */
    struct { Token_Kind op; Syntax_Node *left; Syntax_Node *right; } binary;
    struct { Token_Kind op; Syntax_Node *operand; } unary;

    /* NK_AGGREGATE */
    struct { Node_List items; bool is_named; bool is_parenthesized; } aggregate;

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
    struct { Syntax_Node *digits_expr; Syntax_Node *range; } digits_constraint;  /* NK_DIGITS_CONSTRAINT */
    struct { Syntax_Node *delta_expr; Syntax_Node *range; } delta_constraint;    /* NK_DELTA_CONSTRAINT */

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
    struct { Syntax_Node *range; uint128_t modulus; bool is_modular; } integer_type;
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
      Symbol *label_symbol;           /* Pre-registered label for GOTO */
      Syntax_Node *iteration_scheme;  /* for/while condition */
      Node_List statements;
      bool is_reverse;
    } loop_stmt;

    /* NK_BLOCK */
    struct {
      String_Slice label;
      Symbol *label_symbol;           /* Pre-registered label for GOTO */
      Node_List declarations;
      Node_List statements;
      Node_List handlers;
    } block_stmt;

    /* NK_EXIT */
    struct { String_Slice loop_name; Syntax_Node *condition; Symbol *target; } exit_stmt;

    /* NK_GOTO */
    struct { String_Slice name; Symbol *target; } goto_stmt;

    /* NK_LABEL */
    struct { String_Slice name; Syntax_Node *statement; Symbol *symbol; } label_node;

    /* NK_RAISE */
    struct { Syntax_Node *exception_name; } raise_stmt;

    /* NK_ACCEPT */
    struct {
      String_Slice entry_name;
      Syntax_Node *index;
      Node_List parameters;
      Node_List statements;
      Symbol *entry_sym;      /* Resolved entry symbol (for entry_index) */
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
      Syntax_Node *init;       /* For renames, this is the renamed object */
      bool is_constant;
      bool is_aliased;
      bool is_rename;          /* True for RENAMES declarations */
    } object_decl;

    /* NK_TYPE_DECL, NK_SUBTYPE_DECL */
    struct {
      String_Slice name;
      Node_List discriminants;
      Syntax_Node *definition;
      bool is_limited;
      bool is_private;
    } type_decl;

    /* NK_EXCEPTION_DECL, NK_EXCEPTION_RENAMING */
    struct { Node_List names; Syntax_Node *renamed; } exception_decl;

    /* NK_PROCEDURE_SPEC, NK_FUNCTION_SPEC, NK_SUBPROGRAM_RENAMING */
    struct {
      String_Slice name;
      Node_List parameters;
      Syntax_Node *return_type;  /* NULL for procedures */
      Syntax_Node *renamed;      /* For NK_SUBPROGRAM_RENAMING: the renamed entity */
    } subprogram_spec;

    /* NK_PROCEDURE_BODY, NK_FUNCTION_BODY */
    struct {
      Syntax_Node *specification;
      Node_List declarations;
      Node_List statements;
      Node_List handlers;
      bool is_separate;
      bool code_generated;  /* Prevents duplicate code generation */
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

    /* NK_PACKAGE_RENAMING */
    struct {
      String_Slice new_name;
      Syntax_Node *old_name;
    } package_renaming;

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

    /* Generic type definition kinds (RM 12.1) */
    /* NK_GENERIC_TYPE_PARAM: type T is ... */
    struct {
      String_Slice name;
      enum {
        GEN_DEF_PRIVATE = 0,      /* type T is private */
        GEN_DEF_LIMITED_PRIVATE,   /* type T is limited private */
        GEN_DEF_DISCRETE,          /* type T is (<>) */
        GEN_DEF_INTEGER,           /* type T is range <> */
        GEN_DEF_FLOAT,             /* type T is digits <> */
        GEN_DEF_FIXED,             /* type T is delta <> */
        GEN_DEF_ARRAY,             /* type T is array (...) of ... */
        GEN_DEF_ACCESS,            /* type T is access ... */
        GEN_DEF_DERIVED            /* type T is new ... */
      } def_kind;
      Syntax_Node *def_detail;
      Node_List    discriminants;  /* Known discriminants for formal private types */
    } generic_type_param;

    /* NK_GENERIC_OBJECT_PARAM: X : [mode] type [:= default] */
    struct {
      Node_List names;
      Syntax_Node *object_type;
      Syntax_Node *default_expr;
      enum {
        GEN_MODE_IN = 0,     /* in (default) */
        GEN_MODE_OUT,        /* out */
        GEN_MODE_IN_OUT      /* in out */
      } mode;
    } generic_object_param;

    /* NK_GENERIC_SUBPROGRAM_PARAM: with procedure/function spec [is name|<>] */
    struct {
      String_Slice name;
      Node_List parameters;
      Syntax_Node *return_type;  /* NULL for procedures */
      Syntax_Node *default_name;
      bool is_function;
      bool default_box;
    } generic_subprog_param;

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
    struct {
      Syntax_Node *context;
      Syntax_Node *unit;
      Syntax_Node *separate_parent;  /* Parent name for SEPARATE subunits */
    } compilation_unit;
  };
};

/* ═══════════════════════════════════════════════════════════════════════════════════════════════════
 * §9. PARSER — Recursive Descent with Unified Postfix Handling
 * ═══════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Recursive descent mirrors the grammar, making the grammar itself the invariant.
 *
 * Key design decisions:
 *
 * 1. UNIFIED APPLY NODE — All X(...) forms parse as NK_APPLY.  Semantic analysis later
 *    distinguishes calls, indexing, slicing, and type conversions.
 *
 * 2. UNIFIED ASSOCIATION PARSING — One helper handles positional, named, and choice associations
 *    used in aggregates, calls, and generic actuals.
 *
 * 3. UNIFIED POSTFIX CHAIN — One loop handles .selector, 'attribute, and (args).
 * ═══════════════════════════════════════════════════════════════════════════════════════════════ */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §9.1 Parser State
 * ─────────────────────────────────────────────────────────────────────────────────────────────── */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §9.5 Expression Parsing — Operator Precedence
 *
 * The grammar encodes precedence; recursion direction determines associativity.
 *
 * Ada precedence (highest to lowest):
 *   **                               (right-associative exponentiation)
 *   ABS  NOT                         (unary prefix)
 *   *  /  MOD  REM                   (multiplying operators)
 *   +  -  &  (binary)  +  - (unary)  (adding operators and concatenation)
 *   =  /=  <  <=  >  >=  IN  NOT IN (relational)
 *   AND  OR  XOR  AND THEN  OR ELSE (logical, short-circuit)
 * ─────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declarations */
Syntax_Node *Parse_Expression (Parser *p);
Syntax_Node *Parse_Choice (Parser *p);
Syntax_Node *Parse_Name (Parser *p);
Syntax_Node *Parse_Simple_Name (Parser *p);
Syntax_Node *Parse_Subtype_Indication (Parser *p);
Syntax_Node *Parse_Array_Type (Parser *p);
void Parse_Association_List (Parser *p, Node_List *list);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §9.8 Binary Expression Parsing — Precedence Climbing
 *
 * Climbing starts at low precedence and consumes equal-or-higher before returning.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

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
Syntax_Node *Parse_Expression_Precedence(Parser *p, Precedence min_prec);

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §9.11 Statement Parsing
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Statements run in sequence while expressions form a tree, and parsing reflects this.
 */

/* Forward declarations */
Syntax_Node *Parse_Statement (Parser *p);
void Parse_Statement_Sequence (Parser *p, Node_List *list);
void Parse_Declarative_Part (Parser *p, Node_List *list);
Syntax_Node *Parse_Declaration (Parser *p);
Syntax_Node *Parse_Enumeration_Type (Parser *p);
Syntax_Node *Parse_Record_Type (Parser *p);
Syntax_Node *Parse_Access_Type(Parser *p);
Syntax_Node *Parse_Derived_Type(Parser *p);
Syntax_Node *Parse_Subprogram_Body (Parser *p, Syntax_Node *spec);
Syntax_Node *Parse_Block_Statement(Parser *p, String_Slice label);
Syntax_Node *Parse_Loop_Statement(Parser *p, String_Slice label);
Syntax_Node *Parse_Discrete_Range(Parser *p);

/* Forward declaration for variant part parsing */
Syntax_Node *Parse_Variant_Part (Parser *p);

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §10. TYPE SYSTEM — Ada Type Semantics
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * A type combines name, range, and representation as three orthogonal concerns.
 *
 * INVARIANT: All sizes are stored in BYTES, not bits.
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §10.1 Type Kinds
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §10.2 Type Information Structure
 *
 * Each type has:
 * - Kind and name
 * - Size and alignment (in BYTES)
 * - Bounds for scalars
 * - Component info for composites
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct Type_Info Type_Info;
typedef struct Symbol Symbol;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * Runtime Check Bit Constants (GNAT-style, RM 11.5)
 *
 * Each bit controls a check category that can be independently suppressed
 * via pragma Suppress (Check_Name).  Stored in Type_Info.suppressed_checks
 * and Symbol.suppressed_checks as a bitmask.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
#define CHK_RANGE          ((uint32_t)1)
#define CHK_OVERFLOW       ((uint32_t)2)
#define CHK_INDEX          ((uint32_t)4)
#define CHK_LENGTH         ((uint32_t)8)
#define CHK_DIVISION       ((uint32_t)16)
#define CHK_ACCESS         ((uint32_t)32)
#define CHK_DISCRIMINANT   ((uint32_t)64)
#define CHK_ELABORATION    ((uint32_t)128)
#define CHK_STORAGE        ((uint32_t)256)
#define CHK_ALL            ((uint32_t)0xFFFFFFFF)

/* Bound representation: explicit tagged union to avoid bitcast.
 * int_value is int128_t to support i128/u128 ranges (mod 2**128).
 * Values that fit in 64 bits are implicitly widened on assignment. */
typedef struct {
  enum { BOUND_NONE, BOUND_INTEGER, BOUND_FLOAT, BOUND_EXPR } kind;
  union {
    int128_t     int_value;
    double       float_value;
    Syntax_Node *expr;
  };
  uint32_t cached_temp;  /* If non-zero, pre-evaluated LLVM temp to use */
} Type_Bound;

/* Variant information for discriminated records (RM 3.7.3) */
typedef struct {
  int64_t  disc_value_low;     /* Low bound of range selecting this variant */
  int64_t  disc_value_high;    /* High bound (same as low for single values) */
  bool     is_others;          /* WHEN OTHERS variant */
  uint32_t first_component;    /* Index of first component in this variant */
  uint32_t component_count;    /* Number of components in this variant */
  uint32_t variant_size;       /* Size of this variant's components in bytes */
} Variant_Info;

/* Component information for records */
typedef struct {
  String_Slice  name;
  Type_Info    *component_type;
  uint32_t      byte_offset;
  uint32_t      bit_offset;    /* For representation clauses */
  uint32_t      bit_size;
  Syntax_Node  *default_expr;  /* Default initialization expression (RM 3.7) */
  bool          is_discriminant;   /* True if this is a discriminant (RM 3.7.1) */
  int32_t       variant_index;     /* Which variant this belongs to (-1 = fixed part) */
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

  /* Size and alignment in BYTES (not bits) */
  uint32_t     size;
  uint32_t     alignment;
  uint32_t     specified_bit_size;  /* Exact 'SIZE from rep clause (0 = not specified) */

  /* Scalar bounds */
  Type_Bound   low_bound;
  Type_Bound   high_bound;
  uint128_t    modulus;        /* For modular types: 0..2^128 */

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

      /* Discriminant tracking (RM 3.7) */
      uint32_t        discriminant_count;  /* Number of discriminant components */
      bool            has_discriminants;    /* Type has discriminant part */
      bool            all_defaults;         /* All discriminants have defaults (mutable) */
      bool            is_constrained;       /* Object/subtype is constrained */

      /* Variant part tracking (RM 3.7.3) */
      Variant_Info   *variants;
      uint32_t        variant_count;
      uint32_t        variant_offset;       /* Byte offset where variant part begins */
      uint32_t        max_variant_size;     /* Max size across all variants */
      Syntax_Node    *variant_part_node;    /* AST node for variant part */

      /* Discriminant constraint values (for constrained subtypes) */
      int64_t        *disc_constraint_values;  /* Array [discriminant_count] */
      Syntax_Node   **disc_constraint_exprs;   /* Runtime expr nodes (NULL if static) */
      uint32_t       *disc_constraint_preeval;  /* LLVM alloca temps for pre-evaluated non-disc exprs */
      bool            has_disc_constraints;
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
    struct {  /* TYPE_FLOAT */
      int digits;     /* Declared DIGITS value (RM 3.5.7) */
    } flt;
  };

  /* Runtime check suppression */
  uint32_t     suppressed_checks;

  /* Pragma Pack - pack components to minimum size */
  bool         is_packed;

  /* Limited type flag (RM 7.5) - type cannot be copied */
  bool         is_limited;

  /* Freezing status - once frozen, representation cannot change */
  bool         is_frozen;

  /* STORAGE_SIZE specification (RM 13.7.1) — in bits, 0 = unspecified */
  int64_t      storage_size;

  /* Implicitly generated equality function name (set at freeze time) */
  const char  *equality_func_name;

  /* Runtime type elaboration (RM §3.3.1): constrained array types whose
   * bounds are BOUND_EXPR (function calls evaluated at elaboration time).
   * Nonzero ⇒ @__rt_type_<id>_size holds byte size after elaboration.
   * For record types with such components, @__rt_rec_<id>_off<i> holds
   * byte offset of component i and @__rt_rec_<id>_size the total. */
  uint32_t     rt_global_id;
};

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * IEEE 754 Named Constants — replaces magic numbers throughout codegen.
 * Single source of truth for float/double structural parameters.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
#define IEEE_FLOAT_DIGITS       6
#define IEEE_DOUBLE_DIGITS      15
#define IEEE_FLOAT_MANTISSA     24
#define IEEE_DOUBLE_MANTISSA    53
#define IEEE_FLOAT_EMAX         128
#define IEEE_DOUBLE_EMAX        1024
#define IEEE_FLOAT_EMIN  (-125)
#define IEEE_DOUBLE_EMIN (-1021)
#define IEEE_MACHINE_RADIX      2
#define IEEE_DOUBLE_MIN_NORMAL  2.2250738585072014e-308   /* 2^(-1022) */
#define IEEE_FLOAT_MIN_NORMAL   1.1754943508222875e-38    /* 2^(-126)  */
#define LOG2_OF_10              3.321928094887362

/*
 * NOTE: Type compatibility checking is consolidated in Type_Covers ()
 * defined in §11.6.2 (Overload Resolution section). That function provides
 * coverage checking for:
 * - Same type identity
 * - Universal type compatibility
 * - Base type matching
 * - Array/string structural compatibility
 * - Access type designated type compatibility
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §10.6 Type Freezing
 *
 * Freezing determines the point at which a type's representation is fixed.
 * The compiler must track what the RM permits but the programmer cannot see.
 * Per RM 13.14:
 * - Types are frozen by object declarations, bodies, end of declarative part
 * - Subtypes freeze their base type
 * - Composite types freeze their component types
 * - Once frozen, size/alignment/layout cannot change
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declaration for Symbol */
typedef struct Symbol Symbol;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §10.7 LLVM Type Mapping
 *
 * The source type is semantic while the target type is representational.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declarations for array helpers (defined after Type_Bound_Value) */
int128_t Type_Bound_Value (Type_Bound bound);

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §11. SYMBOL TABLE — Scoped Name Resolution
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * The symbol table implements Ada's visibility and overloading rules:
 *
 * - Hierarchical scopes (packages can nest, blocks create new scopes)
 * - Overloading: same name, different parameter profiles
 * - Use clauses: make names directly visible without qualification
 * - Visibility: immediately visible, use-visible, directly visible
 *
 * We use a hash table with chaining and a scope stack for nested contexts.
 * Collisions are inevitable; we make them cheap rather than trying to
 * eliminate them.
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §11.1 Symbol Kinds
 *
 * Eighteen kinds where the RM defines most and the implementation adds two.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §11.2 Symbol Structure
 *
 * The symbol table maps names to meanings while the scope stack provides context.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
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

  /* ───────────────────────────────────────────────────────────────────────────────────────────────
   * Pragma Effects
   * ──────────────────────────────────────────────────────────────────────────────────────────── */

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
  bool            body_emitted;        /* Function/procedure body already emitted */
  bool            is_named_number;     /* Named number (constant without explicit type) */
  bool            is_overloaded;       /* Part of an overload set (needs unique_id suffix) */
  bool            body_claimed;        /* Body has been matched to this spec (for homographs) */
  bool            is_predefined;       /* Predefined operator from STANDARD */
  bool            needs_address_marker; /* Needs @__addr.X global for 'ADDRESS */
  bool            is_identity_function; /* Function body is just RETURN param (can inline) */
  uint32_t        disc_agg_temp;       /* Temp ID for aggregate discriminant storage (0 = none) */

  /* Discriminant constraint (RM 3.7.2) */
  bool            is_disc_constrained;  /* Object has discriminant constraints */

  /* Fat pointer storage: set when variable needs { ptr, ptr } representation.
   * True for unconstrained arrays and constrained arrays with dynamic bounds. */
  bool            needs_fat_ptr_storage;

  /* Derived type operations (RM 3.4) */
  Symbol         *parent_operation;    /* Parent operation that implements this derived op */
  Type_Info      *derived_from_type;   /* The derived type this op is for */

  /* LLVM label ID for SYMBOL_LABEL */
  uint32_t        llvm_label_id;       /* 0 = not yet assigned */
  uint32_t        loop_exit_label_id;  /* EXIT label for named loops */

  /* Entry index within task (for SYMBOL_ENTRY) */
  uint32_t        entry_index;         /* 0-based index for entry matching */

  /* For RENAMES: pointer to the renamed object's AST node */
  Syntax_Node    *renamed_object;

  /* ───────────────────────────────────────────────────────────────────────────────────────────────
   * Generic Support
   * ──────────────────────────────────────────────────────────────────────────────────────────── */

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
    Token_Kind   builtin_operator;   /* For built-in operators as subprogram actuals */
  } *generic_actuals;
  uint32_t        generic_actual_count;

  /* For generic instances: expanded (cloned) trees with substitutions */
  Syntax_Node    *expanded_spec;       /* Cloned spec with actuals substituted */
  Syntax_Node    *expanded_body;       /* Cloned body with actuals substituted */
};

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §11.3 Scope Structure
 *
 * Each scope has its own hash table with 1024 buckets, which covers most programs.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
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

  /* Frame variables propagated from child scopes (DECLARE blocks, loops).
   * Separate from 'symbols' to avoid affecting symbol lookup.
   * Used only for generating frame aliases in nested functions. */
  Symbol **frame_vars;
  uint32_t frame_var_count;
  uint32_t frame_var_capacity;
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
  Type_Info *type_address;  /* SYSTEM.ADDRESS */

  /* Unique ID counter for symbol mangling */
  uint32_t   next_unique_id;
} Symbol_Manager;

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §11.6 OVERLOAD RESOLUTION
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Overload resolution is a two-pass process:
 *
 * 1. Bottom-up pass: Collect all possible interpretations of each identifier
 *    based on visibility rules. Each interpretation is a (Symbol, Type) pair.
 *
 * 2. Top-down pass: Given context type expectations, select the unique valid
 *    interpretation using disambiguation rules.
 *
 * Key concepts:
 * - Interp: Record of (Nam, Typ, Opnd_Typ) representing one interpretation
 * - Covers: Type compatibility test (T1 covers T2 if T2's values are legal for T1)
 * - Disambiguate: Select best interpretation when multiple are valid
 *
 * Per RM 8.6: Overload resolution identifies the unique declaration for each
 * identifier. It fails if no interpretation is valid or if multiple are valid.
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §11.6.1 Interpretation Structure
 *
 * "type Interp is record Nam, Typ, Opnd_Typ..."
 * We store interpretations in a contiguous array during resolution.
 * Sixty-four interpretations suffices since deeper ambiguity signals a pathological program.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §11.6.3 Parameter Conformance
 *
 * Check if an argument list matches a subprogram's parameter profile.
 * Per RM 6.4.1: actual parameters must be type conformant with formals.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declaration for Resolve_Expression - needed for argument resolution */
typedef struct {
  Type_Info **types;       /* Array of argument types */
  uint32_t    count;       /* Number of arguments */
  String_Slice *names;     /* Named association names (NULL for positional) */
} Argument_Info;

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §12. SEMANTIC ANALYSIS — Type Checking and Resolution
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * A permissive parser gives the type checker material to work with.
 *
 * Semantic analysis performs:
 * - Name resolution: bind identifiers to symbols
 * - Type checking: verify type compatibility of operations
 * - Overload resolution: select correct subprogram
 * - Constraint checking: verify bounds, indices, etc.
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §12.1 Expression Resolution
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
Type_Info *Resolve_Expression (Syntax_Node *node);
void Resolve_Statement (Syntax_Node *node);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §12.2 Statement Resolution
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
void Resolve_Declaration_List (Node_List *list);

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §15. ALI FILE WRITER — GNAT-Compatible Library Information
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Ada Library Information (.ali) files record compilation dependencies and
 * unit metadata. Format follows GNAT's lib-writ.ads specification:
 *
 *   V "version"              -- compiler version
 *   P flags                  -- compilation parameters
 *   U name source version    -- unit entry
 *   W name [source ali]      -- with dependency
 *   D source timestamp       -- source dependency
 *
 * The ALI file enables:
 *   • Separate compilation with dependency tracking
 *   • Binder consistency checking
 *   • IDE cross-reference navigation
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.1 Unit_Info — Compilation unit metadata collector
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  String_Slice      unit_name;        /* Canonical Ada name (Package.Child%b) */
  String_Slice      source_name;      /* File name (package-child.adb) */
  uint32_t          source_checksum;  /* CRC32 of source text */
  bool              is_body;          /* spec (false) or body (true) */
  bool              is_generic;       /* Generic declaration */
  bool              is_preelaborate;  /* Pragma Preelaborate */
  bool              is_pure;          /* Pragma Pure */
  bool              has_elaboration;  /* Has elaboration code */
} Unit_Info;
typedef struct {
  String_Slice      name;             /* WITH'd unit name */
  String_Slice      source_file;      /* Source file name */
  String_Slice      ali_file;         /* ALI file name */
  bool              is_limited;       /* LIMITED WITH */
  bool              elaborate;        /* Pragma Elaborate */
  bool              elaborate_all;    /* Pragma Elaborate_All */
} With_Info;
typedef struct {
  String_Slice      source_file;      /* Depended-on source */
  uint32_t          timestamp;        /* Modification time (Unix epoch) */
  uint32_t          checksum;         /* CRC32 */
} Dependency_Info;

/* Exported symbol info for X lines */
typedef struct {
  String_Slice      name;             /* Ada name */
  String_Slice      mangled_name;     /* LLVM symbol name */
  char              kind;             /* T=type, S=subtype, V=variable, C=constant, P=procedure, F=function, E=exception */
  uint32_t          line;             /* Declaration line number */
  String_Slice      type_name;        /* Type name (for typed symbols) */
  String_Slice      llvm_type;        /* LLVM type signature (e.g., "i64", "ptr", "void (i64)") */
  uint32_t          param_count;      /* Parameter count (for subprograms) */
} Export_Info;
typedef struct {
  Unit_Info         units[8];         /* Units in this compilation */
  uint32_t          unit_count;
  With_Info         withs[64];        /* WITH dependencies */
  uint32_t          with_count;
  Dependency_Info   deps[128];        /* Source dependencies */
  uint32_t          dep_count;
  Export_Info       exports[256];     /* Exported symbols */
  uint32_t          export_count;
} ALI_Info;

/* Forward declarations for functions used by ALI_Collect_Exports */
String_Slice Mangle_Qualified_Name (String_Slice parent, String_Slice name);
String_Slice LLVM_Type_Basic (String_Slice ada_type);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.5 ALI_Write — Emit .ali file in GNAT format
 *
 * Per lib-writ.ads, the minimum valid ALI file needs:
 *   V line (version) — MUST be first
 *   P line (parameters) — MUST be present
 *   At least one U line (unit)
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
#define ALI_VERSION "Ada83 1.0 built " __DATE__ " " __TIME__

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7 ALI_Reader — Parse .ali files for dependency management
 *
 * We read ALI files to:
 *   1. Skip recompilation of unchanged units (checksum match)
 *   2. Load exported symbols from precompiled packages
 *   3. Track dependencies for elaboration ordering
 *   4. Find generic templates for instantiation
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Parsed export from X line */
typedef struct {
  char            kind;            /* T/S/V/C/P/F/E */
  char           *name;            /* Ada symbol name */
  char           *mangled_name;    /* LLVM symbol name for linking */
  char           *llvm_type;       /* LLVM type signature */
  uint32_t        line;            /* Source line */
  char           *type_name;       /* Ada type name (or NULL) */
  uint32_t        param_count;     /* For subprograms */
} ALI_Export;

/* Cached ALI information for loaded units
 * ALI_Cache_Entry is forward declared as ALI_Cache_Entry_Forward in the
 * Load_Package_Spec forward declarations section. */
typedef struct ALI_Cache_Entry_Forward {
  char           *unit_name;       /* Canonical name (e.g., "text_io") */
  char           *source_file;     /* Source file name */
  char           *ali_file;        /* ALI file path */
  uint32_t        checksum;        /* Source checksum from ALI */
  bool            is_spec;         /* true = spec, false = body */
  bool            is_generic;      /* Generic unit */
  bool            is_preelaborate; /* Has Preelaborate pragma */
  bool            is_pure;         /* Has Pure pragma */
  bool            loaded;          /* Symbols already loaded */

  /* With dependencies */
  char           *withs[64];       /* WITH'd unit names */
  uint32_t        with_count;

  /* Exported symbols from X lines */
  ALI_Export      exports[256];
  uint32_t        export_count;
} ALI_Cache_Entry;

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §15.7 ELABORATION MODEL — Standard-Style Dependency Graph Algorithm
 *
 * Implements the full Standard elaboration ordering algorithm as described
 * in bindo-elaborators.adb. This determines the safe order in which library
 * units must be elaborated at program startup (Ada RM 10.2).
 *
 * The algorithm proceeds in phases:
 *   1. BUILD GRAPH: Create vertices for units, edges for dependencies
 *   2. FIND COMPONENTS: Tarjan's SCC for cyclic dependency handling
 *   3. ELABORATE: Topological sort with priority ordering
 *   4. VALIDATE: Verify all constraints satisfied
 *
 * Key insight from GNAT: Edges are classified as "strong" (must-satisfy) or
 * "weak" (can-ignore-for-dynamic-model). This allows breaking cycles when
 * compiled with -gnatE (dynamic elaboration checking).
 *
 * Style: Haskell-like C99 with algebraic data types (tagged unions),
 * pure functions where possible, and composition over mutation.
 * ══════════════════════════════════════════════════════════════════════════════════════════════ */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7.1 Algebraic Types — Sum types via tagged unions
 *
 * Following the Haskell pattern: data Kind = A | B | C
 * In C99: enum for tag, union for payload, struct wrapper.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Unit_Kind: What kind of compilation unit is this vertex? */
typedef enum {
  UNIT_SPEC,       /* Package/subprogram specification with separate body */
  UNIT_BODY,       /* Package/subprogram body (paired with spec) */
  UNIT_SPEC_ONLY,  /* Spec without body (e.g., pure package spec) */
  UNIT_BODY_ONLY   /* Body without explicit spec (e.g., main subprogram) */
} Elab_Unit_Kind;

/* Edge_Kind: What dependency relationship does this edge represent?
 * Per GNAT bindo-graphs.ads, edge kinds determine precedence and strength. */
typedef enum {
  EDGE_WITH,            /* WITH clause dependency (strong) */
  EDGE_ELABORATE,       /* pragma Elaborate (strong) */
  EDGE_ELABORATE_ALL,   /* pragma Elaborate_All (strong, transitive) */
  EDGE_SPEC_BEFORE_BODY,/* Spec must elaborate before its body (strong) */
  EDGE_INVOCATION,      /* Call discovered during elaboration (weak) */
  EDGE_FORCED           /* Compiler-forced ordering (strong) */
} Elab_Edge_Kind;

/* Precedence_Kind: Result of comparing two vertices for elaboration order */
typedef enum {
  PREC_HIGHER,  /* First vertex should elaborate first */
  PREC_EQUAL,   /* No preference (use tiebreaker) */
  PREC_LOWER    /* Second vertex should elaborate first */
} Elab_Precedence;

/* Elaboration order status after algorithm completion */
typedef enum {
  ELAB_ORDER_OK,                    /* Valid order found */
  ELAB_ORDER_HAS_CYCLE,             /* Unresolvable cycle detected */
  ELAB_ORDER_HAS_ELABORATE_ALL_CYCLE /* Elaborate_All cycle (fatal) */
} Elab_Order_Status;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7.2 Graph Vertex — Compilation unit representation
 *
 * Each vertex represents one compilation unit (spec or body).
 * Tracks pending predecessor counts for the elaboration algorithm.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct Elab_Vertex Elab_Vertex;
typedef struct Elab_Edge   Elab_Edge;
struct Elab_Vertex {

  /* Identity */
  uint32_t         id;              /* Unique vertex ID */
  String_Slice     name;            /* Unit name (e.g., "Text_IO") */
  Elab_Unit_Kind   kind;            /* Spec/Body/Spec_Only/Body_Only */
  Symbol          *symbol;          /* Associated package/subprogram symbol */

  /* Component membership (set by Tarjan's SCC) */
  uint32_t         component_id;    /* SCC ID (0 = not yet assigned) */

  /* Pending predecessor counts (decremented during elaboration) */
  uint32_t         pending_strong;  /* Strong predecessors remaining */
  uint32_t         pending_weak;    /* Weak predecessors remaining */

  /* Flags */
  bool             in_elab_order;   /* Already added to elaboration order? */
  bool             is_preelaborate; /* pragma Preelaborate */
  bool             is_pure;         /* pragma Pure */
  bool             has_elab_body;   /* pragma Elaborate_Body */
  bool             is_predefined;   /* Ada.*, System.*, Interfaces.* */
  bool             is_internal;     /* GNAT.*, Ada83.* internal units */
  bool             needs_elab_code; /* Has elaboration code to run? */

  /* Spec/body pairing */
  Elab_Vertex     *body_vertex;     /* For spec: pointer to body vertex */
  Elab_Vertex     *spec_vertex;     /* For body: pointer to spec vertex */

  /* Edge lists (indices into graph's edge array) */
  uint32_t         first_pred_edge; /* First incoming edge index (or 0) */
  uint32_t         first_succ_edge; /* First outgoing edge index (or 0) */

  /* Tarjan's algorithm temporaries */
  int32_t          tarjan_index;    /* Discovery index (-1 = unvisited) */
  int32_t          tarjan_lowlink;  /* Lowest reachable index */
  bool             tarjan_on_stack; /* Currently on the DFS stack? */
};

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7.3 Graph Edge — Dependency relationship
 *
 * Edges are intrusive linked lists through vertices for O(1) iteration.
 * Each edge knows whether it's "strong" (must satisfy) or "weak" (can skip).
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
struct Elab_Edge {
  uint32_t         id;              /* Unique edge ID */
  Elab_Edge_Kind   kind;            /* WITH/ELABORATE/etc. */
  bool             is_strong;       /* Strong edge must be satisfied */

  /* Endpoints */
  uint32_t         pred_vertex_id;  /* Predecessor (must elaborate first) */
  uint32_t         succ_vertex_id;  /* Successor (elaborates after) */

  /* Linked list threading */
  uint32_t         next_pred_edge;  /* Next edge with same predecessor */
  uint32_t         next_succ_edge;  /* Next edge with same successor */
};

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7.4 Graph Structure — Vertices + Edges + Components
 *
 * Uses arena allocation for vertices/edges, dynamic arrays for order.
 * Maximum capacities chosen to handle large Ada programs.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
#define ELAB_MAX_VERTICES 512
#define ELAB_MAX_EDGES    2048
#define ELAB_MAX_COMPONENTS 256
typedef struct {

  /* Vertices */
  Elab_Vertex      vertices[ELAB_MAX_VERTICES];
  uint32_t         vertex_count;

  /* Edges */
  Elab_Edge        edges[ELAB_MAX_EDGES];
  uint32_t         edge_count;

  /* Components (SCCs) */
  uint32_t         component_pending_strong[ELAB_MAX_COMPONENTS];
  uint32_t         component_pending_weak[ELAB_MAX_COMPONENTS];
  uint32_t         component_count;

  /* Elaboration order (result) */
  Elab_Vertex     *order[ELAB_MAX_VERTICES];
  uint32_t         order_count;

  /* Has Elaborate_All cycle? (fatal error) */
  bool             has_elaborate_all_cycle;
} Elab_Graph;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7.6 Tarjan's SCC Algorithm — Find strongly connected components
 *
 * Standard O(V+E) algorithm for finding SCCs. Each SCC becomes a component
 * that must be elaborated together (handles circular dependencies).
 *
 * Invariant: After completion, every vertex has a non-zero component_id.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  uint32_t stack[ELAB_MAX_VERTICES];
  uint32_t stack_top;
  int32_t  index;
} Tarjan_State;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7.8 Vertex Set Operations — Functional set manipulation
 *
 * Uses bitmap representation for O(1) membership testing.
 * Pure functions that return new sets rather than mutating.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  uint64_t bits[(ELAB_MAX_VERTICES + 63) / 64];
} Elab_Vertex_Set;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.7.9 Best Vertex Selection — Find optimal elaboration candidate
 *
 * Scans a vertex set to find the best candidate using a comparator.
 * Pure function with no side effects.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef bool (*Elab_Vertex_Pred)(const Elab_Vertex *v);
typedef Elab_Precedence (*Elab_Vertex_Cmp)(const Elab_Graph *,
                       const Elab_Vertex *,
                       const Elab_Vertex *);

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §15.8 BUILD-IN-PLACE — Limited Type Function Returns
 *
 * Ada limited types cannot be copied (RM 7.5). Functions returning limited
 * types must construct the result directly in caller-provided space—the
 * "Build-in-Place" (BIP) protocol. This eliminates intermediate temporaries.
 *
 * The protocol passes extra hidden parameters to BIP functions:
 *   __BIPalloc  - Allocation form selector (caller space, heap, pool, etc.)
 *   __BIPaccess - Pointer to destination where result is constructed
 *   __BIPfinal  - Finalization collection (for controlled components)
 *   __BIPmaster - Task master ID (for task components)
 *   __BIPchain  - Activation chain (for task components)
 *
 * Reference: Ada RM 7.5 (Limited Types), RM 6.5 (Return Statements)
 * ══════════════════════════════════════════════════════════════════════════════════════════════ */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.8.1 Algebraic Types — Sum types for BIP protocol
 *
 * BIP_Alloc_Form determines where the function result is allocated:
 *   - CALLER: Caller provides stack/object space (most common)
 *   - SECONDARY_STACK: Use secondary stack for dynamic-sized returns
 *   - GLOBAL_HEAP: Allocate on heap (from 'new' expression)
 *   - USER_POOL: Use user-defined storage pool
 *
 * BIP_Formal_Kind identifies which extra formal parameter is being accessed.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef enum {
  BIP_ALLOC_UNSPECIFIED = 0,    /* Let callee decide (propagate)          */
  BIP_ALLOC_CALLER      = 1,    /* Build in caller-provided space         */
  BIP_ALLOC_SECONDARY   = 2,    /* Allocate on secondary stack            */
  BIP_ALLOC_GLOBAL_HEAP = 3,    /* Allocate on global heap                */
  BIP_ALLOC_USER_POOL   = 4     /* Allocate from user storage pool        */
} BIP_Alloc_Form;
typedef enum {
  BIP_FORMAL_ALLOC_FORM,        /* Allocation strategy selector           */
  BIP_FORMAL_STORAGE_POOL,      /* Storage pool access (for USER_POOL)    */
  BIP_FORMAL_FINALIZATION,      /* Finalization collection pointer        */
  BIP_FORMAL_TASK_MASTER,       /* Task master ID for task components     */
  BIP_FORMAL_ACTIVATION,        /* Activation chain for task components   */
  BIP_FORMAL_OBJECT_ACCESS      /* Pointer to result destination          */
} BIP_Formal_Kind;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.8.2 BIP Context — State for call-site and return transformation
 *
 * Tracks the BIP state during code generation: what allocation form to use,
 * where to build the result, and whether task/finalization handling needed.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  Symbol         *func;              /* Function being transformed         */
  Type_Info      *result_type;       /* Return type                        */
  BIP_Alloc_Form  alloc_form;        /* Determined allocation strategy     */
  uint32_t        dest_ptr;          /* Temp holding destination address   */
  bool            needs_finalization;/* Has controlled components          */
  bool            has_tasks;         /* Has task components                */
} BIP_Context;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.8.3 Type Predicates — Pure functions for BIP decisions
 *
 * These predicates determine whether a type requires BIP handling.
 * Per Ada RM 7.5, limited types include:
 *   - Task types (always limited)
 *   - Types with "limited" in their declaration
 *   - Private types declared "limited private"
 *   - Composite types with limited components
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declaration - full Type_Info checking */
bool BIP_Type_Has_Task_Component (const Type_Info *t);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.8.4 Extra Formal Parameters — Hidden BIP parameters
 *
 * BIP functions receive extra hidden parameters prepended to their formals:
 *   __BIPalloc  : i32     (BIP_Alloc_Form enum value)
 *   __BIPaccess : ptr     (pointer to result destination)
 *   __BIPmaster : i32     (task master ID, if tasks)
 *   __BIPchain  : ptr     (activation chain, if tasks)
 *
 * These are added during code generation, not during semantic analysis,
 * so the Symbol structure remains unchanged.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* BIP extra formal names (matched by code generator) */
#define BIP_ALLOC_NAME   "__BIPalloc"
#define BIP_ACCESS_NAME  "__BIPaccess"
#define BIP_MASTER_NAME  "__BIPmaster"
#define BIP_CHAIN_NAME   "__BIPchain"
#define BIP_FINAL_NAME   "__BIPfinal"

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §15.8.6 Return Statement Expansion — Building result in place
 *
 * In a BIP function, return statements build directly into __BIPaccess:
 *
 *   return (Field1 => V1, Field2 => V2);
 *
 * Becomes (for CALLER allocation):
 *   __BIPaccess->Field1 = V1;
 *   __BIPaccess->Field2 = V2;
 *   return;
 *
 * For HEAP allocation, we allocate first then build:
 *   tmp = malloc (sizeof (T));
 *   tmp->Field1 = V1;
 *   tmp->Field2 = V2;
 *   *__BIPaccess = tmp;  // Return allocated pointer
 *   return;
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* BIP return state - tracks current function's BIP context */
typedef struct {
  bool     is_bip_function;     /* Current function uses BIP              */
  uint32_t bip_alloc_param;     /* Temp holding __BIPalloc value          */
  uint32_t bip_access_param;    /* Temp holding __BIPaccess pointer       */
  uint32_t bip_master_param;    /* Temp holding __BIPmaster (if tasks)    */
  uint32_t bip_chain_param;     /* Temp holding __BIPchain (if tasks)     */
  bool     has_task_components; /* Return type has tasks                  */
} BIP_Function_State;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §14 Include Path and Unit Loading
 *
 * Resolves Ada WITH clauses by searching include paths for source files,
 * loading package specs/bodies on demand, and tracking which units have
 * already been loaded to avoid duplicate processing.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

const char     *Include_Paths[32];

/* Track packages currently being loaded to detect cycles */
typedef struct {
  String_Slice names[64];
  int count;
} Loading_Set;

/* Forward declarations for functions defined after Load_Package_Spec */
char *Lookup_Path      (String_Slice name);
bool  Has_Precompiled_LL (String_Slice name);
char *Lookup_Path_Body (String_Slice name);

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §16. GENERIC EXPANSION - Macro-style instantiation
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * Generics via macro expansion:
 *   1. Parse generic declaration > store template AST
 *   2. On instantiation: clone template, substitute actuals
 *   3. Analyze cloned tree with actual types
 *   4. Generate code for each instantiation separately
 *
 * Key insight: We do NOT share code between instantiations. Each instance
 * gets its own copy with types fully substituted.
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §16.1 Instantiation_Env — Formal-to-actual mapping
 *
 * Instead of mutating nodes, we carry substitution environment through.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  String_Slice  formal_name;    /* Generic formal parameter name */
  Type_Info    *actual_type;    /* Substituted actual type */
  Symbol       *actual_symbol;  /* Actual symbol (for subprogram formals) */
  Syntax_Node  *actual_expr;    /* Actual expression (for object formals) */
} Generic_Mapping;
typedef struct {
  Generic_Mapping  mappings[32];
  uint32_t         count;
  Symbol          *instance_sym;   /* The instantiation symbol */
  Symbol          *template_sym;   /* The generic template symbol */
} Instantiation_Env;

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §16.3 Node_Deep_Clone — Deep copy with environment substitution
 *
 * Unlike the existing node_clone_substitute, this:
 *   • ALWAYS allocates new nodes (no aliasing)
 *   • Uses recursion depth tracking with proper error
 *   • Carries environment for type substitution
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
Syntax_Node *Node_Deep_Clone (Syntax_Node *node, Instantiation_Env *env,
                  int depth);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §12.3 Declaration Resolution
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
void Resolve_Declaration (Syntax_Node *node);

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * §13. LLVM IR CODE GENERATION
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 *
 * The AST is semantic and the IR is operational, with translation bridging the gap.
 *
 * Generate LLVM IR from the resolved AST. Key principles:
 *
 * 1. Operate at native type width;
 *    convert only at explicit Ada type conversions and LLVM intrinsic
 *    boundaries (memcpy length, alloc size must be i64)
 * 2. All pointer types use opaque 'ptr' (LLVM 15+)
 * 3. Static links for nested subprogram access
 * 4. Fat pointers for unconstrained arrays (ptr + bounds)
 */

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §13.1 Code Generator State
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  FILE         *output;

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
  bool          block_terminated;  /* True if current block has a terminator (ret/br) */

  /* Module header tracking for multi-unit files */
  bool          header_emitted;
  Symbol       *main_candidate;  /* Last parameterless library-level procedure */

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

  /* Address markers needed for 'ADDRESS on packages/generics */
  Symbol       *address_markers[256];
  uint32_t      address_marker_count;

  /* Track emitted function unique_ids to prevent duplicate definitions */
  uint32_t      emitted_func_ids[1024];
  uint32_t      emitted_func_count;

  /* Task body context: task entry points return ptr (for pthread compat) */
  bool          in_task_body;

  /* Package elaboration functions to call before main (for task starts etc.) */
  Symbol       *elab_funcs[64];
  uint32_t      elab_func_count;

  /* Temp register type tracking: maps temp_id to actual LLVM type string.
   * Used to resolve divergence between Expression_Llvm_Type (Ada type) and
   * actual generated type (from 'VAL, 'POS, arithmetic, etc.).
   * Ring buffer: index = temp_id % capacity. */
  #define TEMP_TYPE_CAPACITY 4096
  uint32_t      temp_type_keys[TEMP_TYPE_CAPACITY];
  const char   *temp_types[TEMP_TYPE_CAPACITY];

  /* Bitmap: is this temp a fat-pointer alloca (needs load before extractvalue)? */
  uint8_t       temp_is_fat_alloca[TEMP_TYPE_CAPACITY];

  /* Track all exception global names referenced during codegen.
   * Stored as heap-allocated strings like "seq_io__status_error_s0".
   * Used by Generate_Exception_Globals to emit definitions for all. */
  #define EXC_REF_CAPACITY 512
  char         *exc_refs[EXC_REF_CAPACITY];
  uint32_t      exc_ref_count;
  bool          needs_trim_helpers;

  /* Counter for assigning unique runtime type elaboration IDs */
  uint32_t      rt_type_counter;

  /* Nonzero when generating a component expression inside an outer
   * aggregate (array element or record field).  Used by Generate_Aggregate
   * to determine if positional aggregate bounds should start from the base
   * type's index subtype FIRST (RM 4.3.2(6) sub-aggregate rule). */
  uint32_t      in_agg_component;

  /* Multi-dim aggregate inner bounds tracking (RM 4.3.2(6)).
   * Set by inner Generate_Aggregate to report computed bounds
   * back to the outer multidim aggregate for consistency checking.
   * inner_agg_bnd[0] = this sub-aggregate's own first-dimension bounds.
   * inner_agg_bnd[1..n-1] = deeper inner dimensions' bounds (tracked
   * from child sub-aggregates via their inner consistency checks).
   * This allows outer aggregates to check ALL inner dimensions at once. */
#define MAX_AGG_DIMS 8
  uint32_t      inner_agg_bnd_lo[MAX_AGG_DIMS];
  uint32_t      inner_agg_bnd_hi[MAX_AGG_DIMS];
  int           inner_agg_bnd_n;    /* number of dimension levels reported */

  /* Cached discriminant constraint temps for multi-object declarations.
   * Set before generating a record aggregate so that discriminant checks
   * inside Generate_Aggregate reuse pre-evaluated values instead of
   * re-calling side-effectful constraint expressions (RM 3.2.2). */
#define MAX_DISC_CACHE 16
  uint32_t      disc_cache[MAX_DISC_CACHE];
  uint32_t      disc_cache_count;
  Type_Info    *disc_cache_type;  /* The record type these caches belong to */
} Code_Generator;
#define Emit_Raise_Constraint_Error(comment) Emit_Raise_Exception ("constraint_error", comment)
#define Emit_Raise_Program_Error(comment)    Emit_Raise_Exception ("program_error", comment)

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §13.1.1a Granular Runtime Check Emission
 *
 * Each Emit_*_Check function:
 *   1. Consults Check_Is_Suppressed () — returns early if suppressed.
 *   2. Emits the check logic (comparison + conditional branch).
 *   3. On failure, branches to a block that calls Emit_Raise_Constraint_Error.
 *   4. On success, falls through to a continuation block.
 *
 * LLVM IR pattern for every check:
 *   %cmp = icmp <pred> <type> %val, <bound>
 *   br i1 %cmp, label %raise, label %cont
 *   raise:
 *     <raise constraint_error>
 *   cont:
 *     ; continue execution
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declaration for Emit_Check_With_Raise (defined in §13.2.4) */
void Emit_Check_With_Raise (uint32_t cond,
                   bool raise_on_true, const char *comment);
uint32_t Emit_Convert (uint32_t src,
  const char *src_type, const char *dst_type);
uint32_t Emit_Coerce (uint32_t temp,
  const char *desired_type);
uint32_t Emit_Coerce_Default_Int (uint32_t temp,
  const char *desired_type);

/* Forward declaration for use in bound evaluation */
uint32_t Generate_Expression (Syntax_Node *node);
#define Emit_Fat_Pointer_Low(fp, bt)  Emit_Fat_Pointer_Bound ((fp), (bt), 0)
#define Emit_Fat_Pointer_High(fp, bt) Emit_Fat_Pointer_Bound ((fp), (bt), 1)

/* Consolidated helpers (defined in §13.2.1, §13.2.3) */
uint32_t Emit_Length_Clamped (uint32_t low, uint32_t high, const char *bt);
uint32_t Emit_Length_From_Bounds (uint32_t low, uint32_t high, const char *bt);
uint32_t Emit_Static_Int (int128_t value, const char *ty);
uint32_t Emit_Memcmp_Eq (uint32_t left_ptr, uint32_t right_ptr,
  uint32_t byte_size_temp, int64_t byte_size_static, bool is_dynamic);
uint32_t Emit_Type_Bound (Type_Bound *bound, const char *ty);
uint32_t Emit_Min_Value (uint32_t left, uint32_t right, const char *ty);
uint32_t Emit_Array_Lex_Compare (uint32_t left_ptr, uint32_t right_ptr,
  uint32_t elem_size, const char *bt);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §13.2.2 Bound Extraction Helpers
 *
 * Ada types carry bounds as either:
 *   - BOUND_INTEGER: compile-time constant
 *   - BOUND_EXPR: runtime expression (dynamic subtypes)
 *   - BOUND_FLOAT: floating-point constant (for float types)
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Bound_Temps: Holds emitted temps for a dimension's low and high bounds.
 * The bound_type field records the LLVM type used (e.g., "i32", "i64"). */
typedef struct {
  uint32_t low_temp;      /* Temp ID holding low bound value  */
  uint32_t high_temp;     /* Temp ID holding high bound value */
  const char *bound_type; /* LLVM type of bounds (e.g., "i32") */
} Bound_Temps;

/* Structure returned by Emit_Exception_Handler_Setup */
typedef struct {
  uint32_t handler_frame;   /* Alloca for { ptr, [200 x i8] } */
  uint32_t jmp_buf;         /* GEP to jmp_buf field */
  uint32_t normal_label;    /* Label for normal execution path */
  uint32_t handler_label;   /* Label for exception handler path */
} Exception_Setup;

/* Forward declarations for exception handler dispatch */
void Generate_Statement_List (Node_List *list);
void Emit_Branch_If_Needed (uint32_t label);
void Emit_Exception_Ref (Symbol *sym);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §13.3.1 Implicit Operators for Composite Types
 *
 * Ada requires equality operators for all non-limited types. For composite
 * types (records, arrays), equality is defined component-wise.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declaration for mutual recursion */
uint32_t Generate_Array_Equality (uint32_t left_ptr,
                    uint32_t right_ptr, Type_Info *array_type);

/* ── § 13a: Array Aggregate Helpers (RM 4.3.2) ──────────────────────
 *
 * These helpers factor out the repeated patterns in Generate_Aggregate:
 *   Agg_Classify       — count positional/named/others items
 *   Agg_Resolve_Elem   — generate element value, extract from fat ptr
 *   Agg_Store_At       — store element at array index (scalar or composite)
 *   Agg_Emit_Fill_Loop — emit a loop that fills a range with a value
 *   Agg_Wrap_Fat_Ptr   — wrap data+bounds into { ptr, ptr }
 *
 * Design: each helper is a pure function of its arguments — no hidden
 * state, no implicit coupling.  Haskell-flavoured C99 with `and`/`or`. */

/* ── Agg_Classify: classify aggregate items into positional / named / others */
typedef struct {
  uint32_t     n_positional;  /* count of bare (non-association) items    */
  bool         has_named;     /* any NK_ASSOCIATION items?                */
  bool         has_others;    /* is there an OTHERS choice?              */
  Syntax_Node *others_expr;   /* expression of OTHERS association (or NULL) */
} Agg_Class;

/* RM 3.7.1: After memcpy-ing a composite value into a record component,
 * verify that the stored discriminant(s) match the component type's
 * discriminant constraint(s).  E.g.  (H => INIT (1)) where H : PRIV (0). */
/* Forward declarations - defined later near Collect_Disc_Symbols_In_Expr */
uint32_t Emit_Disc_Constraint_Value (Type_Info *type_info, uint32_t disc_index, const char *disc_type);
void Emit_Nested_Disc_Checks (Type_Info *parent_type);

/* ── § 13b.0: Record Aggregate Component Store ─────────────────────────
 *
 * Literate summary:  a record aggregate (A => 1, B => "hello", C => rec)
 * must store each component value to the correct byte offset.  Three
 * representations arise — fat pointers, composite pointers, and scalars
 * — each requiring distinct LLVM IR.  This helper unifies the logic that
 * was previously duplicated across named, positional, and OTHERS paths.
 *
 *   data Src = Fat { ptr, ptr }   -- unconstrained array / dynamic bounds
 *            | Ptr ptr             -- composite record / constrained array
 *            | Val T               -- scalar / enumeration
 *
 *   store :: Src → CompPtr → IO ()
 *   store (Fat fp) dst = Emit_Fat_To_Array_Memcpy fp dst
 *   store (Ptr p)  dst = memcpy dst p (sizeof comp)
 *   store (Val v)  dst = store v dst
 *
 * After storage, discriminant values are mirrored to disc_agg_temps and
 * constraint-checked against the enclosing type (RM 3.7.1, 4.3.1). */
typedef struct { Symbol *sym; uint32_t temp; } Disc_Alloc_Entry;
typedef struct { Disc_Alloc_Entry *entries; uint32_t count; } Disc_Alloc_Info;
void Emit_Task_Function_Name (Symbol *task_sym, String_Slice fallback_name);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §13.4 Statement Code Generation
 *
 * Statements modify state while expressions compute values, a distinction Ada enforces.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
void Generate_Statement (Syntax_Node *node);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §13.4.8 Exception Handling
 *
 * The stack unwinder's memory is what makes exceptions possible.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */

/* Forward declarations */
void Generate_Declaration_List (Node_List *list);
void Generate_Generic_Instance_Body (Symbol *inst_sym, Syntax_Node *template_body);
void Generate_Task_Body (Syntax_Node *node);
void Generate_Subprogram_Body (Syntax_Node *node);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * §13.5 Declaration Code Generation
 *
 * Names get bound to meanings, and those bindings are what we generate.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
void Generate_Declaration (Syntax_Node *node);

/* Forward declare for recursive search */
bool Has_Nested_Subprograms (Node_List *declarations, Node_List *statements);

/* ─────────────────────────────────────────────────────────────────────────────────────────────────
 * Parallel compilation — fork-based worker called from a pthread.
 *
 * Each thread forks a child process that compiles one file.  fork() gives
 * complete isolation of all global state (arena, error count, loaded
 * packages, etc.) without refactoring Compile_File.
 * ────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
  const char *input_path;
  const char *output_path;  /* NULL > derive from input */
  int         exit_status;  /* 0 = success, 1 = failure */
} Compile_Job;

/* ═════════════════════════════════════════════════════════════════════════════════════════════════
 * END OF Ada 83
 * ═════════════════════════════════════════════════════════════════════════════════════════════════
 */

/* ------------------------------------------------------------------------------------------- */
/*  Global Variable Declarations                                                             */
/* ------------------------------------------------------------------------------------------- */

extern int Simd_Has_Avx512;
extern int Simd_Has_Avx2;
extern const uint8_t Id_Char_Table[256];
extern Memory_Arena Global_Arena;
extern const String_Slice Empty_Slice;
extern const Source_Location No_Location;
extern int Error_Count;
extern const char *Token_Name[TK_COUNT];
extern Type_Info *Frozen_Composite_Types[256];
extern uint32_t Frozen_Composite_Count;
extern Symbol *Exception_Symbols[256];
extern uint32_t Exception_Symbol_Count;
extern Symbol_Manager *sm;
extern uint32_t Crc32_Table[256];
extern bool Crc32_Table_Initialized;
extern ALI_Cache_Entry ALI_Cache[256];
extern uint32_t ALI_Cache_Count;
extern Elab_Graph g_elab_graph;
extern bool g_elab_graph_initialized;
extern BIP_Function_State g_bip_state;
extern uint32_t Include_Path_Count;
extern Syntax_Node *Loaded_Package_Bodies[128];
extern int Loaded_Body_Count;
extern String_Slice Loaded_Body_Names[128];
extern int Loaded_Body_Names_Count;
extern Loading_Set Loading_Packages;
extern Code_Generator *cg;

/* ------------------------------------------------------------------------------------------- */
/*  Function Prototypes                                                                       */
/* ------------------------------------------------------------------------------------------- */

bool Llvm_Type_Is_Pointer (const char *llvm_type);
bool Llvm_Type_Is_Fat_Pointer (const char *llvm_type);
void Simd_Detect_Features (void);
int Is_Alpha (char ch);
int Is_Digit (char ch);
int Is_Xdigit (char ch);
int Is_Space (char ch);
char To_Lower (char ch);
uint64_t To_Bits (uint64_t bytes);
uint64_t To_Bytes (uint64_t bits);
uint64_t Byte_Align (uint64_t bits);
size_t Align_To (size_t size, size_t alignment);
const char *Llvm_Int_Type (uint32_t bits);
const char *Llvm_Float_Type (uint32_t bits);
bool Fits_In_Signed (int128_t lo, int128_t hi, uint32_t bits);
bool Fits_In_Unsigned (int128_t lo, int128_t hi, uint32_t bits);
uint32_t Bits_For_Range (int128_t lo, int128_t hi);
uint32_t Bits_For_Modulus (uint128_t modulus);
void *Arena_Allocate (size_t size);
void Arena_Free_All (void);
String_Slice Slice_From_Cstring (const char *source);
String_Slice Slice_Duplicate (String_Slice slice);
bool Slice_Equal (String_Slice left, String_Slice right);
bool Slice_Equal_Ignore_Case (String_Slice left, String_Slice right);
uint64_t Slice_Hash (String_Slice slice);
__attribute__ ((unused))
int Edit_Distance (String_Slice left, String_Slice right);
void Report_Error (Source_Location location, const char *format, ...);
__attribute__ ((unused, noreturn))
void Fatal_Error (Source_Location location, const char *format, ...);
Big_Integer *Big_Integer_New (uint32_t capacity);
void Big_Integer_Ensure_Capacity (Big_Integer *integer, uint32_t needed);
void Big_Integer_Normalize (Big_Integer *integer);
void Big_Integer_Mul_Add_Small (Big_Integer *integer, uint64_t factor, uint64_t addend);
bool Big_Integer_Fits_Int64 (const Big_Integer *integer, int64_t *out);
bool Big_Integer_To_Uint128 (const Big_Integer *integer, uint128_t *out);
bool Big_Integer_To_Int128 (const Big_Integer *integer, int128_t *out);
int Big_Integer_Compare (const Big_Integer *left, const Big_Integer *right);
Big_Integer *Big_Integer_Add (const Big_Integer *left, const Big_Integer *right);
uint32_t Simd_Parse_8_Digits_Avx2 (const char *digits);
int Simd_Parse_Digits_Avx2 (const char *cursor, const char *limit, uint64_t *out);
Big_Integer *Big_Integer_From_Decimal_SIMD (const char *text);
Big_Real *Big_Real_New (void);
Big_Real *Big_Real_From_String (const char *text);
double Big_Real_To_Double (const Big_Real *real);
__attribute__ ((unused))
bool Big_Real_Fits_Double (const Big_Real *real);
__attribute__ ((unused))
void Big_Real_To_Hex (const Big_Real *real, char *buffer, size_t buffer_size);
Big_Integer *Big_Integer_Clone (const Big_Integer *source);
__attribute__ ((unused))
int Big_Real_Compare (const Big_Real *left, const Big_Real *right);
__attribute__ ((unused))
Big_Real *Big_Real_Add_Sub (const Big_Real *left, const Big_Real *right, bool subtract);
__attribute__ ((unused))
Big_Real *Big_Real_Scale (const Big_Real *real, int32_t scale);
__attribute__ ((unused))
Big_Real *Big_Real_Divide_Int (const Big_Real *dividend, int64_t divisor);
__attribute__ ((unused))
Big_Real *Big_Real_Multiply (const Big_Real *left, const Big_Real *right);
Big_Integer *Big_Integer_Multiply (const Big_Integer *left, const Big_Integer *right);
void Big_Integer_Div_Rem (const Big_Integer *dividend, const Big_Integer *divisor, Big_Integer **quotient_out, Big_Integer **remainder_out);
Big_Integer *Big_Integer_GCD (const Big_Integer *left, const Big_Integer *right);
Big_Integer *Big_Integer_One (void);
Rational Rational_Reduce (Big_Integer *numer, Big_Integer *denom);
Rational Rational_From_Big_Real (const Big_Real *real);
Rational Rational_From_Int (int64_t value);
Rational Rational_Add (Rational left, Rational right);
Rational Rational_Sub (Rational left, Rational right);
Rational Rational_Mul (Rational left, Rational right);
Rational Rational_Div (Rational left, Rational right);
Rational Rational_Pow (Rational base, int exponent);
int Rational_Compare (Rational left, Rational right);
__attribute__ ((unused))
double Rational_To_Double (Rational rational);
Token_Kind Lookup_Keyword (String_Slice name);
Lexer Lexer_New (const char *source, size_t length, const char *filename);
uint32_t Tzcnt32 (uint32_t value);
uint64_t Tzcnt64 (uint64_t value);
const char *Simd_Skip_Whitespace_Avx512 (const char *cursor, const char *limit);
const char *Simd_Skip_Whitespace_Avx2 (const char *cursor, const char *limit);
const char *Simd_Skip_Whitespace (const char *cursor, const char *limit);
const char *Simd_Find_Char_X86 (const char *cursor, const char *limit, char target);
const char *Simd_Find_Newline (const char *cursor, const char *limit);
const char *Simd_Find_Quote (const char *cursor, const char *limit);
const char *Simd_Find_Double_Quote (const char *cursor, const char *limit);
const char *Simd_Scan_Identifier (const char *cursor, const char *limit);
const char *Simd_Scan_Digits (const char *cursor, const char *limit);
char Lexer_Peek (const Lexer *lex, size_t offset);
char Lexer_Advance (Lexer *lex);
void Lexer_Skip_Whitespace_And_Comments (Lexer *lex);
Token Make_Token (Token_Kind kind, Source_Location location, String_Slice text);
Token Scan_Identifier (Lexer *lex);
int Digit_Value (char ch);
Token Scan_Number (Lexer *lex);
Token Scan_Character_Literal (Lexer *lex);
Token Scan_String_Literal (Lexer *lex);
Token Lexer_Next_Token (Lexer *lex);
void Node_List_Push (Node_List *list, Syntax_Node *node);
Syntax_Node *Node_New (Node_Kind kind, Source_Location location);
Parser Parser_New (const char *source, size_t length, const char *filename);
bool Parser_At (Parser *parser, Token_Kind kind);
bool Parser_At_Any (Parser *parser, Token_Kind k1, Token_Kind k2);
bool Parser_Peek_At (Parser *parser, Token_Kind kind);
Token Parser_Advance (Parser *parser);
bool Parser_Match (Parser *parser, Token_Kind kind);
Source_Location Parser_Location (Parser *parser);
void Parser_Error (Parser *parser, const char *message);
void Parser_Error_At_Current (Parser *parser, const char *expected);
void Parser_Synchronize (Parser *parser);
bool Parser_Check_Progress (Parser *parser);
bool Parser_Expect (Parser *parser, Token_Kind kind);
String_Slice Parser_Identifier (Parser *parser);
void Parser_Check_End_Name (Parser *parser, String_Slice expected_name);
void Parse_Parameter_List (Parser *p, Node_List *params);
Syntax_Node *Parse_Primary (Parser *p);
Syntax_Node *Parse_Name (Parser *p);
Syntax_Node *Parse_Simple_Name (Parser *p);
Syntax_Node *Parse_Choice (Parser *p);
void Parse_Association_List (Parser *p, Node_List *list);
Precedence Get_Infix_Precedence (Token_Kind kind);
bool Is_Right_Associative (Token_Kind kind);
Syntax_Node *Parse_Unary(Parser *p);
Syntax_Node *Parse_Expression_Precedence(Parser *p, Precedence min_prec);
Syntax_Node *Parse_Expression (Parser *p);
Syntax_Node *Parse_Range(Parser *p);
Syntax_Node *Parse_Subtype_Indication (Parser *p);
Syntax_Node *Parse_Type_Definition (Parser *p);
Syntax_Node *Parse_Pragma (Parser *p);
Syntax_Node *Parse_Assignment_Or_Call(Parser *p);
Syntax_Node *Parse_Return_Statement(Parser *p);
Syntax_Node *Parse_Exit_Statement(Parser *p);
Syntax_Node *Parse_Goto_Statement(Parser *p);
Syntax_Node *Parse_Raise_Statement(Parser *p);
Syntax_Node *Parse_Delay_Statement(Parser *p);
Syntax_Node *Parse_Abort_Statement(Parser *p);
Syntax_Node *Parse_If_Statement(Parser *p);
Syntax_Node *Parse_Case_Statement(Parser *p);
Syntax_Node *Parse_Loop_Statement(Parser *p, String_Slice label);
Syntax_Node *Parse_Block_Statement(Parser *p, String_Slice label);
Syntax_Node *Parse_Accept_Statement(Parser *p);
Syntax_Node *Parse_Select_Statement(Parser *p);
Syntax_Node *Parse_Statement (Parser *p);
void Parse_Statement_Sequence (Parser *p, Node_List *list);
Syntax_Node *Parse_Object_Declaration(Parser *p);
Syntax_Node *Parse_Discriminant_Part (Parser *p);
Syntax_Node *Parse_Type_Declaration(Parser *p);
Syntax_Node *Parse_Subtype_Declaration(Parser *p);
Syntax_Node *Parse_Enumeration_Type (Parser *p);
Syntax_Node *Parse_Array_Type (Parser *p);
Syntax_Node *Parse_Discrete_Range(Parser *p);
Syntax_Node *Parse_Record_Type (Parser *p);
Syntax_Node *Parse_Variant_Part (Parser *p);
Syntax_Node *Parse_Access_Type(Parser *p);
Syntax_Node *Parse_Derived_Type(Parser *p);
Syntax_Node *Parse_Procedure_Specification(Parser *p);
Syntax_Node *Parse_Function_Specification(Parser *p);
Syntax_Node *Parse_Subprogram_Body (Parser *p, Syntax_Node *spec);
Syntax_Node *Parse_Package_Specification(Parser *p);
Syntax_Node *Parse_Package_Body (Parser *p);
void Parse_Generic_Formal_Part (Parser *p, Node_List *formals);
Syntax_Node *Parse_Generic_Declaration(Parser *p);
Syntax_Node *Parse_Use_Clause (Parser *p);
Syntax_Node *Parse_With_Clause (Parser *p);
Syntax_Node *Parse_Representation_Clause (Parser *p);
Syntax_Node *Parse_Declaration (Parser *p);
void Parse_Declarative_Part (Parser *p, Node_List *list);
Syntax_Node *Parse_Context_Clause (Parser *p);
Syntax_Node *Parse_Compilation_Unit (Parser *p);
Type_Info *Type_New (Type_Kind kind, String_Slice name);
bool Type_Is_Scalar (const Type_Info *type_info);
bool Type_Is_Discrete (const Type_Info *type_info);
bool Type_Is_Numeric (const Type_Info *type_info);
bool Type_Is_Real (const Type_Info *type_info);
bool Type_Is_Float_Representation (const Type_Info *type_info);
bool Type_Is_Array_Like (const Type_Info *type_info);
bool Type_Is_Composite (const Type_Info *type_info);
bool Type_Is_Access (const Type_Info *type_info);
bool Type_Is_Record (const Type_Info *type_info);
bool Type_Is_Task (const Type_Info *type_info);
bool Type_Is_Float (const Type_Info *type_info);
bool Type_Is_Fixed_Point (const Type_Info *type_info);
const char *Float_Llvm_Type_Of (const Type_Info *type_info);
bool Float_Is_Single (const Type_Info *type_info);
int Float_Effective_Digits (const Type_Info *type_info);
void Float_Model_Parameters (const Type_Info *type_info, int64_t *out_mantissa, int64_t *out_emax);
bool Type_Is_Private (const Type_Info *type_info);
bool Type_Is_Limited (const Type_Info *type_info);
bool Type_Is_Integer_Like (const Type_Info *type_info);
bool Type_Is_Unsigned (const Type_Info *type_info);
bool Type_Is_Enumeration (const Type_Info *type_info);
bool Type_Is_Boolean (const Type_Info *type_info);
bool Type_Is_Character (const Type_Info *type_info);
bool Type_Is_String (const Type_Info *type_info);
bool Type_Needs_Fat_Pointer (const Type_Info *type_info);
bool Type_Is_Unconstrained_Array (const Type_Info *type_info);
bool Type_Is_Constrained_Array (const Type_Info *type_info);
bool Type_Is_Universal_Integer (const Type_Info *type_info);
bool Type_Is_Universal_Real (const Type_Info *type_info);
bool Type_Is_Universal (const Type_Info *type_info);
bool Type_Has_Dynamic_Bounds (const Type_Info *type_info);
bool Expression_Is_Slice (const Syntax_Node *node);
bool Expression_Produces_Fat_Pointer (const Syntax_Node *node, const Type_Info *type);
bool Type_Needs_Fat_Pointer_Load (const Type_Info *type_info);
Type_Info *Type_Base (Type_Info *type_info);
Type_Info *Type_Root (Type_Info *type_info);
void Freeze_Type (Type_Info *type_info);
const char *Type_To_Llvm (Type_Info *type_info);
const char *Type_To_Llvm_Sig (Type_Info *type_info);
const char *Array_Bound_Llvm_Type (const Type_Info *type_info);
const char *Bounds_Type_For (const char *bound_type);
int Bounds_Alloc_Size (const char *bound_type);
bool Param_Is_By_Reference (Parameter_Mode mode);
void Set_Generic_Type_Map (Symbol *inst);
bool Check_Is_Suppressed (Type_Info *type, Symbol *sym, uint32_t check_bit);
Scope *Scope_New (Scope *parent);
void Symbol_Manager_Push_Scope (Symbol *owner);
void Symbol_Manager_Pop_Scope (void);
void Symbol_Manager_Push_Existing_Scope (Scope *scope);
uint32_t Symbol_Hash_Name (String_Slice name);
Symbol *Symbol_New (Symbol_Kind kind, String_Slice name, Source_Location location);
void Symbol_Add (Symbol *sym);
Symbol *Symbol_Find (String_Slice name);
Symbol *Symbol_Find_By_Type (String_Slice name, Type_Info *expected_type);
bool Type_Covers (Type_Info *expected, Type_Info *actual);
bool Arguments_Match_Profile (Symbol *sym, Argument_Info *args);
void Collect_Interpretations (String_Slice name, Interp_List *interps);
void Filter_By_Arguments (Interp_List *interps, Argument_Info *args);
bool Symbol_Hides (Symbol *sym1, Symbol *sym2);
int32_t Score_Interpretation (Interpretation *interp, Type_Info *context_type, Argument_Info *args);
Symbol *Disambiguate(Interp_List *interps, Type_Info *context_type, Argument_Info *args);
Symbol *Resolve_Overloaded_Call ( String_Slice name, Argument_Info *args, Type_Info *context_type);
void Symbol_Manager_Init_Predefined (void);
void Symbol_Manager_Init (void);
Type_Info *Resolve_Identifier (Syntax_Node *node);
Type_Info *Resolve_Selected (Syntax_Node *node);
String_Slice Operator_Name (Token_Kind op);
bool Resolve_Char_As_Enum (Syntax_Node *char_node, Type_Info *enum_type);
Type_Info *Resolve_Binary_Op (Syntax_Node *node);
Type_Info *Resolve_Apply (Syntax_Node *node);
bool Is_Integer_Expr (Syntax_Node *node); /* Forward declaration */ bool Is_Integer_Expr (Syntax_Node *node);
double Eval_Const_Numeric (Syntax_Node *node);
bool Eval_Const_Rational (Syntax_Node *node, Rational *out);
bool Eval_Const_Uint128 (Syntax_Node *node, uint128_t *out);
int128_t Type_Bound_Value (Type_Bound bound);
const char *I128_Decimal (int128_t value);
const char *U128_Decimal (uint128_t value);
int128_t Array_Element_Count (Type_Info *type_info);
int128_t Array_Low_Bound (Type_Info *type_info);
Type_Info *Resolve_Expression (Syntax_Node *node);
void Freeze_Declaration_List (Node_List *list);
void Populate_Package_Exports (Symbol *pkg_sym, Syntax_Node *pkg_spec);
void Preregister_Labels (Node_List *list);
void Resolve_Statement_List (Node_List *list);
void Resolve_Statement (Syntax_Node *node);
char *Read_File(const char *path, size_t *out_size);
void Crc32_Init_Table (void);
uint32_t Crc32 (const char *data, size_t length);
void Unit_Name_To_File (String_Slice unit_name, bool is_body, char *out, size_t out_size);
void ALI_Collect_Withs (ALI_Info *ali, Syntax_Node *ctx);
String_Slice Get_Subprogram_Name (Syntax_Node *node);
void ALI_Collect_Exports (ALI_Info *ali, Syntax_Node *unit);
void ALI_Collect_Unit (ALI_Info *ali, Syntax_Node *cu, const char *source, size_t source_size);
String_Slice LLVM_Type_Basic (String_Slice ada_type);
void ALI_Write (FILE *out, ALI_Info *ali);
void Generate_ALI_File (const char *output_path, Syntax_Node **units, int unit_count, const char *source, size_t source_size);
const char *ALI_Skip_Ws (const char *cursor);
const char *ALI_Read_Token (const char *cursor, char *buf, size_t bufsize);
uint32_t ALI_Parse_Hex (const char *str);
ALI_Cache_Entry *ALI_Read (const char *ali_path);
bool ALI_Is_Current (const char *ali_path, const char *source_path);
bool Edge_Kind_Is_Strong (Elab_Edge_Kind k);
Elab_Graph Elab_Graph_New (void);
uint32_t Elab_Add_Vertex (Elab_Graph *g, String_Slice name, Elab_Unit_Kind kind, Symbol *sym);
uint32_t Elab_Find_Vertex (const Elab_Graph *g, String_Slice name, Elab_Unit_Kind kind);
Elab_Vertex *Elab_Get_Vertex(Elab_Graph *g, uint32_t id);
const Elab_Vertex *Elab_Get_Vertex_Const(const Elab_Graph *g, uint32_t id);
uint32_t Elab_Add_Edge (Elab_Graph *g, uint32_t pred_id, uint32_t succ_id, Elab_Edge_Kind kind);
void Tarjan_Strongconnect (Elab_Graph *g, Tarjan_State *s, uint32_t v_id);
void Elab_Find_Components (Elab_Graph *g);
bool Elab_Is_Elaborable (const Elab_Vertex *v);
bool Elab_Is_Weakly_Elaborable (const Elab_Vertex *v);
bool Elab_Has_Elaborable_Body (const Elab_Graph *g, const Elab_Vertex *v);
Elab_Precedence Elab_Compare_Vertices (const Elab_Graph *g, const Elab_Vertex *a, const Elab_Vertex *b);
Elab_Precedence Elab_Compare_Weak (const Elab_Graph *g, const Elab_Vertex *a, const Elab_Vertex *b);
Elab_Vertex_Set Elab_Set_Empty (void);
bool Elab_Set_Contains (const Elab_Vertex_Set *s, uint32_t id);
void Elab_Set_Insert (Elab_Vertex_Set *s, uint32_t id);
void Elab_Set_Remove (Elab_Vertex_Set *s, uint32_t id);
uint32_t Elab_Set_Size (const Elab_Vertex_Set *s);
uint32_t Elab_Find_Best_Vertex (const Elab_Graph *g, const Elab_Vertex_Set *candidates, Elab_Vertex_Pred pred, Elab_Vertex_Cmp cmp);
void Elab_Update_Successor (Elab_Graph *g, uint32_t edge_id, Elab_Vertex_Set *elaborable, Elab_Vertex_Set *waiting);
void Elab_Elaborate_Vertex (Elab_Graph *g, uint32_t v_id, Elab_Vertex_Set *elaborable, Elab_Vertex_Set *waiting);
Elab_Order_Status Elab_Elaborate_Graph (Elab_Graph *g);
void Elab_Pair_Specs_Bodies (Elab_Graph *g);
void Elab_Init (void);
uint32_t Elab_Register_Unit (String_Slice name, bool is_body, Symbol *sym, bool is_preelaborate, bool is_pure, bool has_elab_code);
Elab_Order_Status Elab_Compute_Order (void);
uint32_t Elab_Get_Order_Count (void);
Symbol *Elab_Get_Order_Symbol(uint32_t index);
bool Elab_Needs_Elab_Call (uint32_t index);
bool BIP_Is_Explicitly_Limited (const Type_Info *t);
bool BIP_Is_Task_Type (const Type_Info *t);
bool BIP_Record_Has_Limited_Component (const Type_Info *t);
bool BIP_Is_Limited_Type (const Type_Info *t);
bool BIP_Is_BIP_Function (const Symbol *func);
bool BIP_Type_Has_Task_Component (const Type_Info *t);
bool BIP_Needs_Alloc_Form (const Symbol *func);
uint32_t BIP_Extra_Formal_Count (const Symbol *func);
BIP_Alloc_Form BIP_Determine_Alloc_Form (bool is_allocator, bool in_return_stmt, bool has_target);
void BIP_Begin_Function (const Symbol *func);
bool BIP_In_BIP_Function (void);
void BIP_End_Function (void);
bool Body_Already_Loaded (String_Slice name);
void Mark_Body_Loaded (String_Slice name);
bool Loading_Set_Contains (String_Slice name);
void Loading_Set_Add (String_Slice name);
void Loading_Set_Remove (String_Slice name);
char *ALI_Find (String_Slice unit_name);
void ALI_Load_Symbols (ALI_Cache_Entry *entry);
bool Try_Load_From_ALI (String_Slice name);
void Load_Package_Spec (String_Slice name, char *src);
Type_Info *Env_Lookup_Type (Instantiation_Env *env, String_Slice name);
Syntax_Node *Env_Lookup_Expr (Instantiation_Env *env, String_Slice name);
void Node_List_Clone (Node_List *dst, Node_List *src, Instantiation_Env *env, int depth);
Syntax_Node *Node_Deep_Clone (Syntax_Node *node, Instantiation_Env *env, int depth);
void Build_Instantiation_Env (Instantiation_Env *env, Symbol *template_sym, Symbol *instance_sym);
void Expand_Generic_Package (Symbol *instance_sym);
void Install_Declaration_Symbols (Node_List *decls);
char *Read_File_Simple(const char *path);
char *Lookup_Path(String_Slice name);
bool Has_Precompiled_LL (String_Slice name);
char *Lookup_Path_Body (String_Slice name);
void Resolve_Declaration_List (Node_List *list);
bool Types_Same_Named (Type_Info *t1, Type_Info *t2);
bool Subprogram_Is_Primitive_Of (Symbol *sub, Type_Info *type);
void Create_Derived_Operation (Symbol *sub, Type_Info *derived_type, Type_Info *parent_type, Symbol *type_sym);
void Derive_Subprograms (Type_Info *derived_type, Type_Info *parent_type, Symbol *type_sym);
void Resolve_Declaration (Syntax_Node *node);
void Resolve_Compilation_Unit (Syntax_Node *node);
void Code_Generator_Init (FILE *output);
void Temp_Set_Type (uint32_t temp_id, const char *llvm_type);
const char *Temp_Get_Type (uint32_t temp_id);
void Temp_Mark_Fat_Alloca (uint32_t temp_id);
bool Temp_Is_Fat_Alloca (uint32_t temp_id);
const char *String_Bound_Type (void);
const char *String_Bounds_Struct (void);
int String_Bounds_Alloc (void);
const char *Integer_Arith_Type (void);
void Emit_String_Const (const char *format, ...);
void Emit_String_Const_Char (char ch);
uint32_t Emit_Temp (void);
uint32_t Emit_Label (void);
void Emit (const char *format, ...);
void Emit_Location (Source_Location location);
void Emit_Label_Here (uint32_t label);
void Emit_Branch_If_Needed (uint32_t label);
void Emit_Float_Constant (uint32_t result, const char *float_type, double value, const char *comment);
bool Symbol_Is_Global (Symbol *sym);
size_t Mangle_Slice_Into (char *buf, size_t pos, size_t max, String_Slice name);
size_t Mangle_Into_Buffer (char *buf, size_t pos, size_t max, Symbol *sym);
String_Slice Symbol_Mangle_Name (Symbol *sym);
String_Slice Mangle_Qualified_Name (String_Slice parent, String_Slice name);
Symbol *Find_Instance_Local (const Symbol *template_sym);
void Emit_Symbol_Name (Symbol *sym);
void Emit_Symbol_Ref (Symbol *sym);
void Emit_Exception_Ref (Symbol *exc);
Symbol *Find_Enclosing_Subprogram (Symbol *sym);
bool Subprogram_Needs_Static_Chain (Symbol *sym);
bool Is_Uplevel_Access (const Symbol *sym);
void Emit_Symbol_Storage (Symbol *sym);
int Nested_Frame_Depth (Symbol *proc);
uint32_t Precompute_Nested_Frame_Arg (Symbol *proc);
bool Emit_Nested_Frame_Arg (Symbol *proc, uint32_t precomp);
Type_Info *Resolve_Generic_Actual_Type (Type_Info *type);
void Emit_Raise_Exception (const char *exc_name, const char *comment);
uint32_t Emit_Overflow_Checked_Op ( uint32_t left, uint32_t right, const char *op, /* "add", "sub", "mul" */ const char *llvm_type, /* "i8", "i16", "i32", "i64", "i128" */ Type_Info *result_type);
void Emit_Division_Check (uint32_t divisor, const char *llvm_type, Type_Info *type);
void Emit_Signed_Division_Overflow_Check (uint32_t dividend, uint32_t divisor, const char *llvm_type, Type_Info *type);
uint32_t Emit_Index_Check (uint32_t index, uint32_t low_bound, uint32_t high_bound, const char *index_type, Type_Info *array_type);
void Emit_Length_Check (uint32_t src_length, uint32_t dst_length, const char *len_type, Type_Info *array_type);
void Emit_Access_Check (uint32_t ptr_val, Type_Info *acc_type);
void Emit_Discriminant_Check (uint32_t actual, uint32_t expected, const char *disc_type, Type_Info *record_type);
int Type_Bits (const char *llvm_type);
const char *Wider_Int_Type (const char *left, const char *right);
bool Is_Float_Type (const char *llvm_type);
const char *Float_Cmp_Predicate (int op);
const char *Int_Cmp_Predicate (int op, bool is_unsigned);
bool Expression_Is_Boolean (Syntax_Node *node);
bool Expression_Is_Float (Syntax_Node *node);
const char *Expression_Llvm_Type (Syntax_Node *node);
uint32_t Emit_Convert_Ext (uint32_t src, const char *src_type, const char *dst_type, bool is_unsigned);
uint32_t Emit_Convert (uint32_t src, const char *src_type, const char *dst_type);
uint32_t Emit_Coerce (uint32_t temp, const char *desired_type);
uint32_t Emit_Coerce_Default_Int (uint32_t temp, const char *desired_type);
uint32_t Emit_Bound_Value_Typed (Type_Bound *bound, const char **out_type);
uint32_t Emit_Bound_Value (Type_Bound *bound);
uint32_t Emit_Constraint_Check_With_Type (uint32_t val, Type_Info *target, Type_Info *source, const char *actual_val_type);
uint32_t Emit_Constraint_Check (uint32_t val, Type_Info *target, Type_Info *source);
void Emit_Subtype_Constraint_Compat_Check (Type_Info *subtype);
uint32_t Emit_Fat_Pointer (uint32_t data_ptr, int128_t low, int128_t high, const char *bt);
uint32_t Emit_Widen_For_Intrinsic (uint32_t val, const char *from_type);
uint32_t Emit_Extend_To_I64 (uint32_t val, const char *from_type);
uint32_t Fat_Ptr_As_Value (uint32_t fat_ptr);
uint32_t Emit_Fat_Pointer_Data (uint32_t fat_ptr, const char *bt);
uint32_t Emit_Fat_Pointer_Bound (uint32_t fat_ptr, const char *bt, uint32_t field_index);
uint32_t Emit_Fat_Pointer_Low_Dim (uint32_t fat_ptr, const char *bt, uint32_t dim);
uint32_t Emit_Fat_Pointer_High_Dim (uint32_t fat_ptr, const char *bt, uint32_t dim);
uint32_t Emit_Fat_Pointer_Length_Dim (uint32_t fat_ptr, const char *bt, uint32_t dim);
uint32_t Emit_Alloc_Bounds_MultiDim (uint32_t *bounds_lo, uint32_t *bounds_hi, uint32_t ndims, const char *bt);
uint32_t Emit_Fat_Pointer_MultiDim (uint32_t data_ptr, uint32_t *bounds_lo, uint32_t *bounds_hi, uint32_t ndims, const char *bt);
uint32_t Emit_Alloc_Bounds_Struct (uint32_t low_temp, uint32_t high_temp, const char *bt);
uint32_t Emit_Fat_Pointer_Dynamic (uint32_t data_ptr, uint32_t low_temp, uint32_t high_temp, const char *bt);
uint32_t Emit_Heap_Bounds_Struct (uint32_t low_temp, uint32_t high_temp, const char *bt);
uint32_t Emit_Fat_Pointer_Heap (uint32_t data_ptr, uint32_t low_temp, uint32_t high_temp, const char *bt);
uint32_t Emit_Fat_Pointer_Length (uint32_t fat_ptr, const char *bt);
uint32_t Emit_Length_From_Bounds (uint32_t low, uint32_t high, const char *bt);
void Emit_Fat_To_Array_Memcpy (uint32_t fat_val, uint32_t dest_ptr, Type_Info *array_type);
uint32_t Emit_Length_Clamped (uint32_t low, uint32_t high, const char *bt);
uint32_t Emit_Static_Int (int128_t value, const char *ty);
uint32_t Emit_Memcmp_Eq (uint32_t left_ptr, uint32_t right_ptr, uint32_t byte_size_temp, int64_t byte_size_static, bool is_dynamic);
uint32_t Emit_Single_Bound (Type_Bound *bound, const char *target_type);
Bound_Temps Emit_Bounds (Type_Info *type, uint32_t dim);
Bound_Temps Emit_Bounds_From_Fat (uint32_t fat_ptr, const char *bt);
Bound_Temps Emit_Bounds_From_Fat_Dim (uint32_t fat_ptr, const char *bt, uint32_t dim);
void Emit_Check_With_Raise (uint32_t cond, bool raise_on_true, const char *comment);
void Emit_Range_Check_With_Raise (uint32_t val, int64_t lo_val, int64_t hi_val, const char *type, const char *comment);
uint32_t Emit_Type_Bound (Type_Bound *bound, const char *ty);
void Emit_Fat_Pointer_Copy_To_Name (uint32_t fat_ptr, Symbol *dst, const char *bt);
uint32_t Emit_Load_Fat_Pointer (Symbol *sym, const char *bt);
uint32_t Emit_Load_Fat_Pointer_From_Temp (uint32_t ptr_temp, const char *bt);
void Emit_Store_Fat_Pointer_To_Symbol (uint32_t fat_val, Symbol *sym, const char *bt);
void Emit_Store_Fat_Pointer_Fields_To_Symbol (uint32_t data_ptr, uint32_t low_temp, uint32_t high_temp, Symbol *sym, const char *bt);
void Emit_Store_Fat_Pointer_Fields_To_Temp (uint32_t data_ptr, uint32_t low_temp, uint32_t high_temp, uint32_t fat_alloca, const char *bt);
uint32_t Emit_Fat_Pointer_Compare (uint32_t left_fat, uint32_t right_fat, const char *bt);
uint32_t Emit_Min_Value (uint32_t left, uint32_t right, const char *ty);
Exception_Setup Emit_Exception_Handler_Setup (void);
uint32_t Emit_Current_Exception_Id (void);
void Generate_Exception_Dispatch (Node_List *handlers, uint32_t exc_id, uint32_t end_label);
uint32_t Emit_Array_Lex_Compare (uint32_t left_ptr, uint32_t right_ptr, uint32_t elem_size, const char *bt);
void Emit_Fat_Pointer_Insertvalue_Named (const char *prefix, const char *data_expr, const char *low_expr, const char *high_expr, const char *bt);
void Emit_Fat_Pointer_Extractvalue_Named (const char *src_name, const char *data_name, const char *low_name, const char *high_name, const char *bt);
void Emit_Widen_Named_For_Intrinsic (const char *src_name, const char *dst_name, const char *bt);
void Emit_Narrow_Named_From_Intrinsic (const char *src_name, const char *dst_name, const char *bt);
uint32_t Emit_Fat_Pointer_Null (const char *bt);
uint32_t Generate_Lvalue (Syntax_Node *node);
uint32_t Generate_Bound_Value (Type_Bound bound, const char *target_type);
uint32_t Generate_Integer_Literal (Syntax_Node *node);
uint32_t Generate_Real_Literal (Syntax_Node *node);
uint32_t Generate_String_Literal (Syntax_Node *node);
uint32_t Generate_Identifier (Syntax_Node *node);
uint32_t Generate_Record_Equality (uint32_t left_ptr, uint32_t right_ptr, Type_Info *record_type);
uint32_t Generate_Array_Equality (uint32_t left_ptr, uint32_t right_ptr, Type_Info *array_type);
uint32_t Generate_Composite_Address (Syntax_Node *node);
uint32_t Emit_Bool_Array_Binop (uint32_t left, uint32_t right, Type_Info *result_type, const char *ir_op);
uint32_t Emit_Bool_Array_Not (uint32_t operand, Type_Info *result_type);
bool Type_Is_Bool_Array (const Type_Info *t);
uint32_t Normalize_To_Fat_Pointer (Syntax_Node *expr, uint32_t raw, Type_Info *type, const char *bt);
bool Aggregate_Produces_Fat_Pointer (const Type_Info *t);
void Ensure_Runtime_Type_Globals (Type_Info *t);
uint32_t Wrap_Constrained_As_Fat (Syntax_Node *expr, Type_Info *type, const char *bt);
uint32_t Convert_Real_To_Fixed (uint32_t val, double small, const char *fix_type);
uint32_t Get_Dimension_Index (Syntax_Node *arg); /* forward decl */ uint32_t Generate_Binary_Op (Syntax_Node *node);
uint32_t Generate_Unary_Op (Syntax_Node *node);
uint32_t Generate_Apply (Syntax_Node *node);
uint32_t Generate_Selected (Syntax_Node *node);
bool Type_Bound_Is_Compile_Time_Known (Type_Bound b);
double Type_Bound_Float_Value (Type_Bound b);
bool Type_Bound_Is_Set (Type_Bound b);
void Emit_Float_Type_Limit (uint32_t t, Type_Info *type, bool is_low, String_Slice attr);
uint32_t Emit_Bound_Attribute (uint32_t t, Type_Info *prefix_type, Symbol *prefix_sym, Syntax_Node *prefix_expr, bool needs_runtime_bounds, uint32_t dim, bool is_low, String_Slice attr);
uint32_t Generate_Attribute (Syntax_Node *node);
int32_t Find_Record_Component (Type_Info *record_type, String_Slice name);
bool Is_Others_Choice (Syntax_Node *choice);
bool Is_Static_Int_Node (Syntax_Node *n);
int128_t Static_Int_Value (Syntax_Node *n);
Agg_Class Agg_Classify (Syntax_Node *node);
bool Bound_Pair_Overflows (Type_Bound low, Type_Bound high);
uint32_t Agg_Resolve_Elem (Syntax_Node *expr, bool multidim, bool elem_is_composite, Type_Info *agg_type, const char *elem_type, Type_Info *elem_ti);
void Agg_Store_At_Static (uint32_t base, uint32_t val, int128_t idx, const char *elem_type, uint32_t elem_size, bool is_composite);
void Agg_Store_At_Dynamic (uint32_t base, uint32_t val, uint32_t arr_idx, const char *idx_type, const char *elem_type, uint32_t elem_size, uint32_t rt_row_size, bool is_composite);
bool Agg_Elem_Is_Composite (Type_Info *elem_ti, bool multidim);
void Emit_Comp_Disc_Check (uint32_t ptr, Type_Info *comp_ti);
void Emit_Inner_Consistency_Track ( uint32_t *inner_trk_lo, uint32_t *inner_trk_hi, uint32_t inner_trk_first, uint32_t inner_trk_mm, int n_inner_dims, const char *bt);
void Desugar_Aggregate_Range_Choices (Syntax_Node *agg);
uint32_t Agg_Comp_Byte_Size (Type_Info *ti, Syntax_Node *src_expr);
void Agg_Rec_Store (uint32_t val, uint32_t dest_ptr, Component_Info *comp, Syntax_Node *src_expr);
void Agg_Rec_Disc_Post (uint32_t val, Component_Info *comp, uint32_t disc_ordinal, Type_Info *agg_type, Disc_Alloc_Info *da_info);
uint32_t Disc_Ordinal_Before (Type_Info *type_info, uint32_t comp_index);
uint32_t Generate_Aggregate (Syntax_Node *node);
uint32_t Generate_Qualified (Syntax_Node *node);
uint32_t Generate_Allocator (Syntax_Node *node);
uint32_t Generate_Expression (Syntax_Node *node);
void Generate_Statement_List (Node_List *list);
void Generate_Assignment (Syntax_Node *node);
void Generate_If_Statement (Syntax_Node *node);
void Generate_Loop_Statement (Syntax_Node *node);
void Generate_Return_Statement (Syntax_Node *node);
void Generate_Case_Statement (Syntax_Node *node);
void Generate_For_Loop (Syntax_Node *node);
void Generate_Raise_Statement (Syntax_Node *node);
void Generate_Block_Statement (Syntax_Node *node);
void Generate_Statement (Syntax_Node *node);
bool Is_Builtin_Function (String_Slice name);
void Emit_Extern_Subprogram (Symbol *sym);
void Generate_Declaration_List (Node_List *list);
uint32_t Emit_Disc_Constraint_Value (Type_Info *type_info, uint32_t disc_index, const char *disc_type);
void Emit_Nested_Disc_Checks (Type_Info *parent_type);
void Collect_Disc_Symbols_In_Expr (Syntax_Node *node, Symbol **found, uint32_t *count, uint32_t max);
void Generate_Object_Declaration (Syntax_Node *node);
bool Has_Nested_In_Statements (Node_List *statements);
bool Has_Nested_Subprograms (Node_List *declarations, Node_List *statements);
void Emit_Function_Header (Symbol *sym, bool is_nested);
Syntax_Node *Find_Homograph_Body (Symbol **exports, uint32_t idx, String_Slice name, Node_List *body_decls);
void Process_Deferred_Bodies (uint32_t saved_deferred_count);
void Generate_Subprogram_Body (Syntax_Node *node);
void Generate_Generic_Instance_Body (Symbol *inst_sym, Syntax_Node *template_body);
void Emit_Task_Function_Name (Symbol *task_sym, String_Slice fallback_name);
void Generate_Task_Body (Syntax_Node *node);
void Generate_Declaration (Syntax_Node *node);
void Generate_Type_Equality_Function (Type_Info *t);
void Generate_Implicit_Operators (void);
void Generate_Exception_Globals (void);
void Generate_Extern_Declarations (Syntax_Node *node);
void Generate_Compilation_Unit (Syntax_Node *node);
void Compile_File (const char *input_path, const char *output_path);
void Derive_Output_Path (const char *input, char *out, size_t out_size);
void *Compile_Worker (void *arg);
int main (int argc, char *argv[]);

#endif /* ADA83_H */
