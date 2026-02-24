//                                                                                                  //
//                                  A D A 8 3   C O M P I L E R                                     //
//                                                                                                  //
//              An Ada 1983 Compiler Targeting LLVM Intermediate Representation                     //
//                                                                                                  //
//  Ch.  1.  Foundations         Includes, typedefs, target constants, ctype wrappers               //
//  Ch.  2.  Measurement         Bit/byte morphisms, LLVM type selection, range checks              //
//  Ch.  3.  Memory              Arena allocator for the compilation session                        //
//  Ch.  4.  Text                String slices, hashing, edit distance                              //
//  Ch.  5.  Provenance          Source locations and diagnostic reporting                          //
//  Ch.  6.  Arithmetic          Big integers, big reals, exact rationals                           //
//  Ch.  7.  Lexical Analysis    Token kinds, lexer state, scanning functions                       //
//  Ch.  8.  Syntax              Node kinds, the syntax tree, node lists                            //
//  Ch.  9.  Parsing             Recursive descent for the full Ada 83 grammar                      //
//  Ch. 10.  Types               The Ada type lattice, Type_Info, classification                    //
//  Ch. 11.  Names               Symbol table, scopes, overload resolution                          //
//  Ch. 12.  Semantics           Name resolution, type checking, constant folding                   //
//  Ch. 13.  Code Generation     LLVM IR emission for every Ada construct                           //
//  Ch. 14.  Library Management  ALI files, checksums, dependency tracking                          //
//  Ch. 15.  Elaboration         Dependency ordering for multi-unit programs                        //
//  Ch. 16.  Generics            Macro-style instantiation of generic units                         //
//  Ch. 17.  File Loading        Include-path search, source file I/O                               //
//  Ch. 18.  Vector Paths        SIMD-accelerated scanning on x86-64 and ARM64                      //
//  Ch. 19.  Driver              Command-line parsing and top-level orchestration                   //
//                                                                                                  //

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

// -----------------
// -- Foundations --
// -----------------

// -- Extended Integer Types
// Ada's numeric model demands integers wider than 64 bits; GCC/Clang __int128 gives us
// native 128-bit registers on 64-bit targets.
typedef __int128          int128_t;
typedef unsigned __int128 uint128_t;

// -- Target Data Model
// 64-bit LP64 host throughout; sizes in Type_Info are bytes, bit widths only in IR.
enum { Bits_Per_Unit = 8 };

typedef enum {
  Width_1      = 1,   Width_8     = 8,   Width_16     = 16,
  Width_32     = 32,  Width_64    = 64,  Width_128    = 128,
  Width_Ptr    = 64,  Width_Float = 32,  Width_Double = 64
} Bit_Width;

typedef enum {
  Ada_Short_Short_Integer_Bits    = Width_8,
  Ada_Short_Integer_Bits          = Width_16,
  Ada_Integer_Bits                = Width_32,
  Ada_Long_Integer_Bits           = Width_64,
  Ada_Long_Long_Integer_Bits      = Width_64,
  Ada_Long_Long_Long_Integer_Bits = Width_128
} Ada_Integer_Width;

enum {
  Default_Size_Bits   = Ada_Integer_Bits,
  Default_Size_Bytes  = Ada_Integer_Bits / Bits_Per_Unit,
  Default_Align_Bytes = Default_Size_Bytes
};

// -- Fat-Pointer Layout
// Unconstrained array parameters carry { data, bounds } -- 16 bytes on 64-bit targets.
#define FAT_PTR_TYPE         "{ ptr, ptr }"
#define FAT_PTR_ALLOC_SIZE   16
#define STRING_BOUND_TYPE    "i32"
#define STRING_BOUND_WIDTH   32
#define STRING_BOUNDS_STRUCT "{ i32, i32 }"
#define STRING_BOUNDS_ALLOC  8

// -- IEEE Floating-Point Model
#define IEEE_FLOAT_DIGITS      6
#define IEEE_DOUBLE_DIGITS     15
#define IEEE_FLOAT_MANTISSA    24
#define IEEE_DOUBLE_MANTISSA   53
#define IEEE_FLOAT_EMAX        128
#define IEEE_DOUBLE_EMAX       1024
#define IEEE_FLOAT_EMIN        (-125)
#define IEEE_DOUBLE_EMIN       (-1021)
#define IEEE_MACHINE_RADIX     2
#define IEEE_DOUBLE_MIN_NORMAL 2.2250738585072014e-308
#define IEEE_FLOAT_MIN_NORMAL  1.1754943508222875e-38
#define LOG2_OF_10             3.321928094887362

// -- Subsystem Capacities
enum { Default_Chunk_Size = 1 << 24 }; // Arena chunk: 16 MiB
#define SYMBOL_TABLE_SIZE   1024       // Hash buckets (Ch. 11)
#define MAX_INTERPRETATIONS 64         // Overload ceiling
#define ALI_VERSION         "Ada83 1.0 built " __DATE__ " " __TIME__
#define ELAB_MAX_VERTICES   512        // Elaboration graph (Ch. 15)
#define ELAB_MAX_EDGES      2048
#define ELAB_MAX_COMPONENTS 256

// -- Build-in-Place Formal Names
#define BIP_ALLOC_NAME  "__BIPalloc"
#define BIP_ACCESS_NAME "__BIPaccess"
#define BIP_MASTER_NAME "__BIPmaster"
#define BIP_CHAIN_NAME  "__BIPchain"
#define BIP_FINAL_NAME  "__BIPfinal"

// -- Code Generator Capacities
#define TEMP_TYPE_CAPACITY 4096 // Ring buffer for temp types
#define EXC_REF_CAPACITY   512  // Exception references per unit
#define MAX_AGG_DIMS       8    // Array aggregate dimensions
#define MAX_DISC_CACHE     16   // Discriminant value cache depth

// -- Runtime Check Flags
// Each bit controls a check category suppressible via pragma Suppress (RM 11.5).
#define CHK_RANGE        ((uint32_t)1)
#define CHK_OVERFLOW     ((uint32_t)2)
#define CHK_INDEX        ((uint32_t)4)
#define CHK_LENGTH       ((uint32_t)8)
#define CHK_DIVISION     ((uint32_t)16)
#define CHK_ACCESS       ((uint32_t)32)
#define CHK_DISCRIMINANT ((uint32_t)64)
#define CHK_ELABORATION  ((uint32_t)128)
#define CHK_STORAGE      ((uint32_t)256)
#define CHK_ALL          ((uint32_t)0xFFFFFFFF)

// -- Platform Detection
#if defined(__x86_64__) || defined(_M_X64)
  #define SIMD_X86_64  1
#elif defined(__aarch64__) || defined(_M_ARM64)
  #define SIMD_ARM64   1
#else
  #define SIMD_GENERIC 1
#endif

// -- Safe Character Classification
// Wrappers cast to unsigned char before calling ctype.  Id_Char_Table encodes
// Ada RM 2.3: ASCII letters, digits, underscore, Latin-1 in 0xC0..0xFF minus 0xD7, 0xF7.
int  Is_Alpha  (char ch);
int  Is_Digit  (char ch);
int  Is_Xdigit (char ch);
int  Is_Space  (char ch);
char To_Lower  (char ch);

extern const uint8_t Id_Char_Table[256];
#define Is_Id_Char(ch) (Id_Char_Table[(uint8_t)(ch)])

// -----------------
// -- Measurement --
// -----------------

// -- Bit/Byte Conversions
// To_Bits and To_Bytes form a Galois connection between the bit and byte domains:
// To_Bits is multiplicative (total, never truncates), To_Bytes is ceiling division.
// Byte_Align is their composition.
uint64_t To_Bits    (uint64_t bytes);
uint64_t To_Bytes   (uint64_t bits);
uint64_t Byte_Align (uint64_t bits);
size_t   Align_To   (size_t   size, size_t alignment);

// -- LLVM Type Selection
const char *Llvm_Int_Type            (uint32_t    bits);
const char *Llvm_Float_Type          (uint32_t    bits);
bool        Llvm_Type_Is_Pointer     (const char *llvm_type);
bool        Llvm_Type_Is_Fat_Pointer (const char *llvm_type);

// -- Range Predicates
// All bounds are int128_t / uint128_t so that the full Ada integer range is representable.
// Fits_In_Signed asks: does [lo..hi] lie within the signed range of N bits?
// Bits_For_Range returns the smallest standard width (8, 16, 32, 64, 128) that covers it.
bool     Fits_In_Signed   (int128_t  lo, int128_t hi, uint32_t bits);
bool     Fits_In_Unsigned (int128_t  lo, int128_t hi, uint32_t bits);
uint32_t Bits_For_Range   (int128_t  lo, int128_t hi);
uint32_t Bits_For_Modulus (uint128_t modulus);

// ----------
// -- Memory --
// ----------

// Bump allocator exploiting the single-lifetime invariant of compilation.
// Chunks are 16 MiB by default; oversized requests get their own chunk.
// All pointers are 16-byte aligned.
typedef struct Arena_Chunk Arena_Chunk;
struct Arena_Chunk {
  Arena_Chunk *previous; // Singly-linked list of chunks
  char        *base;     // First usable byte
  char        *current;  // Bump pointer
  char        *end;      // One past last usable byte
};

typedef struct {
  Arena_Chunk *head;
  size_t       chunk_size;
} Memory_Arena;

extern Memory_Arena Global_Arena;

void *Arena_Allocate (size_t size); // Zero-filled, 16-byte aligned
void  Arena_Free_All (void);        // Release every chunk at end of main

// --------
// -- Text --
// --------

// String_Slice: a non-owning (pointer, length) view into the source buffer or arena.
// No null terminator required.  Comparison and hashing fold to lower case (Ada RM 2.3).
typedef struct {
  const char *data;
  uint32_t    length;
} String_Slice;

#define S(lit) ((String_Slice){ .data = (lit), .length = sizeof(lit) - 1 })

extern const String_Slice Empty_Slice;

String_Slice Slice_From_Cstring      (const char  *source);
String_Slice Slice_Duplicate         (String_Slice slice);
bool         Slice_Equal             (String_Slice left,  String_Slice right);
bool         Slice_Equal_Ignore_Case (String_Slice left,  String_Slice right);
uint64_t     Slice_Hash              (String_Slice slice);                    // FNV-1a, case-folded
int          Edit_Distance           (String_Slice left,  String_Slice right); // Levenshtein

// ----------------
// -- Provenance --
// ----------------

// Every token, syntax node, and symbol carries a Source_Location.  Errors are
// accumulated; Error_Count is checked after each phase.  Fatal_Error calls exit(1).
typedef struct {
  const char *filename;
  uint32_t    line;
  uint32_t    column;
} Source_Location;

extern const Source_Location No_Location;
extern int                   Error_Count;

void Report_Error (Source_Location location, const char *format, ...);

__attribute__((noreturn))
void Fatal_Error  (Source_Location location, const char *format, ...);

// ----------------
// -- Arithmetic --
// ----------------

// Three levels of exact arithmetic for Ada numeric literals and constant folding:
//   Big_Integer  Arbitrary-precision integers as little-endian 64-bit limb arrays.
//   Big_Real     Significand (Big_Integer) paired with a power-of-ten exponent.
//   Rational     Exact quotient of two Big_Integers, always reduced by GCD.
// All storage is arena-allocated; no explicit deallocation.

// -- Big_Integer
typedef struct {
  uint64_t *limbs;       // Little-endian 64-bit digits
  uint32_t  count;       // Active limbs
  uint32_t  capacity;    // Allocated slots
  bool      is_negative; // Sign flag; magnitude always positive
} Big_Integer;

Big_Integer *Big_Integer_New             (uint32_t     capacity);
Big_Integer *Big_Integer_Clone           (const Big_Integer *source);
Big_Integer *Big_Integer_One             (void);
void         Big_Integer_Ensure_Capacity (Big_Integer *integer, uint32_t needed);
void         Big_Integer_Normalize       (Big_Integer *integer);
void         Big_Integer_Mul_Add_Small   (Big_Integer *integer,
                                          uint64_t     factor,
                                          uint64_t     addend);

int          Big_Integer_Compare         (const Big_Integer  *left,
                                          const Big_Integer  *right);
Big_Integer *Big_Integer_Add             (const Big_Integer  *left,
                                          const Big_Integer  *right);
Big_Integer *Big_Integer_Multiply        (const Big_Integer  *left,
                                          const Big_Integer  *right);
Big_Integer *Big_Integer_GCD             (const Big_Integer  *left,
                                          const Big_Integer  *right);
void         Big_Integer_Div_Rem         (const Big_Integer  *dividend,
                                          const Big_Integer  *divisor,
                                          Big_Integer       **quotient,
                                          Big_Integer       **remainder);

Big_Integer *Big_Integer_From_Decimal_SIMD (const char        *text);
bool         Big_Integer_Fits_Int64        (const Big_Integer  *integer, int64_t   *out);
bool         Big_Integer_To_Uint128        (const Big_Integer  *integer, uint128_t *out);
bool         Big_Integer_To_Int128         (const Big_Integer  *integer, int128_t  *out);

// -- Big_Real
typedef struct {
  Big_Integer *significand;
  int32_t      exponent;
} Big_Real;

Big_Real *Big_Real_New         (void);
Big_Real *Big_Real_From_String (const char    *text);
double    Big_Real_To_Double   (const Big_Real *real);
bool      Big_Real_Fits_Double (const Big_Real *real);
int       Big_Real_Compare     (const Big_Real *left,     const Big_Real *right);
Big_Real *Big_Real_Scale       (const Big_Real *real,     int32_t         scale);
Big_Real *Big_Real_Divide_Int  (const Big_Real *dividend, int64_t         divisor);
void      Big_Real_To_Hex      (const Big_Real *real,
                                char           *buffer,
                                size_t          buffer_size);
Big_Real *Big_Real_Add_Sub     (const Big_Real *left,
                                const Big_Real *right,
                                bool            subtract);
Big_Real *Big_Real_Multiply    (const Big_Real *left,
                                const Big_Real *right);

// -- Rational
typedef struct {
  Big_Integer *numerator;
  Big_Integer *denominator;
} Rational;

Rational Rational_Reduce        (Big_Integer    *numer, Big_Integer    *denom);
Rational Rational_From_Big_Real (const Big_Real *real);
Rational Rational_From_Int      (int64_t         value);
Rational Rational_Add           (Rational         left, Rational        right);
Rational Rational_Sub           (Rational         left, Rational        right);
Rational Rational_Mul           (Rational         left, Rational        right);
Rational Rational_Div           (Rational         left, Rational        right);
Rational Rational_Pow           (Rational         base, int             exponent);
int      Rational_Compare       (Rational         left, Rational        right);
double   Rational_To_Double     (Rational         rational);

// ----------------------
// -- Lexical Analysis --
// ----------------------

// -- Token Kinds
// Token_Kind enumerates every lexeme in the Ada 83 grammar: identifiers, numeric and
// string literals, delimiters, operator symbols, and the sixty-three reserved words of RM 2.9.
typedef enum {
  TK_EOF = 0,
  TK_ERROR,

  // Literals
  TK_IDENTIFIER, TK_INTEGER,   TK_REAL,      TK_CHARACTER, TK_STRING,

  // Delimiters
  TK_LPAREN,     TK_RPAREN,    TK_LBRACKET,  TK_RBRACKET,
  TK_COMMA,      TK_DOT,       TK_SEMICOLON, TK_COLON,     TK_TICK,

  // Compound delimiters
  TK_ASSIGN,     TK_ARROW,     TK_DOTDOT,    TK_LSHIFT,
  TK_RSHIFT,     TK_BOX,       TK_BAR,

  // Operators
  TK_EQ,         TK_NE,        TK_LT,        TK_LE,
  TK_GT,         TK_GE,        TK_PLUS,      TK_MINUS,
  TK_STAR,       TK_SLASH,     TK_AMPERSAND, TK_EXPON,

  // Reserved words -- the sixty-three of Ada 83
  TK_ABORT,     TK_ABS,       TK_ACCEPT,    TK_ACCESS,    TK_ALL,
  TK_AND,       TK_AND_THEN,  TK_ARRAY,     TK_AT,        TK_BEGIN,
  TK_BODY,      TK_CASE,      TK_CONSTANT,  TK_DECLARE,   TK_DELAY,
  TK_DELTA,     TK_DIGITS,    TK_DO,        TK_ELSE,      TK_ELSIF,
  TK_END,       TK_ENTRY,     TK_EXCEPTION, TK_EXIT,      TK_FOR,
  TK_FUNCTION,  TK_GENERIC,   TK_GOTO,      TK_IF,        TK_IN,
  TK_IS,        TK_LIMITED,   TK_LOOP,      TK_MOD,       TK_NEW,
  TK_NOT,       TK_NULL,      TK_OF,        TK_OR,        TK_OR_ELSE,
  TK_OTHERS,    TK_OUT,       TK_PACKAGE,   TK_PRAGMA,    TK_PRIVATE,
  TK_PROCEDURE, TK_RAISE,     TK_RANGE,     TK_RECORD,    TK_REM,
  TK_RENAMES,   TK_RETURN,    TK_REVERSE,   TK_SELECT,    TK_SEPARATE,
  TK_SUBTYPE,   TK_TASK,      TK_TERMINATE, TK_THEN,      TK_TYPE,
  TK_USE,       TK_WHEN,      TK_WHILE,     TK_WITH,      TK_XOR,

  TK_COUNT
} Token_Kind;

extern const char *Token_Name[TK_COUNT];

Token_Kind Lookup_Keyword (String_Slice name);

// -- Token Record
typedef struct {
  Token_Kind      kind;
  Source_Location location;
  String_Slice    text;
  union { int64_t      integer_value; double    float_value; };
  union { Big_Integer *big_integer;   Big_Real *big_real;    };
} Token;

Token Make_Token (Token_Kind      kind,
                  Source_Location location,
                  String_Slice    text);

// -- Lexer State
typedef struct {
  const char *source_start;
  const char *current;
  const char *source_end;
  const char *filename;
  uint32_t    line;
  uint32_t    column;
  Token_Kind  prev_token_kind; // Disambiguates tick vs. attribute
} Lexer;

Lexer Lexer_New     (const char *source, size_t length, const char *filename);
char  Lexer_Peek    (const Lexer *lex, size_t offset);
char  Lexer_Advance (Lexer *lex);
void  Lexer_Skip_Whitespace_And_Comments (Lexer *lex);
Token Lexer_Next_Token                   (Lexer *lex);

Token Scan_Identifier        (Lexer *lex);
Token Scan_Number            (Lexer *lex);
Token Scan_Character_Literal (Lexer *lex);
Token Scan_String_Literal    (Lexer *lex);
int   Digit_Value            (char   ch);

// ----------
// -- Syntax --
// ----------

// The abstract syntax tree is the central data structure.  Every syntactic construct
// maps to a Node_Kind; the Syntax_Node record carries kind, location, type annotation,
// symbol link, and a payload union discriminated by the kind tag.

// -- Forward Declarations
typedef struct Syntax_Node Syntax_Node;
typedef struct Type_Info   Type_Info;
typedef struct Symbol      Symbol;
typedef struct Scope       Scope;

// -- Node List
typedef struct {
  Syntax_Node **items;
  uint32_t      count;
  uint32_t      capacity;
} Node_List;

void Node_List_Push (Node_List *list, Syntax_Node *node);

// -- Node Kinds
typedef enum {

  // Literals and primaries
  NK_INTEGER,          NK_REAL,             NK_STRING,           NK_CHARACTER,
  NK_NULL,             NK_OTHERS,           NK_IDENTIFIER,       NK_SELECTED,
  NK_ATTRIBUTE,        NK_QUALIFIED,

  // Expressions
  NK_BINARY_OP,        NK_UNARY_OP,         NK_AGGREGATE,        NK_ALLOCATOR,
  NK_APPLY,            // Unified: call, index, slice, conversion
  NK_RANGE,
  NK_ASSOCIATION,      // name => value

  // Type definitions
  NK_SUBTYPE_INDICATION,     NK_RANGE_CONSTRAINT,
  NK_INDEX_CONSTRAINT,       NK_DISCRIMINANT_CONSTRAINT,
  NK_DIGITS_CONSTRAINT,      NK_DELTA_CONSTRAINT,
  NK_ARRAY_TYPE,       NK_RECORD_TYPE,      NK_ACCESS_TYPE,      NK_DERIVED_TYPE,
  NK_ENUMERATION_TYPE, NK_INTEGER_TYPE,     NK_REAL_TYPE,
  NK_COMPONENT_DECL,   NK_VARIANT_PART,     NK_VARIANT,
  NK_DISCRIMINANT_SPEC,

  // Statements
  NK_ASSIGNMENT,       NK_CALL_STMT,        NK_RETURN,           NK_IF,         NK_CASE,
  NK_LOOP,             NK_BLOCK,            NK_EXIT,             NK_GOTO,       NK_RAISE,
  NK_NULL_STMT,        NK_LABEL,            NK_ACCEPT,           NK_SELECT,
  NK_DELAY,            NK_ABORT,            NK_CODE,

  // Declarations
  NK_OBJECT_DECL,      NK_TYPE_DECL,        NK_SUBTYPE_DECL,     NK_EXCEPTION_DECL,
  NK_PROCEDURE_SPEC,   NK_FUNCTION_SPEC,
  NK_PROCEDURE_BODY,   NK_FUNCTION_BODY,
  NK_PACKAGE_SPEC,     NK_PACKAGE_BODY,     NK_TASK_SPEC,        NK_TASK_BODY,
  NK_ENTRY_DECL,       NK_SUBPROGRAM_RENAMING,
  NK_PACKAGE_RENAMING, NK_EXCEPTION_RENAMING,
  NK_GENERIC_DECL,     NK_GENERIC_INST,
  NK_PARAM_SPEC,       NK_USE_CLAUSE,       NK_WITH_CLAUSE,      NK_PRAGMA,
  NK_REPRESENTATION_CLAUSE, NK_EXCEPTION_HANDLER,
  NK_CONTEXT_CLAUSE,        NK_COMPILATION_UNIT,

  // Generic formals
  NK_GENERIC_TYPE_PARAM,     NK_GENERIC_OBJECT_PARAM,
  NK_GENERIC_SUBPROGRAM_PARAM,

  NK_COUNT
} Node_Kind;

// -- Syntax Node Record
// The payload union is discriminated by `kind'.  Each variant is written as in an Ada
// variant record: one field per line, with a trailing comment describing its role.
struct Syntax_Node {
  Node_Kind       kind;     // Discriminant tag for the payload union
  Source_Location location; // Where this construct appeared in source
  Type_Info      *type;     // Set by semantic analysis, NULL before
  Symbol         *symbol;   // Set by name resolution, NULL before

  union {

    //  when NK_INTEGER =>
    struct {
      int64_t      value;     // Machine-width literal value
      Big_Integer *big_value; // Arbitrary-precision overflow
    } integer_lit;

    //  when NK_REAL =>
    struct {
      double    value;     // Machine-width float value
      Big_Real *big_value; // Arbitrary-precision overflow
    } real_lit;

    //  when NK_STRING | NK_CHARACTER | NK_IDENTIFIER =>
    struct {
      String_Slice text; // Raw source text
    } string_val;

    //  when NK_SELECTED =>
    struct {
      Syntax_Node *prefix;   // The dotted prefix expression
      String_Slice selector; // The selected component name
    } selected;

    //  when NK_ATTRIBUTE =>
    struct {
      Syntax_Node *prefix;    // The prefix before the tick
      String_Slice name;      // Attribute designator
      Node_List    arguments; // Optional attribute arguments
    } attribute;

    //  when NK_QUALIFIED =>
    struct {
      Syntax_Node *subtype_mark; // Qualifying subtype
      Syntax_Node *expression;   // Qualified expression
    } qualified;

    //  when NK_BINARY_OP =>
    struct {
      Token_Kind   op;    // Operator token kind
      Syntax_Node *left;  // Left operand
      Syntax_Node *right; // Right operand
    } binary;

    //  when NK_UNARY_OP =>
    struct {
      Token_Kind   op;      // Operator token kind
      Syntax_Node *operand; // Sole operand
    } unary;

    //  when NK_AGGREGATE =>
    struct {
      Node_List items;            // Component associations
      bool      is_named;         // True if using named notation
      bool      is_parenthesized; // True if parenthesized
    } aggregate;

    //  when NK_ALLOCATOR =>
    struct {
      Syntax_Node *subtype_mark; // Allocated subtype
      Syntax_Node *expression;   // Initializer or NULL
    } allocator;

    //  when NK_APPLY =>
    struct {
      Syntax_Node *prefix;    // Called / indexed / sliced / converted name
      Node_List    arguments; // Actual parameter list
    } apply;

    //  when NK_RANGE =>
    struct {
      Syntax_Node *low;  // Low bound expression
      Syntax_Node *high; // High bound expression
    } range;

    //  when NK_ASSOCIATION =>
    struct {
      Node_List    choices;    // Discrete choices or names
      Syntax_Node *expression; // Associated value
    } association;

    //  when NK_SUBTYPE_INDICATION =>
    struct {
      Syntax_Node *subtype_mark; // Named subtype
      Syntax_Node *constraint;   // Optional constraint or NULL
    } subtype_ind;

    //  when NK_INDEX_CONSTRAINT =>
    struct {
      Node_List ranges; // Discrete range per dimension
    } index_constraint;

    //  when NK_RANGE_CONSTRAINT =>
    struct {
      Syntax_Node *range; // The constraining range
    } range_constraint;

    //  when NK_DISCRIMINANT_CONSTRAINT =>
    struct {
      Node_List associations; // Discriminant value associations
    } discriminant_constraint;

    //  when NK_DIGITS_CONSTRAINT =>
    struct {
      Syntax_Node *digits_expr; // Digits expression
      Syntax_Node *range;       // Optional range or NULL
    } digits_constraint;

    //  when NK_DELTA_CONSTRAINT =>
    struct {
      Syntax_Node *delta_expr; // Delta expression
      Syntax_Node *range;      // Optional range or NULL
    } delta_constraint;

    //  when NK_ARRAY_TYPE =>
    struct {
      Node_List    indices;        // Index subtype definitions
      Syntax_Node *component_type; // Element subtype indication
      bool         is_constrained; // True for constrained arrays
    } array_type;

    //  when NK_RECORD_TYPE =>
    struct {
      Node_List    discriminants; // Discriminant specifications
      Node_List    components;    // Component declarations
      Syntax_Node *variant_part;  // Variant part or NULL
      bool         is_null;       // True for null record
    } record_type;

    //  when NK_ACCESS_TYPE =>
    struct {
      Syntax_Node *designated;  // Designated subtype
      bool         is_constant; // Access-to-constant
    } access_type;

    //  when NK_DERIVED_TYPE =>
    struct {
      Syntax_Node *parent_type; // Parent subtype indication
      Syntax_Node *constraint;  // Optional constraint or NULL
    } derived_type;

    //  when NK_ENUMERATION_TYPE =>
    struct {
      Node_List literals; // Enumeration literal names
    } enum_type;

    //  when NK_INTEGER_TYPE =>
    struct {
      Syntax_Node *range;      // Range constraint
      uint128_t    modulus;    // Modular type modulus
      bool         is_modular; // True for mod types
    } integer_type;

    //  when NK_REAL_TYPE =>
    struct {
      Syntax_Node *precision; // Digits or delta expression
      Syntax_Node *range;     // Optional range or NULL
      Syntax_Node *delta;     // Delta for fixed-point
    } real_type;

    //  when NK_COMPONENT_DECL =>
    struct {
      Node_List    names;          // Defining identifiers
      Syntax_Node *component_type; // Component subtype indication
      Syntax_Node *init;           // Default expression or NULL
    } component;

    //  when NK_VARIANT_PART =>
    struct {
      String_Slice discriminant; // Discriminant name
      Node_List    variants;     // Variant alternatives
    } variant_part;

    //  when NK_VARIANT =>
    struct {
      Node_List    choices;      // Discrete choice list
      Node_List    components;   // Component declarations
      Syntax_Node *variant_part; // Nested variant part or NULL
    } variant;

    //  when NK_DISCRIMINANT_SPEC =>
    struct {
      Node_List    names;        // Discriminant names
      Syntax_Node *disc_type;    // Discriminant subtype
      Syntax_Node *default_expr; // Default value or NULL
    } discriminant;

    //  when NK_ASSIGNMENT =>
    struct {
      Syntax_Node *target; // Left-hand side name
      Syntax_Node *value;  // Right-hand side expression
    } assignment;

    //  when NK_RETURN =>
    struct {
      Syntax_Node *expression; // Return value or NULL
    } return_stmt;

    //  when NK_IF =>
    struct {
      Syntax_Node *condition;   // Boolean condition
      Node_List    then_stmts;  // Then-part statements
      Node_List    elsif_parts; // Elsif clauses
      Node_List    else_stmts;  // Else-part statements
    } if_stmt;

    //  when NK_CASE =>
    struct {
      Syntax_Node *expression;   // Selecting expression
      Node_List    alternatives; // Case alternatives
    } case_stmt;

    //  when NK_LOOP =>
    struct {
      String_Slice  label;            // Optional loop label
      Symbol       *label_symbol;     // Resolved label symbol
      Syntax_Node  *iteration_scheme; // For/while or NULL
      Node_List     statements;       // Loop body
      bool          is_reverse;       // True for reverse iteration
    } loop_stmt;

    //  when NK_BLOCK =>
    struct {
      String_Slice label;        // Optional block label
      Symbol      *label_symbol; // Resolved label symbol
      Node_List    declarations; // Declarative part
      Node_List    statements;   // Statement sequence
      Node_List    handlers;     // Exception handlers
    } block_stmt;

    //  when NK_EXIT =>
    struct {
      String_Slice  loop_name; // Target loop name or empty
      Syntax_Node  *condition; // When condition or NULL
      Symbol       *target;    // Resolved loop symbol
    } exit_stmt;

    //  when NK_GOTO =>
    struct {
      String_Slice name;   // Target label name
      Symbol      *target; // Resolved label symbol
    } goto_stmt;

    //  when NK_LABEL =>
    struct {
      String_Slice  name;      // Label identifier
      Syntax_Node  *statement; // Labelled statement
      Symbol       *symbol;    // Label symbol
    } label_node;

    //  when NK_RAISE =>
    struct {
      Syntax_Node *exception_name; // Exception name or NULL for reraise
    } raise_stmt;

    //  when NK_ACCEPT =>
    struct {
      String_Slice  entry_name; // Accepted entry name
      Syntax_Node  *index;      // Entry family index or NULL
      Node_List     parameters; // Formal parameters
      Node_List     statements; // Accept body
      Symbol       *entry_sym;  // Resolved entry symbol
    } accept_stmt;

    //  when NK_SELECT =>
    struct {
      Node_List    alternatives; // Select alternatives
      Syntax_Node *else_part;    // Else part or NULL
    } select_stmt;

    //  when NK_DELAY =>
    struct {
      Syntax_Node *expression; // Duration expression
    } delay_stmt;

    //  when NK_ABORT =>
    struct {
      Node_List task_names; // Task objects to abort
    } abort_stmt;

    //  when NK_OBJECT_DECL =>
    struct {
      Node_List    names;       // Defining identifiers
      Syntax_Node *object_type; // Object subtype indication
      Syntax_Node *init;        // Initial value or NULL
      bool         is_constant; // True for constant declarations
      bool         is_aliased;  // True for aliased objects
      bool         is_rename;   // True for renaming declarations
    } object_decl;

    //  when NK_TYPE_DECL =>
    struct {
      String_Slice  name;          // Type name
      Node_List     discriminants; // Known discriminant part
      Syntax_Node  *definition;    // Type definition
      bool          is_limited;    // Limited type
      bool          is_private;    // Private type
    } type_decl;

    //  when NK_EXCEPTION_DECL =>
    struct {
      Node_List    names;   // Exception identifiers
      Syntax_Node *renamed; // Renamed exception or NULL
    } exception_decl;

    //  when NK_PROCEDURE_SPEC | NK_FUNCTION_SPEC =>
    struct {
      String_Slice  name;        // Subprogram name
      Node_List     parameters;  // Formal parameter list
      Syntax_Node  *return_type; // Return type or NULL
      Syntax_Node  *renamed;     // Renamed entity or NULL
    } subprogram_spec;

    //  when NK_PROCEDURE_BODY | NK_FUNCTION_BODY =>
    struct {
      Syntax_Node *specification;  // The subprogram spec
      Node_List    declarations;   // Declarative part
      Node_List    statements;     // Body statements
      Node_List    handlers;       // Exception handlers
      bool         is_separate;    // Is a subunit stub
      bool         code_generated; // Already emitted
    } subprogram_body;

    //  when NK_PACKAGE_SPEC =>
    struct {
      String_Slice name;          // Package name
      Node_List    visible_decls; // Visible part declarations
      Node_List    private_decls; // Private part declarations
    } package_spec;

    //  when NK_PACKAGE_BODY =>
    struct {
      String_Slice name;         // Package name
      Node_List    declarations; // Body declarations
      Node_List    statements;   // Body statements
      Node_List    handlers;     // Exception handlers
      bool         is_separate;  // Is a subunit stub
    } package_body;

    //  when NK_PACKAGE_RENAMING =>
    struct {
      String_Slice  new_name; // New package name
      Syntax_Node  *old_name; // Renamed package name
    } package_renaming;

    //  when NK_TASK_SPEC =>
    struct {
      String_Slice name;    // Task name
      Node_List    entries; // Entry declarations
      bool         is_type; // True for task type
    } task_spec;

    //  when NK_TASK_BODY =>
    struct {
      String_Slice name;         // Task name
      Node_List    declarations; // Body declarations
      Node_List    statements;   // Body statements
      Node_List    handlers;     // Exception handlers
      bool         is_separate;  // Is a subunit stub
    } task_body;

    //  when NK_ENTRY_DECL =>
    struct {
      String_Slice name;              // Entry name
      Node_List    parameters;        // Formal parameters
      Node_List    index_constraints; // Family index constraints
    } entry_decl;

    //  when NK_PARAM_SPEC =>
    struct {
      Node_List    names;        // Parameter identifiers
      Syntax_Node *param_type;   // Parameter subtype indication
      Syntax_Node *default_expr; // Default value or NULL
      enum { MODE_IN, MODE_OUT, MODE_IN_OUT } mode;
    } param_spec;

    //  when NK_GENERIC_DECL =>
    struct {
      Node_List    formals; // Generic formal parameters
      Syntax_Node *unit;    // The generic unit declaration
    } generic_decl;

    //  when NK_GENERIC_INST =>
    struct {
      Syntax_Node *generic_name;  // Name of generic template
      Node_List    actuals;       // Actual parameter list
      String_Slice instance_name; // Instance defining name
      Token_Kind   unit_kind;     // TK_PACKAGE / TK_PROCEDURE / TK_FUNCTION
    } generic_inst;

    //  when NK_GENERIC_TYPE_PARAM =>
    struct {
      String_Slice name;
      enum {
        GEN_DEF_PRIVATE = 0, GEN_DEF_LIMITED_PRIVATE,
        GEN_DEF_DISCRETE,    GEN_DEF_INTEGER,
        GEN_DEF_FLOAT,       GEN_DEF_FIXED,
        GEN_DEF_ARRAY,       GEN_DEF_ACCESS,
        GEN_DEF_DERIVED
      } def_kind;                 // Form of the generic formal type
      Syntax_Node *def_detail;    // Type definition detail
      Node_List    discriminants; // Discriminant part
    } generic_type_param;

    //  when NK_GENERIC_OBJECT_PARAM =>
    struct {
      Node_List    names;        // Object parameter names
      Syntax_Node *object_type;  // Object subtype indication
      Syntax_Node *default_expr; // Default value or NULL
      enum { GEN_MODE_IN = 0, GEN_MODE_OUT, GEN_MODE_IN_OUT } mode;
    } generic_object_param;

    //  when NK_GENERIC_SUBPROGRAM_PARAM =>
    struct {
      String_Slice  name;         // Formal subprogram name
      Node_List     parameters;   // Formal parameters
      Syntax_Node  *return_type;  // Return type or NULL
      Syntax_Node  *default_name; // Default subprogram or NULL
      bool          is_function;  // True for generic function
      bool          default_box;  // True for <>
    } generic_subprog_param;

    //  when NK_USE_CLAUSE =>
    struct {
      Node_List names; // Used package names
    } use_clause;

    //  when NK_PRAGMA =>
    struct {
      String_Slice name;      // Pragma identifier
      Node_List    arguments; // Pragma arguments
    } pragma_node;

    //  when NK_EXCEPTION_HANDLER =>
    struct {
      Node_List exceptions; // Handled exception choices
      Node_List statements; // Handler statements
    } handler;

    //  when NK_REPRESENTATION_CLAUSE =>
    struct {
      Syntax_Node *entity_name;       // Entity being represented
      String_Slice attribute;         // Representation attribute
      Syntax_Node *expression;        // Representation expression
      Node_List    component_clauses; // Record rep component clauses
      bool         is_record_rep;     // True for record rep clause
      bool         is_enum_rep;       // True for enum rep clause
    } rep_clause;

    //  when NK_CONTEXT_CLAUSE =>
    struct {
      Node_List with_clauses; // With clauses
      Node_List use_clauses;  // Use clauses
    } context;

    //  when NK_COMPILATION_UNIT =>
    struct {
      Syntax_Node *context;         // Context clause
      Syntax_Node *unit;            // The library unit
      Syntax_Node *separate_parent; // Separate parent or NULL
    } compilation_unit;

  };
};

Syntax_Node *Node_New (Node_Kind kind, Source_Location location);

// -----------
// -- Parsing --
// -----------

// Recursive descent mirrors the grammar.  Three simplifying principles:
//   1. All X(...) forms parse as NK_APPLY; semantic analysis disambiguates later.
//   2. One helper handles positional, named, and choice associations.
//   3. One postfix loop handles .selector, 'attribute, and (args).

typedef struct {
  Lexer      lexer;
  Token      current_token;
  Token      previous_token;
  bool       had_error;
  bool       panic_mode;
  uint32_t   last_line;
  uint32_t   last_column;
  Token_Kind last_kind;
} Parser;

typedef enum {
  PREC_NONE = 0,  PREC_LOGICAL,        PREC_RELATIONAL,
  PREC_ADDITIVE,  PREC_MULTIPLICATIVE, PREC_EXPONENTIAL,
  PREC_UNARY,     PREC_PRIMARY
} Precedence;

Parser          Parser_New              (const char *source, size_t length, const char *filename);
bool            Parser_At               (Parser *p, Token_Kind kind);
bool            Parser_At_Any           (Parser *p, Token_Kind k1, Token_Kind k2);
bool            Parser_Peek_At          (Parser *p, Token_Kind kind);
Token           Parser_Advance          (Parser *p);
bool            Parser_Match            (Parser *p, Token_Kind kind);
bool            Parser_Expect           (Parser *p, Token_Kind kind);
Source_Location Parser_Location         (Parser *p);
String_Slice    Parser_Identifier       (Parser *p);
void            Parser_Error            (Parser *p, const char *message);
void            Parser_Error_At_Current (Parser *p, const char *expected);
void            Parser_Synchronize      (Parser *p);
bool            Parser_Check_Progress   (Parser *p);
void            Parser_Check_End_Name   (Parser *p, String_Slice expected);
Precedence      Get_Infix_Precedence    (Token_Kind kind);
bool            Is_Right_Associative    (Token_Kind kind);

Syntax_Node *Parse_Expression            (Parser *p);
Syntax_Node *Parse_Expression_Precedence (Parser *p, Precedence min_prec);
Syntax_Node *Parse_Unary                 (Parser *p);
Syntax_Node *Parse_Primary               (Parser *p);
Syntax_Node *Parse_Name                  (Parser *p);
Syntax_Node *Parse_Simple_Name           (Parser *p);
Syntax_Node *Parse_Choice                (Parser *p);
Syntax_Node *Parse_Range                 (Parser *p);
void         Parse_Association_List      (Parser *p, Node_List *list);
void         Parse_Parameter_List        (Parser *p, Node_List *params);

Syntax_Node *Parse_Subtype_Indication (Parser *p);
Syntax_Node *Parse_Type_Definition    (Parser *p);
Syntax_Node *Parse_Enumeration_Type   (Parser *p);
Syntax_Node *Parse_Array_Type         (Parser *p);
Syntax_Node *Parse_Record_Type        (Parser *p);
Syntax_Node *Parse_Access_Type        (Parser *p);
Syntax_Node *Parse_Derived_Type       (Parser *p);
Syntax_Node *Parse_Discrete_Range     (Parser *p);
Syntax_Node *Parse_Variant_Part       (Parser *p);
Syntax_Node *Parse_Discriminant_Part  (Parser *p);

Syntax_Node *Parse_Statement               (Parser *p);
void         Parse_Statement_Sequence      (Parser *p, Node_List *list);
Syntax_Node *Parse_Assignment_Or_Call      (Parser *p);
Syntax_Node *Parse_Return_Statement        (Parser *p);
Syntax_Node *Parse_If_Statement            (Parser *p);
Syntax_Node *Parse_Case_Statement          (Parser *p);
Syntax_Node *Parse_Loop_Statement          (Parser *p, String_Slice label);
Syntax_Node *Parse_Block_Statement         (Parser *p, String_Slice label);
Syntax_Node *Parse_Exit_Statement          (Parser *p);
Syntax_Node *Parse_Goto_Statement          (Parser *p);
Syntax_Node *Parse_Raise_Statement         (Parser *p);
Syntax_Node *Parse_Delay_Statement         (Parser *p);
Syntax_Node *Parse_Abort_Statement         (Parser *p);
Syntax_Node *Parse_Accept_Statement        (Parser *p);
Syntax_Node *Parse_Select_Statement        (Parser *p);
Syntax_Node *Parse_Pragma                  (Parser *p);

Syntax_Node *Parse_Declaration             (Parser *p);
void         Parse_Declarative_Part        (Parser *p, Node_List *list);
Syntax_Node *Parse_Object_Declaration      (Parser *p);
Syntax_Node *Parse_Type_Declaration        (Parser *p);
Syntax_Node *Parse_Subtype_Declaration     (Parser *p);
Syntax_Node *Parse_Procedure_Specification (Parser *p);
Syntax_Node *Parse_Function_Specification  (Parser *p);
Syntax_Node *Parse_Subprogram_Body         (Parser *p, Syntax_Node *spec);
Syntax_Node *Parse_Package_Specification   (Parser *p);
Syntax_Node *Parse_Package_Body            (Parser *p);
void         Parse_Generic_Formal_Part     (Parser *p, Node_List *formals);
Syntax_Node *Parse_Generic_Declaration     (Parser *p);
Syntax_Node *Parse_Use_Clause              (Parser *p);
Syntax_Node *Parse_With_Clause             (Parser *p);
Syntax_Node *Parse_Representation_Clause   (Parser *p);
Syntax_Node *Parse_Context_Clause          (Parser *p);
Syntax_Node *Parse_Compilation_Unit        (Parser *p);

// ---------
// -- Types --
// ---------

// Ada's types form a lattice rooted at the universal types.  Every type in the program
// is represented by a Type_Info descriptor carrying kind, scalar bounds, composite
// structure, representation size, and derivation chains.  Sizes in Type_Info are BYTES.

typedef enum {
  TYPE_UNKNOWN = 0,
  TYPE_BOOLEAN,          TYPE_CHARACTER,       TYPE_INTEGER,
  TYPE_MODULAR,          TYPE_ENUMERATION,     TYPE_FLOAT,
  TYPE_FIXED,            TYPE_ARRAY,           TYPE_RECORD,
  TYPE_STRING,           TYPE_ACCESS,          TYPE_UNIVERSAL_INTEGER,
  TYPE_UNIVERSAL_REAL,   TYPE_TASK,            TYPE_SUBPROGRAM,
  TYPE_PRIVATE,          TYPE_LIMITED_PRIVATE, TYPE_INCOMPLETE,
  TYPE_PACKAGE,
  TYPE_COUNT
} Type_Kind;

typedef struct {
  enum { BOUND_NONE, BOUND_INTEGER, BOUND_FLOAT, BOUND_EXPR } kind;
  union { int128_t int_value; double float_value; Syntax_Node *expr; };
  uint32_t cached_temp; // LLVM temp register, 0 if not yet emitted
} Type_Bound;

typedef struct {
  int64_t  disc_value_low;
  int64_t  disc_value_high;
  bool     is_others;
  uint32_t first_component;
  uint32_t component_count;
  uint32_t variant_size;
} Variant_Info;

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

typedef struct {
  Type_Info *index_type;
  Type_Bound low_bound;
  Type_Bound high_bound;
} Index_Info;

struct Type_Info {
  Type_Kind    kind;
  String_Slice name;
  Symbol      *defining_symbol;
  uint32_t     size;               // Bytes
  uint32_t     alignment;          // Bytes
  uint32_t     specified_bit_size; // From 'Size rep clause, or 0
  Type_Bound   low_bound;
  Type_Bound   high_bound;
  uint128_t    modulus;            // TYPE_MODULAR only
  Type_Info   *base_type;
  Type_Info   *parent_type;

  union {

    //  array
    struct {
      Index_Info *indices;
      uint32_t    index_count;
      Type_Info  *element_type;
      bool        is_constrained;
    } array;

    //  record
    struct {
      Component_Info *components;
      uint32_t        component_count;
      uint32_t        discriminant_count;
      bool            has_discriminants;
      bool            all_defaults;
      bool            is_constrained;
      Variant_Info   *variants;
      uint32_t        variant_count;
      uint32_t        variant_offset;
      uint32_t        max_variant_size;
      Syntax_Node    *variant_part_node;
      int64_t        *disc_constraint_values;
      Syntax_Node   **disc_constraint_exprs;
      uint32_t       *disc_constraint_preeval;
      bool            has_disc_constraints;
    } record;

    //  access
    struct {
      Type_Info *designated_type;
      bool       is_access_constant;
    } access;

    //  enumeration
    struct {
      String_Slice *literals;
      uint32_t      literal_count;
      int64_t      *rep_values;
    } enumeration;

    struct { double delta; double small; int scale; } fixed;
    struct { int digits; }                            flt;
  };

  uint32_t    suppressed_checks;
  bool        is_packed;
  bool        is_limited;
  bool        is_frozen;
  int64_t     storage_size;
  const char *equality_func_name;
  uint32_t    rt_global_id;
};

extern Type_Info *Frozen_Composite_Types[256];
extern uint32_t   Frozen_Composite_Count;
extern Symbol    *Exception_Symbols[256];
extern uint32_t   Exception_Symbol_Count;

Type_Info *Type_New    (Type_Kind kind, String_Slice name);
void       Freeze_Type (Type_Info *type_info);

bool Type_Is_Scalar               (const Type_Info *t);
bool Type_Is_Discrete             (const Type_Info *t);
bool Type_Is_Numeric              (const Type_Info *t);
bool Type_Is_Real                 (const Type_Info *t);
bool Type_Is_Float_Representation (const Type_Info *t);
bool Type_Is_Array_Like           (const Type_Info *t);
bool Type_Is_Composite            (const Type_Info *t);
bool Type_Is_Access               (const Type_Info *t);
bool Type_Is_Record               (const Type_Info *t);
bool Type_Is_Task                 (const Type_Info *t);
bool Type_Is_Float                (const Type_Info *t);
bool Type_Is_Fixed_Point          (const Type_Info *t);
bool Type_Is_Private              (const Type_Info *t);
bool Type_Is_Limited              (const Type_Info *t);
bool Type_Is_Integer_Like         (const Type_Info *t);
bool Type_Is_Unsigned             (const Type_Info *t);
bool Type_Is_Enumeration          (const Type_Info *t);
bool Type_Is_Boolean              (const Type_Info *t);
bool Type_Is_Character            (const Type_Info *t);
bool Type_Is_String               (const Type_Info *t);
bool Type_Is_Universal_Integer    (const Type_Info *t);
bool Type_Is_Universal_Real       (const Type_Info *t);
bool Type_Is_Universal            (const Type_Info *t);
bool Type_Is_Unconstrained_Array  (const Type_Info *t);
bool Type_Is_Constrained_Array    (const Type_Info *t);
bool Type_Is_Bool_Array           (const Type_Info *t);
bool Type_Has_Dynamic_Bounds      (const Type_Info *t);
bool Type_Needs_Fat_Pointer       (const Type_Info *t);
bool Type_Needs_Fat_Pointer_Load  (const Type_Info *t);

Type_Info *Type_Base (Type_Info *t);
Type_Info *Type_Root (Type_Info *t);

int128_t Type_Bound_Value                 (Type_Bound  bound);
bool     Type_Bound_Is_Compile_Time_Known (Type_Bound  b);
bool     Type_Bound_Is_Set                (Type_Bound  b);
double   Type_Bound_Float_Value           (Type_Bound  b);
int128_t Array_Element_Count              (Type_Info  *t);
int128_t Array_Low_Bound                  (Type_Info  *t);

const char *Type_To_Llvm           (Type_Info       *t);
const char *Type_To_Llvm_Sig       (Type_Info       *t);
const char *Array_Bound_Llvm_Type  (const Type_Info *t);
const char *Bounds_Type_For        (const char      *bound_type);
const char *Float_Llvm_Type_Of     (const Type_Info *t);
int         Bounds_Alloc_Size      (const char      *bound_type);
bool        Float_Is_Single        (const Type_Info *t);
int         Float_Effective_Digits (const Type_Info *t);

void Float_Model_Parameters (const Type_Info *t,
                              int64_t         *out_mantissa,
                              int64_t         *out_emax);

bool        Expression_Is_Slice             (const Syntax_Node *node);
bool        Expression_Is_Boolean           (Syntax_Node       *node);
bool        Expression_Is_Float             (Syntax_Node       *node);
const char *Expression_Llvm_Type            (Syntax_Node       *node);
bool        Expression_Produces_Fat_Pointer (const Syntax_Node *node,
                                             const Type_Info   *type);

bool Types_Same_Named           (Type_Info *t1,   Type_Info *t2);
bool Subprogram_Is_Primitive_Of (Symbol    *sub,  Type_Info *type);
void Create_Derived_Operation   (Symbol    *sub,
                                 Type_Info *derived_type,
                                 Type_Info *parent_type,
                                 Symbol    *type_sym);
void Derive_Subprograms         (Type_Info *derived_type,
                                 Type_Info *parent_type,
                                 Symbol    *type_sym);

typedef struct {
  uint32_t count;
  struct {
    String_Slice formal_name;
    Type_Info   *actual_type;
  } mappings[32];
} Generic_Type_Map;

extern Generic_Type_Map g_generic_type_map;

void       Set_Generic_Type_Map        (Symbol    *inst);
Type_Info *Resolve_Generic_Actual_Type (Type_Info *type);
bool       Check_Is_Suppressed         (Type_Info *type, Symbol *sym, uint32_t check_bit);

// ---------
// -- Names --
// ---------

// Every named entity -- variable, constant, type, subprogram, package, exception, loop,
// label -- is represented by a Symbol.  Symbols live in Scopes chained outward from
// inner to enclosing.  A Scope is a hash table of Symbol chains.

typedef enum {
  SYMBOL_UNKNOWN = 0,
  SYMBOL_VARIABLE,    SYMBOL_CONSTANT,     SYMBOL_TYPE,
  SYMBOL_SUBTYPE,     SYMBOL_PROCEDURE,    SYMBOL_FUNCTION,
  SYMBOL_PARAMETER,   SYMBOL_PACKAGE,      SYMBOL_EXCEPTION,
  SYMBOL_LABEL,       SYMBOL_LOOP,         SYMBOL_ENTRY,
  SYMBOL_COMPONENT,   SYMBOL_DISCRIMINANT, SYMBOL_LITERAL,
  SYMBOL_GENERIC,     SYMBOL_GENERIC_INSTANCE,
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
  Symbol_Kind      kind;
  String_Slice     name;
  Source_Location  location;
  Type_Info       *type;
  Scope           *defining_scope;
  Symbol          *parent;
  Symbol          *next_overload;
  Symbol          *next_in_bucket;

  enum {
    VIS_HIDDEN              = 0,
    VIS_IMMEDIATELY_VISIBLE = 1,
    VIS_USE_VISIBLE         = 2,
    VIS_DIRECTLY_VISIBLE    = 3
  } visibility;

  Syntax_Node    *declaration;

  Parameter_Info *parameters;      // Subprogram formals
  uint32_t        parameter_count;
  Type_Info      *return_type;     // NULL for procedures

  Symbol        **exported;        // Package visible-part symbols
  uint32_t        exported_count;

  uint32_t        unique_id;
  uint32_t        nesting_level;
  int64_t         frame_offset;
  Scope          *scope;

  bool            is_inline;
  bool            is_imported;
  bool            is_exported;
  String_Slice    external_name;
  String_Slice    link_name;
  enum {
    CONVENTION_ADA = 0,    CONVENTION_C,        CONVENTION_STDCALL,
    CONVENTION_INTRINSIC,  CONVENTION_ASSEMBLER
  } convention;
  uint32_t        suppressed_checks;
  bool            is_unreferenced;

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

  Symbol         *parent_operation;
  Type_Info      *derived_from_type;

  uint32_t        llvm_label_id;
  uint32_t        loop_exit_label_id;
  uint32_t        entry_index;
  Syntax_Node    *renamed_object;

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
  Symbol   *buckets[SYMBOL_TABLE_SIZE];
  Scope    *parent;
  Symbol   *owner;
  uint32_t  nesting_level;
  Symbol  **symbols;
  uint32_t  symbol_count;
  uint32_t  symbol_capacity;
  int64_t   frame_size;
  Symbol  **frame_vars;
  uint32_t  frame_var_count;
  uint32_t  frame_var_capacity;
};

typedef struct {
  Scope     *current_scope;
  Scope     *global_scope;
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

Scope   *Scope_New                         (Scope  *parent);
void     Symbol_Manager_Push_Scope         (Symbol *owner);
void     Symbol_Manager_Pop_Scope          (void);
void     Symbol_Manager_Push_Existing_Scope(Scope  *scope);
uint32_t Symbol_Hash_Name                  (String_Slice name);

Symbol *Symbol_New          (Symbol_Kind kind, String_Slice name, Source_Location location);
void    Symbol_Add          (Symbol *sym);
Symbol *Symbol_Find         (String_Slice name);
Symbol *Symbol_Find_By_Type (String_Slice name, Type_Info *expected_type);

void Symbol_Manager_Init_Predefined (void);
void Symbol_Manager_Init            (void);

typedef struct {
  Type_Info   **types;
  uint32_t      count;
  String_Slice *names;
} Argument_Info;

typedef struct {
  Symbol    *nam;          // Candidate symbol
  Type_Info *typ;          // Result type
  Type_Info *opnd_typ;     // Operand type (for operator resolution)
  bool       is_universal; // From a universal context
  uint32_t   scope_depth;  // Distance from use to declaration
} Interpretation;

typedef struct {
  Interpretation items[MAX_INTERPRETATIONS];
  uint32_t       count;
} Interp_List;

bool Type_Covers             (Type_Info    *expected, Type_Info    *actual);
bool Arguments_Match_Profile (Symbol       *sym,      Argument_Info *args);
void Collect_Interpretations (String_Slice  name,     Interp_List   *list);
void Filter_By_Arguments     (Interp_List  *interps,  Argument_Info *args);
bool Symbol_Hides            (Symbol       *sym1,     Symbol        *sym2);

int32_t Score_Interpretation    (Interpretation *interp,
                                 Type_Info      *context_type,
                                 Argument_Info  *args);
Symbol *Disambiguate            (Interp_List    *interps,
                                 Type_Info      *context_type,
                                 Argument_Info  *args);
Symbol *Resolve_Overloaded_Call (String_Slice    name,
                                 Argument_Info  *args,
                                 Type_Info      *context_type);

String_Slice Operator_Name (Token_Kind op);

// -------------
// -- Semantics --
// -------------

// The semantic pass walks the syntax tree to resolve identifiers, check type
// compatibility, fold static expressions, and freeze type representations.

Type_Info *Resolve_Expression   (Syntax_Node *node);
Type_Info *Resolve_Identifier   (Syntax_Node *node);
Type_Info *Resolve_Selected     (Syntax_Node *node);
Type_Info *Resolve_Binary_Op    (Syntax_Node *node);
Type_Info *Resolve_Apply        (Syntax_Node *node);
bool       Resolve_Char_As_Enum (Syntax_Node *char_node, Type_Info *enum_type);

void Resolve_Statement           (Syntax_Node *node);
void Resolve_Declaration         (Syntax_Node *node);
void Resolve_Declaration_List    (Node_List   *list);
void Resolve_Statement_List      (Node_List   *list);
void Resolve_Compilation_Unit    (Syntax_Node *node);
void Freeze_Declaration_List     (Node_List   *list);
void Populate_Package_Exports    (Symbol      *pkg_sym,  Syntax_Node *pkg_spec);
void Preregister_Labels          (Node_List   *list);
void Install_Declaration_Symbols (Node_List   *decls);

bool        Is_Integer_Expr     (Syntax_Node *node);
double      Eval_Const_Numeric  (Syntax_Node *node);
bool        Eval_Const_Rational (Syntax_Node *node, Rational *out);
const char *I128_Decimal        (int128_t     value);
const char *U128_Decimal        (uint128_t    value);

// -------------------
// -- Code Generation --
// -------------------

// The code generator walks the resolved syntax tree and emits LLVM IR as plain text
// to a FILE*.  The Code_Generator record holds all mutable state for one compilation unit.

typedef struct {
  FILE        *output;
  uint32_t     temp_id;
  uint32_t     label_id;
  uint32_t     global_id;
  uint32_t     string_id;
  Symbol      *current_function;
  uint32_t     current_nesting_level;
  Symbol      *current_instance;
  uint32_t     loop_exit_label;
  uint32_t     loop_continue_label;
  bool         has_return;
  bool         block_terminated;
  bool         header_emitted;
  Symbol      *main_candidate;
  Syntax_Node *deferred_bodies[64];
  uint32_t     deferred_count;
  Symbol      *enclosing_function;
  bool         is_nested;
  uint32_t     exception_handler_label;
  uint32_t     exception_jmp_buf;
  bool         in_exception_region;
  char        *string_const_buffer;
  size_t       string_const_size;
  size_t       string_const_capacity;
  Symbol      *address_markers[256];
  uint32_t     address_marker_count;
  uint32_t     emitted_func_ids[1024];
  uint32_t     emitted_func_count;
  bool         in_task_body;
  Symbol      *elab_funcs[64];
  uint32_t     elab_func_count;
  uint32_t     temp_type_keys[TEMP_TYPE_CAPACITY];
  const char  *temp_types[TEMP_TYPE_CAPACITY];
  uint8_t      temp_is_fat_alloca[TEMP_TYPE_CAPACITY];
  char        *exc_refs[EXC_REF_CAPACITY];
  uint32_t     exc_ref_count;
  bool         needs_trim_helpers;
  uint32_t     rt_type_counter;
  uint32_t     in_agg_component;
  uint32_t     inner_agg_bnd_lo[MAX_AGG_DIMS];
  uint32_t     inner_agg_bnd_hi[MAX_AGG_DIMS];
  int          inner_agg_bnd_n;
  uint32_t     disc_cache[MAX_DISC_CACHE];
  uint32_t     disc_cache_count;
  Type_Info   *disc_cache_type;
} Code_Generator;

extern Code_Generator *cg;

void Code_Generator_Init (FILE *output);

uint32_t    Emit_Temp             (void);
uint32_t    Emit_Label            (void);
void        Temp_Set_Type         (uint32_t temp_id, const char *llvm_type);
const char *Temp_Get_Type         (uint32_t temp_id);
void        Temp_Mark_Fat_Alloca  (uint32_t temp_id);
bool        Temp_Is_Fat_Alloca    (uint32_t temp_id);

void Emit                   (const char *format, ...);
void Emit_Location          (Source_Location location);
void Emit_Label_Here        (uint32_t label);
void Emit_Branch_If_Needed  (uint32_t label);
void Emit_String_Const      (const char *format, ...);
void Emit_String_Const_Char (char ch);
void Emit_Float_Constant    (uint32_t    result,
                             const char *float_type,
                             double      value,
                             const char *comment);

const char *String_Bound_Type    (void);
const char *String_Bounds_Struct (void);
const char *Integer_Arith_Type   (void);
const char *Wider_Int_Type       (const char *left, const char *right);
const char *Float_Cmp_Predicate  (int op);
const char *Int_Cmp_Predicate    (int op, bool is_unsigned);
int         String_Bounds_Alloc  (void);
int         Type_Bits            (const char *llvm_type);
bool        Is_Float_Type        (const char *llvm_type);

bool   Symbol_Is_Global   (Symbol *sym);
size_t Mangle_Slice_Into  (char         *buf,
                            size_t        pos,
                            size_t        max,
                            String_Slice  name);
size_t Mangle_Into_Buffer (char   *buf,
                            size_t  pos,
                            size_t  max,
                            Symbol *sym);

String_Slice Symbol_Mangle_Name    (Symbol      *sym);
String_Slice Mangle_Qualified_Name (String_Slice parent, String_Slice name);
void         Emit_Symbol_Name      (Symbol *sym);
void         Emit_Symbol_Ref       (Symbol *sym);
void         Emit_Symbol_Storage   (Symbol *sym);
void         Emit_Exception_Ref    (Symbol *exc);
Symbol      *Find_Instance_Local   (const Symbol *template_sym);

Symbol  *Find_Enclosing_Subprogram      (Symbol *sym);
bool     Subprogram_Needs_Static_Chain  (Symbol *sym);
bool     Is_Uplevel_Access              (const Symbol *sym);
int      Nested_Frame_Depth             (Symbol *proc);
uint32_t Precompute_Nested_Frame_Arg    (Symbol *proc);
bool     Emit_Nested_Frame_Arg          (Symbol *proc, uint32_t precomp);

// -- Checks and Conversions
void Emit_Raise_Exception  (const char *exc_name, const char *comment);
void Emit_Check_With_Raise (uint32_t    cond,
                             bool        raise_on_true,
                             const char *comment);
void Emit_Range_Check_With_Raise (uint32_t    val,
                                   int64_t     lo_val,
                                   int64_t     hi_val,
                                   const char *type,
                                   const char *comment);
uint32_t Emit_Overflow_Checked_Op (uint32_t    left,
                                    uint32_t    right,
                                    const char *op,
                                    const char *llvm_type,
                                    Type_Info  *result_type);
void Emit_Division_Check (uint32_t    divisor,
                           const char *type,
                           Type_Info  *t);
void Emit_Signed_Division_Overflow_Check (uint32_t    dividend,
                                           uint32_t    divisor,
                                           const char *llvm_type,
                                           Type_Info  *type);

uint32_t Emit_Convert           (uint32_t src, const char *from, const char *to);
uint32_t Emit_Convert_Ext       (uint32_t    src,
                                  const char *src_type,
                                  const char *dst_type,
                                  bool        is_unsigned);
uint32_t Emit_Coerce            (uint32_t temp, const char *desired_type);
uint32_t Emit_Coerce_Default_Int(uint32_t temp, const char *desired_type);

uint32_t Emit_Index_Check (uint32_t    index,
                            uint32_t    low_bound,
                            uint32_t    high_bound,
                            const char *index_type,
                            Type_Info  *array_type);
void Emit_Length_Check (uint32_t    src_length,
                         uint32_t    dst_length,
                         const char *len_type,
                         Type_Info  *array_type);
void Emit_Access_Check       (uint32_t ptr_val, Type_Info *acc_type);
void Emit_Discriminant_Check (uint32_t    actual,
                               uint32_t    expected,
                               const char *disc_type,
                               Type_Info  *record_type);
uint32_t Emit_Constraint_Check (uint32_t   val,
                                 Type_Info *target,
                                 Type_Info *source);
uint32_t Emit_Constraint_Check_With_Type (uint32_t    val,
                                          Type_Info  *target,
                                          Type_Info  *source,
                                          const char *actual_val_type);
void Emit_Subtype_Constraint_Compat_Check (Type_Info *subtype);

uint32_t Emit_Widen_For_Intrinsic (uint32_t val, const char *from_type);
uint32_t Emit_Extend_To_I64       (uint32_t val, const char *from);

// -- Fat-Pointer Operations
uint32_t Emit_Fat_Pointer          (uint32_t    data_ptr,
                                    int128_t    low,
                                    int128_t    high,
                                    const char *bt);
uint32_t Emit_Fat_Pointer_Dynamic  (uint32_t    data_ptr,
                                    uint32_t    lo,
                                    uint32_t    hi,
                                    const char *bt);
uint32_t Emit_Fat_Pointer_Heap     (uint32_t    data_ptr,
                                    uint32_t    lo,
                                    uint32_t    hi,
                                    const char *bt);
uint32_t Emit_Fat_Pointer_MultiDim (uint32_t    data_ptr,
                                    uint32_t   *lo,
                                    uint32_t   *hi,
                                    uint32_t    ndims,
                                    const char *bt);
uint32_t Emit_Fat_Pointer_Null     (const char *bt);
uint32_t Fat_Ptr_As_Value          (uint32_t fat_ptr);

uint32_t Emit_Fat_Pointer_Data     (uint32_t fat_ptr, const char *bt);
uint32_t Emit_Fat_Pointer_Bound    (uint32_t    fat_ptr,
                                    const char *bt,
                                    uint32_t    field_index);
uint32_t Emit_Fat_Pointer_Low_Dim  (uint32_t fat_ptr, const char *bt, uint32_t dim);
uint32_t Emit_Fat_Pointer_High_Dim (uint32_t fat_ptr, const char *bt, uint32_t dim);
uint32_t Emit_Fat_Pointer_Length   (uint32_t fat_ptr, const char *bt);
uint32_t Emit_Fat_Pointer_Length_Dim(uint32_t fat_ptr, const char *bt, uint32_t dim);
uint32_t Emit_Fat_Pointer_Compare  (uint32_t    left_fat,
                                    uint32_t    right_fat,
                                    const char *bt);

uint32_t Emit_Load_Fat_Pointer           (Symbol   *sym, const char *bt);
uint32_t Emit_Load_Fat_Pointer_From_Temp (uint32_t  ptr, const char *bt);
void     Emit_Store_Fat_Pointer_To_Symbol     (uint32_t    fat,
                                               Symbol     *sym,
                                               const char *bt);
void     Emit_Store_Fat_Pointer_Fields_To_Temp(uint32_t    data,
                                               uint32_t    lo,
                                               uint32_t    hi,
                                               uint32_t    dest,
                                               const char *bt);
void     Emit_Fat_Pointer_Copy_To_Name (uint32_t    fat_ptr,
                                        Symbol     *dst,
                                        const char *bt);
void     Emit_Fat_To_Array_Memcpy      (uint32_t   fat_val,
                                        uint32_t   dest_ptr,
                                        Type_Info *t);

uint32_t Emit_Alloc_Bounds_Struct  (uint32_t lo, uint32_t hi, const char *bt);
uint32_t Emit_Alloc_Bounds_MultiDim(uint32_t   *lo,
                                    uint32_t   *hi,
                                    uint32_t    ndims,
                                    const char *bt);
uint32_t Emit_Heap_Bounds_Struct   (uint32_t lo, uint32_t hi, const char *bt);

void Emit_Fat_Pointer_Insertvalue_Named  (const char *prefix,
                                          const char *data_expr,
                                          const char *low_expr,
                                          const char *high_expr,
                                          const char *bt);
void Emit_Fat_Pointer_Extractvalue_Named (const char *src_name,
                                          const char *data_name,
                                          const char *low_name,
                                          const char *high_name,
                                          const char *bt);
void Emit_Widen_Named_For_Intrinsic   (const char *src, const char *dst, const char *bt);
void Emit_Narrow_Named_From_Intrinsic (const char *src, const char *dst, const char *bt);

// -- Bound and Length Emission
typedef struct {
  uint32_t    low_temp;
  uint32_t    high_temp;
  const char *bound_type;
} Bound_Temps;

uint32_t    Emit_Bound_Value          (Type_Bound *bound);
uint32_t    Emit_Bound_Value_Typed    (Type_Bound *bound, const char **out_type);
uint32_t    Emit_Single_Bound         (Type_Bound *bound, const char *ty);
Bound_Temps Emit_Bounds               (Type_Info  *type,  uint32_t dim);
Bound_Temps Emit_Bounds_From_Fat      (uint32_t    fat,   const char *bt);
Bound_Temps Emit_Bounds_From_Fat_Dim  (uint32_t    fat,   const char *bt, uint32_t dim);
uint32_t    Emit_Length_From_Bounds   (uint32_t    lo,    uint32_t hi, const char *bt);
uint32_t    Emit_Length_Clamped       (uint32_t    lo,    uint32_t hi, const char *bt);
uint32_t    Emit_Static_Int           (int128_t    value, const char *ty);
uint32_t    Emit_Type_Bound           (Type_Bound *bound, const char *ty);
uint32_t    Emit_Min_Value            (uint32_t    left,  uint32_t right, const char *ty);
uint32_t    Emit_Memcmp_Eq            (uint32_t    left_ptr,
                                       uint32_t    right_ptr,
                                       uint32_t    byte_size_temp,
                                       int64_t     byte_size_static,
                                       bool        is_dynamic);
uint32_t    Emit_Array_Lex_Compare    (uint32_t    left_ptr,
                                       uint32_t    right_ptr,
                                       uint32_t    elem_size,
                                       const char *bt);

// -- Exception Handling
typedef struct {
  uint32_t handler_frame;
  uint32_t jmp_buf;
  uint32_t normal_label;
  uint32_t handler_label;
} Exception_Setup;

Exception_Setup Emit_Exception_Handler_Setup (void);
uint32_t        Emit_Current_Exception_Id    (void);
void            Generate_Exception_Dispatch  (Node_List *handlers,
                                              uint32_t   exc_id,
                                              uint32_t   end_label);

// -- Expression Generation
uint32_t Generate_Expression        (Syntax_Node *node);
uint32_t Generate_Lvalue            (Syntax_Node *node);
uint32_t Generate_Integer_Literal   (Syntax_Node *node);
uint32_t Generate_Real_Literal      (Syntax_Node *node);
uint32_t Generate_String_Literal    (Syntax_Node *node);
uint32_t Generate_Identifier        (Syntax_Node *node);
uint32_t Generate_Binary_Op         (Syntax_Node *node);
uint32_t Generate_Unary_Op          (Syntax_Node *node);
uint32_t Generate_Apply             (Syntax_Node *node);
uint32_t Generate_Selected          (Syntax_Node *node);
uint32_t Generate_Attribute         (Syntax_Node *node);
uint32_t Generate_Qualified         (Syntax_Node *node);
uint32_t Generate_Allocator         (Syntax_Node *node);
uint32_t Generate_Aggregate         (Syntax_Node *node);
uint32_t Generate_Composite_Address (Syntax_Node *node);
uint32_t Generate_Bound_Value       (Type_Bound   bound, const char *ty);
uint32_t Generate_Array_Equality    (uint32_t    left_ptr,
                                     uint32_t    right_ptr,
                                     Type_Info  *t);
uint32_t Generate_Record_Equality   (uint32_t    left_ptr,
                                     uint32_t    right_ptr,
                                     Type_Info  *t);
uint32_t Normalize_To_Fat_Pointer   (Syntax_Node *expr,
                                     uint32_t     raw,
                                     Type_Info   *type,
                                     const char  *bt);
uint32_t Wrap_Constrained_As_Fat    (Syntax_Node *expr,
                                     Type_Info   *type,
                                     const char  *bt);
uint32_t Convert_Real_To_Fixed      (uint32_t val, double small, const char *fix_type);

bool     Aggregate_Produces_Fat_Pointer (const Type_Info *t);
void     Ensure_Runtime_Type_Globals    (Type_Info *t);
uint32_t Emit_Bool_Array_Binop          (uint32_t    left,
                                         uint32_t    right,
                                         Type_Info  *result_type,
                                         const char *ir_op);
uint32_t Emit_Bool_Array_Not            (uint32_t operand, Type_Info *result_type);
uint32_t Get_Dimension_Index            (Syntax_Node *arg);

void     Emit_Float_Type_Limit   (uint32_t     t,
                                   Type_Info   *type,
                                   bool         is_low,
                                   String_Slice attr);
uint32_t Emit_Bound_Attribute    (uint32_t     t,
                                   Type_Info   *prefix_type,
                                   Symbol      *prefix_sym,
                                   Syntax_Node *prefix_expr,
                                   bool         needs_runtime_bounds,
                                   uint32_t     dim,
                                   bool         is_low,
                                   String_Slice attr);
uint32_t Emit_Disc_Constraint_Value (Type_Info  *type_info,
                                      uint32_t    disc_index,
                                      const char *disc_type);
void Emit_Nested_Disc_Checks (Type_Info *parent_type);
void Emit_Comp_Disc_Check    (uint32_t   ptr, Type_Info *comp_ti);

// -- Statement Generation
void Generate_Statement        (Syntax_Node *node);
void Generate_Statement_List   (Node_List   *list);
void Generate_Assignment       (Syntax_Node *node);
void Generate_If_Statement     (Syntax_Node *node);
void Generate_Loop_Statement   (Syntax_Node *node);
void Generate_For_Loop         (Syntax_Node *node);
void Generate_Return_Statement (Syntax_Node *node);
void Generate_Case_Statement   (Syntax_Node *node);
void Generate_Block_Statement  (Syntax_Node *node);
void Generate_Raise_Statement  (Syntax_Node *node);

// -- Declaration Generation
void         Generate_Declaration            (Syntax_Node *node);
void         Generate_Declaration_List       (Node_List   *list);
void         Generate_Object_Declaration     (Syntax_Node *node);
void         Generate_Subprogram_Body        (Syntax_Node *node);
void         Generate_Task_Body              (Syntax_Node *node);
void         Emit_Extern_Subprogram          (Symbol *sym);
void         Emit_Function_Header            (Symbol *sym, bool is_nested);
void         Process_Deferred_Bodies         (uint32_t saved_deferred_count);
void         Generate_Generic_Instance_Body  (Symbol      *inst_sym,
                                              Syntax_Node *template_body);
bool         Is_Builtin_Function             (String_Slice name);
bool         Has_Nested_Subprograms          (Node_List *declarations, Node_List *statements);
bool         Has_Nested_In_Statements        (Node_List *statements);
Syntax_Node *Find_Homograph_Body             (Symbol      **exports,
                                              uint32_t      idx,
                                              String_Slice  name,
                                              Node_List    *body_decls);
void         Emit_Task_Function_Name         (Symbol *task_sym, String_Slice fallback_name);

// -- Aggregate Helpers
typedef struct {
  uint32_t     n_positional;
  bool         has_named;
  bool         has_others;
  Syntax_Node *others_expr;
} Agg_Class;

typedef struct { Symbol *sym; uint32_t temp; }            Disc_Alloc_Entry;
typedef struct { Disc_Alloc_Entry *entries; uint32_t count; } Disc_Alloc_Info;

Agg_Class Agg_Classify        (Syntax_Node *node);
bool      Bound_Pair_Overflows (Type_Bound low, Type_Bound high);
uint32_t  Agg_Resolve_Elem    (Syntax_Node *expr,
                                bool         multidim,
                                bool         elem_is_composite,
                                Type_Info   *agg_type,
                                const char  *elem_type,
                                Type_Info   *elem_ti);
void      Agg_Store_At_Static (uint32_t    base,
                                uint32_t    val,
                                int128_t    idx,
                                const char *elem_type,
                                uint32_t    elem_size,
                                bool        is_composite);
void      Agg_Store_At_Dynamic(uint32_t    base,
                                uint32_t    val,
                                uint32_t    arr_idx,
                                const char *idx_type,
                                const char *elem_type,
                                uint32_t    elem_size,
                                uint32_t    rt_row_size,
                                bool        is_composite);
bool      Agg_Elem_Is_Composite  (Type_Info *elem, bool multidim);
uint32_t  Agg_Comp_Byte_Size     (Type_Info *ti,   Syntax_Node *src);
void      Agg_Rec_Store          (uint32_t        val,
                                   uint32_t        dest_ptr,
                                   Component_Info *comp,
                                   Syntax_Node    *src_expr);
void      Agg_Rec_Disc_Post      (uint32_t         val,
                                   Component_Info  *comp,
                                   uint32_t         disc_ordinal,
                                   Type_Info       *agg_type,
                                   Disc_Alloc_Info *da_info);

uint32_t Disc_Ordinal_Before             (Type_Info *type_info, uint32_t comp_index);
int32_t  Find_Record_Component           (Type_Info *record_type, String_Slice name);
bool     Is_Others_Choice                (Syntax_Node *choice);
bool     Is_Static_Int_Node              (Syntax_Node *n);
int128_t Static_Int_Value                (Syntax_Node *n);
void     Desugar_Aggregate_Range_Choices (Syntax_Node *agg);
void     Emit_Inner_Consistency_Track    (uint32_t   *inner_trk_lo,
                                          uint32_t   *inner_trk_hi,
                                          uint32_t    inner_trk_first,
                                          uint32_t    inner_trk_mm,
                                          int         n_inner_dims,
                                          const char *bt);
void     Collect_Disc_Symbols_In_Expr    (Syntax_Node *node,
                                          Symbol     **found,
                                          uint32_t    *count,
                                          uint32_t     max);

void Generate_Type_Equality_Function (Type_Info *t);
void Generate_Implicit_Operators     (void);
void Generate_Exception_Globals      (void);
void Generate_Extern_Declarations    (Syntax_Node *node);
void Generate_Compilation_Unit       (Syntax_Node *node);

// -- Build-in-Place Protocol
typedef enum {
  BIP_ALLOC_UNSPECIFIED = 0, BIP_ALLOC_CALLER      = 1,
  BIP_ALLOC_SECONDARY   = 2, BIP_ALLOC_GLOBAL_HEAP = 3,
  BIP_ALLOC_USER_POOL   = 4
} BIP_Alloc_Form;

typedef enum {
  BIP_FORMAL_ALLOC_FORM,   BIP_FORMAL_STORAGE_POOL,
  BIP_FORMAL_FINALIZATION, BIP_FORMAL_TASK_MASTER,
  BIP_FORMAL_ACTIVATION,   BIP_FORMAL_OBJECT_ACCESS
} BIP_Formal_Kind;

typedef struct {
  Symbol        *func;
  Type_Info     *result_type;
  BIP_Alloc_Form alloc_form;
  uint32_t       dest_ptr;
  bool           needs_finalization;
  bool           has_tasks;
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

bool BIP_Is_Explicitly_Limited        (const Type_Info *t);
bool BIP_Is_Task_Type                 (const Type_Info *t);
bool BIP_Record_Has_Limited_Component (const Type_Info *t);
bool BIP_Is_Limited_Type              (const Type_Info *t);
bool BIP_Is_BIP_Function              (const Symbol    *func);
bool BIP_Type_Has_Task_Component      (const Type_Info *t);
bool BIP_Needs_Alloc_Form             (const Symbol    *func);

uint32_t       BIP_Extra_Formal_Count   (const Symbol *func);
BIP_Alloc_Form BIP_Determine_Alloc_Form (bool is_allocator,
                                          bool in_return_stmt,
                                          bool has_target);
void           BIP_Begin_Function       (const Symbol *func);
bool           BIP_In_BIP_Function      (void);
void           BIP_End_Function         (void);

// ----------------------
// -- Library Management --
// ----------------------

// ALI files record dependencies, checksums, and exported symbols per compilation unit.

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
  Export_Info     exports[256];
  uint32_t        export_count;
} ALI_Info;

typedef struct {
  char     kind;
  char    *name;
  char    *mangled_name;
  char    *llvm_type;
  uint32_t line;
  char    *type_name;
  uint32_t param_count;
} ALI_Export;

typedef struct ALI_Cache_Entry_Forward {
  char       *unit_name;
  char       *source_file;
  char       *ali_file;
  uint32_t    checksum;
  bool        is_spec;
  bool        is_generic;
  bool        is_preelaborate;
  bool        is_pure;
  bool        loaded;
  char       *withs[64];
  uint32_t    with_count;
  ALI_Export  exports[256];
  uint32_t    export_count;
} ALI_Cache_Entry;

extern ALI_Cache_Entry ALI_Cache[256];
extern uint32_t        ALI_Cache_Count;

uint32_t CRC32_Update (uint32_t crc, const void *data, size_t length);

void ALI_Collect_Exports (ALI_Info *ali, Syntax_Node *unit);
void ALI_Collect_Withs   (ALI_Info *ali, Syntax_Node *ctx);
void ALI_Collect_Unit    (ALI_Info    *ali,
                           Syntax_Node *cu,
                           const char  *source,
                           size_t       source_size);

void             ALI_Write (FILE *out, ALI_Info *ali);
ALI_Cache_Entry *ALI_Read  (const char *ali_path);

char *ALI_Find          (String_Slice     unit_name);
bool  Try_Load_From_ALI (String_Slice     name);
void  ALI_Load_Symbols  (ALI_Cache_Entry *entry);

void Generate_ALI_File (const char   *output_path,
                         Syntax_Node **units,
                         int           unit_count,
                         const char   *source,
                         size_t        source_size);

// ----------------
// -- Elaboration --
// ----------------

// Library-level packages must be elaborated in dependency order.  This chapter builds
// the dependency graph, detects SCCs via Tarjan's algorithm, and produces a topological
// ordering.

typedef enum { UNIT_SPEC, UNIT_BODY, UNIT_SPEC_ONLY, UNIT_BODY_ONLY } Elab_Unit_Kind;

typedef enum {
  EDGE_WITH,          EDGE_ELABORATE,
  EDGE_ELABORATE_ALL, EDGE_SPEC_BEFORE_BODY,
  EDGE_INVOCATION,    EDGE_FORCED
} Elab_Edge_Kind;

typedef enum { PREC_HIGHER, PREC_EQUAL, PREC_LOWER } Elab_Precedence;

typedef enum {
  ELAB_ORDER_OK,
  ELAB_ORDER_HAS_CYCLE,
  ELAB_ORDER_HAS_ELABORATE_ALL_CYCLE
} Elab_Order_Status;

typedef struct Elab_Vertex Elab_Vertex;
typedef struct Elab_Edge   Elab_Edge;

struct Elab_Vertex {
  uint32_t       id;              // Graph-unique vertex index
  String_Slice   name;            // Unit name
  Elab_Unit_Kind kind;            // Spec, body, or standalone
  Symbol        *symbol;          // Corresponding package symbol
  uint32_t       component_id;    // SCC component from Tarjan
  uint32_t       pending_strong;  // Unelaborated strong predecessors
  uint32_t       pending_weak;    // Unelaborated weak predecessors
  bool           in_elab_order;   // Already placed in final order
  bool           is_preelaborate; // Pragma Preelaborate
  bool           is_pure;         // Pragma Pure
  bool           has_elab_body;   // Has elaboration code
  bool           is_predefined;   // Standard library unit
  bool           is_internal;     // Compiler-generated unit
  bool           needs_elab_code; // Needs an elab call in main
  Elab_Vertex   *body_vertex;     // Paired body (from a spec)
  Elab_Vertex   *spec_vertex;     // Paired spec (from a body)
  uint32_t       first_pred_edge; // Head of predecessor edge list
  uint32_t       first_succ_edge; // Head of successor edge list
  int32_t        tarjan_index;    // Tarjan discovery index
  int32_t        tarjan_lowlink;  // Tarjan lowlink value
  bool           tarjan_on_stack; // Currently on Tarjan stack
};

struct Elab_Edge {
  uint32_t       id;             // Graph-unique edge index
  Elab_Edge_Kind kind;           // Dependency kind
  bool           is_strong;      // Strong edges block elaboration
  uint32_t       pred_vertex_id; // Source vertex
  uint32_t       succ_vertex_id; // Target vertex
  uint32_t       next_pred_edge; // Next edge in pred list
  uint32_t       next_succ_edge; // Next edge in succ list
};

typedef struct { uint64_t bits[(ELAB_MAX_VERTICES + 63) / 64]; } Elab_Vertex_Set;

typedef struct {
  Elab_Vertex  vertices[ELAB_MAX_VERTICES];
  uint32_t     vertex_count;
  Elab_Edge    edges[ELAB_MAX_EDGES];
  uint32_t     edge_count;
  uint32_t     component_pending_strong[ELAB_MAX_COMPONENTS];
  uint32_t     component_pending_weak[ELAB_MAX_COMPONENTS];
  uint32_t     component_count;
  Elab_Vertex *order[ELAB_MAX_VERTICES];
  uint32_t     order_count;
  bool         has_elaborate_all_cycle;
} Elab_Graph;

typedef struct {
  uint32_t stack[ELAB_MAX_VERTICES];
  uint32_t stack_top;
  int32_t  index;
} Tarjan_State;

extern Elab_Graph g_elab_graph;
extern bool       g_elab_graph_initialized;

Elab_Graph Elab_Graph_New        (void);
void       Elab_Init             (void);
void       Elab_Find_Components  (Elab_Graph *g);
void       Elab_Pair_Specs_Bodies(Elab_Graph *g);
uint32_t   Elab_Find_Vertex      (const Elab_Graph *g,
                                   String_Slice      name,
                                   Elab_Unit_Kind    kind);
uint32_t   Elab_Add_Vertex       (Elab_Graph    *g,
                                   String_Slice   name,
                                   Elab_Unit_Kind kind,
                                   Symbol        *sym);
uint32_t   Elab_Add_Edge         (Elab_Graph    *g,
                                   uint32_t       pred_id,
                                   uint32_t       succ_id,
                                   Elab_Edge_Kind kind);

bool               Edge_Kind_Is_Strong   (Elab_Edge_Kind    k);
Elab_Vertex       *Elab_Get_Vertex       (Elab_Graph       *g, uint32_t id);
const Elab_Vertex *Elab_Get_Vertex_Const (const Elab_Graph *g, uint32_t id);

Elab_Vertex_Set Elab_Set_Empty    (void);
bool            Elab_Set_Contains (const Elab_Vertex_Set *s, uint32_t id);
void            Elab_Set_Insert   (Elab_Vertex_Set *s, uint32_t id);
void            Elab_Set_Remove   (Elab_Vertex_Set *s, uint32_t id);
uint32_t        Elab_Set_Size     (const Elab_Vertex_Set *s);

void Elab_Elaborate_Vertex (Elab_Graph      *g,
                             uint32_t         v_id,
                             Elab_Vertex_Set *elaborable,
                             Elab_Vertex_Set *waiting);
Elab_Order_Status Elab_Elaborate_Graph (Elab_Graph *g);
uint32_t          Elab_Register_Unit   (String_Slice name,
                                         bool         is_body,
                                         Symbol      *sym,
                                         bool         is_preelaborate,
                                         bool         is_pure,
                                         bool         has_elab_code);

Elab_Order_Status Elab_Compute_Order    (void);
uint32_t          Elab_Get_Order_Count  (void);
Symbol           *Elab_Get_Order_Symbol (uint32_t index);
bool              Elab_Needs_Elab_Call  (uint32_t index);

// -----------
// -- Generics --
// -----------

// Generic units are instantiated by macro-style expansion: the template AST is
// deep-cloned with formal-to-actual substitution, then resolved and code-generated
// as though the programmer had written the expanded text by hand.

typedef struct {
  String_Slice  formal_name;   // Generic formal parameter name
  Type_Info    *actual_type;   // Substituted actual type
  Symbol       *actual_symbol; // Actual symbol (for subprogram formals)
  Syntax_Node  *actual_expr;   // Actual expression (for object formals)
} Generic_Mapping;

typedef struct {
  Generic_Mapping  mappings[32]; // Formal-to-actual mapping array
  uint32_t         count;        // Number of active mappings
  Symbol          *instance_sym; // The instantiation symbol
  Symbol          *template_sym; // The generic template symbol
} Instantiation_Env;

Type_Info   *Env_Lookup_Type (Instantiation_Env *env, String_Slice name);
Syntax_Node *Env_Lookup_Expr (Instantiation_Env *env, String_Slice name);

Syntax_Node *Node_Deep_Clone (Syntax_Node       *node,
                               Instantiation_Env *env,
                               int                depth);
void         Node_List_Clone (Node_List         *dst,
                               Node_List         *src,
                               Instantiation_Env *env,
                               int                depth);

void Build_Instantiation_Env (Instantiation_Env *env, Symbol *inst, Symbol *tmpl);
void Expand_Generic_Package  (Symbol *instance_sym);

// ----------------
// -- File Loading --
// ----------------

// WITH clauses name packages that must be found on disk, loaded, parsed, analysed,
// and code-generated before the withing unit can proceed.  Include_Paths lists the
// directories to search; Lookup_Path maps a unit name to a file path.  Loading_Set
// detects circular WITH dependencies by tracking which units are currently being loaded.

extern const char   *Include_Paths[32];
extern uint32_t      Include_Path_Count;
extern Syntax_Node  *Loaded_Package_Bodies[128];
extern int           Loaded_Body_Count;
extern String_Slice  Loaded_Body_Names[128];
extern int           Loaded_Body_Names_Count;

typedef struct {
  String_Slice names[64]; // Unit names currently being loaded
  int          count;
} Loading_Set;

extern Loading_Set Loading_Packages;

bool  Body_Already_Loaded  (String_Slice name);
void  Mark_Body_Loaded     (String_Slice name);
bool  Loading_Set_Contains (String_Slice name);
void  Loading_Set_Add      (String_Slice name);
void  Loading_Set_Remove   (String_Slice name);
char *Lookup_Path          (String_Slice name);
char *Lookup_Path_Body     (String_Slice name);
bool  Has_Precompiled_LL   (String_Slice name);

void  Load_Package_Spec (String_Slice name, char *src);
char *Read_File         (const char *path, size_t *out_size);
char *Read_File_Simple  (const char *path);

// -----------------
// -- Vector Paths --
// -----------------

// Vectorised scanning primitives for whitespace skipping, identifier recognition,
// digit scanning, and single-character search.  Three implementations are selected
// at compile time by the platform detection above:
//   x86-64   AVX-512BW (64-byte), AVX2 (32-byte), with scalar tail
//   ARM64    NEON/ASIMD (16-byte), with scalar tail
//   Generic  Scalar fallback with unrolled loops

#ifdef SIMD_X86_64
  extern int Simd_Has_Avx512;
  extern int Simd_Has_Avx2;
  uint32_t   Tzcnt32 (uint32_t value);
  uint64_t   Tzcnt64 (uint64_t value);
  #ifdef __AVX2__
    uint32_t Simd_Parse_8_Digits_Avx2 (const char *digits);
    int      Simd_Parse_Digits_Avx2   (const char *cursor,
                                        const char *limit,
                                        uint64_t   *out);
  #endif
#elif defined(SIMD_ARM64)
  uint64_t Tzcnt64 (uint64_t value);
#endif

void        Simd_Detect_Features   (void);
const char *Simd_Skip_Whitespace   (const char *cursor, const char *limit);
const char *Simd_Find_Newline      (const char *cursor, const char *limit);
const char *Simd_Find_Quote        (const char *cursor, const char *limit);
const char *Simd_Find_Double_Quote (const char *cursor, const char *limit);
const char *Simd_Scan_Identifier   (const char *cursor, const char *limit);
const char *Simd_Scan_Digits       (const char *cursor, const char *limit);

// ----------
// -- Driver --
// ----------

// The main driver parses command-line arguments, compiles each source file to LLVM IR
// -- optionally forking a subprocess per file for parallel compilation -- and returns
// an exit status.  Derive_Output_Path maps an input .adb or .ads to the .ll output path.

typedef struct {
  const char *input_path;  // Source file to compile
  const char *output_path; // NULL means derive from input
  int         exit_status; // Zero for success, one for failure
} Compile_Job;

void  Compile_File        (const char *input_path, const char *output_path);
void  Derive_Output_Path  (const char *input, char *out, size_t out_size);
void *Compile_Worker      (void *arg);
int   main                (int argc, char *argv[]);

#endif // ADA83_H
