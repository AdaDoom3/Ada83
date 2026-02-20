/* ═══════════════════════════════════════════════════════════════════════════════════════════ */
/*                                                                                           */
/*                             A D A 8 3   C O M P I L E R                                   */
/*                                                                                           */
/*                        Package Specification   (ada83.h)                                   */
/*                                                                                           */
/*                  An Ada 1983 Compiler Targeting LLVM Intermediate Representation           */
/*                                                                                           */
/* ═══════════════════════════════════════════════════════════════════════════════════════════ */
/*                                                                                           */
/*  This header declares the complete public interface of the Ada83 compiler, a              */
/*  single-translation-unit implementation that reads Ada 1983 source text conforming         */
/*  to ANSI/MIL-STD-1815A and emits LLVM IR suitable for compilation by llc or clang.        */
/*                                                                                           */
/*  The material is arranged as one would arrange a book: it begins with the simplest        */
/*  vocabulary -- scalar types, character predicates, unit conversions -- and builds          */
/*  steadily toward the full complexity of generic expansion and code generation.             */
/*  A reader who proceeds from beginning to end will encounter each concept only              */
/*  after its prerequisites have been established.                                           */
/*                                                                                           */
/*  Chapter  1.   Foundations        Includes, typedefs, target constants, ctype wrappers     */
/*  Chapter  2.   Measurement        Bit/byte morphisms, LLVM type selection, range checks   */
/*  Chapter  3.   Memory             Arena allocator for the compilation session              */
/*  Chapter  4.   Text               String slices, hashing, edit distance                   */
/*  Chapter  5.   Provenance         Source locations and diagnostic reporting                */
/*  Chapter  6.   Arithmetic         Big integers, big reals, exact rationals                */
/*  Chapter  7.   Lexical Analysis   Token kinds, lexer state, scanning functions             */
/*  Chapter  8.   Syntax             Node kinds, the syntax tree, node lists                  */
/*  Chapter  9.   Parsing            Recursive descent for the full Ada 83 grammar           */
/*  Chapter 10.   Types              The Ada type lattice, Type_Info, classification          */
/*  Chapter 11.   Names              Symbol table, scopes, overload resolution               */
/*  Chapter 12.   Semantics          Name resolution, type checking, constant folding        */
/*  Chapter 13.   Code Generation    LLVM IR emission for every Ada construct                */
/*  Chapter 14.   Library Management ALI files, checksums, dependency tracking               */
/*  Chapter 15.   Elaboration        Dependency ordering for multi-unit programs              */
/*  Chapter 16.   Generics           Macro-style instantiation of generic units              */
/*  Chapter 17.   File Loading       Include-path search, source file I/O                    */
/*  Chapter 18.   Vector Paths       SIMD-accelerated scanning on x86-64 and ARM64          */
/*  Chapter 19.   Driver             Command-line parsing and top-level orchestration        */
/*                                                                                           */
/* ═══════════════════════════════════════════════════════════════════════════════════════════ */

#ifndef ADA83_H
#define ADA83_H

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 1.   FOUNDATIONS                                                                 */
/*                                                                                           */
/*  Every edifice rests on its materials.  This chapter establishes the raw vocabulary:       */
/*  the standard headers that supply primitive types and I/O, the 128-bit integer             */
/*  extensions required by Ada's numeric model, the target data-model constants, and          */
/*  the safe character-classification wrappers that protect against the perennial             */
/*  signed-char pitfall in the C library.                                                    */
/*                                                                                           */
/* ========================================================================================= */

/*  Standard C headers -- one per line for grep-friendliness.  iso646.h provides             */
/*  the alternate tokens `and', `or', `not' that make Boolean expressions read               */
/*  like Ada rather than like punctuation.                                                   */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.1  Extended Integer Types                                                              */
/*                                                                                           */
/*  Ada's numeric model demands integers wider than 64 bits.  Modular types permit           */
/*  moduli up to 2**128, and universal_integer arithmetic is unbounded.  The GCC/Clang       */
/*  __int128 extension gives us native 128-bit registers on 64-bit targets, which            */
/*  suffices for all compile-time range checks and representation computations.              */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef __int128          int128_t;
typedef unsigned __int128 uint128_t;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.2  Target Data Model                                                                   */
/*                                                                                           */
/*  The compiler assumes a 64-bit LP64 host throughout.  All sizes stored in                 */
/*  Type_Info (Chapter 10) are in bytes, matching the LLVM DataLayout convention.             */
/*  Bit widths appear only in IR emission and range checks; everywhere else,                 */
/*  arithmetic is done in bytes to prevent the confusion that inevitably follows              */
/*  when both units circulate in the same code.                                              */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

enum { Bits_Per_Unit = 8 };
/*  Number of bits in the smallest addressable storage unit.                                 */

typedef enum {
  Width_1    = 1,     Width_8   = 8,    Width_16     = 16,
  Width_32   = 32,    Width_64  = 64,   Width_128    = 128,
  Width_Ptr  = 64,    Width_Float = 32, Width_Double = 64
} Bit_Width;
/*  Named bit widths used when selecting LLVM IR integer and float types.                    */

typedef enum {
  Ada_Short_Short_Integer_Bits    = Width_8,
  Ada_Short_Integer_Bits          = Width_16,
  Ada_Integer_Bits                = Width_32,
  Ada_Long_Integer_Bits           = Width_64,
  Ada_Long_Long_Integer_Bits      = Width_64,
  Ada_Long_Long_Long_Integer_Bits = Width_128       /* Ada 2022 extension */
} Ada_Integer_Width;
/*  Bit widths for the predefined integer types declared in package Standard.                 */
/*  The semantic pass uses these when installing the universal Standard types.                */

enum {
  Default_Size_Bits   = Ada_Integer_Bits,
  Default_Size_Bytes  = Ada_Integer_Bits / Bits_Per_Unit,
  Default_Align_Bytes = Default_Size_Bytes
};
/*  Default size and alignment for objects when no representation clause                     */
/*  or type constraint specifies otherwise.                                                  */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.3  Fat-Pointer Layout                                                                  */
/*                                                                                           */
/*  An unconstrained array parameter in Ada carries both a data pointer and a                */
/*  bounds pointer.  The compiler packs these into a fat pointer: { data, bounds },          */
/*  which occupies 16 bytes on all 64-bit targets.  STRING bounds are a pair of              */
/*  i32 indices -- first and last -- matching the layout described in the                    */
/*  gnatllvm-arrays-create.adb source (lines 684-707).                                      */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

#define FAT_PTR_TYPE          "{ ptr, ptr }"
#define FAT_PTR_ALLOC_SIZE    16
#define STRING_BOUND_TYPE     "i32"
#define STRING_BOUND_WIDTH    32
#define STRING_BOUNDS_STRUCT  "{ i32, i32 }"
#define STRING_BOUNDS_ALLOC   8

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.4  IEEE Floating-Point Model                                                           */
/*                                                                                           */
/*  Model parameters for FLOAT (single) and LONG_FLOAT (double) as defined by                */
/*  Ada RM 3.5.8.  The code generator uses these when emitting attribute references           */
/*  such as T'Digits, T'Machine_Mantissa, and T'Model_Emin.                                  */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

#define IEEE_FLOAT_DIGITS        6
#define IEEE_DOUBLE_DIGITS       15
#define IEEE_FLOAT_MANTISSA      24
#define IEEE_DOUBLE_MANTISSA     53
#define IEEE_FLOAT_EMAX          128
#define IEEE_DOUBLE_EMAX         1024
#define IEEE_FLOAT_EMIN          (-125)
#define IEEE_DOUBLE_EMIN         (-1021)
#define IEEE_MACHINE_RADIX       2
#define IEEE_DOUBLE_MIN_NORMAL   2.2250738585072014e-308    /* 2^(-1022) */
#define IEEE_FLOAT_MIN_NORMAL    1.1754943508222875e-38     /* 2^(-126)  */
#define LOG2_OF_10               3.321928094887362

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.5  Subsystem Capacities                                                                */
/*                                                                                           */
/*  These capacity constants are deliberately generous.  Exceeding them signals              */
/*  a pathological program rather than a compiler limitation.                                */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

enum { Default_Chunk_Size = 1 << 24 };
/*  Memory arena chunk size: 16 MiB keeps the number of malloc calls low.                    */

#define SYMBOL_TABLE_SIZE        1024
/*  Hash table width for the symbol table (Ch. 11).  1024 buckets covers                    */
/*  most programs without excessive chaining.                                                */

#define MAX_INTERPRETATIONS      64
/*  Maximum simultaneous overload interpretations during name resolution.                    */
/*  Sixty-four suffices; deeper ambiguity signals a pathological program.                    */

#define ALI_VERSION              "Ada83 1.0 built " __DATE__ " " __TIME__
/*  Version stamp written into Ada Library Information files (Ch. 14).                       */

#define ELAB_MAX_VERTICES        512
#define ELAB_MAX_EDGES           2048
#define ELAB_MAX_COMPONENTS      256
/*  Elaboration graph capacities (Ch. 15).  These bound the number of                       */
/*  compilation units, dependency edges, and strongly-connected components                   */
/*  the elaboration pass can handle in a single closure.                                     */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.6  Build-in-Place Formal Names                                                         */
/*                                                                                           */
/*  The BIP protocol (Ch. 13) passes extra implicit formals whose linker names               */
/*  must follow the conventional naming so that mixed-language linking works.                 */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

#define BIP_ALLOC_NAME           "__BIPalloc"
#define BIP_ACCESS_NAME          "__BIPaccess"
#define BIP_MASTER_NAME          "__BIPmaster"
#define BIP_CHAIN_NAME           "__BIPchain"
#define BIP_FINAL_NAME           "__BIPfinal"

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.7  Code Generator Capacities                                                           */
/*                                                                                           */
/*  Ring-buffer and array sizes for the LLVM IR emitter (Ch. 13).                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

#define TEMP_TYPE_CAPACITY       4096
/*  Temp register type slots in the ring buffer.                                             */

#define EXC_REF_CAPACITY         512
/*  Maximum distinct exception references per compilation unit.                              */

#define MAX_AGG_DIMS             8
/*  Maximum array dimensions tracked during aggregate generation.                            */

#define MAX_DISC_CACHE           16
/*  Discriminant value cache depth for record variant dispatch.                              */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.8  Runtime Check Flags                                                                 */
/*                                                                                           */
/*  Each bit controls a category of runtime check suppressible via pragma Suppress            */
/*  (RM 11.5).  The flags are stored in Type_Info and Symbol so that suppression              */
/*  is inherited through derivation.                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.9  Platform Detection                                                                  */
/*                                                                                           */
/*  Selects SIMD fast paths at compile time (Ch. 18).  The x86-64 path uses                  */
/*  AVX-512, AVX2, or SSE4.2 depending on what the toolchain supports; ARM64                 */
/*  uses NEON; everything else gets a scalar fallback that produces identical                 */
/*  results on every platform.                                                               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

#if defined (__x86_64__) || defined (_M_X64)
  #define SIMD_X86_64 1
#elif defined (__aarch64__) || defined (_M_ARM64)
  #define SIMD_ARM64 1
#else
  #define SIMD_GENERIC 1
#endif

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  1.10  Safe Character Classification                                                      */
/*                                                                                           */
/*  The C library ctype functions take int and require unsigned-char arguments to             */
/*  avoid undefined behaviour on platforms where plain char is signed.  These                 */
/*  wrappers perform the cast once so that callers need not think about it.                   */
/*                                                                                           */
/*  The identifier-character table encodes Ada RM 2.3 as a 256-byte bitmap:                  */
/*  ASCII letters, digits, underscore, and Latin-1 letters in the ranges                     */
/*  0xC0-0xD6, 0xD8-0xF6, 0xF8-0xFF.  The multiplication sign (0xD7) and                    */
/*  division sign (0xF7) are explicitly excluded.                                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

int  Is_Alpha  (char ch);
int  Is_Digit  (char ch);
int  Is_Xdigit (char ch);
int  Is_Space  (char ch);
char To_Lower  (char ch);

extern const uint8_t Id_Char_Table[256];
#define Is_Id_Char(ch) (Id_Char_Table[(uint8_t)(ch)])

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 2.   MEASUREMENT                                                                 */
/*                                                                                           */
/*  Before any data structure can be laid out in memory, the compiler must answer             */
/*  a prior question: how wide is this value?  The functions in this chapter convert          */
/*  between bits and bytes, select the narrowest LLVM integer type for a given                */
/*  width, and determine whether a range of values fits within a given number of              */
/*  signed or unsigned bits.  These are the rulers and callipers of the compiler.             */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  2.1  Bit/Byte Conversions                                                                */
/*                                                                                           */
/*  To_Bits and To_Bytes form a Galois connection between the bit and byte                   */
/*  domains: To_Bits is multiplicative (total, never truncates), while To_Bytes               */
/*  is a ceiling division that rounds up to the next whole byte.  Byte_Align                  */
/*  is their composition: round a bit count up to a byte boundary.                           */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

uint64_t    To_Bits              (uint64_t bytes);
uint64_t    To_Bytes             (uint64_t bits);
uint64_t    Byte_Align           (uint64_t bits);
size_t      Align_To             (size_t size, size_t alignment);
/*  Round size up to the next multiple of alignment.                                         */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  2.2  LLVM Type Selection                                                                 */
/*                                                                                           */
/*  Given a bit width, return the smallest LLVM integer (or float) type that                 */
/*  can hold that many bits.  These are pure functions of their argument --                   */
/*  they consult no global state.                                                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

const char *Llvm_Int_Type        (uint32_t bits);
const char *Llvm_Float_Type      (uint32_t bits);
bool        Llvm_Type_Is_Pointer     (const char *llvm_type);
bool        Llvm_Type_Is_Fat_Pointer (const char *llvm_type);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  2.3  Range Predicates                                                                    */
/*                                                                                           */
/*  All bounds are int128_t / uint128_t so that the full Ada 2022 integer range               */
/*  is representable.  These predicates drive the code generator's choice of                  */
/*  LLVM integer widths and sign-extension operations.                                       */
/*                                                                                           */
/*  Fits_In_Signed asks: does [lo..hi] lie within the signed range of N bits?                */
/*  Fits_In_Unsigned asks the same for the unsigned range.  Bits_For_Range                   */
/*  returns the smallest standard width (8, 16, 32, 64, or 128) that covers                  */
/*  the range.  Bits_For_Modulus does likewise for a modular type whose range                 */
/*  is 0 .. modulus-1.                                                                       */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

bool        Fits_In_Signed       (int128_t lo, int128_t hi, uint32_t bits);
bool        Fits_In_Unsigned     (int128_t lo, int128_t hi, uint32_t bits);
uint32_t    Bits_For_Range       (int128_t lo, int128_t hi);
uint32_t    Bits_For_Modulus     (uint128_t modulus);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 3.   MEMORY                                                                      */
/*                                                                                           */
/*  A compiler's memory regime differs from that of a long-running server: all               */
/*  allocations share a single lifetime -- the compilation -- and are freed in                */
/*  one shot at the end.  The bump allocator here exploits that invariant.                    */
/*  Individual frees are neither needed nor supported.  Chunks are 16 MiB by                 */
/*  default (see Default_Chunk_Size in Ch. 1); oversized requests get their                  */
/*  own chunk.  All returned pointers are 16-byte aligned.                                   */
/*                                                                                           */
/* ========================================================================================= */

typedef struct Arena_Chunk Arena_Chunk;
struct Arena_Chunk {
  Arena_Chunk *previous;        /*  Singly-linked list of chunks             */
  char        *base;            /*  First usable byte in this chunk          */
  char        *current;         /*  Next free byte (the bump pointer)        */
  char        *end;             /*  One past the last usable byte            */
};

typedef struct {
  Arena_Chunk *head;            /*  Most recently allocated chunk             */
  size_t       chunk_size;      /*  Minimum allocation granularity           */
} Memory_Arena;

extern Memory_Arena Global_Arena;

void *Arena_Allocate (size_t size);
/*  Allocate `size' bytes from the global arena, zero-filled and 16-byte aligned.            */

void  Arena_Free_All (void);
/*  Release every chunk in one operation.  Called once, at the end of main.                   */

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 4.   TEXT                                                                        */
/*                                                                                           */
/*  The fundamental textual unit of the compiler is the String_Slice: a non-owning            */
/*  (pointer, length) view into the source buffer or the arena.  No null terminator           */
/*  is required or expected.  Because Ada identifiers are case-insensitive                   */
/*  (RM 2.3), comparison and hashing fold to lower case.                                     */
/*                                                                                           */
/*  The S() macro constructs a compile-time slice from a C string literal,                   */
/*  computing the length at compile time via sizeof.  It is the closest C comes               */
/*  to an Ada string aggregate.                                                              */
/*                                                                                           */
/* ========================================================================================= */

typedef struct {
  const char *data;             /*  Points into the source buffer or arena   */
  uint32_t    length;           /*  Number of characters in the view         */
} String_Slice;

#define S(literal) \
  ((String_Slice){ .data = (literal), .length = sizeof (literal) - 1 })

extern const String_Slice Empty_Slice;
/*  The zero-length slice, used as a sentinel throughout.                                    */

String_Slice Slice_From_Cstring      (const char *source);
String_Slice Slice_Duplicate         (String_Slice slice);
/*  Allocate a fresh copy on the arena.                                                      */

bool         Slice_Equal             (String_Slice left, String_Slice right);
bool         Slice_Equal_Ignore_Case (String_Slice left, String_Slice right);

uint64_t     Slice_Hash              (String_Slice slice);
/*  FNV-1a hash with case folding, used by the symbol table (Ch. 11).                        */

int          Edit_Distance           (String_Slice left, String_Slice right);
/*  Levenshtein distance for "did you mean ...?" diagnostic suggestions.                     */

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 5.   PROVENANCE                                                                  */
/*                                                                                           */
/*  Every token, syntax node, and symbol carries a Source_Location so that error              */
/*  messages can point the programmer at the exact file, line, and column where               */
/*  the problem was detected.  No_Location is a sentinel for compiler-generated               */
/*  constructs that have no corresponding source text.                                       */
/*                                                                                           */
/*  Errors are accumulated rather than triggering an immediate abort, so the                  */
/*  compiler can report multiple issues in a single invocation.  Error_Count                  */
/*  is checked after each phase to decide whether to proceed.  Fatal_Error                   */
/*  is reserved for internal consistency violations that make further progress                */
/*  impossible -- it prints a diagnostic and calls exit(1).                                  */
/*                                                                                           */
/* ========================================================================================= */

typedef struct {
  const char *filename;         /*  Path of the source file                  */
  uint32_t    line;             /*  One-based line number                    */
  uint32_t    column;           /*  One-based column number                  */
} Source_Location;

extern const Source_Location No_Location;

extern int Error_Count;

void Report_Error (Source_Location location, const char *format, ...);

__attribute__ ((noreturn))
void Fatal_Error  (Source_Location location, const char *format, ...);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 6.   ARITHMETIC                                                                  */
/*                                                                                           */
/*  Ada numeric literals can exceed the 64-bit range: modular types allow moduli              */
/*  up to 2**128, and universal_integer expressions are unbounded.  This chapter              */
/*  provides three levels of exact arithmetic:                                               */
/*                                                                                           */
/*    Big_Integer   Arbitrary-precision integers stored as little-endian arrays               */
/*                  of 64-bit limbs.  The operations needed for literal parsing               */
/*                  are: construction from a based or decimal string, multiply-add            */
/*                  by a small constant, comparison, full multiplication, division            */
/*                  with remainder, and greatest common divisor.                              */
/*                                                                                           */
/*    Big_Real      A significand (Big_Integer) paired with a power-of-ten exponent.          */
/*                  Real literals are represented exactly until the code generator            */
/*                  rounds to float or double at the point of use.                           */
/*                                                                                           */
/*    Rational      An exact quotient of two Big_Integers, always reduced by GCD.             */
/*                  The semantic pass uses Rationals for constant folding of                  */
/*                  fixed-point arithmetic, where intermediate results must be               */
/*                  carried without rounding until the final conversion.                     */
/*                                                                                           */
/*  All storage is arena-allocated (Ch. 3), so there is no explicit deallocation.             */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  6.1  Big_Integer                                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  uint64_t *limbs;              /*  Little-endian array of 64-bit digits     */
  uint32_t  count;              /*  Number of active limbs                   */
  uint32_t  capacity;           /*  Allocated limb slots                     */
  bool      is_negative;        /*  Sign flag; magnitude is always positive  */
} Big_Integer;

Big_Integer *Big_Integer_New             (uint32_t capacity);
Big_Integer *Big_Integer_Clone           (const Big_Integer *source);
Big_Integer *Big_Integer_One             (void);

void         Big_Integer_Ensure_Capacity (Big_Integer *integer, uint32_t needed);
void         Big_Integer_Normalize       (Big_Integer *integer);
/*  Strip leading zero limbs and canonicalise the sign.                                      */

void         Big_Integer_Mul_Add_Small   (Big_Integer *integer,
                                          uint64_t factor, uint64_t addend);
/*  In-place: integer = integer * factor + addend.  The core accumulation                    */
/*  loop for building a big integer one digit at a time.                                     */

int          Big_Integer_Compare         (const Big_Integer *left,
                                          const Big_Integer *right);
/*  Sign-aware comparison.  Returns -1, 0, or +1.                                            */

Big_Integer *Big_Integer_Add             (const Big_Integer *left,
                                          const Big_Integer *right);
Big_Integer *Big_Integer_Multiply        (const Big_Integer *left,
                                          const Big_Integer *right);

void         Big_Integer_Div_Rem         (const Big_Integer *dividend,
                                          const Big_Integer *divisor,
                                          Big_Integer **quotient,
                                          Big_Integer **remainder);
/*  Schoolbook long division (Knuth Algorithm D for multi-limb divisors).                    */

Big_Integer *Big_Integer_GCD             (const Big_Integer *left,
                                          const Big_Integer *right);
/*  Euclidean algorithm on magnitudes; used to reduce Rationals.                             */

Big_Integer *Big_Integer_From_Decimal_SIMD (const char *text);
/*  Parse a decimal digit string (with optional leading sign) using SIMD                     */
/*  acceleration where available.                                                            */

bool         Big_Integer_Fits_Int64      (const Big_Integer *integer,
                                          int64_t *out);
bool         Big_Integer_To_Uint128      (const Big_Integer *integer,
                                          uint128_t *out);
bool         Big_Integer_To_Int128       (const Big_Integer *integer,
                                          int128_t *out);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  6.2  Big_Real                                                                            */
/*                                                                                           */
/*  A real literal is significand * 10^exponent.  The literal 3.14159 becomes                 */
/*  significand = 314159, exponent = -5.  This representation is exact; rounding              */
/*  happens only when converting to a machine float.                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  Big_Integer *significand;     /*  The unscaled digits of the literal       */
  int32_t      exponent;        /*  Power-of-ten scale factor                */
} Big_Real;

Big_Real *Big_Real_New         (void);
Big_Real *Big_Real_From_String (const char *text);

double    Big_Real_To_Double   (const Big_Real *real);
bool      Big_Real_Fits_Double (const Big_Real *real);

void      Big_Real_To_Hex     (const Big_Real *real,
                                char *buffer, size_t buffer_size);
/*  Emit the LLVM IR hex-float representation (e.g. 0x3FF0000000000000).                     */

int       Big_Real_Compare     (const Big_Real *left, const Big_Real *right);
Big_Real *Big_Real_Add_Sub     (const Big_Real *left,
                                const Big_Real *right, bool subtract);
Big_Real *Big_Real_Scale       (const Big_Real *real, int32_t scale);
Big_Real *Big_Real_Divide_Int  (const Big_Real *dividend, int64_t divisor);
Big_Real *Big_Real_Multiply    (const Big_Real *left, const Big_Real *right);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  6.3  Rational                                                                            */
/*                                                                                           */
/*  A rational number is numerator / denominator, always reduced by GCD so that               */
/*  the fraction is in lowest terms.  The denominator is always positive; the                 */
/*  sign lives on the numerator.                                                             */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 7.   LEXICAL ANALYSIS                                                            */
/*                                                                                           */
/*  A token is the smallest meaningful unit of Ada source text.  Token_Kind                   */
/*  enumerates every lexeme in the Ada 83 grammar: identifiers, numeric and                  */
/*  string literals, delimiters, operator symbols, and the sixty-three reserved               */
/*  words of RM 2.9.                                                                         */
/*                                                                                           */
/*  The Token record carries the kind, source location, raw text, and -- for                  */
/*  numeric literals -- both a machine-width parsed value and an optional                    */
/*  arbitrary-precision value for literals that overflow 64 bits.                             */
/*                                                                                           */
/*  The Lexer is a cursor over the source buffer that produces tokens on                     */
/*  demand.  It owns no allocations; all string data points into the original                */
/*  source text.  SIMD fast paths for whitespace skipping, identifier scanning,              */
/*  and digit scanning are declared in Chapter 18.                                           */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  7.1  Token Kinds                                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

  /* Reserved words -- the sixty-three of Ada 83 (RM 2.9) */
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
/*  Printable name for each token kind, used in diagnostic messages.                         */

Token_Kind Lookup_Keyword (String_Slice name);
/*  Search the reserved-word table.  Returns TK_IDENTIFIER if no match.                     */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  7.2  Token Record                                                                        */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  Token_Kind      kind;         /*  What sort of lexeme this is              */
  Source_Location  location;    /*  Where it appeared in the source          */
  String_Slice    text;         /*  The raw source text of the token         */
  union { int64_t integer_value; double float_value; };
  /*  Machine-width literal value, valid for TK_INTEGER / TK_REAL.                           */
  union { Big_Integer *big_integer; Big_Real *big_real; };
  /*  Arbitrary-precision value when the literal overflows 64 bits.                           */
} Token;

Token Make_Token (Token_Kind kind, Source_Location location, String_Slice text);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  7.3  Lexer State                                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  const char *source_start;     /*  Beginning of the source buffer           */
  const char *current;          /*  Read cursor -- advances as we scan       */
  const char *source_end;       /*  One past the last source character       */
  const char *filename;         /*  Source file name for error reporting      */
  uint32_t    line;             /*  Current line, one-based                  */
  uint32_t    column;           /*  Current column, one-based                */
  Token_Kind  prev_token_kind;  /*  Disambiguates tick vs. attribute         */
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

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 8.   SYNTAX                                                                      */
/*                                                                                           */
/*  The abstract syntax tree is the central data structure of the compiler.  Every            */
/*  syntactic construct in Ada 83 maps to a Node_Kind enumerator; the Syntax_Node             */
/*  record carries the kind, source location, a type annotation set during semantic           */
/*  analysis (Ch. 12), a symbol link set during name resolution (Ch. 11), and a              */
/*  payload union discriminated by the kind tag.                                             */
/*                                                                                           */
/*  Three types are mutually recursive -- Syntax_Node, Type_Info, and Symbol --               */
/*  because a syntax node references a type and a symbol, which may in turn                  */
/*  reference syntax nodes.  Forward declarations break the cycle.                           */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  8.1  Forward Declarations                                                                */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct Syntax_Node Syntax_Node;
typedef struct Type_Info   Type_Info;
typedef struct Symbol      Symbol;
typedef struct Scope       Scope;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  8.2  Node List                                                                           */
/*                                                                                           */
/*  A growable array of Syntax_Node pointers, used for statement lists, declaration           */
/*  lists, parameter lists, and similar sequences.  All storage is arena-allocated;           */
/*  doubling gives amortised O(1) append.                                                    */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  Syntax_Node **items;          /*  Array of node pointers                   */
  uint32_t      count;          /*  Number of active entries                 */
  uint32_t      capacity;       /*  Allocated slots                          */
} Node_List;

void Node_List_Push (Node_List *list, Syntax_Node *node);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  8.3  Node Kinds                                                                          */
/*                                                                                           */
/*  One enumerator per syntactic construct.  The grouping follows the RM chapter              */
/*  structure: primaries, expressions, type definitions, statements, declarations,            */
/*  and generic formals.                                                                     */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum {

  /* Literals and primaries */
  NK_INTEGER, NK_REAL, NK_STRING, NK_CHARACTER, NK_NULL, NK_OTHERS,
  NK_IDENTIFIER, NK_SELECTED, NK_ATTRIBUTE, NK_QUALIFIED,

  /* Expressions */
  NK_BINARY_OP, NK_UNARY_OP, NK_AGGREGATE, NK_ALLOCATOR,
  NK_APPLY,                     /*  Unified: call, index, slice, conversion  */
  NK_RANGE,                     /*  low .. high                              */
  NK_ASSOCIATION,               /*  name => value                            */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  8.4  Syntax Node Record                                                                  */
/*                                                                                           */
/*  The payload union is large but flat: each node kind activates exactly one                  */
/*  anonymous struct member.  The tag field is `kind'; no other discriminant is               */
/*  needed.  The comments name the Node_Kind(s) that each union member serves.               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

struct Syntax_Node {
  Node_Kind        kind;        /*  Discriminant tag for the payload union   */
  Source_Location   location;   /*  Where this construct appeared            */
  Type_Info       *type;        /*  Set by semantic analysis, NULL before    */
  Symbol          *symbol;      /*  Set by name resolution, NULL before      */

  union {

    /* NK_INTEGER */
    struct { int64_t value; Big_Integer *big_value; }              integer_lit;

    /* NK_REAL */
    struct { double value; Big_Real *big_value; }                  real_lit;

    /* NK_STRING, NK_CHARACTER, NK_IDENTIFIER */
    struct { String_Slice text; }                                  string_val;

    /* NK_SELECTED -- prefix.selector */
    struct { Syntax_Node *prefix; String_Slice selector; }        selected;

    /* NK_ATTRIBUTE -- prefix'name(args) */
    struct { Syntax_Node *prefix; String_Slice name;
             Node_List arguments; }                                attribute;

    /* NK_QUALIFIED -- subtype_mark'(expression) */
    struct { Syntax_Node *subtype_mark;
             Syntax_Node *expression; }                            qualified;

    /* NK_BINARY_OP */
    struct { Token_Kind op; Syntax_Node *left;
             Syntax_Node *right; }                                 binary;

    /* NK_UNARY_OP */
    struct { Token_Kind op; Syntax_Node *operand; }               unary;

    /* NK_AGGREGATE */
    struct { Node_List items; bool is_named;
             bool is_parenthesized; }                              aggregate;

    /* NK_ALLOCATOR -- new subtype_mark'(expression) */
    struct { Syntax_Node *subtype_mark;
             Syntax_Node *expression; }                            allocator;

    /* NK_APPLY -- unified: calls, indexing, slicing, conversion */
    struct { Syntax_Node *prefix; Node_List arguments; }          apply;

    /* NK_RANGE -- low .. high */
    struct { Syntax_Node *low; Syntax_Node *high; }               range;

    /* NK_ASSOCIATION -- choices => expression */
    struct { Node_List choices; Syntax_Node *expression; }        association;

    /* NK_SUBTYPE_INDICATION */
    struct { Syntax_Node *subtype_mark;
             Syntax_Node *constraint; }                            subtype_ind;

    /* NK_INDEX_CONSTRAINT */
    struct { Node_List ranges; }                                  index_constraint;

    /* NK_RANGE_CONSTRAINT */
    struct { Syntax_Node *range; }                                range_constraint;

    /* NK_DISCRIMINANT_CONSTRAINT */
    struct { Node_List associations; }                            discriminant_constraint;

    /* NK_DIGITS_CONSTRAINT */
    struct { Syntax_Node *digits_expr;
             Syntax_Node *range; }                                 digits_constraint;

    /* NK_DELTA_CONSTRAINT */
    struct { Syntax_Node *delta_expr;
             Syntax_Node *range; }                                 delta_constraint;

    /* NK_ARRAY_TYPE */
    struct { Node_List indices; Syntax_Node *component_type;
             bool is_constrained; }                                array_type;

    /* NK_RECORD_TYPE */
    struct { Node_List discriminants; Node_List components;
             Syntax_Node *variant_part; bool is_null; }           record_type;

    /* NK_ACCESS_TYPE */
    struct { Syntax_Node *designated; bool is_constant; }         access_type;

    /* NK_DERIVED_TYPE */
    struct { Syntax_Node *parent_type;
             Syntax_Node *constraint; }                            derived_type;

    /* NK_ENUMERATION_TYPE */
    struct { Node_List literals; }                                enum_type;

    /* NK_INTEGER_TYPE */
    struct { Syntax_Node *range; uint128_t modulus;
             bool is_modular; }                                    integer_type;

    /* NK_REAL_TYPE */
    struct { Syntax_Node *precision; Syntax_Node *range;
             Syntax_Node *delta; }                                 real_type;

    /* NK_COMPONENT_DECL */
    struct { Node_List names; Syntax_Node *component_type;
             Syntax_Node *init; }                                  component;

    /* NK_VARIANT_PART */
    struct { String_Slice discriminant; Node_List variants; }     variant_part;

    /* NK_VARIANT */
    struct { Node_List choices; Node_List components;
             Syntax_Node *variant_part; }                          variant;

    /* NK_DISCRIMINANT_SPEC */
    struct { Node_List names; Syntax_Node *disc_type;
             Syntax_Node *default_expr; }                          discriminant;

    /* NK_ASSIGNMENT */
    struct { Syntax_Node *target; Syntax_Node *value; }           assignment;

    /* NK_RETURN */
    struct { Syntax_Node *expression; }                           return_stmt;

    /* NK_IF */
    struct { Syntax_Node *condition; Node_List then_stmts;
             Node_List elsif_parts; Node_List else_stmts; }       if_stmt;

    /* NK_CASE */
    struct { Syntax_Node *expression;
             Node_List alternatives; }                             case_stmt;

    /* NK_LOOP */
    struct { String_Slice label; Symbol *label_symbol;
             Syntax_Node *iteration_scheme;
             Node_List statements; bool is_reverse; }             loop_stmt;

    /* NK_BLOCK */
    struct { String_Slice label; Symbol *label_symbol;
             Node_List declarations; Node_List statements;
             Node_List handlers; }                                 block_stmt;

    /* NK_EXIT */
    struct { String_Slice loop_name; Syntax_Node *condition;
             Symbol *target; }                                     exit_stmt;

    /* NK_GOTO */
    struct { String_Slice name; Symbol *target; }                 goto_stmt;

    /* NK_LABEL */
    struct { String_Slice name; Syntax_Node *statement;
             Symbol *symbol; }                                     label_node;

    /* NK_RAISE */
    struct { Syntax_Node *exception_name; }                       raise_stmt;

    /* NK_ACCEPT */
    struct { String_Slice entry_name; Syntax_Node *index;
             Node_List parameters; Node_List statements;
             Symbol *entry_sym; }                                  accept_stmt;

    /* NK_SELECT */
    struct { Node_List alternatives;
             Syntax_Node *else_part; }                             select_stmt;

    /* NK_DELAY */
    struct { Syntax_Node *expression; }                           delay_stmt;

    /* NK_ABORT */
    struct { Node_List task_names; }                              abort_stmt;

    /* NK_OBJECT_DECL */
    struct { Node_List names; Syntax_Node *object_type;
             Syntax_Node *init; bool is_constant;
             bool is_aliased; bool is_rename; }                   object_decl;

    /* NK_TYPE_DECL */
    struct { String_Slice name; Node_List discriminants;
             Syntax_Node *definition; bool is_limited;
             bool is_private; }                                    type_decl;

    /* NK_EXCEPTION_DECL */
    struct { Node_List names; Syntax_Node *renamed; }             exception_decl;

    /* NK_PROCEDURE_SPEC, NK_FUNCTION_SPEC */
    struct { String_Slice name; Node_List parameters;
             Syntax_Node *return_type;
             Syntax_Node *renamed; }                               subprogram_spec;

    /* NK_PROCEDURE_BODY, NK_FUNCTION_BODY */
    struct { Syntax_Node *specification; Node_List declarations;
             Node_List statements; Node_List handlers;
             bool is_separate; bool code_generated; }             subprogram_body;

    /* NK_PACKAGE_SPEC */
    struct { String_Slice name; Node_List visible_decls;
             Node_List private_decls; }                            package_spec;

    /* NK_PACKAGE_BODY */
    struct { String_Slice name; Node_List declarations;
             Node_List statements; Node_List handlers;
             bool is_separate; }                                   package_body;

    /* NK_PACKAGE_RENAMING */
    struct { String_Slice new_name;
             Syntax_Node *old_name; }                              package_renaming;

    /* NK_TASK_SPEC */
    struct { String_Slice name; Node_List entries;
             bool is_type; }                                       task_spec;

    /* NK_TASK_BODY */
    struct { String_Slice name; Node_List declarations;
             Node_List statements; Node_List handlers;
             bool is_separate; }                                   task_body;

    /* NK_ENTRY_DECL */
    struct { String_Slice name; Node_List parameters;
             Node_List index_constraints; }                        entry_decl;

    /* NK_PARAM_SPEC */
    struct { Node_List names; Syntax_Node *param_type;
             Syntax_Node *default_expr;
             enum { MODE_IN, MODE_OUT, MODE_IN_OUT } mode; }     param_spec;

    /* NK_GENERIC_DECL */
    struct { Node_List formals; Syntax_Node *unit; }              generic_decl;

    /* NK_GENERIC_INST */
    struct { Syntax_Node *generic_name; Node_List actuals;
             String_Slice instance_name;
             Token_Kind unit_kind; }                               generic_inst;

    /* NK_GENERIC_TYPE_PARAM */
    struct { String_Slice name;
             enum { GEN_DEF_PRIVATE = 0, GEN_DEF_LIMITED_PRIVATE,
                    GEN_DEF_DISCRETE, GEN_DEF_INTEGER, GEN_DEF_FLOAT,
                    GEN_DEF_FIXED, GEN_DEF_ARRAY, GEN_DEF_ACCESS,
                    GEN_DEF_DERIVED } def_kind;
             Syntax_Node *def_detail;
             Node_List discriminants; }                            generic_type_param;

    /* NK_GENERIC_OBJECT_PARAM */
    struct { Node_List names; Syntax_Node *object_type;
             Syntax_Node *default_expr;
             enum { GEN_MODE_IN = 0, GEN_MODE_OUT,
                    GEN_MODE_IN_OUT } mode; }                     generic_object_param;

    /* NK_GENERIC_SUBPROGRAM_PARAM */
    struct { String_Slice name; Node_List parameters;
             Syntax_Node *return_type; Syntax_Node *default_name;
             bool is_function; bool default_box; }                generic_subprog_param;

    /* NK_USE_CLAUSE */
    struct { Node_List names; }                                   use_clause;

    /* NK_PRAGMA */
    struct { String_Slice name; Node_List arguments; }            pragma_node;

    /* NK_EXCEPTION_HANDLER */
    struct { Node_List exceptions; Node_List statements; }        handler;

    /* NK_REPRESENTATION_CLAUSE */
    struct { Syntax_Node *entity_name; String_Slice attribute;
             Syntax_Node *expression; Node_List component_clauses;
             bool is_record_rep; bool is_enum_rep; }              rep_clause;

    /* NK_CONTEXT_CLAUSE */
    struct { Node_List with_clauses;
             Node_List use_clauses; }                              context;

    /* NK_COMPILATION_UNIT */
    struct { Syntax_Node *context; Syntax_Node *unit;
             Syntax_Node *separate_parent; }                       compilation_unit;
  };
};

Syntax_Node *Node_New (Node_Kind kind, Source_Location location);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 9.   PARSING                                                                     */
/*                                                                                           */
/*  Recursive descent mirrors the grammar: each nonterminal becomes a function,               */
/*  each alternative becomes a branch.  Three simplifying principles keep the                 */
/*  parser compact:                                                                          */
/*                                                                                           */
/*    1.  All X(...) forms parse as NK_APPLY.  Semantic analysis later                        */
/*        distinguishes calls, indexing, slicing, and type conversions.                       */
/*                                                                                           */
/*    2.  One helper handles positional, named, and choice associations.                      */
/*                                                                                           */
/*    3.  One postfix loop handles .selector, 'attribute, and (args).                         */
/*                                                                                           */
/*  The parser never allocates heap memory directly; all nodes are arena-                     */
/*  allocated through Node_New (Ch. 8).                                                      */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  9.1  Parser State                                                                        */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  Lexer        lexer;           /*  The token source                         */
  Token        current_token;   /*  Lookahead -- the token under the cursor  */
  Token        previous_token;  /*  The most recently consumed token         */
  bool         had_error;       /*  True after any syntax error              */
  bool         panic_mode;      /*  True while synchronising after an error  */
  uint32_t     last_line;       /*  Line of the previous token               */
  uint32_t     last_column;     /*  Column of the previous token             */
  Token_Kind   last_kind;       /*  Kind of the previous token               */
} Parser;

typedef enum {
  PREC_NONE = 0, PREC_LOGICAL, PREC_RELATIONAL, PREC_ADDITIVE,
  PREC_MULTIPLICATIVE, PREC_EXPONENTIAL, PREC_UNARY, PREC_PRIMARY
} Precedence;
/*  Operator precedence levels for Pratt-style expression parsing (RM 4.5).                  */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  9.2  Parser Infrastructure                                                               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

Parser           Parser_New               (const char *source, size_t length,
                                           const char *filename);
bool             Parser_At                (Parser *p, Token_Kind kind);
bool             Parser_At_Any            (Parser *p, Token_Kind k1,
                                           Token_Kind k2);
bool             Parser_Peek_At           (Parser *p, Token_Kind kind);
Token            Parser_Advance           (Parser *p);
bool             Parser_Match             (Parser *p, Token_Kind kind);
bool             Parser_Expect            (Parser *p, Token_Kind kind);
/*  Consume a token of the given kind, or report "expected ..." and return false.             */

Source_Location   Parser_Location          (Parser *p);
String_Slice     Parser_Identifier        (Parser *p);
void             Parser_Error             (Parser *p, const char *message);
void             Parser_Error_At_Current  (Parser *p, const char *expected);
void             Parser_Synchronize       (Parser *p);
/*  Discard tokens until a statement or declaration boundary is found.                       */

bool             Parser_Check_Progress    (Parser *p);
/*  Return false if the parser has not advanced since the last call.                         */

void             Parser_Check_End_Name    (Parser *p, String_Slice expected);
Precedence       Get_Infix_Precedence     (Token_Kind kind);
bool             Is_Right_Associative     (Token_Kind kind);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  9.3  Expressions and Names                                                               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  9.4  Type Definitions                                                                    */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  9.5  Statements                                                                          */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  9.6  Declarations and Packages                                                           */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 10.  TYPES                                                                       */
/*                                                                                           */
/*  Ada's types form a lattice rooted at the universal types.  Boolean and                    */
/*  Character are enumerations; Integer and its derivatives are discrete; Float               */
/*  and Fixed are real; String is a constrained array.  Every type in the program              */
/*  is represented by a Type_Info descriptor that carries the kind, scalar bounds,            */
/*  composite structure, representation size, and a chain to the base type and                */
/*  parent type (for derived types).                                                         */
/*                                                                                           */
/*  Sizes in Type_Info are always BYTES, matching the LLVM DataLayout convention.             */
/*  The only places bit widths appear are in Type_Bound values and IR emission.               */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.1  Type Kinds                                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.2  Scalar Bounds                                                                      */
/*                                                                                           */
/*  Scalar bounds are represented as a tagged union.  BOUND_INTEGER and                       */
/*  BOUND_FLOAT hold compile-time-known values; BOUND_EXPR holds a pointer                   */
/*  to an expression node whose value is only known at runtime (e.g. a                       */
/*  discriminant reference in an array index constraint).                                     */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  enum { BOUND_NONE, BOUND_INTEGER, BOUND_FLOAT, BOUND_EXPR } kind;
  union { int128_t int_value; double float_value; Syntax_Node *expr; };
  uint32_t cached_temp;         /*  LLVM temp register, 0 if not yet emitted */
} Type_Bound;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.3  Composite Descriptors                                                              */
/*                                                                                           */
/*  Discriminated records (RM 3.7) use variant parts to overlay fields                       */
/*  conditionally.  Variant_Info describes one arm of the case; Component_Info                */
/*  describes one field or discriminant.  Index_Info describes one dimension                  */
/*  of an array type.                                                                        */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  int64_t  disc_value_low;      /*  Low bound of the discrete choice         */
  int64_t  disc_value_high;     /*  High bound (equal to low for scalars)    */
  bool     is_others;           /*  True for the "when others" arm           */
  uint32_t first_component;     /*  Index into the component array           */
  uint32_t component_count;     /*  Number of components in this arm         */
  uint32_t variant_size;        /*  Byte size of this variant's fields       */
} Variant_Info;

typedef struct {
  String_Slice  name;           /*  Component name (Ada identifier)          */
  Type_Info    *component_type; /*  Type of the component                    */
  uint32_t      byte_offset;    /*  Offset from the record origin            */
  uint32_t      bit_offset;     /*  Bit offset within the byte               */
  uint32_t      bit_size;       /*  Size in bits (for rep clauses)           */
  Syntax_Node  *default_expr;   /*  Default initializer, or NULL             */
  bool          is_discriminant; /* True for discriminant components         */
  int32_t       variant_index;  /*  -1 if not in a variant part              */
} Component_Info;

typedef struct {
  Type_Info *index_type;        /*  The discrete type of this dimension      */
  Type_Bound low_bound;
  Type_Bound high_bound;
} Index_Info;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.4  Type_Info Record                                                                   */
/*                                                                                           */
/*  The central type descriptor.  Every Ada type and subtype in the program has               */
/*  exactly one Type_Info.  The record is large because Ada types carry a great               */
/*  deal of semantic information: bounds, constraints, component layouts, variant             */
/*  parts, representation attributes, and derivation chains.                                 */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

struct Type_Info {
  Type_Kind    kind;
  String_Slice name;            /*  Source name of the type                  */
  Symbol      *defining_symbol; /*  The SYMBOL_TYPE or SYMBOL_SUBTYPE        */
  uint32_t     size;            /*  Size in BYTES                            */
  uint32_t     alignment;       /*  Alignment in BYTES                       */
  uint32_t     specified_bit_size; /* From a 'Size rep clause, or 0         */
  Type_Bound   low_bound;       /*  Scalar low bound                         */
  Type_Bound   high_bound;      /*  Scalar high bound                        */
  uint128_t    modulus;          /*  For TYPE_MODULAR only                    */
  Type_Info   *base_type;       /*  The base type for a subtype              */
  Type_Info   *parent_type;     /*  The parent type for a derived type       */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.5  Type Construction and Classification                                               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

Type_Info  *Type_New             (Type_Kind kind, String_Slice name);
void        Freeze_Type          (Type_Info *type_info);

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.6  Type Chain Navigation and Bound Queries                                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

Type_Info  *Type_Base                    (Type_Info *t);
Type_Info  *Type_Root                    (Type_Info *t);

int128_t    Type_Bound_Value             (Type_Bound bound);
bool        Type_Bound_Is_Compile_Time_Known (Type_Bound b);
bool        Type_Bound_Is_Set            (Type_Bound b);
double      Type_Bound_Float_Value       (Type_Bound b);
int128_t    Array_Element_Count          (Type_Info *t);
int128_t    Array_Low_Bound              (Type_Info *t);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.7  LLVM Type Mapping                                                                  */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.8  Expression Type Queries                                                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

bool        Expression_Is_Slice              (const Syntax_Node *node);
bool        Expression_Produces_Fat_Pointer  (const Syntax_Node *node,
                                              const Type_Info *type);
bool        Expression_Is_Boolean            (Syntax_Node *node);
bool        Expression_Is_Float              (Syntax_Node *node);
const char *Expression_Llvm_Type             (Syntax_Node *node);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.9  Derived Type Operations                                                            */
/*                                                                                           */
/*  When a derived type is declared (RM 3.4), it inherits primitive subprograms               */
/*  from its parent type.  These functions identify and create the inherited                  */
/*  operations.                                                                              */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

bool        Types_Same_Named         (Type_Info *t1, Type_Info *t2);
bool        Subprogram_Is_Primitive_Of (Symbol *sub, Type_Info *type);
void        Create_Derived_Operation (Symbol *sub, Type_Info *derived_type,
                                      Type_Info *parent_type);
void        Derive_Subprograms      (Type_Info *derived_type,
                                      Type_Info *parent_type);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  10.10  Generic Type Map                                                                  */
/*                                                                                           */
/*  During generic instantiation (Ch. 16), formal type names are mapped to actual             */
/*  types.  This small fixed-size table is set before resolving the expanded                  */
/*  body and cleared afterward.                                                              */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

extern struct {
  uint32_t count;
  struct { String_Slice formal_name; Type_Info *actual_type; } mappings[32];
} g_generic_type_map;

void        Set_Generic_Type_Map        (Symbol *inst);
Type_Info  *Resolve_Generic_Actual_Type (Type_Info *type);
bool        Check_Is_Suppressed         (Type_Info *type, Symbol *sym,
                                         uint32_t check_bit);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 11.  NAMES                                                                       */
/*                                                                                           */
/*  Every named entity in an Ada program -- variable, constant, type, subprogram,             */
/*  package, exception, loop, label -- is represented by a Symbol.  Symbols live              */
/*  in Scopes, which are chained outward from inner to enclosing.  A Scope is a              */
/*  hash table (1024 buckets) of Symbol chains; push and pop operations bracket               */
/*  each declarative region.                                                                 */
/*                                                                                           */
/*  Ada permits overloading of subprograms and enumeration literals (RM 8.3).                 */
/*  The Interpretation and Interp_List types model the set of possible meanings               */
/*  of an overloaded name; filtering and disambiguation narrow the set to a                  */
/*  unique solution or produce an ambiguity error.                                           */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  11.1  Symbol Kinds                                                                       */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum {
  SYMBOL_VARIABLE = 0, SYMBOL_CONSTANT, SYMBOL_TYPE, SYMBOL_SUBTYPE,
  SYMBOL_PROCEDURE, SYMBOL_FUNCTION, SYMBOL_PARAMETER,
  SYMBOL_PACKAGE, SYMBOL_COMPONENT, SYMBOL_LOOP,
  SYMBOL_LABEL, SYMBOL_EXCEPTION, SYMBOL_ENTRY,
  SYMBOL_GENERIC_PACKAGE, SYMBOL_GENERIC_PROCEDURE, SYMBOL_GENERIC_FUNCTION,
  SYMBOL_NAMED_NUMBER, SYMBOL_ENUM_LITERAL,
  SYMBOL_TASK, SYMBOL_TASK_TYPE,
  SYMBOL_COUNT
} Symbol_Kind;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  11.2  Symbol Record                                                                      */
/*                                                                                           */
/*  The Symbol record gathers everything the compiler knows about a named                    */
/*  entity.  It is large because Ada names carry rich semantics: a subprogram                 */
/*  symbol holds parameter types, a package symbol holds exported members, a                 */
/*  generic symbol holds the template AST and the list of formal-to-actual                   */
/*  bindings.                                                                                */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

struct Symbol {
  Symbol_Kind   kind;           /*  What this symbol represents              */
  String_Slice  name;           /*  Source name (case-preserved)             */
  Source_Location location;     /*  Where it was declared                    */
  Type_Info    *type;           /*  Type of the entity                       */
  uint32_t      unique_id;      /*  Disambiguation tag for mangling          */
  Scope        *scope;          /*  Scope where this symbol was declared     */
  Symbol       *next;           /*  Hash chain link in the scope bucket      */

  /* Subprogram profile */
  Type_Info   **param_types;    /*  Array of formal parameter types          */
  uint32_t      param_count;
  Type_Info    *return_type;    /*  NULL for procedures                      */
  Syntax_Node **param_nodes;    /*  AST nodes of the formal parameters       */
  Syntax_Node  *body_node;      /*  AST of the body, if available            */
  Syntax_Node  *spec_node;      /*  AST of the specification                 */

  /* Package exports */
  Symbol      **exports;        /*  Array of symbols visible outside         */
  uint32_t      export_count;

  /* Object storage */
  int64_t       stack_offset;   /*  Byte offset in the enclosing frame       */
  uint32_t      llvm_temp;      /*  Temp register holding the alloca         */
  uint32_t      llvm_global_id; /*  Global variable slot in the .ll file     */
  bool          is_global;
  bool          is_formal_param;
  bool          is_by_reference;
  bool          is_constrained;
  bool          needs_alloca;

  /* Named numbers and constants */
  int128_t      constant_value;
  double        float_constant;
  bool          has_constant_value;
  Rational      rational_value;
  bool          has_rational_value;

  /* Representation and pragma attributes */
  String_Slice  external_name;  /*  pragma Import link name                  */
  String_Slice  link_name;
  enum { CONVENTION_ADA = 0, CONVENTION_C, CONVENTION_STDCALL,
         CONVENTION_INTRINSIC, CONVENTION_ASSEMBLER } convention;
  uint32_t      suppressed_checks;
  bool          is_unreferenced;

  /* Code-generation flags */
  bool          extern_emitted;
  bool          body_emitted;
  bool          is_named_number;
  bool          is_overloaded;
  bool          body_claimed;
  bool          is_predefined;
  bool          needs_address_marker;
  bool          is_identity_function;
  uint32_t      disc_agg_temp;
  bool          is_disc_constrained;
  bool          needs_fat_ptr_storage;

  /* Derived type operations (RM 3.4) */
  Symbol       *parent_operation;
  Type_Info    *derived_from_type;

  /* Labels and entries */
  uint32_t      llvm_label_id;
  uint32_t      loop_exit_label_id;
  uint32_t      entry_index;
  Syntax_Node  *renamed_object;

  /* Generic support */
  Syntax_Node  *generic_formals;
  Syntax_Node  *generic_unit;
  Syntax_Node  *generic_body;
  Symbol       *generic_template;
  Symbol       *instantiated_subprogram;
  struct {
    String_Slice  formal_name;
    Type_Info    *actual_type;
    Symbol       *actual_subprogram;
    Syntax_Node  *actual_expr;
    Token_Kind    builtin_operator;
  } *generic_actuals;
  uint32_t      generic_actual_count;
  Syntax_Node  *expanded_spec;
  Syntax_Node  *expanded_body;
};

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  11.3  Scope                                                                              */
/*                                                                                           */
/*  A scope is a hash table of symbols together with metadata about the                      */
/*  enclosing subprogram's stack frame.  Scopes are pushed when entering a                   */
/*  declarative region and popped when leaving.  The flat symbol list enables                 */
/*  iteration without walking all 1024 hash buckets.                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

struct Scope {
  Symbol   *buckets[SYMBOL_TABLE_SIZE];
  Scope    *parent;             /*  Lexically enclosing scope                */
  Symbol   *owner;              /*  The subprogram or package that owns it   */
  uint32_t  nesting_level;      /*  Depth from the global scope              */
  Symbol  **symbols;            /*  Flat list of all symbols in this scope   */
  uint32_t  symbol_count;
  uint32_t  symbol_capacity;
  int64_t   frame_size;         /*  Running total of local variable bytes    */
  Symbol  **frame_vars;         /*  Variables needing stack frame slots       */
  uint32_t  frame_var_count;
  uint32_t  frame_var_capacity;
};

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  11.4  Symbol Manager                                                                     */
/*                                                                                           */
/*  The Symbol_Manager holds the global and current scopes, pointers to the                  */
/*  predefined types from package Standard, and the unique-ID counter for                    */
/*  symbol mangling.  There is exactly one instance, pointed to by `sm'.                     */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  11.5  Scope and Symbol Lifecycle                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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
/*  Install Boolean, Integer, Float, Character, String, and the                              */
/*  universal types into the global scope.                                                   */

void     Symbol_Manager_Init             (void);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  11.6  Overload Resolution                                                                */
/*                                                                                           */
/*  Ada allows subprogram names and enumeration literals to be overloaded                    */
/*  (RM 8.3).  Resolution collects all visible interpretations, filters                      */
/*  by argument profile, and selects the unique match whose result type                      */
/*  agrees with the context.  When no unique match exists, the call is                       */
/*  ambiguous.                                                                               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  Type_Info    **types;
  uint32_t       count;
  String_Slice  *names;
} Argument_Info;

typedef struct {
  Symbol    *nam;               /*  Candidate symbol                         */
  Type_Info *typ;               /*  Result type                              */
  Type_Info *opnd_typ;          /*  Operand type (for operator resolution)   */
  bool       is_universal;      /*  True if from a universal context         */
  uint32_t   scope_depth;       /*  Distance from use to declaration         */
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

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 12.  SEMANTICS                                                                   */
/*                                                                                           */
/*  The semantic pass walks the syntax tree to resolve identifiers to their                   */
/*  declarations, check type compatibility, fold static expressions to compile-               */
/*  time constants, and freeze type representations.  It bridges parsing (which               */
/*  knows only syntax) and code generation (which needs fully resolved types                 */
/*  and symbols).                                                                            */
/*                                                                                           */
/*  Each Resolve_ function receives a syntax node and returns the type of the                 */
/*  expression, or NULL on error.  Declaration processing installs new symbols               */
/*  and types into the current scope; statement processing checks legality rules             */
/*  and resolves inner expressions.                                                          */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  12.1  Expression Resolution                                                              */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

Type_Info   *Resolve_Expression          (Syntax_Node *node);
Type_Info   *Resolve_Identifier          (Syntax_Node *node);
Type_Info   *Resolve_Selected            (Syntax_Node *node);
Type_Info   *Resolve_Binary_Op           (Syntax_Node *node);
Type_Info   *Resolve_Apply               (Syntax_Node *node);
/*  Resolve a unified NK_APPLY node: distinguish calls, indexing,                            */
/*  slicing, and type conversions based on the resolved prefix.                              */

bool         Resolve_Char_As_Enum        (Syntax_Node *char_node,
                                          Type_Info *enum_type);
/*  Attempt to interpret a character literal as an enumeration                                */
/*  literal of the given type (RM 4.2).                                                      */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  12.2  Statements and Declarations                                                        */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

void         Resolve_Statement           (Syntax_Node *node);
void         Resolve_Declaration         (Syntax_Node *node);
void         Resolve_Declaration_List    (Node_List *list);
void         Resolve_Statement_List      (Node_List *list);
void         Resolve_Compilation_Unit    (Syntax_Node *node);

void         Freeze_Declaration_List     (Node_List *list);
/*  Freeze every type declared in the list.  Called at the end of each                       */
/*  declarative region before the statement part begins.                                     */

void         Populate_Package_Exports    (Symbol *pkg_sym,
                                          Syntax_Node *pkg_spec);
void         Preregister_Labels          (Node_List *list);
/*  Scan a statement list for labels and install them before processing                      */
/*  statements, because forward gotos are legal (RM 5.9).                                    */

void         Install_Declaration_Symbols (Node_List *decls);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  12.3  Constant Folding                                                                   */
/*                                                                                           */
/*  The constant folder evaluates static expressions at compile time.  It is                  */
/*  needed for range bounds, discriminant constraints, representation clauses,                */
/*  and named numbers -- all of which must be known before code generation.                  */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

bool         Is_Integer_Expr             (Syntax_Node *node);
double       Eval_Const_Numeric          (Syntax_Node *node);
bool         Eval_Const_Rational         (Syntax_Node *node, Rational *out);

const char  *I128_Decimal                (int128_t value);
const char  *U128_Decimal                (uint128_t value);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 13.  CODE GENERATION                                                             */
/*                                                                                           */
/*  The code generator walks the resolved syntax tree and emits LLVM IR as                    */
/*  plain text to a FILE*.  Every Ada construct maps to a sequence of LLVM                    */
/*  instructions.  The generator tracks temp registers (SSA values), labels,                  */
/*  string constants, and deferred nested subprogram bodies.                                 */
/*                                                                                           */
/*  The Code_Generator record holds all mutable state for one compilation                    */
/*  unit.  There is exactly one instance, pointed to by `cg'.                                */
/*                                                                                           */
/*  Build-in-place (BIP) support handles Ada limited types that cannot be                    */
/*  copied: functions returning them build the result directly in the                        */
/*  caller's storage via implicit extra formals.                                             */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.1  Code_Generator State                                                               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  FILE         *output;         /*  The .ll file being written               */
  uint32_t      temp_id;        /*  Next SSA temp register number            */
  uint32_t      label_id;       /*  Next basic-block label number            */
  uint32_t      global_id;      /*  Next global variable number              */
  uint32_t      string_id;      /*  Next string constant number              */
  Symbol       *current_function;
  uint32_t      current_nesting_level;
  Symbol       *current_instance;  /*  Current generic instance, if any      */
  uint32_t      loop_exit_label;   /*  Target label for exit statements      */
  uint32_t      loop_continue_label;
  bool          has_return;     /*  True if current block ends with ret      */
  bool          block_terminated;
  bool          header_emitted;
  Symbol       *main_candidate; /*  The entry point, if one was found        */
  Syntax_Node  *deferred_bodies[64];
  uint32_t      deferred_count; /*  Nested bodies to emit after the parent   */
  Symbol       *enclosing_function;
  bool          is_nested;      /*  True inside a nested subprogram          */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.2  Lifecycle                                                                          */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

void Code_Generator_Init (FILE *output);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.3  Temp and Label Management                                                          */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

uint32_t    Emit_Temp                    (void);
uint32_t    Emit_Label                   (void);
void        Temp_Set_Type                (uint32_t temp_id,
                                          const char *llvm_type);
const char *Temp_Get_Type                (uint32_t temp_id);
void        Temp_Mark_Fat_Alloca         (uint32_t temp_id);
bool        Temp_Is_Fat_Alloca           (uint32_t temp_id);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.4  IR Emission Primitives                                                             */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

void        Emit                         (const char *format, ...);
void        Emit_Location                (Source_Location location);
void        Emit_Label_Here              (uint32_t label);
void        Emit_Branch_If_Needed        (uint32_t label);
void        Emit_String_Const            (const char *format, ...);
void        Emit_String_Const_Char       (char ch);
void        Emit_Float_Constant          (uint32_t result,
                                          const char *type, double value);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.5  String and Type Helpers                                                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.6  Symbol Mangling                                                                    */
/*                                                                                           */
/*  Ada names are mangled into linker-friendly identifiers: dotted names become               */
/*  underscored, casing is preserved, unique IDs disambiguate overloads.                     */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.7  Static Link Support                                                                */
/*                                                                                           */
/*  Ada allows nested subprograms to access variables of their enclosing                     */
/*  subprograms.  The code generator implements this with static links: each                  */
/*  nested frame carries a pointer to its lexical parent's frame.                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

Symbol      *Find_Enclosing_Subprogram   (Symbol *sym);
bool         Subprogram_Needs_Static_Chain (Symbol *sym);
bool         Is_Uplevel_Access           (const Symbol *sym);
int          Nested_Frame_Depth          (Symbol *proc);
uint32_t     Precompute_Nested_Frame_Arg (Symbol *proc);
bool         Emit_Nested_Frame_Arg       (Symbol *proc, uint32_t precomp);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.8  Checks and Conversions                                                             */
/*                                                                                           */
/*  Runtime checks implement Ada's safety model (RM 11.5).  Each check can be                */
/*  suppressed per-type or per-symbol via the CHK_ flags (Ch. 1).  The                       */
/*  Emit_*_Check functions generate the check-and-raise sequence.                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.9  Fat-Pointer Operations                                                             */
/*                                                                                           */
/*  Fat pointers are the uniform representation for unconstrained arrays passed               */
/*  by reference.  The `bt' parameter is always the LLVM bound type                          */
/*  (e.g. "i32" for String).                                                                 */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.10  Bound and Length Emission                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  uint32_t    low_temp;         /*  Temp holding the low bound               */
  uint32_t    high_temp;        /*  Temp holding the high bound              */
  const char *bound_type;       /*  LLVM type of both bounds                 */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.11  Exception Handling                                                                */
/*                                                                                           */
/*  Ada exceptions are implemented via setjmp/longjmp.  Each handler region                  */
/*  sets up a jump buffer; raising an exception longjmps to the nearest                      */
/*  enclosing handler and dispatches by exception identity.                                  */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  uint32_t handler_frame;       /*  Temp holding the jmp_buf alloca          */
  uint32_t jmp_buf;             /*  Temp holding the setjmp result           */
  uint32_t normal_label;        /*  Label for the non-exception path         */
  uint32_t handler_label;       /*  Label for the exception dispatch         */
} Exception_Setup;

Exception_Setup Emit_Exception_Handler_Setup (void);
uint32_t     Emit_Current_Exception_Id   (void);
void         Generate_Exception_Dispatch (Node_List *handlers,
                                          uint32_t handler_label);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.12  Expression Generation                                                             */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.13  Statement Generation                                                              */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.14  Declaration Generation                                                            */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.15  Aggregate Helpers                                                                 */
/*                                                                                           */
/*  Ada aggregates can be positional, named, or mixed, and may contain "others"              */
/*  choices.  These functions classify, resolve, and emit aggregate components.               */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  uint32_t n_positional;        /*  Number of positional components          */
  bool     has_named;           /*  True if named associations are present   */
  bool     has_others;          /*  True if an "others =>" clause exists     */
  Syntax_Node *others_expr;     /*  The expression for the others clause     */
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

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.16  Top-Level Unit Generation                                                         */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

void         Generate_Type_Equality_Function (Type_Info *t);
void         Generate_Implicit_Operators (void);
void         Generate_Exception_Globals  (void);
void         Generate_Extern_Declarations (Syntax_Node *node);
void         Generate_Compilation_Unit   (Syntax_Node *node);

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  13.17  Build-in-Place (BIP) Protocol                                                     */
/*                                                                                           */
/*  Ada limited types cannot be copied (RM 7.5), so functions returning them                  */
/*  must build the result directly in the caller's storage.  The BIP protocol                 */
/*  passes extra implicit formals that carry the allocation strategy and                     */
/*  destination address.                                                                     */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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

bool         BIP_Is_Explicitly_Limited        (const Type_Info *t);
bool         BIP_Is_Task_Type                 (const Type_Info *t);
bool         BIP_Record_Has_Limited_Component (const Type_Info *t);
bool         BIP_Is_Limited_Type              (const Type_Info *t);
bool         BIP_Is_BIP_Function              (const Symbol *func);
bool         BIP_Type_Has_Task_Component      (const Type_Info *t);
bool         BIP_Needs_Alloc_Form             (const Symbol *func);
uint32_t     BIP_Extra_Formal_Count           (const Symbol *func);
BIP_Alloc_Form BIP_Determine_Alloc_Form      (bool is_allocator,
                                               bool has_pool);
void         BIP_Begin_Function               (const Symbol *func);
bool         BIP_In_BIP_Function              (void);
void         BIP_End_Function                 (void);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 14.  LIBRARY MANAGEMENT                                                          */
/*                                                                                           */
/*  Ada Library Information (ALI) files record dependencies, checksums, and                   */
/*  exported symbols so that the binder can check consistency and the linker                  */
/*  can resolve cross-unit references.  Each ALI file corresponds to one                     */
/*  compilation unit.                                                                        */
/*                                                                                           */
/*  The CRC32 checksum covers the source text so that recompilation can be                   */
/*  skipped when a source file has not changed.                                              */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  14.1  ALI Data Structures                                                                */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

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
  bool         elaborate;       /*  pragma Elaborate was specified            */
  bool         elaborate_all;   /*  pragma Elaborate_All was specified        */
} With_Info;

typedef struct {
  String_Slice source_file;
  uint32_t     timestamp;
  uint32_t     checksum;
} Dependency_Info;

typedef struct {
  String_Slice name;
  String_Slice llvm_name;
  String_Slice type_name;
  bool         is_function;
  bool         is_generic;
  bool         is_type;
  const char  *llvm_type;
} Export_Info;

typedef struct {
  Unit_Info       unit;
  With_Info      *withs;        /*  Array of with-clause records             */
  uint32_t        with_count;
  Dependency_Info *deps;        /*  Array of dependency records              */
  uint32_t        dep_count;
  Export_Info     *exports;      /*  Array of exported symbol records         */
  uint32_t        export_count;
  String_Slice    body_source;
  uint32_t        body_checksum;
} ALI_Info;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  14.2  ALI Cache                                                                          */
/*                                                                                           */
/*  The cache avoids redundant file I/O when the same unit is depended upon                  */
/*  by multiple compilation units in the same session.                                       */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
  String_Slice    unit_name;
  String_Slice    source_name;
  uint32_t        source_checksum;
  bool            is_preelaborate;
  bool            is_pure;
  bool            has_elaboration;
  Export_Info     *exports;
  uint32_t        export_count;
} ALI_Export;

typedef struct {
  String_Slice ali_path;
  ALI_Export   exports;
  bool         loaded;
} ALI_Cache_Entry;

extern ALI_Cache_Entry ALI_Cache[256];
extern uint32_t        ALI_Cache_Count;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  14.3  ALI and CRC Operations                                                             */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

void     Crc32_Init_Table                (void);
uint32_t CRC32_Update                    (uint32_t crc, const void *data,
                                          size_t length);
uint32_t CRC32_File                      (const char *path);

void     ALI_Write                       (const char *path, ALI_Info *ali);
bool     ALI_Read                        (const char *path, ALI_Info *ali);
void     ALI_Collect_Exports             (Syntax_Node *unit,
                                          ALI_Info *ali);
String_Slice Get_Subprogram_Name         (Syntax_Node *node);
String_Slice LLVM_Type_Basic             (String_Slice ada_type);
void     ALI_Register_Dependency         (ALI_Info *ali, String_Slice name,
                                          const char *source_file);
void     ALI_Build_Info                  (Syntax_Node *comp_unit,
                                          const char *source_file,
                                          ALI_Info *ali);
void     ALI_Write_For_Unit              (Syntax_Node *comp_unit,
                                          const char *source_file);
bool     ALI_Load_Exports                (const char *ali_path,
                                          ALI_Export *out);
ALI_Export *ALI_Get_Cached_Exports       (String_Slice unit_name);
void     ALI_Install_Exports_Into_Scope  (ALI_Export *exports, Scope *scope);
char    *ALI_Find                        (String_Slice unit_name);
void     ALI_Load_Symbols               (ALI_Cache_Entry *entry);
bool     Try_Load_From_ALI              (String_Slice name);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 15.  ELABORATION                                                                 */
/*                                                                                           */
/*  Ada requires that library-level packages be elaborated in an order consistent             */
/*  with their dependency graph.  This chapter builds a directed graph of                    */
/*  elaboration dependencies, detects strongly-connected components using                    */
/*  Tarjan's algorithm, and produces a topological ordering.                                 */
/*                                                                                           */
/*  The elaboration model handles spec-before-body ordering, pragma Elaborate                 */
/*  and Elaborate_All, and circular dependency detection with diagnostic                     */
/*  reporting.                                                                               */
/*                                                                                           */
/* ========================================================================================= */

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  15.1  Elaboration Types                                                                  */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

typedef enum { ELAB_SPEC = 0, ELAB_BODY, ELAB_BOTH } Elab_Unit_Kind;
typedef enum { ELAB_DEP_WITH = 0, ELAB_DEP_ELABORATE,
               ELAB_DEP_ELABORATE_ALL, ELAB_DEP_SPEC_BODY } Elab_Edge_Kind;
typedef enum { ELAB_NORMAL = 0, ELAB_FORCED, ELAB_DEFERRED } Elab_Precedence;
typedef enum { ELAB_OK = 0, ELAB_CIRCULAR, ELAB_MISSING } Elab_Order_Status;

typedef struct {
  String_Slice   name;
  Elab_Unit_Kind kind;
  Symbol        *symbol;
  bool           elaborated;
  bool           in_progress;
  uint32_t       order_index;   /*  Position in the final ordering           */
  uint32_t       scc_index;     /*  Tarjan SCC index                         */
  uint32_t       scc_lowlink;   /*  Tarjan lowlink value                     */
  bool           scc_on_stack;
} Elab_Vertex;

typedef struct {
  uint32_t        from;         /*  Source vertex index                       */
  uint32_t        to;           /*  Target vertex index                       */
  Elab_Edge_Kind  kind;
  Elab_Precedence precedence;
} Elab_Edge;

typedef struct {
  uint32_t items[ELAB_MAX_VERTICES];
  uint32_t count;
} Elab_Vertex_Set;

typedef struct {
  Elab_Vertex    vertices[ELAB_MAX_VERTICES];
  uint32_t       vertex_count;
  Elab_Edge      edges[ELAB_MAX_EDGES];
  uint32_t       edge_count;
  uint32_t       order[ELAB_MAX_VERTICES];
  uint32_t       order_count;
  uint32_t       scc_components[ELAB_MAX_COMPONENTS][ELAB_MAX_VERTICES];
  uint32_t       scc_sizes[ELAB_MAX_COMPONENTS];
  uint32_t       scc_count;
} Elab_Graph;

typedef struct {
  uint32_t stack[ELAB_MAX_VERTICES];
  uint32_t stack_top;
  uint32_t index;
} Tarjan_State;

/* ─────────────────────────────────────────────────────────────────────────────────────────── */
/*  15.2  Elaboration Operations                                                             */
/* ─────────────────────────────────────────────────────────────────────────────────────────── */

uint32_t          Elab_Find_Vertex        (Elab_Graph *g, String_Slice name,
                                            Elab_Unit_Kind kind);
uint32_t          Elab_Add_Vertex         (Elab_Graph *g, String_Slice name,
                                            Elab_Unit_Kind kind, Symbol *sym);
void              Elab_Add_Edge           (Elab_Graph *g, uint32_t from,
                                            uint32_t to, Elab_Edge_Kind kind);
bool              Edge_Kind_Is_Strong     (Elab_Edge_Kind k);

void              Elab_Tarjan_Visit       (Elab_Graph *g, Tarjan_State *st,
                                            uint32_t v);
void              Elab_Compute_SCCs       (Elab_Graph *g);
void              Elab_Find_Components    (Elab_Graph *g);
Elab_Graph        Elab_Graph_New          (void);
Elab_Vertex_Set   Elab_Set_Empty          (void);

void              Elab_Elaborate_Vertex   (Elab_Graph *g, uint32_t v_id,
                                            Elab_Vertex_Set *ready);
Elab_Order_Status Elab_Elaborate_Graph    (Elab_Graph *g);
void              Elab_Pair_Specs_Bodies  (Elab_Graph *g);

extern bool       g_elab_graph_initialized;
void              Elab_Init               (void);
uint32_t          Elab_Register_Unit      (String_Slice name, bool is_body,
                                            Symbol *sym);
Elab_Order_Status Elab_Compute_Order      (void);
uint32_t          Elab_Get_Order_Count    (void);
Symbol           *Elab_Get_Order_Symbol   (uint32_t index);
bool              Elab_Needs_Elab_Call    (uint32_t index);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 16.  GENERICS                                                                    */
/*                                                                                           */
/*  Generic units are instantiated by macro-style expansion: the template AST                 */
/*  is deep-cloned with formal-to-actual substitution, then resolved and                     */
/*  code-generated as though the programmer had written the expanded text by hand.            */
/*                                                                                           */
/*  The Instantiation_Env maps formal names to actual types, subprograms, and                */
/*  expressions.  Node_Deep_Clone recursively duplicates the AST while replacing             */
/*  formal references with their actuals.                                                    */
/*                                                                                           */
/* ========================================================================================= */

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

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 17.  FILE LOADING                                                                */
/*                                                                                           */
/*  WITH clauses name packages that must be found on disk, loaded, parsed,                    */
/*  semantically analysed, and code-generated before the withing unit can                    */
/*  proceed.  Include_Paths lists the directories to search; Lookup_Path                     */
/*  maps a unit name to a file path using standard naming conventions.                       */
/*                                                                                           */
/*  The Loading_Set detects circular WITH dependencies by tracking which                     */
/*  units are currently being loaded.                                                        */
/*                                                                                           */
/* ========================================================================================= */

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
bool  Has_Precompiled_LL     (String_Slice name);

void  Load_Package_Spec      (String_Slice name, char *src);
char *Read_File              (const char *path, size_t *out_size);
char *Read_File_Simple       (const char *path);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 18.  VECTOR PATHS                                                                */
/*                                                                                           */
/*  Vectorised scanning primitives for whitespace skipping, identifier recognition,           */
/*  digit scanning, and single-character search.  Three implementations are                  */
/*  selected at compile time by the platform detection in Chapter 1:                         */
/*                                                                                           */
/*    x86-64   AVX-512BW (64-byte), AVX2 (32-byte), with scalar tail                        */
/*    ARM64    NEON/ASIMD (16-byte), with scalar tail                                        */
/*    Generic  Scalar fallback with unrolled loops                                           */
/*                                                                                           */
/*  All SIMD paths produce results identical to their scalar equivalents for                 */
/*  every possible input.  The scanning interface is the same on all platforms;               */
/*  only the implementation changes.                                                         */
/*                                                                                           */
/* ========================================================================================= */

#ifdef SIMD_X86_64
  extern int  Simd_Has_Avx512;
  extern int  Simd_Has_Avx2;
  uint32_t    Tzcnt32 (uint32_t value);
  uint64_t    Tzcnt64 (uint64_t value);
  #ifdef __AVX2__
  uint32_t    Simd_Parse_8_Digits_Avx2 (const char *digits);
  int         Simd_Parse_Digits_Avx2   (const char *cursor,
                                         const char *limit, uint64_t *out);
  #endif
#elif defined(SIMD_ARM64)
  uint64_t    Tzcnt64 (uint64_t value);
#endif

void         Simd_Detect_Features       (void);
const char  *Simd_Skip_Whitespace       (const char *cursor, const char *limit);
const char  *Simd_Find_Newline          (const char *cursor, const char *limit);
const char  *Simd_Find_Quote            (const char *cursor, const char *limit);
const char  *Simd_Find_Double_Quote     (const char *cursor, const char *limit);
const char  *Simd_Scan_Identifier       (const char *cursor, const char *limit);
const char  *Simd_Scan_Digits           (const char *cursor, const char *limit);

/* ========================================================================================= */
/*                                                                                           */
/*  Chapter 19.  DRIVER                                                                      */
/*                                                                                           */
/*  The main driver parses command-line arguments, compiles each source file to               */
/*  LLVM IR -- optionally forking a subprocess per file for parallel compilation --           */
/*  and returns an exit status.  Derive_Output_Path maps an input .adb or .ads               */
/*  file to the corresponding .ll output path.                                               */
/*                                                                                           */
/* ========================================================================================= */

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

