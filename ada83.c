/* ===========================================================================
 * Ada83 Compiler - A complete Ada 1983 compiler targeting LLVM IR
 * ===========================================================================
 *
 * This compiler implements the Ada 1983 standard, translating Ada source code
 * directly to LLVM intermediate representation. This implementation targets
 * both correctness and the generation of highly optimized code, using LLVM's
 * extensive optimization passes while maintaining strict Ada 83 semantics.
 *
 * Architecture Overview:
 *   - Lexical Analysis: Character-by-character scanning with Ada-specific rules
 *   - Parsing: Recursive descent parser producing abstract syntax trees
 *   - Semantic Analysis: Symbol table management with scope tracking
 *   - Code Generation: Direct emission of LLVM IR with Ada semantics
 *
 * Key Design Decisions:
 *   - Arena allocation for AST nodes (no individual frees during compilation)
 *   - Arbitrary precision integers for accurate constant evaluation
 *   - Fat pointers for Ada's unconstrained arrays and access types
 *   - Direct LLVM IR emission rather than intermediate representations
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iso646.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *include_paths[32];
static int include_path_count = 0;
typedef struct
{
  uint64_t *digits;
  uint32_t count, capacity;
  bool is_negative;
} Unsigned_Big_Integer;
typedef struct
{
  Unsigned_Big_Integer *numerator, *denominator;
} Rational_Number;
static Unsigned_Big_Integer *unsigned_bigint_new(uint32_t c)
{
  Unsigned_Big_Integer *u = malloc(sizeof(Unsigned_Big_Integer));
  u->digits = calloc(c, 8);
  u->count = 0;
  u->capacity = c;
  u->is_negative = 0;
  return u;
}
static void unsigned_bigint_free(Unsigned_Big_Integer *u)
{
  if (u)
  {
    free(u->digits);
    free(u);
  }
}
static void unsigned_bigint_grow(Unsigned_Big_Integer *u, uint32_t c)
{
  if (c > u->capacity)
  {
    u->digits = realloc(u->digits, c * 8);
    memset(u->digits + u->capacity, 0, (c - u->capacity) * 8);
    u->capacity = c;
  }
}
#define UNSIGNED_BIGINT_NORMALIZE(u)                                                               \
  do                                                                                               \
  {                                                                                                \
    while ((u)->count > 0 and not(u)->digits[(u)->count - 1])                                     \
      (u)->count--;                                                                                \
    if (not(u)->count)                                                                             \
      (u)->is_negative = 0;                                                                        \
  } while (0)
static int unsigned_bigint_compare_abs(const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  if (a->count != b->count)
    return a->count > b->count ? 1 : -1;
  for (int i = a->count - 1; i >= 0; i--)
    if (a->digits[i] != b->digits[i])
      return a->digits[i] > b->digits[i] ? 1 : -1;
  return 0;
}
static inline uint64_t add_with_carry(uint64_t a, uint64_t b, uint64_t c, uint64_t *r)
{
  __uint128_t s = (__uint128_t) a + b + c;
  *r = s;
  return s >> 64;
}
static inline uint64_t subtract_with_borrow(uint64_t a, uint64_t b, uint64_t c, uint64_t *r)
{
  __uint128_t d = (__uint128_t) a - b - c;
  *r = d;
  return -(d >> 64);
}
static void unsigned_bigint_binary_op(
    Unsigned_Big_Integer *r,
    const Unsigned_Big_Integer *a,
    const Unsigned_Big_Integer *b,
    bool is_add)
{
  if (is_add)
  {
    uint32_t m = (a->count > b->count ? a->count : b->count) + 1;
    unsigned_bigint_grow(r, m);
    uint64_t c = 0;
    uint32_t i;
    for (i = 0; i < a->count or i < b->count or c; i++)
    {
      uint64_t ai = i < a->count ? a->digits[i] : 0, bi = i < b->count ? b->digits[i] : 0;
      c = add_with_carry(ai, bi, c, &r->digits[i]);
    }
    r->count = i;
  }
  else
  {
    unsigned_bigint_grow(r, a->count);
    uint64_t c = 0;
    for (uint32_t i = 0; i < a->count; i++)
    {
      uint64_t ai = a->digits[i], bi = i < b->count ? b->digits[i] : 0;
      c = subtract_with_borrow(ai, bi, c, &r->digits[i]);
    }
    r->count = a->count;
  }
  UNSIGNED_BIGINT_NORMALIZE(r);
}
static void unsigned_bigint_add_abs(
    Unsigned_Big_Integer *r, const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  unsigned_bigint_binary_op(r, a, b, 1);
}
static void unsigned_bigint_sub_abs(
    Unsigned_Big_Integer *r, const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  unsigned_bigint_binary_op(r, a, b, 0);
}
static void unsigned_bigint_add(
    Unsigned_Big_Integer *r, const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  if (a->is_negative == b->is_negative)
  {
    unsigned_bigint_add_abs(r, a, b);
    r->is_negative = a->is_negative;
  }
  else
  {
    int c = unsigned_bigint_compare_abs(a, b);
    if (c >= 0)
    {
      unsigned_bigint_sub_abs(r, a, b);
      r->is_negative = a->is_negative;
    }
    else
    {
      unsigned_bigint_sub_abs(r, b, a);
      r->is_negative = b->is_negative;
    }
  }
}
static void unsigned_bigint_subtract(
    Unsigned_Big_Integer *r, const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  Unsigned_Big_Integer t = *b;
  t.is_negative = not b->is_negative;
  unsigned_bigint_add(r, a, &t);
}
static void unsigned_bigint_multiply_basic(
    Unsigned_Big_Integer *r, const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  unsigned_bigint_grow(r, a->count + b->count);
  memset(r->digits, 0, (a->count + b->count) * 8);
  for (uint32_t i = 0; i < a->count; i++)
  {
    uint64_t c = 0;
    for (uint32_t j = 0; j < b->count; j++)
    {
      __uint128_t p = (__uint128_t) a->digits[i] * b->digits[j] + r->digits[i + j] + c;
      r->digits[i + j] = p;
      c = p >> 64;
    }
    r->digits[i + b->count] = c;
  }
  r->count = a->count + b->count;
  r->is_negative = a->is_negative != b->is_negative;
  UNSIGNED_BIGINT_NORMALIZE(r);
}
static void unsigned_bigint_multiply_karatsuba(
    Unsigned_Big_Integer *r, const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  uint32_t n = a->count > b->count ? a->count : b->count;
  if (n < 20)
  {
    unsigned_bigint_multiply_basic(r, a, b);
    return;
  }
  uint32_t m = n / 2;
  Unsigned_Big_Integer a0 = {a->digits, a->count > m ? m : a->count, a->capacity, 0},
                       a1 = {a->count > m ? a->digits + m : 0, a->count > m ? a->count - m : 0, 0, 0};
  Unsigned_Big_Integer b0 = {b->digits, b->count > m ? m : b->count, b->capacity, 0},
                       b1 = {b->count > m ? b->digits + m : 0, b->count > m ? b->count - m : 0, 0, 0};
  Unsigned_Big_Integer *z0 = unsigned_bigint_new(a0.count + b0.count),
                       *z2 = unsigned_bigint_new(a1.count + b1.count), *z1 = unsigned_bigint_new(n * 2);
  unsigned_bigint_multiply_karatsuba(z0, &a0, &b0);
  unsigned_bigint_multiply_karatsuba(z2, &a1, &b1);
  Unsigned_Big_Integer *as = unsigned_bigint_new(m + 1), *bs = unsigned_bigint_new(m + 1);
  unsigned_bigint_add(as, &a0, &a1);
  unsigned_bigint_add(bs, &b0, &b1);
  unsigned_bigint_multiply_karatsuba(z1, as, bs);
  unsigned_bigint_subtract(z1, z1, z0);
  unsigned_bigint_subtract(z1, z1, z2);
  unsigned_bigint_grow(r, 2 * n);
  memset(r->digits, 0, 2 * n * 8);
  for (uint32_t i = 0; i < z0->count; i++)
    r->digits[i] = z0->digits[i];
  uint64_t c = 0;
  for (uint32_t i = 0; i < z1->count or c; i++)
  {
    uint64_t v = r->digits[m + i] + (i < z1->count ? z1->digits[i] : 0) + c;
    r->digits[m + i] = v;
    c = v < r->digits[m + i];
  }
  c = 0;
  for (uint32_t i = 0; i < z2->count or c; i++)
  {
    uint64_t v = r->digits[2 * m + i] + (i < z2->count ? z2->digits[i] : 0) + c;
    r->digits[2 * m + i] = v;
    c = v < r->digits[2 * m + i];
  }
  r->count = 2 * n;
  r->is_negative = a->is_negative != b->is_negative;
  UNSIGNED_BIGINT_NORMALIZE(r);
  unsigned_bigint_free(z0);
  unsigned_bigint_free(z1);
  unsigned_bigint_free(z2);
  unsigned_bigint_free(as);
  unsigned_bigint_free(bs);
}
static void unsigned_bigint_multiply(
    Unsigned_Big_Integer *r, const Unsigned_Big_Integer *a, const Unsigned_Big_Integer *b)
{
  unsigned_bigint_multiply_karatsuba(r, a, b);
}
static Unsigned_Big_Integer *unsigned_bigint_from_decimal(const char *s)
{
  Unsigned_Big_Integer *r = unsigned_bigint_new(4), *ten = unsigned_bigint_new(1);
  ten->digits[0] = 10;
  ten->count = 1;
  bool neg = *s == '-';
  if (neg or *s == '+')
    s++;
  while (*s)
  {
    if (*s >= '0' and *s <= '9')
    {
      Unsigned_Big_Integer *d = unsigned_bigint_new(1);
      d->digits[0] = *s - '0';
      d->count = 1;
      Unsigned_Big_Integer *t = unsigned_bigint_new(r->count * 2);
      unsigned_bigint_multiply(t, r, ten);
      unsigned_bigint_free(r);
      r = t;
      t = unsigned_bigint_new(r->count + 1);
      unsigned_bigint_add(t, r, d);
      unsigned_bigint_free(r);
      unsigned_bigint_free(d);
      r = t;
    }
    s++;
  }
  r->is_negative = neg;
  unsigned_bigint_free(ten);
  return r;
}
typedef struct
{
  char *base, *pointer, *end;
} Arena_Allocator;
typedef struct
{
  const char *string;
  uint32_t length;
} String_Slice;
typedef struct
{
  uint32_t line, column;
  const char *filename;
} Source_Location;
#define STRING_LITERAL(x) ((String_Slice){x, sizeof(x) - 1})
#define N ((String_Slice){0, 0})
static Arena_Allocator main_arena = {0};
static int error_count = 0;
static String_Slice SEPARATE_PACKAGE = N;
static void *arena_allocate(size_t n)
{
  n = (n + 7) & ~7;
  if (not main_arena.base or main_arena.pointer + n > main_arena.end)
  {
    size_t z = 1 << 24;
    main_arena.base = main_arena.pointer = malloc(z);
    main_arena.end = main_arena.base + z;
  }
  void *r = main_arena.pointer;
  main_arena.pointer += n;
  return memset(r, 0, n);
}
static String_Slice string_duplicate(String_Slice s)
{
  char *p = arena_allocate(s.length + 1);
  memcpy(p, s.string, s.length);
  return (String_Slice){p, s.length};
}
static bool string_equal_ignore_case(String_Slice a, String_Slice b)
{
  if (a.length != b.length)
    return 0;
  for (uint32_t i = 0; i < a.length; i++)
    if (tolower(a.string[i]) != tolower(b.string[i]))
      return 0;
  return 1;
}
static char *string_to_lowercase(String_Slice s)
{
  static char b[8][256];
  static int i;
  char *p = b[i++ & 7];
  uint32_t n = s.length < 255 ? s.length : 255;
  for (uint32_t j = 0; j < n; j++)
    p[j] = tolower(s.string[j]);
  p[n] = 0;
  return p;
}
static uint64_t string_hash(String_Slice s)
{
  uint64_t h = 14695981039346656037ULL;
  for (uint32_t i = 0; i < s.length; i++)
    h = (h ^ (uint8_t) tolower(s.string[i])) * 1099511628211ULL;
  return h;
}
static _Noreturn void fatal_error(Source_Location l, const char *f, ...)
{
  va_list v;
  va_start(v, f);
  fprintf(stderr, "%s:%u:%u: ", l.filename, l.line, l.column);
  vfprintf(stderr, f, v);
  fputc('\n', stderr);
  va_end(v);
  error_count++;
  exit(1);
}
typedef enum
{
  T_EOF = 0,
  T_ERR,
  T_ID,
  T_INT,
  T_REAL,
  T_CHAR,
  T_STR,
  T_LP,
  T_RP,
  T_LB,
  T_RB,
  T_CM,
  T_DT,
  T_SC,
  T_CL,
  T_TK,
  T_AS,
  T_AR,
  T_DD,
  T_LL,
  T_GG,
  T_BX,
  T_BR,
  T_EQ,
  T_NE,
  T_LT,
  T_LE,
  T_GT,
  T_GE,
  T_PL,
  T_MN,
  T_ST,
  T_SL,
  T_AM,
  T_EX,
  T_AB,
  T_ABS,
  T_ACC,
  T_ACCS,
  T_ALITK,
  T_ALL,
  T_AND,
  T_ATHN,
  T_ARR,
  T_AT,
  T_BEG,
  T_BOD,
  T_CSE,
  T_CONST,
  T_DEC,
  T_DEL,
  T_DELTA,
  T_DIG,
  T_DO,
  T_ELSE,
  T_ELSIF,
  T_END,
  T_ENT,
  T_EXCP,
  T_EXIT,
  T_FOR,
  T_FUN,
  T_GEN,
  T_GOTO,
  T_IF,
  T_IN,
  T_IS,
  T_LIM,
  T_LOOP,
  T_MOD,
  T_NEW,
  T_NOT,
  T_NULL,
  T_OF,
  T_OR,
  T_OREL,
  T_OTH,
  T_OUT,
  T_PKG,
  T_PGM,
  T_PRV,
  T_PROC,
  T_RAS,
  T_RNG,
  T_REC,
  T_REM,
  T_REN,
  T_RET,
  T_REV,
  T_SEL,
  T_SEP,
  T_SUB,
  T_TSK,
  T_TER,
  T_THEN,
  T_TYP,
  T_USE,
  T_WHN,
  T_WHI,
  T_WITH,
  T_XOR,
  T_CNT
} Token_Kind;
enum
{
  CHK_OVF = 1,
  CHK_RNG = 2,
  CHK_IDX = 4,
  CHK_DSC = 8,
  CHK_LEN = 16,
  CHK_DIV = 32,
  CHK_ELB = 64,
  CHK_ACC = 128,
  CHK_STG = 256
};
static const char *TN[T_CNT] = {
    [T_EOF] = "eof",        [T_ERR] = "ERR",        [T_ID] = "id",          [T_INT] = "int",
    [T_REAL] = "real",      [T_CHAR] = "char",      [T_STR] = "str",        [T_LP] = "(",
    [T_RP] = ")",           [T_LB] = "[",           [T_RB] = "]",           [T_CM] = ",",
    [T_DT] = ".",           [T_SC] = ";",           [T_CL] = ":",           [T_TK] = "'",
    [T_AS] = ":=",          [T_AR] = "=>",          [T_DD] = "..",          [T_LL] = "<<",
    [T_GG] = ">>",          [T_BX] = "<>",          [T_BR] = "|",           [T_EQ] = "=",
    [T_NE] = "/=",          [T_LT] = "<",           [T_LE] = "<=",          [T_GT] = ">",
    [T_GE] = ">=",          [T_PL] = "+",           [T_MN] = "-",           [T_ST] = "*",
    [T_SL] = "/",           [T_AM] = "&",           [T_EX] = "**",          [T_AB] = "ABORT",
    [T_ABS] = "ABS",        [T_ACC] = "ACCEPT",     [T_ACCS] = "ACCESS",    [T_ALITK] = "ALIASED",
    [T_ALL] = "ALL",        [T_AND] = "AND",        [T_ATHN] = "AND THEN",  [T_ARR] = "ARRAY",
    [T_AT] = "AT",          [T_BEG] = "BEGIN",      [T_BOD] = "BODY",       [T_CSE] = "CASE",
    [T_CONST] = "CONSTANT", [T_DEC] = "DECLARE",    [T_DEL] = "DELAY",      [T_DELTA] = "DELTA",
    [T_DIG] = "DIGITS",     [T_DO] = "DO",          [T_ELSE] = "ELSE",      [T_ELSIF] = "ELSIF",
    [T_END] = "END",        [T_ENT] = "ENTRY",      [T_EXCP] = "EXCEPTION", [T_EXIT] = "EXIT",
    [T_FOR] = "FOR",        [T_FUN] = "FUNCTION",   [T_GEN] = "GENERIC",    [T_GOTO] = "GOTO",
    [T_IF] = "IF",          [T_IN] = "IN",          [T_IS] = "IS",          [T_LIM] = "LIMITED",
    [T_LOOP] = "LOOP",      [T_MOD] = "MOD",        [T_NEW] = "NEW",        [T_NOT] = "NOT",
    [T_NULL] = "NULL",      [T_OF] = "OF",          [T_OR] = "OR",          [T_OREL] = "OR ELSE",
    [T_OTH] = "OTHERS",     [T_OUT] = "OUT",        [T_PKG] = "PACKAGE",    [T_PGM] = "PRAGMA",
    [T_PRV] = "PRIVATE",    [T_PROC] = "PROCEDURE", [T_RAS] = "RAISE",      [T_RNG] = "RANGE",
    [T_REC] = "RECORD",     [T_REM] = "REM",        [T_REN] = "RENAMES",    [T_RET] = "RETURN",
    [T_REV] = "REVERSE",    [T_SEL] = "SELECT",     [T_SEP] = "SEPARATE",   [T_SUB] = "SUBTYPE",
    [T_TSK] = "TASK",       [T_TER] = "TERMINATE",  [T_THEN] = "THEN",      [T_TYP] = "TYPE",
    [T_USE] = "USE",        [T_WHN] = "WHEN",       [T_WHI] = "WHILE",      [T_WITH] = "WITH",
    [T_XOR] = "XOR"};
typedef struct
{
  Token_Kind kind;
  Source_Location location;
  String_Slice literal;
  int64_t integer_value;
  double float_value;
  Unsigned_Big_Integer *unsigned_integer;
  Rational_Number *unsigned_rational;
} Token;
typedef struct
{
  const char *start, *current, *end;
  uint32_t line_number, column;
  const char *filename;
  Token_Kind previous_token;
} Lexer;
static struct
{
  String_Slice keyword;
  Token_Kind token_kind;
} KW[] = {{STRING_LITERAL("abort"), T_AB},     {STRING_LITERAL("abs"), T_ABS},
          {STRING_LITERAL("accept"), T_ACC},   {STRING_LITERAL("access"), T_ACCS},
          {STRING_LITERAL("all"), T_ALL},      {STRING_LITERAL("and"), T_AND},
          {STRING_LITERAL("array"), T_ARR},    {STRING_LITERAL("at"), T_AT},
          {STRING_LITERAL("begin"), T_BEG},    {STRING_LITERAL("body"), T_BOD},
          {STRING_LITERAL("case"), T_CSE},     {STRING_LITERAL("constant"), T_CONST},
          {STRING_LITERAL("declare"), T_DEC},  {STRING_LITERAL("delay"), T_DEL},
          {STRING_LITERAL("delta"), T_DELTA},  {STRING_LITERAL("digits"), T_DIG},
          {STRING_LITERAL("do"), T_DO},        {STRING_LITERAL("else"), T_ELSE},
          {STRING_LITERAL("elsif"), T_ELSIF},  {STRING_LITERAL("end"), T_END},
          {STRING_LITERAL("entry"), T_ENT},    {STRING_LITERAL("exception"), T_EXCP},
          {STRING_LITERAL("exit"), T_EXIT},    {STRING_LITERAL("for"), T_FOR},
          {STRING_LITERAL("function"), T_FUN}, {STRING_LITERAL("generic"), T_GEN},
          {STRING_LITERAL("goto"), T_GOTO},    {STRING_LITERAL("if"), T_IF},
          {STRING_LITERAL("in"), T_IN},        {STRING_LITERAL("is"), T_IS},
          {STRING_LITERAL("limited"), T_LIM},  {STRING_LITERAL("loop"), T_LOOP},
          {STRING_LITERAL("mod"), T_MOD},      {STRING_LITERAL("new"), T_NEW},
          {STRING_LITERAL("not"), T_NOT},      {STRING_LITERAL("null"), T_NULL},
          {STRING_LITERAL("of"), T_OF},        {STRING_LITERAL("or"), T_OR},
          {STRING_LITERAL("others"), T_OTH},   {STRING_LITERAL("out"), T_OUT},
          {STRING_LITERAL("package"), T_PKG},  {STRING_LITERAL("pragma"), T_PGM},
          {STRING_LITERAL("private"), T_PRV},  {STRING_LITERAL("procedure"), T_PROC},
          {STRING_LITERAL("raise"), T_RAS},    {STRING_LITERAL("range"), T_RNG},
          {STRING_LITERAL("record"), T_REC},   {STRING_LITERAL("rem"), T_REM},
          {STRING_LITERAL("renames"), T_REN},  {STRING_LITERAL("return"), T_RET},
          {STRING_LITERAL("reverse"), T_REV},  {STRING_LITERAL("select"), T_SEL},
          {STRING_LITERAL("separate"), T_SEP}, {STRING_LITERAL("subtype"), T_SUB},
          {STRING_LITERAL("task"), T_TSK},     {STRING_LITERAL("terminate"), T_TER},
          {STRING_LITERAL("then"), T_THEN},    {STRING_LITERAL("type"), T_TYP},
          {STRING_LITERAL("use"), T_USE},      {STRING_LITERAL("when"), T_WHN},
          {STRING_LITERAL("while"), T_WHI},    {STRING_LITERAL("with"), T_WITH},
          {STRING_LITERAL("xor"), T_XOR},      {N, T_EOF}};
static Token_Kind keyword_lookup(String_Slice slice)
{
  for (int index = 0; KW[index].keyword.string; index++)
    if (string_equal_ignore_case(slice, KW[index].keyword))
      return KW[index].token_kind;
  return T_ID;
}
static Lexer lexer_new(const char *source, size_t size, const char *filename)
{
  return (Lexer){source, source, source + size, 1, 1, filename, T_EOF};
}
static char peek(Lexer *lexer, size_t offset)
{
  return lexer->current + offset < lexer->end ? lexer->current[offset] : 0;
}
static char advance_character(Lexer *lexer)
{
  if (lexer->current >= lexer->end)
    return 0;
  char character = *lexer->current++;
  if (character == '\n')
  {
    lexer->line_number++;
    lexer->column = 1;
  }
  else
    lexer->column++;
  return character;
}
static void skip_whitespace(Lexer *lexer)
{
  for (;;)
  {
    while (
        lexer->current < lexer->end
        and (*lexer->current == ' ' or *lexer->current == '\t' or *lexer->current == '\n' or *lexer->current == '\r' or *lexer->current == '\v' or *lexer->current == '\f'))
      advance_character(lexer);
    if (lexer->current + 1 < lexer->end and lexer->current[0] == '-' and lexer->current[1] == '-')
    {
      while (lexer->current < lexer->end and *lexer->current != '\n')
        advance_character(lexer);
    }
    else
      break;
  }
}
static Token make_token(Token_Kind token_kind, Source_Location location, String_Slice literal_text)
{
  return (Token){token_kind, location, literal_text, 0, 0.0, 0, 0};
}
static Token scan_identifier(Lexer *lexer)
{
  Source_Location location = {lexer->line_number, lexer->column, lexer->filename};
  const char *start = lexer->current;
  while (isalnum(peek(lexer, 0)) or peek(lexer, 0) == '_')
    advance_character(lexer);
  String_Slice literal_text = {start, lexer->current - start};
  Token_Kind token_kind = keyword_lookup(literal_text);
  if (token_kind != T_ID and lexer->current < lexer->end and (isalnum(*lexer->current) or *lexer->current == '_'))
    return make_token(T_ERR, location, STRING_LITERAL("kw+x"));
  return make_token(token_kind, location, literal_text);
}
static Token scan_number_literal(Lexer *lexer)
{
  Source_Location location = {lexer->line_number, lexer->column, lexer->filename};
  const char *start = lexer->current;
  const char *mantissa_start = 0, *mantissa_end = 0, *exponent_start = 0;
  int base = 10;
  bool is_real = false, based_exponent = false, has_dot = false, has_exp = false;
  char base_delimiter = 0;
  while (isdigit(peek(lexer, 0)) or peek(lexer, 0) == '_')
    advance_character(lexer);
  if (peek(lexer, 0) == '#' or (peek(lexer, 0) == ':' and isxdigit(peek(lexer, 1))))
  {
    base_delimiter = peek(lexer, 0);
    const char *base_end = lexer->current;
    advance_character(lexer);
    char *base_pointer = arena_allocate(32);
    int base_index = 0;
    for (const char *p = start; p < base_end; p++)
      if (*p != '_')
        base_pointer[base_index++] = *p;
    base_pointer[base_index] = 0;
    base = atoi(base_pointer);
    mantissa_start = lexer->current;
    while (isxdigit(peek(lexer, 0)) or peek(lexer, 0) == '_')
      advance_character(lexer);
    if (peek(lexer, 0) == '.')
    {
      is_real = true;
      advance_character(lexer);
      while (isxdigit(peek(lexer, 0)) or peek(lexer, 0) == '_')
        advance_character(lexer);
    }
    if (peek(lexer, 0) == base_delimiter)
    {
      mantissa_end = lexer->current;
      advance_character(lexer);
    }
    if (tolower(peek(lexer, 0)) == 'e')
    {
      based_exponent = true;
      advance_character(lexer);
      if (peek(lexer, 0) == '+' or peek(lexer, 0) == '-')
        advance_character(lexer);
      exponent_start = lexer->current;
      while (isdigit(peek(lexer, 0)) or peek(lexer, 0) == '_')
        advance_character(lexer);
    }
  }
  else
  {
    if (peek(lexer, 0) == '.')
    {
      if (peek(lexer, 1) != '.' and not isalpha(peek(lexer, 1)))
      {
        is_real = true;
        has_dot = true;
        advance_character(lexer);
        while (isdigit(peek(lexer, 0)) or peek(lexer, 0) == '_')
          advance_character(lexer);
      }
    }
    if (tolower(peek(lexer, 0)) == 'e')
    {
      has_exp = true;
      advance_character(lexer);
      if (peek(lexer, 0) == '+' or peek(lexer, 0) == '-')
        advance_character(lexer);
      while (isdigit(peek(lexer, 0)) or peek(lexer, 0) == '_')
        advance_character(lexer);
    }
  }
  if (isalpha(peek(lexer, 0)))
    return make_token(T_ERR, location, STRING_LITERAL("num+alpha"));
  Token token =
      make_token(based_exponent ? (is_real ? T_REAL : T_INT) : (is_real ? T_REAL : T_INT), location, (String_Slice){start, lexer->current - start});
  if (based_exponent and exponent_start)
  {
    char *mantissa_pointer = arena_allocate(512);
    char *exponent_pointer = arena_allocate(512);
    int mantissa_index = 0, exponent_index = 0;
    for (const char *p = mantissa_start; p < mantissa_end; p++)
      if (*p != '_' and *p != base_delimiter)
        mantissa_pointer[mantissa_index++] = *p;
    mantissa_pointer[mantissa_index] = 0;
    for (const char *p = exponent_start; p < lexer->current; p++)
      if (*p != '_')
        exponent_pointer[exponent_index++] = *p;
    exponent_pointer[exponent_index] = 0;
    double mantissa = 0;
    int decimal_point = -1;
    for (int i = 0; i < mantissa_index; i++)
    {
      if (mantissa_pointer[i] == '.')
      {
        decimal_point = i;
        break;
      }
    }
    int fractional_position = 0;
    for (int i = 0; i < mantissa_index; i++)
    {
      if (mantissa_pointer[i] == '.')
        continue;
      int digit_value = mantissa_pointer[i] >= 'A' and mantissa_pointer[i] <= 'F'   ? mantissa_pointer[i] - 'A' + 10
               : mantissa_pointer[i] >= 'a' and mantissa_pointer[i] <= 'f' ? mantissa_pointer[i] - 'a' + 10
                                               : mantissa_pointer[i] - '0';
      if (decimal_point < 0 or i < decimal_point)
        mantissa = mantissa * base + digit_value;
      else
      {
        fractional_position++;
        mantissa += digit_value / pow(base, fractional_position);
      }
    }
    int exponent = atoi(exponent_pointer);
    double result_value = mantissa * pow(base, exponent);
    if (is_real)
      token.float_value = result_value;
    else
    {
      if (result_value > LLONG_MAX or result_value < LLONG_MIN)
      {
        fprintf(
            stderr,
            "Error %d:%d: based integer constant out of range: %.*s\n",
            location.line,
            location.column,
            (int) (lexer->current - start),
            start);
        exit(1);
      }
      token.integer_value = (int64_t) result_value;
    }
  }
  else
  {
    char *text_pointer = arena_allocate(512);
    int text_index = 0;
    const char *text_start = (base_delimiter and mantissa_start) ? mantissa_start : start;
    const char *text_end = (base_delimiter and mantissa_end) ? mantissa_end : lexer->current;
    for (const char *p = text_start; p < text_end; p++)
      if (*p != '_' and *p != '#' and *p != ':')
        text_pointer[text_index++] = *p;
    text_pointer[text_index] = 0;
    if (base_delimiter and not is_real)
    {
      int64_t value = 0;
      for (int i = 0; i < text_index; i++)
      {
        int digit_value = text_pointer[i] >= 'A' and text_pointer[i] <= 'F'   ? text_pointer[i] - 'A' + 10
                 : text_pointer[i] >= 'a' and text_pointer[i] <= 'f' ? text_pointer[i] - 'a' + 10
                                                 : text_pointer[i] - '0';
        value = value * base + digit_value;
      }
      token.integer_value = value;
    }
    else if (base_delimiter and is_real)
    {
      double mantissa = 0;
      int decimal_point = -1;
      for (int i = 0; i < text_index; i++)
      {
        if (text_pointer[i] == '.')
        {
          decimal_point = i;
          break;
        }
      }
      int fractional_position = 0;
      for (int i = 0; i < text_index; i++)
      {
        if (text_pointer[i] == '.')
          continue;
        int digit_value = text_pointer[i] >= 'A' and text_pointer[i] <= 'F'   ? text_pointer[i] - 'A' + 10
                 : text_pointer[i] >= 'a' and text_pointer[i] <= 'f' ? text_pointer[i] - 'a' + 10
                                                 : text_pointer[i] - '0';
        if (decimal_point < 0 or i < decimal_point)
          mantissa = mantissa * base + digit_value;
        else
        {
          fractional_position++;
          mantissa += digit_value / pow(base, fractional_position);
        }
      }
      token.float_value = mantissa;
    }
    else
    {
      errno = 0;
      token.float_value = strtod(text_pointer, 0);
      if (errno == ERANGE and (token.float_value == HUGE_VAL or token.float_value == -HUGE_VAL))
      {
        fprintf(stderr, "Warning %d:%d: float constant overflow to infinity: %s\n", location.line, location.column, text_pointer);
      }
      token.unsigned_integer = unsigned_bigint_from_decimal(text_pointer);
      token.integer_value = (token.unsigned_integer->count == 1) ? token.unsigned_integer->digits[0] : 0;
      if (has_exp and not has_dot and token.float_value >= LLONG_MIN and token.float_value <= LLONG_MAX
          and token.float_value == (double) (int64_t) token.float_value)
      {
        token.integer_value = (int64_t) token.float_value;
        token.kind = T_INT;
      }
      else if (is_real or has_dot)
      {
        token.kind = T_REAL;
      }
      else if (token.unsigned_integer and token.unsigned_integer->count > 1)
      {
        fprintf(stderr, "Error %d:%d: integer constant too large for i64: %s\n", location.line, location.column, text_pointer);
      }
    }
  }
  return token;
}
static Token scan_character_literal(Lexer *lexer)
{
  Source_Location location = {lexer->line_number, lexer->column, lexer->filename};
  advance_character(lexer);
  if (not peek(lexer, 0))
    return make_token(T_ERR, location, STRING_LITERAL("uc"));
  char character = peek(lexer, 0);
  advance_character(lexer);
  if (peek(lexer, 0) != '\'')
    return make_token(T_ERR, location, STRING_LITERAL("uc"));
  advance_character(lexer);
  Token token = make_token(T_CHAR, location, (String_Slice){&character, 1});
  token.integer_value = character;
  return token;
}
static Token scan_string_literal(Lexer *lexer)
{
  Source_Location location = {lexer->line_number, lexer->column, lexer->filename};
  char delimiter = peek(lexer, 0);
  advance_character(lexer);
  const char *start = lexer->current;
  (void) start;
  char *buffer = arena_allocate(256), *buffer_pointer = buffer;
  int length = 0;
  while (peek(lexer, 0))
  {
    if (peek(lexer, 0) == delimiter)
    {
      if (peek(lexer, 1) == delimiter)
      {
        advance_character(lexer);
        advance_character(lexer);
        if (length < 255)
          *buffer_pointer++ = delimiter;
        length++;
      }
      else
        break;
    }
    else
    {
      if (length < 255)
        *buffer_pointer++ = peek(lexer, 0);
      length++;
      advance_character(lexer);
    }
  }
  if (peek(lexer, 0) == delimiter)
    advance_character(lexer);
  else
    return make_token(T_ERR, location, STRING_LITERAL("us"));
  *buffer_pointer = 0;
  String_Slice literal_text = {buffer, length};
  return make_token(T_STR, location, literal_text);
}
static Token lexer_next_token(Lexer *lexer)
{
  const char *position_before_whitespace = lexer->current;
  skip_whitespace(lexer);
  bool had_whitespace = lexer->current != position_before_whitespace;
  Source_Location location = {lexer->line_number, lexer->column, lexer->filename};
  char character = peek(lexer, 0);
  if (not character)
  {
    lexer->previous_token = T_EOF;
    return make_token(T_EOF, location, N);
  }
  if (isalpha(character))
  {
    Token token = scan_identifier(lexer);
    lexer->previous_token = token.kind;
    return token;
  }
  if (isdigit(character))
  {
    Token token = scan_number_literal(lexer);
    lexer->previous_token = token.kind;
    return token;
  }
  if (character == '\'')
  {
    char next_character = peek(lexer, 1);
    char previous_character = lexer->current > lexer->start ? lexer->current[-1] : 0;
    bool is_identifier_attribute = lexer->previous_token == T_ID and not had_whitespace and isalnum(previous_character);
    if (next_character and peek(lexer, 2) == '\'' and (lexer->current + 3 >= lexer->end or lexer->current[3] != '\'') and not is_identifier_attribute)
    {
      lexer->previous_token = T_CHAR;
      return scan_character_literal(lexer);
    }
    advance_character(lexer);
    lexer->previous_token = T_TK;
    return make_token(T_TK, location, STRING_LITERAL("'"));
  }
  if (character == '"' or character == '%')
  {
    Token token = scan_string_literal(lexer);
    lexer->previous_token = token.kind;
    return token;
  }
  advance_character(lexer);
  Token_Kind token_type;
  switch (character)
  {
  case '(':
    token_type = T_LP;
    break;
  case ')':
    token_type = T_RP;
    break;
  case '[':
    token_type = T_LB;
    break;
  case ']':
    token_type = T_RB;
    break;
  case ',':
    token_type = T_CM;
    break;
  case ';':
    token_type = T_SC;
    break;
  case '&':
    token_type = T_AM;
    break;
  case '|':
  case '!':
    token_type = T_BR;
    break;
  case '+':
    token_type = T_PL;
    break;
  case '-':
    token_type = T_MN;
    break;
  case '/':
    if (peek(lexer, 0) == '=')
    {
      advance_character(lexer);
      token_type = T_NE;
    }
    else
      token_type = T_SL;
    break;
  case '*':
    if (peek(lexer, 0) == '*')
    {
      advance_character(lexer);
      token_type = T_EX;
    }
    else
      token_type = T_ST;
    break;
  case '=':
    if (peek(lexer, 0) == '>')
    {
      advance_character(lexer);
      token_type = T_AR;
    }
    else
      token_type = T_EQ;
    break;
  case ':':
    if (peek(lexer, 0) == '=')
    {
      advance_character(lexer);
      token_type = T_AS;
    }
    else
      token_type = T_CL;
    break;
  case '.':
    if (peek(lexer, 0) == '.')
    {
      advance_character(lexer);
      token_type = T_DD;
    }
    else
      token_type = T_DT;
    break;
  case '<':
    if (peek(lexer, 0) == '=')
    {
      advance_character(lexer);
      token_type = T_LE;
    }
    else if (peek(lexer, 0) == '<')
    {
      advance_character(lexer);
      token_type = T_LL;
    }
    else if (peek(lexer, 0) == '>')
    {
      advance_character(lexer);
      token_type = T_BX;
    }
    else
      token_type = T_LT;
    break;
  case '>':
    if (peek(lexer, 0) == '=')
    {
      advance_character(lexer);
      token_type = T_GE;
    }
    else if (peek(lexer, 0) == '>')
    {
      advance_character(lexer);
      token_type = T_GG;
    }
    else
      token_type = T_GT;
    break;
  default:
    token_type = T_ERR;
    break;
  }
  lexer->previous_token = token_type;
  return make_token(token_type, location, token_type == T_ERR ? STRING_LITERAL("ux") : N);
}
typedef enum
{
  N_ERR = 0,
  N_ID,
  N_INT,
  N_REAL,
  N_CHAR,
  N_STR,
  N_NULL,
  N_AG,
  N_BIN,
  N_UN,
  N_AT,
  N_QL,
  N_CL,
  N_IX,
  N_SL,
  N_SEL,
  N_ALC,
  N_TI,
  N_TE,
  N_TF,
  N_TX,
  N_TA,
  N_TR,
  N_TAC,
  N_TP,
  N_ST,
  N_RN,
  N_CN,
  N_CM,
  N_VR,
  N_VP,
  N_DS,
  N_PM,
  N_PS,
  N_FS,
  N_PB,
  N_FB,
  N_PD,
  N_FD,
  N_PKS,
  N_PKB,
  N_PKD,
  N_OD,
  N_ND,
  N_TD,
  N_SD,
  N_ED,
  N_RE,
  N_AS,
  N_IF,
  N_CS,
  N_LP,
  N_BL,
  N_EX,
  N_RT,
  N_GT,
  N_RS,
  N_NS,
  N_CLT,
  N_EC,
  N_DL,
  N_AB,
  N_CD,
  N_ACC,
  N_SLS,
  N_SA,
  N_TKS,
  N_TKB,
  N_TKD,
  N_ENT,
  N_EI,
  N_HD,
  N_CH,
  N_ASC,
  N_WH,
  N_EL,
  N_WI,
  N_US,
  N_PG,
  N_RP,
  N_GD,
  N_GI,
  N_GF,
  N_CU,
  N_CX,
  N_LST,
  N_DRF,
  N_CVT,
  N_CHK,
  N_RRC,
  N_ERC,
  N_LNC,
  N_ADC,
  N_ALC2,
  N_DRV,
  N_LBL,
  N_OPID,
  N_GTP,
  N_GVL,
  N_GSP,
  N_GEN,
  N_GINST,
  N_TRM,
  N_CNT
} Node_Kind;
typedef struct Type_Info Type_Info;
typedef struct Syntax_Node Syntax_Node;
typedef struct Symbol Symbol;
typedef struct Representation_Clause Representation_Clause;
typedef struct Library_Unit Library_Unit;
typedef struct Generic_Template Generic_Template;
typedef struct Label_Entry Label_Entry;
struct Label_Entry
{
  String_Slice name;
  int basic_block;
};
typedef struct
{
  Syntax_Node **data;
  uint32_t count, capacity;
} Node_Vector;
typedef struct
{
  Symbol **data;
  uint32_t count, capacity;
} Symbol_Vector;
typedef struct
{
  Representation_Clause **data;
  uint32_t count, capacity;
} Representation_Clause_Vector;
typedef struct
{
  Library_Unit **data;
  uint32_t count, capacity;
} Library_Unit_Vector;
typedef struct
{
  Generic_Template **data;
  uint32_t count, capacity;
} Generic_Template_Vector;
typedef struct
{
  FILE **data;
  uint32_t count, capacity;
} File_Vector;
typedef struct
{
  String_Slice *data;
  uint32_t count, capacity;
} String_List_Vector;
typedef struct
{
  Label_Entry **data;
  uint32_t count, capacity;
} Label_Entry_Vector;
#define VECPUSH(vtype, etype, fname)                                                               \
  static void fname(vtype *v, etype e)                                                             \
  {                                                                                                \
    if (v->count >= v->capacity)                                                                   \
    {                                                                                              \
      v->capacity = v->capacity ? v->capacity << 1 : 8;                                            \
      v->data = realloc(v->data, v->capacity * sizeof(etype));                                     \
    }                                                                                              \
    v->data[v->count++] = e;                                                                       \
  }
VECPUSH(Node_Vector, Syntax_Node *, nv)
VECPUSH(Symbol_Vector, Symbol *, sv)
VECPUSH(Library_Unit_Vector, Library_Unit *, lv)
VECPUSH(Generic_Template_Vector, Generic_Template *, gv)
VECPUSH(Label_Entry_Vector, Label_Entry *, lev)
VECPUSH(File_Vector, FILE *, fv)
VECPUSH(String_List_Vector, String_Slice, slv)
struct Syntax_Node
{
  Node_Kind k;
  Source_Location location;
  Type_Info *ty;
  Symbol *symbol;
  union
  {
    String_Slice string_value;
    int64_t integer_value;
    double float_value;
    struct
    {
      Token_Kind op;
      Syntax_Node *left, *right;
    } binary_node;
    struct
    {
      Token_Kind op;
      Syntax_Node *operand;
    } unary_node;
    struct
    {
      Syntax_Node *prefix;
      String_Slice attribute_name;
      Node_Vector arguments;
    } attribute;
    struct
    {
      Syntax_Node *name, *aggregate;
    } qualified;
    struct
    {
      Syntax_Node *function_name;
      Node_Vector arguments;
    } call;
    struct
    {
      Syntax_Node *prefix;
      Node_Vector indices;
    } index;
    struct
    {
      Syntax_Node *prefix;
      Syntax_Node *low_bound, *high_bound;
    } slice;
    struct
    {
      Syntax_Node *prefix;
      String_Slice selector;
    } selected_component;
    struct
    {
      Syntax_Node *subtype;
      Syntax_Node *initializer;
    } allocator;
    struct
    {
      Syntax_Node *low_bound, *high_bound;
    } range;
    struct
    {
      Syntax_Node *range_spec;
      Node_Vector constraints;
    } constraint;
    struct
    {
      String_Slice name;
      Syntax_Node *ty;
      Syntax_Node *in;
      bool is_aliased;
      uint32_t offset, bit_offset;
      Syntax_Node *discriminant_constraint;
      Syntax_Node *discriminant_spec;
    } component_decl;
    struct
    {
      Node_Vector choices;
      Node_Vector components;
    } variant;
    struct
    {
      Syntax_Node *discriminant_spec;
      Node_Vector variants;
      uint32_t size;
    } variant_part;
    struct
    {
      String_Slice name;
      Syntax_Node *ty;
      Syntax_Node *default_value;
      uint8_t mode;
    } parameter;
    struct
    {
      String_Slice name;
      Node_Vector parameters;
      Syntax_Node *return_type;
      String_Slice operator_symbol;
    } subprogram;
    struct
    {
      Syntax_Node *subprogram_spec;
      Node_Vector declarations;
      Node_Vector statements;
      Node_Vector handlers;
      int elaboration_level;
      Symbol *parent;
      Node_Vector locks;
    } body;
    struct
    {
      String_Slice name;
      Node_Vector declarations;
      Node_Vector private_declarations;
      int elaboration_level;
    } package_spec;
    struct
    {
      String_Slice name;
      Node_Vector declarations;
      Node_Vector statements;
      Node_Vector handlers;
      int elaboration_level;
    } package_body;
    struct
    {
      Node_Vector identifiers;
      Syntax_Node *ty;
      Syntax_Node *in;
      bool is_constant;
    } object_decl;
    struct
    {
      String_Slice name;
      Syntax_Node *definition;
      Syntax_Node *discriminant;
      bool is_new;
      bool is_derived;
      Syntax_Node *parent_type;
      Node_Vector discriminants;
    } type_decl;
    struct
    {
      String_Slice name;
      Syntax_Node *index_constraint;
      Syntax_Node *constraint;
      Syntax_Node *range_constraint;
    } subtype_decl;
    struct
    {
      Node_Vector identifiers;
      Syntax_Node *renamed_entity;
    } exception_decl;
    struct
    {
      String_Slice name;
      Syntax_Node *renamed_entity;
    } renaming;
    struct
    {
      Syntax_Node *target;
      Syntax_Node *value;
    } assignment;
    struct
    {
      Syntax_Node *condition;
      Node_Vector then_statements;
      Node_Vector elsif_statements;
      Node_Vector else_statements;
    } if_stmt;
    struct
    {
      Syntax_Node *expression;
      Node_Vector alternatives;
    } case_stmt;
    struct
    {
      String_Slice label;
      Syntax_Node *iterator;
      bool is_reverse;
      Node_Vector statements;
      Node_Vector locks;
    } loop_stmt;
    struct
    {
      String_Slice label;
      Node_Vector declarations;
      Node_Vector statements;
      Node_Vector handlers;
    } block;
    struct
    {
      String_Slice label;
      Syntax_Node *condition;
    } exit_stmt;
    struct
    {
      Syntax_Node *value;
    } return_stmt;
    struct
    {
      String_Slice label;
    } goto_stmt;
    struct
    {
      Syntax_Node *exception_choice;
    } raise_stmt;
    struct
    {
      Syntax_Node *name;
      Node_Vector arguments;
    } code_stmt;
    struct
    {
      String_Slice name;
      Node_Vector index_constraints;
      Node_Vector parameters;
      Node_Vector statements;
      Node_Vector handlers;
      Syntax_Node *guard;
    } accept_stmt;
    struct
    {
      Node_Vector alternatives;
      Node_Vector else_statements;
    } select_stmt;
    struct
    {
      uint8_t kind;
      Syntax_Node *guard;
      Node_Vector statements;
    } abort_stmt;
    struct
    {
      String_Slice name;
      Node_Vector entries;
      bool is_type;
    } task_spec;
    struct
    {
      String_Slice name;
      Node_Vector declarations;
      Node_Vector statements;
      Node_Vector handlers;
    } task_body;
    struct
    {
      String_Slice name;
      Node_Vector index_constraints;
      Node_Vector parameters;
      Syntax_Node *guard;
    } entry_decl;
    struct
    {
      Node_Vector exception_choices;
      Node_Vector statements;
    } exception_handler;
    struct
    {
      Node_Vector items;
    } choices;
    struct
    {
      Node_Vector choices;
      Syntax_Node *value;
    } association;
    struct
    {
      Node_Vector items;
    } list;
    struct
    {
      Node_Vector with_clauses;
      Node_Vector use_clauses;
    } context;
    struct
    {
      String_Slice name;
    } with_clause;
    struct
    {
      Syntax_Node *nm;
    } use_clause;
    struct
    {
      String_Slice name;
      Node_Vector arguments;
    } pragma;
    struct
    {
      Syntax_Node *context;
      Node_Vector units;
    } compilation_unit;
    struct
    {
      Syntax_Node *expression;
    } dereference;
    struct
    {
      Syntax_Node *ty, *expression;
    } conversion;
    struct
    {
      Syntax_Node *expression;
      String_Slice exception_name;
    } check;
    struct
    {
      Syntax_Node *base_type;
      Node_Vector operators;
    } derived_type;
    struct
    {
      Node_Vector formal_parameters;
      Node_Vector declarations;
      Syntax_Node *unit;
    } generic_decl;
    struct
    {
      String_Slice name;
      String_Slice generic_name;
      Node_Vector actual_parameters;
    } generic_inst;
    struct
    {
      Node_Vector items;
      Syntax_Node *low_bound, *high_bound;
      uint8_t dimensions;
    } aggregate;
  };
};
struct Representation_Clause
{
  uint8_t k;
  Type_Info *ty;
  union
  {
    struct
    {
      String_Slice name;
      uint32_t po;
    } er;
    struct
    {
      String_Slice name;
      uint64_t address;
    } ad;
    struct
    {
      String_Slice name;
      Node_Vector components;
    } rr;
    struct
    {
      String_Slice language;
      String_Slice name;
      String_Slice external_name;
    } im;
  };
};
struct Library_Unit
{
  uint8_t k;
  String_Slice name;
  String_Slice path;
  Syntax_Node *sp;
  Syntax_Node *body;
  Library_Unit_Vector wth;
  Library_Unit_Vector elb;
  uint64_t timestamp;
  bool is_compiled;
};
struct Generic_Template
{
  String_Slice name;
  Node_Vector formal_parameters;
  Node_Vector declarations;
  Syntax_Node *unit;
  Syntax_Node *body;
};
static Syntax_Node *node_new(Node_Kind k, Source_Location l)
{
  Syntax_Node *n = arena_allocate(sizeof(Syntax_Node));
  n->k = k;
  n->location = l;
  return n;
}
static Representation_Clause *reference_counter_new(uint8_t k, Type_Info *t)
{
  Representation_Clause *r = arena_allocate(sizeof(Representation_Clause));
  r->k = k;
  r->ty = t;
  return r;
}
static Library_Unit *label_use_new(uint8_t k, String_Slice nm, String_Slice pth)
{
  Library_Unit *l = arena_allocate(sizeof(Library_Unit));
  l->k = k;
  l->name = nm;
  l->path = pth;
  return l;
}
static Generic_Template *generic_type_new(String_Slice nm)
{
  Generic_Template *g = arena_allocate(sizeof(Generic_Template));
  g->name = nm;
  return g;
}
#define ND(k, l) node_new(N_##k, l)
typedef struct
{
  Lexer lexer;
  Token current_token, peek_token;
  int error_count;
  String_List_Vector label_stack;
} Parser;
static void parser_next(Parser *parser)
{
  parser->current_token = parser->peek_token;
  parser->peek_token = lexer_next_token(&parser->lexer);
  if (parser->current_token.kind == T_AND and parser->peek_token.kind == T_THEN)
  {
    parser->current_token.kind = T_ATHN;
    parser->peek_token = lexer_next_token(&parser->lexer);
  }
  if (parser->current_token.kind == T_OR and parser->peek_token.kind == T_ELSE)
  {
    parser->current_token.kind = T_OREL;
    parser->peek_token = lexer_next_token(&parser->lexer);
  }
}
static bool parser_at(Parser *parser, Token_Kind token_kind)
{
  return parser->current_token.kind == token_kind;
}
static bool parser_match(Parser *parser, Token_Kind token_kind)
{
  if (parser_at(parser, token_kind))
  {
    parser_next(parser);
    return 1;
  }
  return 0;
}
static void parser_expect(Parser *parser, Token_Kind token_kind)
{
  if (not parser_match(parser, token_kind))
    fatal_error(parser->current_token.location, "exp '%s' got '%s'", TN[token_kind], TN[parser->current_token.kind]);
}
static Source_Location parser_location(Parser *parser)
{
  return parser->current_token.location;
}
static String_Slice parser_identifier(Parser *parser)
{
  String_Slice identifier = string_duplicate(parser->current_token.literal);
  parser_expect(parser, T_ID);
  return identifier;
}
static String_Slice parser_attribute(Parser *parser)
{
  String_Slice s;
  if (parser_at(parser, T_ID))
    s = parser_identifier(parser);
  else if (parser_at(parser, T_RNG))
  {
    s = STRING_LITERAL("RANGE");
    parser_next(parser);
  }
  else if (parser_at(parser, T_ACCS))
  {
    s = STRING_LITERAL("ACCESS");
    parser_next(parser);
  }
  else if (parser_at(parser, T_DIG))
  {
    s = STRING_LITERAL("DIGITS");
    parser_next(parser);
  }
  else if (parser_at(parser, T_DELTA))
  {
    s = STRING_LITERAL("DELTA");
    parser_next(parser);
  }
  else if (parser_at(parser, T_MOD))
  {
    s = STRING_LITERAL("MOD");
    parser_next(parser);
  }
  else if (parser_at(parser, T_REM))
  {
    s = STRING_LITERAL("REM");
    parser_next(parser);
  }
  else if (parser_at(parser, T_ABS))
  {
    s = STRING_LITERAL("ABS");
    parser_next(parser);
  }
  else if (parser_at(parser, T_NOT))
  {
    s = STRING_LITERAL("NOT");
    parser_next(parser);
  }
  else if (parser_at(parser, T_AND))
  {
    s = STRING_LITERAL("AND");
    parser_next(parser);
  }
  else if (parser_at(parser, T_OR))
  {
    s = STRING_LITERAL("OR");
    parser_next(parser);
  }
  else if (parser_at(parser, T_XOR))
  {
    s = STRING_LITERAL("XOR");
    parser_next(parser);
  }
  else if (parser_at(parser, T_PL))
  {
    s = STRING_LITERAL("+");
    parser_next(parser);
  }
  else if (parser_at(parser, T_MN))
  {
    s = STRING_LITERAL("-");
    parser_next(parser);
  }
  else if (parser_at(parser, T_ST))
  {
    s = STRING_LITERAL("*");
    parser_next(parser);
  }
  else if (parser_at(parser, T_SL))
  {
    s = STRING_LITERAL("/");
    parser_next(parser);
  }
  else if (parser_at(parser, T_EQ))
  {
    s = STRING_LITERAL("=");
    parser_next(parser);
  }
  else if (parser_at(parser, T_NE))
  {
    s = STRING_LITERAL("/=");
    parser_next(parser);
  }
  else if (parser_at(parser, T_LT))
  {
    s = STRING_LITERAL("<");
    parser_next(parser);
  }
  else if (parser_at(parser, T_LE))
  {
    s = STRING_LITERAL("<=");
    parser_next(parser);
  }
  else if (parser_at(parser, T_GT))
  {
    s = STRING_LITERAL(">");
    parser_next(parser);
  }
  else if (parser_at(parser, T_GE))
  {
    s = STRING_LITERAL(">=");
    parser_next(parser);
  }
  else if (parser_at(parser, T_AM))
  {
    s = STRING_LITERAL("&");
    parser_next(parser);
  }
  else if (parser_at(parser, T_EX))
  {
    s = STRING_LITERAL("**");
    parser_next(parser);
  }
  else
    fatal_error(parser_location(parser), "exp attr");
  return s;
}
static Syntax_Node *parse_name(Parser *parser);
static Syntax_Node *parse_expression(Parser *parser);
static Syntax_Node *parse_primary(Parser *parser);
static Syntax_Node *parse_range(Parser *parser);
static Node_Vector parse_statement(Parser *parser);
static Node_Vector parse_declarative_part(Parser *parser);
static Node_Vector parse_handle_declaration(Parser *parser);
static Syntax_Node *parse_statement_or_label(Parser *parser);
static Syntax_Node *parse_generic_formal(Parser *parser);
static Representation_Clause *parse_representation_clause(Parser *parser);
static Syntax_Node *parse_primary(Parser *parser)
{
  Source_Location location = parser_location(parser);
  if (parser_match(parser, T_LP))
  {
    Node_Vector aggregate_vector = {0};
    do
    {
      Node_Vector choices = {0};
      Syntax_Node *expression = parse_expression(parser);
      nv(&choices, expression);
      while (parser_match(parser, T_BR))
        nv(&choices, parse_expression(parser));
      if (parser_at(parser, T_AR))
      {
        parser_next(parser);
        Syntax_Node *value = parse_expression(parser);
        for (uint32_t i = 0; i < choices.count; i++)
        {
          Syntax_Node *association = ND(ASC, location);
          nv(&association->association.choices, choices.data[i]);
          association->association.value = value;
          nv(&aggregate_vector, association);
        }
      }
      else if (choices.count == 1 and choices.data[0]->k == N_ID and parser_match(parser, T_RNG))
      {
        Syntax_Node *range = parse_range(parser);
        parser_expect(parser, T_AR);
        Syntax_Node *value = parse_expression(parser);
        Syntax_Node *slice_iterator = ND(ST, location);
        Syntax_Node *constraint = ND(CN, location);
        constraint->constraint.range_spec = range;
        slice_iterator->subtype_decl.index_constraint = choices.data[0];
        slice_iterator->subtype_decl.constraint = constraint;
        Syntax_Node *association = ND(ASC, location);
        nv(&association->association.choices, slice_iterator);
        association->association.value = value;
        nv(&aggregate_vector, association);
      }
      else
      {
        if (choices.count == 1)
          nv(&aggregate_vector, choices.data[0]);
        else
          fatal_error(location, "exp '=>'");
      }
    } while (parser_match(parser, T_CM));
    parser_expect(parser, T_RP);
    if (aggregate_vector.count == 1 and aggregate_vector.data[0]->k != N_ASC)
      return aggregate_vector.data[0];
    Syntax_Node *node = ND(AG, location);
    node->aggregate.items = aggregate_vector;
    return node;
  }
  if (parser_match(parser, T_NEW))
  {
    Syntax_Node *node = ND(ALC, location);
    node->allocator.subtype = parse_name(parser);
    if (parser_match(parser, T_TK))
    {
      parser_expect(parser, T_LP);
      node->allocator.initializer = parse_expression(parser);
      parser_expect(parser, T_RP);
    }
    return node;
  }
  if (parser_match(parser, T_NULL))
    return ND(NULL, location);
  if (parser_match(parser, T_OTH))
  {
    Syntax_Node *node = ND(ID, location);
    node->string_value = STRING_LITERAL("others");
    return node;
  }
  if (parser_at(parser, T_INT))
  {
    Syntax_Node *node = ND(INT, location);
    node->integer_value = parser->current_token.integer_value;
    parser_next(parser);
    return node;
  }
  if (parser_at(parser, T_REAL))
  {
    Syntax_Node *node = ND(REAL, location);
    node->float_value = parser->current_token.float_value;
    parser_next(parser);
    return node;
  }
  if (parser_at(parser, T_CHAR))
  {
    Syntax_Node *node = ND(CHAR, location);
    node->integer_value = parser->current_token.integer_value;
    parser_next(parser);
    return node;
  }
  if (parser_at(parser, T_STR))
  {
    Syntax_Node *node = ND(STR, location);
    node->string_value = string_duplicate(parser->current_token.literal);
    parser_next(parser);
    for (;;)
    {
      if (parser_at(parser, T_LP))
      {
        parser_next(parser);
        Node_Vector aggregate_vector = {0};
        do
        {
          Node_Vector choices = {0};
          Syntax_Node *expression = parse_expression(parser);
          if (expression->k == N_ID and parser_at(parser, T_AR))
          {
            parser_next(parser);
            Syntax_Node *association = ND(ASC, location);
            nv(&association->association.choices, expression);
            association->association.value = parse_expression(parser);
            nv(&aggregate_vector, association);
          }
          else
          {
            nv(&choices, expression);
            while (parser_match(parser, T_BR))
              nv(&choices, parse_expression(parser));
            if (parser_at(parser, T_AR))
            {
              parser_next(parser);
              Syntax_Node *value = parse_expression(parser);
              for (uint32_t i = 0; i < choices.count; i++)
              {
                Syntax_Node *association = ND(ASC, location);
                nv(&association->association.choices, choices.data[i]);
                association->association.value = value;
                nv(&aggregate_vector, association);
              }
            }
            else
            {
              if (choices.count == 1)
                nv(&aggregate_vector, choices.data[0]);
              else
                fatal_error(location, "exp '=>'");
            }
          }
        } while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
        Syntax_Node *m = ND(CL, location);
        m->call.function_name = node;
        m->call.arguments = aggregate_vector;
        node = m;
      }
      else
        break;
    }
    return node;
  }
  if (parser_at(parser, T_ID))
    return parse_name(parser);
  if (parser_match(parser, T_NOT))
  {
    Syntax_Node *node = ND(UN, location);
    node->unary_node.op = T_NOT;
    node->unary_node.operand = parse_primary(parser);
    return node;
  }
  if (parser_match(parser, T_ABS))
  {
    Syntax_Node *node = ND(UN, location);
    node->unary_node.op = T_ABS;
    node->unary_node.operand = parse_primary(parser);
    return node;
  }
  if (parser_match(parser, T_ALL))
  {
    Syntax_Node *node = ND(DRF, location);
    node->dereference.expression = parse_primary(parser);
    return node;
  }
  fatal_error(location, "exp expr");
}
static Syntax_Node *parse_name(Parser *parser)
{
  Source_Location location = parser_location(parser);
  Syntax_Node *node = ND(ID, location);
  node->string_value = parser_identifier(parser);
  for (;;)
  {
    if (parser_match(parser, T_DT))
    {
      if (parser_match(parser, T_ALL))
      {
        Syntax_Node *modified_node = ND(DRF, location);
        modified_node->dereference.expression = node;
        node = modified_node;
      }
      else
      {
        Syntax_Node *modified_node = ND(SEL, location);
        modified_node->selected_component.prefix = node;
        if (parser_at(parser, T_STR))
        {
          modified_node->selected_component.selector = string_duplicate(parser->current_token.literal);
          parser_next(parser);
        }
        else if (parser_at(parser, T_CHAR))
        {
          char *c = arena_allocate(2);
          c[0] = parser->current_token.integer_value;
          c[1] = 0;
          modified_node->selected_component.selector = (String_Slice){c, 1};
          parser_next(parser);
        }
        else
          modified_node->selected_component.selector = parser_identifier(parser);
        node = modified_node;
      }
    }
    else if (parser_match(parser, T_TK))
    {
      if (parser_at(parser, T_LP))
      {
        parser_next(parser);
        Syntax_Node *modified_node = ND(QL, location);
        modified_node->qualified.name = node;
        Node_Vector v = {0};
        do
        {
          Node_Vector ch = {0};
          Syntax_Node *e = parse_expression(parser);
          nv(&ch, e);
          while (parser_match(parser, T_BR))
            nv(&ch, parse_expression(parser));
          if (parser_at(parser, T_AR))
          {
            parser_next(parser);
            Syntax_Node *vl = parse_expression(parser);
            for (uint32_t i = 0; i < ch.count; i++)
            {
              Syntax_Node *a = ND(ASC, location);
              nv(&a->association.choices, ch.data[i]);
              a->association.value = vl;
              nv(&v, a);
            }
          }
          else
          {
            if (ch.count == 1)
              nv(&v, ch.data[0]);
            else
              fatal_error(location, "exp '=>'");
          }
        } while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
        if (v.count == 1 and v.data[0]->k != N_ASC)
          modified_node->qualified.aggregate = v.data[0];
        else
        {
          Syntax_Node *ag = ND(AG, location);
          ag->aggregate.items = v;
          modified_node->qualified.aggregate = ag;
        }
        node = modified_node;
      }
      else
      {
        String_Slice at = parser_attribute(parser);
        Syntax_Node *modified_node = ND(AT, location);
        modified_node->attribute.prefix = node;
        modified_node->attribute.attribute_name = at;
        if (parser_match(parser, T_LP))
        {
          do
            nv(&modified_node->attribute.arguments, parse_expression(parser));
          while (parser_match(parser, T_CM));
          parser_expect(parser, T_RP);
        }
        node = modified_node;
      }
    }
    else if (parser_at(parser, T_LP))
    {
      parser_next(parser);
      if (parser_at(parser, T_RP))
      {
        parser_expect(parser, T_RP);
        Syntax_Node *modified_node = ND(CL, location);
        modified_node->call.function_name = node;
        node = modified_node;
      }
      else
      {
        Node_Vector v = {0};
        do
        {
          Node_Vector ch = {0};
          Syntax_Node *e = parse_expression(parser);
          if (e->k == N_ID and parser_at(parser, T_AR))
          {
            parser_next(parser);
            Syntax_Node *a = ND(ASC, location);
            nv(&a->association.choices, e);
            a->association.value = parse_expression(parser);
            nv(&v, a);
          }
          else
          {
            nv(&ch, e);
            while (parser_match(parser, T_BR))
              nv(&ch, parse_expression(parser));
            if (parser_at(parser, T_AR))
            {
              parser_next(parser);
              Syntax_Node *vl = parse_expression(parser);
              for (uint32_t i = 0; i < ch.count; i++)
              {
                Syntax_Node *a = ND(ASC, location);
                nv(&a->association.choices, ch.data[i]);
                a->association.value = vl;
                nv(&v, a);
              }
            }
            else
            {
              if (ch.count == 1)
                nv(&v, ch.data[0]);
              else
                fatal_error(location, "exp '=>'");
            }
          }
        } while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
        Syntax_Node *modified_node = ND(CL, location);
        modified_node->call.function_name = node;
        modified_node->call.arguments = v;
        node = modified_node;
      }
    }
    else
      break;
  }
  return node;
}
static Syntax_Node *parse_power_expression(Parser *parser)
{
  Syntax_Node *node = parse_primary(parser);
  if (parser_match(parser, T_EX))
  {
    Source_Location location = parser_location(parser);
    Syntax_Node *binary_node = ND(BIN, location);
    binary_node->binary_node.op = T_EX;
    binary_node->binary_node.left = node;
    binary_node->binary_node.right = parse_power_expression(parser);
    return binary_node;
  }
  return node;
}
static Syntax_Node *parse_term(Parser *parser)
{
  Syntax_Node *node = parse_power_expression(parser);
  while (parser_at(parser, T_ST) or parser_at(parser, T_SL) or parser_at(parser, T_MOD) or parser_at(parser, T_REM))
  {
    Token_Kind operator = parser->current_token.kind;
    parser_next(parser);
    Source_Location location = parser_location(parser);
    Syntax_Node *binary_node = ND(BIN, location);
    binary_node->binary_node.op = operator;
    binary_node->binary_node.left = node;
    binary_node->binary_node.right = parse_power_expression(parser);
    node = binary_node;
  }
  return node;
}
static Syntax_Node *parse_signed_term(Parser *parser)
{
  Source_Location location = parser_location(parser);
  Token_Kind unary_operator = 0;
  if (parser_match(parser, T_MN))
    unary_operator = T_MN;
  else if (parser_match(parser, T_PL))
    unary_operator = T_PL;
  Syntax_Node *node = parse_term(parser);
  if (unary_operator)
  {
    Syntax_Node *unary_node = ND(UN, location);
    unary_node->unary_node.op = unary_operator;
    unary_node->unary_node.operand = node;
    node = unary_node;
  }
  while (parser_at(parser, T_PL) or parser_at(parser, T_MN) or parser_at(parser, T_AM))
  {
    Token_Kind operator = parser->current_token.kind;
    parser_next(parser);
    location = parser_location(parser);
    Syntax_Node *binary_node = ND(BIN, location);
    binary_node->binary_node.op = operator;
    binary_node->binary_node.left = node;
    binary_node->binary_node.right = parse_term(parser);
    node = binary_node;
  }
  return node;
}
static Syntax_Node *parse_relational(Parser *parser)
{
  Syntax_Node *node = parse_signed_term(parser);
  if (parser_match(parser, T_DD))
  {
    Source_Location location = parser_location(parser);
    Syntax_Node *binary_node = ND(RN, location);
    binary_node->range.low_bound = node;
    binary_node->range.high_bound = parse_signed_term(parser);
    return binary_node;
  }
  if (parser_at(parser, T_EQ) or parser_at(parser, T_NE) or parser_at(parser, T_LT) or parser_at(parser, T_LE)
      or parser_at(parser, T_GT) or parser_at(parser, T_GE) or parser_at(parser, T_IN) or parser_at(parser, T_NOT))
  {
    Token_Kind operator = parser->current_token.kind;
    parser_next(parser);
    if (operator == T_NOT)
      parser_expect(parser, T_IN);
    Source_Location location = parser_location(parser);
    Syntax_Node *binary_node = ND(BIN, location);
    binary_node->binary_node.op = operator;
    binary_node->binary_node.left = node;
    if (operator == T_IN or operator == T_NOT)
      binary_node->binary_node.right = parse_range(parser);
    else
      binary_node->binary_node.right = parse_signed_term(parser);
    return binary_node;
  }
  return node;
}
static Syntax_Node *parse_and_expression(Parser *parser)
{
  Syntax_Node *node = parse_relational(parser);
  while (parser_at(parser, T_AND) or parser_at(parser, T_ATHN))
  {
    Token_Kind operator = parser->current_token.kind;
    parser_next(parser);
    Source_Location location = parser_location(parser);
    Syntax_Node *binary_node = ND(BIN, location);
    binary_node->binary_node.op = operator;
    binary_node->binary_node.left = node;
    binary_node->binary_node.right = parse_relational(parser);
    node = binary_node;
  }
  return node;
}
static Syntax_Node *parse_or_expression(Parser *parser)
{
  Syntax_Node *node = parse_and_expression(parser);
  while (parser_at(parser, T_OR) or parser_at(parser, T_OREL) or parser_at(parser, T_XOR))
  {
    Token_Kind operator = parser->current_token.kind;
    parser_next(parser);
    Source_Location location = parser_location(parser);
    Syntax_Node *binary_node = ND(BIN, location);
    binary_node->binary_node.op = operator;
    binary_node->binary_node.left = node;
    binary_node->binary_node.right = parse_and_expression(parser);
    node = binary_node;
  }
  return node;
}
static Syntax_Node *parse_expression(Parser *parser)
{
  return parse_or_expression(parser);
}
static Syntax_Node *parse_range(Parser *parser)
{
  Source_Location location = parser_location(parser);
  if (parser_match(parser, T_BX))
  {
    Syntax_Node *node = ND(RN, location);
    node->range.low_bound = 0;
    node->range.high_bound = 0;
    return node;
  }
  Syntax_Node *low_bound = parse_signed_term(parser);
  if (parser_match(parser, T_DD))
  {
    Syntax_Node *range_node = ND(RN, location);
    range_node->range.low_bound = low_bound;
    range_node->range.high_bound = parse_signed_term(parser);
    return range_node;
  }
  return low_bound;
}
static Syntax_Node *parse_simple_expression(Parser *parser)
{
  Source_Location location = parser_location(parser);
  Syntax_Node *node = ND(ID, location);
  node->string_value = parser_identifier(parser);
  for (;;)
  {
    if (parser_match(parser, T_DT))
    {
      if (parser_match(parser, T_ALL))
      {
        Syntax_Node *modified_node = ND(DRF, location);
        modified_node->dereference.expression = node;
        node = modified_node;
      }
      else
      {
        Syntax_Node *modified_node = ND(SEL, location);
        modified_node->selected_component.prefix = node;
        modified_node->selected_component.selector = parser_identifier(parser);
        node = modified_node;
      }
    }
    else if (parser_match(parser, T_TK))
    {
      String_Slice attribute = parser_attribute(parser);
      Syntax_Node *modified_node = ND(AT, location);
      modified_node->attribute.prefix = node;
      modified_node->attribute.attribute_name = attribute;
      if (parser_match(parser, T_LP))
      {
        do
          nv(&modified_node->attribute.arguments, parse_expression(parser));
        while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
      }
      node = modified_node;
    }
    else
      break;
  }
  if (parser_match(parser, T_DELTA))
  {
    parse_signed_term(parser);
  }
  if (parser_match(parser, T_DIG))
  {
    parse_expression(parser);
  }
  if (parser_match(parser, T_RNG))
  {
    Source_Location location = parser_location(parser);
    Syntax_Node *c = ND(CN, location);
    c->constraint.range_spec = parse_range(parser);
    Syntax_Node *modified_node = ND(ST, location);
    modified_node->subtype_decl.index_constraint = node;
    modified_node->subtype_decl.constraint = c;
    return modified_node;
  }
  if (parser_at(parser, T_LP))
  {
    parser_next(parser);
    Source_Location location = parser_location(parser);
    Syntax_Node *c = ND(CN, location);
    do
    {
      Source_Location lc2 = parser_location(parser);
      Node_Vector ch = {0};
      Syntax_Node *r = parse_range(parser);
      nv(&ch, r);
      while (parser_match(parser, T_BR))
        nv(&ch, parse_range(parser));
      if (ch.count > 0 and ch.data[0]->k == N_ID and parser_match(parser, T_RNG))
      {
        Syntax_Node *tn = ND(ID, lc2);
        tn->string_value = ch.data[0]->string_value;
        Syntax_Node *rng = parse_range(parser);
        Syntax_Node *si = ND(ST, lc2);
        Syntax_Node *cn = ND(CN, lc2);
        cn->constraint.range_spec = rng;
        si->subtype_decl.index_constraint = tn;
        si->subtype_decl.constraint = cn;
        nv(&c->constraint.constraints, si);
      }
      else if (ch.count > 0 and ch.data[0]->k == N_ID and parser_match(parser, T_AR))
      {
        Syntax_Node *vl = parse_expression(parser);
        for (uint32_t i = 0; i < ch.count; i++)
        {
          Syntax_Node *a = ND(ASC, lc2);
          nv(&a->association.choices, ch.data[i]);
          a->association.value = vl;
          nv(&c->constraint.constraints, a);
        }
      }
      else
      {
        if (ch.count > 0)
          nv(&c->constraint.constraints, ch.data[0]);
      }
    } while (parser_match(parser, T_CM));
    parser_expect(parser, T_RP);
    Syntax_Node *modified_node = ND(ST, location);
    modified_node->subtype_decl.index_constraint = node;
    modified_node->subtype_decl.constraint = c;
    return modified_node;
  }
  return node;
}
static Node_Vector parse_parameter_mode(Parser *parser)
{
  Node_Vector params = {0};
  if (not parser_match(parser, T_LP))
    return params;
  do
  {
    Source_Location location = parser_location(parser);
    Node_Vector id = {0};
    do
    {
      String_Slice nm = parser_identifier(parser);
      Syntax_Node *i = ND(ID, location);
      i->string_value = nm;
      nv(&id, i);
    } while (parser_match(parser, T_CM));
    parser_expect(parser, T_CL);
    uint8_t md = 0;
    if (parser_match(parser, T_IN))
      md |= 1;
    if (parser_match(parser, T_OUT))
      md |= 2;
    if (not md)
      md = 1;
    Syntax_Node *ty = parse_name(parser);
    Syntax_Node *df = 0;
    if (parser_match(parser, T_AS))
      df = parse_expression(parser);
    for (uint32_t i = 0; i < id.count; i++)
    {
      Syntax_Node *node = ND(PM, location);
      node->parameter.name = id.data[i]->string_value;
      node->parameter.ty = ty;
      node->parameter.default_value = df;
      node->parameter.mode = md;
      nv(&params, node);
    }
  } while (parser_match(parser, T_SC));
  parser_expect(parser, T_RP);
  return params;
}
static Syntax_Node *parse_procedure_specification(Parser *parser)
{
  Source_Location location = parser_location(parser);
  parser_expect(parser, T_PROC);
  Syntax_Node *node = ND(PS, location);
  if (parser_at(parser, T_STR))
  {
    node->subprogram.name = string_duplicate(parser->current_token.literal);
    parser_next(parser);
  }
  else
    node->subprogram.name = parser_identifier(parser);
  node->subprogram.parameters = parse_parameter_mode(parser);
  return node;
}
static Syntax_Node *parse_function_specification(Parser *parser)
{
  Source_Location location = parser_location(parser);
  parser_expect(parser, T_FUN);
  Syntax_Node *node = ND(FS, location);
  if (parser_at(parser, T_STR))
  {
    node->subprogram.name = string_duplicate(parser->current_token.literal);
    parser_next(parser);
  }
  else
    node->subprogram.name = parser_identifier(parser);
  node->subprogram.parameters = parse_parameter_mode(parser);
  parser_expect(parser, T_RET);
  node->subprogram.return_type = parse_name(parser);
  return node;
}
static Syntax_Node *parse_type_definition(Parser *parser);
static Node_Vector parse_generic_formal_part(Parser *parser)
{
  Node_Vector generics = {0};
  while (not parser_at(parser, T_PROC) and not parser_at(parser, T_FUN) and not parser_at(parser, T_PKG))
  {
    if (parser_match(parser, T_TYP))
    {
      Source_Location location = parser_location(parser);
      String_Slice nm = parser_identifier(parser);
      bool isp = false;
      if (parser_match(parser, T_LP))
      {
        while (not parser_at(parser, T_RP))
          parser_next(parser);
        parser_expect(parser, T_RP);
      }
      if (parser_match(parser, T_IS))
      {
        if (parser_match(parser, T_DIG) or parser_match(parser, T_DELTA) or parser_match(parser, T_RNG))
        {
          parser_expect(parser, T_BX);
        }
        else if (parser_match(parser, T_LP))
        {
          parser_expect(parser, T_BX);
          parser_expect(parser, T_RP);
          isp = true;
          (void) isp;
        }
        else if (
            parser_match(parser, T_LIM) or parser_at(parser, T_ARR) or parser_at(parser, T_REC)
            or parser_at(parser, T_ACCS) or parser_at(parser, T_PRV))
        {
          parse_type_definition(parser);
        }
        else
          parse_expression(parser);
      }
      Syntax_Node *node = ND(GTP, location);
      node->type_decl.name = nm;
      nv(&generics, node);
      parser_expect(parser, T_SC);
    }
    else if (parser_match(parser, T_WITH))
    {
      if (parser_at(parser, T_PROC))
      {
        Syntax_Node *sp = parse_procedure_specification(parser);
        sp->k = N_GSP;
        if (parser_match(parser, T_IS))
        {
          if (not parser_match(parser, T_BX))
          {
            while (not parser_at(parser, T_SC))
              parser_next(parser);
          }
        }
        nv(&generics, sp);
      }
      else if (parser_at(parser, T_FUN))
      {
        Syntax_Node *sp = parse_function_specification(parser);
        sp->k = N_GSP;
        if (parser_match(parser, T_IS))
        {
          if (not parser_match(parser, T_BX))
          {
            while (not parser_at(parser, T_SC))
              parser_next(parser);
          }
        }
        nv(&generics, sp);
      }
      else
      {
        Source_Location location = parser_location(parser);
        Node_Vector id = {0};
        do
        {
          String_Slice nm = parser_identifier(parser);
          Syntax_Node *i = ND(ID, location);
          i->string_value = nm;
          nv(&id, i);
        } while (parser_match(parser, T_CM));
        parser_expect(parser, T_CL);
        uint8_t md = 0;
        if (parser_match(parser, T_IN))
          md |= 1;
        if (parser_match(parser, T_OUT))
          md |= 2;
        if (not md)
          md = 1;
        Syntax_Node *ty = parse_name(parser);
        parser_match(parser, T_AS);
        if (not parser_at(parser, T_SC))
          parse_expression(parser);
        Syntax_Node *node = ND(GVL, location);
        node->object_decl.identifiers = id;
        node->object_decl.ty = ty;
        nv(&generics, node);
      }
      parser_expect(parser, T_SC);
    }
    else
    {
      Source_Location location = parser_location(parser);
      Node_Vector id = {0};
      do
      {
        String_Slice nm = parser_identifier(parser);
        Syntax_Node *i = ND(ID, location);
        i->string_value = nm;
        nv(&id, i);
      } while (parser_match(parser, T_CM));
      parser_expect(parser, T_CL);
      uint8_t md = 0;
      if (parser_match(parser, T_IN))
        md |= 1;
      if (parser_match(parser, T_OUT))
        md |= 2;
      if (not md)
        md = 1;
      Syntax_Node *ty = parse_name(parser);
      parser_match(parser, T_AS);
      if (not parser_at(parser, T_SC))
        parse_expression(parser);
      Syntax_Node *node = ND(GVL, location);
      node->object_decl.identifiers = id;
      node->object_decl.ty = ty;
      nv(&generics, node);
      parser_expect(parser, T_SC);
    }
  }
  return generics;
}
static Syntax_Node *parse_generic_formal(Parser *parser)
{
  Source_Location location = parser_location(parser);
  parser_expect(parser, T_GEN);
  Syntax_Node *node = ND(GEN, location);
  node->generic_decl.formal_parameters = parse_generic_formal_part(parser);
  if (parser_at(parser, T_PROC))
  {
    Syntax_Node *sp = parse_procedure_specification(parser);
    parser_expect(parser, T_SC);
    node->generic_decl.unit = ND(PD, location);
    node->generic_decl.unit->body.subprogram_spec = sp;
    return node;
  }
  if (parser_at(parser, T_FUN))
  {
    Syntax_Node *sp = parse_function_specification(parser);
    parser_expect(parser, T_SC);
    node->generic_decl.unit = ND(FD, location);
    node->generic_decl.unit->body.subprogram_spec = sp;
    return node;
  }
  if (parser_match(parser, T_PKG))
  {
    String_Slice nm = parser_identifier(parser);
    parser_expect(parser, T_IS);
    Node_Vector dc = parse_declarative_part(parser);
    if (parser_match(parser, T_PRV))
    {
      Node_Vector pr = parse_declarative_part(parser);
      for (uint32_t i = 0; i < pr.count; i++)
        nv(&dc, pr.data[i]);
    }
    node->generic_decl.declarations = dc;
    parser_expect(parser, T_END);
    if (parser_at(parser, T_ID))
      parser_next(parser);
    parser_expect(parser, T_SC);
    Syntax_Node *pk = ND(PKS, location);
    pk->package_spec.name = nm;
    pk->package_spec.declarations = node->generic_decl.declarations;
    node->generic_decl.unit = pk;
    return node;
  }
  return node;
}
static Syntax_Node *parse_if(Parser *parser)
{
  Source_Location location = parser_location(parser);
  parser_expect(parser, T_IF);
  Syntax_Node *node = ND(IF, location);
  node->if_stmt.condition = parse_expression(parser);
  parser_expect(parser, T_THEN);
  while (not parser_at(parser, T_ELSIF) and not parser_at(parser, T_ELSE) and not parser_at(parser, T_END))
    nv(&node->if_stmt.then_statements, parse_statement_or_label(parser));
  while (parser_match(parser, T_ELSIF))
  {
    Syntax_Node *elsif_node = ND(EL, location);
    elsif_node->if_stmt.condition = parse_expression(parser);
    parser_expect(parser, T_THEN);
    while (not parser_at(parser, T_ELSIF) and not parser_at(parser, T_ELSE) and not parser_at(parser, T_END))
      nv(&elsif_node->if_stmt.then_statements, parse_statement_or_label(parser));
    nv(&node->if_stmt.elsif_statements, elsif_node);
  }
  if (parser_match(parser, T_ELSE))
    while (not parser_at(parser, T_END))
      nv(&node->if_stmt.else_statements, parse_statement_or_label(parser));
  parser_expect(parser, T_END);
  parser_expect(parser, T_IF);
  parser_expect(parser, T_SC);
  return node;
}
static Syntax_Node *parse_case(Parser *parser)
{
  Source_Location location = parser_location(parser);
  parser_expect(parser, T_CSE);
  Syntax_Node *node = ND(CS, location);
  node->case_stmt.expression = parse_expression(parser);
  parser_expect(parser, T_IS);
  while (parser_at(parser, T_PGM))
    parse_representation_clause(parser);
  while (parser_match(parser, T_WHN))
  {
    Syntax_Node *a = ND(WH, location);
    do
    {
      Syntax_Node *elsif_node = parse_expression(parser);
      if (elsif_node->k == N_ID and parser_match(parser, T_RNG))
      {
        Syntax_Node *r = parse_range(parser);
        nv(&a->choices.items, r);
      }
      else if (parser_match(parser, T_DD))
      {
        Syntax_Node *r = ND(RN, location);
        r->range.low_bound = elsif_node;
        r->range.high_bound = parse_expression(parser);
        nv(&a->choices.items, r);
      }
      else
        nv(&a->choices.items, elsif_node);
    } while (parser_match(parser, T_BR));
    parser_expect(parser, T_AR);
    while (not parser_at(parser, T_WHN) and not parser_at(parser, T_END))
      nv(&a->exception_handler.statements, parse_statement_or_label(parser));
    nv(&node->case_stmt.alternatives, a);
  }
  parser_expect(parser, T_END);
  parser_expect(parser, T_CSE);
  parser_expect(parser, T_SC);
  return node;
}
static Syntax_Node *parse_loop(Parser *parser, String_Slice label)
{
  Source_Location location = parser_location(parser);
  Syntax_Node *node = ND(LP, location);
  node->loop_stmt.label = label;
  if (parser_match(parser, T_WHI))
    node->loop_stmt.iterator = parse_expression(parser);
  else if (parser_match(parser, T_FOR))
  {
    String_Slice vr = parser_identifier(parser);
    parser_expect(parser, T_IN);
    node->loop_stmt.is_reverse = parser_match(parser, T_REV);
    Syntax_Node *rng = parse_range(parser);
    if (parser_match(parser, T_RNG))
    {
      Syntax_Node *r = ND(RN, location);
      r->range.low_bound = parse_signed_term(parser);
      parser_expect(parser, T_DD);
      r->range.high_bound = parse_signed_term(parser);
      rng = r;
    }
    Syntax_Node *it = ND(BIN, location);
    it->binary_node.op = T_IN;
    it->binary_node.left = ND(ID, location);
    it->binary_node.left->string_value = vr;
    it->binary_node.right = rng;
    node->loop_stmt.iterator = it;
  }
  parser_expect(parser, T_LOOP);
  while (not parser_at(parser, T_END))
    nv(&node->loop_stmt.statements, parse_statement_or_label(parser));
  parser_expect(parser, T_END);
  parser_expect(parser, T_LOOP);
  if (parser_at(parser, T_ID))
    parser_next(parser);
  parser_expect(parser, T_SC);
  return node;
}
static Syntax_Node *parse_block(Parser *parser, String_Slice label)
{
  Source_Location location = parser_location(parser);
  Syntax_Node *node = ND(BL, location);
  node->block.label = label;
  if (parser_match(parser, T_DEC))
    node->block.declarations = parse_declarative_part(parser);
  parser_expect(parser, T_BEG);
  while (not parser_at(parser, T_EXCP) and not parser_at(parser, T_END))
    nv(&node->block.statements, parse_statement_or_label(parser));
  if (parser_match(parser, T_EXCP))
    node->block.handlers = parse_handle_declaration(parser);
  parser_expect(parser, T_END);
  if (parser_at(parser, T_ID))
    parser_next(parser);
  parser_expect(parser, T_SC);
  return node;
}
static Syntax_Node *parse_statement_list(Parser *parser)
{
  Source_Location location = parser_location(parser);
  parser_expect(parser, T_SEL);
  Syntax_Node *node = ND(SA, location);
  node->abort_stmt.kind = 0;
  if (parser_match(parser, T_DEL))
  {
    node->abort_stmt.kind = 1;
    node->abort_stmt.guard = parse_expression(parser);
    parser_expect(parser, T_THEN);
    if (parser_match(parser, T_AB))
      node->abort_stmt.kind = 3;
    while (not parser_at(parser, T_OR) and not parser_at(parser, T_ELSE) and not parser_at(parser, T_END))
      nv(&node->abort_stmt.statements, parse_statement_or_label(parser));
  }
  else if (parser_at(parser, T_WHN))
  {
    while (parser_match(parser, T_WHN))
    {
      Syntax_Node *alternative = ND(WH, location);
      do
        nv(&alternative->choices.items, parse_expression(parser));
      while (parser_match(parser, T_BR));
      parser_expect(parser, T_AR);
      if (parser_match(parser, T_ACC))
      {
        alternative->k = N_ACC;
        alternative->accept_stmt.name = parser_identifier(parser);
        if (parser_at(parser, T_LP))
        {
          if (parser->peek_token.kind == T_ID)
          {
            Token saved_current_token = parser->current_token, saved_peek_token = parser->peek_token;
            Lexer saved_lexer = parser->lexer;
            parser_next(parser);
            parser_next(parser);
            if (parser->current_token.kind == T_CM or parser->current_token.kind == T_CL)
            {
              parser->current_token = saved_current_token;
              parser->peek_token = saved_peek_token;
              parser->lexer = saved_lexer;
              alternative->accept_stmt.parameters = parse_parameter_mode(parser);
            }
            else
            {
              parser->current_token = saved_current_token;
              parser->peek_token = saved_peek_token;
              parser->lexer = saved_lexer;
              parser_expect(parser, T_LP);
              do
                nv(&alternative->accept_stmt.index_constraints, parse_expression(parser));
              while (parser_match(parser, T_CM));
              parser_expect(parser, T_RP);
              alternative->accept_stmt.parameters = parse_parameter_mode(parser);
            }
          }
          else
          {
            parser_expect(parser, T_LP);
            do
              nv(&alternative->accept_stmt.index_constraints, parse_expression(parser));
            while (parser_match(parser, T_CM));
            parser_expect(parser, T_RP);
            alternative->accept_stmt.parameters = parse_parameter_mode(parser);
          }
        }
        else
        {
          alternative->accept_stmt.parameters = parse_parameter_mode(parser);
        }
        if (parser_match(parser, T_DO))
        {
          while (not parser_at(parser, T_END) and not parser_at(parser, T_OR) and not parser_at(parser, T_ELSE))
            nv(&alternative->accept_stmt.statements, parse_statement_or_label(parser));
          parser_expect(parser, T_END);
          if (parser_at(parser, T_ID))
            parser_next(parser);
        }
        while (not parser_at(parser, T_OR) and not parser_at(parser, T_ELSE) and not parser_at(parser, T_END)
               and not parser_at(parser, T_WHN))
          nv(&alternative->exception_handler.statements, parse_statement_or_label(parser));
      }
      else if (parser_match(parser, T_TER))
      {
        alternative->k = N_TRM;
      }
      else if (parser_match(parser, T_DEL))
      {
        alternative->k = N_DL;
        alternative->exit_stmt.condition = parse_expression(parser);
        parser_expect(parser, T_THEN);
        while (not parser_at(parser, T_OR) and not parser_at(parser, T_ELSE) and not parser_at(parser, T_END))
          nv(&alternative->exception_handler.statements, parse_statement_or_label(parser));
      }
      nv(&node->abort_stmt.statements, alternative);
    }
  }
  else
  {
    do
    {
      Syntax_Node *alternative = ND(WH, location);
      if (parser_match(parser, T_ACC))
      {
        alternative->k = N_ACC;
        alternative->accept_stmt.name = parser_identifier(parser);
        if (parser_at(parser, T_LP))
        {
          if (parser->peek_token.kind == T_ID)
          {
            Token saved_current_token = parser->current_token, saved_peek_token = parser->peek_token;
            Lexer saved_lexer = parser->lexer;
            parser_next(parser);
            parser_next(parser);
            if (parser->current_token.kind == T_CM or parser->current_token.kind == T_CL)
            {
              parser->current_token = saved_current_token;
              parser->peek_token = saved_peek_token;
              parser->lexer = saved_lexer;
              alternative->accept_stmt.parameters = parse_parameter_mode(parser);
            }
            else
            {
              parser->current_token = saved_current_token;
              parser->peek_token = saved_peek_token;
              parser->lexer = saved_lexer;
              parser_expect(parser, T_LP);
              do
                nv(&alternative->accept_stmt.index_constraints, parse_expression(parser));
              while (parser_match(parser, T_CM));
              parser_expect(parser, T_RP);
              alternative->accept_stmt.parameters = parse_parameter_mode(parser);
            }
          }
          else
          {
            parser_expect(parser, T_LP);
            do
              nv(&alternative->accept_stmt.index_constraints, parse_expression(parser));
            while (parser_match(parser, T_CM));
            parser_expect(parser, T_RP);
            alternative->accept_stmt.parameters = parse_parameter_mode(parser);
          }
        }
        else
        {
          alternative->accept_stmt.parameters = parse_parameter_mode(parser);
        }
        if (parser_match(parser, T_DO))
        {
          while (not parser_at(parser, T_END) and not parser_at(parser, T_OR) and not parser_at(parser, T_ELSE))
            nv(&alternative->accept_stmt.statements, parse_statement_or_label(parser));
          parser_expect(parser, T_END);
          if (parser_at(parser, T_ID))
            parser_next(parser);
        }
        parser_expect(parser, T_SC);
        while (not parser_at(parser, T_OR) and not parser_at(parser, T_ELSE) and not parser_at(parser, T_END))
          nv(&alternative->exception_handler.statements, parse_statement_or_label(parser));
      }
      else if (parser_match(parser, T_DEL))
      {
        alternative->k = N_DL;
        alternative->exit_stmt.condition = parse_expression(parser);
        parser_expect(parser, T_SC);
      }
      else if (parser_match(parser, T_TER))
      {
        alternative->k = N_TRM;
        parser_expect(parser, T_SC);
      }
      else
      {
        while (not parser_at(parser, T_OR) and not parser_at(parser, T_ELSE) and not parser_at(parser, T_END))
          nv(&alternative->exception_handler.statements, parse_statement_or_label(parser));
      }
      nv(&node->abort_stmt.statements, alternative);
    } while (parser_match(parser, T_OR));
  }
  if (parser_match(parser, T_ELSE))
    while (not parser_at(parser, T_END))
      nv(&node->select_stmt.else_statements, parse_statement_or_label(parser));
  parser_expect(parser, T_END);
  parser_expect(parser, T_SEL);
  parser_expect(parser, T_SC);
  return node;
}
static Syntax_Node *parse_statement_or_label(Parser *parser)
{
  Source_Location location = parser_location(parser);
  String_Slice label = N;
  while (parser_at(parser, T_LL))
  {
    parser_next(parser);
    label = parser_identifier(parser);
    parser_expect(parser, T_GG);
    slv(&parser->label_stack, label);
  }
  if (not label.string and parser_at(parser, T_ID) and parser->peek_token.kind == T_CL)
  {
    label = parser_identifier(parser);
    parser_expect(parser, T_CL);
    slv(&parser->label_stack, label);
  }
  if (parser_at(parser, T_IF))
    return parse_if(parser);
  if (parser_at(parser, T_CSE))
    return parse_case(parser);
  if (parser_at(parser, T_SEL))
    return parse_statement_list(parser);
  if (parser_at(parser, T_LOOP) or parser_at(parser, T_WHI) or parser_at(parser, T_FOR))
    return parse_loop(parser, label);
  if (parser_at(parser, T_DEC) or parser_at(parser, T_BEG))
    return parse_block(parser, label);
  if (label.string)
  {
    Syntax_Node *block = ND(BL, location);
    block->block.label = label;
    Node_Vector statements = {0};
    nv(&statements, parse_statement_or_label(parser));
    block->block.statements = statements;
    return block;
  }
  if (parser_match(parser, T_ACC))
  {
    Syntax_Node *node = ND(ACC, location);
    node->accept_stmt.name = parser_identifier(parser);
    if (parser_at(parser, T_LP))
    {
      if (parser->peek_token.kind == T_ID)
      {
        Token saved_current_token = parser->current_token, saved_peek_token = parser->peek_token;
        Lexer saved_lexer = parser->lexer;
        parser_next(parser);
        parser_next(parser);
        if (parser->current_token.kind == T_CM or parser->current_token.kind == T_CL)
        {
          parser->current_token = saved_current_token;
          parser->peek_token = saved_peek_token;
          parser->lexer = saved_lexer;
          node->accept_stmt.parameters = parse_parameter_mode(parser);
        }
        else
        {
          parser->current_token = saved_current_token;
          parser->peek_token = saved_peek_token;
          parser->lexer = saved_lexer;
          parser_expect(parser, T_LP);
          do
            nv(&node->accept_stmt.index_constraints, parse_expression(parser));
          while (parser_match(parser, T_CM));
          parser_expect(parser, T_RP);
          node->accept_stmt.parameters = parse_parameter_mode(parser);
        }
      }
      else
      {
        parser_expect(parser, T_LP);
        do
          nv(&node->accept_stmt.index_constraints, parse_expression(parser));
        while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
        node->accept_stmt.parameters = parse_parameter_mode(parser);
      }
    }
    else
    {
      node->accept_stmt.parameters = parse_parameter_mode(parser);
    }
    if (parser_match(parser, T_DO))
    {
      while (not parser_at(parser, T_END))
        nv(&node->accept_stmt.statements, parse_statement_or_label(parser));
      parser_expect(parser, T_END);
      if (parser_at(parser, T_ID))
        parser_next(parser);
    }
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_DEL))
  {
    Syntax_Node *node = ND(DL, location);
    node->exit_stmt.condition = parse_expression(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_AB))
  {
    Syntax_Node *node = ND(AB, location);
    if (not parser_at(parser, T_SC))
      node->raise_stmt.exception_choice = parse_name(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_RET))
  {
    Syntax_Node *node = ND(RT, location);
    if (not parser_at(parser, T_SC))
      node->return_stmt.value = parse_expression(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_EXIT))
  {
    Syntax_Node *node = ND(EX, location);
    if (parser_at(parser, T_ID))
      node->exit_stmt.label = parser_identifier(parser);
    if (parser_match(parser, T_WHN))
      node->exit_stmt.condition = parse_expression(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_GOTO))
  {
    Syntax_Node *node = ND(GT, location);
    node->goto_stmt.label = parser_identifier(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_RAS))
  {
    Syntax_Node *node = ND(RS, location);
    if (not parser_at(parser, T_SC))
      node->raise_stmt.exception_choice = parse_name(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_NULL))
  {
    parser_expect(parser, T_SC);
    return ND(NS, location);
  }
  if (parser_match(parser, T_PGM))
  {
    Syntax_Node *node = ND(PG, location);
    node->pragma.name = parser_identifier(parser);
    if (parser_match(parser, T_LP))
    {
      do
        nv(&node->pragma.arguments, parse_expression(parser));
      while (parser_match(parser, T_CM));
      parser_expect(parser, T_RP);
    }
    parser_expect(parser, T_SC);
    return node;
  }
  Syntax_Node *expression = parse_name(parser);
  if (parser_match(parser, T_AS))
  {
    Syntax_Node *node = ND(AS, location);
    if (expression and expression->k == N_CL)
    {
      Syntax_Node *function_name = expression->call.function_name;
      Node_Vector arguments = expression->call.arguments;
      expression->k = N_IX;
      expression->index.prefix = function_name;
      expression->index.indices = arguments;
    }
    node->assignment.target = expression;
    node->assignment.value = parse_expression(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  Syntax_Node *node = ND(CLT, location);
  if (expression->k == N_IX)
  {
    node->code_stmt.name = expression->index.prefix;
    node->code_stmt.arguments = expression->index.indices;
  }
  else if (expression->k == N_CL)
  {
    node->code_stmt.name = expression->call.function_name;
    node->code_stmt.arguments = expression->call.arguments;
  }
  else
    node->code_stmt.name = expression;
  parser_expect(parser, T_SC);
  return node;
}
static Node_Vector parse_statement(Parser *parser)
{
  Node_Vector statements = {0};
  while (not parser_at(parser, T_END) and not parser_at(parser, T_EXCP) and not parser_at(parser, T_ELSIF)
         and not parser_at(parser, T_ELSE) and not parser_at(parser, T_WHN) and not parser_at(parser, T_OR))
    nv(&statements, parse_statement_or_label(parser));
  return statements;
}
static Node_Vector parse_handle_declaration(Parser *parser)
{
  Node_Vector handlers = {0};
  while (parser_match(parser, T_WHN))
  {
    Source_Location location = parser_location(parser);
    Syntax_Node *handler = ND(HD, location);
    do
    {
      if (parser_match(parser, T_OTH))
      {
        Syntax_Node *node = ND(ID, location);
        node->string_value = STRING_LITERAL("others");
        nv(&handler->exception_handler.exception_choices, node);
      }
      else
        nv(&handler->exception_handler.exception_choices, parse_name(parser));
    } while (parser_match(parser, T_BR));
    parser_expect(parser, T_AR);
    while (not parser_at(parser, T_WHN) and not parser_at(parser, T_END))
      nv(&handler->exception_handler.statements, parse_statement_or_label(parser));
    nv(&handlers, handler);
  }
  return handlers;
}
static Syntax_Node *parse_type_definition(Parser *parser)
{
  Source_Location location = parser_location(parser);
  if (parser_match(parser, T_LP))
  {
    Syntax_Node *node = ND(TE, location);
    do
    {
      if (parser_at(parser, T_CHAR))
      {
        Syntax_Node *c = ND(CHAR, location);
        c->integer_value = parser->current_token.integer_value;
        parser_next(parser);
        nv(&node->list.items, c);
      }
      else
      {
        String_Slice nm = parser_identifier(parser);
        Syntax_Node *i = ND(ID, location);
        i->string_value = nm;
        nv(&node->list.items, i);
      }
    } while (parser_match(parser, T_CM));
    parser_expect(parser, T_RP);
    return node;
  }
  if (parser_match(parser, T_RNG))
  {
    Syntax_Node *node = ND(TI, location);
    if (parser_match(parser, T_BX))
    {
      node->range.low_bound = 0;
      node->range.high_bound = 0;
    }
    else
    {
      node->range.low_bound = parse_signed_term(parser);
      parser_expect(parser, T_DD);
      node->range.high_bound = parse_signed_term(parser);
    }
    return node;
  }
  if (parser_match(parser, T_MOD))
  {
    Syntax_Node *node = ND(TI, location);
    node->unary_node.op = T_MOD;
    node->unary_node.operand = parse_expression(parser);
    return node;
  }
  if (parser_match(parser, T_DIG))
  {
    Syntax_Node *node = ND(TF, location);
    if (parser_match(parser, T_BX))
      node->unary_node.operand = 0;
    else
      node->unary_node.operand = parse_expression(parser);
    if (parser_match(parser, T_RNG))
    {
      node->range.low_bound = parse_signed_term(parser);
      parser_expect(parser, T_DD);
      node->range.high_bound = parse_signed_term(parser);
    }
    return node;
  }
  if (parser_match(parser, T_DELTA))
  {
    Syntax_Node *node = ND(TX, location);
    if (parser_match(parser, T_BX))
    {
      node->range.low_bound = 0;
      node->range.high_bound = 0;
      node->binary_node.right = 0;
    }
    else
    {
      node->range.low_bound = parse_expression(parser);
      parser_expect(parser, T_RNG);
      node->range.high_bound = parse_signed_term(parser);
      parser_expect(parser, T_DD);
      node->binary_node.right = parse_signed_term(parser);
    }
    return node;
  }
  if (parser_match(parser, T_ARR))
  {
    parser_expect(parser, T_LP);
    Syntax_Node *node = ND(TA, location);
    do
    {
      Syntax_Node *ix = parse_range(parser);
      if (ix->k == N_ID and parser_match(parser, T_RNG))
      {
        Syntax_Node *st = ND(ST, location);
        st->subtype_decl.index_constraint = ix;
        Syntax_Node *cn = ND(CN, location);
        cn->constraint.range_spec = parse_range(parser);
        st->subtype_decl.constraint = cn;
        nv(&node->index.indices, st);
      }
      else
        nv(&node->index.indices, ix);
    } while (parser_match(parser, T_CM));
    parser_expect(parser, T_RP);
    parser_expect(parser, T_OF);
    node->index.prefix = parse_simple_expression(parser);
    return node;
  }
  if (parser_match(parser, T_REC))
  {
    Syntax_Node *node = ND(TR, location);
    uint32_t of = 0;
    Node_Vector dc = {0};
    if (parser_match(parser, T_LP))
    {
      do
      {
        String_Slice dn = parser_identifier(parser);
        parser_expect(parser, T_CL);
        Syntax_Node *dt = parse_name(parser);
        Syntax_Node *dd = 0;
        if (parser_match(parser, T_AS))
          dd = parse_expression(parser);
        Syntax_Node *dp = ND(DS, location);
        dp->parameter.name = dn;
        dp->parameter.ty = dt;
        dp->parameter.default_value = dd;
        nv(&dc, dp);
      } while (parser_match(parser, T_SC));
      parser_expect(parser, T_RP);
      if (not parser_at(parser, T_IS))
        parser_expect(parser, T_SC);
    }
    if (parser_match(parser, T_IS))
    {
      parser_expect(parser, T_REC);
    }
    while (not parser_at(parser, T_END) and not parser_at(parser, T_CSE) and not parser_at(parser, T_NULL))
    {
      Node_Vector id = {0};
      do
      {
        String_Slice nm = parser_identifier(parser);
        Syntax_Node *i = ND(ID, location);
        i->string_value = nm;
        nv(&id, i);
      } while (parser_match(parser, T_CM));
      parser_expect(parser, T_CL);
      Syntax_Node *ty = parse_simple_expression(parser);
      Syntax_Node *in = 0;
      if (parser_match(parser, T_AS))
        in = parse_expression(parser);
      parser_expect(parser, T_SC);
      for (uint32_t i = 0; i < id.count; i++)
      {
        Syntax_Node *c = ND(CM, location);
        c->component_decl.name = id.data[i]->string_value;
        c->component_decl.ty = ty;
        c->component_decl.in = in;
        c->component_decl.offset = of++;
        c->component_decl.discriminant_constraint = 0;
        c->component_decl.discriminant_spec = 0;
        if (dc.count > 0)
        {
          c->component_decl.discriminant_constraint = ND(LST, location);
          c->component_decl.discriminant_constraint->list.items = dc;
        }
        nv(&node->list.items, c);
      }
    }
    if (parser_match(parser, T_NULL))
    {
      parser_expect(parser, T_SC);
    }
    if (parser_match(parser, T_CSE))
    {
      Syntax_Node *vp = ND(VP, location);
      vp->variant_part.discriminant_spec = parse_name(parser);
      parser_expect(parser, T_IS);
      while (parser_match(parser, T_WHN))
      {
        Syntax_Node *v = ND(VR, location);
        do
        {
          Syntax_Node *e = parse_expression(parser);
          if (parser_match(parser, T_DD))
          {
            Syntax_Node *r = ND(RN, location);
            r->range.low_bound = e;
            r->range.high_bound = parse_expression(parser);
            e = r;
          }
          nv(&v->variant.choices, e);
        } while (parser_match(parser, T_BR));
        parser_expect(parser, T_AR);
        while (not parser_at(parser, T_WHN) and not parser_at(parser, T_END) and not parser_at(parser, T_NULL))
        {
          Node_Vector id = {0};
          do
          {
            String_Slice nm = parser_identifier(parser);
            Syntax_Node *i = ND(ID, location);
            i->string_value = nm;
            nv(&id, i);
          } while (parser_match(parser, T_CM));
          parser_expect(parser, T_CL);
          Syntax_Node *ty = parse_simple_expression(parser);
          Syntax_Node *in = 0;
          if (parser_match(parser, T_AS))
            in = parse_expression(parser);
          parser_expect(parser, T_SC);
          for (uint32_t i = 0; i < id.count; i++)
          {
            Syntax_Node *c = ND(CM, location);
            c->component_decl.name = id.data[i]->string_value;
            c->component_decl.ty = ty;
            c->component_decl.in = in;
            c->component_decl.offset = of++;
            c->component_decl.discriminant_constraint = 0;
            c->component_decl.discriminant_spec = 0;
            if (dc.count > 0)
            {
              c->component_decl.discriminant_constraint = ND(LST, location);
              c->component_decl.discriminant_constraint->list.items = dc;
            }
            nv(&v->variant.components, c);
          }
        }
        if (parser_match(parser, T_NULL))
          parser_expect(parser, T_SC);
        nv(&vp->variant_part.variants, v);
      }
      vp->variant_part.size = of;
      if (parser_match(parser, T_NULL))
      {
        parser_expect(parser, T_REC);
      }
      parser_expect(parser, T_END);
      parser_expect(parser, T_CSE);
      parser_expect(parser, T_SC);
      nv(&node->list.items, vp);
    }
    parser_expect(parser, T_END);
    parser_expect(parser, T_REC);
    return node;
  }
  if (parser_match(parser, T_ACCS))
  {
    Syntax_Node *node = ND(TAC, location);
    node->unary_node.operand = parse_simple_expression(parser);
    return node;
  }
  if (parser_match(parser, T_PRV))
    return ND(TP, location);
  if (parser_match(parser, T_LIM))
  {
    parser_match(parser, T_PRV);
    return ND(TP, location);
  }
  return parse_simple_expression(parser);
}
static Representation_Clause *parse_representation_clause(Parser *parser)
{
  Source_Location location = parser_location(parser);
  (void) location;
  if (parser_match(parser, T_FOR))
  {
    parse_name(parser);
    parser_expect(parser, T_USE);
    if (parser_match(parser, T_AT))
    {
      Representation_Clause *r = reference_counter_new(2, 0);
      parse_expression(parser);
      parser_expect(parser, T_SC);
      return r;
    }
    if (parser_match(parser, T_REC))
    {
      while (not parser_at(parser, T_END))
      {
        parser_identifier(parser);
        parser_expect(parser, T_AT);
        parse_expression(parser);
        parser_expect(parser, T_RNG);
        parse_range(parser);
        parser_expect(parser, T_SC);
      }
      parser_expect(parser, T_END);
      parser_expect(parser, T_REC);
      parser_expect(parser, T_SC);
      return 0;
    }
    parse_expression(parser);
    parser_expect(parser, T_SC);
    return 0;
  }
  if (parser_match(parser, T_PGM))
  {
    String_Slice nm = parser_identifier(parser);
    Representation_Clause *r = 0;
    if (string_equal_ignore_case(nm, STRING_LITERAL("SUPPRESS")))
    {
      if (parser_at(parser, T_LP))
      {
        parser_expect(parser, T_LP);
        String_Slice ck = parser_identifier(parser);
        uint16_t cm = string_equal_ignore_case(ck, STRING_LITERAL("OVERFLOW_CHECK"))       ? CHK_OVF
                      : string_equal_ignore_case(ck, STRING_LITERAL("RANGE_CHECK"))        ? CHK_RNG
                      : string_equal_ignore_case(ck, STRING_LITERAL("INDEX_CHECK"))        ? CHK_IDX
                      : string_equal_ignore_case(ck, STRING_LITERAL("DISCRIMINANT_CHECK")) ? CHK_DSC
                      : string_equal_ignore_case(ck, STRING_LITERAL("LENGTH_CHECK"))       ? CHK_LEN
                      : string_equal_ignore_case(ck, STRING_LITERAL("DIVISION_CHECK"))     ? CHK_DIV
                      : string_equal_ignore_case(ck, STRING_LITERAL("ELABORATION_CHECK"))  ? CHK_ELB
                      : string_equal_ignore_case(ck, STRING_LITERAL("ACCESS_CHECK"))       ? CHK_ACC
                      : string_equal_ignore_case(ck, STRING_LITERAL("STORAGE_CHECK"))      ? CHK_STG
                                                                                           : 0;
        if (cm)
        {
          r = reference_counter_new(4, 0);
          r->ad.name = parser_match(parser, T_CM) ? parser_identifier(parser) : N;
          while (parser_match(parser, T_CM))
            parser_identifier(parser);
          r->ad.address = cm;
        }
        else
        {
          while (parser_match(parser, T_CM))
            parser_identifier(parser);
        }
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
      return r;
    }
    else if (string_equal_ignore_case(nm, STRING_LITERAL("PACK")))
    {
      if (parser_at(parser, T_LP))
      {
        parser_expect(parser, T_LP);
        Syntax_Node *tn = parse_name(parser);
        while (parser_match(parser, T_CM))
          parse_name(parser);
        parser_expect(parser, T_RP);
        r = reference_counter_new(5, 0);
        r->er.name = tn and tn->k == N_ID ? tn->string_value : N;
      }
      parser_expect(parser, T_SC);
      return r;
    }
    else if (string_equal_ignore_case(nm, STRING_LITERAL("INLINE")))
    {
      if (parser_at(parser, T_LP))
      {
        parser_expect(parser, T_LP);
        r = reference_counter_new(6, 0);
        r->er.name = parser_identifier(parser);
        while (parser_match(parser, T_CM))
          parser_identifier(parser);
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
      return r;
    }
    else if (string_equal_ignore_case(nm, STRING_LITERAL("CONTROLLED")))
    {
      if (parser_at(parser, T_LP))
      {
        parser_expect(parser, T_LP);
        r = reference_counter_new(7, 0);
        r->er.name = parser_identifier(parser);
        while (parser_match(parser, T_CM))
          parser_identifier(parser);
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
      return r;
    }
    else if (
        string_equal_ignore_case(nm, STRING_LITERAL("INTERFACE"))
        or string_equal_ignore_case(nm, STRING_LITERAL("IMPORT")))
    {
      if (parser_at(parser, T_LP))
      {
        parser_expect(parser, T_LP);
        r = reference_counter_new(8, 0);
        r->im.language = parser_identifier(parser);
        if (parser_match(parser, T_CM))
        {
          r->im.name = parser_identifier(parser);
          if (parser_match(parser, T_CM))
          {
            if (parser_at(parser, T_STR))
            {
              r->im.external_name = parser->current_token.literal;
              parser_next(parser);
            }
            else
              r->im.external_name = parser_identifier(parser);
          }
          else
            r->im.external_name = r->im.name;
        }
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
      return r;
    }
    else if (
        string_equal_ignore_case(nm, STRING_LITERAL("OPTIMIZE"))
        or string_equal_ignore_case(nm, STRING_LITERAL("PRIORITY"))
        or string_equal_ignore_case(nm, STRING_LITERAL("STORAGE_SIZE"))
        or string_equal_ignore_case(nm, STRING_LITERAL("SHARED"))
        or string_equal_ignore_case(nm, STRING_LITERAL("LIST"))
        or string_equal_ignore_case(nm, STRING_LITERAL("PAGE")))
    {
      if (parser_match(parser, T_LP))
      {
        do
          parse_expression(parser);
        while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
      return 0;
    }
    else
    {
      if (string_equal_ignore_case(nm, STRING_LITERAL("ELABORATE"))
          or string_equal_ignore_case(nm, STRING_LITERAL("ELABORATE_ALL")))
      {
        parser_expect(parser, T_LP);
        do
          parser_identifier(parser);
        while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
      }
      else if (parser_match(parser, T_LP))
      {
        do
          parser_identifier(parser);
        while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
    }
  }
  return 0;
}
static Syntax_Node *parse_declaration(Parser *parser)
{
  Source_Location location = parser_location(parser);
  if (parser_at(parser, T_GEN))
    return parse_generic_formal(parser);
  if (parser_match(parser, T_TYP))
  {
    String_Slice nm = parser_identifier(parser);
    Syntax_Node *node = ND(TD, location);
    node->type_decl.name = nm;
    if (parser_match(parser, T_LP))
    {
      Syntax_Node *ds = ND(LST, location);
      do
      {
        Node_Vector dn = {0};
        do
        {
          String_Slice dnm = parser_identifier(parser);
          Syntax_Node *di = ND(ID, location);
          di->string_value = dnm;
          nv(&dn, di);
        } while (parser_match(parser, T_CM));
        parser_expect(parser, T_CL);
        Syntax_Node *dt = parse_name(parser);
        Syntax_Node *dd = 0;
        if (parser_match(parser, T_AS))
          dd = parse_expression(parser);
        for (uint32_t i = 0; i < dn.count; i++)
        {
          Syntax_Node *dp = ND(DS, location);
          dp->parameter.name = dn.data[i]->string_value;
          dp->parameter.ty = dt;
          dp->parameter.default_value = dd;
          nv(&ds->list.items, dp);
        }
      } while (parser_match(parser, T_SC));
      parser_expect(parser, T_RP);
      node->type_decl.discriminants = ds->list.items;
    }
    if (parser_match(parser, T_IS))
    {
      node->type_decl.is_new = parser_match(parser, T_NEW);
      node->type_decl.is_derived = node->type_decl.is_new;
      if (node->type_decl.is_derived)
      {
        node->type_decl.parent_type = parse_name(parser);
        node->type_decl.definition = node->type_decl.parent_type;
        if (parser_match(parser, T_DIG))
        {
          parse_expression(parser);
          if (parser_match(parser, T_RNG))
          {
            parse_signed_term(parser);
            parser_expect(parser, T_DD);
            parse_signed_term(parser);
          }
        }
        else if (parser_match(parser, T_DELTA))
        {
          parse_expression(parser);
          parser_expect(parser, T_RNG);
          parse_signed_term(parser);
          parser_expect(parser, T_DD);
          parse_signed_term(parser);
        }
        else if (parser_match(parser, T_RNG))
        {
          Syntax_Node *rn = ND(RN, location);
          rn->range.low_bound = parse_signed_term(parser);
          parser_expect(parser, T_DD);
          rn->range.high_bound = parse_signed_term(parser);
          node->type_decl.definition = rn;
        }
      }
      else
        node->type_decl.definition = parse_type_definition(parser);
    }
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_SUB))
  {
    String_Slice nm = parser_identifier(parser);
    parser_expect(parser, T_IS);
    Syntax_Node *node = ND(SD, location);
    node->subtype_decl.name = nm;
    node->subtype_decl.index_constraint = parse_simple_expression(parser);
    if (node->subtype_decl.index_constraint->k == N_ST)
      node->subtype_decl.range_constraint = node->subtype_decl.index_constraint->subtype_decl.constraint->constraint.range_spec;
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_at(parser, T_PROC))
  {
    Syntax_Node *sp = parse_procedure_specification(parser);
    if (parser_match(parser, T_REN))
    {
      parse_expression(parser);
      parser_expect(parser, T_SC);
      Syntax_Node *node = ND(PD, location);
      node->body.subprogram_spec = sp;
      return node;
    }
    if (parser_match(parser, T_IS))
    {
      if (parser_match(parser, T_SEP))
      {
        parser_expect(parser, T_SC);
        Syntax_Node *node = ND(PD, location);
        node->body.subprogram_spec = sp;
        return node;
      }
      if (parser_match(parser, T_NEW))
      {
        String_Slice gn = parser_identifier(parser);
        Node_Vector ap = {0};
        if (parser_match(parser, T_LP))
        {
          do
          {
            Syntax_Node *e = parse_expression(parser);
            if (e->k == N_ID and parser_at(parser, T_AR))
            {
              parser_next(parser);
              Syntax_Node *a = ND(ASC, location);
              nv(&a->association.choices, e);
              a->association.value = parse_expression(parser);
              nv(&ap, a);
            }
            else
            {
              nv(&ap, e);
            }
          } while (parser_match(parser, T_CM));
          parser_expect(parser, T_RP);
        }
        parser_expect(parser, T_SC);
        Syntax_Node *node = ND(GINST, location);
        node->generic_inst.name = sp->subprogram.name;
        node->generic_inst.generic_name = gn;
        node->generic_inst.actual_parameters = ap;
        return node;
      }
      Syntax_Node *node = ND(PB, location);
      node->body.subprogram_spec = sp;
      node->body.declarations = parse_declarative_part(parser);
      parser_expect(parser, T_BEG);
      node->body.statements = parse_statement(parser);
      if (parser_match(parser, T_EXCP))
        node->body.handlers = parse_handle_declaration(parser);
      parser_expect(parser, T_END);
      if (parser_at(parser, T_ID) or parser_at(parser, T_STR))
        parser_next(parser);
      parser_expect(parser, T_SC);
      return node;
    }
    parser_expect(parser, T_SC);
    Syntax_Node *node = ND(PD, location);
    node->body.subprogram_spec = sp;
    return node;
  }
  if (parser_match(parser, T_FUN))
  {
    String_Slice nm;
    if (parser_at(parser, T_STR))
    {
      nm = parser->current_token.literal;
      parser_next(parser);
    }
    else
      nm = parser_identifier(parser);
    if (parser_match(parser, T_IS) and parser_match(parser, T_NEW))
    {
      String_Slice gn = parser_identifier(parser);
      Node_Vector ap = {0};
      if (parser_match(parser, T_LP))
      {
        do
        {
          Syntax_Node *e = parse_expression(parser);
          if (e->k == N_ID and parser_at(parser, T_AR))
          {
            parser_next(parser);
            Syntax_Node *a = ND(ASC, location);
            nv(&a->association.choices, e);
            a->association.value = parse_expression(parser);
            nv(&ap, a);
          }
          else
          {
            nv(&ap, e);
          }
        } while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
      Syntax_Node *node = ND(GINST, location);
      node->generic_inst.name = nm;
      node->generic_inst.generic_name = gn;
      node->generic_inst.actual_parameters = ap;
      return node;
    }
    Syntax_Node *sp = ND(FS, location);
    sp->subprogram.name = nm;
    sp->subprogram.parameters = parse_parameter_mode(parser);
    parser_expect(parser, T_RET);
    sp->subprogram.return_type = parse_name(parser);
    if (parser_match(parser, T_REN))
    {
      parse_expression(parser);
      parser_expect(parser, T_SC);
      Syntax_Node *node = ND(FD, location);
      node->body.subprogram_spec = sp;
      return node;
    }
    if (parser_match(parser, T_IS))
    {
      if (parser_match(parser, T_SEP))
      {
        parser_expect(parser, T_SC);
        Syntax_Node *node = ND(FD, location);
        node->body.subprogram_spec = sp;
        return node;
      }
      Syntax_Node *node = ND(FB, location);
      node->body.subprogram_spec = sp;
      node->body.declarations = parse_declarative_part(parser);
      parser_expect(parser, T_BEG);
      node->body.statements = parse_statement(parser);
      if (parser_match(parser, T_EXCP))
        node->body.handlers = parse_handle_declaration(parser);
      parser_expect(parser, T_END);
      if (parser_at(parser, T_ID) or parser_at(parser, T_STR))
        parser_next(parser);
      parser_expect(parser, T_SC);
      return node;
    }
    parser_expect(parser, T_SC);
    Syntax_Node *node = ND(FD, location);
    node->body.subprogram_spec = sp;
    return node;
  }
  if (parser_match(parser, T_PKG))
  {
    if (parser_match(parser, T_BOD))
    {
      String_Slice nm = parser_identifier(parser);
      parser_expect(parser, T_IS);
      if (parser_match(parser, T_SEP))
      {
        parser_expect(parser, T_SC);
        Syntax_Node *node = ND(PKB, location);
        node->package_body.name = nm;
        return node;
      }
      Syntax_Node *node = ND(PKB, location);
      node->package_body.name = nm;
      node->package_body.declarations = parse_declarative_part(parser);
      if (parser_match(parser, T_BEG))
      {
        node->package_body.statements = parse_statement(parser);
        if (parser_match(parser, T_EXCP))
          node->package_body.handlers = parse_handle_declaration(parser);
      }
      parser_expect(parser, T_END);
      if (parser_at(parser, T_ID))
        parser_next(parser);
      parser_expect(parser, T_SC);
      return node;
    }
    String_Slice nm = parser_identifier(parser);
    if (parser_match(parser, T_REN))
    {
      Syntax_Node *rn = parse_expression(parser);
      parser_expect(parser, T_SC);
      Syntax_Node *node = ND(RE, location);
      node->renaming.name = nm;
      node->renaming.renamed_entity = rn;
      return node;
    }
    parser_expect(parser, T_IS);
    if (parser_match(parser, T_NEW))
    {
      String_Slice gn = parser_identifier(parser);
      Node_Vector ap = {0};
      if (parser_match(parser, T_LP))
      {
        do
        {
          Syntax_Node *e = parse_expression(parser);
          if (e->k == N_ID and parser_at(parser, T_AR))
          {
            parser_next(parser);
            Syntax_Node *a = ND(ASC, location);
            nv(&a->association.choices, e);
            a->association.value = parse_expression(parser);
            nv(&ap, a);
          }
          else
          {
            nv(&ap, e);
          }
        } while (parser_match(parser, T_CM));
        parser_expect(parser, T_RP);
      }
      parser_expect(parser, T_SC);
      Syntax_Node *node = ND(GINST, location);
      node->generic_inst.name = nm;
      node->generic_inst.generic_name = gn;
      node->generic_inst.actual_parameters = ap;
      return node;
    }
    Syntax_Node *node = ND(PKS, location);
    node->package_spec.name = nm;
    node->package_spec.declarations = parse_declarative_part(parser);
    if (parser_match(parser, T_PRV))
      node->package_spec.private_declarations = parse_declarative_part(parser);
    parser_expect(parser, T_END);
    if (parser_at(parser, T_ID))
      parser_next(parser);
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_TSK))
  {
    if (parser_match(parser, T_BOD))
    {
      String_Slice nm = parser_identifier(parser);
      parser_expect(parser, T_IS);
      if (parser_match(parser, T_SEP))
      {
        parser_expect(parser, T_SC);
        Syntax_Node *node = ND(TKB, location);
        node->task_body.name = nm;
        return node;
      }
      Syntax_Node *node = ND(TKB, location);
      node->task_body.name = nm;
      node->task_body.declarations = parse_declarative_part(parser);
      parser_expect(parser, T_BEG);
      node->task_body.statements = parse_statement(parser);
      if (parser_match(parser, T_EXCP))
        node->task_body.handlers = parse_handle_declaration(parser);
      parser_expect(parser, T_END);
      if (parser_at(parser, T_ID))
        parser_next(parser);
      parser_expect(parser, T_SC);
      return node;
    }
    bool it = parser_match(parser, T_TYP);
    String_Slice nm = parser_identifier(parser);
    Syntax_Node *node = ND(TKS, location);
    node->task_spec.name = nm;
    node->task_spec.is_type = it;
    if (parser_match(parser, T_IS))
    {
      while (not parser_at(parser, T_END))
      {
        if (parser_match(parser, T_ENT))
        {
          Syntax_Node *e = ND(ENT, location);
          e->entry_decl.name = parser_identifier(parser);
          if (parser_at(parser, T_LP))
          {
            if (parser->peek_token.kind == T_ID or parser->peek_token.kind == T_INT or parser->peek_token.kind == T_CHAR)
            {
              Token scr = parser->current_token, spk = parser->peek_token;
              Lexer slx = parser->lexer;
              parser_next(parser);
              parser_next(parser);
              if (parser->current_token.kind == T_CM or parser->current_token.kind == T_CL)
              {
                parser->current_token = scr;
                parser->peek_token = spk;
                parser->lexer = slx;
                e->entry_decl.parameters = parse_parameter_mode(parser);
              }
              else
              {
                parser->current_token = scr;
                parser->peek_token = spk;
                parser->lexer = slx;
                parser_expect(parser, T_LP);
                Syntax_Node *ix = parse_range(parser);
                if (ix->k != N_RN and parser_match(parser, T_RNG))
                {
                  Syntax_Node *rng = parse_range(parser);
                  Syntax_Node *si = ND(ST, location);
                  Syntax_Node *cn = ND(CN, location);
                  cn->constraint.range_spec = rng;
                  si->subtype_decl.index_constraint = ix;
                  si->subtype_decl.constraint = cn;
                  nv(&e->entry_decl.index_constraints, si);
                }
                else
                {
                  nv(&e->entry_decl.index_constraints, ix);
                }
                parser_expect(parser, T_RP);
                e->entry_decl.parameters = parse_parameter_mode(parser);
              }
            }
            else
            {
              parser_expect(parser, T_LP);
              Syntax_Node *ix = parse_range(parser);
              if (ix->k != N_RN and parser_match(parser, T_RNG))
              {
                Syntax_Node *rng = parse_range(parser);
                Syntax_Node *si = ND(ST, location);
                Syntax_Node *cn = ND(CN, location);
                cn->constraint.range_spec = rng;
                si->subtype_decl.index_constraint = ix;
                si->subtype_decl.constraint = cn;
                nv(&e->entry_decl.index_constraints, si);
              }
              else
              {
                nv(&e->entry_decl.index_constraints, ix);
              }
              parser_expect(parser, T_RP);
              e->entry_decl.parameters = parse_parameter_mode(parser);
            }
          }
          else
          {
            e->entry_decl.parameters = parse_parameter_mode(parser);
          }
          parser_expect(parser, T_SC);
          nv(&node->task_spec.entries, e);
        }
        else if (parser_match(parser, T_PGM))
        {
          parser_identifier(parser);
          if (parser_match(parser, T_LP))
          {
            do
              parse_expression(parser);
            while (parser_match(parser, T_CM));
            parser_expect(parser, T_RP);
          }
          parser_expect(parser, T_SC);
        }
      }
      parser_expect(parser, T_END);
      if (parser_at(parser, T_ID))
        parser_next(parser);
    }
    parser_expect(parser, T_SC);
    return node;
  }
  if (parser_match(parser, T_USE))
  {
    Node_Vector nms = {0};
    do
      nv(&nms, parse_name(parser));
    while (parser_match(parser, T_CM));
    parser_expect(parser, T_SC);
    if (nms.count == 1)
    {
      Syntax_Node *node = ND(US, location);
      node->use_clause.nm = nms.data[0];
      return node;
    }
    Syntax_Node *lst = ND(LST, location);
    for (uint32_t i = 0; i < nms.count; i++)
    {
      Syntax_Node *u = ND(US, location);
      u->use_clause.nm = nms.data[i];
      nv(&lst->list.items, u);
    }
    return lst;
  }
  if (parser_match(parser, T_PGM))
  {
    Syntax_Node *node = ND(PG, location);
    node->pragma.name = parser_identifier(parser);
    if (parser_match(parser, T_LP))
    {
      do
        nv(&node->pragma.arguments, parse_expression(parser));
      while (parser_match(parser, T_CM));
      parser_expect(parser, T_RP);
    }
    parser_expect(parser, T_SC);
    return node;
  }
  {
    Node_Vector id = {0};
    do
    {
      String_Slice nm = parser_identifier(parser);
      Syntax_Node *i = ND(ID, location);
      i->string_value = nm;
      nv(&id, i);
    } while (parser_match(parser, T_CM));
    parser_expect(parser, T_CL);
    bool co = parser_match(parser, T_CONST);
    if (parser_match(parser, T_EXCP))
    {
      Syntax_Node *node = ND(ED, location);
      node->exception_decl.identifiers = id;
      if (parser_match(parser, T_REN))
        node->exception_decl.renamed_entity = parse_expression(parser);
      parser_expect(parser, T_SC);
      return node;
    }
    Syntax_Node *ty = 0;
    if (not parser_at(parser, T_AS))
    {
      if (parser_at(parser, T_ARR) or parser_at(parser, T_ACCS))
        ty = parse_type_definition(parser);
      else
        ty = parse_simple_expression(parser);
    }
    Syntax_Node *in = 0;
    if (parser_match(parser, T_REN))
      in = parse_expression(parser);
    else if (parser_match(parser, T_AS))
      in = parse_expression(parser);
    parser_expect(parser, T_SC);
    Syntax_Node *node = ND(OD, location);
    node->object_decl.identifiers = id;
    node->object_decl.ty = ty;
    node->object_decl.in = in;
    node->object_decl.is_constant = co;
    return node;
  }
}
static Node_Vector parse_declarative_part(Parser *parser)
{
  Node_Vector declarations = {0};
  while (not parser_at(parser, T_BEG) and not parser_at(parser, T_END) and not parser_at(parser, T_PRV)
         and not parser_at(parser, T_EOF) and not parser_at(parser, T_ENT))
  {
    if (parser_at(parser, T_FOR))
    {
      Representation_Clause *r = parse_representation_clause(parser);
      if (r)
      {
        Syntax_Node *n = ND(RRC, parser_location(parser));
        memcpy(&n->aggregate.items.data, &r, sizeof(Representation_Clause *));
        nv(&declarations, n);
      }
      continue;
    }
    if (parser_at(parser, T_PGM))
    {
      Representation_Clause *r = parse_representation_clause(parser);
      if (r)
      {
        Syntax_Node *n = ND(RRC, parser_location(parser));
        memcpy(&n->aggregate.items.data, &r, sizeof(Representation_Clause *));
        nv(&declarations, n);
      }
      continue;
    }
    nv(&declarations, parse_declaration(parser));
  }
  return declarations;
}
static Syntax_Node *parse_context(Parser *parser)
{
  Source_Location lc = parser_location(parser);
  Syntax_Node *cx = ND(CX, lc);
  while (parser_at(parser, T_WITH) or parser_at(parser, T_USE) or parser_at(parser, T_PGM))
  {
    if (parser_match(parser, T_WITH))
    {
      do
      {
        Syntax_Node *w = ND(WI, lc);
        w->with_clause.name = parser_identifier(parser);
        nv(&cx->context.with_clauses, w);
      } while (parser_match(parser, T_CM));
      parser_expect(parser, T_SC);
    }
    else if (parser_match(parser, T_USE))
    {
      do
      {
        Syntax_Node *u = ND(US, lc);
        u->use_clause.nm = parse_name(parser);
        nv(&cx->context.use_clauses, u);
      } while (parser_match(parser, T_CM));
      parser_expect(parser, T_SC);
    }
    else
    {
      Syntax_Node *pg = parse_declaration(parser);
      if (pg)
        nv(&cx->context.use_clauses, pg);
    }
  }
  return cx;
}
static Syntax_Node *parse_compilation_unit(Parser *parser)
{
  Source_Location location = parser_location(parser);
  Syntax_Node *node = ND(CU, location);
  node->compilation_unit.context = parse_context(parser);
  while (parser_at(parser, T_WITH) or parser_at(parser, T_USE) or parser_at(parser, T_PROC) or parser_at(parser, T_FUN)
         or parser_at(parser, T_PKG) or parser_at(parser, T_GEN) or parser_at(parser, T_PGM)
         or parser_at(parser, T_SEP))
  {
    if (parser_at(parser, T_WITH) or parser_at(parser, T_USE) or parser_at(parser, T_PGM))
    {
      Syntax_Node *cx = parse_context(parser);
      for (uint32_t i = 0; i < cx->context.with_clauses.count; i++)
        nv(&node->compilation_unit.context->context.with_clauses, cx->context.with_clauses.data[i]);
      for (uint32_t i = 0; i < cx->context.use_clauses.count; i++)
        nv(&node->compilation_unit.context->context.use_clauses, cx->context.use_clauses.data[i]);
    }
    else if (parser_at(parser, T_SEP))
    {
      parser_expect(parser, T_SEP);
      parser_expect(parser, T_LP);
      Syntax_Node *pnm_ = parse_name(parser);
      parser_expect(parser, T_RP);
      String_Slice ppkg = pnm_->k == N_ID ? pnm_->string_value : pnm_->k == N_SEL ? pnm_->selected_component.prefix->string_value : N;
      SEPARATE_PACKAGE = ppkg.string ? string_duplicate(ppkg) : N;
      if (ppkg.string)
      {
        FILE *pf = 0;
        char fn[512];
        static const char *ex[] = {".ada", ".adb", ".ads"};
        for (int i = 0; not pf and i < include_path_count; i++)
          for (int e = 0; not pf and e < 3; e++)
          {
            snprintf(
                fn,
                512,
                "%s%s%.*s%s",
                include_paths[i],
                include_paths[i][0] and include_paths[i][strlen(include_paths[i]) - 1] != '/' ? "/"
                                                                                              : "",
                (int) ppkg.length,
                ppkg.string,
                ex[e]);
            for (char *c = fn + strlen(include_paths[i]); *c and *c != '.'; c++)
              *c = tolower(*c);
            pf = fopen(fn, "r");
          }
        if (pf)
        {
          fseek(pf, 0, 2);
          long sz = ftell(pf);
          fseek(pf, 0, 0);
          char *psrc = malloc(sz + 1);
          fread(psrc, 1, sz, pf);
          psrc[sz] = 0;
          fclose(pf);
          Parser pp = {lexer_new(psrc, sz, fn), {0}, {0}, 0, {0}};
          parser_next(&pp);
          parser_next(&pp);
          Syntax_Node *pcu_ = parse_compilation_unit(&pp);
          if (pcu_ and pcu_->compilation_unit.context)
          {
            for (uint32_t i = 0; i < pcu_->compilation_unit.context->context.with_clauses.count; i++)
              nv(&node->compilation_unit.context->context.with_clauses, pcu_->compilation_unit.context->context.with_clauses.data[i]);
            for (uint32_t i = 0; i < pcu_->compilation_unit.context->context.use_clauses.count; i++)
              nv(&node->compilation_unit.context->context.use_clauses, pcu_->compilation_unit.context->context.use_clauses.data[i]);
          }
        }
      }
      nv(&node->compilation_unit.units, parse_declaration(parser));
    }
    else
      nv(&node->compilation_unit.units, parse_declaration(parser));
  }
  return node;
}
static Parser parser_new(const char *source, size_t size, const char *filename)
{
  Parser parser = {lexer_new(source, size, filename), {0}, {0}, 0, {0}};
  parser_next(&parser);
  parser_next(&parser);
  return parser;
}
typedef enum
{
  TY_V = 0,
  TYPE_INTEGER,
  TYPE_BOOLEAN,
  TYPE_CHARACTER,
  TYPE_FLOAT,
  TYPE_ENUMERATION,
  TYPE_ARRAY,
  TYPE_RECORD,
  TYPE_ACCESS,
  TY_T,
  TYPE_STRING,
  TY_P,
  TYPE_UNSIGNED_INTEGER,
  TYPE_UNIVERSAL_FLOAT,
  TYPE_DERIVED,
  TY_PT,
  TYPE_FAT_POINTER,
  TYPE_FIXED_POINT
} Type_Kind;
struct Type_Info
{
  Type_Kind k;
  String_Slice name;
  Type_Info *base_type, *element_type, *parent_type;
  Type_Info *index_type;
  int64_t low_bound, high_bound;
  Node_Vector components, discriminants;
  uint32_t size, alignment;
  Symbol_Vector enum_values;
  Representation_Clause_Vector rc;
  uint64_t address;
  bool is_packed;
  Node_Vector operations;
  int64_t small_value, large_value;
  uint16_t suppressed_checks;
  bool is_controlled;
  uint8_t frozen;
  Syntax_Node *frozen_node;
};
struct Symbol
{
  String_Slice name;
  uint8_t k;
  Type_Info *type_info;
  Syntax_Node *definition;
  Symbol *next, *previous;
  int scope;
  int storage_size;
  int64_t value;
  uint32_t offset;
  Node_Vector overloads;
  Symbol_Vector use_clauses;
  int elaboration_level;
  Generic_Template *generic_template;
  Symbol *parent;
  int level;
  bool is_inline;
  bool is_shared;
  bool is_external;
  String_Slice external_name;
  String_Slice external_language;
  String_Slice mangled_name;
  uint8_t frozen;
  Syntax_Node *frozen_node;
  uint8_t visibility;
  Symbol *homonym;
  uint32_t uid;
};
typedef struct
{
  Symbol *sy[4096];
  int sc;
  int ss;
  Syntax_Node *ds;
  Syntax_Node *pk;
  Symbol_Vector uv;
  int eo;
  Library_Unit_Vector lu;
  Generic_Template_Vector gt;
  jmp_buf *eb[16];
  int ed;
  String_Slice ce[16];
  File_Vector io;
  int fn;
  String_List_Vector lb;
  int lv;
  Node_Vector ib;
  Symbol *sst[256];
  int ssd;
  Symbol_Vector dps[256];
  int dpn;
  Symbol_Vector ex;
  uint64_t uv_vis[64];
  String_List_Vector eh;
  String_List_Vector ap;
  uint32_t uid_ctr;
  Symbol *ps[256];  // Procedure stack: tracks enclosing procedures/functions for parent pointer assignment
  int pn;           // Procedure nesting count
} Symbol_Manager;
static uint32_t symbol_hash(String_Slice s)
{
  return string_hash(s) & 4095;
}
static Symbol *symbol_new(String_Slice nm, uint8_t k, Type_Info *ty, Syntax_Node *df)
{
  Symbol *s = arena_allocate(sizeof(Symbol));
  s->name = string_duplicate(nm);
  s->k = k;
  s->type_info = ty;
  s->definition = df;
  s->elaboration_level = -1;
  s->level = -1;
  return s;
}
static Symbol *symbol_add_overload(Symbol_Manager *symbol_manager, Symbol *s)
{
  uint32_t h = symbol_hash(s->name);
  s->homonym = symbol_manager->sy[h];
  s->next = symbol_manager->sy[h];
  s->scope = symbol_manager->sc;
  s->storage_size = symbol_manager->ss;
  s->elaboration_level = symbol_manager->eo++;
  s->level = symbol_manager->lv;
  s->visibility = 1;
  uint64_t u = string_hash(s->name);
  if (s->parent)
  {
    u = u * 31 + string_hash(s->parent->name);
    if (s->level > 0)
    {
      u = u * 31 + s->scope;
      u = u * 31 + s->elaboration_level;
    }
  }
  s->uid = (uint32_t) (u & 0xFFFFFFFF);
  symbol_manager->sy[h] = s;
  if (symbol_manager->ssd < 256)
    symbol_manager->sst[symbol_manager->ssd++] = s;
  return s;
}
static Symbol *symbol_find(Symbol_Manager *symbol_manager, String_Slice nm)
{
  Symbol *imm = 0, *pot = 0;
  uint32_t h = symbol_hash(nm);
  for (Symbol *s = symbol_manager->sy[h]; s; s = s->next)
    if (string_equal_ignore_case(s->name, nm))
    {
      if (s->visibility & 1 and (not imm or s->scope > imm->scope))
        imm = s;
      if (s->visibility & 2 and not pot)
        pot = s;
    }
  if (imm)
    return imm;
  if (pot)
    return pot;
  for (Symbol *s = symbol_manager->sy[h]; s; s = s->next)
    if (string_equal_ignore_case(s->name, nm) and (not imm or s->scope > imm->scope))
      imm = s;
  return imm;
}
static void symbol_find_use(Symbol_Manager *symbol_manager, Symbol *s, String_Slice nm)
{
  uint32_t h = symbol_hash(nm) & 63, b = 1ULL << (symbol_hash(nm) & 63);
  if (symbol_manager->uv_vis[h] & b)
    return;
  symbol_manager->uv_vis[h] |= b;
  for (Symbol *parser = s; parser; parser = parser->next)
    if (string_equal_ignore_case(parser->name, nm) and parser->k == 6 and parser->definition and parser->definition->k == N_PKS)
    {
      Syntax_Node *pk = parser->definition;
      for (uint32_t i = 0; i < pk->package_spec.declarations.count; i++)
      {
        Syntax_Node *d = pk->package_spec.declarations.data[i];
        if (d->symbol)
        {
          sv(&s->use_clauses, d->symbol);
          d->symbol->visibility |= 2;
        }
        else if (d->k == N_ED)
        {
          for (uint32_t j = 0; j < d->exception_decl.identifiers.count; j++)
          {
            Syntax_Node *e = d->exception_decl.identifiers.data[j];
            if (e->symbol)
            {
              sv(&s->use_clauses, e->symbol);
              e->symbol->visibility |= 2;
              sv(&symbol_manager->ex, e->symbol);
            }
          }
        }
        else if (d->k == N_OD)
        {
          for (uint32_t j = 0; j < d->object_decl.identifiers.count; j++)
          {
            Syntax_Node *oid = d->object_decl.identifiers.data[j];
            if (oid->symbol)
            {
              sv(&s->use_clauses, oid->symbol);
              oid->symbol->visibility |= 2;
            }
          }
        }
      }
      if (symbol_manager->dpn < 256)
      {
        bool f = 0;
        for (int i = 0; i < symbol_manager->dpn; i++)
          if (symbol_manager->dps[i].count and string_equal_ignore_case(symbol_manager->dps[i].data[0]->name, parser->name))
          {
            f = 1;
            break;
          }
        if (not f)
        {
          sv(&symbol_manager->dps[symbol_manager->dpn], parser);
          symbol_manager->dpn++;
        }
      }
    }
  symbol_manager->uv_vis[h] &= ~b;
}
static Generic_Template *generic_find(Symbol_Manager *symbol_manager, String_Slice nm)
{
  for (uint32_t i = 0; i < symbol_manager->gt.count; i++)
  {
    Generic_Template *g = symbol_manager->gt.data[i];
    if (string_equal_ignore_case(g->name, nm))
      return g;
  }
  return 0;
}
static int type_scope(Type_Info *, Type_Info *, Type_Info *);
static Symbol *symbol_find_with_arity(Symbol_Manager *symbol_manager, String_Slice nm, int na, Type_Info *tx)
{
  Symbol_Vector cv = {0};
  int msc = -1;
  for (Symbol *s = symbol_manager->sy[symbol_hash(nm)]; s; s = s->next)
  {
    // Include symbols that are: visible (visibility & 3), external (is_external), or nested at current level or above
    if (string_equal_ignore_case(s->name, nm) and ((s->visibility & 3) or s->is_external or (s->level > 0 and s->level <= symbol_manager->lv)))
    {
      if (s->scope > msc)
      {
        cv.count = 0;
        msc = s->scope;
      }
      if (s->scope == msc)
        sv(&cv, s);
    }
  }
  if (not cv.count)
    return 0;
  if (cv.count == 1)
    return cv.data[0];
  Symbol *br = 0;
  int bs = -1;
  for (uint32_t i = 0; i < cv.count; i++)
  {
    Symbol *c = cv.data[i];
    int sc = 0;
    if ((c->k == 4 or c->k == 5) and na >= 0)
    {
      if (c->overloads.count > 0)
      {
        for (uint32_t j = 0; j < c->overloads.count; j++)
        {
          Syntax_Node *b = c->overloads.data[j];
          if (b->k == N_PB or b->k == N_FB or b->k == N_PD or b->k == N_FD)
          {
            int np = b->body.subprogram_spec->subprogram.parameters.count;
            if (np == na)
            {
              sc += 1000;
              if (tx and c->type_info and c->type_info->element_type)
              {
                int ts = type_scope(c->type_info->element_type, tx, 0);
                sc += ts;
              }
              for (uint32_t k = 0; k < b->body.subprogram_spec->subprogram.parameters.count and k < (uint32_t) na; k++)
              {
                Syntax_Node *parser = b->body.subprogram_spec->subprogram.parameters.data[k];
                if (parser->symbol and parser->symbol->type_info and tx)
                {
                  int ps = type_scope(parser->symbol->type_info, tx, 0);
                  sc += ps;
                }
              }
              if (sc > bs)
              {
                bs = sc;
                br = c;
              }
            }
          }
        }
      }
      else if (c->k == 1)
      {
        if (na == 1)
        {
          sc = 500;
          if (tx)
          {
            int ts = type_scope(c->type_info, tx, 0);
            sc += ts;
          }
          if (sc > bs)
          {
            bs = sc;
            br = c;
          }
        }
      }
    }
    else if (c->k == 1 and na < 0)
    {
      sc = 100;
      if (sc > bs)
      {
        bs = sc;
        br = c;
      }
    }
  }
  return br ?: cv.data[0];
}
static Type_Info *type_new(Type_Kind k, String_Slice nm)
{
  Type_Info *t = arena_allocate(sizeof(Type_Info));
  t->k = k;
  t->name = string_duplicate(nm);
  t->size = 8;
  t->alignment = 8;
  return t;
}
static Type_Info *TY_INT, *TY_BOOL, *TY_CHAR, *TY_STR, *TY_FLT, *TY_UINT, *TY_UFLT, *TY_FILE,
    *TY_NAT, *TY_POS;
static void symbol_manager_init(Symbol_Manager *symbol_manager)
{
  memset(symbol_manager, 0, sizeof(*symbol_manager));
  TY_INT = type_new(TYPE_INTEGER, STRING_LITERAL("INTEGER"));
  TY_INT->low_bound = -2147483648LL;
  TY_INT->high_bound = 2147483647LL;
  TY_NAT = type_new(TYPE_INTEGER, STRING_LITERAL("NATURAL"));
  TY_NAT->low_bound = 0;
  TY_NAT->high_bound = 2147483647LL;
  TY_POS = type_new(TYPE_INTEGER, STRING_LITERAL("POSITIVE"));
  TY_POS->low_bound = 1;
  TY_POS->high_bound = 2147483647LL;
  TY_BOOL = type_new(TYPE_BOOLEAN, STRING_LITERAL("BOOLEAN"));
  TY_CHAR = type_new(TYPE_CHARACTER, STRING_LITERAL("CHARACTER"));
  TY_CHAR->size = 1;
  TY_STR = type_new(TYPE_ARRAY, STRING_LITERAL("STRING"));
  TY_STR->element_type = TY_CHAR;
  TY_STR->low_bound = 0;
  TY_STR->high_bound = -1;
  TY_STR->index_type = TY_POS;
  TY_FLT = type_new(TYPE_FLOAT, STRING_LITERAL("FLOAT"));
  TY_UINT = type_new(TYPE_UNSIGNED_INTEGER, STRING_LITERAL("universal_integer"));
  TY_UFLT = type_new(TYPE_UNIVERSAL_FLOAT, STRING_LITERAL("universal_real"));
  TY_FILE = type_new(TYPE_FAT_POINTER, STRING_LITERAL("FILE_TYPE"));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("INTEGER"), 1, TY_INT, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("NATURAL"), 1, TY_NAT, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("POSITIVE"), 1, TY_POS, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("BOOLEAN"), 1, TY_BOOL, 0));
  Symbol *st = symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("TRUE"), 2, TY_BOOL, 0));
  st->value = 1;
  sv(&TY_BOOL->enum_values, st);
  Symbol *sf = symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("FALSE"), 2, TY_BOOL, 0));
  sf->value = 0;
  sv(&TY_BOOL->enum_values, sf);
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("CHARACTER"), 1, TY_CHAR, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("STRING"), 1, TY_STR, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("FLOAT"), 1, TY_FLT, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("FILE_TYPE"), 1, TY_FILE, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("CONSTRAINT_ERROR"), 3, 0, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("PROGRAM_ERROR"), 3, 0, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("STORAGE_ERROR"), 3, 0, 0));
  symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("TASKING_ERROR"), 3, 0, 0));

  // Add TEXT_IO package
  Symbol *text_io = symbol_add_overload(symbol_manager, symbol_new(STRING_LITERAL("TEXT_IO"), 6, 0, 0));

  // Add PUT procedures to TEXT_IO
  Symbol *put_int = symbol_new(STRING_LITERAL("PUT"), 4, 0, 0);
  put_int->parent = text_io;
  symbol_add_overload(symbol_manager, put_int);

  Symbol *put_line_int = symbol_new(STRING_LITERAL("PUT_LINE"), 4, 0, 0);
  put_line_int->parent = text_io;
  symbol_add_overload(symbol_manager, put_line_int);

  Symbol *new_line = symbol_new(STRING_LITERAL("NEW_LINE"), 4, 0, 0);
  new_line->parent = text_io;
  symbol_add_overload(symbol_manager, new_line);

  fv(&symbol_manager->io, stdin);
  fv(&symbol_manager->io, stdout);
  fv(&symbol_manager->io, stderr);
  symbol_manager->fn = 3;
}
static Syntax_Node *generate_equality_operator(Type_Info *t, Source_Location l)
{
  Syntax_Node *f = ND(FB, l);
  f->body.subprogram_spec = ND(FS, l);
  char b[256];
  snprintf(b, 256, "Oeq%.*s", (int) t->name.length, t->name.string);
  f->body.subprogram_spec->subprogram.name = string_duplicate((String_Slice){b, strlen(b)});
  f->body.subprogram_spec->subprogram.operator_symbol = STRING_LITERAL("=");
  Syntax_Node *p1 = ND(PM, l);
  p1->parameter.name = STRING_LITERAL("Source_Location");
  p1->parameter.ty = ND(ID, l);
  p1->parameter.ty->string_value = t->name;
  p1->parameter.mode = 0;
  Syntax_Node *p2 = ND(PM, l);
  p2->parameter.name = STRING_LITERAL("Rational_Number");
  p2->parameter.ty = ND(ID, l);
  p2->parameter.ty->string_value = t->name;
  p2->parameter.mode = 0;
  nv(&f->body.subprogram_spec->subprogram.parameters, p1);
  nv(&f->body.subprogram_spec->subprogram.parameters, p2);
  f->body.subprogram_spec->subprogram.return_type = ND(ID, l);
  f->body.subprogram_spec->subprogram.return_type->string_value = STRING_LITERAL("BOOLEAN");
  Syntax_Node *s = ND(RT, l);
  s->return_stmt.value = ND(BIN, l);
  s->return_stmt.value->binary_node.op = T_EQ;
  if (t->k == TYPE_RECORD)
  {
    s->return_stmt.value->binary_node.left = ND(BIN, l);
    s->return_stmt.value->binary_node.left->binary_node.op = T_AND;
    for (uint32_t i = 0; i < t->components.count; i++)
    {
      Syntax_Node *c = t->components.data[i];
      if (c->k != N_CM)
        continue;
      Syntax_Node *cmp = ND(BIN, l);
      cmp->binary_node.op = T_EQ;
      Syntax_Node *lf = ND(SEL, l);
      lf->selected_component.prefix = ND(ID, l);
      lf->selected_component.prefix->string_value = STRING_LITERAL("Source_Location");
      lf->selected_component.selector = c->component_decl.name;
      Syntax_Node *rf = ND(SEL, l);
      rf->selected_component.prefix = ND(ID, l);
      rf->selected_component.prefix->string_value = STRING_LITERAL("Rational_Number");
      rf->selected_component.selector = c->component_decl.name;
      cmp->binary_node.left = lf;
      cmp->binary_node.right = rf;
      if (not i)
        s->return_stmt.value->binary_node.left = cmp;
      else
      {
        Syntax_Node *a = ND(BIN, l);
        a->binary_node.op = T_AND;
        a->binary_node.left = s->return_stmt.value->binary_node.left;
        a->binary_node.right = cmp;
        s->return_stmt.value->binary_node.left = a;
      }
    }
  }
  else if (t->k == TYPE_ARRAY)
  {
    Syntax_Node *lp = ND(LP, l);
    lp->loop_stmt.iterator = ND(BIN, l);
    lp->loop_stmt.iterator->binary_node.op = T_IN;
    lp->loop_stmt.iterator->binary_node.left = ND(ID, l);
    lp->loop_stmt.iterator->binary_node.left->string_value = STRING_LITERAL("I");
    lp->loop_stmt.iterator->binary_node.right = ND(AT, l);
    lp->loop_stmt.iterator->binary_node.right->attribute.prefix = ND(ID, l);
    lp->loop_stmt.iterator->binary_node.right->attribute.prefix->string_value = STRING_LITERAL("Source_Location");
    lp->loop_stmt.iterator->binary_node.right->attribute.attribute_name = STRING_LITERAL("RANGE");
    Syntax_Node *cmp = ND(BIN, l);
    cmp->binary_node.op = T_NE;
    Syntax_Node *li = ND(IX, l);
    li->index.prefix = ND(ID, l);
    li->index.prefix->string_value = STRING_LITERAL("Source_Location");
    nv(&li->index.indices, ND(ID, l));
    li->index.indices.data[0]->string_value = STRING_LITERAL("I");
    Syntax_Node *ri = ND(IX, l);
    ri->index.prefix = ND(ID, l);
    ri->index.prefix->string_value = STRING_LITERAL("Rational_Number");
    nv(&ri->index.indices, ND(ID, l));
    ri->index.indices.data[0]->string_value = STRING_LITERAL("I");
    cmp->binary_node.left = li;
    cmp->binary_node.right = ri;
    Syntax_Node *rt = ND(RT, l);
    rt->return_stmt.value = ND(ID, l);
    rt->return_stmt.value->string_value = STRING_LITERAL("FALSE");
    Syntax_Node *ifs = ND(IF, l);
    ifs->if_stmt.condition = cmp;
    nv(&ifs->if_stmt.then_statements, rt);
    nv(&lp->loop_stmt.statements, ifs);
    nv(&f->body.statements, lp);
    s->return_stmt.value = ND(ID, l);
    s->return_stmt.value->string_value = STRING_LITERAL("TRUE");
  }
  nv(&f->body.statements, s);
  return f;
}
static Syntax_Node *generate_assignment_operator(Type_Info *t, Source_Location l)
{
  Syntax_Node *parser = ND(PB, l);
  parser->body.subprogram_spec = ND(PS, l);
  char b[256];
  snprintf(b, 256, "Oas%.*s", (int) t->name.length, t->name.string);
  parser->body.subprogram_spec->subprogram.name = string_duplicate((String_Slice){b, strlen(b)});
  parser->body.subprogram_spec->subprogram.operator_symbol = STRING_LITERAL(":=");
  Syntax_Node *p1 = ND(PM, l);
  p1->parameter.name = STRING_LITERAL("T");
  p1->parameter.ty = ND(ID, l);
  p1->parameter.ty->string_value = t->name;
  p1->parameter.mode = 3;
  Syntax_Node *p2 = ND(PM, l);
  p2->parameter.name = STRING_LITERAL("String_Slice");
  p2->parameter.ty = ND(ID, l);
  p2->parameter.ty->string_value = t->name;
  p2->parameter.mode = 0;
  nv(&parser->body.subprogram_spec->subprogram.parameters, p1);
  nv(&parser->body.subprogram_spec->subprogram.parameters, p2);
  if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->components.count; i++)
    {
      Syntax_Node *c = t->components.data[i];
      if (c->k != N_CM)
        continue;
      Syntax_Node *as = ND(AS, l);
      Syntax_Node *lt = ND(SEL, l);
      lt->selected_component.prefix = ND(ID, l);
      lt->selected_component.prefix->string_value = STRING_LITERAL("T");
      lt->selected_component.selector = c->component_decl.name;
      Syntax_Node *rs = ND(SEL, l);
      rs->selected_component.prefix = ND(ID, l);
      rs->selected_component.prefix->string_value = STRING_LITERAL("String_Slice");
      rs->selected_component.selector = c->component_decl.name;
      as->assignment.target = lt;
      as->assignment.value = rs;
      nv(&parser->body.statements, as);
    }
  }
  else if (t->k == TYPE_ARRAY)
  {
    Syntax_Node *lp = ND(LP, l);
    lp->loop_stmt.iterator = ND(BIN, l);
    lp->loop_stmt.iterator->binary_node.op = T_IN;
    lp->loop_stmt.iterator->binary_node.left = ND(ID, l);
    lp->loop_stmt.iterator->binary_node.left->string_value = STRING_LITERAL("I");
    lp->loop_stmt.iterator->binary_node.right = ND(AT, l);
    lp->loop_stmt.iterator->binary_node.right->attribute.prefix = ND(ID, l);
    lp->loop_stmt.iterator->binary_node.right->attribute.prefix->string_value = STRING_LITERAL("T");
    lp->loop_stmt.iterator->binary_node.right->attribute.attribute_name = STRING_LITERAL("RANGE");
    Syntax_Node *as = ND(AS, l);
    Syntax_Node *ti = ND(IX, l);
    ti->index.prefix = ND(ID, l);
    ti->index.prefix->string_value = STRING_LITERAL("T");
    nv(&ti->index.indices, ND(ID, l));
    ti->index.indices.data[0]->string_value = STRING_LITERAL("I");
    Syntax_Node *si = ND(IX, l);
    si->index.prefix = ND(ID, l);
    si->index.prefix->string_value = STRING_LITERAL("String_Slice");
    nv(&si->index.indices, ND(ID, l));
    si->index.indices.data[0]->string_value = STRING_LITERAL("I");
    as->assignment.target = ti;
    as->assignment.value = si;
    nv(&lp->loop_stmt.statements, as);
    nv(&parser->body.statements, lp);
  }
  return parser;
}
static Syntax_Node *generate_input_operator(Type_Info *t, Source_Location l)
{
  Syntax_Node *f = ND(FB, l);
  f->body.subprogram_spec = ND(FS, l);
  char b[256];
  snprintf(b, 256, "Oin%.*s", (int) t->name.length, t->name.string);
  f->body.subprogram_spec->subprogram.name = string_duplicate((String_Slice){b, strlen(b)});
  f->body.subprogram_spec->subprogram.return_type = ND(ID, l);
  f->body.subprogram_spec->subprogram.return_type->string_value = t->name;
  Syntax_Node *ag = ND(AG, l);
  if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->components.count; i++)
    {
      Syntax_Node *c = t->components.data[i];
      if (c->k != N_CM or not c->component_decl.in)
        continue;
      Syntax_Node *a = ND(ASC, l);
      nv(&a->association.choices, ND(ID, l));
      a->association.choices.data[0]->string_value = c->component_decl.name;
      a->association.value = c->component_decl.in;
      nv(&ag->aggregate.items, a);
    }
  }
  Syntax_Node *rt = ND(RT, l);
  rt->return_stmt.value = ag;
  nv(&f->body.statements, rt);
  return ag->aggregate.items.count > 0 ? f : 0;
}
static void find_type(Symbol_Manager *symbol_manager, Type_Info *t, Source_Location l)
{
  if (not t or t->frozen)
    return;
  if (t->k == TY_PT and t->parent_type and not t->parent_type->frozen)
    return;
  t->frozen = 1;
  t->frozen_node = ND(ERR, l);
  if (t->base_type and t->base_type != t and not t->base_type->frozen)
    find_type(symbol_manager, t->base_type, l);
  if (t->parent_type and not t->parent_type->frozen)
    find_type(symbol_manager, t->parent_type, l);
  if (t->element_type and not t->element_type->frozen)
    find_type(symbol_manager, t->element_type, l);
  if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->components.count; i++)
      if (t->components.data[i]->symbol and t->components.data[i]->symbol->type_info)
        find_type(symbol_manager, t->components.data[i]->symbol->type_info, l);
    uint32_t of = 0, mx = 1;
    for (uint32_t i = 0; i < t->components.count; i++)
    {
      Syntax_Node *c = t->components.data[i];
      if (c->k != N_CM)
        continue;
      Type_Info *ct = c->component_decl.ty ? c->component_decl.ty->ty : 0;
      uint32_t ca = ct and ct->alignment ? ct->alignment : 8, cs = ct and ct->size ? ct->size : 8;
      if (ca > mx)
        mx = ca;
      of = (of + ca - 1) & ~(ca - 1);
      c->component_decl.offset = of;
      of += cs;
    }
    t->size = (of + mx - 1) & ~(mx - 1);
    t->alignment = mx;
  }
  if (t->k == TYPE_ARRAY and t->element_type)
  {
    Type_Info *et = t->element_type;
    uint32_t ea = et->alignment ? et->alignment : 8, es = et->size ? et->size : 8;
    int64_t n = (t->high_bound - t->low_bound + 1);
    t->size = n > 0 ? n * es : 0;
    t->alignment = ea;
  }
  if ((t->k == TYPE_RECORD or t->k == TYPE_ARRAY) and t->name.string and t->name.length)
  {
    Syntax_Node *eq = generate_equality_operator(t, l);
    if (eq)
      nv(&t->operations, eq);
    Syntax_Node *as = generate_assignment_operator(t, l);
    if (as)
      nv(&t->operations, as);
    Syntax_Node *in = generate_input_operator(t, l);
    if (in)
      nv(&t->operations, in);
  }
}
static void find_symbol(Symbol_Manager *symbol_manager, Symbol *s, Source_Location l)
{
  if (not s or s->frozen)
    return;
  s->frozen = 1;
  s->frozen_node = ND(ERR, l);
  if (s->type_info and not s->type_info->frozen)
    find_type(symbol_manager, s->type_info, l);
}
static void find_ada_library(Symbol_Manager *symbol_manager, Source_Location l)
{
  for (int i = 0; i < 4096; i++)
    for (Symbol *s = symbol_manager->sy[i]; s; s = s->next)
      if (s->scope == symbol_manager->sc and not s->frozen)
      {
        if (s->type_info and s->type_info->k == TY_PT and s->type_info->parent_type and not s->type_info->parent_type->frozen)
          continue;
        if (s->type_info)
          find_type(symbol_manager, s->type_info, l);
        find_symbol(symbol_manager, s, l);
      }
}
static void symbol_compare_parameter(Symbol_Manager *symbol_manager)
{
  symbol_manager->sc++;
  symbol_manager->ss++;
  if (symbol_manager->ssd < 256)
  {
    int m = symbol_manager->ssd;
    symbol_manager->ssd++;
    symbol_manager->sst[m] = 0;
  }
}
static void symbol_compare_overload(Symbol_Manager *symbol_manager)
{
  find_ada_library(symbol_manager, (Source_Location){0, 0, ""});
  for (int i = 0; i < 4096; i++)
    for (Symbol *s = symbol_manager->sy[i]; s; s = s->next)
    {
      if (s->scope == symbol_manager->sc)
      {
        s->visibility &= ~1;
        if (s->k == 6)
          s->visibility = 3;
      }
      if (s->visibility & 2 and s->parent and s->parent->scope >= symbol_manager->sc)
        s->visibility &= ~2;
    }
  if (symbol_manager->ssd > 0)
    symbol_manager->ssd--;
  symbol_manager->sc--;
}
static Type_Info *resolve_subtype(Symbol_Manager *symbol_manager, Syntax_Node *node);
static void resolve_expression(Symbol_Manager *symbol_manager, Syntax_Node *node, Type_Info *tx);
static void resolve_statement_sequence(Symbol_Manager *symbol_manager, Syntax_Node *node);
static void resolve_declaration(Symbol_Manager *symbol_manager, Syntax_Node *n);
static void runtime_register_compare(Symbol_Manager *symbol_manager, Representation_Clause *r);
static Syntax_Node *generate_clone(Symbol_Manager *symbol_manager, Syntax_Node *n);
static Type_Info *type_canonical_concrete(Type_Info *t)
{
  if (not t)
    return TY_INT;
  if (t->k == TYPE_UNSIGNED_INTEGER)
    return TY_INT;
  if (t->k == TYPE_UNIVERSAL_FLOAT)
    return TY_FLT;
  if (t->k == TYPE_FIXED_POINT)
    return TY_FLT;
  if ((t->k == TYPE_DERIVED or t->k == TY_PT) and t->parent_type)
    return type_canonical_concrete(t->parent_type);
  return t;
}
typedef enum
{
  RC_INT,
  REPR_CAT_FLOAT,
  REPR_CAT_POINTER,
  RC_STRUCT
} ReprCat;
typedef enum
{
  COMP_NONE = 0,
  COMP_SAME = 1000,
  COMP_DERIVED,
  COMP_BASED_ON,
  COMP_ARRAY_ELEMENT,
  COMP_ACCESS_DESIGNATED
} CompatKind;
static ReprCat representation_category(Type_Info *t)
{
  if (not t)
    return RC_INT;
  switch (t->k)
  {
  case TYPE_FLOAT:
  case TYPE_UNIVERSAL_FLOAT:
  case TYPE_FIXED_POINT:
    return REPR_CAT_FLOAT;
  case TYPE_FAT_POINTER:
  case TYPE_ARRAY:
  case TYPE_RECORD:
  case TYPE_STRING:
  case TYPE_ACCESS:
    return REPR_CAT_POINTER;
  default:
    return RC_INT;
  }
}
static Type_Info *semantic_base(Type_Info *t)
{
  if (not t)
    return TY_INT;
  for (Type_Info *parser = t; parser; parser = parser->base_type ? parser->base_type : parser->parent_type)
  {
    if (not parser->base_type and not parser->parent_type)
      return parser;
    if (parser->k == TYPE_DERIVED and parser->parent_type)
      return semantic_base(parser->parent_type);
    if (parser->k == TYPE_UNSIGNED_INTEGER)
      return TY_INT;
    if (parser->k == TYPE_UNIVERSAL_FLOAT or parser->k == TYPE_FIXED_POINT)
      return TY_FLT;
  }
  return t;
}
static inline bool is_integer_type(Type_Info *t)
{
  t = semantic_base(t);
  return t->k == TYPE_INTEGER;
}
static inline bool is_real_type(Type_Info *t)
{
  t = semantic_base(t);
  return t->k == TYPE_FLOAT;
}
static inline bool is_discrete(Type_Info *t)
{
  return is_integer_type(t) or t->k == TYPE_ENUMERATION or t->k == TYPE_CHARACTER;
}
static inline bool is_array(Type_Info *t)
{
  return t and type_canonical_concrete(t)->k == TYPE_ARRAY;
}
static inline bool is_record(Type_Info *t)
{
  return t and type_canonical_concrete(t)->k == TYPE_RECORD;
}
static inline bool is_access(Type_Info *t)
{
  return t and type_canonical_concrete(t)->k == TYPE_ACCESS;
}
static bool is_check_suppressed(Type_Info *t, unsigned kind)
{
  for (Type_Info *parser = t; parser; parser = parser->base_type)
  {
    if (parser->suppressed_checks & kind)
      return true;
  }
  return false;
}
static CompatKind type_compat_kind(Type_Info *a, Type_Info *b)
{
  if (not a or not b)
    return COMP_NONE;
  if (a == b)
    return COMP_SAME;
  if (a == TY_STR and b->k == TYPE_ARRAY and b->element_type and b->element_type->k == TYPE_CHARACTER)
    return COMP_ARRAY_ELEMENT;
  if (b == TY_STR and a->k == TYPE_ARRAY and a->element_type and a->element_type->k == TYPE_CHARACTER)
    return COMP_ARRAY_ELEMENT;
  if (a->parent_type == b or b->parent_type == a)
    return COMP_DERIVED;
  if (a->base_type == b or b->base_type == a)
    return COMP_BASED_ON;
  if ((a->k == TYPE_INTEGER or a->k == TYPE_UNSIGNED_INTEGER)
      and (b->k == TYPE_INTEGER or b->k == TYPE_UNSIGNED_INTEGER))
    return COMP_SAME;
  if ((a->k == TYPE_FLOAT or a->k == TYPE_UNIVERSAL_FLOAT)
      and (b->k == TYPE_FLOAT or b->k == TYPE_UNIVERSAL_FLOAT))
    return COMP_SAME;
  if (a->k == TYPE_ARRAY and b->k == TYPE_ARRAY and type_compat_kind(a->element_type, b->element_type) != COMP_NONE)
    return COMP_ARRAY_ELEMENT;
  if (a->k == TYPE_ACCESS and b->k == TYPE_ACCESS)
    return type_compat_kind(a->element_type, b->element_type) != COMP_NONE ? COMP_ACCESS_DESIGNATED : COMP_NONE;
  if (a->k == TYPE_DERIVED)
    return type_compat_kind(a->parent_type, b);
  if (b->k == TYPE_DERIVED)
    return type_compat_kind(a, b->parent_type);
  return COMP_NONE;
}
static int type_scope(Type_Info *a, Type_Info *b, Type_Info *tx)
{
  CompatKind k = type_compat_kind(a, b);
  switch (k)
  {
  case COMP_SAME:
    return 1000;
  case COMP_DERIVED:
    return 900;
  case COMP_BASED_ON:
    return 800;
  case COMP_ARRAY_ELEMENT:
    return 600 + type_scope(a->element_type, b->element_type, tx);
  case COMP_ACCESS_DESIGNATED:
    return 500 + type_scope(a->element_type, b->element_type, 0);
  default:
    break;
  }
  if (tx and a and a->element_type and b == tx)
    return 400;
  return 0;
}
static bool type_covers(Type_Info *a, Type_Info *b)
{
  if (not a or not b)
    return 0;
  if (type_compat_kind(a, b) != COMP_NONE)
    return 1;
  Type_Info *ab = semantic_base(a), *bb = semantic_base(b);
  if ((ab == TY_BOOL or ab->k == TYPE_BOOLEAN) and (bb == TY_BOOL or bb->k == TYPE_BOOLEAN))
    return 1;
  if (is_discrete(a) and is_discrete(b))
    return 1;
  if (is_real_type(a) and is_real_type(b))
    return 1;
  return 0;
}
static Type_Info *resolve_subtype(Symbol_Manager *symbol_manager, Syntax_Node *node)
{
  if (not node)
    return TY_INT;
  if (node->k == N_ID)
  {
    Symbol *s = symbol_find(symbol_manager, node->string_value);
    if (s and s->type_info)
      return s->type_info;
    return TY_INT;
  }
  if (node->k == N_SEL)
  {
    Syntax_Node *parser = node->selected_component.prefix;
    if (parser->k == N_ID)
    {
      Symbol *ps = symbol_find(symbol_manager, parser->string_value);
      if (ps and ps->k == 6 and ps->definition and ps->definition->k == N_PKS)
      {
        Syntax_Node *pk = ps->definition;
        for (uint32_t i = 0; i < pk->package_spec.private_declarations.count; i++)
        {
          Syntax_Node *d = pk->package_spec.private_declarations.data[i];
          if (d->symbol and string_equal_ignore_case(d->symbol->name, node->selected_component.selector) and d->symbol->type_info)
            return d->symbol->type_info;
          if (d->k == N_TD and string_equal_ignore_case(d->type_decl.name, node->selected_component.selector))
            return resolve_subtype(symbol_manager, d->type_decl.definition);
        }
        for (uint32_t i = 0; i < pk->package_spec.declarations.count; i++)
        {
          Syntax_Node *d = pk->package_spec.declarations.data[i];
          if (d->symbol and string_equal_ignore_case(d->symbol->name, node->selected_component.selector) and d->symbol->type_info)
            return d->symbol->type_info;
          if (d->k == N_TD and string_equal_ignore_case(d->type_decl.name, node->selected_component.selector))
            return resolve_subtype(symbol_manager, d->type_decl.definition);
        }
      }
      return resolve_subtype(symbol_manager, parser);
    }
    return TY_INT;
  }
  if (node->k == N_ST)
  {
    Type_Info *bt = resolve_subtype(symbol_manager, node->subtype_decl.index_constraint);
    Syntax_Node *cn = node->subtype_decl.constraint ? node->subtype_decl.constraint : node->subtype_decl.range_constraint;
    if (cn and bt)
    {
      Type_Info *t = type_new(bt->k, N);
      t->base_type = bt;
      t->element_type = bt->element_type;
      t->components = bt->components;
      t->discriminants = bt->discriminants;
      t->size = bt->size;
      t->alignment = bt->alignment;
      t->address = bt->address;
      t->is_packed = bt->is_packed;
      t->index_type = bt->index_type;
      if (cn->k == 27 and cn->constraint.constraints.count > 0 and cn->constraint.constraints.data[0] and cn->constraint.constraints.data[0]->k == 26)
      {
        Syntax_Node *rn = cn->constraint.constraints.data[0];
        resolve_expression(symbol_manager, rn->range.low_bound, 0);
        resolve_expression(symbol_manager, rn->range.high_bound, 0);
        Syntax_Node *lo = rn->range.low_bound;
        Syntax_Node *hi = rn->range.high_bound;
        int64_t lov = lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_INT ? -lo->unary_node.operand->integer_value
                      : lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -lo->unary_node.operand->float_value})
                                .i
                      : lo->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = lo->float_value})
                                              .i
                      : lo->k == N_ID and lo->symbol and lo->symbol->k == 2 ? lo->symbol->value
                                                                    : lo->integer_value;
        int64_t hiv = hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_INT ? -hi->unary_node.operand->integer_value
                      : hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -hi->unary_node.operand->float_value})
                                .i
                      : hi->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = hi->float_value})
                                              .i
                      : hi->k == N_ID and hi->symbol and hi->symbol->k == 2 ? hi->symbol->value
                                                                    : hi->integer_value;
        t->low_bound = lov;
        t->high_bound = hiv;
        return t;
      }
      else if (cn->k == 27 and cn->constraint.range_spec)
      {
        resolve_expression(symbol_manager, cn->constraint.range_spec->range.low_bound, 0);
        resolve_expression(symbol_manager, cn->constraint.range_spec->range.high_bound, 0);
        Syntax_Node *lo = cn->constraint.range_spec->range.low_bound;
        Syntax_Node *hi = cn->constraint.range_spec->range.high_bound;
        int64_t lov = lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_INT ? -lo->unary_node.operand->integer_value
                      : lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -lo->unary_node.operand->float_value})
                                .i
                      : lo->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = lo->float_value})
                                              .i
                      : lo->k == N_ID and lo->symbol and lo->symbol->k == 2 ? lo->symbol->value
                                                                    : lo->integer_value;
        int64_t hiv = hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_INT ? -hi->unary_node.operand->integer_value
                      : hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -hi->unary_node.operand->float_value})
                                .i
                      : hi->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = hi->float_value})
                                              .i
                      : hi->k == N_ID and hi->symbol and hi->symbol->k == 2 ? hi->symbol->value
                                                                    : hi->integer_value;
        t->low_bound = lov;
        t->high_bound = hiv;
        return t;
      }
      else if (cn->k == N_RN)
      {
        resolve_expression(symbol_manager, cn->range.low_bound, 0);
        resolve_expression(symbol_manager, cn->range.high_bound, 0);
        Syntax_Node *lo = cn->range.low_bound;
        Syntax_Node *hi = cn->range.high_bound;
        int64_t lov = lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_INT ? -lo->unary_node.operand->integer_value
                      : lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -lo->unary_node.operand->float_value})
                                .i
                      : lo->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = lo->float_value})
                                              .i
                      : lo->k == N_ID and lo->symbol and lo->symbol->k == 2 ? lo->symbol->value
                                                                    : lo->integer_value;
        int64_t hiv = hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_INT ? -hi->unary_node.operand->integer_value
                      : hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -hi->unary_node.operand->float_value})
                                .i
                      : hi->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = hi->float_value})
                                              .i
                      : hi->k == N_ID and hi->symbol and hi->symbol->k == 2 ? hi->symbol->value
                                                                    : hi->integer_value;
        t->low_bound = lov;
        t->high_bound = hiv;
        return t;
      }
    }
    return bt;
  }
  if (node->k == N_TI)
  {
    resolve_expression(symbol_manager, node->range.low_bound, 0);
    resolve_expression(symbol_manager, node->range.high_bound, 0);
    Type_Info *t = type_new(TYPE_INTEGER, N);
    if (node->range.low_bound and node->range.low_bound->k == N_INT)
      t->low_bound = node->range.low_bound->integer_value;
    else if (
        node->range.low_bound and node->range.low_bound->k == N_UN and node->range.low_bound->unary_node.op == T_MN and node->range.low_bound->unary_node.operand->k == N_INT)
      t->low_bound = -node->range.low_bound->unary_node.operand->integer_value;
    if (node->range.high_bound and node->range.high_bound->k == N_INT)
      t->high_bound = node->range.high_bound->integer_value;
    else if (
        node->range.high_bound and node->range.high_bound->k == N_UN and node->range.high_bound->unary_node.op == T_MN and node->range.high_bound->unary_node.operand->k == N_INT)
      t->high_bound = -node->range.high_bound->unary_node.operand->integer_value;
    return t;
  }
  if (node->k == N_TX)
  {
    Type_Info *t = type_new(TYPE_FIXED_POINT, N);
    double d = 1.0;
    if (node->range.low_bound and node->range.low_bound->k == N_REAL)
      d = node->range.low_bound->float_value;
    else if (node->range.low_bound and node->range.low_bound->k == N_INT)
      d = node->range.low_bound->integer_value;
    t->small_value = (int64_t) (1.0 / d);
    if (node->range.high_bound and node->range.high_bound->k == N_INT)
      t->low_bound = node->range.high_bound->integer_value;
    if (node->binary_node.right and node->binary_node.right->k == N_INT)
      t->high_bound = node->binary_node.right->integer_value;
    return t;
  }
  if (node->k == N_TE)
    return type_new(TYPE_INTEGER, N);
  if (node->k == N_TF)
  {
    Type_Info *t = type_new(TYPE_FLOAT, N);
    if (node->unary_node.operand)
    {
      resolve_expression(symbol_manager, node->unary_node.operand, 0);
      if (node->unary_node.operand->k == N_INT)
        t->small_value = node->unary_node.operand->integer_value;
    }
    return t;
  }
  if (node->k == N_TA)
  {
    Type_Info *t = type_new(TYPE_ARRAY, N);
    t->element_type = resolve_subtype(symbol_manager, node->index.prefix);
    if (node->index.indices.count == 1)
    {
      Syntax_Node *r = node->index.indices.data[0];
      if (r and r->k == N_RN)
      {
        resolve_expression(symbol_manager, r->range.low_bound, 0);
        resolve_expression(symbol_manager, r->range.high_bound, 0);
        Syntax_Node *lo = r->range.low_bound;
        Syntax_Node *hi = r->range.high_bound;
        if (lo and lo->k == N_INT)
          t->low_bound = lo->integer_value;
        else if (lo and lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_INT)
          t->low_bound = -lo->unary_node.operand->integer_value;
        if (hi and hi->k == N_INT)
          t->high_bound = hi->integer_value;
        else if (hi and hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_INT)
          t->high_bound = -hi->unary_node.operand->integer_value;
      }
    }
    return t;
  }
  if (node->k == N_TR)
    return type_new(TYPE_RECORD, N);
  if (node->k == N_TP)
    return type_new(TY_PT, N);
  if (node->k == N_TAC)
  {
    Type_Info *t = type_new(TYPE_ACCESS, N);
    t->element_type = resolve_subtype(symbol_manager, node->unary_node.operand);
    return t;
  }
  if (node->k == N_IX)
  {
    Type_Info *bt = resolve_subtype(symbol_manager, node->index.prefix);
    if (bt and bt->k == TYPE_ARRAY and bt->low_bound == 0 and bt->high_bound == -1 and node->index.indices.count == 1)
    {
      Syntax_Node *r = node->index.indices.data[0];
      if (r and r->k == N_RN)
      {
        resolve_expression(symbol_manager, r->range.low_bound, 0);
        resolve_expression(symbol_manager, r->range.high_bound, 0);
        Type_Info *t = type_new(TYPE_ARRAY, N);
        t->element_type = bt->element_type;
        t->index_type = bt->index_type;
        t->base_type = bt;
        if (r->range.low_bound and r->range.low_bound->k == N_INT)
          t->low_bound = r->range.low_bound->integer_value;
        else if (
            r->range.low_bound and r->range.low_bound->k == N_UN and r->range.low_bound->unary_node.op == T_MN
            and r->range.low_bound->unary_node.operand->k == N_INT)
          t->low_bound = -r->range.low_bound->unary_node.operand->integer_value;
        if (r->range.high_bound and r->range.high_bound->k == N_INT)
          t->high_bound = r->range.high_bound->integer_value;
        else if (
            r->range.high_bound and r->range.high_bound->k == N_UN and r->range.high_bound->unary_node.op == T_MN
            and r->range.high_bound->unary_node.operand->k == N_INT)
          t->high_bound = -r->range.high_bound->unary_node.operand->integer_value;
        return t;
      }
    }
    return bt;
  }
  if (node->k == N_RN)
  {
    resolve_expression(symbol_manager, node->range.low_bound, 0);
    resolve_expression(symbol_manager, node->range.high_bound, 0);
    Type_Info *t = type_new(TYPE_INTEGER, N);
    if (node->range.low_bound and node->range.low_bound->k == N_INT)
      t->low_bound = node->range.low_bound->integer_value;
    else if (
        node->range.low_bound and node->range.low_bound->k == N_UN and node->range.low_bound->unary_node.op == T_MN and node->range.low_bound->unary_node.operand->k == N_INT)
      t->low_bound = -node->range.low_bound->unary_node.operand->integer_value;
    if (node->range.high_bound and node->range.high_bound->k == N_INT)
      t->high_bound = node->range.high_bound->integer_value;
    else if (
        node->range.high_bound and node->range.high_bound->k == N_UN and node->range.high_bound->unary_node.op == T_MN and node->range.high_bound->unary_node.operand->k == N_INT)
      t->high_bound = -node->range.high_bound->unary_node.operand->integer_value;
    return t;
  }
  if (node->k == N_CL)
  {
    Type_Info *bt = resolve_subtype(symbol_manager, node->call.function_name);
    if (bt and bt->k == TYPE_ARRAY and bt->low_bound == 0 and bt->high_bound == -1 and node->call.arguments.count == 1)
    {
      Syntax_Node *r = node->call.arguments.data[0];
      if (r and r->k == N_RN)
      {
        resolve_expression(symbol_manager, r->range.low_bound, 0);
        resolve_expression(symbol_manager, r->range.high_bound, 0);
        Type_Info *t = type_new(TYPE_ARRAY, N);
        t->element_type = bt->element_type;
        t->index_type = bt->index_type;
        t->base_type = bt;
        if (r->range.low_bound and r->range.low_bound->k == N_INT)
          t->low_bound = r->range.low_bound->integer_value;
        else if (
            r->range.low_bound and r->range.low_bound->k == N_UN and r->range.low_bound->unary_node.op == T_MN
            and r->range.low_bound->unary_node.operand->k == N_INT)
          t->low_bound = -r->range.low_bound->unary_node.operand->integer_value;
        if (r->range.high_bound and r->range.high_bound->k == N_INT)
          t->high_bound = r->range.high_bound->integer_value;
        else if (
            r->range.high_bound and r->range.high_bound->k == N_UN and r->range.high_bound->unary_node.op == T_MN
            and r->range.high_bound->unary_node.operand->k == N_INT)
          t->high_bound = -r->range.high_bound->unary_node.operand->integer_value;
        return t;
      }
    }
    return bt;
  }
  return TY_INT;
}
static Symbol *symbol_character_literal(Symbol_Manager *symbol_manager, char c, Type_Info *tx)
{
  if (tx and tx->k == TYPE_ENUMERATION)
  {
    for (uint32_t i = 0; i < tx->enum_values.count; i++)
    {
      Symbol *e = tx->enum_values.data[i];
      if (e->name.length == 1 and tolower(e->name.string[0]) == tolower(c))
        return e;
    }
  }
  if (tx and tx->k == TYPE_DERIVED and tx->parent_type)
    return symbol_character_literal(symbol_manager, c, tx->parent_type);
  for (Symbol *s = symbol_manager->sy[symbol_hash((String_Slice){&c, 1})]; s; s = s->next)
    if (s->name.length == 1 and tolower(s->name.string[0]) == tolower(c) and s->k == 2 and s->type_info
        and (s->type_info->k == TYPE_ENUMERATION or (s->type_info->k == TYPE_DERIVED and s->type_info->parent_type and s->type_info->parent_type->k == TYPE_ENUMERATION)))
      return s;
  return 0;
}
static inline Syntax_Node *make_check(Syntax_Node *ex, String_Slice ec, Source_Location l)
{
  Syntax_Node *c = ND(CHK, l);
  c->check.expression = ex;
  c->check.exception_name = ec;
  c->ty = ex->ty;
  return c;
}
static inline bool is_unconstrained_array(Type_Info *t)
{
  return t and t->k == TYPE_ARRAY and t->low_bound == 0 and t->high_bound == -1;
}
static Type_Info *base_scalar(Type_Info *t)
{
  if (not t)
    return TY_INT;
  for (Type_Info *parser = t; parser; parser = parser->base_type)
    if (not parser->base_type
        or (parser->k != TYPE_INTEGER and parser->k != TYPE_ENUMERATION and parser->k != TYPE_DERIVED
            and parser->k != TYPE_CHARACTER and parser->k != TYPE_FLOAT))
      return parser;
  return t;
}
static inline bool is_unc_scl(Type_Info *t)
{
  if (not t or not(is_discrete(t) or is_real_type(t)))
    return 0;
  Type_Info *b = base_scalar(t);
  return t->low_bound == b->low_bound and t->high_bound == b->high_bound;
}
static inline double type_bound_double(int64_t b)
{
  union
  {
    int64_t i;
    double d;
  } u;
  u.i = b;
  return u.d;
}
static bool descendant_conformant(Type_Info *t, Type_Info *s)
{
  if (not t or not s or t->discriminants.count == 0 or s->discriminants.count == 0)
    return 0;
  uint32_t node = t->discriminants.count < s->discriminants.count ? t->discriminants.count : s->discriminants.count;
  for (uint32_t i = 0; i < node; i++)
  {
    Syntax_Node *ad = t->discriminants.data[i], *bd = s->discriminants.data[i];
    if (not(ad and bd and ad->k == N_DS and bd->k == N_DS and ad->parameter.default_value and bd->parameter.default_value
            and ad->parameter.default_value->k == N_INT and bd->parameter.default_value->k == N_INT))
      continue;
    if (ad->parameter.default_value->integer_value != bd->parameter.default_value->integer_value)
      return 1;
  }
  return 0;
}
static Syntax_Node *chk(Symbol_Manager *symbol_manager, Syntax_Node *node, Source_Location l)
{
  (void) symbol_manager;
  if (not node or not node->ty)
    return node;
  Type_Info *t = type_canonical_concrete(node->ty);
  if ((is_discrete(t) or is_real_type(t)) and (node->ty->low_bound != TY_INT->low_bound or node->ty->high_bound != TY_INT->high_bound)
      and not is_check_suppressed(node->ty, CHK_RNG))
    return make_check(node, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  if (t->k == TYPE_RECORD and descendant_conformant(t, node->ty) and not is_check_suppressed(t, CHK_DSC))
    return make_check(node, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  if (t->k == TYPE_ARRAY and node->ty and node->ty->k == TYPE_ARRAY and node->ty->index_type
      and (node->ty->low_bound < node->ty->index_type->low_bound or node->ty->high_bound > node->ty->index_type->high_bound))
    return make_check(node, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  if (t->k == TYPE_ARRAY and node->ty and node->ty->k == TYPE_ARRAY and not is_unconstrained_array(t)
      and not is_check_suppressed(t, CHK_IDX) and (t->low_bound != node->ty->low_bound or t->high_bound != node->ty->high_bound))
    return make_check(node, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  return node;
}
static inline int64_t range_size(int64_t lo, int64_t hi)
{
  return hi >= lo ? hi - lo + 1 : 0;
}
static inline bool is_static(Syntax_Node *node)
{
  return node
         and (node->k == N_INT or (node->k == N_UN and node->unary_node.op == T_MN and node->unary_node.operand and node->unary_node.operand->k == N_INT));
}
static int find_or_throw(Syntax_Node *ag)
{
  if (not ag or ag->k != N_AG)
    return -1;
  for (uint32_t i = 0; i < ag->aggregate.items.count; i++)
  {
    Syntax_Node *e = ag->aggregate.items.data[i];
    if (e->k == N_ASC and e->association.choices.count == 1 and e->association.choices.data[0]->k == N_ID
        and string_equal_ignore_case(e->association.choices.data[0]->string_value, STRING_LITERAL("others")))
      return i;
  }
  return -1;
}
static void normalize_array_aggregate(Symbol_Manager *symbol_manager, Type_Info *at, Syntax_Node *ag)
{
  (void) symbol_manager;
  if (not ag or not at or at->k != TYPE_ARRAY)
    return;
  int64_t asz = range_size(at->low_bound, at->high_bound);
  if (asz > 4096)
    return;
  Node_Vector xv = {0};
  bool *cov = calloc(asz, 1);
  int oi = find_or_throw(ag);
  uint32_t px = 0;
  for (uint32_t i = 0; i < ag->aggregate.items.count; i++)
  {
    if ((int) i == oi)
      continue;
    Syntax_Node *e = ag->aggregate.items.data[i];
    if (e->k == N_ASC)
    {
      for (uint32_t j = 0; j < e->association.choices.count; j++)
      {
        Syntax_Node *ch = e->association.choices.data[j];
        int64_t idx = -1;
        if (ch->k == N_INT)
          idx = ch->integer_value - at->low_bound;
        else if (ch->k == N_RN)
        {
          for (int64_t k = ch->range.low_bound->integer_value; k <= ch->range.high_bound->integer_value; k++)
          {
            int64_t ridx = k - at->low_bound;
            if (ridx >= 0 and ridx < asz)
            {
              if (cov[ridx] and error_count < 99)
                fatal_error(ag->location, "dup ag");
              cov[ridx] = 1;
              while (xv.count <= (uint32_t) ridx)
                nv(&xv, ND(INT, ag->location));
              xv.data[ridx] = e->association.value;
            }
          }
          continue;
        }
        if (idx >= 0 and idx < asz)
        {
          if (cov[idx] and error_count < 99)
            fatal_error(ag->location, "dup ag");
          cov[idx] = 1;
          while (xv.count <= (uint32_t) idx)
            nv(&xv, ND(INT, ag->location));
          xv.data[idx] = e->association.value;
        }
      }
    }
    else
    {
      if (px < (uint32_t) asz)
      {
        if (cov[px] and error_count < 99)
          fatal_error(ag->location, "dup ag");
        cov[px] = 1;
        while (xv.count <= px)
          nv(&xv, ND(INT, ag->location));
        xv.data[px] = e;
      }
      px++;
    }
  }
  if (oi >= 0)
  {
    Syntax_Node *oe = ag->aggregate.items.data[oi];
    for (int64_t i = 0; i < asz; i++)
      if (not cov[i])
      {
        while (xv.count <= (uint32_t) i)
          nv(&xv, ND(INT, ag->location));
        xv.data[i] = oe->association.value;
        cov[i] = 1;
      }
  }
  for (int64_t i = 0; i < asz; i++)
    if (not cov[i] and error_count < 99)
      fatal_error(ag->location, "ag gap %lld", i);
  ag->aggregate.items = xv;
  free(cov);
}
static void normalize_record_aggregate(Symbol_Manager *symbol_manager, Type_Info *rt, Syntax_Node *ag)
{
  (void) symbol_manager;
  if (not ag or not rt or rt->k != TYPE_RECORD)
    return;
  bool cov[256] = {0};
  for (uint32_t i = 0; i < ag->aggregate.items.count; i++)
  {
    Syntax_Node *e = ag->aggregate.items.data[i];
    if (e->k != N_ASC)
      continue;
    for (uint32_t j = 0; j < e->association.choices.count; j++)
    {
      Syntax_Node *ch = e->association.choices.data[j];
      if (ch->k == N_ID)
      {
        if (string_equal_ignore_case(ch->string_value, STRING_LITERAL("others")))
        {
          for (uint32_t k = 0; k < rt->components.count; k++)
            if (not cov[k])
              cov[k] = 1;
          continue;
        }
        for (uint32_t k = 0; k < rt->components.count; k++)
        {
          Syntax_Node *c = rt->components.data[k];
          if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, ch->string_value))
          {
            if (cov[c->component_decl.offset] and error_count < 99)
              fatal_error(ag->location, "dup cm");
            cov[c->component_decl.offset] = 1;
            break;
          }
        }
      }
    }
  }
}
static Type_Info *universal_composite_aggregate(Type_Info *at, Syntax_Node *ag)
{
  if (not at or not ag or at->k != TYPE_ARRAY or ag->k != N_AG)
    return at;
  if (at->low_bound != 0 or at->high_bound != -1)
    return at;
  int asz = ag->aggregate.items.count;
  Type_Info *nt = type_new(TYPE_ARRAY, N);
  nt->element_type = at->element_type;
  nt->index_type = at->index_type;
  nt->low_bound = 1;
  nt->high_bound = asz;
  return nt;
}
static void is_compile_valid(Type_Info *t, Syntax_Node *node)
{
  if (not t or not node)
    return;
  if (node->k == N_CL)
  {
    for (uint32_t i = 0; i < node->call.arguments.count; i++)
      resolve_expression(0, node->call.arguments.data[i], 0);
  }
  else if (node->k == N_AG and t->k == TYPE_ARRAY)
  {
    normalize_array_aggregate(0, type_canonical_concrete(t), node);
  }
  else if (node->k == N_AG and t->k == TYPE_RECORD)
  {
    normalize_record_aggregate(0, type_canonical_concrete(t), node);
  }
}
static bool has_return_statement(Node_Vector *statements)
{
  for (uint32_t i = 0; i < statements->count; i++)
    if (statements->data[i]->k != N_PG)
      return 1;
  return 0;
}
static void resolve_expression(Symbol_Manager *symbol_manager, Syntax_Node *node, Type_Info *tx)
{
  if (not node)
    return;
  switch (node->k)
  {
  case N_ID:
  {
    Type_Info *_tx = tx and tx->k == TYPE_DERIVED ? type_canonical_concrete(tx) : tx;
    if (_tx and _tx->k == TYPE_ENUMERATION)
    {
      for (uint32_t i = 0; i < tx->enum_values.count; i++)
      {
        Symbol *e = tx->enum_values.data[i];
        if (string_equal_ignore_case(e->name, node->string_value))
        {
          node->ty = tx;
          node->symbol = e;
          return;
        }
      }
    }
    Symbol *s = symbol_find(symbol_manager, node->string_value);
    if (s)
    {
      node->ty = s->type_info;
      node->symbol = s;
      if (s->k == 5)
      {
        Symbol *s0 = symbol_find_with_arity(symbol_manager, node->string_value, 0, tx);
        if (s0 and s0->type_info and s0->type_info->k == TYPE_STRING and s0->type_info->element_type)
        {
          node->ty = s0->type_info->element_type;
          node->symbol = s0;
        }
      }
      if (s->k == 2 and s->definition)
      {
        if (s->definition->k == N_INT)
        {
          node->k = N_INT;
          node->integer_value = s->definition->integer_value;
          node->ty = TY_UINT;
        }
        else if (s->definition->k == N_REAL)
        {
          node->k = N_REAL;
          node->float_value = s->definition->float_value;
          node->ty = TY_UFLT;
        }
      }
    }
    else
    {
      if (error_count < 99 and not string_equal_ignore_case(node->string_value, STRING_LITERAL("others")))
        fatal_error(node->location, "undef '%.*s'", (int) node->string_value.length, node->string_value.string);
      node->ty = TY_INT;
    }
  }
  break;
  case N_INT:
    node->ty = TY_UINT;
    break;
  case N_REAL:
    node->ty = TY_UFLT;
    break;
  case N_CHAR:
  {
    Symbol *s = symbol_character_literal(symbol_manager, node->integer_value, tx);
    if (s)
    {
      node->ty = s->type_info;
      node->symbol = s;
      node->k = N_ID;
      node->string_value = s->name;
    }
    else
      node->ty = TY_CHAR;
  }
  break;
  case N_STR:
    node->ty =
        tx and (tx->k == TYPE_ARRAY or type_canonical_concrete(tx)->k == TYPE_ARRAY) ? tx : TY_STR;
    break;
  case N_NULL:
    node->ty = tx and tx->k == TYPE_ACCESS ? tx : TY_INT;
    break;
  case N_BIN:
    resolve_expression(symbol_manager, node->binary_node.left, tx);
    resolve_expression(symbol_manager, node->binary_node.right, tx);
    if (node->binary_node.op == T_ATHN or node->binary_node.op == T_OREL)
    {
      node->ty = TY_BOOL;
      break;
    }
    if (node->binary_node.op == T_AND or node->binary_node.op == T_OR or node->binary_node.op == T_XOR)
    {
      node->binary_node.left = chk(symbol_manager, node->binary_node.left, node->location);
      node->binary_node.right = chk(symbol_manager, node->binary_node.right, node->location);
      Type_Info *lt = node->binary_node.left->ty ? type_canonical_concrete(node->binary_node.left->ty) : 0;
      node->ty = lt and lt->k == TYPE_ARRAY ? lt : TY_BOOL;
      break;
    }
    if (node->binary_node.op == T_IN)
    {
      node->binary_node.left = chk(symbol_manager, node->binary_node.left, node->location);
      node->binary_node.right = chk(symbol_manager, node->binary_node.right, node->location);
      node->ty = TY_BOOL;
      break;
    }
    if (node->binary_node.left->k == N_INT and node->binary_node.right->k == N_INT
        and (node->binary_node.op == T_PL or node->binary_node.op == T_MN or node->binary_node.op == T_ST or node->binary_node.op == T_SL or node->binary_node.op == T_MOD or node->binary_node.op == T_REM))
    {
      int64_t a = node->binary_node.left->integer_value, b = node->binary_node.right->integer_value, r = 0;
      if (node->binary_node.op == T_PL)
        r = a + b;
      else if (node->binary_node.op == T_MN)
        r = a - b;
      else if (node->binary_node.op == T_ST)
        r = a * b;
      else if (node->binary_node.op == T_SL and b != 0)
        r = a / b;
      else if ((node->binary_node.op == T_MOD or node->binary_node.op == T_REM) and b != 0)
        r = a % b;
      node->k = N_INT;
      node->integer_value = r;
      node->ty = TY_UINT;
    }
    else if (
        (node->binary_node.left->k == N_REAL or node->binary_node.right->k == N_REAL)
        and (node->binary_node.op == T_PL or node->binary_node.op == T_MN or node->binary_node.op == T_ST or node->binary_node.op == T_SL or node->binary_node.op == T_EX))
    {
      double a = node->binary_node.left->k == N_INT ? (double) node->binary_node.left->integer_value : node->binary_node.left->float_value,
             b = node->binary_node.right->k == N_INT ? (double) node->binary_node.right->integer_value : node->binary_node.right->float_value, r = 0;
      if (node->binary_node.op == T_PL)
        r = a + b;
      else if (node->binary_node.op == T_MN)
        r = a - b;
      else if (node->binary_node.op == T_ST)
        r = a * b;
      else if (node->binary_node.op == T_SL and b != 0)
        r = a / b;
      else if (node->binary_node.op == T_EX)
        r = pow(a, b);
      node->k = N_REAL;
      node->float_value = r;
      node->ty = TY_UFLT;
    }
    else
    {
      node->ty = type_canonical_concrete(node->binary_node.left->ty);
    }
    if (node->binary_node.op >= T_EQ and node->binary_node.op <= T_GE)
      node->ty = TY_BOOL;
    break;
  case N_UN:
    resolve_expression(symbol_manager, node->unary_node.operand, tx);
    if (node->unary_node.op == T_MN and node->unary_node.operand->k == N_INT)
    {
      node->k = N_INT;
      node->integer_value = -node->unary_node.operand->integer_value;
      node->ty = TY_UINT;
    }
    else if (node->unary_node.op == T_MN and node->unary_node.operand->k == N_REAL)
    {
      node->k = N_REAL;
      node->float_value = -node->unary_node.operand->float_value;
      node->ty = TY_UFLT;
    }
    else if (node->unary_node.op == T_PL and (node->unary_node.operand->k == N_INT or node->unary_node.operand->k == N_REAL))
    {
      node->k = node->unary_node.operand->k;
      if (node->k == N_INT)
      {
        node->integer_value = node->unary_node.operand->integer_value;
        node->ty = TY_UINT;
      }
      else
      {
        node->float_value = node->unary_node.operand->float_value;
        node->ty = TY_UFLT;
      }
    }
    else
    {
      node->ty = type_canonical_concrete(node->unary_node.operand->ty);
    }
    if (node->unary_node.op == T_NOT)
    {
      Type_Info *xt = node->unary_node.operand->ty ? type_canonical_concrete(node->unary_node.operand->ty) : 0;
      node->ty = xt and xt->k == TYPE_ARRAY ? xt : TY_BOOL;
    }
    break;
  case N_IX:
    resolve_expression(symbol_manager, node->index.prefix, 0);
    for (uint32_t i = 0; i < node->index.indices.count; i++)
    {
      resolve_expression(symbol_manager, node->index.indices.data[i], 0);
      node->index.indices.data[i] = chk(symbol_manager, node->index.indices.data[i], node->location);
    }
    if (node->index.prefix->ty and node->index.prefix->ty->k == TYPE_ARRAY)
      node->ty = type_canonical_concrete(node->index.prefix->ty->element_type);
    else
      node->ty = TY_INT;
    break;
  case N_SL:
    resolve_expression(symbol_manager, node->slice.prefix, 0);
    resolve_expression(symbol_manager, node->slice.low_bound, 0);
    resolve_expression(symbol_manager, node->slice.high_bound, 0);
    if (node->slice.prefix->ty and node->slice.prefix->ty->k == TYPE_ARRAY)
      node->ty = node->slice.prefix->ty;
    else
      node->ty = TY_INT;
    break;
  case N_SEL:
  {
    resolve_expression(symbol_manager, node->selected_component.prefix, 0);
    Syntax_Node *parser = node->selected_component.prefix;
    if (parser->k == N_ID)
    {
      Symbol *ps = symbol_find(symbol_manager, parser->string_value);
      if (ps and ps->k == 6 and ps->definition and ps->definition->k == N_PKS)
      {
        Syntax_Node *pk = ps->definition;
        for (uint32_t i = 0; i < pk->package_spec.declarations.count; i++)
        {
          Syntax_Node *d = pk->package_spec.declarations.data[i];
          if (d->symbol and string_equal_ignore_case(d->symbol->name, node->selected_component.selector))
          {
            node->ty = d->symbol->type_info ? d->symbol->type_info : TY_INT;
            node->symbol = d->symbol;
            if (d->symbol->k == 5 and d->symbol->type_info and d->symbol->type_info->k == TYPE_STRING and d->symbol->type_info->element_type)
            {
              node->ty = d->symbol->type_info->element_type;
            }
            if (d->symbol->k == 2 and d->symbol->definition)
            {
              Syntax_Node *df = d->symbol->definition;
              if (df->k == N_CHK)
                df = df->check.expression;
              if (df->k == N_INT)
              {
                node->k = N_INT;
                node->integer_value = df->integer_value;
                node->ty = TY_UINT;
              }
              else if (df->k == N_REAL)
              {
                node->k = N_REAL;
                node->float_value = df->float_value;
                node->ty = TY_UFLT;
              }
            }
            return;
          }
          if (d->k == N_ED)
          {
            for (uint32_t j = 0; j < d->exception_decl.identifiers.count; j++)
            {
              Syntax_Node *eid = d->exception_decl.identifiers.data[j];
              if (eid->symbol and string_equal_ignore_case(eid->symbol->name, node->selected_component.selector))
              {
                node->ty = eid->symbol->type_info ? eid->symbol->type_info : TY_INT;
                node->symbol = eid->symbol;
                return;
              }
            }
          }
          if (d->k == N_OD)
          {
            for (uint32_t j = 0; j < d->object_decl.identifiers.count; j++)
            {
              Syntax_Node *oid = d->object_decl.identifiers.data[j];
              if (oid->symbol and string_equal_ignore_case(oid->symbol->name, node->selected_component.selector))
              {
                node->ty = oid->symbol->type_info ? oid->symbol->type_info : TY_INT;
                node->symbol = oid->symbol;
                if (oid->symbol->k == 2 and oid->symbol->definition)
                {
                  Syntax_Node *df = oid->symbol->definition;
                  if (df->k == N_CHK)
                    df = df->check.expression;
                  if (df->k == N_INT)
                  {
                    node->k = N_INT;
                    node->integer_value = df->integer_value;
                    node->ty = TY_UINT;
                  }
                  else if (df->k == N_REAL)
                  {
                    node->k = N_REAL;
                    node->float_value = df->float_value;
                    node->ty = TY_UFLT;
                  }
                }
                return;
              }
            }
          }
        }
        for (uint32_t i = 0; i < pk->package_spec.declarations.count; i++)
        {
          Syntax_Node *d = pk->package_spec.declarations.data[i];
          if (d->k == N_TD and d->symbol and d->symbol->type_info)
          {
            Type_Info *et = d->symbol->type_info;
            if (et->k == TYPE_ENUMERATION)
            {
              for (uint32_t j = 0; j < et->enum_values.count; j++)
              {
                Symbol *e = et->enum_values.data[j];
                if (string_equal_ignore_case(e->name, node->selected_component.selector))
                {
                  node->ty = et;
                  node->symbol = e;
                  return;
                }
              }
            }
          }
          if (d->symbol and string_equal_ignore_case(d->symbol->name, node->selected_component.selector))
          {
            node->ty = d->symbol->type_info;
            node->symbol = d->symbol;
            return;
          }
        }
        for (int h = 0; h < 4096; h++)
          for (Symbol *s = symbol_manager->sy[h]; s; s = s->next)
            if (s->parent == ps and string_equal_ignore_case(s->name, node->selected_component.selector))
            {
              node->ty = s->type_info;
              node->symbol = s;
              if (s->k == 5 and s->type_info and s->type_info->k == TYPE_STRING and s->type_info->element_type)
              {
                node->ty = s->type_info->element_type;
              }
              if (s->k == 2 and s->definition)
              {
                Syntax_Node *df = s->definition;
                if (df->k == N_CHK)
                  df = df->check.expression;
                if (df->k == N_INT)
                {
                  node->k = N_INT;
                  node->integer_value = df->integer_value;
                  node->ty = TY_UINT;
                }
                else if (df->k == N_REAL)
                {
                  node->k = N_REAL;
                  node->float_value = df->float_value;
                  node->ty = TY_UFLT;
                }
              }
              return;
            }
        if (error_count < 99)
          fatal_error(node->location, "?'%.*s' in pkg", (int) node->selected_component.selector.length, node->selected_component.selector.string);
      }
    }
    if (parser->ty)
    {
      Type_Info *pt = type_canonical_concrete(parser->ty);
      if (pt->k == TYPE_RECORD)
      {
        for (uint32_t i = 0; i < pt->components.count; i++)
        {
          Syntax_Node *c = pt->components.data[i];
          if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, node->selected_component.selector))
          {
            node->ty = resolve_subtype(symbol_manager, c->component_decl.ty);
            return;
          }
        }
        for (uint32_t i = 0; i < pt->discriminants.count; i++)
        {
          Syntax_Node *d = pt->discriminants.data[i];
          if (d->k == N_DS and string_equal_ignore_case(d->parameter.name, node->selected_component.selector))
          {
            node->ty = resolve_subtype(symbol_manager, d->parameter.ty);
            return;
          }
        }
        for (uint32_t i = 0; i < pt->components.count; i++)
        {
          Syntax_Node *c = pt->components.data[i];
          if (c->k == N_VP)
          {
            for (uint32_t j = 0; j < c->variant_part.variants.count; j++)
            {
              Syntax_Node *v = c->variant_part.variants.data[j];
              for (uint32_t k = 0; k < v->variant.components.count; k++)
              {
                Syntax_Node *vc = v->variant.components.data[k];
                if (string_equal_ignore_case(vc->component_decl.name, node->selected_component.selector))
                {
                  node->ty = resolve_subtype(symbol_manager, vc->component_decl.ty);
                  return;
                }
              }
            }
          }
        }
        if (error_count < 99)
          fatal_error(node->location, "?fld '%.*s'", (int) node->selected_component.selector.length, node->selected_component.selector.string);
      }
    }
    node->ty = TY_INT;
  }
  break;
  case N_AT:
    resolve_expression(symbol_manager, node->attribute.prefix, 0);
    for (uint32_t i = 0; i < node->attribute.arguments.count; i++)
      resolve_expression(symbol_manager, node->attribute.arguments.data[i], 0);
    {
      Type_Info *pt = node->attribute.prefix ? node->attribute.prefix->ty : 0;
      Type_Info *ptc = pt ? type_canonical_concrete(pt) : 0;
      String_Slice a = node->attribute.attribute_name;
      if (string_equal_ignore_case(a, STRING_LITERAL("FIRST"))
          or string_equal_ignore_case(a, STRING_LITERAL("LAST")))
        node->ty = ptc and ptc->element_type                                     ? ptc->element_type
                : (ptc and (is_discrete(ptc) or is_real_type(ptc))) ? pt
                                                                    : TY_INT;
      else if (string_equal_ignore_case(a, STRING_LITERAL("ADDRESS")))
      {
        Syntax_Node *sel = ND(SEL, node->location);
        sel->selected_component.prefix = ND(ID, node->location);
        sel->selected_component.prefix->string_value = STRING_LITERAL("SYSTEM");
        sel->selected_component.selector = STRING_LITERAL("ADDRESS");
        node->ty = resolve_subtype(symbol_manager, sel);
      }
      else if (
          string_equal_ignore_case(a, STRING_LITERAL("LENGTH"))
          or string_equal_ignore_case(a, STRING_LITERAL("SIZE"))
          or string_equal_ignore_case(a, STRING_LITERAL("POS"))
          or string_equal_ignore_case(a, STRING_LITERAL("COUNT"))
          or string_equal_ignore_case(a, STRING_LITERAL("STORAGE_SIZE"))
          or string_equal_ignore_case(a, STRING_LITERAL("POSITION"))
          or string_equal_ignore_case(a, STRING_LITERAL("FIRST_BIT"))
          or string_equal_ignore_case(a, STRING_LITERAL("LAST_BIT"))
          or string_equal_ignore_case(a, STRING_LITERAL("AFT"))
          or string_equal_ignore_case(a, STRING_LITERAL("FORE"))
          or string_equal_ignore_case(a, STRING_LITERAL("WIDTH"))
          or string_equal_ignore_case(a, STRING_LITERAL("DIGITS"))
          or string_equal_ignore_case(a, STRING_LITERAL("MANTISSA"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_EMAX"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_EMIN"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_MANTISSA"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_RADIX"))
          or string_equal_ignore_case(a, STRING_LITERAL("SAFE_EMAX"))
          or string_equal_ignore_case(a, STRING_LITERAL("EMAX")))
        node->ty = TY_INT;
      else if (
          string_equal_ignore_case(a, STRING_LITERAL("DELTA"))
          or string_equal_ignore_case(a, STRING_LITERAL("EPSILON"))
          or string_equal_ignore_case(a, STRING_LITERAL("SMALL"))
          or string_equal_ignore_case(a, STRING_LITERAL("LARGE"))
          or string_equal_ignore_case(a, STRING_LITERAL("SAFE_LARGE"))
          or string_equal_ignore_case(a, STRING_LITERAL("SAFE_SMALL")))
        node->ty = TY_FLT;
      else if (
          string_equal_ignore_case(a, STRING_LITERAL("CALLABLE"))
          or string_equal_ignore_case(a, STRING_LITERAL("TERMINATED"))
          or string_equal_ignore_case(a, STRING_LITERAL("CONSTRAINED"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_OVERFLOWS"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_ROUNDS")))
        node->ty = TY_BOOL;
      else if (string_equal_ignore_case(a, STRING_LITERAL("ACCESS")))
        node->ty = type_new(TYPE_ACCESS, N);
      else if (string_equal_ignore_case(a, STRING_LITERAL("IMAGE")))
        node->ty = TY_STR;
      else if (
          string_equal_ignore_case(a, STRING_LITERAL("VALUE"))
          or string_equal_ignore_case(a, STRING_LITERAL("SUCC"))
          or string_equal_ignore_case(a, STRING_LITERAL("PRED"))
          or string_equal_ignore_case(a, STRING_LITERAL("VAL")))
        node->ty = pt ?: TY_INT;
      else if (string_equal_ignore_case(a, STRING_LITERAL("RANGE")))
        node->ty = TY_INT;
      else if (string_equal_ignore_case(a, STRING_LITERAL("BASE")))
        node->ty = pt and pt->base_type ? pt->base_type : pt;
      else
        node->ty = TY_INT;
      if (string_equal_ignore_case(a, STRING_LITERAL("POS")) and node->attribute.arguments.count > 0
          and node->attribute.arguments.data[0]->k == N_INT)
      {
        if (ptc and is_integer_type(ptc))
        {
          node->k = N_INT;
          node->integer_value = node->attribute.arguments.data[0]->integer_value;
          node->ty = TY_UINT;
        }
      }
      if (string_equal_ignore_case(a, STRING_LITERAL("VAL")) and node->attribute.arguments.count > 0
          and node->attribute.arguments.data[0]->k == N_INT)
      {
        int64_t pos = node->attribute.arguments.data[0]->integer_value;
        if (ptc == TY_CHAR and pos >= 0 and pos <= 127)
        {
          node->k = N_CHAR;
          node->integer_value = pos;
          node->ty = TY_CHAR;
        }
        else if (
            ptc and ptc->k == TYPE_ENUMERATION and pos >= ptc->low_bound and pos <= ptc->high_bound
            and (uint32_t) pos < ptc->enum_values.count)
        {
          Symbol *e = ptc->enum_values.data[pos];
          node->k = N_ID;
          node->string_value = e->name;
          node->ty = pt;
          node->symbol = e;
        }
      }
    }
    break;
  case N_QL:
  {
    Type_Info *qt = resolve_subtype(symbol_manager, node->qualified.name);
    resolve_expression(symbol_manager, node->qualified.aggregate, qt);
    node->ty = qt;
    if (node->qualified.aggregate->ty and qt)
    {
      is_compile_valid(qt, node->qualified.aggregate);
      node->qualified.aggregate->ty = qt;
    }
  }
  break;
  case N_CL:
  {
    resolve_expression(symbol_manager, node->call.function_name, 0);
    for (uint32_t i = 0; i < node->call.arguments.count; i++)
      resolve_expression(symbol_manager, node->call.arguments.data[i], 0);
    Type_Info *ft = node->call.function_name ? node->call.function_name->ty : 0;
    if (ft and ft->k == TYPE_ARRAY)
    {
      Syntax_Node *fn = node->call.function_name;
      Node_Vector ar = node->call.arguments;
      node->k = N_IX;
      node->index.prefix = fn;
      node->index.indices = ar;
      resolve_expression(symbol_manager, node, tx);
      break;
    }
    if (node->call.function_name->k == N_ID or node->call.function_name->k == N_STR)
    {
      String_Slice fnm = node->call.function_name->k == N_ID ? node->call.function_name->string_value : node->call.function_name->string_value;
      Symbol *s = node->call.function_name->symbol;
      if (not s)
        s = symbol_find_with_arity(symbol_manager, fnm, node->call.arguments.count, tx);
      if (s)
      {
        node->call.function_name->symbol = s;
        if (s->type_info and s->type_info->k == TYPE_STRING and s->type_info->element_type)
        {
          node->ty = s->type_info->element_type;
          node->symbol = s;
        }
        else if (s->k == 1)
        {
          Syntax_Node *cv = ND(CVT, node->location);
          cv->conversion.ty = node->call.function_name;
          cv->conversion.expression = node->call.arguments.count > 0 ? node->call.arguments.data[0] : 0;
          node->k = N_CVT;
          node->conversion = cv->conversion;
          node->ty = s->type_info ? s->type_info : TY_INT;
        }
        else
          node->ty = TY_INT;
      }
      else
        node->ty = TY_INT;
    }
    else
      node->ty = TY_INT;
  }
  break;
  case N_AG:
    for (uint32_t i = 0; i < node->aggregate.items.count; i++)
      resolve_expression(symbol_manager, node->aggregate.items.data[i], tx);
    node->ty = tx ?: TY_INT;
    is_compile_valid(tx, node);
    break;
  case N_ALC:
    node->ty = type_new(TYPE_ACCESS, N);
    node->ty->element_type = resolve_subtype(symbol_manager, node->allocator.subtype);
    if (node->allocator.initializer)
    {
      Type_Info *et = node->ty->element_type ? type_canonical_concrete(node->ty->element_type) : 0;
      if (et and et->k == TYPE_RECORD and et->discriminants.count > 0)
      {
        for (uint32_t i = 0; i < et->discriminants.count; i++)
        {
          Syntax_Node *d = et->discriminants.data[i];
          if (d->k == N_DS and d->parameter.default_value)
          {
            resolve_expression(symbol_manager, d->parameter.default_value, resolve_subtype(symbol_manager, d->parameter.ty));
          }
        }
      }
      resolve_expression(symbol_manager, node->allocator.initializer, node->ty->element_type);
      if (tx and tx->k == TYPE_ACCESS and tx->element_type)
      {
        Type_Info *ct = type_canonical_concrete(tx->element_type);
        if (ct and ct->k == TYPE_RECORD and ct->discriminants.count > 0)
        {
          bool hcd = 0;
          for (uint32_t i = 0; i < ct->discriminants.count; i++)
            if (ct->discriminants.data[i]->k == N_DS and ct->discriminants.data[i]->parameter.default_value)
              hcd = 1;
          if (hcd and et and et->discriminants.count > 0)
          {
            for (uint32_t i = 0; i < ct->discriminants.count and i < et->discriminants.count; i++)
            {
              Syntax_Node *cd = ct->discriminants.data[i], *ed = et->discriminants.data[i];
              if (cd->k == N_DS and cd->parameter.default_value and ed->k == N_DS)
              {
                bool mtch = cd->parameter.default_value->k == N_INT and ed->parameter.default_value and ed->parameter.default_value->k == N_INT
                            and cd->parameter.default_value->integer_value == ed->parameter.default_value->integer_value;
                if (not mtch)
                {
                  node->allocator.initializer = chk(symbol_manager, node->allocator.initializer, node->location);
                  break;
                }
              }
            }
          }
        }
      }
    }
    break;
  case N_RN:
    resolve_expression(symbol_manager, node->range.low_bound, tx);
    resolve_expression(symbol_manager, node->range.high_bound, tx);
    node->range.low_bound = chk(symbol_manager, node->range.low_bound, node->location);
    node->range.high_bound = chk(symbol_manager, node->range.high_bound, node->location);
    node->ty = type_canonical_concrete(node->range.low_bound->ty);
    break;
  case N_ASC:
    if (node->association.value)
    {
      Type_Info *vt = tx and tx->k == TYPE_ARRAY ? tx->element_type : tx;
      resolve_expression(symbol_manager, node->association.value, vt);
    }
    break;
  case N_DRF:
    resolve_expression(symbol_manager, node->dereference.expression, 0);
    {
      Type_Info *dty = node->dereference.expression->ty ? type_canonical_concrete(node->dereference.expression->ty) : 0;
      if (dty and dty->k == TYPE_ACCESS)
        node->ty = dty->element_type;
      else
      {
        if (error_count < 99 and node->dereference.expression->ty)
          fatal_error(node->location, ".all non-ac");
        node->ty = TY_INT;
      }
    }
    break;
  case N_CVT:
    resolve_expression(symbol_manager, node->conversion.expression, 0);
    node->ty = resolve_subtype(symbol_manager, node->conversion.ty);
    break;
  case N_CHK:
    resolve_expression(symbol_manager, node->check.expression, tx);
    node->ty = node->check.expression->ty;
    break;
  default:
    break;
  }
}
static void resolve_statement_sequence(Symbol_Manager *symbol_manager, Syntax_Node *node)
{
  if (not node)
    return;
  switch (node->k)
  {
  case N_AS:
    resolve_expression(symbol_manager, node->assignment.target, 0);
    resolve_expression(symbol_manager, node->assignment.value, node->assignment.target->ty);
    {
      // Check compatibility before canonicalizing to preserve base_type relationships
      Type_Info *tgt_orig = node->assignment.target->ty;
      Type_Info *vlt_orig = node->assignment.value->ty;
      Type_Info *tgt = tgt_orig ? type_canonical_concrete(tgt_orig) : 0;
      Type_Info *vlt = vlt_orig ? type_canonical_concrete(vlt_orig) : 0;
      if (error_count < 99 and tgt and vlt and not type_covers(tgt, vlt))
      {
        Type_Info *tgb = semantic_base(tgt);
        Type_Info *vlb = semantic_base(vlt);
        // Allow assignments involving arrays (bounds will be checked at runtime)
        bool has_array = (tgt->k == TYPE_ARRAY or vlt->k == TYPE_ARRAY);
        if (not type_covers(tgb, vlb) and not has_array
            and not(tgt->k == TYPE_BOOLEAN and is_discrete(vlt))
            and not(is_discrete(tgt) and is_discrete(vlt)))
          fatal_error(node->location, "typ mis");
      }
    }
    break;
    if (node->assignment.target->ty)
      node->assignment.value->ty = node->assignment.target->ty;
    node->assignment.value = chk(symbol_manager, node->assignment.value, node->location);
    break;
  case N_IF:
    resolve_expression(symbol_manager, node->if_stmt.condition, TY_BOOL);
    if (node->if_stmt.then_statements.count > 0 and not has_return_statement(&node->if_stmt.then_statements))
      fatal_error(node->location, "seq needs stmt");
    for (uint32_t i = 0; i < node->if_stmt.then_statements.count; i++)
      resolve_statement_sequence(symbol_manager, node->if_stmt.then_statements.data[i]);
    for (uint32_t i = 0; i < node->if_stmt.elsif_statements.count; i++)
    {
      Syntax_Node *e = node->if_stmt.elsif_statements.data[i];
      resolve_expression(symbol_manager, e->if_stmt.condition, TY_BOOL);
      if (e->if_stmt.then_statements.count > 0 and not has_return_statement(&e->if_stmt.then_statements))
        fatal_error(e->location, "seq needs stmt");
      for (uint32_t j = 0; j < e->if_stmt.then_statements.count; j++)
        resolve_statement_sequence(symbol_manager, e->if_stmt.then_statements.data[j]);
    }
    if (node->if_stmt.else_statements.count > 0 and not has_return_statement(&node->if_stmt.else_statements))
      fatal_error(node->location, "seq needs stmt");
    for (uint32_t i = 0; i < node->if_stmt.else_statements.count; i++)
      resolve_statement_sequence(symbol_manager, node->if_stmt.else_statements.data[i]);
    break;
  case N_CS:
    resolve_expression(symbol_manager, node->case_stmt.expression, 0);
    for (uint32_t i = 0; i < node->case_stmt.alternatives.count; i++)
    {
      Syntax_Node *a = node->case_stmt.alternatives.data[i];
      for (uint32_t j = 0; j < a->choices.items.count; j++)
        resolve_expression(symbol_manager, a->choices.items.data[j], node->case_stmt.expression->ty);
      if (a->exception_handler.statements.count > 0 and not has_return_statement(&a->exception_handler.statements))
        fatal_error(a->location, "seq needs stmt");
      for (uint32_t j = 0; j < a->exception_handler.statements.count; j++)
        resolve_statement_sequence(symbol_manager, a->exception_handler.statements.data[j]);
    }
    break;
  case N_LP:
    if (node->loop_stmt.label.string)
    {
      Symbol *lbs = symbol_add_overload(symbol_manager, symbol_new(node->loop_stmt.label, 10, 0, node));
      (void) lbs;
    }
    if (node->loop_stmt.iterator)
    {
      if (node->loop_stmt.iterator->k == N_BIN and node->loop_stmt.iterator->binary_node.op == T_IN)
      {
        Syntax_Node *v = node->loop_stmt.iterator->binary_node.left;
        if (v->k == N_ID)
        {
          Type_Info *rt = node->loop_stmt.iterator->binary_node.right->ty;
          Symbol *lvs = symbol_new(v->string_value, 0, rt ?: TY_INT, 0);
          symbol_add_overload(symbol_manager, lvs);
          lvs->level = -1;
          v->symbol = lvs;
        }
      }
      resolve_expression(symbol_manager, node->loop_stmt.iterator, TY_BOOL);
    }
    if (node->loop_stmt.statements.count > 0 and not has_return_statement(&node->loop_stmt.statements))
      fatal_error(node->location, "seq needs stmt");
    for (uint32_t i = 0; i < node->loop_stmt.statements.count; i++)
      resolve_statement_sequence(symbol_manager, node->loop_stmt.statements.data[i]);
    break;
  case N_BL:
    if (node->block.label.string)
    {
      Symbol *lbs = symbol_add_overload(symbol_manager, symbol_new(node->block.label, 10, 0, node));
      (void) lbs;
    }
    symbol_compare_parameter(symbol_manager);
    for (uint32_t i = 0; i < node->block.declarations.count; i++)
      resolve_declaration(symbol_manager, node->block.declarations.data[i]);
    for (uint32_t i = 0; i < node->block.statements.count; i++)
      resolve_statement_sequence(symbol_manager, node->block.statements.data[i]);
    if (node->block.handlers.count > 0)
    {
      for (uint32_t i = 0; i < node->block.handlers.count; i++)
      {
        Syntax_Node *h = node->block.handlers.data[i];
        for (uint32_t j = 0; j < h->exception_handler.exception_choices.count; j++)
        {
          Syntax_Node *e = h->exception_handler.exception_choices.data[j];
          if (e->k == N_ID and not string_equal_ignore_case(e->string_value, STRING_LITERAL("others")))
            slv(&symbol_manager->eh, e->string_value);
        }
        for (uint32_t j = 0; j < h->exception_handler.statements.count; j++)
          resolve_statement_sequence(symbol_manager, h->exception_handler.statements.data[j]);
      }
    }
    symbol_compare_overload(symbol_manager);
    break;
  case N_RT:
    if (node->return_stmt.value)
      resolve_expression(symbol_manager, node->return_stmt.value, 0);
    break;
  case N_EX:
    if (node->exit_stmt.condition)
      resolve_expression(symbol_manager, node->exit_stmt.condition, TY_BOOL);
    break;
  case N_RS:
    if (node->raise_stmt.exception_choice and node->raise_stmt.exception_choice->k == N_ID)
      slv(&symbol_manager->eh, node->raise_stmt.exception_choice->string_value);
    else
      slv(&symbol_manager->eh, STRING_LITERAL("PROGRAM_ERROR"));
    if (node->raise_stmt.exception_choice)
      resolve_expression(symbol_manager, node->raise_stmt.exception_choice, 0);
    break;
  case N_CLT:
    resolve_expression(symbol_manager, node->code_stmt.name, 0);
    for (uint32_t i = 0; i < node->code_stmt.arguments.count; i++)
      resolve_expression(symbol_manager, node->code_stmt.arguments.data[i], 0);
    break;
  case N_ACC:
    symbol_compare_parameter(symbol_manager);
    {
      Symbol *ens = symbol_add_overload(symbol_manager, symbol_new(node->accept_stmt.name, 9, 0, node));
      (void) ens;
      for (uint32_t i = 0; i < node->accept_stmt.parameters.count; i++)
      {
        Syntax_Node *parser = node->accept_stmt.parameters.data[i];
        Type_Info *pt = resolve_subtype(symbol_manager, parser->parameter.ty);
        Symbol *ps = symbol_add_overload(symbol_manager, symbol_new(parser->parameter.name, 0, pt, parser));
        parser->symbol = ps;
      }
      if (node->accept_stmt.statements.count > 0 and not has_return_statement(&node->accept_stmt.statements))
        fatal_error(node->location, "seq needs stmt");
      for (uint32_t i = 0; i < node->accept_stmt.statements.count; i++)
        resolve_statement_sequence(symbol_manager, node->accept_stmt.statements.data[i]);
    }
    symbol_compare_overload(symbol_manager);
    break;
  case N_SA:
    if (node->abort_stmt.guard)
      resolve_expression(symbol_manager, node->abort_stmt.guard, 0);
    for (uint32_t i = 0; i < node->abort_stmt.statements.count; i++)
    {
      Syntax_Node *s = node->abort_stmt.statements.data[i];
      if (s->k == N_ACC)
      {
        for (uint32_t j = 0; j < s->accept_stmt.parameters.count; j++)
          resolve_expression(symbol_manager, s->accept_stmt.parameters.data[j], 0);
        if (s->accept_stmt.statements.count > 0 and not has_return_statement(&s->accept_stmt.statements))
          fatal_error(s->location, "seq needs stmt");
        for (uint32_t j = 0; j < s->accept_stmt.statements.count; j++)
          resolve_statement_sequence(symbol_manager, s->accept_stmt.statements.data[j]);
      }
      else if (s->k == N_DL)
        resolve_expression(symbol_manager, s->exit_stmt.condition, 0);
    }
    break;
  case N_DL:
    resolve_expression(symbol_manager, node->exit_stmt.condition, 0);
    break;
  case N_AB:
    if (node->raise_stmt.exception_choice and node->raise_stmt.exception_choice->k == N_ID)
      slv(&symbol_manager->eh, node->raise_stmt.exception_choice->string_value);
    else
      slv(&symbol_manager->eh, STRING_LITERAL("TASKING_ERROR"));
    if (node->raise_stmt.exception_choice)
      resolve_expression(symbol_manager, node->raise_stmt.exception_choice, 0);
    break;
  case N_US:
    if (node->use_clause.nm->k == N_ID)
    {
      Symbol *s = symbol_find(symbol_manager, node->use_clause.nm->string_value);
      if (s)
        symbol_find_use(symbol_manager, s, node->use_clause.nm->string_value);
    }
    break;
  default:
    break;
  }
}
static void runtime_register_compare(Symbol_Manager *symbol_manager, Representation_Clause *r)
{
  if (not r)
    return;
  switch (r->k)
  {
  case 1:
  {
    Symbol *ts = symbol_find(symbol_manager, r->er.name);
    if (ts and ts->type_info)
    {
      Type_Info *t = type_canonical_concrete(ts->type_info);
      for (uint32_t i = 0; i < r->rr.components.count; i++)
      {
        Syntax_Node *e = r->rr.components.data[i];
        for (uint32_t j = 0; j < t->enum_values.count; j++)
        {
          Symbol *ev = t->enum_values.data[j];
          if (string_equal_ignore_case(ev->name, e->string_value))
          {
            ev->value = e->integer_value;
            break;
          }
        }
      }
    }
  }
  break;
  case 2:
  {
    Symbol *s = symbol_find(symbol_manager, r->ad.name);
    if (s and s->type_info)
    {
      Type_Info *t = type_canonical_concrete(s->type_info);
      t->address = r->ad.address;
    }
  }
  break;
  case 3:
  {
    Symbol *s = symbol_find(symbol_manager, r->rr.name);
    if (s and s->type_info)
    {
      Type_Info *t = type_canonical_concrete(s->type_info);
      uint32_t bt = 0;
      for (uint32_t i = 0; i < r->rr.components.count; i++)
      {
        Syntax_Node *cp = r->rr.components.data[i];
        for (uint32_t j = 0; j < t->components.count; j++)
        {
          Syntax_Node *c = t->components.data[j];
          if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, cp->component_decl.name))
          {
            c->component_decl.offset = cp->component_decl.offset;
            c->component_decl.bit_offset = cp->component_decl.bit_offset;
            bt += cp->component_decl.bit_offset;
            break;
          }
        }
      }
      t->size = (bt + 7) / 8;
      t->is_packed = true;
    }
  }
  break;
  case 4:
  {
    Symbol *s = r->ad.name.length > 0 ? symbol_find(symbol_manager, r->ad.name) : 0;
    if (s and s->type_info)
    {
      Type_Info *t = type_canonical_concrete(s->type_info);
      t->suppressed_checks |= r->ad.address;
    }
  }
  break;
  case 5:
  {
    Symbol *s = symbol_find(symbol_manager, r->er.name);
    if (s and s->type_info)
    {
      Type_Info *t = type_canonical_concrete(s->type_info);
      t->is_packed = true;
    }
  }
  break;
  case 6:
  {
    Symbol *s = symbol_find(symbol_manager, r->er.name);
    if (s)
      s->is_inline = true;
  }
  break;
  case 7:
  {
    Symbol *s = symbol_find(symbol_manager, r->er.name);
    if (s and s->type_info)
    {
      Type_Info *t = type_canonical_concrete(s->type_info);
      t->is_controlled = true;
    }
  }
  break;
  case 8:
  {
    Symbol *s = symbol_find(symbol_manager, r->im.name);
    if (s)
    {
      s->is_external = true;
      s->external_name = r->im.external_name.length > 0 ? string_duplicate(r->im.external_name) : string_duplicate(r->im.name);
      s->external_language = string_duplicate(r->im.language);
    }
  }
  break;
  }
}
static void is_higher_order_parameter(Type_Info *dt, Type_Info *pt)
{
  if (not dt or not pt)
    return;
  for (uint32_t i = 0; i < pt->operations.count; i++)
  {
    Syntax_Node *op = pt->operations.data[i];
    if (op->k == N_FB or op->k == N_PB)
    {
      Syntax_Node *nop = node_new(op->k, op->location);
      nop->body = op->body;
      Syntax_Node *nsp = node_new(N_FS, op->location);
      nsp->subprogram = op->body.subprogram_spec->subprogram;
      nsp->subprogram.name = string_duplicate(op->body.subprogram_spec->subprogram.name);
      nsp->subprogram.parameters = op->body.subprogram_spec->subprogram.parameters;
      if (op->k == N_FB)
        nsp->subprogram.return_type = op->body.subprogram_spec->subprogram.return_type;
      nop->body.subprogram_spec = nsp;
      nop->body.elaboration_level = -1;
      nv(&dt->operations, nop);
    }
  }
}
static int node_clone_depth = 0;
#define MAX_NODE_CLONE_DEPTH 1000
static bool match_formal_parameter(Syntax_Node *f, String_Slice nm);
static Syntax_Node *node_clone_substitute(Syntax_Node *node, Node_Vector *fp, Node_Vector *ap);
static const char *lookup_path(Symbol_Manager *symbol_manager, String_Slice nm);
static Syntax_Node *pks2(Symbol_Manager *symbol_manager, String_Slice nm, const char *src);
static void parse_package_specification(Symbol_Manager *symbol_manager, String_Slice nm, const char *src);
static void resolve_array_parameter(Symbol_Manager *symbol_manager, Node_Vector *fp, Node_Vector *ap)
{
  if (not fp or not ap)
    return;
  for (uint32_t i = 0; i < fp->count and i < ap->count; i++)
  {
    Syntax_Node *f = fp->data[i];
    Syntax_Node *a = ap->data[i];
    if (f->k == N_GSP and a->k == N_STR)
    {
      int pc = f->subprogram.parameters.count;
      Type_Info *rt = 0;
      if (f->subprogram.return_type)
      {
        if (f->subprogram.return_type->k == N_ID)
        {
          String_Slice tn = f->subprogram.return_type->string_value;
          for (uint32_t j = 0; j < fp->count; j++)
          {
            Syntax_Node *tf = fp->data[j];
            if (tf->k == N_GTP and string_equal_ignore_case(tf->type_decl.name, tn) and j < ap->count)
            {
              Syntax_Node *ta = ap->data[j];
              if (ta->k == N_ID)
              {
                Symbol *ts = symbol_find(symbol_manager, ta->string_value);
                if (ts and ts->type_info)
                  rt = ts->type_info;
              }
              break;
            }
          }
        }
      }
      Symbol *s = symbol_find_with_arity(symbol_manager, a->string_value, pc, rt);
      if (s)
      {
        a->k = N_ID;
        a->symbol = s;
      }
    }
  }
}
static void normalize_compile_symbol_vector(Node_Vector *d, Node_Vector *s, Node_Vector *fp, Node_Vector *ap)
{
  if (not s)
  {
    d->count = 0;
    d->capacity = 0;
    d->data = 0;
    return;
  }
  Syntax_Node **orig_d = d->data;
  (void) orig_d;
  uint32_t sn = s->count;
  if (sn == 0 or not s->data)
  {
    d->count = 0;
    d->capacity = 0;
    d->data = 0;
    return;
  }
  if (sn > 100000)
  {
    d->count = 0;
    d->capacity = 0;
    d->data = 0;
    return;
  }
  Syntax_Node **sd = malloc(sn * sizeof(Syntax_Node *));
  if (not sd)
  {
    d->count = 0;
    d->capacity = 0;
    d->data = 0;
    return;
  }
  memcpy(sd, s->data, sn * sizeof(Syntax_Node *));
  d->count = 0;
  d->capacity = 0;
  d->data = 0;
  for (uint32_t i = 0; i < sn; i++)
  {
    Syntax_Node *node = sd[i];
    if (node)
      nv(d, node_clone_substitute(node, fp, ap));
  }
  free(sd);
}
static Syntax_Node *node_clone_substitute(Syntax_Node *n, Node_Vector *fp, Node_Vector *ap)
{
  if (not n)
    return 0;
  if (node_clone_depth++ > MAX_NODE_CLONE_DEPTH)
  {
    node_clone_depth--;
    return n;
  }
  if (fp and n->k == N_ID)
  {
    for (uint32_t i = 0; i < fp->count; i++)
      if (match_formal_parameter(fp->data[i], n->string_value))
      {
        if (i < ap->count)
        {
          Syntax_Node *a = ap->data[i];
          Syntax_Node *r = a->k == N_ASC and a->association.value ? node_clone_substitute(a->association.value, 0, 0) : node_clone_substitute(a, 0, 0);
          node_clone_depth--;
          return r;
        }
        Syntax_Node *r = node_clone_substitute(n, 0, 0);
        node_clone_depth--;
        return r;
      }
  }
  if (fp and n->k == N_STR)
  {
    for (uint32_t i = 0; i < fp->count; i++)
      if (match_formal_parameter(fp->data[i], n->string_value))
      {
        if (i < ap->count)
        {
          Syntax_Node *a = ap->data[i];
          Syntax_Node *r = a->k == N_ASC and a->association.value ? node_clone_substitute(a->association.value, 0, 0) : node_clone_substitute(a, 0, 0);
          node_clone_depth--;
          return r;
        }
        Syntax_Node *r = node_clone_substitute(n, 0, 0);
        node_clone_depth--;
        return r;
      }
  }
  Syntax_Node *c = node_new(n->k, n->location);
  c->ty = 0;
  c->symbol = (n->k == N_ID and n->symbol) ? n->symbol : 0;
  switch (n->k)
  {
  case N_ID:
    c->string_value = n->string_value.string ? string_duplicate(n->string_value) : n->string_value;
    break;
  case N_INT:
    c->integer_value = n->integer_value;
    break;
  case N_REAL:
    c->float_value = n->float_value;
    break;
  case N_CHAR:
    c->integer_value = n->integer_value;
    break;
  case N_STR:
    c->string_value = n->string_value.string ? string_duplicate(n->string_value) : n->string_value;
    break;
  case N_NULL:
    break;
  case N_BIN:
    c->binary_node.op = n->binary_node.op;
    c->binary_node.left = node_clone_substitute(n->binary_node.left, fp, ap);
    c->binary_node.right = node_clone_substitute(n->binary_node.right, fp, ap);
    break;
  case N_UN:
    c->unary_node.op = n->unary_node.op;
    c->unary_node.operand = node_clone_substitute(n->unary_node.operand, fp, ap);
    break;
  case N_AT:
    c->attribute.prefix = node_clone_substitute(n->attribute.prefix, fp, ap);
    c->attribute.attribute_name = n->attribute.attribute_name;
    normalize_compile_symbol_vector(&c->attribute.arguments, &n->attribute.arguments, fp, ap);
    break;
  case N_QL:
    c->qualified.name = node_clone_substitute(n->qualified.name, fp, ap);
    c->qualified.aggregate = node_clone_substitute(n->qualified.aggregate, fp, ap);
    break;
  case N_CL:
    c->call.function_name = node_clone_substitute(n->call.function_name, fp, ap);
    normalize_compile_symbol_vector(&c->call.arguments, &n->call.arguments, fp, ap);
    break;
  case N_IX:
    c->index.prefix = node_clone_substitute(n->index.prefix, fp, ap);
    normalize_compile_symbol_vector(&c->index.indices, &n->index.indices, fp, ap);
    break;
  case N_SL:
    c->slice.prefix = node_clone_substitute(n->slice.prefix, fp, ap);
    c->slice.low_bound = node_clone_substitute(n->slice.low_bound, fp, ap);
    c->slice.high_bound = node_clone_substitute(n->slice.high_bound, fp, ap);
    break;
  case N_SEL:
    c->selected_component.prefix = node_clone_substitute(n->selected_component.prefix, fp, ap);
    c->selected_component.selector = n->selected_component.selector;
    break;
  case N_ALC:
    c->allocator.subtype = node_clone_substitute(n->allocator.subtype, fp, ap);
    c->allocator.initializer = node_clone_substitute(n->allocator.initializer, fp, ap);
    break;
  case N_RN:
    c->range.low_bound = node_clone_substitute(n->range.low_bound, fp, ap);
    c->range.high_bound = node_clone_substitute(n->range.high_bound, fp, ap);
    break;
  case N_CN:
    c->constraint.range_spec = node_clone_substitute(n->constraint.range_spec, fp, ap);
    normalize_compile_symbol_vector(&c->constraint.constraints, &n->constraint.constraints, fp, ap);
    break;
  case N_CM:
    c->component_decl.name = n->component_decl.name;
    c->component_decl.ty = node_clone_substitute(n->component_decl.ty, fp, ap);
    c->component_decl.in = node_clone_substitute(n->component_decl.in, fp, ap);
    c->component_decl.is_aliased = n->component_decl.is_aliased;
    c->component_decl.offset = n->component_decl.offset;
    c->component_decl.bit_offset = n->component_decl.bit_offset;
    c->component_decl.discriminant_constraint = node_clone_substitute(n->component_decl.discriminant_constraint, fp, ap);
    c->component_decl.discriminant_spec = node_clone_substitute(n->component_decl.discriminant_spec, fp, ap);
    break;
  case N_VR:
    normalize_compile_symbol_vector(&c->variant.choices, &n->variant.choices, fp, ap);
    normalize_compile_symbol_vector(&c->variant.components, &n->variant.components, fp, ap);
    break;
  case N_VP:
    c->variant_part.discriminant_spec = node_clone_substitute(n->variant_part.discriminant_spec, fp, ap);
    normalize_compile_symbol_vector(&c->variant_part.variants, &n->variant_part.variants, fp, ap);
    c->variant_part.size = n->variant_part.size;
    break;
  case N_DS:
  case N_PM:
    c->parameter.name = n->parameter.name;
    c->parameter.ty = node_clone_substitute(n->parameter.ty, fp, ap);
    c->parameter.default_value = node_clone_substitute(n->parameter.default_value, fp, ap);
    c->parameter.mode = n->parameter.mode;
    break;
  case N_PS:
  case N_FS:
  case N_GSP:
    c->subprogram.name = n->subprogram.name;
    normalize_compile_symbol_vector(&c->subprogram.parameters, &n->subprogram.parameters, fp, ap);
    c->subprogram.return_type = node_clone_substitute(n->subprogram.return_type, fp, ap);
    c->subprogram.operator_symbol = n->subprogram.operator_symbol;
    break;
  case N_PD:
  case N_PB:
  case N_FD:
  case N_FB:
    c->body.subprogram_spec = node_clone_substitute(n->body.subprogram_spec, fp, ap);
    normalize_compile_symbol_vector(&c->body.declarations, &n->body.declarations, fp, ap);
    normalize_compile_symbol_vector(&c->body.statements, &n->body.statements, fp, ap);
    normalize_compile_symbol_vector(&c->body.handlers, &n->body.handlers, fp, ap);
    c->body.elaboration_level = n->body.elaboration_level;
    c->body.parent = 0;
    normalize_compile_symbol_vector(&c->body.locks, &n->body.locks, fp, ap);
    break;
  case N_PKS:
    c->package_spec.name = n->package_spec.name;
    normalize_compile_symbol_vector(&c->package_spec.declarations, &n->package_spec.declarations, fp, ap);
    normalize_compile_symbol_vector(&c->package_spec.private_declarations, &n->package_spec.private_declarations, fp, ap);
    c->package_spec.elaboration_level = n->package_spec.elaboration_level;
    break;
  case N_PKB:
    c->package_body.name = n->package_body.name;
    normalize_compile_symbol_vector(&c->package_body.declarations, &n->package_body.declarations, fp, ap);
    normalize_compile_symbol_vector(&c->package_body.statements, &n->package_body.statements, fp, ap);
    normalize_compile_symbol_vector(&c->package_body.handlers, &n->package_body.handlers, fp, ap);
    c->package_body.elaboration_level = n->package_body.elaboration_level;
    break;
  case N_OD:
  case N_GVL:
    normalize_compile_symbol_vector(&c->object_decl.identifiers, &n->object_decl.identifiers, fp, ap);
    c->object_decl.ty = node_clone_substitute(n->object_decl.ty, fp, ap);
    c->object_decl.in = node_clone_substitute(n->object_decl.in, fp, ap);
    c->object_decl.is_constant = n->object_decl.is_constant;
    break;
  case N_TD:
  case N_GTP:
    c->type_decl.name = n->type_decl.name;
    c->type_decl.definition = node_clone_substitute(n->type_decl.definition, fp, ap);
    c->type_decl.discriminant = node_clone_substitute(n->type_decl.discriminant, fp, ap);
    c->type_decl.is_new = n->type_decl.is_new;
    c->type_decl.is_derived = n->type_decl.is_derived;
    c->type_decl.parent_type = node_clone_substitute(n->type_decl.parent_type, fp, ap);
    normalize_compile_symbol_vector(&c->type_decl.discriminants, &n->type_decl.discriminants, fp, ap);
    break;
  case N_SD:
    c->subtype_decl.name = n->subtype_decl.name;
    c->subtype_decl.index_constraint = node_clone_substitute(n->subtype_decl.index_constraint, fp, ap);
    c->subtype_decl.constraint = node_clone_substitute(n->subtype_decl.constraint, fp, ap);
    c->subtype_decl.range_constraint = node_clone_substitute(n->subtype_decl.range_constraint, fp, ap);
    break;
  case N_ED:
    normalize_compile_symbol_vector(&c->exception_decl.identifiers, &n->exception_decl.identifiers, fp, ap);
    c->exception_decl.renamed_entity = node_clone_substitute(n->exception_decl.renamed_entity, fp, ap);
    break;
  case N_RE:
    c->renaming.name = n->renaming.name;
    c->renaming.renamed_entity = node_clone_substitute(n->renaming.renamed_entity, fp, ap);
    break;
  case N_AS:
    c->assignment.target = node_clone_substitute(n->assignment.target, fp, ap);
    c->assignment.value = node_clone_substitute(n->assignment.value, fp, ap);
    break;
  case N_IF:
  case N_EL:
    c->if_stmt.condition = node_clone_substitute(n->if_stmt.condition, fp, ap);
    normalize_compile_symbol_vector(&c->if_stmt.then_statements, &n->if_stmt.then_statements, fp, ap);
    normalize_compile_symbol_vector(&c->if_stmt.elsif_statements, &n->if_stmt.elsif_statements, fp, ap);
    normalize_compile_symbol_vector(&c->if_stmt.else_statements, &n->if_stmt.else_statements, fp, ap);
    break;
  case N_CS:
    c->case_stmt.expression = node_clone_substitute(n->case_stmt.expression, fp, ap);
    normalize_compile_symbol_vector(&c->case_stmt.alternatives, &n->case_stmt.alternatives, fp, ap);
    break;
  case N_LP:
    c->loop_stmt.label = n->loop_stmt.label;
    c->loop_stmt.iterator = node_clone_substitute(n->loop_stmt.iterator, fp, ap);
    c->loop_stmt.is_reverse = n->loop_stmt.is_reverse;
    normalize_compile_symbol_vector(&c->loop_stmt.statements, &n->loop_stmt.statements, fp, ap);
    normalize_compile_symbol_vector(&c->loop_stmt.locks, &n->loop_stmt.locks, fp, ap);
    break;
  case N_BL:
    c->block.label = n->block.label;
    normalize_compile_symbol_vector(&c->block.declarations, &n->block.declarations, fp, ap);
    normalize_compile_symbol_vector(&c->block.statements, &n->block.statements, fp, ap);
    normalize_compile_symbol_vector(&c->block.handlers, &n->block.handlers, fp, ap);
    break;
  case N_EX:
    c->exit_stmt.label = n->exit_stmt.label;
    c->exit_stmt.condition = node_clone_substitute(n->exit_stmt.condition, fp, ap);
    break;
  case N_RT:
    c->return_stmt.value = node_clone_substitute(n->return_stmt.value, fp, ap);
    break;
  case N_GT:
    c->goto_stmt.label = n->goto_stmt.label;
    break;
  case N_RS:
  case N_AB:
    c->raise_stmt.exception_choice = node_clone_substitute(n->raise_stmt.exception_choice, fp, ap);
    break;
  case N_NS:
    break;
  case N_CLT:
    c->code_stmt.name = node_clone_substitute(n->code_stmt.name, fp, ap);
    normalize_compile_symbol_vector(&c->code_stmt.arguments, &n->code_stmt.arguments, fp, ap);
    break;
  case N_ACC:
    c->accept_stmt.name = n->accept_stmt.name;
    normalize_compile_symbol_vector(&c->accept_stmt.index_constraints, &n->accept_stmt.index_constraints, fp, ap);
    normalize_compile_symbol_vector(&c->accept_stmt.parameters, &n->accept_stmt.parameters, fp, ap);
    normalize_compile_symbol_vector(&c->accept_stmt.statements, &n->accept_stmt.statements, fp, ap);
    normalize_compile_symbol_vector(&c->accept_stmt.handlers, &n->accept_stmt.handlers, fp, ap);
    c->accept_stmt.guard = node_clone_substitute(n->accept_stmt.guard, fp, ap);
    break;
  case N_SLS:
    normalize_compile_symbol_vector(&c->select_stmt.alternatives, &n->select_stmt.alternatives, fp, ap);
    normalize_compile_symbol_vector(&c->select_stmt.else_statements, &n->select_stmt.else_statements, fp, ap);
    break;
  case N_SA:
    c->abort_stmt.kind = n->abort_stmt.kind;
    c->abort_stmt.guard = node_clone_substitute(n->abort_stmt.guard, fp, ap);
    normalize_compile_symbol_vector(&c->abort_stmt.statements, &n->abort_stmt.statements, fp, ap);
    break;
  case N_TKS:
    c->task_spec.name = n->task_spec.name;
    normalize_compile_symbol_vector(&c->task_spec.entries, &n->task_spec.entries, fp, ap);
    c->task_spec.is_type = n->task_spec.is_type;
    break;
  case N_TKB:
    c->task_body.name = n->task_body.name;
    normalize_compile_symbol_vector(&c->task_body.declarations, &n->task_body.declarations, fp, ap);
    normalize_compile_symbol_vector(&c->task_body.statements, &n->task_body.statements, fp, ap);
    normalize_compile_symbol_vector(&c->task_body.handlers, &n->task_body.handlers, fp, ap);
    break;
  case N_ENT:
    c->entry_decl.name = n->entry_decl.name;
    normalize_compile_symbol_vector(&c->entry_decl.index_constraints, &n->entry_decl.index_constraints, fp, ap);
    normalize_compile_symbol_vector(&c->entry_decl.parameters, &n->entry_decl.parameters, fp, ap);
    c->entry_decl.guard = node_clone_substitute(n->entry_decl.guard, fp, ap);
    break;
  case N_HD:
  case N_WH:
  case N_DL:
  case N_TRM:
    normalize_compile_symbol_vector(&c->exception_handler.exception_choices, &n->exception_handler.exception_choices, fp, ap);
    normalize_compile_symbol_vector(&c->exception_handler.statements, &n->exception_handler.statements, fp, ap);
    break;
  case N_CH:
    normalize_compile_symbol_vector(&c->choices.items, &n->choices.items, fp, ap);
    break;
  case N_ASC:
    normalize_compile_symbol_vector(&c->association.choices, &n->association.choices, fp, ap);
    c->association.value = node_clone_substitute(n->association.value, fp, ap);
    break;
  case N_CX:
    normalize_compile_symbol_vector(&c->context.with_clauses, &n->context.with_clauses, fp, ap);
    normalize_compile_symbol_vector(&c->context.use_clauses, &n->context.use_clauses, fp, ap);
    break;
  case N_WI:
    c->with_clause.name = n->with_clause.name;
    break;
  case N_US:
    c->use_clause.nm = node_clone_substitute(n->use_clause.nm, fp, ap);
    break;
  case N_PG:
    c->pragma.name = n->pragma.name;
    normalize_compile_symbol_vector(&c->pragma.arguments, &n->pragma.arguments, fp, ap);
    break;
  case N_CU:
    c->compilation_unit.context = node_clone_substitute(n->compilation_unit.context, fp, ap);
    normalize_compile_symbol_vector(&c->compilation_unit.units, &n->compilation_unit.units, fp, ap);
    break;
  case N_DRF:
    c->dereference.expression = node_clone_substitute(n->dereference.expression, fp, ap);
    break;
  case N_CVT:
    c->conversion.ty = node_clone_substitute(n->conversion.ty, fp, ap);
    c->conversion.expression = node_clone_substitute(n->conversion.expression, fp, ap);
    break;
  case N_CHK:
    c->check.expression = node_clone_substitute(n->check.expression, fp, ap);
    c->check.exception_name = n->check.exception_name;
    break;
  case N_DRV:
    c->derived_type.base_type = node_clone_substitute(n->derived_type.base_type, fp, ap);
    normalize_compile_symbol_vector(&c->derived_type.operators, &n->derived_type.operators, fp, ap);
    break;
  case N_GEN:
    normalize_compile_symbol_vector(&c->generic_decl.formal_parameters, &n->generic_decl.formal_parameters, fp, ap);
    normalize_compile_symbol_vector(&c->generic_decl.declarations, &n->generic_decl.declarations, fp, ap);
    c->generic_decl.unit = node_clone_substitute(n->generic_decl.unit, fp, ap);
    break;
  case N_GINST:
    c->generic_inst.name = n->generic_inst.name.string ? string_duplicate(n->generic_inst.name) : n->generic_inst.name;
    c->generic_inst.generic_name = n->generic_inst.generic_name.string ? string_duplicate(n->generic_inst.generic_name) : n->generic_inst.generic_name;
    normalize_compile_symbol_vector(&c->generic_inst.actual_parameters, &n->generic_inst.actual_parameters, fp, ap);
    break;
  case N_AG:
    normalize_compile_symbol_vector(&c->aggregate.items, &n->aggregate.items, fp, ap);
    c->aggregate.low_bound = node_clone_substitute(n->aggregate.low_bound, fp, ap);
    c->aggregate.high_bound = node_clone_substitute(n->aggregate.high_bound, fp, ap);
    c->aggregate.dimensions = n->aggregate.dimensions;
    break;
  case N_TA:
    normalize_compile_symbol_vector(&c->index.indices, &n->index.indices, fp, ap);
    c->index.prefix = node_clone_substitute(n->index.prefix, fp, ap);
    break;
  case N_TI:
  case N_TE:
  case N_TF:
  case N_TX:
  case N_TR:
  case N_TAC:
  case N_TP:
  case N_ST:
  case N_LST:
    normalize_compile_symbol_vector(&c->list.items, &n->list.items, fp, ap);
    break;
  default:
    break;
  }
  node_clone_depth--;
  return c;
}
static bool match_formal_parameter(Syntax_Node *f, String_Slice nm)
{
  if (f->k == N_GTP)
    return string_equal_ignore_case(f->type_decl.name, nm);
  if (f->k == N_GSP)
    return string_equal_ignore_case(f->subprogram.name, nm);
  if (f->k == N_GVL)
    for (uint32_t j = 0; j < f->object_decl.identifiers.count; j++)
      if (string_equal_ignore_case(f->object_decl.identifiers.data[j]->string_value, nm))
        return 1;
  return 0;
}
static Syntax_Node *generate_clone(Symbol_Manager *symbol_manager, Syntax_Node *n)
{
  if (not n)
    return 0;
  if (n->k == N_GEN)
  {
    String_Slice nm = n->generic_decl.unit ? (n->generic_decl.unit->k == N_PKS ? n->generic_decl.unit->package_spec.name
                                   : n->generic_decl.unit->body.subprogram_spec    ? n->generic_decl.unit->body.subprogram_spec->subprogram.name
                                                         : N)
                                : N;
    Generic_Template *g = generic_find(symbol_manager, nm);
    if (not g)
    {
      g = generic_type_new(nm);
      g->formal_parameters = n->generic_decl.formal_parameters;
      g->declarations = n->generic_decl.declarations;
      g->unit = n->generic_decl.unit;
      gv(&symbol_manager->gt, g);
      if (g->name.string and g->name.length)
      {
        Symbol *gs = symbol_new(g->name, 11, 0, n);
        gs->generic_template = g;
        if (g->unit and g->unit->k == N_PKS)
          gs->definition = g->unit;
        symbol_add_overload(symbol_manager, gs);
      }
    }
  }
  else if (n->k == N_GINST)
  {
    Generic_Template *g = generic_find(symbol_manager, n->generic_inst.generic_name);
    if (g)
    {
      resolve_array_parameter(symbol_manager, &g->formal_parameters, &n->generic_inst.actual_parameters);
      Syntax_Node *inst = node_clone_substitute(g->unit, &g->formal_parameters, &n->generic_inst.actual_parameters);
      if (inst)
      {
        if (inst->k == N_PB or inst->k == N_FB or inst->k == N_PD or inst->k == N_FD)
        {
          inst->body.subprogram_spec->subprogram.name = n->generic_inst.name;
        }
        else if (inst->k == N_PKS)
        {
          inst->package_spec.name = n->generic_inst.name;
        }
        return inst;
      }
    }
  }
  return 0;
}
static Symbol *get_pkg_sym(Symbol_Manager *symbol_manager, Syntax_Node *pk)
{
  if (not pk or not pk->symbol)
    return 0;
  String_Slice nm = pk->k == N_PKS ? pk->package_spec.name : pk->symbol->name;
  if (not nm.string or nm.length == 0)
    return 0;
  uint32_t h = symbol_hash(nm);
  for (Symbol *s = symbol_manager->sy[h]; s; s = s->next)
    if (s->k == 6 and string_equal_ignore_case(s->name, nm) and s->level == 0)
      return s;
  return pk->symbol;
}
static void resolve_declaration(Symbol_Manager *symbol_manager, Syntax_Node *n)
{
  if (not n)
    return;
  switch (n->k)
  {
  case N_GINST:
  {
    Syntax_Node *inst = generate_clone(symbol_manager, n);
    if (inst)
    {
      resolve_declaration(symbol_manager, inst);
      if (inst->k == N_PKS)
      {
        Generic_Template *g = generic_find(symbol_manager, n->generic_inst.generic_name);
        if (g and g->body)
        {
          Syntax_Node *bd = node_clone_substitute(g->body, &g->formal_parameters, &n->generic_inst.actual_parameters);
          if (bd)
          {
            bd->package_body.name = n->generic_inst.name;
            resolve_declaration(symbol_manager, bd);
            nv(&symbol_manager->ib, bd);
          }
        }
      }
    }
  }
  break;
  case N_RRC:
  {
    Representation_Clause *r = *(Representation_Clause **) &n->aggregate.items.data;
    runtime_register_compare(symbol_manager, r);
  }
  break;
  case N_GVL:
  case N_OD:
  {
    Type_Info *t = resolve_subtype(symbol_manager, n->object_decl.ty);
    for (uint32_t i = 0; i < n->object_decl.identifiers.count; i++)
    {
      Syntax_Node *id = n->object_decl.identifiers.data[i];
      Type_Info *ct = universal_composite_aggregate(t, n->object_decl.in);
      Symbol *x = symbol_find(symbol_manager, id->string_value);
      Symbol *s = 0;
      if (x and x->scope == symbol_manager->sc and x->storage_size == symbol_manager->ss and x->k != 11)
      {
        if (x->k == 2 and x->definition and x->definition->k == N_OD and not((Syntax_Node *) x->definition)->object_decl.in
            and n->object_decl.is_constant and n->object_decl.in)
        {
          s = x;
        }
        else if (error_count < 99)
          fatal_error(n->location, "dup '%.*s'", (int) id->string_value.length, id->string_value.string);
      }
      if (not s)
      {
        s = symbol_add_overload(symbol_manager, symbol_new(id->string_value, n->object_decl.is_constant ? 2 : 0, ct, n));
        // Set parent to enclosing procedure if nested, otherwise to package
        s->parent = symbol_manager->pn > 0 ? symbol_manager->ps[symbol_manager->pn - 1]
                    : (symbol_manager->pk ? get_pkg_sym(symbol_manager, symbol_manager->pk)
                    : (SEPARATE_PACKAGE.string ? symbol_find(symbol_manager, SEPARATE_PACKAGE) : 0));
      }
      id->symbol = s;
      if (n->object_decl.in)
      {
        resolve_expression(symbol_manager, n->object_decl.in, t);
        n->object_decl.in->ty = t;
        n->object_decl.in = chk(symbol_manager, n->object_decl.in, n->location);
        if (t and t->discriminants.count > 0 and n->object_decl.in and n->object_decl.in->ty)
        {
          Type_Info *it = type_canonical_concrete(n->object_decl.in->ty);
          if (it and it->discriminants.count > 0)
          {
            for (uint32_t di = 0; di < t->discriminants.count and di < it->discriminants.count; di++)
            {
              Syntax_Node *td = t->discriminants.data[di];
              Syntax_Node *id = it->discriminants.data[di];
              if (td->k == N_DS and id->k == N_DS and td->parameter.default_value and id->parameter.default_value)
              {
                if (td->parameter.default_value->k == N_INT and id->parameter.default_value->k == N_INT
                    and td->parameter.default_value->integer_value != id->parameter.default_value->integer_value)
                {
                  Syntax_Node *dc = ND(CHK, n->location);
                  dc->check.expression = n->object_decl.in;
                  dc->check.exception_name = STRING_LITERAL("CONSTRAINT_ERROR");
                  n->object_decl.in = dc;
                  break;
                }
              }
            }
          }
        }
        s->definition = n->object_decl.in;
        if (n->object_decl.is_constant and n->object_decl.in->k == N_INT)
          s->value = n->object_decl.in->integer_value;
        else if (n->object_decl.is_constant and n->object_decl.in->k == N_ID and n->object_decl.in->symbol and n->object_decl.in->symbol->k == 2)
          s->value = n->object_decl.in->symbol->value;
        else if (n->object_decl.is_constant and n->object_decl.in->k == N_AT)
        {
          Type_Info *pt = n->object_decl.in->attribute.prefix ? type_canonical_concrete(n->object_decl.in->attribute.prefix->ty) : 0;
          String_Slice a = n->object_decl.in->attribute.attribute_name;
          if (pt and string_equal_ignore_case(a, STRING_LITERAL("FIRST")))
            s->value = pt->low_bound;
          else if (pt and string_equal_ignore_case(a, STRING_LITERAL("LAST")))
            s->value = pt->high_bound;
        }
        else if (n->object_decl.is_constant and n->object_decl.in->k == N_QL and n->object_decl.in->qualified.aggregate)
        {
          Syntax_Node *ag = n->object_decl.in->qualified.aggregate;
          if (ag->k == N_ID and ag->symbol and ag->symbol->k == 2)
            s->value = ag->symbol->value;
          else if (ag->k == N_INT)
            s->value = ag->integer_value;
        }
      }
    }
  }
  break;
  case N_GTP:
  case N_TD:
  {
    uint32_t of = 0;
    Type_Info *t = 0;
    if (n->type_decl.is_derived and n->type_decl.parent_type)
    {
      Type_Info *pt = resolve_subtype(symbol_manager, n->type_decl.parent_type);
      if (n->type_decl.parent_type->k == N_TAC and error_count < 99)
        fatal_error(n->location, "der acc ty");
      t = type_new(TYPE_DERIVED, n->type_decl.name);
      t->parent_type = pt;
      if (pt)
      {
        t->low_bound = pt->low_bound;
        t->high_bound = pt->high_bound;
        t->element_type = pt->element_type;
        t->discriminants = pt->discriminants;
        t->size = pt->size;
        t->alignment = pt->alignment;
        {
          Type_Info *_ept = pt;
          while (_ept and _ept->enum_values.count == 0 and (_ept->base_type or _ept->parent_type))
            _ept = _ept->base_type ? _ept->base_type : _ept->parent_type;
          t->enum_values.count = 0;
          t->enum_values.data = 0;
          if (_ept)
            for (uint32_t _i = 0; _i < _ept->enum_values.count; _i++)
            {
              Symbol *_pe = _ept->enum_values.data[_i];
              Symbol *_ne = symbol_add_overload(symbol_manager, symbol_new(_pe->name, 2, t, n));
              _ne->value = _pe->value;
              sv(&t->enum_values, _ne);
            }
        }
        is_higher_order_parameter(t, pt);
      }
      if (n->type_decl.definition and n->type_decl.definition->k == N_RN)
      {
        resolve_expression(symbol_manager, n->type_decl.definition->range.low_bound, 0);
        resolve_expression(symbol_manager, n->type_decl.definition->range.high_bound, 0);
        t->low_bound = n->type_decl.definition->range.low_bound->integer_value;
        t->high_bound = n->type_decl.definition->range.high_bound->integer_value;
      }
    }
    else
    {
      Symbol *px = symbol_find(symbol_manager, n->type_decl.name);
      if (px and px->k == 1 and px->type_info and (px->type_info->k == TYPE_INTEGER or px->type_info->k == TY_PT)
          and n->type_decl.definition)
      {
        if (px->type_info->k == TY_PT)
        {
          t = px->type_info;
          t->parent_type = resolve_subtype(symbol_manager, n->type_decl.definition);
        }
        else
          t = px->type_info;
      }
      else if (n->type_decl.definition)
      {
        t = resolve_subtype(symbol_manager, n->type_decl.definition);
      }
      else
      {
        t = type_new(n->type_decl.definition or n->type_decl.is_derived ? TYPE_INTEGER : TY_PT, n->type_decl.name);
        if (t and n->type_decl.name.string)
        {
          Symbol *s = symbol_add_overload(symbol_manager, symbol_new(n->type_decl.name, 1, t, n));
          n->symbol = s;
        }
        break;
      }
    }
    if (t and n->type_decl.name.string and n->type_decl.name.length > 0 and not t->name.string)
      t->name = n->type_decl.name;
    if (n->type_decl.discriminants.count > 0)
    {
      for (uint32_t i = 0; i < n->type_decl.discriminants.count; i++)
      {
        Syntax_Node *d = n->type_decl.discriminants.data[i];
        if (d->k == N_DS)
        {
          Symbol *ds = symbol_add_overload(symbol_manager, symbol_new(d->parameter.name, 8, resolve_subtype(symbol_manager, d->parameter.ty), d));
          if (d->parameter.default_value)
            resolve_expression(symbol_manager, d->parameter.default_value, ds->type_info);
        }
      }
      t->discriminants = n->type_decl.discriminants;
    }
    if (n->type_decl.name.string and n->type_decl.name.length > 0)
    {
      Symbol *px2 = symbol_find(symbol_manager, n->type_decl.name);
      if (px2 and px2->k == 1 and px2->type_info == t)
      {
        n->symbol = px2;
        if (n->type_decl.definition)
          px2->definition = n;
      }
      else
      {
        Symbol *s = symbol_add_overload(symbol_manager, symbol_new(n->type_decl.name, 1, t, n));
        n->symbol = s;
      }
    }
    if (n->type_decl.definition and n->type_decl.definition->k == N_TE)
    {
      t->k = TYPE_ENUMERATION;
      int vl = 0;
      for (uint32_t i = 0; i < n->type_decl.definition->list.items.count; i++)
      {
        Syntax_Node *it = n->type_decl.definition->list.items.data[i];
        Symbol *es = symbol_add_overload(
            symbol_manager, symbol_new(it->k == N_CHAR ? (String_Slice){(const char *) &it->integer_value, 1} : it->string_value, 2, t, n));
        es->value = vl++;
        sv(&t->enum_values, es);
      }
      t->low_bound = 0;
      t->high_bound = vl - 1;
    }
    if (n->type_decl.definition and n->type_decl.definition->k == N_TR)
    {
      t->k = TYPE_RECORD;
      of = 0;
      for (uint32_t i = 0; i < n->type_decl.definition->list.items.count; i++)
      {
        Syntax_Node *c = n->type_decl.definition->list.items.data[i];
        if (c->k == N_CM)
        {
          c->component_decl.offset = of++;
          Type_Info *ct = resolve_subtype(symbol_manager, c->component_decl.ty);
          if (ct)
            c->component_decl.ty->ty = ct;
          if (c->component_decl.discriminant_constraint)
          {
            for (uint32_t j = 0; j < c->component_decl.discriminant_constraint->list.items.count; j++)
            {
              Syntax_Node *dc = c->component_decl.discriminant_constraint->list.items.data[j];
              if (dc->k == N_DS and dc->parameter.default_value)
                resolve_expression(symbol_manager, dc->parameter.default_value, resolve_subtype(symbol_manager, dc->parameter.ty));
            }
          }
          if (c->component_decl.discriminant_spec)
          {
            for (uint32_t j = 0; j < c->component_decl.discriminant_spec->list.items.count; j++)
            {
              Syntax_Node *dc = c->component_decl.discriminant_spec->list.items.data[j];
              if (dc->k == N_DS)
              {
                Symbol *ds =
                    symbol_add_overload(symbol_manager, symbol_new(dc->parameter.name, 8, resolve_subtype(symbol_manager, dc->parameter.ty), dc));
                if (dc->parameter.default_value)
                  resolve_expression(symbol_manager, dc->parameter.default_value, ds->type_info);
              }
            }
            c->component_decl.discriminant_constraint = c->component_decl.discriminant_spec;
          }
        }
        else if (c->k == N_VP)
        {
          for (uint32_t j = 0; j < c->variant_part.variants.count; j++)
          {
            Syntax_Node *v = c->variant_part.variants.data[j];
            for (uint32_t k = 0; k < v->variant.components.count; k++)
            {
              Syntax_Node *vc = v->variant.components.data[k];
              vc->component_decl.offset = of++;
              Type_Info *vct = resolve_subtype(symbol_manager, vc->component_decl.ty);
              if (vct)
                vc->component_decl.ty->ty = vct;
              if (vc->component_decl.discriminant_constraint)
              {
                for (uint32_t m = 0; m < vc->component_decl.discriminant_constraint->list.items.count; m++)
                {
                  Syntax_Node *dc = vc->component_decl.discriminant_constraint->list.items.data[m];
                  if (dc->k == N_DS and dc->parameter.default_value)
                    resolve_expression(symbol_manager, dc->parameter.default_value, resolve_subtype(symbol_manager, dc->parameter.ty));
                }
              }
              if (vc->component_decl.discriminant_spec)
              {
                for (uint32_t m = 0; m < vc->component_decl.discriminant_spec->list.items.count; m++)
                {
                  Syntax_Node *dc = vc->component_decl.discriminant_spec->list.items.data[m];
                  if (dc->k == N_DS)
                  {
                    Symbol *ds = symbol_add_overload(
                        symbol_manager, symbol_new(dc->parameter.name, 8, resolve_subtype(symbol_manager, dc->parameter.ty), dc));
                    if (dc->parameter.default_value)
                      resolve_expression(symbol_manager, dc->parameter.default_value, ds->type_info);
                  }
                }
                vc->component_decl.discriminant_constraint = vc->component_decl.discriminant_spec;
              }
            }
          }
        }
      }
    }
    t->components = n->type_decl.definition->list.items;
    t->size = of * 8;
  }
  break;
  case N_SD:
  {
    Type_Info *b = resolve_subtype(symbol_manager, n->subtype_decl.index_constraint);
    Type_Info *t = type_new(b ? b->k : TYPE_INTEGER, n->subtype_decl.name);
    if (b)
    {
      t->base_type = b;
      t->element_type = b->element_type;
      t->components = b->components;
      t->discriminants = b->discriminants;
      t->size = b->size;
      t->alignment = b->alignment;
      t->address = b->address;
      t->is_packed = b->is_packed;
      t->low_bound = b->low_bound;
      t->high_bound = b->high_bound;
      t->parent_type = b->parent_type ? b->parent_type : b;
    }
    if (n->subtype_decl.range_constraint)
    {
      resolve_expression(symbol_manager, n->subtype_decl.range_constraint->range.low_bound, 0);
      resolve_expression(symbol_manager, n->subtype_decl.range_constraint->range.high_bound, 0);
      Syntax_Node *lo = n->subtype_decl.range_constraint->range.low_bound;
      Syntax_Node *hi = n->subtype_decl.range_constraint->range.high_bound;
      int64_t lov = lo->k == N_UN and lo->unary_node.op == T_MN and lo->unary_node.operand->k == N_INT ? -lo->unary_node.operand->integer_value
                    : lo->k == N_ID and lo->symbol and lo->symbol->k == 2                ? lo->symbol->value
                                                                                 : lo->integer_value;
      int64_t hiv = hi->k == N_UN and hi->unary_node.op == T_MN and hi->unary_node.operand->k == N_INT ? -hi->unary_node.operand->integer_value
                    : hi->k == N_ID and hi->symbol and hi->symbol->k == 2                ? hi->symbol->value
                                                                                 : hi->integer_value;
      t->low_bound = lov;
      t->high_bound = hiv;
    }
    symbol_add_overload(symbol_manager, symbol_new(n->subtype_decl.name, 1, t, n));
  }
  break;
  case N_ED:
    for (uint32_t i = 0; i < n->exception_decl.identifiers.count; i++)
    {
      Syntax_Node *id = n->exception_decl.identifiers.data[i];
      if (n->exception_decl.renamed_entity)
      {
        resolve_expression(symbol_manager, n->exception_decl.renamed_entity, 0);
        Symbol *tgt = n->exception_decl.renamed_entity->symbol;
        if (tgt and tgt->k == 3)
        {
          Symbol *al = symbol_add_overload(symbol_manager, symbol_new(id->string_value, 3, 0, n));
          al->definition = n->exception_decl.renamed_entity;
          id->symbol = al;
        }
        else if (error_count < 99)
          fatal_error(n->location, "renames must be exception");
      }
      else
      {
        id->symbol = symbol_add_overload(symbol_manager, symbol_new(id->string_value, 3, 0, n));
      }
    }
    break;
  case N_GSP:
  {
    Type_Info *ft = type_new(TYPE_STRING, n->subprogram.name);
    if (n->subprogram.return_type)
    {
      Type_Info *rt = resolve_subtype(symbol_manager, n->subprogram.return_type);
      ft->element_type = rt;
      Symbol *s = symbol_add_overload(symbol_manager, symbol_new(n->subprogram.name, 5, ft, n));
      nv(&s->overloads, n);
      n->symbol = s;
      s->parent = symbol_manager->pk ? get_pkg_sym(symbol_manager, symbol_manager->pk) : SEPARATE_PACKAGE.string ? symbol_find(symbol_manager, SEPARATE_PACKAGE) : 0;
      nv(&ft->operations, n);
    }
    else
    {
      Symbol *s = symbol_add_overload(symbol_manager, symbol_new(n->subprogram.name, 4, ft, n));
      nv(&s->overloads, n);
      n->symbol = s;
      s->parent = symbol_manager->pk ? get_pkg_sym(symbol_manager, symbol_manager->pk) : SEPARATE_PACKAGE.string ? symbol_find(symbol_manager, SEPARATE_PACKAGE) : 0;
      nv(&ft->operations, n);
    }
  }
  break;
  case N_PD:
  case N_PB:
  {
    Syntax_Node *sp = n->body.subprogram_spec;
    Type_Info *ft = type_new(TYPE_STRING, sp->subprogram.name);
    Symbol *s = symbol_add_overload(symbol_manager, symbol_new(sp->subprogram.name, 4, ft, n));
    nv(&s->overloads, n);
    n->symbol = s;
    n->body.elaboration_level = s->elaboration_level;
    // Set parent to enclosing procedure if nested, otherwise to package
    s->parent = symbol_manager->pn > 0 ? symbol_manager->ps[symbol_manager->pn - 1]
                : (symbol_manager->pk ? get_pkg_sym(symbol_manager, symbol_manager->pk)
                : (SEPARATE_PACKAGE.string ? symbol_find(symbol_manager, SEPARATE_PACKAGE) : 0));
    nv(&ft->operations, n);
    if (n->k == N_PB)
    {
      symbol_manager->lv++;
      // Push this procedure onto the procedure stack
      if (symbol_manager->pn < 256)
        symbol_manager->ps[symbol_manager->pn++] = s;
      symbol_compare_parameter(symbol_manager);
      n->body.parent = s;
      Generic_Template *gt = generic_find(symbol_manager, sp->subprogram.name);
      if (gt)
        for (uint32_t i = 0; i < gt->formal_parameters.count; i++)
          resolve_declaration(symbol_manager, gt->formal_parameters.data[i]);
      for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
      {
        Syntax_Node *p = sp->subprogram.parameters.data[i];
        Type_Info *pt = resolve_subtype(symbol_manager, p->parameter.ty);
        Symbol *ps = symbol_add_overload(symbol_manager, symbol_new(p->parameter.name, 0, pt, p));
        p->symbol = ps;
      }
      for (uint32_t i = 0; i < n->body.declarations.count; i++)
        resolve_declaration(symbol_manager, n->body.declarations.data[i]);
      for (uint32_t i = 0; i < n->body.statements.count; i++)
        resolve_statement_sequence(symbol_manager, n->body.statements.data[i]);
      symbol_compare_overload(symbol_manager);
      // Pop this procedure from the stack
      if (symbol_manager->pn > 0)
        symbol_manager->pn--;
      symbol_manager->lv--;
    }
  }
  break;
  case N_FB:
  {
    Syntax_Node *sp = n->body.subprogram_spec;
    Type_Info *rt = resolve_subtype(symbol_manager, sp->subprogram.return_type);
    Type_Info *ft = type_new(TYPE_STRING, sp->subprogram.name);
    ft->element_type = rt;
    Symbol *s = symbol_add_overload(symbol_manager, symbol_new(sp->subprogram.name, 5, ft, n));
    nv(&s->overloads, n);
    n->symbol = s;
    n->body.elaboration_level = s->elaboration_level;
    // Set parent to enclosing procedure if nested, otherwise to package
    s->parent = symbol_manager->pn > 0 ? symbol_manager->ps[symbol_manager->pn - 1]
                : (symbol_manager->pk ? get_pkg_sym(symbol_manager, symbol_manager->pk)
                : (SEPARATE_PACKAGE.string ? symbol_find(symbol_manager, SEPARATE_PACKAGE) : 0));
    nv(&ft->operations, n);
    if (n->k == N_FB)
    {
      symbol_manager->lv++;
      // Push this function onto the procedure stack
      if (symbol_manager->pn < 256)
        symbol_manager->ps[symbol_manager->pn++] = s;
      symbol_compare_parameter(symbol_manager);
      n->body.parent = s;
      Generic_Template *gt = generic_find(symbol_manager, sp->subprogram.name);
      if (gt)
        for (uint32_t i = 0; i < gt->formal_parameters.count; i++)
          resolve_declaration(symbol_manager, gt->formal_parameters.data[i]);
      for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
      {
        Syntax_Node *p = sp->subprogram.parameters.data[i];
        Type_Info *pt = resolve_subtype(symbol_manager, p->parameter.ty);
        Symbol *ps = symbol_add_overload(symbol_manager, symbol_new(p->parameter.name, 0, pt, p));
        p->symbol = ps;
      }
      for (uint32_t i = 0; i < n->body.declarations.count; i++)
        resolve_declaration(symbol_manager, n->body.declarations.data[i]);
      for (uint32_t i = 0; i < n->body.statements.count; i++)
        resolve_statement_sequence(symbol_manager, n->body.statements.data[i]);
      symbol_compare_overload(symbol_manager);
      // Pop this function from the stack
      if (symbol_manager->pn > 0)
        symbol_manager->pn--;
      symbol_manager->lv--;
    }
  }
  break;
  case N_FD:
  {
    Syntax_Node *sp = n->body.subprogram_spec;
    Type_Info *rt = resolve_subtype(symbol_manager, sp->subprogram.return_type);
    Type_Info *ft = type_new(TYPE_STRING, sp->subprogram.name);
    ft->element_type = rt;
    Symbol *s = symbol_add_overload(symbol_manager, symbol_new(sp->subprogram.name, 5, ft, n));
    nv(&s->overloads, n);
    n->symbol = s;
    n->body.elaboration_level = s->elaboration_level;
    s->parent = symbol_manager->pk ? get_pkg_sym(symbol_manager, symbol_manager->pk) : SEPARATE_PACKAGE.string ? symbol_find(symbol_manager, SEPARATE_PACKAGE) : 0;
    nv(&ft->operations, n);
  }
  break;
  case N_PKS:
  {
    Type_Info *t = type_new(TY_P, n->package_spec.name);
    Symbol *s = symbol_add_overload(symbol_manager, symbol_new(n->package_spec.name, 6, t, n));
    n->symbol = s;
    n->package_spec.elaboration_level = s->elaboration_level;
    symbol_manager->pk = n;
    symbol_compare_parameter(symbol_manager);
    for (uint32_t i = 0; i < n->package_spec.declarations.count; i++)
      resolve_declaration(symbol_manager, n->package_spec.declarations.data[i]);
    for (uint32_t i = 0; i < n->package_spec.private_declarations.count; i++)
      resolve_declaration(symbol_manager, n->package_spec.private_declarations.data[i]);
    symbol_compare_overload(symbol_manager);
    symbol_manager->pk = 0;
  }
  break;
  case N_PKB:
  {
    Symbol *ps = symbol_find(symbol_manager, n->package_body.name);
    Generic_Template *gt = 0;
    if (ps and ps->k == 11)
    {
      gt = ps->generic_template ? ps->generic_template : generic_find(symbol_manager, n->package_body.name);
    }
    if (gt)
    {
      gt->body = n;
      Syntax_Node *pk = gt->unit and gt->unit->k == N_PKS ? gt->unit : 0;
      if (not pk and ps and ps->definition and ps->definition->k == N_PKS)
        pk = ps->definition;
      if (pk)
      {
        symbol_compare_parameter(symbol_manager);
        symbol_manager->pk = pk;
        for (uint32_t i = 0; i < pk->package_spec.declarations.count; i++)
          resolve_declaration(symbol_manager, pk->package_spec.declarations.data[i]);
        for (uint32_t i = 0; i < gt->formal_parameters.count; i++)
          resolve_declaration(symbol_manager, gt->formal_parameters.data[i]);
        for (uint32_t i = 0; i < n->package_body.declarations.count; i++)
          resolve_declaration(symbol_manager, n->package_body.declarations.data[i]);
        for (uint32_t i = 0; i < n->package_body.statements.count; i++)
          resolve_statement_sequence(symbol_manager, n->package_body.statements.data[i]);
        symbol_compare_overload(symbol_manager);
        symbol_manager->pk = 0;
      }
      break;
    }
    symbol_compare_parameter(symbol_manager);
    {
      const char *_src = lookup_path(symbol_manager, n->package_body.name);
      if (_src)
      {
        char _af[512];
        snprintf(_af, 512, "%.*s.ads", (int) n->package_body.name.length, n->package_body.name.string);
        Parser _p = parser_new(_src, strlen(_src), _af);
        Syntax_Node *_cu = parse_compilation_unit(&_p);
        if (_cu)
          for (uint32_t _i = 0; _i < _cu->compilation_unit.units.count; _i++)
          {
            Syntax_Node *_u = _cu->compilation_unit.units.data[_i];
            Syntax_Node *_pk = _u->k == N_PKS ? _u
                               : _u->k == N_GEN and _u->generic_decl.unit and _u->generic_decl.unit->k == N_PKS
                                   ? _u->generic_decl.unit
                                   : 0;
            if (_pk and string_equal_ignore_case(_pk->package_spec.name, n->package_body.name))
            {
              symbol_manager->pk = _pk;
              for (uint32_t _j = 0; _j < _pk->package_spec.declarations.count; _j++)
                resolve_declaration(symbol_manager, _pk->package_spec.declarations.data[_j]);
              for (uint32_t _j = 0; _j < _pk->package_spec.private_declarations.count; _j++)
                resolve_declaration(symbol_manager, _pk->package_spec.private_declarations.data[_j]);
              symbol_manager->pk = 0;
              break;
            }
          }
      }
    }
    ps = symbol_find(symbol_manager, n->package_body.name);
    if (not ps or not ps->definition)
    {
      Type_Info *t = type_new(TY_P, n->package_body.name);
      ps = symbol_add_overload(symbol_manager, symbol_new(n->package_body.name, 6, t, 0));
      ps->elaboration_level = symbol_manager->eo++;
      Syntax_Node *pk = ND(PKS, n->location);
      pk->package_spec.name = n->package_body.name;
      pk->symbol = ps;
      ps->definition = pk;
      n->symbol = ps;
    }
    if (ps)
    {
      sv(&symbol_manager->uv, ps);
      n->package_body.elaboration_level = ps->elaboration_level;
      symbol_manager->pk = ps->definition;
      if (ps->definition and ps->definition->k == N_PKS)
      {
        Syntax_Node *pk = ps->definition;
        for (uint32_t i = 0; i < pk->package_spec.declarations.count; i++)
        {
          Syntax_Node *d = pk->package_spec.declarations.data[i];
          if (d->symbol)
          {
            d->symbol->visibility |= 2;
            sv(&symbol_manager->uv, d->symbol);
          }
          if (d->k == N_ED)
            for (uint32_t j = 0; j < d->exception_decl.identifiers.count; j++)
              if (d->exception_decl.identifiers.data[j]->symbol)
              {
                d->exception_decl.identifiers.data[j]->symbol->visibility |= 2;
                sv(&symbol_manager->uv, d->exception_decl.identifiers.data[j]->symbol);
              }
        }
        for (uint32_t i = 0; i < pk->package_spec.private_declarations.count; i++)
        {
          Syntax_Node *d = pk->package_spec.private_declarations.data[i];
          if (d->symbol)
          {
            d->symbol->visibility |= 2;
            sv(&symbol_manager->uv, d->symbol);
          }
          if (d->k == N_ED)
            for (uint32_t j = 0; j < d->exception_decl.identifiers.count; j++)
              if (d->exception_decl.identifiers.data[j]->symbol)
              {
                d->exception_decl.identifiers.data[j]->symbol->visibility |= 2;
                sv(&symbol_manager->uv, d->exception_decl.identifiers.data[j]->symbol);
              }
        }
        for (uint32_t i = 0; i < pk->package_spec.declarations.count; i++)
        {
          Syntax_Node *d = pk->package_spec.declarations.data[i];
          if (d->symbol)
            sv(&symbol_manager->uv, d->symbol);
          else if (d->k == N_ED)
          {
            for (uint32_t j = 0; j < d->exception_decl.identifiers.count; j++)
            {
              Syntax_Node *eid = d->exception_decl.identifiers.data[j];
              if (eid->symbol)
                sv(&symbol_manager->uv, eid->symbol);
            }
          }
          else if (d->k == N_OD)
          {
            for (uint32_t j = 0; j < d->object_decl.identifiers.count; j++)
            {
              Syntax_Node *oid = d->object_decl.identifiers.data[j];
              if (oid->symbol)
                sv(&symbol_manager->uv, oid->symbol);
            }
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->package_body.declarations.count; i++)
      resolve_declaration(symbol_manager, n->package_body.declarations.data[i]);
    for (uint32_t i = 0; i < n->package_body.statements.count; i++)
      resolve_statement_sequence(symbol_manager, n->package_body.statements.data[i]);
    symbol_compare_overload(symbol_manager);
    symbol_manager->pk = 0;
  }
  break;
  case N_TKS:
  {
    Type_Info *t = type_new(TY_T, n->task_spec.name);
    t->components = n->task_spec.entries;
    Symbol *s = symbol_add_overload(symbol_manager, symbol_new(n->task_spec.name, 7, t, n));
    n->symbol = s;
    s->parent = symbol_manager->pk ? get_pkg_sym(symbol_manager, symbol_manager->pk) : SEPARATE_PACKAGE.string ? symbol_find(symbol_manager, SEPARATE_PACKAGE) : 0;
  }
  break;
  case N_TKB:
  {
    Symbol *ts = symbol_find(symbol_manager, n->task_body.name);
    symbol_compare_parameter(symbol_manager);
    if (ts and ts->type_info and ts->type_info->components.count > 0)
    {
      for (uint32_t i = 0; i < ts->type_info->components.count; i++)
      {
        Syntax_Node *en = ts->type_info->components.data[i];
        if (en and en->k == N_ENT)
        {
          Symbol *ens = symbol_add_overload(symbol_manager, symbol_new(en->entry_decl.name, 9, 0, en));
          (void) ens;
        }
      }
    }
    for (uint32_t i = 0; i < n->task_body.declarations.count; i++)
      resolve_declaration(symbol_manager, n->task_body.declarations.data[i]);
    for (uint32_t i = 0; i < n->task_body.statements.count; i++)
      resolve_statement_sequence(symbol_manager, n->task_body.statements.data[i]);
    symbol_compare_overload(symbol_manager);
  }
  break;
  case N_GEN:
    generate_clone(symbol_manager, n);
    break;
  case N_US:
    resolve_statement_sequence(symbol_manager, n);
    break;
  default:
    break;
  }
}
static int elaborate_compilation(Symbol_Manager *symbol_manager, Symbol_Vector *ev, Syntax_Node *n)
{
  if (not n)
    return 0;
  int mx = 0;
  switch (n->k)
  {
  case N_PKS:
  case N_PKB:
  case N_PD:
  case N_PB:
  case N_FD:
  case N_FB:
  {
    Symbol *s = n->symbol;
    if (s and s->elaboration_level < 0)
      s->elaboration_level = symbol_manager->eo;
    if (s)
    {
      sv(ev, s);
      if (s->elaboration_level > mx)
        mx = s->elaboration_level;
    }
  }
  break;
  case N_OD:
    for (uint32_t i = 0; i < n->object_decl.identifiers.count; i++)
    {
      Syntax_Node *id = n->object_decl.identifiers.data[i];
      if (id->symbol)
      {
        sv(ev, id->symbol);
        if (id->symbol->elaboration_level > mx)
          mx = id->symbol->elaboration_level;
      }
    }
    break;
  default:
    break;
  }
  return mx;
}
static char *read_file(const char *path)
{
  FILE *f = fopen(path, "rb");
  if (not f)
    return 0;
  fseek(f, 0, 2);
  long z = ftell(f);
  fseek(f, 0, 0);
  char *b = malloc(z + 1);
  fread(b, 1, z, f);
  b[z] = 0;
  fclose(f);
  return b;
}
static void read_ada_library_interface(Symbol_Manager *symbol_manager, const char *pth)
{
  char a[512];
  snprintf(a, 512, "%s.ali", pth);
  char *ali = read_file(a);
  if (not ali)
    return;
  Source_Location ll = {0, 0, pth};
  String_List_Vector ws = {0}, ds = {0};
  for (char *l = ali; *l;)
  {
    if (*l == 'W' and l[1] == ' ')
    {
      char *e = l + 2;
      while (*e and *e != ' ' and *e != '\n')
        e++;
      String_Slice wn = {l + 2, e - (l + 2)};
      char *c = arena_allocate(wn.length + 1);
      memcpy(c, wn.string, wn.length);
      c[wn.length] = 0;
      slv(&ws, (String_Slice){c, wn.length});
    }
    else if (*l == 'D' and l[1] == ' ')
    {
      char *e = l + 2;
      while (*e and *e != ' ' and *e != '\n')
        e++;
      String_Slice dn = {l + 2, e - (l + 2)};
      char *c = arena_allocate(dn.length + 1);
      memcpy(c, dn.string, dn.length);
      c[dn.length] = 0;
      slv(&ds, (String_Slice){c, dn.length});
    }
    else if (*l == 'X' and l[1] == ' ')
    {
      char *e = l + 2;
      while (*e and *e != ' ' and *e != '\n')
        e++;
      String_Slice sn = {l + 2, e - (l + 2)};
      bool isp = 0;
      int pc = 0;
      char mn[256], *m = mn;
      for (uint32_t i = 0; i < sn.length and i < 250;)
        *m++ = sn.string[i++];
      for (char *t = e; *t and *t != '\n';)
      {
        while (*t == ' ')
          t++;
        if (*t == '\n')
          break;
        char *te = t;
        while (*te and *te != ' ' and *te != '\n')
          te++;
        String_Slice tn = {t, te - t};
        if (string_equal_ignore_case(tn, STRING_LITERAL("void")))
          isp = 1;
        else if (
            string_equal_ignore_case(tn, STRING_LITERAL("i64"))
            or string_equal_ignore_case(tn, STRING_LITERAL("double"))
            or string_equal_ignore_case(tn, STRING_LITERAL("ptr")))
        {
          if (pc++)
            (*m++ = '_', *m++ = '_');
          const char *tx = string_equal_ignore_case(tn, STRING_LITERAL("i64"))      ? "I64"
                           : string_equal_ignore_case(tn, STRING_LITERAL("double")) ? "F64"
                                                                                    : "PTR";
          while (*tx)
            *m++ = *tx++;
        }
        t = te;
      }
      *m = 0;
      String_Slice msn = {mn, m - mn};
      if (pc == 1)
      {
        Type_Info *vt = 0;
        if (strstr(mn, "I64"))
          vt = TY_INT;
        else if (strstr(mn, "F64"))
          vt = TY_FLT;
        else if (strstr(mn, "PTR"))
          vt = TY_STR;
        else
          vt = TY_INT;
        Syntax_Node *n = node_new(N_OD, ll);
        Symbol *s = symbol_add_overload(symbol_manager, symbol_new(sn, 0, vt, n));
        s->is_external = 1;
        s->level = 0;
        s->external_name = string_duplicate(msn);
        s->mangled_name = string_duplicate(sn);
        n->symbol = s;
      }
      else
      {
        Syntax_Node *n = node_new(isp ? N_PD : N_FD, ll), *sp = node_new(N_FS, ll);
        sp->subprogram.name = string_duplicate(msn);
        for (int i = 1; i < pc; i++)
        {
          Syntax_Node *p = node_new(N_PM, ll);
          p->parameter.name = STRING_LITERAL("p");
          nv(&sp->subprogram.parameters, p);
        }
        sp->subprogram.return_type = 0;
        n->body.subprogram_spec = sp;
        Symbol *s = symbol_add_overload(symbol_manager, symbol_new(msn, isp ? 4 : 5, type_new(TYPE_STRING, msn), n));
        s->elaboration_level = symbol_manager->eo++;
        nv(&s->overloads, n);
        n->symbol = s;
        s->mangled_name = string_duplicate(sn);
      }
    }
    while (*l and *l != '\n')
      l++;
    if (*l)
      l++;
  }
  free(ali);
}
static const char *lookup_path(Symbol_Manager *symbol_manager, String_Slice nm)
{
  char pf[256], af[512];
  for (int i = 0; i < include_path_count; i++)
  {
    snprintf(
        pf,
        256,
        "%s%s%.*s",
        include_paths[i],
        include_paths[i][0] and include_paths[i][strlen(include_paths[i]) - 1] != '/' ? "/" : "",
        (int) nm.length,
        nm.string);
    for (char *p = pf + strlen(include_paths[i]); *p; p++)
      *p = tolower(*p);
    read_ada_library_interface(symbol_manager, pf);
    snprintf(af, 512, "%s.ads", pf);
    const char *s = read_file(af);
    if (s)
      return s;
  }
  return 0;
}
static Syntax_Node *pks2(Symbol_Manager *symbol_manager, String_Slice nm, const char *src)
{
  if (not src)
    return 0;
  char af[512];
  snprintf(af, 512, "%.*s.ads", (int) nm.length, nm.string);
  Parser p = parser_new(src, strlen(src), af);
  Syntax_Node *cu = parse_compilation_unit(&p);
  if (cu and cu->compilation_unit.context)
  {
    for (uint32_t i = 0; i < cu->compilation_unit.context->context.with_clauses.count; i++)
      pks2(symbol_manager, cu->compilation_unit.context->context.with_clauses.data[i]->with_clause.name, lookup_path(symbol_manager, cu->compilation_unit.context->context.with_clauses.data[i]->with_clause.name));
  }
  for (uint32_t i = 0; cu and i < cu->compilation_unit.units.count; i++)
  {
    Syntax_Node *u = cu->compilation_unit.units.data[i];
    if (u->k == N_PKS)
    {
      Type_Info *t = type_new(TY_P, nm);
      Symbol *ps = symbol_add_overload(symbol_manager, symbol_new(nm, 6, t, u));
      ps->level = 0;
      u->symbol = ps;
      u->package_spec.elaboration_level = ps->elaboration_level;
      Syntax_Node *oldpk = symbol_manager->pk;
      int oldlv = symbol_manager->lv;
      symbol_manager->pk = u;
      symbol_manager->lv = 0;
      for (uint32_t j = 0; j < u->package_spec.declarations.count; j++)
        resolve_declaration(symbol_manager, u->package_spec.declarations.data[j]);
      for (uint32_t j = 0; j < u->package_spec.private_declarations.count; j++)
        resolve_declaration(symbol_manager, u->package_spec.private_declarations.data[j]);
      symbol_manager->lv = oldlv;
      symbol_manager->pk = oldpk;
    }
    else if (u->k == N_GEN)
      resolve_declaration(symbol_manager, u);
  }
  return cu;
}
static void parse_package_specification(Symbol_Manager *symbol_manager, String_Slice nm, const char *src)
{
  Symbol *ps = symbol_find(symbol_manager, nm);
  if (ps and ps->k == 6)
    return;
  pks2(symbol_manager, nm, src);
}
static void symbol_manager_use_clauses(Symbol_Manager *symbol_manager, Syntax_Node *n)
{
  if (n->k != N_CU)
    return;
  for (uint32_t i = 0; i < n->compilation_unit.context->context.with_clauses.count; i++)
    parse_package_specification(symbol_manager, n->compilation_unit.context->context.with_clauses.data[i]->with_clause.name, lookup_path(symbol_manager, n->compilation_unit.context->context.with_clauses.data[i]->with_clause.name));
  for (uint32_t i = 0; i < n->compilation_unit.context->context.use_clauses.count; i++)
  {
    Syntax_Node *u = n->compilation_unit.context->context.use_clauses.data[i];
    if (u and u->k == N_US and u->use_clause.nm and u->use_clause.nm->k == N_ID)
    {
      Symbol *ps = symbol_find(symbol_manager, u->use_clause.nm->string_value);
      if (ps and ps->k == 6 and ps->definition and ps->definition->k == N_PKS)
      {
        Syntax_Node *pk = ps->definition;
        for (uint32_t j = 0; j < pk->package_spec.declarations.count; j++)
        {
          Syntax_Node *d = pk->package_spec.declarations.data[j];
          if (d->k == N_ED)
          {
            for (uint32_t k = 0; k < d->exception_decl.identifiers.count; k++)
              if (d->exception_decl.identifiers.data[k]->symbol)
              {
                d->exception_decl.identifiers.data[k]->symbol->visibility |= 2;
                sv(&symbol_manager->uv, d->exception_decl.identifiers.data[k]->symbol);
              }
          }
          else if (d->k == N_OD)
          {
            for (uint32_t k = 0; k < d->object_decl.identifiers.count; k++)
              if (d->object_decl.identifiers.data[k]->symbol)
              {
                d->object_decl.identifiers.data[k]->symbol->visibility |= 2;
                sv(&symbol_manager->uv, d->object_decl.identifiers.data[k]->symbol);
              }
          }
          else if (d->symbol)
          {
            d->symbol->visibility |= 2;
            sv(&symbol_manager->uv, d->symbol);
          }
        }
      }
    }
  }
  for (uint32_t i = 0; i < n->compilation_unit.units.count; i++)
  {
    Symbol_Vector eo = {0};
    int mx = 0;
    if (n->compilation_unit.units.data[i]->k == N_PKS)
      for (uint32_t j = 0; j < n->compilation_unit.units.data[i]->package_spec.declarations.count; j++)
      {
        int e = elaborate_compilation(symbol_manager, &eo, n->compilation_unit.units.data[i]->package_spec.declarations.data[j]);
        mx = e > mx ? e : mx;
      }
    else if (n->compilation_unit.units.data[i]->k == N_PKB)
      for (uint32_t j = 0; j < n->compilation_unit.units.data[i]->package_body.declarations.count; j++)
      {
        int e = elaborate_compilation(symbol_manager, &eo, n->compilation_unit.units.data[i]->package_body.declarations.data[j]);
        mx = e > mx ? e : mx;
      }
    for (uint32_t j = 0; j < eo.count; j++)
      if (eo.data[j]->k == 6 and eo.data[j]->definition and eo.data[j]->definition->k == N_PKS)
        for (uint32_t k = 0; k < ((Syntax_Node *) eo.data[j]->definition)->package_spec.declarations.count; k++)
          resolve_declaration(symbol_manager, ((Syntax_Node *) eo.data[j]->definition)->package_spec.declarations.data[k]);
    resolve_declaration(symbol_manager, n->compilation_unit.units.data[i]);
  }
}
typedef enum
{
  VALUE_KIND_INTEGER = 0,
  VALUE_KIND_FLOAT = 1,
  VALUE_KIND_POINTER = 2
} Value_Kind;
typedef struct
{
  int id;
  Value_Kind k;
} Value;
typedef struct Exception_Handler Exception_Handler;
typedef struct Task Task;
typedef struct Protected_Type Protected_Type;
struct Exception_Handler
{
  String_Slice ec;
  jmp_buf jb;
  Exception_Handler *nx;
};
struct Task
{
  pthread_t th;
  int id;
  String_Slice nm;
  Node_Vector en;
  Protected_Type *pt;
  bool ac, tm;
};
struct Protected_Type
{
  pthread_mutex_t mx;
  String_Slice nm;
  Node_Vector en;
  bool lk;
};
typedef struct
{
  FILE *o;
  int tm, lb, md;
  Symbol_Manager *sm;
  int ll[64];
  int ls;
  Symbol_Vector el;
  Exception_Handler *eh;
  Task *tk[64];
  int tn;
  Protected_Type *pt[64];
  int pn;
  String_List_Vector lbs;
  String_List_Vector exs;
  String_List_Vector dcl;
  Label_Entry_Vector ltb;
  uint8_t lopt[64];
  Syntax_Node *current_function;
} Code_Generator;
static int new_temporary_register(Code_Generator *generator)
{
  return generator->tm++;
}
static int new_label_block(Code_Generator *generator)
{
  return generator->lb++;
}
static int normalize_name(Code_Generator *generator)
{
  return ++generator->md;
}
static void emit_loop_metadata(FILE *o, int id)
{
  fprintf(o, ", !llvm.loop !%d", id);
}
static void emit_all_metadata(Code_Generator *generator)
{
  for (int i = 1; i <= generator->md; i++)
  {
    fprintf(generator->o, "!%d = distinct !{!%d", i, i);
    if (i < 64 and (generator->lopt[i] & 1))
      fprintf(generator->o, ", !%d", generator->md + 1);
    if (i < 64 and (generator->lopt[i] & 2))
      fprintf(generator->o, ", !%d", generator->md + 2);
    if (i < 64 and (generator->lopt[i] & 4))
      fprintf(generator->o, ", !%d", generator->md + 3);
    fprintf(generator->o, "}\n");
  }
  if (generator->md > 0)
  {
    fprintf(generator->o, "!%d = !{!\"llvm.loop.unroll.enable\"}\n", generator->md + 1);
    fprintf(generator->o, "!%d = !{!\"llvm.loop.vectorize.enable\"}\n", generator->md + 2);
    fprintf(generator->o, "!%d = !{!\"llvm.loop.distribute.enable\"}\n", generator->md + 3);
  }
}
static int find_label(Code_Generator *generator, String_Slice lb)
{
  for (uint32_t i = 0; i < generator->lbs.count; i++)
    if (string_equal_ignore_case(generator->lbs.data[i], lb))
      return i;
  return -1;
}
static void emit_exception(Code_Generator *generator, String_Slice ex)
{
  for (uint32_t i = 0; i < generator->exs.count; i++)
    if (string_equal_ignore_case(generator->exs.data[i], ex))
      return;
  slv(&generator->exs, ex);
}
static inline void emit_label(Code_Generator *generator, int l);
static int get_or_create_label_basic_block(Code_Generator *generator, String_Slice nm)
{
  for (uint32_t i = 0; i < generator->ltb.count; i++)
    if (string_equal_ignore_case(generator->ltb.data[i]->name, nm))
      return generator->ltb.data[i]->basic_block;
  Label_Entry *e = malloc(sizeof(Label_Entry));
  e->name = nm;
  e->basic_block = new_label_block(generator);
  lev(&generator->ltb, e);
  return e->basic_block;
}
static void emit_label_definition(Code_Generator *generator, String_Slice nm)
{
  int bb = get_or_create_label_basic_block(generator, nm);
  emit_label(generator, bb);
}
static bool add_declaration(Code_Generator *generator, const char *fn)
{
  String_Slice fns = {fn, strlen(fn)};
  for (uint32_t i = 0; i < generator->dcl.count; i++)
    if (string_equal_ignore_case(generator->dcl.data[i], fns))
      return false;
  char *cp = malloc(fns.length + 1);
  memcpy(cp, fn, fns.length);
  cp[fns.length] = 0;
  slv(&generator->dcl, (String_Slice){cp, fns.length});
  return true;
}
static const char *value_llvm_type_string(Value_Kind k)
{
  return k == VALUE_KIND_INTEGER ? "i64" : k == VALUE_KIND_FLOAT ? "double" : "ptr";
}
static const char *ada_to_c_type_string(Type_Info *t)
{
  if (not t)
    return "i32";
  Type_Info *tc = type_canonical_concrete(t);
  if (not tc)
    return "i32";
  switch (tc->k)
  {
  case TYPE_BOOLEAN:
  case TYPE_CHARACTER:
  case TYPE_INTEGER:
  case TYPE_UNSIGNED_INTEGER:
  case TYPE_ENUMERATION:
  case TYPE_DERIVED:
  {
    int64_t lo = tc->low_bound, hi = tc->high_bound;
    if (lo == 0 and hi == 0)
      return "i32";
    if (lo >= 0)
    {
      if (hi < 256)
        return "i8";
      if (hi < 65536)
        return "i16";
    }
    else
    {
      if (lo >= -128 and hi <= 127)
        return "i8";
      if (lo >= -32768 and hi <= 32767)
        return "i16";
    }
    return "i32";
  }
  case TYPE_FLOAT:
  case TYPE_UNIVERSAL_FLOAT:
  case TYPE_FIXED_POINT:
    return tc->size == 32 ? "float" : "double";
  case TYPE_ACCESS:
  case TYPE_FAT_POINTER:
  case TYPE_STRING:
    return "ptr";
  default:
    return "i32";
  }
}
static Value_Kind token_kind_to_value_kind(Type_Info *t)
{
  if (not t)
    return VALUE_KIND_INTEGER;
  switch (representation_category(t))
  {
  case REPR_CAT_FLOAT:
    return VALUE_KIND_FLOAT;
  case REPR_CAT_POINTER:
    return VALUE_KIND_POINTER;
  default:
    return VALUE_KIND_INTEGER;
  }
}
static unsigned long type_hash(Type_Info *t)
{
  if (not t)
    return 0;
  unsigned long h = t->k;
  if (t->k == TYPE_ARRAY)
  {
    h = h * 31 + (unsigned long) t->low_bound;
    h = h * 31 + (unsigned long) t->high_bound;
    h = h * 31 + type_hash(t->element_type);
  }
  else if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->discriminants.count and i < 8; i++)
      if (t->discriminants.data[i])
        h = h * 31;
  }
  else
  {
    h = h * 31 + (unsigned long) t->low_bound;
    h = h * 31 + (unsigned long) t->high_bound;
  }
  return h;
}
static int encode_symbol_name(char *b, int sz, Symbol *s, String_Slice nm, int pc, Syntax_Node *sp)
{
  if (s and s->is_external and s->external_name.string)
  {
    int n = 0;
    for (uint32_t i = 0; i < s->external_name.length and i < sz - 1; i++)
      b[n++] = s->external_name.string[i];
    b[n] = 0;
    return n;
  }
  int n = 0;
  unsigned long uid = s ? s->uid : 0;
  if (s and s->parent and s->parent->name.string and (uintptr_t) s->parent->name.string > 4096)
  {
    for (uint32_t i = 0; i < s->parent->name.length and i < 256; i++)
    {
      char c = s->parent->name.string[i];
      if (not c)
        break;
      if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9'))
        b[n++] = toupper(c);
      else
        n += snprintf(b + n, sz - n, "_%02X", (unsigned char) c);
    }
    b[n++] = '_';
    b[n++] = '_';
    if (nm.string and (uintptr_t) nm.string > 4096)
    {
      for (uint32_t i = 0; i < nm.length and i < 256; i++)
      {
        char c = nm.string[i];
        if (not c)
          break;
        if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9'))
          b[n++] = toupper(c);
        else
          n += snprintf(b + n, sz - n, "_%02X", (unsigned char) c);
      }
    }
    if (sp and sp->subprogram.parameters.count > 0 and sp->subprogram.parameters.count < 64)
    {
      unsigned long h = 0, pnh = 0;
      for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
      {
        Syntax_Node *p = sp->subprogram.parameters.data[i];
        if (p and p->parameter.ty)
          h = h * 31 + type_hash(p->parameter.ty->ty ? p->parameter.ty->ty : 0);
        if (p and p->parameter.name.string)
          pnh = pnh * 31 + string_hash(p->parameter.name);
      }
      n += snprintf(b + n, sz - n, ".%d.%lx.%lu.%lx", pc, h % 0x10000, uid, pnh % 0x10000);
    }
    else
    {
      n += snprintf(b + n, sz - n, ".%d.%lu.1", pc, uid);
    }
    b[n] = 0;
    return n;
  }
  if (nm.string and (uintptr_t) nm.string > 4096)
  {
    for (uint32_t i = 0; i < nm.length and i < 256; i++)
    {
      char c = nm.string[i];
      if (not c)
        break;
      if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '_')
        b[n++] = c;
      else
        n += snprintf(b + n, sz - n, "_%02X", (unsigned char) c);
    }
  }
  if (sp and sp->subprogram.parameters.count > 0 and sp->subprogram.parameters.count < 64)
  {
    unsigned long h = 0, pnh = 0;
    for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
    {
      Syntax_Node *p = sp->subprogram.parameters.data[i];
      if (p and p->parameter.ty)
        h = h * 31 + type_hash(p->parameter.ty->ty ? p->parameter.ty->ty : 0);
      if (p and p->parameter.name.string)
        pnh = pnh * 31 + string_hash(p->parameter.name);
    }
    n += snprintf(b + n, sz - n, ".%d.%lx.%lu.%lx", pc, h % 0x10000, uid, pnh % 0x10000);
  }
  else
  {
    n += snprintf(b + n, sz - n, ".%d.%lu.1", pc, uid);
  }
  b[n] = 0;
  return n;
}
static bool has_nested_function(Node_Vector *dc, Node_Vector *st);
static bool has_nested_function_in_stmts(Node_Vector *st)
{
  for (uint32_t i = 0; i < st->count; i++)
  {
    Syntax_Node *n = st->data[i];
    if (not n)
      continue;
    if (n->k == N_BL and has_nested_function(&n->block.declarations, &n->block.statements))
      return 1;
    if (n->k == N_IF)
    {
      if (has_nested_function_in_stmts(&n->if_stmt.then_statements) or has_nested_function_in_stmts(&n->if_stmt.else_statements))
        return 1;
      for (uint32_t j = 0; j < n->if_stmt.elsif_statements.count; j++)
        if (n->if_stmt.elsif_statements.data[j] and has_nested_function_in_stmts(&n->if_stmt.elsif_statements.data[j]->if_stmt.then_statements))
          return 1;
    }
    if (n->k == N_CS)
    {
      for (uint32_t j = 0; j < n->case_stmt.alternatives.count; j++)
        if (n->case_stmt.alternatives.data[j] and has_nested_function_in_stmts(&n->case_stmt.alternatives.data[j]->exception_handler.statements))
          return 1;
    }
    if (n->k == N_LP and has_nested_function_in_stmts(&n->loop_stmt.statements))
      return 1;
  }
  return 0;
}
static bool has_nested_function(Node_Vector *dc, Node_Vector *st)
{
  for (uint32_t i = 0; i < dc->count; i++)
    if (dc->data[i] and (dc->data[i]->k == N_PB or dc->data[i]->k == N_FB))
      return 1;
  return has_nested_function_in_stmts(st);
}
static Value generate_expression(Code_Generator *generator, Syntax_Node *n);
static void generate_statement_sequence(Code_Generator *generator, Syntax_Node *n);
static void generate_block_frame(Code_Generator *generator)
{
  int mx = 0;
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = generator->sm->sy[h]; s; s = s->next)
      if (s->k == 0 and s->elaboration_level >= 0 and s->elaboration_level > mx)
        mx = s->elaboration_level;
  if (mx > 0)
    fprintf(generator->o, "  %%__frame = alloca [%d x ptr]\n", mx + 1);
}
static void generate_declaration(Code_Generator *generator, Syntax_Node *n);
static inline void emit_label(Code_Generator *generator, int l)
{
  fprintf(generator->o, "Source_Location%d:\n", l);
}
static inline void emit_branch(Code_Generator *generator, int l)
{
  fprintf(generator->o, "  br label %%Source_Location%d\n", l);
}
static inline void emit_conditional_branch(Code_Generator *generator, int c, int lt, int lf)
{
  fprintf(generator->o, "  br i1 %%t%d, label %%Source_Location%d, label %%Source_Location%d\n", c, lt, lf);
}
static void
generate_index_constraint_check(Code_Generator *generator, int idx, const char *lo_s, const char *hi_s)
{
  FILE *o = generator->o;
  int lok = new_label_block(generator), hik = new_label_block(generator), erl = new_label_block(generator),
      dn = new_label_block(generator), lc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %s\n", lc, idx, lo_s);
  emit_conditional_branch(generator, lc, lok, erl);
  emit_label(generator, lok);
  int hc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %s\n", hc, idx, hi_s);
  emit_conditional_branch(generator, hc, hik, erl);
  emit_label(generator, hik);
  emit_branch(generator, dn);
  emit_label(generator, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
  emit_label(generator, dn);
}
static Value value_cast(Code_Generator *generator, Value v, Value_Kind k)
{
  if (v.k == k)
    return v;
  Value r = {new_temporary_register(generator), k};
  if (v.k == VALUE_KIND_INTEGER and k == VALUE_KIND_FLOAT)
    fprintf(generator->o, "  %%t%d = sitofp i64 %%t%d to double\n", r.id, v.id);
  else if (v.k == VALUE_KIND_FLOAT and k == VALUE_KIND_INTEGER)
    fprintf(generator->o, "  %%t%d = fptosi double %%t%d to i64\n", r.id, v.id);
  else if (v.k == VALUE_KIND_POINTER and k == VALUE_KIND_INTEGER)
    fprintf(generator->o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", r.id, v.id);
  else if (v.k == VALUE_KIND_INTEGER and k == VALUE_KIND_POINTER)
    fprintf(generator->o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", r.id, v.id);
  else if (v.k == VALUE_KIND_POINTER and k == VALUE_KIND_FLOAT)
  {
    int tmp = new_temporary_register(generator);
    fprintf(generator->o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", tmp, v.id);
    fprintf(generator->o, "  %%t%d = sitofp i64 %%t%d to double\n", r.id, tmp);
  }
  else if (v.k == VALUE_KIND_FLOAT and k == VALUE_KIND_POINTER)
  {
    int tmp = new_temporary_register(generator);
    fprintf(generator->o, "  %%t%d = fptosi double %%t%d to i64\n", tmp, v.id);
    fprintf(generator->o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", r.id, tmp);
  }
  else
    fprintf(
        generator->o,
        "  %%t%d = bitcast %s %%t%d to %s\n",
        r.id,
        value_llvm_type_string(v.k),
        v.id,
        value_llvm_type_string(k));
  return r;
}
static Value
generate_float_range_check(Code_Generator *generator, Value e, Type_Info *t, String_Slice ec, Value_Kind rk)
{
  if (not t or (t->low_bound == 0 and t->high_bound == 0))
    return value_cast(generator, e, rk);
  FILE *o = generator->o;
  Value ef = value_cast(generator, e, VALUE_KIND_FLOAT);
  union
  {
    int64_t i;
    double d;
  } ulo, uhi;
  ulo.i = t->low_bound;
  uhi.i = t->high_bound;
  int lok = new_label_block(generator), hik = new_label_block(generator), erl = new_label_block(generator),
      dn = new_label_block(generator), lc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = fcmp oge double %%t%d, %e\n", lc, ef.id, ulo.d);
  emit_conditional_branch(generator, lc, lok, erl);
  emit_label(generator, lok);
  int hc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = fcmp ole double %%t%d, %e\n", hc, ef.id, uhi.d);
  emit_conditional_branch(generator, hc, hik, erl);
  emit_label(generator, hik);
  emit_branch(generator, dn);
  emit_label(generator, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.%.*s)\n", (int) ec.length, ec.string);
  fprintf(o, "  unreachable\n");
  emit_label(generator, dn);
  return value_cast(generator, e, rk);
}
static Value generate_array_bounds_check(
    Code_Generator *generator, Value e, Type_Info *t, Type_Info *et, String_Slice ec, Value_Kind rk)
{
  FILE *o = generator->o;
  int lok = new_label_block(generator), hik = new_label_block(generator), erl = new_label_block(generator),
      dn = new_label_block(generator), tlo = new_temporary_register(generator);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, (long long) t->low_bound);
  int thi = new_temporary_register(generator);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", thi, (long long) t->high_bound);
  int elo = new_temporary_register(generator);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", elo, et ? (long long) et->low_bound : 0LL);
  int ehi = new_temporary_register(generator);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", ehi, et ? (long long) et->high_bound : -1LL);
  int lc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", lc, elo, tlo);
  emit_conditional_branch(generator, lc, lok, erl);
  emit_label(generator, lok);
  int hc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", hc, ehi, thi);
  emit_conditional_branch(generator, hc, hik, erl);
  emit_label(generator, hik);
  emit_branch(generator, dn);
  emit_label(generator, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.%.*s)\n", (int) ec.length, ec.string);
  fprintf(o, "  unreachable\n");
  emit_label(generator, dn);
  return value_cast(generator, e, rk);
}
static Value
generate_discrete_range_check(Code_Generator *generator, Value e, Type_Info *t, String_Slice ec, Value_Kind rk)
{
  FILE *o = generator->o;
  int lok = new_label_block(generator), hik = new_label_block(generator), erl = new_label_block(generator),
      dn = new_label_block(generator), lc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %lld\n", lc, e.id, (long long) t->low_bound);
  emit_conditional_branch(generator, lc, lok, erl);
  emit_label(generator, lok);
  int hc = new_temporary_register(generator);
  fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %lld\n", hc, e.id, (long long) t->high_bound);
  emit_conditional_branch(generator, hc, hik, erl);
  emit_label(generator, hik);
  emit_branch(generator, dn);
  emit_label(generator, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.%.*s)\n", (int) ec.length, ec.string);
  fprintf(o, "  unreachable\n");
  emit_label(generator, dn);
  return value_cast(generator, e, rk);
}
static Value value_to_boolean(Code_Generator *generator, Value v)
{
  if (v.k != VALUE_KIND_INTEGER)
    v = value_cast(generator, v, VALUE_KIND_INTEGER);
  int t = new_temporary_register(generator);
  Value c = {new_temporary_register(generator), VALUE_KIND_INTEGER};
  fprintf(generator->o, "  %%t%d = icmp ne i64 %%t%d, 0\n", t, v.id);
  fprintf(generator->o, "  %%t%d = zext i1 %%t%d to i64\n", c.id, t);
  return c;
}
static Value value_compare(Code_Generator *generator, const char *op, Value a, Value b, Value_Kind k)
{
  a = value_cast(generator, a, k);
  b = value_cast(generator, b, k);
  int c = new_temporary_register(generator);
  Value r = {new_temporary_register(generator), VALUE_KIND_INTEGER};
  if (k == VALUE_KIND_INTEGER)
    fprintf(generator->o, "  %%t%d = icmp %s i64 %%t%d, %%t%d\n", c, op, a.id, b.id);
  else
    fprintf(generator->o, "  %%t%d = fcmp %s double %%t%d, %%t%d\n", c, op, a.id, b.id);
  fprintf(generator->o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, c);
  return r;
}
static Value value_compare_integer(Code_Generator *generator, const char *op, Value a, Value b)
{
  return value_compare(generator, op, a, b, VALUE_KIND_INTEGER);
}
static Value value_compare_float(Code_Generator *generator, const char *op, Value a, Value b)
{
  return value_compare(generator, op, a, b, VALUE_KIND_FLOAT);
}
static void generate_fat_pointer(Code_Generator *generator, int fp, int d, int lo, int hi)
{
  FILE *o = generator->o;
  fprintf(o, "  %%t%d = alloca {ptr,ptr}\n", fp);
  int bd = new_temporary_register(generator);
  fprintf(o, "  %%t%d = alloca {i64,i64}\n", bd);
  fprintf(o, "  %%_lo%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 0\n", fp, bd);
  fprintf(o, "  store i64 %%t%d, ptr %%_lo%d\n", lo, fp);
  fprintf(o, "  %%_hi%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 1\n", fp, bd);
  fprintf(o, "  store i64 %%t%d, ptr %%_hi%d\n", hi, fp);
  int dp = new_temporary_register(generator);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 0\n", dp, fp);
  fprintf(o, "  store ptr %%t%d, ptr %%t%d\n", d, dp);
  int bp = new_temporary_register(generator);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 1\n", bp, fp);
  fprintf(o, "  store ptr %%t%d, ptr %%t%d\n", bd, bp);
}
static Value get_fat_pointer_data(Code_Generator *generator, int fp)
{
  Value r = {new_temporary_register(generator), VALUE_KIND_POINTER};
  FILE *o = generator->o;
  int dp = new_temporary_register(generator);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 0\n", dp, fp);
  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", r.id, dp);
  return r;
}
static void get_fat_pointer_bounds(Code_Generator *generator, int fp, int *lo, int *hi)
{
  FILE *o = generator->o;
  int bp = new_temporary_register(generator);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 1\n", bp, fp);
  int bv = new_temporary_register(generator);
  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", bv, bp);
  *lo = new_temporary_register(generator);
  fprintf(o, "  %%t%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 0\n", *lo, bv);
  int lov = new_temporary_register(generator);
  fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lov, *lo);
  *lo = lov;
  *hi = new_temporary_register(generator);
  fprintf(o, "  %%t%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 1\n", *hi, bv);
  int hiv = new_temporary_register(generator);
  fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", hiv, *hi);
  *hi = hiv;
}
static Value value_power(Code_Generator *generator, Value a, Value b, Value_Kind k)
{
  a = value_cast(generator, a, k);
  b = value_cast(generator, b, k);
  Value r = {new_temporary_register(generator), k};
  if (k == VALUE_KIND_INTEGER)
    fprintf(generator->o, "  %%t%d = call i64 @__ada_powi(i64 %%t%d, i64 %%t%d)\n", r.id, a.id, b.id);
  else
    fprintf(generator->o, "  %%t%d = call double @pow(double %%t%d, double %%t%d)\n", r.id, a.id, b.id);
  return r;
}
static Value value_power_integer(Code_Generator *generator, Value a, Value b)
{
  return value_power(generator, a, b, VALUE_KIND_INTEGER);
}
static Value value_power_float(Code_Generator *generator, Value a, Value b)
{
  return value_power(generator, a, b, VALUE_KIND_FLOAT);
}
static Value generate_aggregate(Code_Generator *generator, Syntax_Node *n, Type_Info *ty)
{
  FILE *o = generator->o;
  Value r = {new_temporary_register(generator), VALUE_KIND_POINTER};
  Type_Info *t = ty ? type_canonical_concrete(ty) : 0;
  if (t and t->k == TYPE_ARRAY and n->k == N_AG)
    normalize_array_aggregate(generator->sm, t, n);
  if (t and t->k == TYPE_RECORD and n->k == N_AG)
    normalize_record_aggregate(generator->sm, t, n);
  if (not t or t->k != TYPE_RECORD or t->is_packed)
  {
    int sz = n->aggregate.items.count ? n->aggregate.items.count : 1;
    int p = new_temporary_register(generator);
    int by = new_temporary_register(generator);
    fprintf(o, "  %%t%d = add i64 0, %d\n", by, sz * 8);
    fprintf(o, "  %%t%d = call ptr @__ada_ss_allocate(i64 %%t%d)\n", p, by);
    uint32_t ix = 0;
    for (uint32_t i = 0; i < n->aggregate.items.count; i++)
    {
      Syntax_Node *el = n->aggregate.items.data[i];
      if (el->k == N_ASC)
      {
        if (el->association.choices.data[0]->k == N_ID
            and string_equal_ignore_case(el->association.choices.data[0]->string_value, STRING_LITERAL("others")))
        {
          for (; ix < (uint32_t) sz; ix++)
          {
            Value v = value_cast(generator, generate_expression(generator, el->association.value), VALUE_KIND_INTEGER);
            int ep = new_temporary_register(generator);
            fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p, ix);
            fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
          }
        }
        else
        {
          Value v = value_cast(generator, generate_expression(generator, el->association.value), VALUE_KIND_INTEGER);
          for (uint32_t j = 0; j < el->association.choices.count; j++)
          {
            Syntax_Node *ch = el->association.choices.data[j];
            if (ch->k == N_ID and ch->symbol and ch->symbol->k == 1 and ch->symbol->type_info)
            {
              Type_Info *cht = type_canonical_concrete(ch->symbol->type_info);
              if (cht->k == TYPE_ENUMERATION)
              {
                for (uint32_t ei = 0; ei < cht->enum_values.count; ei++)
                {
                  Value cv = {new_temporary_register(generator), VALUE_KIND_INTEGER};
                  fprintf(o, "  %%t%d = add i64 0, %ld\n", cv.id, (long) cht->enum_values.data[ei]->value);
                  int ep = new_temporary_register(generator);
                  fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", ep, p, cv.id);
                  fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
                }
              }
              else if ((cht->low_bound != 0 or cht->high_bound != 0) and cht->k == TYPE_INTEGER)
              {
                for (int64_t ri = cht->low_bound; ri <= cht->high_bound; ri++)
                {
                  Value cv = {new_temporary_register(generator), VALUE_KIND_INTEGER};
                  fprintf(o, "  %%t%d = add i64 0, %ld\n", cv.id, (long) ri);
                  int ep = new_temporary_register(generator);
                  fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", ep, p, cv.id);
                  fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
                }
              }
            }
            else
            {
              Value ci = value_cast(generator, generate_expression(generator, ch), VALUE_KIND_INTEGER);
              int ep = new_temporary_register(generator);
              fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", ep, p, ci.id);
              fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
            }
          }
          ix++;
        }
      }
      else
      {
        Value v = value_cast(generator, generate_expression(generator, el), VALUE_KIND_INTEGER);
        int ep = new_temporary_register(generator);
        fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p, ix);
        fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
        ix++;
      }
    }
    r.id = p;
  }
  else
  {
    uint32_t sz = t->size / 8;
    int p = new_temporary_register(generator);
    int by = new_temporary_register(generator);
    fprintf(o, "  %%t%d = add i64 0, %u\n", by, sz * 8);
    fprintf(o, "  %%t%d = call ptr @__ada_ss_allocate(i64 %%t%d)\n", p, by);
    uint32_t ix = 0;
    for (uint32_t i = 0; i < n->aggregate.items.count; i++)
    {
      Syntax_Node *el = n->aggregate.items.data[i];
      if (el->k == N_ASC)
      {
        for (uint32_t j = 0; j < el->association.choices.count; j++)
        {
          Syntax_Node *ch = el->association.choices.data[j];
          if (ch->k == N_ID)
          {
            for (uint32_t k = 0; k < t->components.count; k++)
            {
              Syntax_Node *c = t->components.data[k];
              if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, ch->string_value))
              {
                Value v = value_cast(generator, generate_expression(generator, el->association.value), VALUE_KIND_INTEGER);
                int ep = new_temporary_register(generator);
                fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p, c->component_decl.offset);
                fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
                break;
              }
            }
          }
        }
        ix++;
      }
      else
      {
        Value v = value_cast(generator, generate_expression(generator, el), VALUE_KIND_INTEGER);
        int ep = new_temporary_register(generator);
        fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p, ix);
        fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
        ix++;
      }
    }
    r.id = p;
  }
  return r;
}
static Syntax_Node *symbol_body(Symbol *s, int el)
{
  if (not s or s->overloads.count == 0)
    return 0;
  for (uint32_t i = 0; i < s->overloads.count; i++)
  {
    Syntax_Node *b = s->overloads.data[i];
    if ((b->k == N_PB or b->k == N_FB) and b->body.elaboration_level == el)
      return b;
  }
  return 0;
}
static Syntax_Node *symbol_spec(Symbol *s)
{
  if (not s or s->overloads.count == 0)
    return 0;
  Syntax_Node *b = symbol_body(s, s->elaboration_level);
  if (b and b->body.subprogram_spec)
    return b->body.subprogram_spec;
  for (uint32_t i = 0; i < s->overloads.count; i++)
  {
    Syntax_Node *d = s->overloads.data[i];
    if (d->k == N_PD or d->k == N_FD)
      return d->body.subprogram_spec;
  }
  return 0;
}
static const char *get_attribute_name(String_Slice attr, String_Slice tnm)
{
  static char fnm[256];
  int pos = 0;
  pos += snprintf(fnm + pos, 256 - pos, "@__attr_");
  for (uint32_t i = 0; i < attr.length and pos < 250; i++)
    fnm[pos++] = attr.string[i];
  fnm[pos++] = '_';
  for (uint32_t i = 0; i < tnm.length and pos < 255; i++)
    fnm[pos++] = toupper(tnm.string[i]);
  fnm[pos] = 0;
  return fnm;
}
static Value generate_expression(Code_Generator *generator, Syntax_Node *n)
{
  FILE *o = generator->o;
  if (not n)
    return (Value){0, VALUE_KIND_INTEGER};
  Value r = {new_temporary_register(generator), token_kind_to_value_kind(n->ty)};
  switch (n->k)
  {
  case N_INT:
    r.k = VALUE_KIND_INTEGER;
    fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) n->integer_value);
    break;
  case N_REAL:
    r.k = VALUE_KIND_FLOAT;
    fprintf(o, "  %%t%d = fadd double 0.0, %e\n", r.id, n->float_value);
    break;
  case N_CHAR:
    r.k = VALUE_KIND_INTEGER;
    fprintf(o, "  %%t%d = add i64 0, %d\n", r.id, (int) n->integer_value);
    break;
  case N_STR:
    r.k = VALUE_KIND_POINTER;
    {
      int p = new_temporary_register(generator);
      uint32_t sz = n->string_value.length + 1;
      fprintf(o, "  %%t%d = alloca [%u x i8]\n", p, sz);
      for (uint32_t i = 0; i < n->string_value.length; i++)
      {
        int ep = new_temporary_register(generator);
        fprintf(o, "  %%t%d = getelementptr [%u x i8], ptr %%t%d, i64 0, i64 %u\n", ep, sz, p, i);
        fprintf(o, "  store i8 %d, ptr %%t%d\n", (int) (unsigned char) n->string_value.string[i], ep);
      }
      int zp = new_temporary_register(generator);
      fprintf(
          o, "  %%t%d = getelementptr [%u x i8], ptr %%t%d, i64 0, i64 %u\n", zp, sz, p, n->string_value.length);
      fprintf(o, "  store i8 0, ptr %%t%d\n", zp);
      int dp = new_temporary_register(generator);
      fprintf(o, "  %%t%d = getelementptr [%u x i8], ptr %%t%d, i64 0, i64 0\n", dp, sz, p);
      int lo_id = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 0, 1\n", lo_id);
      int hi_id = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 0, %u\n", hi_id, n->string_value.length);
      r.id = new_temporary_register(generator);
      generate_fat_pointer(generator, r.id, dp, lo_id, hi_id);
    }
    break;
  case N_NULL:
    r.k = VALUE_KIND_POINTER;
    fprintf(o, "  %%t%d = inttoptr i64 0 to ptr\n", r.id);
    break;
  case N_ID:
  {
    Symbol *s = n->symbol ? n->symbol : symbol_find(generator->sm, n->string_value);
    if (not s and n->symbol)
    {
      s = n->symbol;
    }
    int gen_0p_call = 0;
    Value_Kind fn_ret_type = r.k;
    if (s and s->k == 5)
    {
      Symbol *s0 = symbol_find_with_arity(generator->sm, n->string_value, 0, n->ty);
      if (s0)
      {
        s = s0;
        gen_0p_call = 1;
      }
    }
    if (s and s->k == 2
        and not(s->type_info and is_unconstrained_array(type_canonical_concrete(s->type_info)) and s->level > 0))
    {
      if (s->definition and s->definition->k == N_STR)
      {
        r.k = VALUE_KIND_POINTER;
        char nb[256];
        if (s->parent and (uintptr_t) s->parent > 4096 and s->parent->name.string)
        {
          int n = 0;
          for (uint32_t j = 0; j < s->parent->name.length; j++)
            nb[n++] = toupper(s->parent->name.string[j]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
          for (uint32_t j = 0; j < s->name.length; j++)
            nb[n++] = toupper(s->name.string[j]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->name.length, s->name.string);
        fprintf(o, "  %%t%d = bitcast ptr @%s to ptr\n", r.id, nb);
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) s->value);
      }
    }
    else
    {
      Value_Kind k = VALUE_KIND_INTEGER;
      if (s and s->type_info)
        k = token_kind_to_value_kind(s->type_info);
      r.k = k;
      if (s and s->level == 0)
      {
        char nb[256];
        if (s->is_external and s->external_name.string)
        {
          snprintf(nb, 256, "%s", s->external_name.string);
        }
        else if (s->parent and (uintptr_t) s->parent > 4096 and s->parent->name.string)
        {
          int n = 0;
          for (uint32_t i = 0; i < s->parent->name.length; i++)
            nb[n++] = toupper(s->parent->name.string[i]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
          for (uint32_t i = 0; i < s->name.length; i++)
            nb[n++] = toupper(s->name.string[i]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->name.length, s->name.string);
        if (s->k == 5)
        {
          if (gen_0p_call)
          {
            char fnb[256];
            encode_symbol_name(fnb, 256, s, n->string_value, 0, 0);
            fprintf(
                o, "  %%t%d = call %s @\"%s\"()\n", r.id, value_llvm_type_string(fn_ret_type), fnb);
            r.k = fn_ret_type;
          }
          else
          {
            Syntax_Node *b = symbol_body(s, s->elaboration_level);
            Syntax_Node *sp = symbol_spec(s);
            if ((sp and sp->subprogram.parameters.count == 0) or (not b))
            {
              char fnb[256];
              encode_symbol_name(fnb, 256, s, n->string_value, 0, sp);
              fprintf(
                  o,
                  "  %%t%d = call %s @\"%s\"()\n",
                  r.id,
                  value_llvm_type_string(fn_ret_type),
                  fnb);
              r.k = fn_ret_type;
            }
            else
              fprintf(o, "  %%t%d = load %s, ptr @%s\n", r.id, value_llvm_type_string(k), nb);
          }
        }
        else
          fprintf(o, "  %%t%d = load %s, ptr @%s\n", r.id, value_llvm_type_string(k), nb);
      }
      else if (s and s->level >= 0 and s->level < generator->sm->lv)
      {
        if (s->k == 5)
        {
          Syntax_Node *b = symbol_body(s, s->elaboration_level);
          Syntax_Node *sp = symbol_spec(s);
          if (sp and sp->subprogram.parameters.count == 0)
          {
            Value_Kind rk = sp and sp->subprogram.return_type
                                ? token_kind_to_value_kind(resolve_subtype(generator->sm, sp->subprogram.return_type))
                                : VALUE_KIND_INTEGER;
            char fnb[256];
            encode_symbol_name(fnb, 256, s, n->string_value, 0, sp);
            fprintf(
                o,
                "  %%t%d = call %s @\"%s\"(ptr %%__slnk)\n",
                r.id,
                value_llvm_type_string(rk),
                fnb);
            r.k = rk;
          }
          else
          {
            int level_diff = generator->sm->lv - s->level - 1;
            int slnk_ptr;
            if (level_diff == 0)
            {
              slnk_ptr = new_temporary_register(generator);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
            }
            else
            {
              slnk_ptr = new_temporary_register(generator);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
              for (int hop = 0; hop < level_diff; hop++)
              {
                int next_slnk = new_temporary_register(generator);
                fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
                int loaded_slnk = new_temporary_register(generator);
                fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
                slnk_ptr = loaded_slnk;
              }
            }
            int p = new_temporary_register(generator);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->elaboration_level);
            int a = new_temporary_register(generator);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
            fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", r.id, value_llvm_type_string(k), a);
          }
        }
        else
        {
          Type_Info *vat = s and s->type_info ? type_canonical_concrete(s->type_info) : 0;
          int p = new_temporary_register(generator);
          int level_diff = generator->sm->lv - s->level - 1;
          int slnk_ptr;
          if (level_diff == 0)
          {
            slnk_ptr = new_temporary_register(generator);
            fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
          }
          else
          {
            slnk_ptr = new_temporary_register(generator);
            fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
            for (int hop = 0; hop < level_diff; hop++)
            {
              int next_slnk = new_temporary_register(generator);
              fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
              int loaded_slnk = new_temporary_register(generator);
              fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
              slnk_ptr = loaded_slnk;
            }
          }
          fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->elaboration_level);
          int a = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
          if (vat and vat->k == TYPE_ARRAY)
          {
            r.id = a;
            r.k = VALUE_KIND_POINTER;
          }
          else
          {
            fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", r.id, value_llvm_type_string(k), a);
          }
        }
      }
      else
      {
        if (s and s->k == 5)
        {
          Syntax_Node *b = symbol_body(s, s->elaboration_level);
          Syntax_Node *sp = symbol_spec(s);
          if (sp and sp->subprogram.parameters.count == 0)
          {
            Value_Kind rk = sp and sp->subprogram.return_type
                                ? token_kind_to_value_kind(resolve_subtype(generator->sm, sp->subprogram.return_type))
                                : VALUE_KIND_INTEGER;
            char fnb[256];
            encode_symbol_name(fnb, 256, s, n->string_value, 0, sp);
            if (s->level >= generator->sm->lv)
              fprintf(
                  o,
                  "  %%t%d = call %s @\"%s\"(ptr %%__frame)\n",
                  r.id,
                  value_llvm_type_string(rk),
                  fnb);
            else
              fprintf(
                  o,
                  "  %%t%d = call %s @\"%s\"(ptr %%__slnk)\n",
                  r.id,
                  value_llvm_type_string(rk),
                  fnb);
            r.k = rk;
          }
          else
          {
            Type_Info *vat = s and s->type_info ? type_canonical_concrete(s->type_info) : 0;
            if (vat and vat->k == TYPE_ARRAY)
            {
              if (vat->low_bound == 0 and vat->high_bound <= 0)
              {
                fprintf(
                    o,
                    "  %%t%d = load ptr, ptr %%v.%s.sc%u.%u\n",
                    r.id,
                    string_to_lowercase(n->string_value),
                    s ? s->scope : 0,
                    s ? s->elaboration_level : 0);
              }
              else
              {
                fprintf(
                    o,
                    "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                    r.id,
                    string_to_lowercase(n->string_value),
                    s ? s->scope : 0,
                    s ? s->elaboration_level : 0);
              }
            }
            else
            {
              fprintf(
                  o,
                  "  %%t%d = load %s, ptr %%v.%s.sc%u.%u\n",
                  r.id,
                  value_llvm_type_string(k),
                  string_to_lowercase(n->string_value),
                  s ? s->scope : 0,
                  s ? s->elaboration_level : 0);
            }
          }
        }
        else
        {
          Type_Info *vat = s and s->type_info ? type_canonical_concrete(s->type_info) : 0;
          if (vat and vat->k == TYPE_ARRAY)
          {
            if (vat->low_bound == 0 and vat->high_bound <= 0)
            {
              fprintf(
                  o,
                  "  %%t%d = load ptr, ptr %%v.%s.sc%u.%u\n",
                  r.id,
                  string_to_lowercase(n->string_value),
                  s ? s->scope : 0,
                  s ? s->elaboration_level : 0);
            }
            else
            {
              fprintf(
                  o,
                  "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                  r.id,
                  string_to_lowercase(n->string_value),
                  s ? s->scope : 0,
                  s ? s->elaboration_level : 0);
            }
          }
          else
          {
            fprintf(
                o,
                "  %%t%d = load %s, ptr %%v.%s.sc%u.%u\n",
                r.id,
                value_llvm_type_string(k),
                string_to_lowercase(n->string_value),
                s ? s->scope : 0,
                s ? s->elaboration_level : 0);
          }
        }
      }
    }
  }
  break;
  case N_BIN:
  {
    Token_Kind op = n->binary_node.op;
    if (op == T_ATHN or op == T_OREL)
    {
      Value lv = value_to_boolean(generator, generate_expression(generator, n->binary_node.left));
      int c = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c, lv.id);
      int lt = new_label_block(generator), lf = new_label_block(generator), ld = new_label_block(generator);
      if (op == T_ATHN)
        emit_conditional_branch(generator, c, lt, lf);
      else
        emit_conditional_branch(generator, c, lf, lt);
      emit_label(generator, lt);
      Value rv = value_to_boolean(generator, generate_expression(generator, n->binary_node.right));
      emit_branch(generator, ld);
      emit_label(generator, lf);
      emit_branch(generator, ld);
      emit_label(generator, ld);
      r.k = VALUE_KIND_INTEGER;
      fprintf(
          o,
          "  %%t%d = phi i64 [%s,%%Source_Location%d],[%%t%d,%%Source_Location%d]\n",
          r.id,
          op == T_ATHN ? "0" : "1",
          lf,
          rv.id,
          lt);
      break;
    }
    if (op == T_AND or op == T_OR or op == T_XOR)
    {
      Type_Info *lt = n->binary_node.left->ty ? type_canonical_concrete(n->binary_node.left->ty) : 0;
      Type_Info *rt = n->binary_node.right->ty ? type_canonical_concrete(n->binary_node.right->ty) : 0;
      if (lt and rt and lt->k == TYPE_ARRAY and rt->k == TYPE_ARRAY)
      {
        int sz = lt->high_bound >= lt->low_bound ? lt->high_bound - lt->low_bound + 1 : 1;
        int p = new_temporary_register(generator);
        fprintf(o, "  %%t%d = alloca [%d x i64]\n", p, sz);
        Value la = generate_expression(generator, n->binary_node.left);
        Value ra = generate_expression(generator, n->binary_node.right);
        for (int i = 0; i < sz; i++)
        {
          int ep1 = new_temporary_register(generator);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep1, la.id, i);
          int lv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lv, ep1);
          int ep2 = new_temporary_register(generator);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep2, ra.id, i);
          int rv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", rv, ep2);
          int res = new_temporary_register(generator);
          fprintf(
              o,
              "  %%t%d = %s i64 %%t%d, %%t%d\n",
              res,
              op == T_AND  ? "and"
              : op == T_OR ? "or"
                           : "xor",
              lv,
              rv);
          int ep3 = new_temporary_register(generator);
          fprintf(
              o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 %d\n", ep3, sz, p, i);
          fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", res, ep3);
        }
        r.k = VALUE_KIND_POINTER;
        fprintf(o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 0\n", r.id, sz, p);
        break;
      }
      Value a = value_to_boolean(generator, generate_expression(generator, n->binary_node.left));
      Value b = value_to_boolean(generator, generate_expression(generator, n->binary_node.right));
      r.k = VALUE_KIND_INTEGER;
      if (op == T_AND)
        fprintf(o, "  %%t%d = and i64 %%t%d, %%t%d\n", r.id, a.id, b.id);
      else if (op == T_OR)
        fprintf(o, "  %%t%d = or i64 %%t%d, %%t%d\n", r.id, a.id, b.id);
      else
        fprintf(o, "  %%t%d = xor i64 %%t%d, %%t%d\n", r.id, a.id, b.id);
      break;
    }
    if (op == T_NOT)
    {
      Value x = value_cast(generator, generate_expression(generator, n->binary_node.left), VALUE_KIND_INTEGER);
      Syntax_Node *rr = n->binary_node.right;
      while (rr and rr->k == N_CHK)
        rr = rr->check.expression;
      if (rr and rr->k == N_RN)
      {
        Value lo = value_cast(generator, generate_expression(generator, rr->range.low_bound), VALUE_KIND_INTEGER),
          hi = value_cast(generator, generate_expression(generator, rr->range.high_bound), VALUE_KIND_INTEGER);
        Value ge = value_compare_integer(generator, "sge", x, lo);
        Value le = value_compare_integer(generator, "sle", x, hi);
        Value b1 = value_to_boolean(generator, ge), b2 = value_to_boolean(generator, le);
        int c1 = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
        int c2 = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
        int a1 = new_temporary_register(generator);
        fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", a1, c1, c2);
        int xr = new_temporary_register(generator);
        fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", xr, a1);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = xor i64 %%t%d, 1\n", r.id, xr);
      }
      else if (rr and rr->k == N_ID)
      {
        Symbol *s = rr->symbol ? rr->symbol : symbol_find(generator->sm, rr->string_value);
        if (s and s->type_info)
        {
          Type_Info *t = type_canonical_concrete(s->type_info);
          if (t)
          {
            int tlo = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, (long long) t->low_bound);
            int thi = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", thi, (long long) t->high_bound);
            Value lo = {tlo, VALUE_KIND_INTEGER}, hi = {thi, VALUE_KIND_INTEGER};
            Value ge = value_compare_integer(generator, "sge", x, lo);
            Value le = value_compare_integer(generator, "sle", x, hi);
            Value b1 = value_to_boolean(generator, ge), b2 = value_to_boolean(generator, le);
            int c1 = new_temporary_register(generator);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
            int c2 = new_temporary_register(generator);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
            int a1 = new_temporary_register(generator);
            fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", a1, c1, c2);
            int xr = new_temporary_register(generator);
            fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", xr, a1);
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = xor i64 %%t%d, 1\n", r.id, xr);
          }
          else
          {
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
          }
        }
        else
        {
          r.k = VALUE_KIND_INTEGER;
          fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
        }
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = add i64 0, 1\n", r.id);
      }
      break;
    }
    if (op == T_IN)
    {
      Value x = value_cast(generator, generate_expression(generator, n->binary_node.left), VALUE_KIND_INTEGER);
      Syntax_Node *rr = n->binary_node.right;
      while (rr and rr->k == N_CHK)
        rr = rr->check.expression;
      if (rr and rr->k == N_RN)
      {
        Value lo = value_cast(generator, generate_expression(generator, rr->range.low_bound), VALUE_KIND_INTEGER),
          hi = value_cast(generator, generate_expression(generator, rr->range.high_bound), VALUE_KIND_INTEGER);
        Value ge = value_compare_integer(generator, "sge", x, lo);
        Value le = value_compare_integer(generator, "sle", x, hi);
        Value b1 = value_to_boolean(generator, ge), b2 = value_to_boolean(generator, le);
        int c1 = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
        int c2 = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
        int a1 = new_temporary_register(generator);
        fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", a1, c1, c2);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, a1);
      }
      else if (rr and rr->k == N_ID)
      {
        Symbol *s = rr->symbol ? rr->symbol : symbol_find(generator->sm, rr->string_value);
        if (s and s->type_info)
        {
          Type_Info *t = type_canonical_concrete(s->type_info);
          if (t)
          {
            int tlo = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, (long long) t->low_bound);
            int thi = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", thi, (long long) t->high_bound);
            Value lo = {tlo, VALUE_KIND_INTEGER}, hi = {thi, VALUE_KIND_INTEGER};
            Value ge = value_compare_integer(generator, "sge", x, lo);
            Value le = value_compare_integer(generator, "sle", x, hi);
            Value b1 = value_to_boolean(generator, ge), b2 = value_to_boolean(generator, le);
            int c1 = new_temporary_register(generator);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
            int c2 = new_temporary_register(generator);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
            int a1 = new_temporary_register(generator);
            fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", a1, c1, c2);
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, a1);
          }
          else
          {
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
          }
        }
        else
        {
          r.k = VALUE_KIND_INTEGER;
          fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
        }
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
      }
      break;
    }
    Value a = generate_expression(generator, n->binary_node.left), b = generate_expression(generator, n->binary_node.right);
    if (op == T_EQ or op == T_NE)
    {
      Type_Info *lt = n->binary_node.left->ty ? type_canonical_concrete(n->binary_node.left->ty) : 0;
      Type_Info *rt = n->binary_node.right->ty ? type_canonical_concrete(n->binary_node.right->ty) : 0;
      if (lt and rt and lt->k == TYPE_ARRAY and rt->k == TYPE_ARRAY)
      {
        int sz = lt->high_bound >= lt->low_bound ? lt->high_bound - lt->low_bound + 1 : 1;
        r.k = VALUE_KIND_INTEGER;
        int res = new_temporary_register(generator);
        fprintf(o, "  %%t%d = add i64 0, 1\n", res);
        for (int i = 0; i < sz; i++)
        {
          int ep1 = new_temporary_register(generator);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep1, a.id, i);
          int lv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lv, ep1);
          int ep2 = new_temporary_register(generator);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep2, b.id, i);
          int rv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", rv, ep2);
          int cmp = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", cmp, lv, rv);
          int ec = new_temporary_register(generator);
          fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", ec, cmp);
          int nres = new_temporary_register(generator);
          fprintf(o, "  %%t%d = and i64 %%t%d, %%t%d\n", nres, res, ec);
          res = nres;
        }
        int cmp_tmp = new_temporary_register(generator);
        fprintf(o, "  %%t%d = %s i64 %%t%d, 1\n", cmp_tmp, op == T_EQ ? "icmp eq" : "icmp ne", res);
        fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, cmp_tmp);
        break;
      }
    }
    if (op == T_EX)
    {
      if (b.k == VALUE_KIND_FLOAT)
      {
        int bc = new_temporary_register(generator);
        fprintf(o, "  %%t%d = fptosi double %%t%d to i64\n", bc, b.id);
        b.id = bc;
        b.k = VALUE_KIND_INTEGER;
      }
      Value bi = value_cast(generator, b, VALUE_KIND_INTEGER);
      int cf = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp slt i64 %%t%d, 0\n", cf, bi.id);
      int lt = new_label_block(generator), lf = new_label_block(generator);
      emit_conditional_branch(generator, cf, lt, lf);
      emit_label(generator, lt);
      fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\nL%d:\n", lf);
      if (token_kind_to_value_kind(n->ty) == VALUE_KIND_FLOAT)
      {
        r = value_power_float(generator, a, b);
      }
      else
      {
        r = value_power_integer(generator, a, bi);
      }
      break;
    }
    if (op == T_PL or op == T_MN or op == T_ST or op == T_SL)
    {
      if (a.k == VALUE_KIND_FLOAT or b.k == VALUE_KIND_FLOAT)
      {
        a = value_cast(generator, a, VALUE_KIND_FLOAT);
        b = value_cast(generator, b, VALUE_KIND_FLOAT);
        r.k = VALUE_KIND_FLOAT;
        fprintf(
            o,
            "  %%t%d = %s double %%t%d, %%t%d\n",
            r.id,
            op == T_PL   ? "fadd"
            : op == T_MN ? "fsub"
            : op == T_ST ? "fmul"
                         : "fdiv",
            a.id,
            b.id);
      }
      else
      {
        a = value_cast(generator, a, VALUE_KIND_INTEGER);
        b = value_cast(generator, b, VALUE_KIND_INTEGER);
        if (op == T_SL)
        {
          int zc = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp eq i64 %%t%d, 0\n", zc, b.id);
          int ze = new_label_block(generator), zd = new_label_block(generator);
          emit_conditional_branch(generator, zc, ze, zd);
          emit_label(generator, ze);
          fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
          emit_label(generator, zd);
        }
        r.k = VALUE_KIND_INTEGER;
        fprintf(
            o,
            "  %%t%d = %s i64 %%t%d, %%t%d\n",
            r.id,
            op == T_PL   ? "add"
            : op == T_MN ? "sub"
            : op == T_ST ? "mul"
                         : "sdiv",
            a.id,
            b.id);
      }
      break;
    }
    if (op == T_MOD or op == T_REM)
    {
      a = value_cast(generator, a, VALUE_KIND_INTEGER);
      b = value_cast(generator, b, VALUE_KIND_INTEGER);
      int zc = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp eq i64 %%t%d, 0\n", zc, b.id);
      int ze = new_label_block(generator), zd = new_label_block(generator);
      emit_conditional_branch(generator, zc, ze, zd);
      emit_label(generator, ze);
      fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
      emit_label(generator, zd);
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = srem i64 %%t%d, %%t%d\n", r.id, a.id, b.id);
      break;
    }
    if (op == T_EQ or op == T_NE or op == T_LT or op == T_LE or op == T_GT or op == T_GE)
    {
      if((op==T_EQ or op==T_NE) and (n->binary_node.left->k==N_STR or n->binary_node.right->k==N_STR or (n->binary_node.left->ty and type_canonical_concrete(n->binary_node.left->ty)->element_type and type_canonical_concrete(n->binary_node.left->ty)->element_type->k==TYPE_CHARACTER) or (n->binary_node.right->ty and type_canonical_concrete(n->binary_node.right->ty)->element_type and type_canonical_concrete(n->binary_node.right->ty)->element_type->k==TYPE_CHARACTER)))
      {
        Value ap = a, bp = b;
        if (ap.k == VALUE_KIND_INTEGER)
        {
          int p1 = new_temporary_register(generator);
          fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", p1, ap.id);
          ap.id = p1;
          ap.k = VALUE_KIND_POINTER;
        }
        if (bp.k == VALUE_KIND_INTEGER)
        {
          int p2 = new_temporary_register(generator);
          fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", p2, bp.id);
          bp.id = p2;
          bp.k = VALUE_KIND_POINTER;
        }
        int cmp = new_temporary_register(generator);
        fprintf(o, "  %%t%d = call i32 @strcmp(ptr %%t%d, ptr %%t%d)\n", cmp, ap.id, bp.id);
        int eq = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp %s i32 %%t%d, 0\n", eq, op == T_EQ ? "eq" : "ne", cmp);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, eq);
        break;
      }
      if (a.k == VALUE_KIND_FLOAT or b.k == VALUE_KIND_FLOAT)
      {
        const char *cc = op == T_EQ   ? "oeq"
                         : op == T_NE ? "one"
                         : op == T_LT ? "olt"
                         : op == T_LE ? "ole"
                         : op == T_GT ? "ogt"
                                      : "oge";
        r = value_compare_float(generator, cc, a, b);
      }
      else
      {
        const char *cc = op == T_EQ   ? "eq"
                         : op == T_NE ? "ne"
                         : op == T_LT ? "slt"
                         : op == T_LE ? "sle"
                         : op == T_GT ? "sgt"
                                      : "sge";
        r = value_compare_integer(generator, cc, a, b);
      }
      break;
    }
    if (op == T_AM and (a.k == VALUE_KIND_POINTER or b.k == VALUE_KIND_POINTER))
    {
      Type_Info *lt = n->binary_node.left->ty ? type_canonical_concrete(n->binary_node.left->ty) : 0;
      Type_Info *rt = n->binary_node.right->ty ? type_canonical_concrete(n->binary_node.right->ty) : 0;
      int alo, ahi, blo, bhi;
      Value ad, bd;
      bool la_fp = lt and lt->k == TYPE_ARRAY and lt->low_bound == 0 and lt->high_bound == -1;
      bool lb_fp = rt and rt->k == TYPE_ARRAY and rt->low_bound == 0 and rt->high_bound == -1;
      if (la_fp)
      {
        ad = get_fat_pointer_data(generator, a.id);
        get_fat_pointer_bounds(generator, a.id, &alo, &ahi);
      }
      else
      {
        ad = value_cast(generator, a, VALUE_KIND_POINTER);
        alo = new_temporary_register(generator);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            alo,
            lt and lt->k == TYPE_ARRAY ? (long long) lt->low_bound : 1LL);
        ahi = new_temporary_register(generator);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            ahi,
            lt and lt->k == TYPE_ARRAY ? (long long) lt->high_bound : 0LL);
      }
      if (lb_fp)
      {
        bd = get_fat_pointer_data(generator, b.id);
        get_fat_pointer_bounds(generator, b.id, &blo, &bhi);
      }
      else
      {
        bd = value_cast(generator, b, VALUE_KIND_POINTER);
        blo = new_temporary_register(generator);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            blo,
            rt and rt->k == TYPE_ARRAY ? (long long) rt->low_bound : 1LL);
        bhi = new_temporary_register(generator);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            bhi,
            rt and rt->k == TYPE_ARRAY ? (long long) rt->high_bound : 0LL);
      }
      int alen = new_temporary_register(generator);
      fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", alen, ahi, alo);
      int alen1 = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", alen1, alen);
      int blen = new_temporary_register(generator);
      fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", blen, bhi, blo);
      int blen1 = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", blen1, blen);
      int tlen = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 %%t%d, %%t%d\n", tlen, alen1, blen1);
      int tlen1 = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", tlen1, tlen);
      int np = new_temporary_register(generator);
      fprintf(o, "  %%t%d = call ptr @malloc(i64 %%t%d)\n", np, tlen1);
      fprintf(
          o,
          "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %%t%d, i1 false)\n",
          np,
          ad.id,
          alen1);
      int dp = new_temporary_register(generator);
      fprintf(o, "  %%t%d = getelementptr i8, ptr %%t%d, i64 %%t%d\n", dp, np, alen1);
      fprintf(
          o,
          "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %%t%d, i1 false)\n",
          dp,
          bd.id,
          blen1);
      int zp = new_temporary_register(generator);
      fprintf(o, "  %%t%d = getelementptr i8, ptr %%t%d, i64 %%t%d\n", zp, np, tlen);
      fprintf(o, "  store i8 0, ptr %%t%d\n", zp);
      int nlo = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 0, 1\n", nlo);
      int nhi = new_temporary_register(generator);
      fprintf(o, "  %%t%d = sub i64 %%t%d, 1\n", nhi, tlen);
      r.k = VALUE_KIND_POINTER;
      r.id = new_temporary_register(generator);
      generate_fat_pointer(generator, r.id, np, nlo, nhi);
      break;
    }
    r.k = VALUE_KIND_INTEGER;
    {
      Value ai = value_cast(generator, a, VALUE_KIND_INTEGER), bi = value_cast(generator, b, VALUE_KIND_INTEGER);
      fprintf(o, "  %%t%d = add i64 %%t%d, %%t%d\n", r.id, ai.id, bi.id);
    }
    break;
  }
  break;
  case N_UN:
  {
    Value x = generate_expression(generator, n->unary_node.operand);
    if (n->unary_node.op == T_MN)
    {
      if (x.k == VALUE_KIND_FLOAT)
      {
        r.k = VALUE_KIND_FLOAT;
        fprintf(o, "  %%t%d = fsub double 0.0, %%t%d\n", r.id, x.id);
      }
      else
      {
        x = value_cast(generator, x, VALUE_KIND_INTEGER);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = sub i64 0, %%t%d\n", r.id, x.id);
      }
      break;
    }
    if (n->unary_node.op == T_NOT)
    {
      Type_Info *xt = n->unary_node.operand->ty ? type_canonical_concrete(n->unary_node.operand->ty) : 0;
      if (xt and xt->k == TYPE_ARRAY)
      {
        int sz = xt->high_bound >= xt->low_bound ? xt->high_bound - xt->low_bound + 1 : 1;
        int p = new_temporary_register(generator);
        fprintf(o, "  %%t%d = alloca [%d x i64]\n", p, sz);
        for (int i = 0; i < sz; i++)
        {
          int ep1 = new_temporary_register(generator);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep1, x.id, i);
          int lv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lv, ep1);
          int res = new_temporary_register(generator);
          fprintf(o, "  %%t%d = xor i64 %%t%d, 1\n", res, lv);
          int ep2 = new_temporary_register(generator);
          fprintf(
              o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 %d\n", ep2, sz, p, i);
          fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", res, ep2);
        }
        r.k = VALUE_KIND_POINTER;
        fprintf(o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 0\n", r.id, sz, p);
      }
      else
      {
        Value b = value_to_boolean(generator, x);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = xor i64 %%t%d, 1\n", r.id, b.id);
      }
      break;
    }
    if (n->unary_node.op == T_ABS)
    {
      if (x.k == VALUE_KIND_FLOAT)
      {
        Value z = value_cast(generator, x, VALUE_KIND_FLOAT);
        int c = new_temporary_register(generator);
        fprintf(o, "  %%t%d = fcmp olt double %%t%d, 0.0\n", c, z.id);
        Value ng = {new_temporary_register(generator), VALUE_KIND_FLOAT};
        fprintf(o, "  %%t%d = fsub double 0.0, %%t%d\n", ng.id, z.id);
        r.k = VALUE_KIND_FLOAT;
        fprintf(o, "  %%t%d = select i1 %%t%d, double %%t%d, double %%t%d\n", r.id, c, ng.id, z.id);
      }
      else
      {
        Value z = value_cast(generator, x, VALUE_KIND_INTEGER);
        int c = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp slt i64 %%t%d, 0\n", c, z.id);
        Value ng = {new_temporary_register(generator), VALUE_KIND_INTEGER};
        fprintf(o, "  %%t%d = sub i64 0, %%t%d\n", ng.id, z.id);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = select i1 %%t%d, i64 %%t%d, i64 %%t%d\n", r.id, c, ng.id, z.id);
      }
      break;
    }
    r = value_cast(generator, x, r.k);
    break;
  }
  break;
  case N_IX:
  {
    Value p = generate_expression(generator, n->index.prefix);
    Type_Info *pt = n->index.prefix->ty ? type_canonical_concrete(n->index.prefix->ty) : 0;
    Type_Info *et = n->ty ? type_canonical_concrete(n->ty) : 0;
    bool is_char = et and et->k == TYPE_CHARACTER;
    const char *elem_type_str = ada_to_c_type_string(et);
    int dp = p.id;
    if (pt and pt->k == TYPE_ARRAY and pt->low_bound == 0 and pt->high_bound <= 0)
    {
      dp = get_fat_pointer_data(generator, p.id).id;
      int blo, bhi;
      get_fat_pointer_bounds(generator, p.id, &blo, &bhi);
      Value i0 = value_cast(generator, generate_expression(generator, n->index.indices.data[0]), VALUE_KIND_INTEGER);
      int adj = new_temporary_register(generator);
      fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", adj, i0.id, blo);
      char lb[32], hb[32];
      snprintf(lb, 32, "%%t%d", blo);
      snprintf(hb, 32, "%%t%d", bhi);
      generate_index_constraint_check(generator, i0.id, lb, hb);
      int ep = new_temporary_register(generator);
      fprintf(
          o,
          "  %%t%d = getelementptr %s, ptr %%t%d, i64 %%t%d\n",
          ep,
          elem_type_str,
          dp,
          adj);
      if (et and (et->k == TYPE_ARRAY or et->k == TYPE_RECORD))
      {
        r.k = VALUE_KIND_POINTER;
        r.id = ep;
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        if (is_char)
        {
          int lv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load i8, ptr %%t%d\n", lv, ep);
          fprintf(o, "  %%t%d = zext i8 %%t%d to i64\n", r.id, lv);
        }
        else if (strcmp(elem_type_str, "i64") != 0)
        {
          int lv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", lv, elem_type_str, ep);
          fprintf(o, "  %%t%d = sext %s %%t%d to i64\n", r.id, elem_type_str, lv);
        }
        else
        {
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", r.id, ep);
        }
      }
    }
    else
    {
      if (p.k == VALUE_KIND_INTEGER)
      {
        int pp = new_temporary_register(generator);
        fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, p.id);
        dp = pp;
      }
      Value i0 = value_cast(generator, generate_expression(generator, n->index.indices.data[0]), VALUE_KIND_INTEGER);
      Type_Info *at = pt;
      int adj_idx = i0.id;
      if (at and at->k == TYPE_ARRAY and at->low_bound != 0)
      {
        int adj = new_temporary_register(generator);
        fprintf(o, "  %%t%d = sub i64 %%t%d, %lld\n", adj, i0.id, (long long) at->low_bound);
        adj_idx = adj;
      }
      if (at and at->k == TYPE_ARRAY and not(at->suppressed_checks & CHK_IDX) and (at->low_bound != 0 or at->high_bound != -1))
      {
        char lb[32], hb[32];
        snprintf(lb, 32, "%lld", (long long) at->low_bound);
        snprintf(hb, 32, "%lld", (long long) at->high_bound);
        generate_index_constraint_check(generator, i0.id, lb, hb);
      }
      int ep = new_temporary_register(generator);
      if (at and at->k == TYPE_ARRAY and at->high_bound >= at->low_bound)
      {
        int asz = (int) (at->high_bound - at->low_bound + 1);
        fprintf(
            o,
            "  %%t%d = getelementptr [%d x %s], ptr %%t%d, i64 0, i64 %%t%d\n",
            ep,
            asz,
            elem_type_str,
            dp,
            adj_idx);
      }
      else
      {
        fprintf(
            o,
            "  %%t%d = getelementptr %s, ptr %%t%d, i64 %%t%d\n",
            ep,
            elem_type_str,
            dp,
            i0.id);
      }
      if (et and (et->k == TYPE_ARRAY or et->k == TYPE_RECORD))
      {
        r.k = VALUE_KIND_POINTER;
        r.id = ep;
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        if (is_char)
        {
          int lv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load i8, ptr %%t%d\n", lv, ep);
          fprintf(o, "  %%t%d = zext i8 %%t%d to i64\n", r.id, lv);
        }
        else if (strcmp(elem_type_str, "i64") != 0)
        {
          int lv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", lv, elem_type_str, ep);
          fprintf(o, "  %%t%d = sext %s %%t%d to i64\n", r.id, elem_type_str, lv);
        }
        else
        {
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", r.id, ep);
        }
      }
    }
  }
  break;
  case N_SL:
  {
    Value p = generate_expression(generator, n->slice.prefix);
    Type_Info *pt = n->slice.prefix->ty ? type_canonical_concrete(n->slice.prefix->ty) : 0;
    Type_Info *et = pt and pt->element_type ? type_canonical_concrete(pt->element_type) : 0;
    const char *elem_type_str = ada_to_c_type_string(et);
    uint32_t elem_size = et and et->size ? et->size / 8 : 8;

    Value lo = value_cast(generator, generate_expression(generator, n->slice.low_bound), VALUE_KIND_INTEGER);
    Value hi = value_cast(generator, generate_expression(generator, n->slice.high_bound), VALUE_KIND_INTEGER);
    int ln = new_temporary_register(generator);
    fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", ln, hi.id, lo.id);
    int sz = new_temporary_register(generator);
    fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", sz, ln);
    int sl = new_temporary_register(generator);
    fprintf(o, "  %%t%d = mul i64 %%t%d, %u\n", sl, sz, elem_size);
    int ap = new_temporary_register(generator);
    fprintf(o, "  %%t%d = alloca i8, i64 %%t%d\n", ap, sl);
    int sp = new_temporary_register(generator);
    fprintf(o, "  %%t%d = getelementptr %s, ptr %%t%d, i64 %%t%d\n", sp, elem_type_str, p.id, lo.id);
    fprintf(
        o,
        "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %%t%d, i1 false)\n",
        ap,
        sp,
        sl);
    r.k = VALUE_KIND_POINTER;
    r.id = new_temporary_register(generator);
    generate_fat_pointer(generator, r.id, ap, lo.id, hi.id);
  }
  break;
  case N_SEL:
  {
    Type_Info *pt = n->selected_component.prefix->ty ? type_canonical_concrete(n->selected_component.prefix->ty) : 0;
    Value p = {new_temporary_register(generator), VALUE_KIND_POINTER};
    if (n->selected_component.prefix->k == N_ID)
    {
      Symbol *s = n->selected_component.prefix->symbol ? n->selected_component.prefix->symbol : symbol_find(generator->sm, n->selected_component.prefix->string_value);
      if (s and s->k != 6)
      {
        Type_Info *vty = s and s->type_info ? type_canonical_concrete(s->type_info) : 0;
        bool has_nested = false;
        if (vty and vty->k == TYPE_RECORD)
        {
          for (uint32_t ci = 0; ci < vty->discriminants.count; ci++)
          {
            Syntax_Node *fd = vty->discriminants.data[ci];
            Type_Info *fty =
                fd and fd->k == N_DS and fd->parameter.ty ? resolve_subtype(generator->sm, fd->parameter.ty) : 0;
            if (fty and (fty->k == TYPE_RECORD or fty->k == TYPE_ARRAY))
            {
              has_nested = true;
              break;
            }
          }
          if (not has_nested)
          {
            for (uint32_t ci = 0; ci < vty->components.count; ci++)
            {
              Syntax_Node *fc = vty->components.data[ci];
              Type_Info *fty =
                  fc and fc->k == N_CM and fc->component_decl.ty ? resolve_subtype(generator->sm, fc->component_decl.ty) : 0;
              if (fty and (fty->k == TYPE_RECORD or fty->k == TYPE_ARRAY))
              {
                has_nested = true;
                break;
              }
            }
          }
        }
        if (s->level >= 0 and s->level < generator->sm->lv)
        {
          if (has_nested)
          {
            int tp = new_temporary_register(generator);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", tp, s->elaboration_level);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", p.id, tp);
          }
          else
            fprintf(
                o,
                "  %%t%d = bitcast ptr %%lnk.%d.%.*s to ptr\n",
                p.id,
                s->level,
                (int) n->selected_component.prefix->string_value.length,
                n->selected_component.prefix->string_value.string);
        }
        else
        {
          if (has_nested)
            fprintf(
                o,
                "  %%t%d = load ptr, ptr %%v.%s.sc%u.%u\n",
                p.id,
                string_to_lowercase(n->selected_component.prefix->string_value),
                s ? s->scope : 0,
                s ? s->elaboration_level : 0);
          else
            fprintf(
                o,
                "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                p.id,
                string_to_lowercase(n->selected_component.prefix->string_value),
                s ? s->scope : 0,
                s ? s->elaboration_level : 0);
        }
      }
    }
    else
    {
      p = generate_expression(generator, n->selected_component.prefix);
    }
    if (pt and pt->k == TYPE_RECORD)
    {
      for (uint32_t i = 0; i < pt->discriminants.count; i++)
      {
        Syntax_Node *d = pt->discriminants.data[i];
        String_Slice dn = d->k == N_DS ? d->parameter.name : d->component_decl.name;
        if (string_equal_ignore_case(dn, n->selected_component.selector))
        {
          int ep = new_temporary_register(generator);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, i);
          Type_Info *fty = d->k == N_DS ? resolve_subtype(generator->sm, d->parameter.ty) : 0;
          if (fty and (fty->k == TYPE_RECORD or fty->k == TYPE_ARRAY))
          {
            r.k = VALUE_KIND_POINTER;
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", r.id, ep);
          }
          else
          {
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", r.id, ep);
          }
          goto sel_done;
        }
      }
      if (pt->is_packed)
      {
        for (uint32_t i = 0; i < pt->components.count; i++)
        {
          Syntax_Node *c = pt->components.data[i];
          if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, n->selected_component.selector))
          {
            int bp = new_temporary_register(generator);
            fprintf(o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", bp, p.id);
            int bo = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 %%t%d, %u\n", bo, bp, c->component_decl.offset / 8);
            int pp = new_temporary_register(generator);
            fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, bo);
            int vp = new_temporary_register(generator);
            fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", vp, pp);
            int sh = new_temporary_register(generator);
            fprintf(o, "  %%t%d = lshr i64 %%t%d, %u\n", sh, vp, c->component_decl.offset % 8);
            uint64_t mk = (1ULL << c->component_decl.bit_offset) - 1;
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = and i64 %%t%d, %llu\n", r.id, sh, (unsigned long long) mk);
            break;
          }
        }
      }
      else
      {
        for (uint32_t i = 0; i < pt->components.count; i++)
        {
          Syntax_Node *c = pt->components.data[i];
          if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, n->selected_component.selector))
          {
            int ep = new_temporary_register(generator);
            fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, c->component_decl.offset);
            Type_Info *fty = resolve_subtype(generator->sm, c->component_decl.ty);
            if (fty and (fty->k == TYPE_RECORD or fty->k == TYPE_ARRAY))
            {
              r.k = VALUE_KIND_POINTER;
              fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", r.id, ep);
            }
            else
            {
              r.k = VALUE_KIND_INTEGER;
              fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", r.id, ep);
            }
            break;
          }
        }
      }
      for (uint32_t i = 0; i < pt->components.count; i++)
      {
        Syntax_Node *c = pt->components.data[i];
        if (c->k == N_VP)
        {
          for (uint32_t j = 0; j < c->variant_part.variants.count; j++)
          {
            Syntax_Node *v = c->variant_part.variants.data[j];
            for (uint32_t k = 0; k < v->variant.components.count; k++)
            {
              Syntax_Node *vc = v->variant.components.data[k];
              if (string_equal_ignore_case(vc->component_decl.name, n->selected_component.selector))
              {
                int ep = new_temporary_register(generator);
                fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, vc->component_decl.offset);
                r.k = VALUE_KIND_INTEGER;
                fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", r.id, ep);
                goto sel_done;
              }
            }
          }
        }
      }
    sel_done:;
    }
    else if (n->symbol and n->symbol->k == 5)
    {
      Syntax_Node *sp = symbol_spec(n->symbol);
      if (sp and sp->subprogram.parameters.count == 0)
      {
        Value_Kind rk = sp and sp->subprogram.return_type
                            ? token_kind_to_value_kind(resolve_subtype(generator->sm, sp->subprogram.return_type))
                            : VALUE_KIND_INTEGER;
        char fnb[256];
        encode_symbol_name(fnb, 256, n->symbol, n->selected_component.selector, 0, sp);
        fprintf(o, "  %%t%d = call %s @\"%s\"()\n", r.id, value_llvm_type_string(rk), fnb);
        r.k = rk;
      }
    }
    else if (n->symbol and n->symbol->k == 2)
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) n->symbol->value);
    }
    else
    {
      r = value_cast(generator, p, r.k);
    }
  }
  break;
  case N_AT:
  {
    String_Slice a = n->attribute.attribute_name;
    Type_Info *t = (n->attribute.prefix and n->attribute.prefix->ty) ? type_canonical_concrete(n->attribute.prefix->ty) : 0;
    if (string_equal_ignore_case(a, STRING_LITERAL("ADDRESS")))
    {
      if (n->attribute.prefix and n->attribute.prefix->k == N_ID)
      {
        Symbol *s = n->attribute.prefix->symbol ? n->attribute.prefix->symbol : symbol_find(generator->sm, n->attribute.prefix->string_value);
        if (s)
        {
          r.k = VALUE_KIND_INTEGER;
          if (s->level == 0)
          {
            char nb[256];
            if (s->parent and (uintptr_t) s->parent > 4096 and s->parent->name.string)
            {
              int n = 0;
              for (uint32_t j = 0; j < s->parent->name.length; j++)
                nb[n++] = toupper(s->parent->name.string[j]);
              n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
              for (uint32_t j = 0; j < s->name.length; j++)
                nb[n++] = toupper(s->name.string[j]);
              nb[n] = 0;
            }
            else
              snprintf(nb, 256, "%.*s", (int) s->name.length, s->name.string);
            int p = new_temporary_register(generator);
            fprintf(o, "  %%t%d = ptrtoint ptr @%s to i64\n", p, nb);
            r.id = p;
          }
          else if (s->level >= 0 and s->level < generator->sm->lv)
          {
            int p = new_temporary_register(generator);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", p, s->elaboration_level);
            int a = new_temporary_register(generator);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
            fprintf(o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", r.id, a);
          }
          else
          {
            int p = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 0, 1\n", p);
            r.id = p;
          }
        }
        else
        {
          Value p = generate_expression(generator, n->attribute.prefix);
          fprintf(
              o,
              "  %%t%d = ptrtoint ptr %%t%d to i64\n",
              r.id,
              value_cast(generator, p, VALUE_KIND_POINTER).id);
        }
      }
      else if (n->attribute.prefix and n->attribute.prefix->k == N_AT)
      {
        Syntax_Node *ap = n->attribute.prefix;
        String_Slice ia = ap->attribute.attribute_name;
        if (string_equal_ignore_case(ia, STRING_LITERAL("PRED"))
            or string_equal_ignore_case(ia, STRING_LITERAL("SUCC"))
            or string_equal_ignore_case(ia, STRING_LITERAL("POS"))
            or string_equal_ignore_case(ia, STRING_LITERAL("VAL"))
            or string_equal_ignore_case(ia, STRING_LITERAL("IMAGE"))
            or string_equal_ignore_case(ia, STRING_LITERAL("VALUE")))
        {
          Type_Info *pt = ap->attribute.prefix ? type_canonical_concrete(ap->attribute.prefix->ty) : 0;
          String_Slice pnm =
              ap->attribute.prefix and ap->attribute.prefix->k == N_ID ? ap->attribute.prefix->string_value : STRING_LITERAL("TYPE");
          const char *afn = get_attribute_name(ia, pnm);
          r.k = VALUE_KIND_INTEGER;
          int p = new_temporary_register(generator);
          fprintf(o, "  %%t%d = ptrtoint ptr %s to i64\n", p, afn);
          r.id = p;
        }
        else
        {
          Value p = generate_expression(generator, n->attribute.prefix);
          r.k = VALUE_KIND_INTEGER;
          fprintf(
              o,
              "  %%t%d = ptrtoint ptr %%t%d to i64\n",
              r.id,
              value_cast(generator, p, VALUE_KIND_POINTER).id);
        }
      }
      else
      {
        Value p = generate_expression(generator, n->attribute.prefix);
        r.k = VALUE_KIND_INTEGER;
        fprintf(
            o,
            "  %%t%d = ptrtoint ptr %%t%d to i64\n",
            r.id,
            value_cast(generator, p, VALUE_KIND_POINTER).id);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("SIZE")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) (t->size * 8) : 64LL);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("FIRST"))
        or string_equal_ignore_case(a, STRING_LITERAL("LAST"))
        or string_equal_ignore_case(a, STRING_LITERAL("LENGTH")))
    {
      Value pv = {0, VALUE_KIND_INTEGER};
      bool is_typ = n->attribute.prefix and n->attribute.prefix->k == N_ID and n->attribute.prefix->symbol and n->attribute.prefix->symbol->k == 1;
      if (n->attribute.prefix and not is_typ)
        pv = generate_expression(generator, n->attribute.prefix);
      if (n->attribute.arguments.count > 0)
        generate_expression(generator, n->attribute.arguments.data[0]);
      int64_t lo = 0, hi = -1;
      if (t and t->k == TYPE_ARRAY)
      {
        if (t->low_bound == 0 and t->high_bound <= 0 and n->attribute.prefix and not is_typ)
        {
          int blo, bhi;
          get_fat_pointer_bounds(generator, pv.id, &blo, &bhi);
          r.k = VALUE_KIND_INTEGER;
          if (string_equal_ignore_case(a, STRING_LITERAL("FIRST")))
          {
            r.id = blo;
          }
          else if (string_equal_ignore_case(a, STRING_LITERAL("LAST")))
          {
            r.id = bhi;
          }
          else
          {
            r.id = new_temporary_register(generator);
            fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", r.id, bhi, blo);
            int tmp = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", tmp, r.id);
            r.id = tmp;
          }
          break;
        }
        lo = t->low_bound;
        hi = t->high_bound;
      }
      else if (t and (is_integer_type(t) or t->k == TYPE_ENUMERATION))
      {
        lo = t->low_bound;
        hi = t->high_bound;
      }
      int64_t v = string_equal_ignore_case(a, STRING_LITERAL("FIRST")) ? lo
                  : string_equal_ignore_case(a, STRING_LITERAL("LAST"))
                      ? hi
                      : (hi >= lo ? hi - lo + 1 : 0);
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) v);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("POS")))
    {
      Value x = generate_expression(generator, n->attribute.arguments.data[0]);
      if (t
          and (t->k == TYPE_ENUMERATION or t->k == TYPE_INTEGER or t->k == TYPE_UNSIGNED_INTEGER or t->k == TYPE_DERIVED))
      {
        r = value_cast(generator, x, VALUE_KIND_INTEGER);
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        int tlo = new_temporary_register(generator);
        fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, t ? (long long) t->low_bound : 0LL);
        fprintf(
            o,
            "  %%t%d = sub i64 %%t%d, %%t%d\n",
            r.id,
            value_cast(generator, x, VALUE_KIND_INTEGER).id,
            tlo);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("VAL")))
    {
      Value x = generate_expression(generator, n->attribute.arguments.data[0]);
      r.k = VALUE_KIND_INTEGER;
      int tlo = new_temporary_register(generator);
      fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, t ? (long long) t->low_bound : 0LL);
      fprintf(
          o,
          "  %%t%d = add i64 %%t%d, %%t%d\n",
          r.id,
          value_cast(generator, x, VALUE_KIND_INTEGER).id,
          tlo);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("SUCC"))
        or string_equal_ignore_case(a, STRING_LITERAL("PRED")))
    {
      Value x = generate_expression(generator, n->attribute.arguments.data[0]);
      r.k = VALUE_KIND_INTEGER;
      fprintf(
          o,
          "  %%t%d = %s i64 %%t%d, 1\n",
          r.id,
          string_equal_ignore_case(a, STRING_LITERAL("SUCC")) ? "add" : "sub",
          value_cast(generator, x, VALUE_KIND_INTEGER).id);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("IMAGE")))
    {
      Value x = generate_expression(generator, n->attribute.arguments.data[0]);
      r.k = VALUE_KIND_POINTER;
      if (t and t->k == TYPE_ENUMERATION)
      {
        fprintf(
            o,
            "  %%t%d = call ptr @__ada_image_enum(i64 %%t%d, i64 %lld, i64 %lld)\n",
            r.id,
            value_cast(generator, x, VALUE_KIND_INTEGER).id,
            t ? (long long) t->low_bound : 0LL,
            t ? (long long) t->high_bound : 127LL);
      }
      else
      {
        fprintf(
            o,
            "  %%t%d = call ptr @__ada_image_int(i64 %%t%d)\n",
            r.id,
            value_cast(generator, x, VALUE_KIND_INTEGER).id);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("VALUE")))
    {
      Value x = generate_expression(generator, n->attribute.arguments.data[0]);
      r.k = VALUE_KIND_INTEGER;
      if (t and t->k == TYPE_ENUMERATION)
      {
        Value buf = get_fat_pointer_data(generator, x.id);
        int fnd = new_temporary_register(generator);
        fprintf(o, "  %%t%d = add i64 0, -1\n", fnd);
        for (uint32_t i = 0; i < t->enum_values.count; i++)
        {
          Symbol *e = t->enum_values.data[i];
          int sz = e->name.length + 1;
          int p = new_temporary_register(generator);
          fprintf(o, "  %%t%d = alloca [%d x i8]\n", p, sz);
          for (uint32_t j = 0; j < e->name.length; j++)
          {
            int ep = new_temporary_register(generator);
            fprintf(
                o, "  %%t%d = getelementptr [%d x i8], ptr %%t%d, i64 0, i64 %u\n", ep, sz, p, j);
            fprintf(o, "  store i8 %d, ptr %%t%d\n", (int) (unsigned char) e->name.string[j], ep);
          }
          int zp = new_temporary_register(generator);
          fprintf(
              o,
              "  %%t%d = getelementptr [%d x i8], ptr %%t%d, i64 0, i64 %u\n",
              zp,
              sz,
              p,
              (unsigned int) e->name.length);
          fprintf(o, "  store i8 0, ptr %%t%d\n", zp);
          int sp = new_temporary_register(generator);
          fprintf(o, "  %%t%d = getelementptr [%d x i8], ptr %%t%d, i64 0, i64 0\n", sp, sz, p);
          int cmp = new_temporary_register(generator);
          fprintf(o, "  %%t%d = call i32 @strcmp(ptr %%t%d, ptr %%t%d)\n", cmp, buf.id, sp);
          int eq = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", eq, cmp);
          int nfnd = new_temporary_register(generator);
          fprintf(
              o, "  %%t%d = select i1 %%t%d, i64 %lld, i64 %%t%d\n", nfnd, eq, (long long) i, fnd);
          fnd = nfnd;
        }
        int chk = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp slt i64 %%t%d, 0\n", chk, fnd);
        int le = new_label_block(generator), ld = new_label_block(generator);
        emit_conditional_branch(generator, chk, le, ld);
        emit_label(generator, le);
        fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
        emit_label(generator, ld);
        fprintf(o, "  %%t%d = add i64 %%t%d, %lld\n", r.id, fnd, t ? (long long) t->low_bound : 0LL);
      }
      else
      {
        fprintf(
            o,
            "  %%t%d = call i64 @__ada_value_int(ptr %%t%d)\n",
            r.id,
            value_cast(generator, x, VALUE_KIND_POINTER).id);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("DIGITS")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) t->small_value : 15LL);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("DELTA")))
    {
      r.k = VALUE_KIND_FLOAT;
      fprintf(
          o, "  %%t%d = fadd double 0.0, %e\n", r.id, t ? 1.0 / pow(2.0, (double) t->small_value) : 0.01);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("SMALL"))
        or string_equal_ignore_case(a, STRING_LITERAL("LARGE"))
        or string_equal_ignore_case(a, STRING_LITERAL("EPSILON")))
    {
      r.k = VALUE_KIND_FLOAT;
      double v = string_equal_ignore_case(a, STRING_LITERAL("SMALL")) ? pow(2.0, -126.0)
                 : string_equal_ignore_case(a, STRING_LITERAL("LARGE"))
                     ? (t and t->small_value > 0 ? (pow(2.0, ceil((double) t->small_value * log2(10.0)) + 1.0) - 1.0)
                                              * pow(2.0, 63.0)
                                        : 1.0e308)
                     : pow(2.0, t ? -(double) t->small_value : -52.0);
      fprintf(o, "  %%t%d = fadd double 0.0, %e\n", r.id, v);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("MANTISSA"))
        or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_MANTISSA")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) t->small_value : 53LL);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("MACHINE_RADIX")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 2\n", r.id);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("EMAX"))
        or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_EMAX"))
        or string_equal_ignore_case(a, STRING_LITERAL("SAFE_EMAX")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 1024\n", r.id);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("MACHINE_EMIN")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, -1021\n", r.id);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("MACHINE_OVERFLOWS"))
        or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_ROUNDS")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 1\n", r.id);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("AFT")))
    {
      r.k = VALUE_KIND_INTEGER;
      int64_t dg = 1;
      if (t and t->small_value > 0)
      {
        while (pow(10.0, (double) dg) * pow(2.0, -(double) t->small_value) < 1.0)
          dg++;
      }
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) dg);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("FORE")))
    {
      r.k = VALUE_KIND_INTEGER;
      int64_t fw = 2;
      if (t and t->high_bound > 0)
      {
        int64_t mx = t->high_bound;
        while (mx >= 10)
        {
          mx /= 10;
          fw++;
        }
      }
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) fw);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("WIDTH")))
    {
      r.k = VALUE_KIND_INTEGER;
      int64_t wd = 1;
      if (t)
      {
        if (t->k == TYPE_ENUMERATION)
        {
          for (uint32_t i = 0; i < t->enum_values.count; i++)
          {
            Symbol *e = t->enum_values.data[i];
            int64_t ln = e->name.length;
            if (ln > wd)
              wd = ln;
          }
        }
        else
        {
          int64_t mx = t->high_bound > -t->low_bound ? t->high_bound : -t->low_bound;
          while (mx >= 10)
          {
            mx /= 10;
            wd++;
          }
          if (t->low_bound < 0)
            wd++;
        }
      }
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) wd);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("STORAGE_SIZE")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) (t->size * 8) : 0LL);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("POSITION"))
        or string_equal_ignore_case(a, STRING_LITERAL("FIRST_BIT"))
        or string_equal_ignore_case(a, STRING_LITERAL("LAST_BIT")))
    {
      r.k = VALUE_KIND_INTEGER;
      int64_t v = 0;
      if (n->attribute.prefix and n->attribute.prefix->k == N_SEL)
      {
        Type_Info *pt = n->attribute.prefix->selected_component.prefix->ty ? type_canonical_concrete(n->attribute.prefix->selected_component.prefix->ty) : 0;
        if (pt and pt->k == TYPE_RECORD)
        {
          for (uint32_t i = 0; i < pt->components.count; i++)
          {
            Syntax_Node *c = pt->components.data[i];
            if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, n->attribute.prefix->selected_component.selector))
            {
              if (pt->is_packed)
              {
                if (string_equal_ignore_case(a, STRING_LITERAL("POSITION")))
                  v = c->component_decl.offset / 8;
                else if (string_equal_ignore_case(a, STRING_LITERAL("FIRST_BIT")))
                  v = c->component_decl.offset % 8;
                else if (string_equal_ignore_case(a, STRING_LITERAL("LAST_BIT")))
                  v = (c->component_decl.offset % 8) + c->component_decl.bit_offset - 1;
              }
              else
              {
                if (string_equal_ignore_case(a, STRING_LITERAL("POSITION")))
                  v = i * 8;
                else if (string_equal_ignore_case(a, STRING_LITERAL("FIRST_BIT")))
                  v = 0;
                else if (string_equal_ignore_case(a, STRING_LITERAL("LAST_BIT")))
                  v = 63;
              }
              break;
            }
          }
        }
      }
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) v);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("CONSTRAINED")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 1\n", r.id);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("COUNT"))
        or string_equal_ignore_case(a, STRING_LITERAL("CALLABLE"))
        or string_equal_ignore_case(a, STRING_LITERAL("TERMINATED")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("ACCESS")))
    {
      Value p = generate_expression(generator, n->attribute.prefix);
      r = value_cast(generator, p, VALUE_KIND_POINTER);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("SAFE_LARGE"))
        or string_equal_ignore_case(a, STRING_LITERAL("SAFE_SMALL")))
    {
      r.k = VALUE_KIND_FLOAT;
      double v = string_equal_ignore_case(a, STRING_LITERAL("SAFE_LARGE")) ? 1.0e307 : 1.0e-307;
      fprintf(o, "  %%t%d = fadd double 0.0, %e\n", r.id, v);
    }
    else
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
    }
    break;
  }
  break;
  case N_QL:
  {
    Value q = generate_expression(generator, n->qualified.aggregate);
    r = value_cast(generator, q, r.k);
  }
  break;
  case N_CL:
  {
    if (not n->call.function_name or (uintptr_t) n->call.function_name < 4096)
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
      break;
    }
    if (n->call.function_name->k == N_ID)
    {
      Symbol *s = symbol_find_with_arity(generator->sm, n->call.function_name->string_value, n->call.arguments.count, n->ty);
      if (s)
      {
        if (s->parent and string_equal_ignore_case(s->parent->name, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->name, STRING_LITERAL("CREATE")) or string_equal_ignore_case(s->name, STRING_LITERAL("OPEN"))))
        {
          r.k = VALUE_KIND_POINTER;
          int md = n->call.arguments.count > 1 ? generate_expression(generator, n->call.arguments.data[1]).id : 0;
          fprintf(
              o,
              "  %%t%d = call ptr @__text_io_%s(i64 %d, ptr %%t%d)\n",
              r.id,
              string_equal_ignore_case(s->name, STRING_LITERAL("CREATE")) ? "create" : "open",
              md,
              n->call.arguments.count > 2 ? generate_expression(generator, n->call.arguments.data[2]).id : 0);
          break;
        }
        if (s->parent and string_equal_ignore_case(s->parent->name, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->name, STRING_LITERAL("CLOSE")) or string_equal_ignore_case(s->name, STRING_LITERAL("DELETE"))))
        {
          if (n->call.arguments.count > 0)
          {
            Value f = generate_expression(generator, n->call.arguments.data[0]);
            fprintf(
                o,
                "  call void @__text_io_%s(ptr %%t%d)\n",
                string_equal_ignore_case(s->name, STRING_LITERAL("CLOSE")) ? "close" : "delete",
                f.id);
          }
          break;
        }
        if (s->parent and string_equal_ignore_case(s->parent->name, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->name, STRING_LITERAL("GET")) or string_equal_ignore_case(s->name, STRING_LITERAL("GET_LINE"))))
        {
          if (n->call.arguments.count > 1)
          {
            Value f = generate_expression(generator, n->call.arguments.data[0]);
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = call i64 @__text_io_get(ptr %%t%d)\n", r.id, f.id);
          }
          else
          {
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = call i64 @__text_io_get(ptr @stdin)\n", r.id);
          }
          break;
        }
        if (s->parent and string_equal_ignore_case(s->parent->name, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->name, STRING_LITERAL("PUT")) or string_equal_ignore_case(s->name, STRING_LITERAL("PUT_LINE"))))
        {
          if (n->call.arguments.count > 1)
          {
            Value f = generate_expression(generator, n->call.arguments.data[0]);
            Value v = generate_expression(generator, n->call.arguments.data[1]);
            const char *func_name = string_equal_ignore_case(s->name, STRING_LITERAL("PUT")) ? "put" : "put_line";
            const char *suffix = "";
            const char *type_str = "";
            if (v.k == VALUE_KIND_INTEGER)
            {
              suffix = ".i64";
              type_str = "i64";
            }
            else if (v.k == VALUE_KIND_FLOAT)
            {
              suffix = ".f64";
              type_str = "double";
            }
            else
            {
              type_str = "ptr";
            }
            fprintf(o, "  call void @__text_io_%s%s(ptr %%t%d, %s %%t%d)\n", func_name, suffix, f.id, type_str, v.id);
          }
          else
          {
            Value v = generate_expression(generator, n->call.arguments.data[0]);
            const char *func_name = string_equal_ignore_case(s->name, STRING_LITERAL("PUT")) ? "put" : "put_line";
            const char *suffix = "";
            const char *type_str = "";
            if (v.k == VALUE_KIND_INTEGER)
            {
              suffix = ".i64";
              type_str = "i64";
            }
            else if (v.k == VALUE_KIND_FLOAT)
            {
              suffix = ".f64";
              type_str = "double";
            }
            else
            {
              type_str = "ptr";
            }
            fprintf(o, "  call void @__text_io_%s%s(ptr @stdout, %s %%t%d)\n", func_name, suffix, type_str, v.id);
          }
          break;
        }
        if (s->type_info and s->type_info->k == TYPE_STRING)
        {
          Value_Kind rk = token_kind_to_value_kind(s->type_info->element_type);
          r.k = rk;
          Syntax_Node *b = symbol_body(s, s->elaboration_level);
          Syntax_Node *sp = symbol_spec(s);
          int arid[64];
          Value_Kind ark[64];
          int arp[64];
          for (uint32_t i = 0; i < n->call.arguments.count and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->subprogram.parameters.count ? sp->subprogram.parameters.data[i] : 0;
            Syntax_Node *arg = n->call.arguments.data[i];
            Value av = {0, VALUE_KIND_INTEGER};
            Value_Kind ek = VALUE_KIND_INTEGER;
            bool rf = false;
            if (pm)
            {
              if (pm->symbol and pm->symbol->type_info)
                ek = token_kind_to_value_kind(pm->symbol->type_info);
              else if (pm->parameter.ty)
              {
                Type_Info *pt = resolve_subtype(generator->sm, pm->parameter.ty);
                ek = token_kind_to_value_kind(pt);
              }
              if (pm->parameter.mode & 2 and arg->k == N_ID)
              {
                rf = true;
                Symbol *as = arg->symbol ? arg->symbol : symbol_find(generator->sm, arg->string_value);
                if (as and as->level == 0)
                {
                  char nb[256];
                  if (as->parent and as->parent->name.string)
                  {
                    int n = 0;
                    for (uint32_t j = 0; j < as->parent->name.length; j++)
                      nb[n++] = toupper(as->parent->name.string[j]);
                    n += snprintf(nb + n, 256 - n, "_S%dE%d__", as->parent->scope, as->parent->elaboration_level);
                    for (uint32_t j = 0; j < as->name.length; j++)
                      nb[n++] = toupper(as->name.string[j]);
                    nb[n] = 0;
                  }
                  else
                    snprintf(nb, 256, "%.*s", (int) as->name.length, as->name.string);
                  av.id = new_temporary_register(generator);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = bitcast ptr @%s to ptr\n", av.id, nb);
                }
                else if (as and as->level >= 0 and as->level < generator->sm->lv)
                {
                  av.id = new_temporary_register(generator);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", av.id, as->elaboration_level);
                  int a2 = new_temporary_register(generator);
                  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a2, av.id);
                  av.id = a2;
                }
                else
                {
                  av.id = new_temporary_register(generator);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(
                      o,
                      "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                      av.id,
                      string_to_lowercase(arg->string_value),
                      as ? as->scope : 0,
                      as ? as->elaboration_level : 0);
                }
                ek = VALUE_KIND_POINTER;
              }
              else if (pm->parameter.mode & 2)
              {
                rf = true;
                av = generate_expression(generator, arg);
                int ap = new_temporary_register(generator);
                fprintf(o, "  %%t%d = alloca %s\n", ap, value_llvm_type_string(av.k));
                fprintf(
                    o, "  store %s %%t%d, ptr %%t%d\n", value_llvm_type_string(av.k), av.id, ap);
                av.id = ap;
                av.k = VALUE_KIND_POINTER;
                ek = VALUE_KIND_POINTER;
              }
              else
              {
                av = generate_expression(generator, arg);
              }
            }
            else
            {
              av = generate_expression(generator, arg);
            }
            if (not rf and ek != VALUE_KIND_INTEGER)
            {
              Value cv = value_cast(generator, av, ek);
              av = cv;
            }
            arid[i] = av.id;
            ark[i] = ek != VALUE_KIND_INTEGER ? ek : av.k;
            arp[i] = i < sp->subprogram.parameters.count and sp->subprogram.parameters.data[i]->parameter.mode & 2 ? 1 : 0;
          }
          char nb[256];
          encode_symbol_name(nb, 256, s, n->call.function_name->string_value, n->call.arguments.count, sp);
          fprintf(o, "  %%t%d = call %s @\"%s\"(", r.id, value_llvm_type_string(rk), nb);
          for (uint32_t i = 0; i < n->call.arguments.count; i++)
          {
            if (i)
              fprintf(o, ", ");
            fprintf(o, "%s %%t%d", value_llvm_type_string(ark[i]), arid[i]);
          }
          if (s->level > 0)
          {
            if (n->call.arguments.count > 0)
              fprintf(o, ", ");
            if (s->level >= generator->sm->lv)
              fprintf(o, "ptr %%__frame");
            else
              fprintf(o, "ptr %%__slnk");
          }
          fprintf(o, ")\n");
          for (uint32_t i = 0; i < n->call.arguments.count and i < 64; i++)
          {
            if (arp[i])
            {
              int lv = new_temporary_register(generator);
              fprintf(
                  o,
                  "  %%t%d = load %s, ptr %%t%d\n",
                  lv,
                  value_llvm_type_string(
                      ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]),
                  arid[i]);
              Value rv = {lv, ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]};
              Value cv = value_cast(generator, rv, token_kind_to_value_kind(n->call.arguments.data[i]->ty));
              Syntax_Node *tg = n->call.arguments.data[i];
              if (tg->k == N_ID)
              {
                Symbol *ts = tg->symbol ? tg->symbol : symbol_find(generator->sm, tg->string_value);
                if (ts)
                {
                  if (ts->level >= 0 and ts->level < generator->sm->lv)
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        ts->level,
                        string_to_lowercase(tg->string_value));
                  else
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        string_to_lowercase(tg->string_value),
                        ts->scope,
                        ts->elaboration_level);
                }
              }
            }
          }
          break;
        }
        else
        {
          r.k = VALUE_KIND_INTEGER;
          fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
        }
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
      }
    }
    else
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
    }
    break;
  }
  break;
  case N_AG:
    return generate_aggregate(generator, n, n->ty);
  case N_ALC:
  {
    r.k = VALUE_KIND_POINTER;
    Type_Info *et = n->ty and n->ty->element_type ? type_canonical_concrete(n->ty->element_type) : 0;
    uint32_t asz = 64;
    if (et and et->discriminants.count > 0)
      asz += et->discriminants.count * 8;
    fprintf(o, "  %%t%d = call ptr @malloc(i64 %u)\n", r.id, asz);
    if (n->allocator.initializer)
    {
      Value v = generate_expression(generator, n->allocator.initializer);
      v = value_cast(generator, v, VALUE_KIND_INTEGER);
      int op = new_temporary_register(generator);
      fprintf(
          o,
          "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n",
          op,
          r.id,
          et and et->discriminants.count > 0 ? (uint32_t) et->discriminants.count : 0);
      fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, op);
    }
  }
  break;
  case N_DRF:
  {
    Value p = generate_expression(generator, n->dereference.expression);
    if (p.k == VALUE_KIND_INTEGER)
    {
      int pp = new_temporary_register(generator);
      fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, p.id);
      p.id = pp;
      p.k = VALUE_KIND_POINTER;
    }
    Type_Info *dt = n->dereference.expression->ty ? type_canonical_concrete(n->dereference.expression->ty) : 0;
    dt = dt and dt->element_type ? type_canonical_concrete(dt->element_type) : 0;
    r.k = dt ? token_kind_to_value_kind(dt) : VALUE_KIND_INTEGER;
    Value pc = value_cast(generator, p, VALUE_KIND_INTEGER);
    int nc = new_temporary_register(generator);
    fprintf(o, "  %%t%d = icmp eq i64 %%t%d, 0\n", nc, pc.id);
    int ne = new_label_block(generator), nd = new_label_block(generator);
    emit_conditional_branch(generator, nc, ne, nd);
    emit_label(generator, ne);
    fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
    emit_label(generator, nd);
    fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", r.id, value_llvm_type_string(r.k), p.id);
  }
  break;
  case N_CVT:
  {
    Value e = generate_expression(generator, n->conversion.expression);
    r = value_cast(generator, e, r.k);
  }
  break;
  case N_CHK:
  {
    Value e = generate_expression(generator, n->check.expression);
    Type_Info *t = n->check.expression->ty ? type_canonical_concrete(n->check.expression->ty) : 0;
    if (t and t->k == TYPE_FLOAT and (t->low_bound != TY_INT->low_bound or t->high_bound != TY_INT->high_bound))
      r = generate_float_range_check(generator, e, t, n->check.exception_name, r.k);
    else if (t and t->k == TYPE_ARRAY and (t->low_bound != 0 or t->high_bound != -1))
    {
      Type_Info *et = n->check.expression->ty;
      r = generate_array_bounds_check(generator, e, t, et, n->check.exception_name, r.k);
    }
    else if (
        t
        and (t->k == TYPE_INTEGER or t->k == TYPE_ENUMERATION or t->k == TYPE_DERIVED or t->k == TYPE_CHARACTER)
        and (t->low_bound != TY_INT->low_bound or t->high_bound != TY_INT->high_bound))
      r = generate_discrete_range_check(generator, e, t, n->check.exception_name, r.k);
    else
      r = value_cast(generator, e, r.k);
  }
  break;
  case N_RN:
  {
    Value lo = generate_expression(generator, n->range.low_bound);
    r = value_cast(generator, lo, r.k);
  }
  break;
  default:
    r.k = VALUE_KIND_INTEGER;
    fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
  }
  return r;
}
static void generate_statement_sequence(Code_Generator *generator, Syntax_Node *n)
{
  FILE *o = generator->o;
  if (not n)
    return;
  switch (n->k)
  {
  case N_NS:
    fprintf(o, "  ; null\n");
    break;
  case N_AS:
  {
    Value v = generate_expression(generator, n->assignment.value);
    if (n->assignment.target->k == N_ID)
    {
      Symbol *s = n->assignment.target->symbol;
      Type_Info *st = s and s->type_info ? type_canonical_concrete(s->type_info) : 0;
      Value_Kind k = st ? token_kind_to_value_kind(st) : VALUE_KIND_INTEGER;

      // Check if this is an array assignment
      bool is_array_assign = (st and st->k == TYPE_ARRAY and v.k == VALUE_KIND_POINTER
                              and st->low_bound != 0 and st->high_bound > 0);

      if (not is_array_assign)
        v = value_cast(generator, v, k);

      if (is_array_assign)
      {
        // Use memcpy for array assignment
        int64_t count = st->high_bound - st->low_bound + 1;
        int64_t elem_size = st->element_type->k == TYPE_INTEGER ? 8 : 4;
        int64_t total_size = count * elem_size;

        // Get target address
        if (s->level == 0)
        {
          char nb[256];
          if (s->parent and (uintptr_t) s->parent > 4096 and s->parent->name.string)
          {
            int n = 0;
            for (uint32_t i = 0; i < s->parent->name.length; i++)
              nb[n++] = toupper(s->parent->name.string[i]);
            n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
            for (uint32_t i = 0; i < s->name.length; i++)
              nb[n++] = toupper(s->name.string[i]);
            nb[n] = 0;
          }
          else
            snprintf(nb, 256, "%.*s", (int) s->name.length, s->name.string);
          fprintf(o, "  call void @llvm.memcpy.p0.p0.i64(ptr @%s, ptr %%t%d, i64 %lld, i1 false)\n",
                  nb, v.id, (long long) total_size);
        }
        else if (s->level >= 0 and s->level < generator->sm->lv)
        {
          int p = new_temporary_register(generator);
          int level_diff = generator->sm->lv - s->level - 1;
          int slnk_ptr;
          if (level_diff == 0)
          {
            slnk_ptr = new_temporary_register(generator);
            fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
          }
          else
          {
            slnk_ptr = new_temporary_register(generator);
            fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
            for (int hop = 0; hop < level_diff; hop++)
            {
              int next_slnk = new_temporary_register(generator);
              fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
              int loaded_slnk = new_temporary_register(generator);
              fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
              slnk_ptr = loaded_slnk;
            }
          }
          fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->elaboration_level);
          int a = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
          fprintf(o, "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %lld, i1 false)\n",
                  a, v.id, (long long) total_size);
        }
        else
        {
          fprintf(o, "  call void @llvm.memcpy.p0.p0.i64(ptr %%v.%s.sc%u.%u, ptr %%t%d, i64 %lld, i1 false)\n",
                  string_to_lowercase(n->assignment.target->string_value),
                  s ? s->scope : 0,
                  s ? s->elaboration_level : 0,
                  v.id,
                  (long long) total_size);
        }
      }
      else if (s and s->level == 0)
      {
        char nb[256];
        if (s->parent and (uintptr_t) s->parent > 4096 and s->parent->name.string)
        {
          int n = 0;
          for (uint32_t i = 0; i < s->parent->name.length; i++)
            nb[n++] = toupper(s->parent->name.string[i]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
          for (uint32_t i = 0; i < s->name.length; i++)
            nb[n++] = toupper(s->name.string[i]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->name.length, s->name.string);
        fprintf(o, "  store %s %%t%d, ptr @%s\n", value_llvm_type_string(k), v.id, nb);
      }
      else if (s and s->level >= 0 and s->level < generator->sm->lv)
      {
        int p = new_temporary_register(generator);
        int level_diff = generator->sm->lv - s->level - 1;
        int slnk_ptr;
        if (level_diff == 0)
        {
          slnk_ptr = new_temporary_register(generator);
          fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
        }
        else
        {
          slnk_ptr = new_temporary_register(generator);
          fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
          for (int hop = 0; hop < level_diff; hop++)
          {
            int next_slnk = new_temporary_register(generator);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
            int loaded_slnk = new_temporary_register(generator);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
            slnk_ptr = loaded_slnk;
          }
        }
        fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->elaboration_level);
        int a = new_temporary_register(generator);
        fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
        fprintf(o, "  store %s %%t%d, ptr %%t%d\n", value_llvm_type_string(k), v.id, a);
      }
      else
        fprintf(
            o,
            "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
            value_llvm_type_string(k),
            v.id,
            string_to_lowercase(n->assignment.target->string_value),
            s ? s->scope : 0,
            s ? s->elaboration_level : 0);
    }
    else if (n->assignment.target->k == N_IX)
    {
      Value p = generate_expression(generator, n->assignment.target->index.prefix);
      if (p.k == VALUE_KIND_INTEGER)
      {
        int pp = new_temporary_register(generator);
        fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, p.id);
        p.id = pp;
        p.k = VALUE_KIND_POINTER;
      }
      Value i0 = value_cast(generator, generate_expression(generator, n->assignment.target->index.indices.data[0]), VALUE_KIND_INTEGER);
      Type_Info *at = n->assignment.target->index.prefix->ty ? type_canonical_concrete(n->assignment.target->index.prefix->ty) : 0;
      Type_Info *et = at and at->element_type ? type_canonical_concrete(at->element_type) : 0;
      const char *elem_type_str = ada_to_c_type_string(et);
      int adj_idx = i0.id;
      if (at and at->k == TYPE_ARRAY and at->low_bound != 0)
      {
        int adj = new_temporary_register(generator);
        fprintf(o, "  %%t%d = sub i64 %%t%d, %lld\n", adj, i0.id, (long long) at->low_bound);
        adj_idx = adj;
      }
      int ep = new_temporary_register(generator);
      if (at and at->k == TYPE_ARRAY and at->high_bound >= at->low_bound)
      {
        int asz = (int) (at->high_bound - at->low_bound + 1);
        fprintf(
            o,
            "  %%t%d = getelementptr [%d x %s], ptr %%t%d, i64 0, i64 %%t%d\n",
            ep,
            asz,
            elem_type_str,
            p.id,
            adj_idx);
      }
      else
      {
        fprintf(o, "  %%t%d = getelementptr %s, ptr %%t%d, i64 %%t%d\n", ep, elem_type_str, p.id, adj_idx);
      }
      Value_Kind target_kind = et ? token_kind_to_value_kind(et) : VALUE_KIND_INTEGER;
      v = value_cast(generator, v, target_kind);
      if (strcmp(elem_type_str, "i64") != 0 and target_kind == VALUE_KIND_INTEGER)
      {
        int tv = new_temporary_register(generator);
        fprintf(o, "  %%t%d = trunc i64 %%t%d to %s\n", tv, v.id, elem_type_str);
        fprintf(o, "  store %s %%t%d, ptr %%t%d\n", elem_type_str, tv, ep);
      }
      else
      {
        fprintf(o, "  store %s %%t%d, ptr %%t%d\n", value_llvm_type_string(target_kind), v.id, ep);
      }
    }
    else if (n->assignment.target->k == N_SEL)
    {
      Type_Info *pt = n->assignment.target->selected_component.prefix->ty ? type_canonical_concrete(n->assignment.target->selected_component.prefix->ty) : 0;
      Value p = {new_temporary_register(generator), VALUE_KIND_POINTER};
      if (n->assignment.target->selected_component.prefix->k == N_ID)
      {
        Symbol *s = n->assignment.target->selected_component.prefix->symbol ? n->assignment.target->selected_component.prefix->symbol : symbol_find(generator->sm, n->assignment.target->selected_component.prefix->string_value);
        if (s and s->level >= 0 and s->level < generator->sm->lv)
          fprintf(
              o,
              "  %%t%d = bitcast ptr %%lnk.%d.%.*s to ptr\n",
              p.id,
              s->level,
              (int) n->assignment.target->selected_component.prefix->string_value.length,
              n->assignment.target->selected_component.prefix->string_value.string);
        else
          fprintf(
              o,
              "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
              p.id,
              string_to_lowercase(n->assignment.target->selected_component.prefix->string_value),
              s ? s->scope : 0,
              s ? s->elaboration_level : 0);
      }
      else
      {
        p = generate_expression(generator, n->assignment.target->selected_component.prefix);
      }
      if (pt and pt->k == TYPE_RECORD)
      {
        if (pt->is_packed)
        {
          for (uint32_t i = 0; i < pt->components.count; i++)
          {
            Syntax_Node *c = pt->components.data[i];
            if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, n->assignment.target->selected_component.selector))
            {
              v = value_cast(generator, v, VALUE_KIND_INTEGER);
              int bp = new_temporary_register(generator);
              fprintf(o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", bp, p.id);
              int bo = new_temporary_register(generator);
              fprintf(o, "  %%t%d = add i64 %%t%d, %u\n", bo, bp, c->component_decl.offset / 8);
              int pp = new_temporary_register(generator);
              fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, bo);
              int ov = new_temporary_register(generator);
              fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", ov, pp);
              uint64_t mk = (1ULL << c->component_decl.bit_offset) - 1;
              int sh = new_temporary_register(generator);
              fprintf(o, "  %%t%d = shl i64 %%t%d, %u\n", sh, v.id, c->component_decl.offset % 8);
              int ms = new_temporary_register(generator);
              fprintf(
                  o,
                  "  %%t%d = and i64 %%t%d, %llu\n",
                  ms,
                  sh,
                  (unsigned long long) (mk << (c->component_decl.offset % 8)));
              uint64_t cmk = ~(mk << (c->component_decl.offset % 8));
              int cl = new_temporary_register(generator);
              fprintf(o, "  %%t%d = and i64 %%t%d, %llu\n", cl, ov, (unsigned long long) cmk);
              int nv_ = new_temporary_register(generator);
              fprintf(o, "  %%t%d = or i64 %%t%d, %%t%d\n", nv_, cl, ms);
              fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", nv_, pp);
              break;
            }
          }
        }
        else
        {
          for (uint32_t i = 0; i < pt->components.count; i++)
          {
            Syntax_Node *c = pt->components.data[i];
            if (c->k == N_CM and string_equal_ignore_case(c->component_decl.name, n->assignment.target->selected_component.selector))
            {
              int ep = new_temporary_register(generator);
              fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, c->component_decl.offset);
              v = value_cast(generator, v, VALUE_KIND_INTEGER);
              fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
              break;
            }
          }
        }
      }
    }
    else
    {
      v = value_cast(generator, v, VALUE_KIND_INTEGER);
      fprintf(o, "  ; store to complex lvalue\n");
    }
  }
  break;
  case N_IF:
  {
    Value c = value_to_boolean(generator, generate_expression(generator, n->if_stmt.condition));
    int ct = new_temporary_register(generator);
    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
    int lt = new_label_block(generator), lf = new_label_block(generator), ld = new_label_block(generator);
    emit_conditional_branch(generator, ct, lt, lf);
    emit_label(generator, lt);
    for (uint32_t i = 0; i < n->if_stmt.then_statements.count; i++)
      generate_statement_sequence(generator, n->if_stmt.then_statements.data[i]);
    emit_branch(generator, ld);
    emit_label(generator, lf);
    if (n->if_stmt.elsif_statements.count > 0)
    {
      for (uint32_t i = 0; i < n->if_stmt.elsif_statements.count; i++)
      {
        Syntax_Node *e = n->if_stmt.elsif_statements.data[i];
        Value ec = value_to_boolean(generator, generate_expression(generator, e->if_stmt.condition));
        int ect = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ect, ec.id);
        int let = new_label_block(generator), lef = new_label_block(generator);
        emit_conditional_branch(generator, ect, let, lef);
        emit_label(generator, let);
        for (uint32_t j = 0; j < e->if_stmt.then_statements.count; j++)
          generate_statement_sequence(generator, e->if_stmt.then_statements.data[j]);
        emit_branch(generator, ld);
        emit_label(generator, lef);
      }
    }
    if (n->if_stmt.else_statements.count > 0)
    {
      for (uint32_t i = 0; i < n->if_stmt.else_statements.count; i++)
        generate_statement_sequence(generator, n->if_stmt.else_statements.data[i]);
    }
    emit_branch(generator, ld);
    emit_label(generator, ld);
  }
  break;
  case N_CS:
  {
    Value ex = generate_expression(generator, n->case_stmt.expression);
    int ld = new_label_block(generator);
    Node_Vector lb = {0};
    for (uint32_t i = 0; i < n->case_stmt.alternatives.count; i++)
    {
      Syntax_Node *a = n->case_stmt.alternatives.data[i];
      int la = new_label_block(generator);
      nv(&lb, ND(INT, n->location));
      lb.data[i]->integer_value = la;
    }
    for (uint32_t i = 0; i < n->case_stmt.alternatives.count; i++)
    {
      Syntax_Node *a = n->case_stmt.alternatives.data[i];
      int la = lb.data[i]->integer_value;
      for (uint32_t j = 0; j < a->choices.items.count; j++)
      {
        Syntax_Node *ch = a->choices.items.data[j];
        if (ch->k == N_ID and string_equal_ignore_case(ch->string_value, STRING_LITERAL("others")))
        {
          emit_branch(generator, la);
          goto cs_sw_done;
        }
        Type_Info *cht = ch->ty ? type_canonical_concrete(ch->ty) : 0;
        if (ch->k == N_ID and cht and (cht->low_bound != 0 or cht->high_bound != 0))
        {
          int lo_id = new_temporary_register(generator);
          fprintf(o, "  %%t%d = add i64 0, %lld\n", lo_id, (long long) cht->low_bound);
          int hi_id = new_temporary_register(generator);
          fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_id, (long long) cht->high_bound);
          int cge = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %%t%d\n", cge, ex.id, lo_id);
          int cle = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", cle, ex.id, hi_id);
          int ca = new_temporary_register(generator);
          fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", ca, cge, cle);
          int lnx = i + 1 < n->case_stmt.alternatives.count ? lb.data[i + 1]->integer_value : ld;
          emit_conditional_branch(generator, ca, la, lnx);
          continue;
        }
        Value cv = generate_expression(generator, ch);
        cv = value_cast(generator, cv, ex.k);
        if (ch->k == N_RN)
        {
          Value lo = generate_expression(generator, ch->range.low_bound);
          lo = value_cast(generator, lo, ex.k);
          Value hi = generate_expression(generator, ch->range.high_bound);
          hi = value_cast(generator, hi, ex.k);
          int cge = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %%t%d\n", cge, ex.id, lo.id);
          int cle = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", cle, ex.id, hi.id);
          int ca = new_temporary_register(generator);
          fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", ca, cge, cle);
          int lnx = i + 1 < n->case_stmt.alternatives.count ? lb.data[i + 1]->integer_value : ld;
          emit_conditional_branch(generator, ca, la, lnx);
        }
        else
        {
          int ceq = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", ceq, ex.id, cv.id);
          int lnx = i + 1 < n->case_stmt.alternatives.count ? lb.data[i + 1]->integer_value : ld;
          emit_conditional_branch(generator, ceq, la, lnx);
        }
      }
    }
  cs_sw_done:;
    for (uint32_t i = 0; i < n->case_stmt.alternatives.count; i++)
    {
      Syntax_Node *a = n->case_stmt.alternatives.data[i];
      int la = lb.data[i]->integer_value;
      emit_label(generator, la);
      for (uint32_t j = 0; j < a->exception_handler.statements.count; j++)
        generate_statement_sequence(generator, a->exception_handler.statements.data[j]);
      emit_branch(generator, ld);
    }
    emit_label(generator, ld);
  }
  break;
  case N_LP:
  {
    int lb = new_label_block(generator), lc = new_label_block(generator), le = new_label_block(generator);
    if (generator->ls < 64)
      generator->ll[generator->ls++] = le;
    if (n->loop_stmt.label.string)
    {
      slv(&generator->lbs, n->loop_stmt.label);
      nv(&n->loop_stmt.locks, ND(INT, n->location));
      n->loop_stmt.locks.data[n->loop_stmt.locks.count - 1]->integer_value = le;
    }
    Syntax_Node *fv = 0;
    Type_Info *ft = 0;
    int hi_var = -1;
    if (n->loop_stmt.iterator and n->loop_stmt.iterator->k == N_BIN and n->loop_stmt.iterator->binary_node.op == T_IN and n->loop_stmt.iterator->binary_node.left->k == N_ID)
    {
      fv = n->loop_stmt.iterator->binary_node.left;
      ft = n->loop_stmt.iterator->binary_node.right->ty;
      if (ft)
      {
        Symbol *vs = fv->symbol;
        if (vs)
        {
          fprintf(o, "  %%v.%s.sc%u.%u = alloca i64\n", string_to_lowercase(fv->string_value), vs->scope, vs->elaboration_level);
          Syntax_Node *rng = n->loop_stmt.iterator->binary_node.right;
          hi_var = new_temporary_register(generator);
          fprintf(o, "  %%v.__for_hi_%d = alloca i64\n", hi_var);
          int ti = new_temporary_register(generator);
          if (rng and rng->k == N_RN)
          {
            Value lo = value_cast(generator, generate_expression(generator, rng->range.low_bound), VALUE_KIND_INTEGER);
            fprintf(o, "  %%t%d = add i64 %%t%d, 0\n", ti, lo.id);
            Value hi = value_cast(generator, generate_expression(generator, rng->range.high_bound), VALUE_KIND_INTEGER);
            fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi.id, hi_var);
          }
          else if (
              rng and rng->k == N_AT
              and string_equal_ignore_case(rng->attribute.attribute_name, STRING_LITERAL("RANGE")))
          {
            Type_Info *at = rng->attribute.prefix ? type_canonical_concrete(rng->attribute.prefix->ty) : 0;
            if (at and at->k == TYPE_ARRAY)
            {
              if (at->low_bound == 0 and at->high_bound == -1 and rng->attribute.prefix)
              {
                Value pv = generate_expression(generator, rng->attribute.prefix);
                int blo, bhi;
                get_fat_pointer_bounds(generator, pv.id, &blo, &bhi);
                fprintf(o, "  %%t%d = add i64 0, 0\n", ti);
                fprintf(
                    o,
                    "  store i64 %%t%d, ptr %%v.%s.sc%u.%u\n",
                    blo,
                    string_to_lowercase(fv->string_value),
                    vs->scope,
                    vs->elaboration_level);
                fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", bhi, hi_var);
                ti = blo;
              }
              else
              {
                fprintf(o, "  %%t%d = add i64 0, %lld\n", ti, (long long) at->low_bound);
                int hi_t = new_temporary_register(generator);
                fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_t, (long long) at->high_bound);
                fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi_t, hi_var);
              }
            }
            else
            {
              fprintf(o, "  %%t%d = add i64 0, %lld\n", ti, (long long) ft->low_bound);
              int hi_t = new_temporary_register(generator);
              fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_t, (long long) ft->high_bound);
              fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi_t, hi_var);
            }
          }
          else
          {
            fprintf(o, "  %%t%d = add i64 0, %lld\n", ti, (long long) ft->low_bound);
            int hi_t = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_t, (long long) ft->high_bound);
            fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi_t, hi_var);
          }
          if (vs->level == 0)
          {
            char nb[256];
            if (vs->parent and vs->parent->name.string)
            {
              int n = 0;
              for (uint32_t i = 0; i < vs->parent->name.length; i++)
                nb[n++] = toupper(vs->parent->name.string[i]);
              n += snprintf(nb + n, 256 - n, "_S%dE%d__", vs->parent->scope, vs->parent->elaboration_level);
              for (uint32_t i = 0; i < vs->name.length; i++)
                nb[n++] = toupper(vs->name.string[i]);
              nb[n] = 0;
            }
            else
              snprintf(nb, 256, "%.*s", (int) vs->name.length, vs->name.string);
            fprintf(o, "  store i64 %%t%d, ptr @%s\n", ti, nb);
          }
          else
            fprintf(
                o,
                "  store i64 %%t%d, ptr %%v.%s.sc%u.%u\n",
                ti,
                string_to_lowercase(fv->string_value),
                vs->scope,
                vs->elaboration_level);
        }
      }
    }
    emit_branch(generator, lb);
    emit_label(generator, lb);
    if (n->loop_stmt.iterator)
    {
      if (fv and ft and hi_var >= 0)
      {
        Symbol *vs = fv->symbol;
        int cv = new_temporary_register(generator);
        if (vs->level == 0)
        {
          char nb[256];
          if (vs->parent and vs->parent->name.string)
          {
            int n = 0;
            for (uint32_t i = 0; i < vs->parent->name.length; i++)
              nb[n++] = toupper(vs->parent->name.string[i]);
            n += snprintf(nb + n, 256 - n, "_S%dE%d__", vs->parent->scope, vs->parent->elaboration_level);
            for (uint32_t i = 0; i < vs->name.length; i++)
              nb[n++] = toupper(vs->name.string[i]);
            nb[n] = 0;
          }
          else
            snprintf(nb, 256, "%.*s", (int) vs->name.length, vs->name.string);
          fprintf(o, "  %%t%d = load i64, ptr @%s\n", cv, nb);
        }
        else
        {
          fprintf(
              o,
              "  %%t%d = load i64, ptr %%v.%s.sc%u.%u\n",
              cv,
              string_to_lowercase(fv->string_value),
              vs->scope,
              vs->elaboration_level);
        }
        int hv = new_temporary_register(generator);
        fprintf(o, "  %%t%d = load i64, ptr %%v.__for_hi_%d\n", hv, hi_var);
        int cmp = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", cmp, cv, hv);
        emit_conditional_branch(generator, cmp, lc, le);
      }
      else
      {
        Value c = value_to_boolean(generator, generate_expression(generator, n->loop_stmt.iterator));
        int ct = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
        emit_conditional_branch(generator, ct, lc, le);
      }
    }
    else
      emit_branch(generator, lc);
    emit_label(generator, lc);
    for (uint32_t i = 0; i < n->loop_stmt.statements.count; i++)
      generate_statement_sequence(generator, n->loop_stmt.statements.data[i]);
    if (fv and ft)
    {
      Symbol *vs = fv->symbol;
      if (vs)
      {
        int cv = new_temporary_register(generator);
        if (vs->level == 0)
        {
          char nb[256];
          if (vs->parent and vs->parent->name.string)
          {
            int n = 0;
            for (uint32_t i = 0; i < vs->parent->name.length; i++)
              nb[n++] = toupper(vs->parent->name.string[i]);
            n += snprintf(nb + n, 256 - n, "_S%dE%d__", vs->parent->scope, vs->parent->elaboration_level);
            for (uint32_t i = 0; i < vs->name.length; i++)
              nb[n++] = toupper(vs->name.string[i]);
            nb[n] = 0;
          }
          else
            snprintf(nb, 256, "%.*s", (int) vs->name.length, vs->name.string);
          fprintf(o, "  %%t%d = load i64, ptr @%s\n", cv, nb);
          int nv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", nv, cv);
          fprintf(o, "  store i64 %%t%d, ptr @%s\n", nv, nb);
        }
        else
        {
          fprintf(
              o,
              "  %%t%d = load i64, ptr %%v.%s.sc%u.%u\n",
              cv,
              string_to_lowercase(fv->string_value),
              vs->scope,
              vs->elaboration_level);
          int nv = new_temporary_register(generator);
          fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", nv, cv);
          fprintf(
              o,
              "  store i64 %%t%d, ptr %%v.%s.sc%u.%u\n",
              nv,
              string_to_lowercase(fv->string_value),
              vs->scope,
              vs->elaboration_level);
        }
      }
    }
    int lmd_id = 0;
    if (generator->ls <= 64)
    {
      lmd_id = normalize_name(generator);
      generator->lopt[lmd_id] = n->loop_stmt.is_reverse ? 7 : 0;
    }
    if (lmd_id)
    {
      fprintf(o, "  br label %%Source_Location%d", lb);
      emit_loop_metadata(o, lmd_id);
      fprintf(o, "\n");
    }
    else
      emit_branch(generator, lb);
    emit_label(generator, le);
    if (generator->ls > 0)
      generator->ls--;
  }
  break;
  case N_EX:
  {
    if (n->exit_stmt.label.string)
    {
      int li = find_label(generator, n->exit_stmt.label);
      if (li >= 0)
      {
        int le = generator->ll[li];
        if (n->exit_stmt.condition)
        {
          Value c = value_to_boolean(generator, generate_expression(generator, n->exit_stmt.condition));
          int ct = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
          int lc = new_label_block(generator);
          emit_conditional_branch(generator, ct, le, lc);
          emit_label(generator, lc);
        }
        else
          emit_branch(generator, le);
      }
      else
      {
        if (n->exit_stmt.condition)
        {
          Value c = value_to_boolean(generator, generate_expression(generator, n->exit_stmt.condition));
          int ct = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
          int le = generator->ls > 0 ? generator->ll[generator->ls - 1] : new_label_block(generator), lc = new_label_block(generator);
          emit_conditional_branch(generator, ct, le, lc);
          emit_label(generator, lc);
        }
        else
        {
          int le = generator->ls > 0 ? generator->ll[generator->ls - 1] : new_label_block(generator);
          emit_branch(generator, le);
        }
      }
    }
    else
    {
      if (n->exit_stmt.condition)
      {
        Value c = value_to_boolean(generator, generate_expression(generator, n->exit_stmt.condition));
        int ct = new_temporary_register(generator);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
        int le = generator->ls > 0 ? generator->ll[generator->ls - 1] : new_label_block(generator), lc = new_label_block(generator);
        emit_conditional_branch(generator, ct, le, lc);
        emit_label(generator, lc);
      }
      else
      {
        int le = generator->ls > 0 ? generator->ll[generator->ls - 1] : new_label_block(generator);
        emit_branch(generator, le);
      }
    }
  }
  break;
  case N_GT:
  {
    int bb = get_or_create_label_basic_block(generator, n->goto_stmt.label);
    fprintf(o, "  br label %%Source_Location%d\n", bb);
    int ul = new_label_block(generator);
    emit_label(generator, ul);
    fprintf(o, "  unreachable\n");
  }
  break;
  case N_RT:
  {
    if (n->return_stmt.value)
    {
      Value v = generate_expression(generator, n->return_stmt.value);
      Type_Info *vt = n->return_stmt.value->ty ? type_canonical_concrete(n->return_stmt.value->ty) : 0;

      // Check for runtime-sized array FIRST (check for size variable)
      bool handled_runtime_array = false;
      if (v.k == VALUE_KIND_POINTER and vt and vt->k == TYPE_ARRAY
          and n->return_stmt.value->k == N_ID and n->return_stmt.value->symbol)
      {
        Symbol *s = n->return_stmt.value->symbol;
        // Check if this is a local variable with a size variable (runtime-sized)
        if (s->k == 0 and s->scope > 0)
        {
          // Try to load the size variable
          int size_val_reg = new_temporary_register(generator);
          int ss_ptr = new_temporary_register(generator);

          fprintf(o, "  %%t%d = load i64, ptr %%v.%s.sc%u.%u__size\n",
              size_val_reg,
              string_to_lowercase(s->name),
              s->scope,
              s->elaboration_level);

          // Allocate on secondary stack and copy with runtime size
          fprintf(o, "  %%t%d = call ptr @__ada_ss_allocate(i64 %%t%d)\n", ss_ptr, size_val_reg);
          fprintf(o, "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %%t%d, i1 false)\n",
                  ss_ptr, v.id, size_val_reg);
          fprintf(o, "  ret ptr %%t%d\n", ss_ptr);
          handled_runtime_array = true;
        }
      }

      if (not handled_runtime_array)
      {
        // Check for compile-time sized array
        if (v.k == VALUE_KIND_POINTER and vt and vt->k == TYPE_ARRAY
            and vt->low_bound != 0 and vt->high_bound > 0 and vt->high_bound >= vt->low_bound)
        {
          // Compile-time sized array - Allocate on secondary stack and copy array data
          int64_t count = vt->high_bound - vt->low_bound + 1;
          int64_t elem_size = vt->element_type->k == TYPE_INTEGER ? 8 : 4;
          int64_t total_size = count * elem_size;

          int sz_reg = new_temporary_register(generator);
          int ss_ptr = new_temporary_register(generator);

          fprintf(o, "  %%t%d = add i64 0, %lld\n", sz_reg, total_size);
          fprintf(o, "  %%t%d = call ptr @__ada_ss_allocate(i64 %%t%d)\n", ss_ptr, sz_reg);
          fprintf(o, "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %lld, i1 false)\n",
                  ss_ptr, v.id, total_size);
          fprintf(o, "  ret ptr %%t%d\n", ss_ptr);
        }
        else
        {
          // Other types or unconstrained arrays
          fprintf(o, "  ret %s %%t%d\n", value_llvm_type_string(v.k), v.id);
        }
      }
    }
    else
      fprintf(o, "  ret void\n");
  }
  break;
  case N_RS:
  {
    String_Slice ec =
        n->raise_stmt.exception_choice and n->raise_stmt.exception_choice->k == N_ID ? n->raise_stmt.exception_choice->string_value : STRING_LITERAL("PROGRAM_ERROR");
    emit_exception(generator, ec);
    int exh = new_temporary_register(generator);
    fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", exh);
    fprintf(o, "  store ptr @.ex.%.*s, ptr @__ex_cur\n", (int) ec.length, ec.string);
    fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", exh);
    fprintf(o, "  unreachable\n");
  }
  break;
  case N_CLT:
  {
    if (n->code_stmt.name->k == N_ID)
    {
      Symbol *s = symbol_find_with_arity(generator->sm, n->code_stmt.name->string_value, n->code_stmt.arguments.count, 0);
      if (s)
      {
        Syntax_Node *b = symbol_body(s, s->elaboration_level);
        if (b)
        {
          int arid[64];
          Value_Kind ark[64];
          int arp[64];
          Syntax_Node *sp = symbol_spec(s);
          for (uint32_t i = 0; i < n->code_stmt.arguments.count and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->subprogram.parameters.count ? sp->subprogram.parameters.data[i] : 0;
            Syntax_Node *arg = n->code_stmt.arguments.data[i];
            Value av = {0, VALUE_KIND_INTEGER};
            Value_Kind ek = VALUE_KIND_INTEGER;
            bool rf = false;

            // Check if parameter is unconstrained array
            Type_Info *param_type = 0;
            bool param_is_unconstrained = false;
            if (pm)
            {
              if (pm->symbol and pm->symbol->type_info)
                param_type = type_canonical_concrete(pm->symbol->type_info);
              else if (pm->parameter.ty)
                param_type = type_canonical_concrete(resolve_subtype(generator->sm, pm->parameter.ty));

              // Unconstrained arrays: check if high <= 0 (catches both high=-1 and high=0 sentinels)
              param_is_unconstrained = param_type and param_type->k == TYPE_ARRAY
                                       and param_type->low_bound == 0 and param_type->high_bound <= 0;
            }

            // Check if argument is constrained array
            Type_Info *arg_type = arg->ty ? type_canonical_concrete(arg->ty) : 0;
            bool arg_is_constrained_array = arg_type and arg_type->k == TYPE_ARRAY
                                            and not (arg_type->low_bound == 0 and arg_type->high_bound <= 0);

            if (pm)
            {
              if (pm->symbol and pm->symbol->type_info)
                ek = token_kind_to_value_kind(pm->symbol->type_info);
              else if (pm->parameter.ty)
              {
                Type_Info *pt = resolve_subtype(generator->sm, pm->parameter.ty);
                ek = token_kind_to_value_kind(pt);
              }

              // Handle unconstrained array parameters with constrained array arguments
              if (param_is_unconstrained and arg_is_constrained_array and arg->k == N_ID)
              {
                // Construct fat pointer on stack
                Symbol *as = arg->symbol ? arg->symbol : symbol_find(generator->sm, arg->string_value);

                // Get pointer to array data
                int data_ptr = new_temporary_register(generator);
                if (as and as->level >= 0 and as->level < generator->sm->lv)
                {
                  // Nested variable - get from frame
                  int fp = new_temporary_register(generator);
                  fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", fp, as->elaboration_level);
                  int vp = new_temporary_register(generator);
                  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", vp, fp);
                  fprintf(o, "  %%t%d = bitcast ptr %%t%d to ptr\n", data_ptr, vp);
                }
                else
                {
                  fprintf(o, "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                         data_ptr,
                         string_to_lowercase(arg->string_value),
                         as ? as->scope : 0,
                         as ? as->elaboration_level : 0);
                }

                // Allocate bounds structure on stack
                int bounds_alloc = new_temporary_register(generator);
                fprintf(o, "  %%t%d = alloca {i64,i64}\n", bounds_alloc);

                // Store low bound
                int lo_ptr = new_temporary_register(generator);
                fprintf(o, "  %%t%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 0\n", lo_ptr, bounds_alloc);
                fprintf(o, "  store i64 %lld, ptr %%t%d\n", (long long)arg_type->low_bound, lo_ptr);

                // Store high bound
                int hi_ptr = new_temporary_register(generator);
                fprintf(o, "  %%t%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 1\n", hi_ptr, bounds_alloc);
                fprintf(o, "  store i64 %lld, ptr %%t%d\n", (long long)arg_type->high_bound, hi_ptr);

                // Allocate fat pointer structure on stack
                int fat_ptr = new_temporary_register(generator);
                fprintf(o, "  %%t%d = alloca {ptr,ptr}\n", fat_ptr);

                // Store data pointer in fat pointer
                int data_field = new_temporary_register(generator);
                fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 0\n", data_field, fat_ptr);
                fprintf(o, "  store ptr %%t%d, ptr %%t%d\n", data_ptr, data_field);

                // Store bounds pointer in fat pointer
                int bounds_field = new_temporary_register(generator);
                fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 1\n", bounds_field, fat_ptr);
                fprintf(o, "  store ptr %%t%d, ptr %%t%d\n", bounds_alloc, bounds_field);

                av.id = fat_ptr;
                av.k = VALUE_KIND_POINTER;
                ek = VALUE_KIND_POINTER;
              }
              else if (pm->parameter.mode & 2 and arg->k == N_ID)
              {
                rf = true;
                Symbol *as = arg->symbol ? arg->symbol : symbol_find(generator->sm, arg->string_value);
                if (as and as->level == 0)
                {
                  char nb[256];
                  if (as->parent and as->parent->name.string)
                  {
                    int n = 0;
                    for (uint32_t j = 0; j < as->parent->name.length; j++)
                      nb[n++] = toupper(as->parent->name.string[j]);
                    n += snprintf(nb + n, 256 - n, "_S%dE%d__", as->parent->scope, as->parent->elaboration_level);
                    for (uint32_t j = 0; j < as->name.length; j++)
                      nb[n++] = toupper(as->name.string[j]);
                    nb[n] = 0;
                  }
                  else
                    snprintf(nb, 256, "%.*s", (int) as->name.length, as->name.string);
                  av.id = new_temporary_register(generator);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = bitcast ptr @%s to ptr\n", av.id, nb);
                }
                else if (as and as->level >= 0 and as->level < generator->sm->lv)
                {
                  av.id = new_temporary_register(generator);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", av.id, as->elaboration_level);
                  int a2 = new_temporary_register(generator);
                  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a2, av.id);
                  av.id = a2;
                }
                else
                {
                  av.id = new_temporary_register(generator);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(
                      o,
                      "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                      av.id,
                      string_to_lowercase(arg->string_value),
                      as ? as->scope : 0,
                      as ? as->elaboration_level : 0);
                }
                ek = VALUE_KIND_POINTER;
              }
              else if (pm->parameter.mode & 2)
              {
                rf = true;
                av = generate_expression(generator, arg);
                int ap = new_temporary_register(generator);
                fprintf(o, "  %%t%d = alloca %s\n", ap, value_llvm_type_string(av.k));
                fprintf(
                    o, "  store %s %%t%d, ptr %%t%d\n", value_llvm_type_string(av.k), av.id, ap);
                av.id = ap;
                av.k = VALUE_KIND_POINTER;
                ek = VALUE_KIND_POINTER;
              }
              else
              {
                av = generate_expression(generator, arg);
              }
            }
            else
            {
              av = generate_expression(generator, arg);
            }
            if (not rf and ek != VALUE_KIND_INTEGER)
            {
              Value cv = value_cast(generator, av, ek);
              av = cv;
            }
            arid[i] = av.id;
            ark[i] = ek != VALUE_KIND_INTEGER ? ek : av.k;
            arp[i] = rf ? 1 : 0;
          }
          char nb[256];
          if (s->is_external)
            snprintf(nb, 256, "%.*s", (int) s->external_name.length, s->external_name.string);
          else
            encode_symbol_name(nb, 256, s, n->code_stmt.name->string_value, n->code_stmt.arguments.count, sp);
          fprintf(o, "  call void @\"%s\"(", nb);
          for (uint32_t i = 0; i < n->code_stmt.arguments.count; i++)
          {
            if (i)
              fprintf(o, ", ");
            fprintf(o, "%s %%t%d", value_llvm_type_string(ark[i]), arid[i]);
          }
          if (s->level > 0 and not s->is_external)
          {
            if (n->code_stmt.arguments.count > 0)
              fprintf(o, ", ");
            if (s->level >= generator->sm->lv)
              fprintf(o, "ptr %%__frame");
            else
              fprintf(o, "ptr %%__slnk");
          }
          fprintf(o, ")\n");
          for (uint32_t i = 0; i < n->code_stmt.arguments.count and i < 64; i++)
          {
            if (arp[i])
            {
              int lv = new_temporary_register(generator);
              fprintf(
                  o,
                  "  %%t%d = load %s, ptr %%t%d\n",
                  lv,
                  value_llvm_type_string(
                      ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]),
                  arid[i]);
              Value rv = {lv, ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]};
              Value cv = value_cast(generator, rv, token_kind_to_value_kind(n->code_stmt.arguments.data[i]->ty));
              Syntax_Node *tg = n->code_stmt.arguments.data[i];
              if (tg->k == N_ID)
              {
                Symbol *ts = tg->symbol ? tg->symbol : symbol_find(generator->sm, tg->string_value);
                if (ts)
                {
                  if (ts->level >= 0 and ts->level < generator->sm->lv)
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        ts->level,
                        string_to_lowercase(tg->string_value));
                  else
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        string_to_lowercase(tg->string_value),
                        ts->scope,
                        ts->elaboration_level);
                }
              }
            }
          }
        }
        else if (s->is_external)
        {
          // Generate all argument expressions first
          int arid[64];
          for (uint32_t i = 0; i < n->code_stmt.arguments.count and i < 64; i++)
          {
            Value av = generate_expression(generator, n->code_stmt.arguments.data[i]);
            arid[i] = av.id;
          }
          // Now emit the call
          char nb[256];
          snprintf(nb, 256, "%.*s", (int) s->external_name.length, s->external_name.string);
          fprintf(o, "  call void @\"%s\"(", nb);
          for (uint32_t i = 0; i < n->code_stmt.arguments.count and i < 64; i++)
          {
            if (i)
              fprintf(o, ", ");
            fprintf(o, "i64 %%t%d", arid[i]);
          }
          fprintf(o, ")\n");
        }
        else if (s->k == 4 or s->k == 5)
        {
          Syntax_Node *sp = symbol_spec(s);
          if (not sp and s->type_info and s->type_info->operations.count > 0)
          {
            sp = s->type_info->operations.data[0]->body.subprogram_spec;
          }
          int arid[64];
          Value_Kind ark[64];
          int arpt[64];
          for (uint32_t i = 0; i < n->code_stmt.arguments.count and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->subprogram.parameters.count ? sp->subprogram.parameters.data[i] : 0;
            arpt[i] = pm and (pm->parameter.mode & 2) ? 1 : 0;
          }
          for (uint32_t i = 0; i < n->code_stmt.arguments.count and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->subprogram.parameters.count ? sp->subprogram.parameters.data[i] : 0;
            Syntax_Node *arg = n->code_stmt.arguments.data[i];
            Value av = {0, VALUE_KIND_INTEGER};
            Value_Kind ek = VALUE_KIND_INTEGER;
            if (pm)
            {
              if (pm->symbol and pm->symbol->type_info)
                ek = token_kind_to_value_kind(pm->symbol->type_info);
              else if (pm->parameter.ty)
              {
                Type_Info *pt = resolve_subtype(generator->sm, pm->parameter.ty);
                ek = token_kind_to_value_kind(pt);
              }
            }
            if (arpt[i] and arg->k == N_ID)
            {
              ek = VALUE_KIND_POINTER;
              Symbol *as = arg->symbol ? arg->symbol : symbol_find(generator->sm, arg->string_value);
              av.id = new_temporary_register(generator);
              av.k = VALUE_KIND_POINTER;
              if (as and as->level >= 0 and as->level < generator->sm->lv)
                fprintf(
                    o,
                    "  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\n",
                    av.id,
                    as->level,
                    string_to_lowercase(arg->string_value));
              else
                fprintf(
                    o,
                    "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                    av.id,
                    string_to_lowercase(arg->string_value),
                    as ? as->scope : 0,
                    as ? as->elaboration_level : 0);
            }
            else
            {
              av = generate_expression(generator, arg);
              Value cv = value_cast(generator, av, ek);
              av = cv;
            }
            arid[i] = av.id;
            ark[i] = ek;
          }
          char nb[256];
          if (s->is_external)
            snprintf(nb, 256, "%.*s", (int) s->external_name.length, s->external_name.string);
          else
            encode_symbol_name(nb, 256, s, n->code_stmt.name->string_value, n->code_stmt.arguments.count, sp);
          if (s->k == 5 and sp and sp->subprogram.return_type)
          {
            Type_Info *rt = resolve_subtype(generator->sm, sp->subprogram.return_type);
            Value_Kind rk = token_kind_to_value_kind(rt);
            int rid = new_temporary_register(generator);
            fprintf(o, "  %%t%d = call %s @\"%s\"(", rid, value_llvm_type_string(rk), nb);
          }
          else
            fprintf(o, "  call void @\"%s\"(", nb);
          for (uint32_t i = 0; i < n->code_stmt.arguments.count; i++)
          {
            if (i)
              fprintf(o, ", ");
            fprintf(o, "%s %%t%d", value_llvm_type_string(ark[i]), arid[i]);
          }
          fprintf(o, ")\n");
        }
      }
    }
    break;
  case N_BL:
  {
    int lblbb = -1;
    if (n->block.label.string)
    {
      lblbb = get_or_create_label_basic_block(generator, n->block.label);
      emit_branch(generator, lblbb);
      emit_label(generator, lblbb);
    }
    int sj = new_temporary_register(generator);
    int prev_eh = new_temporary_register(generator);
    fprintf(o, "  %%t%d = load ptr, ptr @__eh_cur\n", prev_eh);
    fprintf(o, "  %%t%d = call ptr @__ada_setjmp()\n", sj);
    fprintf(o, "  store ptr %%t%d, ptr %%ej\n", sj);
    int sjb = new_temporary_register(generator);
    fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", sjb);
    int sv = new_temporary_register(generator);
    fprintf(o, "  %%t%d = call i32 @setjmp(ptr %%t%d)\n", sv, sjb);
    fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", sjb);
    int ze = new_temporary_register(generator);
    fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", ze, sv);
    int ln = new_label_block(generator), lh = new_label_block(generator);
    emit_conditional_branch(generator, ze, ln, lh);
    emit_label(generator, ln);
    for (uint32_t i = 0; i < n->block.declarations.count; i++)
    {
      Syntax_Node *d = n->block.declarations.data[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PKB and d->k != N_PD and d->k != N_FD)
        generate_declaration(generator, d);
    }
    for (uint32_t i = 0; i < n->block.declarations.count; i++)
    {
      Syntax_Node *d = n->block.declarations.data[i];
      if (d and d->k == N_OD and d->object_decl.in)
      {
        for (uint32_t j = 0; j < d->object_decl.identifiers.count; j++)
        {
          Syntax_Node *id = d->object_decl.identifiers.data[j];
          if (id->symbol)
          {
            Value_Kind k = d->object_decl.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, d->object_decl.ty))
                                    : VALUE_KIND_INTEGER;
            Symbol *s = id->symbol;
            Type_Info *at = d->object_decl.ty ? resolve_subtype(generator->sm, d->object_decl.ty) : 0;
            if (at and at->k == TYPE_RECORD and at->discriminants.count > 0 and d->object_decl.in and d->object_decl.in->ty)
            {
              Type_Info *it = type_canonical_concrete(d->object_decl.in->ty);
              if (it and it->k == TYPE_RECORD and it->discriminants.count > 0)
              {
                for (uint32_t di = 0; di < at->discriminants.count and di < it->discriminants.count; di++)
                {
                  Syntax_Node *td = at->discriminants.data[di];
                  Syntax_Node *id_d = it->discriminants.data[di];
                  if (td->k == N_DS and id_d->k == N_DS and td->parameter.default_value and td->parameter.default_value->k == N_INT)
                  {
                    int tdi = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = add i64 0, %lld\n", tdi, (long long) td->parameter.default_value->integer_value);
                    Value iv = generate_expression(generator, d->object_decl.in);
                    int ivd = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ivd, iv.id, di);
                    int dvl = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", dvl, ivd);
                    int cmp = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", cmp, dvl, tdi);
                    int lok = new_label_block(generator), lf = new_label_block(generator);
                    emit_conditional_branch(generator, cmp, lok, lf);
                    emit_label(generator, lok);
                    fprintf(
                        o,
                        "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n "
                        " unreachable\n");
                    emit_label(generator, lf);
                  }
                }
              }
            }
            {
              Value v = generate_expression(generator, d->object_decl.in);
              v = value_cast(generator, v, k);
              if (s and s->level >= 0 and s->level < generator->sm->lv)
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                    value_llvm_type_string(k),
                    v.id,
                    s->level,
                    string_to_lowercase(id->string_value));
              else
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                    value_llvm_type_string(k),
                    v.id,
                    string_to_lowercase(id->string_value),
                    s ? s->scope : 0,
                    s ? s->elaboration_level : 0);
            }
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->block.declarations.count; i++)
    {
      Syntax_Node *d = n->block.declarations.data[i];
      if (d and d->k == N_OD and not d->object_decl.in)
        for (uint32_t j = 0; j < d->object_decl.identifiers.count; j++)
        {
          Syntax_Node *id = d->object_decl.identifiers.data[j];
          Symbol *s = id->symbol;
          if (not s)
            continue;
          Type_Info *at = d->object_decl.ty ? resolve_subtype(generator->sm, d->object_decl.ty) : 0;
          if (not at or at->k != TYPE_RECORD or not at->discriminants.count)
            continue;
          uint32_t di;
          for (di = 0; di < at->discriminants.count; di++)
            if (at->discriminants.data[di]->k != N_DS or not at->discriminants.data[di]->parameter.default_value)
              goto nx;
          for (uint32_t ci = 0; ci < at->components.count; ci++)
          {
            Syntax_Node *cm = at->components.data[ci];
            if (cm->k != N_CM or not cm->component_decl.ty)
              continue;
            Type_Info *cty = cm->component_decl.ty->ty;
            if (not cty or cty->k != TYPE_ARRAY or not cty->index_type)
              continue;
            for (di = 0; di < at->discriminants.count; di++)
            {
              Syntax_Node *dc = at->discriminants.data[di];
              if (dc->k == N_DS and dc->parameter.default_value and dc->parameter.default_value->k == N_INT)
              {
                int64_t dv = dc->parameter.default_value->integer_value;
                if (dv < cty->index_type->low_bound or dv > cty->index_type->high_bound)
                {
                  fprintf(
                      o,
                      "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  "
                      "unreachable\n");
                  goto nx;
                }
              }
            }
          }
          for (di = 0; di < at->discriminants.count; di++)
          {
            Syntax_Node *dc = at->discriminants.data[di];
            int dv = new_temporary_register(generator);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", dv, (long long) dc->parameter.default_value->integer_value);
            int dp = new_temporary_register(generator);
            if (s->level >= 0 and s->level < generator->sm->lv)
              fprintf(
                  o,
                  "  %%t%d = getelementptr i64, ptr %%lnk.%d.%s, i64 %u\n",
                  dp,
                  s->level,
                  string_to_lowercase(id->string_value),
                  di);
            else
              fprintf(
                  o,
                  "  %%t%d = getelementptr i64, ptr %%v.%s.sc%u.%u, i64 %u\n",
                  dp,
                  string_to_lowercase(id->string_value),
                  s->scope,
                  s->elaboration_level,
                  di);
            fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", dv, dp);
          }
        nx:;
        }
    }
    for (uint32_t i = 0; i < n->block.statements.count; i++)
      generate_statement_sequence(generator, n->block.statements.data[i]);
    int ld = new_label_block(generator);
    emit_branch(generator, ld);
    emit_label(generator, lh);
    if (n->block.handlers.count > 0)
    {
      for (uint32_t i = 0; i < n->block.handlers.count; i++)
      {
        Syntax_Node *h = n->block.handlers.data[i];
        int lhm = new_label_block(generator), lhn = new_label_block(generator);
        for (uint32_t j = 0; j < h->exception_handler.exception_choices.count; j++)
        {
          Syntax_Node *e = h->exception_handler.exception_choices.data[j];
          if (e->k == N_ID and string_equal_ignore_case(e->string_value, STRING_LITERAL("others")))
          {
            emit_branch(generator, lhm);
            break;
          }
          int ec = new_temporary_register(generator);
          fprintf(o, "  %%t%d = load ptr, ptr @__ex_cur\n", ec);
          int cm = new_temporary_register(generator);
          char eb[256];
          for (uint32_t ei = 0; ei < e->string_value.length and ei < 255; ei++)
            eb[ei] = toupper(e->string_value.string[ei]);
          eb[e->string_value.length < 255 ? e->string_value.length : 255] = 0;
          fprintf(o, "  %%t%d = call i32 @strcmp(ptr %%t%d, ptr @.ex.%s)\n", cm, ec, eb);
          int eq = new_temporary_register(generator);
          fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", eq, cm);
          emit_conditional_branch(generator, eq, lhm, lhn);
          emit_label(generator, lhn);
        }
        emit_branch(generator, ld);
        emit_label(generator, lhm);
        for (uint32_t j = 0; j < h->exception_handler.statements.count; j++)
          generate_statement_sequence(generator, h->exception_handler.statements.data[j]);
        emit_branch(generator, ld);
      }
    }
    else
    {
      int nc = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp eq ptr %%t%d, null\n", nc, prev_eh);
      int ex = new_label_block(generator), lj = new_label_block(generator);
      emit_conditional_branch(generator, nc, ex, lj);
      emit_label(generator, ex);
      emit_branch(generator, ld);
      emit_label(generator, lj);
      fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", prev_eh);
      fprintf(o, "  unreachable\n");
    }
    emit_label(generator, ld);
    fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", prev_eh);
    ;
  }
  break;
  case N_DL:
  {
    Value d = generate_expression(generator, n->exit_stmt.condition);
    d = value_cast(generator, d, VALUE_KIND_INTEGER);
    fprintf(o, "  call void @__ada_delay(i64 %%t%d)\n", d.id);
  }
  break;
  case N_SA:
  {
    if (n->abort_stmt.kind == 1 or n->abort_stmt.kind == 3)
    {
      Value gd = generate_expression(generator, n->abort_stmt.guard);
      int ld = new_label_block(generator);
      fprintf(o, "  call void @__ada_delay(i64 %%t%d)\n", gd.id);
      if (n->abort_stmt.kind == 3)
        fprintf(o, "  call void @__ada_raise(ptr @.ex.TASKING_ERROR)\n  unreachable\n");
      for (uint32_t i = 0; i < n->abort_stmt.statements.count; i++)
        generate_statement_sequence(generator, n->abort_stmt.statements.data[i]);
      emit_branch(generator, ld);
      emit_label(generator, ld);
    }
    else
    {
      int ld = new_label_block(generator);
      for (uint32_t i = 0; i < n->abort_stmt.statements.count; i++)
      {
        Syntax_Node *st = n->abort_stmt.statements.data[i];
        if (st->k == N_ACC)
        {
          for (uint32_t j = 0; j < st->accept_stmt.statements.count; j++)
            generate_statement_sequence(generator, st->accept_stmt.statements.data[j]);
        }
        else if (st->k == N_DL)
        {
          Value d = generate_expression(generator, st->exit_stmt.condition);
          fprintf(o, "  call void @__ada_delay(i64 %%t%d)\n", d.id);
          for (uint32_t j = 0; j < st->exception_handler.statements.count; j++)
            generate_statement_sequence(generator, st->exception_handler.statements.data[j]);
        }
      }
      if (n->select_stmt.else_statements.count > 0)
        for (uint32_t i = 0; i < n->select_stmt.else_statements.count; i++)
          generate_statement_sequence(generator, n->select_stmt.else_statements.data[i]);
      emit_branch(generator, ld);
      emit_label(generator, ld);
    }
  }
  break;
  default:
    break;
  }
  }
}
static bool is_runtime_type(const char *name)
{
  return not strcmp(name, "__text_io_new_line") or not strcmp(name, "__text_io_put_char")
         or not strcmp(name, "__text_io_put") or not strcmp(name, "__text_io_put_line")
         or not strcmp(name, "__text_io_get_char") or not strcmp(name, "__text_io_get_line")
         or not strcmp(name, "__ada_ss_init") or not strcmp(name, "__ada_ss_mark")
         or not strcmp(name, "__ada_ss_release") or not strcmp(name, "__ada_ss_allocate")
         or not strcmp(name, "__ada_setjmp") or not strcmp(name, "__ada_raise")
         or not strcmp(name, "__ada_delay") or not strcmp(name, "__ada_powi")
         or not strcmp(name, "__ada_finalize") or not strcmp(name, "__ada_finalize_all")
         or not strcmp(name, "__ada_image_enum") or not strcmp(name, "__ada_value_int")
         or not strcmp(name, "__ada_image_int");
}
static bool has_label_block(Node_Vector *sl)
{
  for (uint32_t i = 0; i < sl->count; i++)
  {
    Syntax_Node *s = sl->data[i];
    if (not s)
      continue;
    if (s->k == N_BL and s->block.label.string)
      return 1;
    if (s->k == N_GT)
      return 1;
    if (s->k == N_BL and has_label_block(&s->block.statements))
      return 1;
    if (s->k == N_IF and (has_label_block(&s->if_stmt.then_statements) or has_label_block(&s->if_stmt.else_statements)))
      return 1;
    for (uint32_t j = 0; s->k == N_IF and j < s->if_stmt.elsif_statements.count; j++)
      if (s->if_stmt.elsif_statements.data[j] and has_label_block(&s->if_stmt.elsif_statements.data[j]->if_stmt.then_statements))
        return 1;
    if (s->k == N_CS)
    {
      for (uint32_t j = 0; j < s->case_stmt.alternatives.count; j++)
        if (s->case_stmt.alternatives.data[j] and has_label_block(&s->case_stmt.alternatives.data[j]->exception_handler.statements))
          return 1;
    }
    if (s->k == N_LP and has_label_block(&s->loop_stmt.statements))
      return 1;
  }
  return 0;
}
static void emit_labels_block_recursive(Code_Generator *generator, Syntax_Node *s, Node_Vector *lbs);
static void emit_labels_block(Code_Generator *generator, Node_Vector *sl)
{
  Node_Vector lbs = {0};
  for (uint32_t i = 0; i < sl->count; i++)
    if (sl->data[i])
      emit_labels_block_recursive(generator, sl->data[i], &lbs);
  FILE *o = generator->o;
  for (uint32_t i = 0; i < lbs.count; i++)
  {
    String_Slice *lb = (String_Slice *) lbs.data[i];
    fprintf(o, "lbl.%.*s:\n", (int) lb->length, lb->string);
  }
}
static void emit_labels_block_recursive(Code_Generator *generator, Syntax_Node *s, Node_Vector *lbs)
{
  if (not s)
    return;
  if (s->k == N_GT)
  {
    bool found = 0;
    for (uint32_t i = 0; i < lbs->count; i++)
    {
      String_Slice *lb = (String_Slice *) lbs->data[i];
      if (string_equal_ignore_case(*lb, s->goto_stmt.label))
      {
        found = 1;
        break;
      }
    }
    if (not found)
    {
      String_Slice *lb = malloc(sizeof(String_Slice));
      *lb = s->goto_stmt.label;
      nv(lbs, (Syntax_Node *) lb);
    }
  }
  if (s->k == N_IF)
  {
    for (uint32_t i = 0; i < s->if_stmt.then_statements.count; i++)
      emit_labels_block_recursive(generator, s->if_stmt.then_statements.data[i], lbs);
    for (uint32_t i = 0; i < s->if_stmt.else_statements.count; i++)
      emit_labels_block_recursive(generator, s->if_stmt.else_statements.data[i], lbs);
    for (uint32_t i = 0; i < s->if_stmt.elsif_statements.count; i++)
      if (s->if_stmt.elsif_statements.data[i])
        for (uint32_t j = 0; j < s->if_stmt.elsif_statements.data[i]->if_stmt.then_statements.count; j++)
          emit_labels_block_recursive(generator, s->if_stmt.elsif_statements.data[i]->if_stmt.then_statements.data[j], lbs);
  }
  if (s->k == N_BL)
  {
    if (s->block.label.string)
    {
      bool found = 0;
      for (uint32_t i = 0; i < lbs->count; i++)
      {
        String_Slice *lb = (String_Slice *) lbs->data[i];
        if (string_equal_ignore_case(*lb, s->block.label))
        {
          found = 1;
          break;
        }
      }
      if (not found)
      {
        String_Slice *lb = malloc(sizeof(String_Slice));
        *lb = s->block.label;
        nv(lbs, (Syntax_Node *) lb);
      }
    }
    for (uint32_t i = 0; i < s->block.statements.count; i++)
      emit_labels_block_recursive(generator, s->block.statements.data[i], lbs);
  }
  if (s->k == N_LP)
  {
    if (s->loop_stmt.label.string)
    {
      bool found = 0;
      for (uint32_t i = 0; i < lbs->count; i++)
      {
        String_Slice *lb = (String_Slice *) lbs->data[i];
        if (string_equal_ignore_case(*lb, s->loop_stmt.label))
        {
          found = 1;
          break;
        }
      }
      if (not found)
      {
        String_Slice *lb = malloc(sizeof(String_Slice));
        *lb = s->loop_stmt.label;
        nv(lbs, (Syntax_Node *) lb);
      }
    }
    for (uint32_t i = 0; i < s->loop_stmt.statements.count; i++)
      emit_labels_block_recursive(generator, s->loop_stmt.statements.data[i], lbs);
  }
  if (s->k == N_CS)
    for (uint32_t i = 0; i < s->case_stmt.alternatives.count; i++)
      if (s->case_stmt.alternatives.data[i])
        for (uint32_t j = 0; j < s->case_stmt.alternatives.data[i]->exception_handler.statements.count; j++)
          emit_labels_block_recursive(generator, s->case_stmt.alternatives.data[i]->exception_handler.statements.data[j], lbs);
}
static void generate_declaration(Code_Generator *generator, Syntax_Node *n);
static void has_basic_label(Code_Generator *generator, Node_Vector *sl)
{
  for (uint32_t i = 0; i < sl->count; i++)
  {
    Syntax_Node *s = sl->data[i];
    if (not s)
      continue;
    if (s->k == N_BL)
    {
      for (uint32_t j = 0; j < s->block.declarations.count; j++)
      {
        Syntax_Node *d = s->block.declarations.data[j];
        if (d and (d->k == N_PB or d->k == N_FB))
          generate_declaration(generator, d);
      }
      has_basic_label(generator, &s->block.statements);
    }
    else if (s->k == N_IF)
    {
      has_basic_label(generator, &s->if_stmt.then_statements);
      has_basic_label(generator, &s->if_stmt.else_statements);
      for (uint32_t j = 0; j < s->if_stmt.elsif_statements.count; j++)
        if (s->if_stmt.elsif_statements.data[j])
          has_basic_label(generator, &s->if_stmt.elsif_statements.data[j]->if_stmt.then_statements);
    }
    else if (s->k == N_CS)
    {
      for (uint32_t j = 0; j < s->case_stmt.alternatives.count; j++)
        if (s->case_stmt.alternatives.data[j])
          has_basic_label(generator, &s->case_stmt.alternatives.data[j]->exception_handler.statements);
    }
    else if (s->k == N_LP)
      has_basic_label(generator, &s->loop_stmt.statements);
  }
}
static void generate_declaration(Code_Generator *generator, Syntax_Node *n)
{
  FILE *o = generator->o;
  if (not n)
    return;
  switch (n->k)
  {
  case N_OD:
  {
    for (uint32_t j = 0; j < n->object_decl.identifiers.count; j++)
    {
      Syntax_Node *id = n->object_decl.identifiers.data[j];
      Symbol *s = id->symbol;
      if (s)
      {
        if (s->k == 0 or s->k == 2)
        {
          Value_Kind k =
              s->type_info ? token_kind_to_value_kind(s->type_info)
                    : (n->object_decl.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, n->object_decl.ty))
                                : VALUE_KIND_INTEGER);
          Type_Info *at = s->type_info ? type_canonical_concrete(s->type_info)
                                : (n->object_decl.ty ? resolve_subtype(generator->sm, n->object_decl.ty) : 0);
          if (s->k == 0 or s->k == 2)
          {
            Type_Info *bt = at;
            while (bt and bt->k == TYPE_ARRAY and bt->element_type)
              bt = type_canonical_concrete(bt->element_type);
            int asz = -1;
            if (n->object_decl.in and n->object_decl.in->k == N_AG and at and at->k == TYPE_ARRAY)
              asz = (int) n->object_decl.in->aggregate.items.count;

            // Check for runtime-sized array FIRST (before checking resolved type bounds)
            if (at and at->k == TYPE_ARRAY and n->object_decl.ty and n->object_decl.ty->k == N_ST)
            {
              // Runtime-sized array: extract bounds from subtype indication
              Syntax_Node *ty_node = n->object_decl.ty;
              Syntax_Node *cn = ty_node->subtype_decl.constraint ? ty_node->subtype_decl.constraint : ty_node->subtype_decl.range_constraint;
              Syntax_Node *lo = 0, *hi = 0;
              if (cn and cn->k == 27 and cn->constraint.constraints.count > 0 and cn->constraint.constraints.data[0] and cn->constraint.constraints.data[0]->k == 26)
              {
                Syntax_Node *rn = cn->constraint.constraints.data[0];
                lo = rn->range.low_bound;
                hi = rn->range.high_bound;
              }
              else if (cn and cn->k == 27 and cn->constraint.range_spec)
              {
                lo = cn->constraint.range_spec->range.low_bound;
                hi = cn->constraint.range_spec->range.high_bound;
              }
              else if (cn and cn->k == N_RN)
              {
                lo = cn->range.low_bound;
                hi = cn->range.high_bound;
              }
              if (lo and hi)
              {
                // Generate code to evaluate bounds at runtime
                Value lo_val = generate_expression(generator, lo);
                Value hi_val = generate_expression(generator, hi);
                int count_reg = new_temporary_register(generator);
                int size_reg = new_temporary_register(generator);
                int elem_size = bt->k == TYPE_INTEGER ? 8 : 4;
                fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", count_reg, hi_val.id, lo_val.id);
                fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", count_reg + 1, count_reg);
                int alloc_size = new_temporary_register(generator);
                fprintf(o, "  %%t%d = mul i64 %%t%d, %d\n", alloc_size, count_reg + 1, elem_size);

                // Store the size for later use (e.g., in return statements)
                fprintf(o, "  %%v.%s.sc%u.%u__size = alloca i64\n",
                    string_to_lowercase(id->string_value),
                    s->scope,
                    s->elaboration_level);
                fprintf(o, "  store i64 %%t%d, ptr %%v.%s.sc%u.%u__size\n",
                    alloc_size,
                    string_to_lowercase(id->string_value),
                    s->scope,
                    s->elaboration_level);

                fprintf(o, "  %%v.%s.sc%u.%u = alloca i8, i64 %%t%d\n",
                    string_to_lowercase(id->string_value),
                    s->scope,
                    s->elaboration_level,
                    alloc_size);
              }
              else
              {
                // N_ST node but no valid constraint - treat as unconstrained
                if (at->low_bound == 0 and at->high_bound == -1 and asz < 0)
                {
                  fprintf(
                      o,
                      "  %%v.%s.sc%u.%u = alloca {ptr,ptr}\n",
                      string_to_lowercase(id->string_value),
                      s->scope,
                      s->elaboration_level);
                }
                else
                {
                  fprintf(
                      o,
                      "  %%v.%s.sc%u.%u = alloca %s\n",
                      string_to_lowercase(id->string_value),
                      s->scope,
                      s->elaboration_level,
                      value_llvm_type_string(k));
                }
              }
            }
            else if (at and at->k == TYPE_ARRAY and at->low_bound == 0 and at->high_bound == -1 and asz < 0)
            {
              // Truly unconstrained array (no constraint)
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca {ptr,ptr}\n",
                  string_to_lowercase(id->string_value),
                  s->scope,
                  s->elaboration_level);
            }
            else if (at and at->k == TYPE_ARRAY and asz > 0)
            {
              // Array with aggregate initializer
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca [%d x %s]\n",
                  string_to_lowercase(id->string_value),
                  s->scope,
                  s->elaboration_level,
                  asz,
                  ada_to_c_type_string(bt));
            }
            else if (at and at->k == TYPE_ARRAY and at->low_bound != 0 and at->high_bound > 0 and at->high_bound >= at->low_bound)
            {
              // Compile-time constrained array
              asz = (int) (at->high_bound - at->low_bound + 1);
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca [%d x %s]\n",
                  string_to_lowercase(id->string_value),
                  s->scope,
                  s->elaboration_level,
                  asz,
                  ada_to_c_type_string(bt));
            }
            else
            {
              // Non-array or other types
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca %s\n",
                  string_to_lowercase(id->string_value),
                  s->scope,
                  s->elaboration_level,
                  value_llvm_type_string(k));
            }
          }
        }
      }
    }
    break;
  }
  case N_PD:
  {
    Syntax_Node *sp = n->body.subprogram_spec;
    char nb[256];
    if (n->symbol and n->symbol->is_external)
      snprintf(nb, 256, "%.*s", (int) n->symbol->external_name.length, n->symbol->external_name.string);
    else
    {
      encode_symbol_name(nb, 256, n->symbol, sp->subprogram.name, sp->subprogram.parameters.count, sp);
    }
    if (not add_declaration(generator, nb))
      break;
    if (is_runtime_type(nb))
      break;
    bool has_body = false;
    if (n->symbol)
      for (uint32_t i = 0; i < n->symbol->overloads.count; i++)
        if (n->symbol->overloads.data[i]->k == N_PB)
        {
          has_body = true;
          break;
        }
    if (has_body)
      break;
    fprintf(o, "declare void @\"%s\"(", nb);
    for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
    {
      if (i)
        fprintf(o, ",");
      Syntax_Node *p = sp->subprogram.parameters.data[i];
      Type_Info *pt = p->parameter.ty ? resolve_subtype(generator->sm, p->parameter.ty) : 0;
      Value_Kind k = VALUE_KIND_INTEGER;
      bool is_unconstrained = false;
      if (pt)
      {
        Type_Info *ptc = type_canonical_concrete(pt);
        is_unconstrained = ptc and ptc->k == TYPE_ARRAY and ptc->low_bound == 0 and ptc->high_bound == -1;
        if (n->symbol and n->symbol->is_external and ptc and ptc->k == TYPE_ARRAY and not(p->parameter.mode & 2))
          k = VALUE_KIND_INTEGER;
        else
          k = token_kind_to_value_kind(pt);
      }
      if (p->parameter.mode & 2 or is_unconstrained)
        fprintf(o, "ptr");
      else
        fprintf(o, "%s", value_llvm_type_string(k));
    }
    fprintf(o, ")\n");
  }
  break;
  case N_FD:
  {
    Syntax_Node *sp = n->body.subprogram_spec;
    char nb[256];
    if (n->symbol and n->symbol->is_external)
      snprintf(nb, 256, "%.*s", (int) n->symbol->external_name.length, n->symbol->external_name.string);
    else
    {
      encode_symbol_name(nb, 256, n->symbol, sp->subprogram.name, sp->subprogram.parameters.count, sp);
    }
    if (not add_declaration(generator, nb))
      break;
    if (is_runtime_type(nb))
      break;
    bool has_body = false;
    if (n->symbol)
      for (uint32_t i = 0; i < n->symbol->overloads.count; i++)
        if (n->symbol->overloads.data[i]->k == N_FB)
        {
          has_body = true;
          break;
        }
    if (has_body)
      break;
    Type_Info *rt = sp->subprogram.return_type ? resolve_subtype(generator->sm, sp->subprogram.return_type) : 0;
    Type_Info *rtc = rt ? type_canonical_concrete(rt) : 0;
    bool return_unconstrained = rtc and rtc->k == TYPE_ARRAY and rtc->low_bound == 0 and rtc->high_bound == -1;
    Value_Kind rk = return_unconstrained ? VALUE_KIND_POINTER : (rt ? token_kind_to_value_kind(rt) : VALUE_KIND_INTEGER);
    fprintf(o, "declare %s @\"%s\"(", value_llvm_type_string(rk), nb);
    for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
    {
      if (i)
        fprintf(o, ",");
      Syntax_Node *p = sp->subprogram.parameters.data[i];
      Type_Info *pt = p->parameter.ty ? resolve_subtype(generator->sm, p->parameter.ty) : 0;
      Value_Kind k = VALUE_KIND_INTEGER;
      bool is_unconstrained = false;
      if (pt)
      {
        Type_Info *ptc = type_canonical_concrete(pt);
        is_unconstrained = ptc and ptc->k == TYPE_ARRAY and ptc->low_bound == 0 and ptc->high_bound == -1;
        if (n->symbol and n->symbol->is_external and ptc and ptc->k == TYPE_ARRAY and not(p->parameter.mode & 2))
          k = VALUE_KIND_INTEGER;
        else
          k = token_kind_to_value_kind(pt);
      }
      if (p->parameter.mode & 2 or is_unconstrained)
        fprintf(o, "ptr");
      else
        fprintf(o, "%s", value_llvm_type_string(k));
    }
    fprintf(o, ")\n");
  }
  break;
  case N_BL:
    for (uint32_t i = 0; i < n->block.declarations.count; i++)
    {
      Syntax_Node *d = n->block.declarations.data[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PD and d->k != N_FD)
        generate_declaration(generator, d);
    }
    for (uint32_t i = 0; i < n->block.declarations.count; i++)
    {
      Syntax_Node *d = n->block.declarations.data[i];
      if (d and (d->k == N_PB or d->k == N_FB))
        generate_declaration(generator, d);
    }
    break;
  case N_PB:
  {
    generator->current_function = n;
    Syntax_Node *sp = n->body.subprogram_spec;
    Generic_Template *gt = generic_find(generator->sm, sp->subprogram.name);
    if (gt)
    {
      generator->current_function = 0;
      break;
    }
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and d->k == N_PKB)
        generate_declaration(generator, d);
    }
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and (d->k == N_PB or d->k == N_FB))
        generate_declaration(generator, d);
    }
    has_basic_label(generator, &n->body.statements);
    char nb[256];
    if (n->symbol and n->symbol->mangled_name.string)
    {
      snprintf(nb, 256, "%.*s", (int) n->symbol->mangled_name.length, n->symbol->mangled_name.string);
    }
    else
    {
      encode_symbol_name(nb, 256, n->symbol, sp->subprogram.name, sp->subprogram.parameters.count, sp);
      if (n->symbol)
      {
        n->symbol->mangled_name.string = arena_allocate(strlen(nb) + 1);
        memcpy((char *) n->symbol->mangled_name.string, nb, strlen(nb) + 1);
        n->symbol->mangled_name.length = strlen(nb);
      }
    }
    fprintf(o, "define linkonce_odr void @\"%s\"(", nb);
    int np = sp->subprogram.parameters.count;
    if (n->symbol and n->symbol->level > 0)
      np++;
    for (int i = 0; i < np; i++)
    {
      if (i)
        fprintf(o, ", ");
      if (i < (int) sp->subprogram.parameters.count)
      {
        Syntax_Node *p = sp->subprogram.parameters.data[i];
        Value_Kind k = p->parameter.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, p->parameter.ty))
                                : VALUE_KIND_INTEGER;
        if (p->parameter.mode & 2)
          fprintf(o, "ptr %%p.%s", string_to_lowercase(p->parameter.name));
        else
          fprintf(o, "%s %%p.%s", value_llvm_type_string(k), string_to_lowercase(p->parameter.name));
      }
      else
        fprintf(o, "ptr %%__slnk");
    }
    fprintf(o, ")%s{\n", n->symbol and n->symbol->is_inline ? " alwaysinline " : " ");
    fprintf(o, "  %%ej = alloca ptr\n");
    int sv = generator->sm->lv;
    generator->sm->lv = n->symbol ? n->symbol->level + 1 : 0;
    bool needs_frame = (n->symbol and n->symbol->level > 0) or has_nested_function(&n->body.declarations, &n->body.statements);
    if (needs_frame)
    {
      generate_block_frame(generator);
      int mx = 0;
      for (int h = 0; h < 4096; h++)
        for (Symbol *s = generator->sm->sy[h]; s; s = s->next)
          if (s->k == 0 and s->elaboration_level >= 0 and s->elaboration_level > mx)
            mx = s->elaboration_level;
      if (mx == 0)
        fprintf(o, "  %%__frame = bitcast ptr %%__slnk to ptr\n");
    }
    if (n->symbol and n->symbol->level > 0)
    {
      int fp0 = new_temporary_register(generator);
      fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__frame, i64 0\n", fp0);
      fprintf(o, "  store ptr %%__slnk, ptr %%t%d\n", fp0);
    }
    for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
    {
      Syntax_Node *p = sp->subprogram.parameters.data[i];
      Value_Kind k = p->parameter.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, p->parameter.ty))
                              : VALUE_KIND_INTEGER;
      Symbol *ps = p->symbol;
      if (ps and ps->level >= 0 and ps->level < generator->sm->lv)
        fprintf(
            o,
            "  %%lnk.%d.%s = alloca %s\n",
            ps->level,
            string_to_lowercase(p->parameter.name),
            value_llvm_type_string(k));
      else
        fprintf(
            o,
            "  %%v.%s.sc%u.%u = alloca %s\n",
            string_to_lowercase(p->parameter.name),
            ps ? ps->scope : 0,
            ps ? ps->elaboration_level : 0,
            value_llvm_type_string(k));
      if (p->parameter.mode & 2)
      {
        int lv = new_temporary_register(generator);
        fprintf(
            o,
            "  %%t%d = load %s, ptr %%p.%s\n",
            lv,
            value_llvm_type_string(k),
            string_to_lowercase(p->parameter.name));
        if (ps and ps->level >= 0 and ps->level < generator->sm->lv)
          fprintf(
              o,
              "  store %s %%t%d, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              lv,
              ps->level,
              string_to_lowercase(p->parameter.name));
        else
          fprintf(
              o,
              "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              lv,
              string_to_lowercase(p->parameter.name),
              ps ? ps->scope : 0,
              ps ? ps->elaboration_level : 0);
      }
      else
      {
        if (ps and ps->level >= 0 and ps->level < generator->sm->lv)
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->parameter.name),
              ps->level,
              string_to_lowercase(p->parameter.name));
        else
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->parameter.name),
              string_to_lowercase(p->parameter.name),
              ps ? ps->scope : 0,
              ps ? ps->elaboration_level : 0);
      }
    }
    // Removed: Code that created local copies of ALL parent variables in nested procedures.
    // This was incorrect - parent variables are accessed directly via frame pointers.
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PKB and d->k != N_PD and d->k != N_FD)
        generate_declaration(generator, d);
    }
    // Store local variable addresses in frame so nested procedures can access them
    if (needs_frame and n->symbol)
    {
      for (int h = 0; h < 4096; h++)
      {
        for (Symbol *s = generator->sm->sy[h]; s; s = s->next)
        {
          // Match variables declared in this procedure's immediate child scope
          // Use parent pointer to ensure we only store variables belonging to this procedure
          if (s->k == 0 and s->elaboration_level >= 0 and s->parent == n->symbol
              and s->scope == (uint32_t) (n->symbol->scope + 1)
              and s->level == generator->sm->lv)
          {
            bool is_pm = false;
            for (uint32_t pi = 0; pi < sp->subprogram.parameters.count; pi++)
            {
              if (sp->subprogram.parameters.data[pi]->symbol == s)
              {
                is_pm = true;
                break;
              }
            }
            if (not is_pm)
            {
              int fp = new_temporary_register(generator);
              int mx = generator->sm->eo;
              fprintf(
                  o,
                  "  %%t%d = getelementptr [%d x ptr], ptr %%__frame, i64 0, i64 "
                  "%u\n",
                  fp,
                  mx,
                  s->elaboration_level);
              fprintf(
                  o,
                  "  store ptr %%v.%s.sc%u.%u, ptr %%t%d\n",
                  string_to_lowercase(s->name),
                  s->scope,
                  s->elaboration_level,
                  fp);
            }
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and d->k == N_OD and d->object_decl.in)
      {
        for (uint32_t j = 0; j < d->object_decl.identifiers.count; j++)
        {
          Syntax_Node *id = d->object_decl.identifiers.data[j];
          if (id->symbol)
          {
            Value_Kind k = d->object_decl.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, d->object_decl.ty))
                                    : VALUE_KIND_INTEGER;
            Symbol *s = id->symbol;
            Type_Info *at = d->object_decl.ty ? resolve_subtype(generator->sm, d->object_decl.ty) : 0;
            if (at and at->k == TYPE_RECORD and at->discriminants.count > 0 and d->object_decl.in and d->object_decl.in->ty)
            {
              Type_Info *it = type_canonical_concrete(d->object_decl.in->ty);
              if (it and it->k == TYPE_RECORD and it->discriminants.count > 0)
              {
                for (uint32_t di = 0; di < at->discriminants.count and di < it->discriminants.count; di++)
                {
                  Syntax_Node *td = at->discriminants.data[di];
                  Syntax_Node *id_d = it->discriminants.data[di];
                  if (td->k == N_DS and id_d->k == N_DS and td->parameter.default_value and td->parameter.default_value->k == N_INT)
                  {
                    int tdi = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = add i64 0, %lld\n", tdi, (long long) td->parameter.default_value->integer_value);
                    Value iv = generate_expression(generator, d->object_decl.in);
                    int ivd = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ivd, iv.id, di);
                    int dvl = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", dvl, ivd);
                    int cmp = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", cmp, dvl, tdi);
                    int lok = new_label_block(generator), lf = new_label_block(generator);
                    emit_conditional_branch(generator, cmp, lok, lf);
                    emit_label(generator, lok);
                    fprintf(
                        o,
                        "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n "
                        " unreachable\n");
                    emit_label(generator, lf);
                  }
                }
              }
            }
            {
              Value v = generate_expression(generator, d->object_decl.in);
              v = value_cast(generator, v, k);
              if (s and s->level >= 0 and s->level < generator->sm->lv)
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                    value_llvm_type_string(k),
                    v.id,
                    s->level,
                    string_to_lowercase(id->string_value));
              else
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                    value_llvm_type_string(k),
                    v.id,
                    string_to_lowercase(id->string_value),
                    s ? s->scope : 0,
                    s ? s->elaboration_level : 0);
            }
          }
        }
      }
    }
    if (has_label_block(&n->body.statements))
    {
      int sj = new_temporary_register(generator);
      int peh = new_temporary_register(generator);
      fprintf(o, "  %%t%d = load ptr, ptr @__eh_cur\n", peh);
      fprintf(o, "  %%t%d = call ptr @__ada_setjmp()\n", sj);
      fprintf(o, "  store ptr %%t%d, ptr %%ej\n", sj);
      int sjb = new_temporary_register(generator);
      fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", sjb);
      int sv = new_temporary_register(generator);
      fprintf(o, "  %%t%d = call i32 @setjmp(ptr %%t%d)\n", sv, sjb);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", sjb);
      int ze = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", ze, sv);
      int ln = new_label_block(generator), lh = new_label_block(generator);
      emit_conditional_branch(generator, ze, ln, lh);
      emit_label(generator, ln);
      for (uint32_t i = 0; i < n->body.statements.count; i++)
        generate_statement_sequence(generator, n->body.statements.data[i]);
      int ld = new_label_block(generator);
      emit_branch(generator, ld);
      emit_label(generator, lh);
      int nc = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp eq ptr %%t%d, null\n", nc, peh);
      int ex = new_label_block(generator), lj = new_label_block(generator);
      emit_conditional_branch(generator, nc, ex, lj);
      emit_label(generator, ex);
      emit_branch(generator, ld);
      emit_label(generator, lj);
      fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", peh);
      fprintf(o, "  unreachable\n");
      emit_label(generator, ld);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", peh);
    }
    else
    {
      for (uint32_t i = 0; i < n->body.statements.count; i++)
        generate_statement_sequence(generator, n->body.statements.data[i]);
    }
    generator->sm->lv = sv;
    fprintf(o, "  ret void\n}\n");
    generator->current_function = 0;
  }
  break;
  case N_FB:
  {
    generator->current_function = n;
    Syntax_Node *sp = n->body.subprogram_spec;
    Generic_Template *gt = generic_find(generator->sm, sp->subprogram.name);
    if (gt)
    {
      generator->current_function = 0;
      break;
    }
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and d->k == N_PKB)
        generate_declaration(generator, d);
    }
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and (d->k == N_PB or d->k == N_FB))
        generate_declaration(generator, d);
    }
    has_basic_label(generator, &n->body.statements);
    Value_Kind rk = sp->subprogram.return_type ? token_kind_to_value_kind(resolve_subtype(generator->sm, sp->subprogram.return_type))
                              : VALUE_KIND_INTEGER;
    char nb[256];
    if (n->symbol and n->symbol->mangled_name.string)
    {
      snprintf(nb, 256, "%.*s", (int) n->symbol->mangled_name.length, n->symbol->mangled_name.string);
    }
    else
    {
      encode_symbol_name(nb, 256, n->symbol, sp->subprogram.name, sp->subprogram.parameters.count, sp);
      if (n->symbol)
      {
        n->symbol->mangled_name.string = arena_allocate(strlen(nb) + 1);
        memcpy((char *) n->symbol->mangled_name.string, nb, strlen(nb) + 1);
        n->symbol->mangled_name.length = strlen(nb);
      }
    }
    fprintf(o, "define linkonce_odr %s @\"%s\"(", value_llvm_type_string(rk), nb);
    int np = sp->subprogram.parameters.count;
    if (n->symbol and n->symbol->level > 0)
      np++;
    for (int i = 0; i < np; i++)
    {
      if (i)
        fprintf(o, ", ");
      if (i < (int) sp->subprogram.parameters.count)
      {
        Syntax_Node *p = sp->subprogram.parameters.data[i];
        Value_Kind k = p->parameter.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, p->parameter.ty))
                                : VALUE_KIND_INTEGER;
        if (p->parameter.mode & 2)
          fprintf(o, "ptr %%p.%s", string_to_lowercase(p->parameter.name));
        else
          fprintf(o, "%s %%p.%s", value_llvm_type_string(k), string_to_lowercase(p->parameter.name));
      }
      else
        fprintf(o, "ptr %%__slnk");
    }
    fprintf(o, ")%s{\n", n->symbol and n->symbol->is_inline ? " alwaysinline " : " ");
    fprintf(o, "  %%ej = alloca ptr\n");
    int sv = generator->sm->lv;
    generator->sm->lv = n->symbol ? n->symbol->level + 1 : 0;
    bool needs_frame = (n->symbol and n->symbol->level > 0) or has_nested_function(&n->body.declarations, &n->body.statements);
    if (needs_frame)
    {
      generate_block_frame(generator);
      int mx = 0;
      for (int h = 0; h < 4096; h++)
        for (Symbol *s = generator->sm->sy[h]; s; s = s->next)
          if (s->k == 0 and s->elaboration_level >= 0 and s->elaboration_level > mx)
            mx = s->elaboration_level;
      if (mx == 0)
        fprintf(o, "  %%__frame = bitcast ptr %%__slnk to ptr\n");
    }
    if (n->symbol and n->symbol->level > 0)
    {
      int fp0 = new_temporary_register(generator);
      fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__frame, i64 0\n", fp0);
      fprintf(o, "  store ptr %%__slnk, ptr %%t%d\n", fp0);
    }
    for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
    {
      Syntax_Node *p = sp->subprogram.parameters.data[i];
      Value_Kind k = p->parameter.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, p->parameter.ty))
                              : VALUE_KIND_INTEGER;
      Symbol *ps = p->symbol;
      if (ps and ps->level >= 0 and ps->level < generator->sm->lv)
        fprintf(
            o,
            "  %%lnk.%d.%s = alloca %s\n",
            ps->level,
            string_to_lowercase(p->parameter.name),
            value_llvm_type_string(k));
      else
        fprintf(
            o,
            "  %%v.%s.sc%u.%u = alloca %s\n",
            string_to_lowercase(p->parameter.name),
            ps ? ps->scope : 0,
            ps ? ps->elaboration_level : 0,
            value_llvm_type_string(k));
      if (p->parameter.mode & 2)
      {
        int lv = new_temporary_register(generator);
        fprintf(
            o,
            "  %%t%d = load %s, ptr %%p.%s\n",
            lv,
            value_llvm_type_string(k),
            string_to_lowercase(p->parameter.name));
        if (ps and ps->level >= 0 and ps->level < generator->sm->lv)
          fprintf(
              o,
              "  store %s %%t%d, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              lv,
              ps->level,
              string_to_lowercase(p->parameter.name));
        else
          fprintf(
              o,
              "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              lv,
              string_to_lowercase(p->parameter.name),
              ps ? ps->scope : 0,
              ps ? ps->elaboration_level : 0);
      }
      else
      {
        if (ps and ps->level >= 0 and ps->level < generator->sm->lv)
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->parameter.name),
              ps->level,
              string_to_lowercase(p->parameter.name));
        else
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->parameter.name),
              string_to_lowercase(p->parameter.name),
              ps ? ps->scope : 0,
              ps ? ps->elaboration_level : 0);
      }
    }
    // Removed: Same buggy code as in N_PB - parent variables accessed via frame pointers.
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PKB and d->k != N_PD and d->k != N_FD)
        generate_declaration(generator, d);
    }
    // Store local variable addresses in frame so nested procedures can access them (N_FB version)
    // Only do this for level 0 procedures to avoid storing variables from sibling procedures
    if (needs_frame and n->symbol and n->symbol->level == 0)
    {
      for (int h = 0; h < 4096; h++)
      {
        for (Symbol *s = generator->sm->sy[h]; s; s = s->next)
        {
          // Match variables declared in this procedure's immediate child scope
          // Note: parent pointer may not be set correctly, so we rely on scope/level matching
          if (s->k == 0 and s->elaboration_level >= 0 and n->symbol and n->symbol->scope >= 0
              and s->scope == (uint32_t) (n->symbol->scope + 1)
              and s->level == generator->sm->lv)
          {
            bool is_pm = false;
            for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
            {
              if (sp->subprogram.parameters.data[i]->symbol == s)
              {
                is_pm = true;
                break;
              }
            }
            if (not is_pm)
            {
              int fp = new_temporary_register(generator);
              int mx = generator->sm->eo;
              fprintf(
                  o,
                  "  %%t%d = getelementptr [%d x ptr], ptr %%__frame, i64 0, i64 %u\n",
                  fp,
                  mx,
                  s->elaboration_level);
              fprintf(
                  o,
                  "  store ptr %%v.%s.sc%u.%u, ptr %%t%d\n",
                  string_to_lowercase(s->name),
                  s->scope,
                  s->elaboration_level,
                  fp);
            }
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->body.declarations.count; i++)
    {
      Syntax_Node *d = n->body.declarations.data[i];
      if (d and d->k == N_OD and d->object_decl.in)
      {
        for (uint32_t j = 0; j < d->object_decl.identifiers.count; j++)
        {
          Syntax_Node *id = d->object_decl.identifiers.data[j];
          if (id->symbol)
          {
            Value_Kind k = d->object_decl.ty ? token_kind_to_value_kind(resolve_subtype(generator->sm, d->object_decl.ty))
                                    : VALUE_KIND_INTEGER;
            Symbol *s = id->symbol;
            Type_Info *at = d->object_decl.ty ? resolve_subtype(generator->sm, d->object_decl.ty) : 0;
            if (at and at->k == TYPE_RECORD and at->discriminants.count > 0 and d->object_decl.in and d->object_decl.in->ty)
            {
              Type_Info *it = type_canonical_concrete(d->object_decl.in->ty);
              if (it and it->k == TYPE_RECORD and it->discriminants.count > 0)
              {
                for (uint32_t di = 0; di < at->discriminants.count and di < it->discriminants.count; di++)
                {
                  Syntax_Node *td = at->discriminants.data[di];
                  Syntax_Node *id_d = it->discriminants.data[di];
                  if (td->k == N_DS and id_d->k == N_DS and td->parameter.default_value and td->parameter.default_value->k == N_INT)
                  {
                    int tdi = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = add i64 0, %lld\n", tdi, (long long) td->parameter.default_value->integer_value);
                    Value iv = generate_expression(generator, d->object_decl.in);
                    int ivd = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ivd, iv.id, di);
                    int dvl = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", dvl, ivd);
                    int cmp = new_temporary_register(generator);
                    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", cmp, dvl, tdi);
                    int lok = new_label_block(generator), lf = new_label_block(generator);
                    emit_conditional_branch(generator, cmp, lok, lf);
                    emit_label(generator, lok);
                    fprintf(
                        o,
                        "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n "
                        " unreachable\n");
                    emit_label(generator, lf);
                  }
                }
              }
            }
            {
              Value v = generate_expression(generator, d->object_decl.in);
              v = value_cast(generator, v, k);
              if (s and s->level >= 0 and s->level < generator->sm->lv)
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                    value_llvm_type_string(k),
                    v.id,
                    s->level,
                    string_to_lowercase(id->string_value));
              else
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                    value_llvm_type_string(k),
                    v.id,
                    string_to_lowercase(id->string_value),
                    s ? s->scope : 0,
                    s ? s->elaboration_level : 0);
            }
          }
        }
      }
    }
    has_basic_label(generator, &n->body.statements);
    if (has_label_block(&n->body.statements))
    {
      int sj = new_temporary_register(generator);
      int peh = new_temporary_register(generator);
      fprintf(o, "  %%t%d = load ptr, ptr @__eh_cur\n", peh);
      fprintf(o, "  %%t%d = call ptr @__ada_setjmp()\n", sj);
      fprintf(o, "  store ptr %%t%d, ptr %%ej\n", sj);
      int sjb = new_temporary_register(generator);
      fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", sjb);
      int sv = new_temporary_register(generator);
      fprintf(o, "  %%t%d = call i32 @setjmp(ptr %%t%d)\n", sv, sjb);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", sjb);
      int ze = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", ze, sv);
      int ln = new_label_block(generator), lh = new_label_block(generator);
      emit_conditional_branch(generator, ze, ln, lh);
      emit_label(generator, ln);
      for (uint32_t i = 0; i < n->body.statements.count; i++)
        generate_statement_sequence(generator, n->body.statements.data[i]);
      int ld = new_label_block(generator);
      emit_branch(generator, ld);
      emit_label(generator, lh);
      int nc = new_temporary_register(generator);
      fprintf(o, "  %%t%d = icmp eq ptr %%t%d, null\n", nc, peh);
      int ex = new_label_block(generator), lj = new_label_block(generator);
      emit_conditional_branch(generator, nc, ex, lj);
      emit_label(generator, ex);
      emit_branch(generator, ld);
      emit_label(generator, lj);
      fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", peh);
      fprintf(o, "  unreachable\n");
      emit_label(generator, ld);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", peh);
    }
    else
    {
      for (uint32_t i = 0; i < n->body.statements.count; i++)
        generate_statement_sequence(generator, n->body.statements.data[i]);
    }
    generator->sm->lv = sv;
    if (rk == VALUE_KIND_POINTER)
    {
      fprintf(o, "  ret ptr null\n}\n");
    }
    else
    {
      fprintf(o, "  ret %s 0\n}\n", value_llvm_type_string(rk));
    }
    generator->current_function = 0;
  }
  break;
  case N_PKB:
    for (uint32_t i = 0; i < n->package_body.declarations.count; i++)
      if (n->package_body.declarations.data[i] and (n->package_body.declarations.data[i]->k == N_PB or n->package_body.declarations.data[i]->k == N_FB))
        generate_declaration(generator, n->package_body.declarations.data[i]);
    break;
  default:
    break;
  }
}
static void generate_expression_llvm(Code_Generator *generator, Syntax_Node *n)
{
  if (n and n->k == N_PKB and n->package_body.statements.count > 0)
  {
    Symbol *ps = symbol_find(generator->sm, n->package_body.name);
    if (ps and ps->k == 11)
      return;
    char nb[256];
    snprintf(nb, 256, "%.*s__elab", (int) n->package_body.name.length, n->package_body.name.string);
    fprintf(generator->o, "define void @\"%s\"() {\n", nb);
    for (uint32_t i = 0; i < n->package_body.statements.count; i++)
      generate_statement_sequence(generator, n->package_body.statements.data[i]);
    fprintf(generator->o, "  ret void\n}\n");
    fprintf(
        generator->o,
        "@llvm.global_ctors=appending global[1 x {i32,ptr,ptr}][{i32,ptr,ptr}{i32 65535,ptr "
        "@\"%s\",ptr null}]\n",
        nb);
  }
}
static void generate_runtime_type(Code_Generator *generator)
{
  FILE *o = generator->o;
  fprintf(
      o,
      "declare i32 @setjmp(ptr)\ndeclare void @longjmp(ptr,i32)\ndeclare void "
      "@exit(i32)\ndeclare i32 @pthread_create(ptr,ptr,ptr,ptr)\ndeclare i32 "
      "@pthread_join(i64,ptr)\ndeclare i32 @pthread_mutex_init(ptr,ptr)\ndeclare i32 "
      "@pthread_mutex_lock(ptr)\ndeclare i32 @pthread_mutex_unlock(ptr)\ndeclare i32 "
      "@pthread_cond_init(ptr,ptr)\ndeclare i32 @pthread_cond_wait(ptr,ptr)\ndeclare i32 "
      "@pthread_cond_signal(ptr)\ndeclare i32 @pthread_cond_broadcast(ptr)\ndeclare i32 "
      "@usleep(i32)\ndeclare ptr @malloc(i64)\ndeclare ptr @realloc(ptr,i64)\ndeclare void "
      "@free(ptr)\ndeclare i32 @printf(ptr,...)\ndeclare i32 @puts(ptr)\ndeclare i32 "
      "@sprintf(ptr,ptr,...)\ndeclare i32 @snprintf(ptr,i64,ptr,...)\ndeclare i32 "
      "@strcmp(ptr,ptr)\ndeclare ptr @strcpy(ptr,ptr)\ndeclare i64 @strlen(ptr)\ndeclare ptr "
      "@memcpy(ptr,ptr,i64)\ndeclare ptr @memset(ptr,i32,i64)\ndeclare double "
      "@pow(double,double)\ndeclare double @sqrt(double)\ndeclare double @sin(double)\ndeclare "
      "double @cos(double)\ndeclare double @exp(double)\ndeclare double @log(double)\ndeclare "
      "void @llvm.memcpy.p0.p0.i64(ptr,ptr,i64,i1)\n");
  fprintf(
      o,
      "define linkonce_odr ptr @__ada_i64str_to_cstr(ptr %%p,i64 %%lo,i64 %%hi){%%ln=sub i64 "
      "%%hi,%%lo\n%%sz=add i64 %%ln,2\n%%buf=call ptr @malloc(i64 %%sz)\nbr label "
      "%%loop\nloop:\n%%i=phi i64[0,%%0],[%%ni,%%body]\n%%cmp=icmp slt i64 %%i,%%sz\nbr i1 "
      "%%cmp,label %%body,label %%done\nbody:\n%%idx=add i64 %%i,%%lo\n%%adj=sub i64 "
      "%%idx,1\n%%ep=getelementptr i64,ptr %%p,i64 %%adj\n%%cv=load i64,ptr %%ep\n%%ch=trunc i64 "
      "%%cv to i8\n%%bp=getelementptr i8,ptr %%buf,i64 %%i\nstore i8 %%ch,ptr %%bp\n%%ni=add i64 "
      "%%i,1\nbr label %%loop\ndone:\n%%zp=getelementptr i8,ptr %%buf,i64 %%ln\nstore i8 0,ptr "
      "%%zp\nret ptr %%buf}\n");
  fprintf(
      o,
      "@stdin=external global ptr\n@stdout=external global ptr\n@stderr=external global "
      "ptr\n@__ss_ptr=linkonce_odr global i64 0\n@__ss_base=linkonce_odr global ptr "
      "null\n@__ss_size=linkonce_odr global i64 0\n@__eh_cur=linkonce_odr global ptr "
      "null\n@__ex_cur=linkonce_odr global ptr null\n@__fin_list=linkonce_odr global ptr null\n");
  fprintf(
      o,
      "@.ex.CONSTRAINT_ERROR=linkonce_odr constant[17 x "
      "i8]c\"CONSTRAINT_ERROR\\00\"\n@.ex.PROGRAM_ERROR=linkonce_odr constant[14 x "
      "i8]c\"PROGRAM_ERROR\\00\"\n@.ex.STORAGE_ERROR=linkonce_odr constant[14 x "
      "i8]c\"STORAGE_ERROR\\00\"\n@.ex.TASKING_ERROR=linkonce_odr constant[14 x "
      "i8]c\"TASKING_ERROR\\00\"\n@.ex.USE_ERROR=linkonce_odr constant[10 x "
      "i8]c\"USE_ERROR\\00\"\n@.ex.NAME_ERROR=linkonce_odr constant[11 x "
      "i8]c\"NAME_ERROR\\00\"\n@.ex.STATUS_ERROR=linkonce_odr constant[13 x "
      "i8]c\"STATUS_ERROR\\00\"\n@.ex.MODE_ERROR=linkonce_odr constant[11 x "
      "i8]c\"MODE_ERROR\\00\"\n@.ex.END_ERROR=linkonce_odr constant[10 x "
      "i8]c\"END_ERROR\\00\"\n@.ex.DATA_ERROR=linkonce_odr constant[11 x "
      "i8]c\"DATA_ERROR\\00\"\n@.ex.DEVICE_ERROR=linkonce_odr constant[13 x "
      "i8]c\"DEVICE_ERROR\\00\"\n@.ex.LAYOUT_ERROR=linkonce_odr constant[13 x "
      "i8]c\"LAYOUT_ERROR\\00\"\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_ss_init(){%%p=call ptr @malloc(i64 1048576)\nstore ptr "
      "%%p,ptr @__ss_base\nstore i64 1048576,ptr @__ss_size\nstore i64 0,ptr @__ss_ptr\nret "
      "void}\n");
  fprintf(o, "define linkonce_odr i64 @__ada_ss_mark(){%%m=load i64,ptr @__ss_ptr\nret i64 %%m}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_ss_release(i64 %%m){store i64 %%m,ptr @__ss_ptr\nret "
      "void}\n");
  fprintf(
      o,
      "define linkonce_odr ptr @__ada_ss_allocate(i64 %%sz){%%1=load ptr,ptr "
      "@__ss_base\n%%2=icmp eq ptr %%1,null\nbr i1 %%2,label %%init,label %%alloc\ninit:\ncall "
      "void @__ada_ss_init()\n%%3=load ptr,ptr @__ss_base\nbr label %%alloc\nalloc:\n%%p=phi "
      "ptr[%%1,%%0],[%%3,%%init]\n%%4=load i64,ptr @__ss_ptr\n%%5=add i64 %%sz,7\n%%6=and i64 "
      "%%5,-8\n%%7=add i64 %%4,%%6\n%%8=load i64,ptr @__ss_size\n%%9=icmp ult i64 %%7,%%8\nbr i1 "
      "%%9,label %%ok,label %%grow\ngrow:\n%%10=mul i64 %%8,2\nstore i64 %%10,ptr "
      "@__ss_size\n%%11=call ptr @realloc(ptr %%p,i64 %%10)\nstore ptr %%11,ptr @__ss_base\nbr "
      "label %%ok\nok:\n%%12=phi ptr[%%p,%%alloc],[%%11,%%grow]\n%%13=getelementptr i8,ptr "
      "%%12,i64 %%4\nstore i64 %%7,ptr @__ss_ptr\nret ptr %%13}\n");
  fprintf(
      o, "define linkonce_odr ptr @__ada_setjmp(){%%p=call ptr @malloc(i64 200)\nret ptr %%p}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_push_handler(ptr %%h){%%1=load ptr,ptr @__eh_cur\nstore "
      "ptr %%1,ptr %%h\nstore ptr %%h,ptr @__eh_cur\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_pop_handler(){%%1=load ptr,ptr @__eh_cur\n%%2=icmp eq ptr "
      "%%1,null\nbr i1 %%2,label %%done,label %%pop\npop:\n%%3=load ptr,ptr %%1\nstore ptr "
      "%%3,ptr @__eh_cur\nbr label %%done\ndone:\nret void}\n");
  fprintf(o, "@.fmt_ue=linkonce_odr constant[25 x i8]c\"Unhandled exception: %%s\\0A\\00\"\n");
  fprintf(
      o,
      "define linkonce_odr ptr @__ada_task_trampoline(ptr %%arg){%%h=alloca [200 x "
      "i8]\n%%hp=getelementptr [200 x i8],ptr %%h,i64 0,i64 0\ncall void @__ada_push_handler(ptr "
      "%%hp)\n%%jv=call i32 @setjmp(ptr %%hp)\n%%jc=icmp eq i32 %%jv,0\nbr i1 %%jc,label "
      "%%run,label %%catch\nrun:\n%%fn=bitcast ptr %%arg to ptr\ncall void %%fn(ptr null)\ncall "
      "void @__ada_pop_handler()\nret ptr null\ncatch:\n%%ex=load ptr,ptr @__ex_cur\ncall "
      "i32(ptr,...)@printf(ptr @.fmt_ue,ptr %%ex)\ncall void @__ada_pop_handler()\nret ptr "
      "null}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_raise(ptr %%msg){store ptr %%msg,ptr @__ex_cur\n%%jb=load "
      "ptr,ptr @__eh_cur\ncall void @longjmp(ptr %%jb,i32 1)\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_delay(i64 %%us){%%t=trunc i64 %%us to i32\n%%r=call i32 "
      "@usleep(i32 %%t)\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr i64 @__ada_powi(i64 %%base,i64 %%exp){entry:\n%%result=alloca "
      "i64\nstore i64 1,ptr %%result\n%%e=alloca i64\nstore i64 %%exp,ptr %%e\nbr label "
      "%%loop\nloop:\n%%ev=load i64,ptr %%e\n%%cmp=icmp sgt i64 %%ev,0\nbr i1 %%cmp,label "
      "%%body,label %%done\nbody:\n%%rv=load i64,ptr %%result\n%%nv=mul i64 %%rv,%%base\nstore "
      "i64 %%nv,ptr %%result\n%%ev2=load i64,ptr %%e\n%%ev3=sub i64 %%ev2,1\nstore i64 %%ev3,ptr "
      "%%e\nbr label %%loop\ndone:\n%%final=load i64,ptr %%result\nret i64 %%final}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_finalize(ptr %%obj,ptr %%fn){%%1=call ptr @malloc(i64 "
      "16)\n%%2=getelementptr ptr,ptr %%1,i64 0\nstore ptr %%obj,ptr %%2\n%%3=getelementptr "
      "ptr,ptr %%1,i64 1\nstore ptr %%fn,ptr %%3\n%%4=load ptr,ptr "
      "@__fin_list\n%%5=getelementptr ptr,ptr %%1,i64 2\nstore ptr %%4,ptr %%5\nstore ptr "
      "%%1,ptr @__fin_list\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_finalize_all(){entry:\n%%1=load ptr,ptr @__fin_list\nbr "
      "label %%loop\nloop:\n%%p=phi ptr[%%1,%%entry],[%%9,%%fin]\n%%2=icmp eq ptr %%p,null\nbr "
      "i1 %%2,label %%done,label %%fin\nfin:\n%%3=getelementptr ptr,ptr %%p,i64 0\n%%4=load "
      "ptr,ptr %%3\n%%5=getelementptr ptr,ptr %%p,i64 1\n%%6=load ptr,ptr %%5\n%%7=bitcast ptr "
      "%%6 to ptr\ncall void %%7(ptr %%4)\n%%8=getelementptr ptr,ptr %%p,i64 2\n%%9=load ptr,ptr "
      "%%8\ncall void @free(ptr %%p)\nbr label %%loop\ndone:\nret void}\n");
  fprintf(
      o,
      "@.fmt_d=linkonce_odr constant[5 x i8]c\"%%lld\\00\"\n@.fmt_s=linkonce_odr constant[3 x "
      "i8]c\"%%s\\00\"\n@.fmt_f=linkonce_odr constant[3 x i8]c\"%%g\\00\"\n");
  fprintf(
      o, "define linkonce_odr void @__text_io_new_line(){call i32 @putchar(i32 10)\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_put_char(i64 %%c){%%1=trunc i64 %%c to i32\ncall i32 "
      "@putchar(i32 %%1)\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_put(ptr %%s){entry:\n%%len=call i64 @strlen(ptr "
      "%%s)\nbr label %%loop\nloop:\n%%i=phi i64[0,%%entry],[%%next,%%body]\n%%cmp=icmp slt i64 "
      "%%i,%%len\nbr i1 %%cmp,label %%body,label %%done\nbody:\n%%charptr=getelementptr i8,ptr "
      "%%s,i64 %%i\n%%ch8=load i8,ptr %%charptr\n%%ch=sext i8 %%ch8 to i32\ncall i32 "
      "@putchar(i32 %%ch)\n%%next=add i64 %%i,1\nbr label %%loop\ndone:\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_put_line(ptr %%s){call void @__text_io_put(ptr "
      "%%s)\ncall void @__text_io_new_line()\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_put.i64(ptr %%file, i64 %%v){%%fmt=getelementptr[5 x i8],ptr "
      "@.fmt_d,i64 0,i64 0\n%%1=call i32(ptr,...)@printf(ptr %%fmt,i64 %%v)\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_put_line.i64(ptr %%file, i64 %%v){call void @__text_io_put.i64(ptr "
      "%%file,i64 %%v)\ncall void @__text_io_new_line()\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_put.f64(ptr %%file, double %%v){%%fmt=getelementptr[3 x i8],ptr "
      "@.fmt_f,i64 0,i64 0\n%%1=call i32(ptr,...)@printf(ptr %%fmt,double %%v)\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_put_line.f64(ptr %%file, double %%v){call void @__text_io_put.f64(ptr "
      "%%file,double %%v)\ncall void @__text_io_new_line()\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_get_char(ptr %%p){%%1=call i32 @getchar()\n%%2=icmp "
      "eq i32 %%1,-1\n%%3=sext i32 %%1 to i64\n%%4=select i1 %%2,i64 0,i64 %%3\nstore i64 "
      "%%4,ptr %%p\nret void}\n");
  fprintf(
      o,
      "define linkonce_odr void @__text_io_get_line(ptr %%b,ptr %%n){store i64 0,ptr %%n\nret "
      "void}\n");
  fprintf(o, "declare i32 @putchar(i32)\ndeclare i32 @getchar()\n");
  fprintf(
      o,
      "define linkonce_odr ptr @__ada_image_enum(i64 %%v,i64 %%f,i64 %%l){%%p=sub i64 "
      "%%v,%%f\n%%fmt=getelementptr[5 x i8],ptr @.fmt_d,i64 0,i64 0\n%%buf=alloca[32 x "
      "i8]\n%%1=getelementptr[32 x i8],ptr %%buf,i64 0,i64 0\n%%add=add i64 %%p,1\n%%2=call "
      "i32(ptr,ptr,...)@sprintf(ptr %%1,ptr %%fmt,i64 %%add)\n%%n=sext i32 %%2 to i64\n%%sz=add "
      "i64 %%n,1\n%%rsz=mul i64 %%sz,8\n%%r=call ptr @malloc(i64 %%rsz)\nstore i64 %%n,ptr "
      "%%r\nbr label %%loop\nloop:\n%%i=phi i64[0,%%0],[%%9,%%body]\n%%3=icmp slt i64 "
      "%%i,%%n\nbr i1 %%3,label %%body,label %%done\nbody:\n%%4=getelementptr[32 x i8],ptr "
      "%%buf,i64 0,i64 %%i\n%%5=load i8,ptr %%4\n%%6=sext i8 %%5 to i64\n%%7=add i64 "
      "%%i,1\n%%8=getelementptr i64,ptr %%r,i64 %%7\nstore i64 %%6,ptr %%8\n%%9=add i64 "
      "%%i,1\nbr label %%loop\ndone:\nret ptr %%r}\n");
  fprintf(
      o,
      "define linkonce_odr i64 @__ada_value_int(ptr %%s){%%pn=load i64,ptr %%s\n%%buf=call ptr "
      "@malloc(i64 %%pn)\nbr label %%copy\ncopy:\n%%ci=phi i64[0,%%0],[%%next,%%cbody]\n%%1=icmp "
      "slt i64 %%ci,%%pn\nbr i1 %%1,label %%cbody,label %%parse\ncbody:\n%%idx=add i64 "
      "%%ci,1\n%%sptr=getelementptr i64,ptr %%s,i64 %%idx\n%%charval=load i64,ptr "
      "%%sptr\n%%ch=trunc i64 %%charval to i8\n%%bptr=getelementptr i8,ptr %%buf,i64 %%ci\nstore "
      "i8 %%ch,ptr %%bptr\n%%next=add i64 %%ci,1\nbr label %%copy\nparse:\n%%null=getelementptr "
      "i8,ptr %%buf,i64 %%pn\nstore i8 0,ptr %%null\n%%result=call "
      "i64(ptr,ptr,i32,...)@strtoll(ptr %%buf,ptr null,i32 10)\ncall void @free(ptr %%buf)\nret "
      "i64 %%result}\ndeclare i64 @strtoll(ptr,ptr,i32,...)\n");
  fprintf(
      o,
      "define linkonce_odr ptr @__ada_image_int(i64 %%v){%%buf=alloca[32 x "
      "i8]\n%%1=getelementptr[32 x i8],ptr %%buf,i64 0,i64 0\n%%fmt=getelementptr[5 x i8],ptr "
      "@.fmt_d,i64 0,i64 0\n%%2=call i32(ptr,ptr,...)@sprintf(ptr %%1,ptr %%fmt,i64 "
      "%%v)\n%%n=sext i32 %%2 to i64\n%%sz=add i64 %%n,1\n%%rsz=mul i64 %%sz,8\n%%r=call ptr "
      "@malloc(i64 %%rsz)\nstore i64 %%n,ptr %%r\nbr label %%loop\nloop:\n%%i=phi "
      "i64[0,%%0],[%%8,%%body]\n%%3=icmp slt i64 %%i,%%n\nbr i1 %%3,label %%body,label "
      "%%done\nbody:\n%%4=getelementptr[32 x i8],ptr %%buf,i64 0,i64 %%i\n%%5=load i8,ptr "
      "%%4\n%%6=sext i8 %%5 to i64\n%%7=add i64 %%i,1\n%%idx=getelementptr i64,ptr %%r,i64 "
      "%%7\nstore i64 %%6,ptr %%idx\n%%8=add i64 %%i,1\nbr label %%loop\ndone:\nret ptr %%r}\n");
  fprintf(
      o,
      "define linkonce_odr void @__ada_check_range(i64 %%v,i64 %%lo,i64 %%hi){%%1=icmp sge i64 "
      "%%v,%%lo\nbr i1 %%1,label %%ok1,label %%err\nok1:\n%%2=icmp sle i64 %%v,%%hi\nbr i1 "
      "%%2,label %%ok2,label %%err\nok2:\nret void\nerr:\ncall void @__ada_raise(ptr "
      "@.ex.CONSTRAINT_ERROR)\nunreachable}\n");
  fprintf(
      o,
      "define linkonce_odr i64 @__attr_PRED_BOOLEAN(i64 %%x){\n  %%t0 = sub i64 %%x, 1\n  ret "
      "i64 %%t0\n}\n");
  fprintf(
      o,
      "define linkonce_odr i64 @__attr_SUCC_BOOLEAN(i64 %%x){\n  %%t0 = add i64 %%x, 1\n  ret "
      "i64 %%t0\n}\n");
  fprintf(
      o,
      "define linkonce_odr i64 @__attr_PRED_INTEGER(i64 %%x){\n  %%t0 = sub i64 %%x, 1\n  ret "
      "i64 %%t0\n}\n");
  fprintf(
      o,
      "define linkonce_odr i64 @__attr_SUCC_INTEGER(i64 %%x){\n  %%t0 = add i64 %%x, 1\n  ret "
      "i64 %%t0\n}\n");
  fprintf(o, "define linkonce_odr i64 @__attr_POS_BOOLEAN(i64 %%x){\n  ret i64 %%x\n}\n");
  fprintf(o, "define linkonce_odr i64 @__attr_POS_INTEGER(i64 %%x){\n  ret i64 %%x\n}\n");
  fprintf(o, "define linkonce_odr i64 @__attr_VAL_BOOLEAN(i64 %%x){\n  ret i64 %%x\n}\n");
  fprintf(o, "define linkonce_odr i64 @__attr_VAL_INTEGER(i64 %%x){\n  ret i64 %%x\n}\n");
  fprintf(
      o,
      "define linkonce_odr ptr @__attr_IMAGE_INTEGER(i64 %%x){\n  %%t0 = call ptr "
      "@__ada_image_int(i64 %%x)\n  ret ptr %%t0\n}\n");
  fprintf(
      o,
      "define linkonce_odr i64 @__attr_VALUE_INTEGER(ptr %%x){\n  %%t0 = call i64 "
      "@__ada_value_int(ptr %%x)\n  ret i64 %%t0\n}\n");
}
static char *read_file_contents(const char *path)
{
  FILE *f = fopen(path, "rb");
  if (not f)
    return 0;
  fseek(f, 0, SEEK_END);
  long z = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *b = malloc(z + 1);
  size_t _ = fread(b, 1, z, f);
  (void) _;
  b[z] = 0;
  fclose(f);
  return b;
}
static void print_forward_declarations(Code_Generator *generator, Symbol_Manager *sm)
{
  // First, emit declarations for external (imported) symbols
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = sm->sy[h]; s; s = s->next)
      if (s->is_external)
        for (uint32_t k = 0; k < s->overloads.count; k++)
        {
          Syntax_Node *n = s->overloads.data[k];
          if (n and (n->k == N_PD or n->k == N_FD))
          {
            // External symbol - emit declare statement
            char nb[256];
            snprintf(nb, 256, "%.*s", (int) s->external_name.length, s->external_name.string);
            if (not add_declaration(generator, nb))
              continue;
            Syntax_Node *sp = n->body.subprogram_spec;
            if (n->k == N_PD)
            {
              // Procedure
              fprintf(generator->o, "declare void @\"%s\"(", nb);
              for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
              {
                if (i)
                  fprintf(generator->o, ",");
                Syntax_Node *p = sp->subprogram.parameters.data[i];
                Type_Info *pt = p->parameter.ty ? resolve_subtype(sm, p->parameter.ty) : 0;
                Value_Kind k = pt ? token_kind_to_value_kind(pt) : VALUE_KIND_INTEGER;
                if (p->parameter.mode & 2)
                  fprintf(generator->o, "ptr");
                else
                  fprintf(generator->o, "%s", value_llvm_type_string(k));
              }
              fprintf(generator->o, ")\n");
            }
            else
            {
              // Function
              Type_Info *rt = sp->subprogram.return_type ? resolve_subtype(sm, sp->subprogram.return_type) : 0;
              Value_Kind rk = rt ? token_kind_to_value_kind(rt) : VALUE_KIND_INTEGER;
              fprintf(generator->o, "declare %s @\"%s\"(", value_llvm_type_string(rk), nb);
              for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
              {
                if (i)
                  fprintf(generator->o, ",");
                Syntax_Node *p = sp->subprogram.parameters.data[i];
                Type_Info *pt = p->parameter.ty ? resolve_subtype(sm, p->parameter.ty) : 0;
                Value_Kind k = pt ? token_kind_to_value_kind(pt) : VALUE_KIND_INTEGER;
                if (p->parameter.mode & 2)
                  fprintf(generator->o, "ptr");
                else
                  fprintf(generator->o, "%s", value_llvm_type_string(k));
              }
              fprintf(generator->o, ")\n");
            }
          }
        }

  // Then emit forward declarations for regular symbols
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = sm->sy[h]; s; s = s->next)
      if (s->level == 0 and not s->is_external)
        for (uint32_t k = 0; k < s->overloads.count; k++)
        {
          Syntax_Node *n = s->overloads.data[k];
          if (n and (n->k == N_PB or n->k == N_FB))
          {
            Syntax_Node *sp = n->body.subprogram_spec;
            char nb[256];
            encode_symbol_name(nb, 256, n->symbol, sp->subprogram.name, sp->subprogram.parameters.count, sp);
            add_declaration(generator, nb);
          }
        }
}
static Library_Unit *lfnd(Symbol_Manager *symbol_manager, String_Slice nm)
{
  for (uint32_t i = 0; i < symbol_manager->lu.count; i++)
  {
    Library_Unit *l = symbol_manager->lu.data[i];
    if (string_equal_ignore_case(l->name, nm))
      return l;
  }
  return 0;
}
static uint64_t find_type_symbol(const char *p)
{
  struct stat s;
  if (stat(p, &s))
    return 0;
  return s.st_mtime;
}
static void write_ada_library_interface(Symbol_Manager *symbol_manager, const char *fn, Syntax_Node *cu)
{
  if (not cu or cu->compilation_unit.units.count == 0)
    return;
  Syntax_Node *u0 = cu->compilation_unit.units.data[0];
  String_Slice nm = u0->k == N_PKS ? u0->package_spec.name : u0->k == N_PKB ? u0->package_body.name : N;
  char alp[520];
  if (nm.string and nm.length > 0)
  {
    const char *sl = strrchr(fn, '/');
    int pos = 0;
    if (sl)
    {
      int dl = sl - fn + 1;
      for (int i = 0; i < dl and pos < 519; i++)
        alp[pos++] = fn[i];
    }
    for (uint32_t i = 0; i < nm.length and pos < 519; i++)
      alp[pos++] = tolower(nm.string[i]);
    alp[pos] = 0;
    strcat(alp, ".ali");
  }
  else
  {
    snprintf(alp, 520, "%s.ali", fn);
  }
  FILE *f = fopen(alp, "w");
  if (not f)
    return;
  fprintf(f, "V 1.0\n");
  fprintf(f, "Unsigned_Big_Integer %.*s\n", (int) nm.length, nm.string);
  if (cu->compilation_unit.context)
    for (uint32_t i = 0; i < cu->compilation_unit.context->context.with_clauses.count; i++)
    {
      Syntax_Node *w = cu->compilation_unit.context->context.with_clauses.data[i];
      char pf[256];
      int n = snprintf(pf, 256, "%.*s", (int) w->with_clause.name.length, w->with_clause.name.string);
      for (int j = 0; j < n; j++)
        pf[j] = tolower(pf[j]);
      uint64_t ts = find_type_symbol(pf);
      fprintf(f, "W %.*s %lu\n", (int) w->with_clause.name.length, w->with_clause.name.string, (unsigned long) ts);
    }
  for (int i = 0; i < symbol_manager->dpn; i++)
    if (symbol_manager->dps[i].count and symbol_manager->dps[i].data[0])
      fprintf(f, "D %.*s\n", (int) symbol_manager->dps[i].data[0]->name.length, symbol_manager->dps[i].data[0]->name.string);
  for (int h = 0; h < 4096; h++)
  {
    for (Symbol *s = symbol_manager->sy[h]; s; s = s->next)
    {
      if ((s->k == 4 or s->k == 5) and s->parent and string_equal_ignore_case(s->parent->name, nm))
      {
        Syntax_Node *sp = s->overloads.count > 0 and s->overloads.data[0]->body.subprogram_spec ? s->overloads.data[0]->body.subprogram_spec : 0;
        char nb[256];
        if (s->mangled_name.string)
        {
          snprintf(nb, 256, "%.*s", (int) s->mangled_name.length, s->mangled_name.string);
        }
        else
        {
          encode_symbol_name(nb, 256, s, s->name, sp ? sp->subprogram.parameters.count : 0, sp);
          if (s)
          {
            s->mangled_name.string = arena_allocate(strlen(nb) + 1);
            memcpy((char *) s->mangled_name.string, nb, strlen(nb) + 1);
            s->mangled_name.length = strlen(nb);
          }
        }
        fprintf(f, "X %s", nb);
        if (s->k == 4)
        {
          fprintf(f, " void");
          if (sp)
            for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
            {
              Syntax_Node *p = sp->subprogram.parameters.data[i];
              Value_Kind k = p->parameter.ty ? token_kind_to_value_kind(resolve_subtype(symbol_manager, p->parameter.ty))
                                      : VALUE_KIND_INTEGER;
              fprintf(f, " %s", value_llvm_type_string(k));
            }
        }
        else
        {
          Type_Info *rt = sp and sp->subprogram.return_type ? resolve_subtype(symbol_manager, sp->subprogram.return_type) : 0;
          Value_Kind rk = token_kind_to_value_kind(rt);
          fprintf(f, " %s", value_llvm_type_string(rk));
          if (sp)
            for (uint32_t i = 0; i < sp->subprogram.parameters.count; i++)
            {
              Syntax_Node *p = sp->subprogram.parameters.data[i];
              Value_Kind k = p->parameter.ty ? token_kind_to_value_kind(resolve_subtype(symbol_manager, p->parameter.ty))
                                      : VALUE_KIND_INTEGER;
              fprintf(f, " %s", value_llvm_type_string(k));
            }
        }
        fprintf(f, "\n");
      }
      else if (
          (s->k == 0 or s->k == 2) and s->level == 0 and s->parent
          and string_equal_ignore_case(s->parent->name, nm))
      {
        char nb[256];
        if (s->parent and (uintptr_t) s->parent > 4096 and s->parent->name.string)
        {
          int n = 0;
          for (uint32_t j = 0; j < s->parent->name.length; j++)
            nb[n++] = toupper(s->parent->name.string[j]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
          for (uint32_t j = 0; j < s->name.length; j++)
            nb[n++] = toupper(s->name.string[j]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->name.length, s->name.string);
        Value_Kind k = s->type_info ? token_kind_to_value_kind(s->type_info) : VALUE_KIND_INTEGER;
        fprintf(f, "X %s %s\n", nb, value_llvm_type_string(k));
      }
    }
  }
  for (uint32_t i = 0; i < symbol_manager->eh.count; i++)
  {
    bool f2 = 0;
    for (uint32_t j = 0; j < i; j++)
      if (string_equal_ignore_case(symbol_manager->eh.data[j], symbol_manager->eh.data[i]))
      {
        f2 = 1;
        break;
      }
    if (not f2)
      fprintf(f, "H %.*s\n", (int) symbol_manager->eh.data[i].length, symbol_manager->eh.data[i].string);
  }
  if (symbol_manager->eo > 0)
    fprintf(f, "E %d\n", symbol_manager->eo);
  fclose(f);
}
static bool label_compare(Symbol_Manager *symbol_manager, String_Slice nm, String_Slice pth)
{
  Library_Unit *ex = lfnd(symbol_manager, nm);
  if (ex and ex->is_compiled)
    return true;
  char fp[512];
  snprintf(fp, 512, "%.*s.adb", (int) pth.length, pth.string);
  char *src = read_file_contents(fp);
  if (not src)
  {
    snprintf(fp, 512, "%.*s.ads", (int) pth.length, pth.string);
    src = read_file_contents(fp);
  }
  if (not src)
    return false;
  Parser p = parser_new(src, strlen(src), fp);
  Syntax_Node *cu = parse_compilation_unit(&p);
  if (not cu)
    return false;
  Symbol_Manager sm;
  symbol_manager_init(&sm);
  sm.lu = symbol_manager->lu;
  sm.gt = symbol_manager->gt;
  symbol_manager_use_clauses(&sm, cu);
  char op[512];
  snprintf(op, 512, "%.*s.ll", (int) pth.length, pth.string);
  FILE *o = fopen(op, "w");
  Code_Generator g = {o, 0, 0, 0, &sm, {0}, 0, {0}, 0, {0}, 0, {0}, 0, {0}, {0}, {0}, {0}, 0};
  generate_runtime_type(&g);
  print_forward_declarations(&g, &sm);
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = sm.sy[h]; s; s = s->next)
      if ((s->k == 0 or s->k == 2) and s->level == 0 and s->parent and not s->is_external and s->overloads.count == 0)
      {
        Value_Kind k = s->type_info ? token_kind_to_value_kind(s->type_info) : VALUE_KIND_INTEGER;
        char nb[256];
        int n = 0;
        for (uint32_t j = 0; j < s->parent->name.length; j++)
          nb[n++] = toupper(s->parent->name.string[j]);
        n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
        for (uint32_t j = 0; j < s->name.length; j++)
          nb[n++] = toupper(s->name.string[j]);
        nb[n] = 0;
        if (s->k == 2 and s->definition and s->definition->k == N_STR)
        {
          uint32_t len = s->definition->string_value.length;
          fprintf(o, "@%s=linkonce_odr constant [%u x i8]c\"", nb, len + 1);
          for (uint32_t i = 0; i < len; i++)
          {
            char c = s->definition->string_value.string[i];
            if (c == '"')
              fprintf(o, "\\22");
            else if (c == '\\')
              fprintf(o, "\\5C");
            else if (c < 32 or c > 126)
              fprintf(o, "\\%02X", (unsigned char) c);
            else
              fprintf(o, "%c", c);
          }
          fprintf(o, "\\00\"\n");
        }
        else
        {
          char iv[64];
          if (k == VALUE_KIND_INTEGER and s->definition and s->definition->k == N_INT)
          {
            snprintf(iv, 64, "%ld", s->definition->integer_value);
          }
          else if (k == VALUE_KIND_INTEGER and s->value != 0)
          {
            snprintf(iv, 64, "%ld", s->value);
          }
          else
          {
            snprintf(
                iv,
                64,
                "%s",
                k == VALUE_KIND_POINTER ? "null"
                : k == VALUE_KIND_FLOAT ? "0.0"
                                        : "0");
          }
          Type_Info *at = s->type_info ? type_canonical_concrete(s->type_info) : 0;
          if (at and at->k == TYPE_ARRAY and at->low_bound <= at->high_bound)
          {
            int asz = (int) (at->high_bound - at->low_bound + 1);
            fprintf(
                o,
                "@%s=linkonce_odr %s [%d x %s] zeroinitializer\n",
                nb,
                s->k == 2 ? "constant" : "global",
                asz,
                ada_to_c_type_string(at->element_type));
          }
          else if (at and at->k == TYPE_ARRAY)
          {
            fprintf(
                o,
                "@%s=linkonce_odr %s {ptr,ptr} {ptr null,ptr null}\n",
                nb,
                s->k == 2 ? "constant" : "global");
          }
          else
          {
            fprintf(
                o,
                "@%s=linkonce_odr %s %s %s\n",
                nb,
                s->k == 2 ? "constant" : "global",
                value_llvm_type_string(k),
                iv);
          }
        }
      }
  for (uint32_t i = 0; i < sm.eo; i++)
  {
    for (uint32_t j = 0; j < 4096; j++)
    {
      for (Symbol *s = sm.sy[j]; s; s = s->next)
      {
        if (s->elaboration_level == i)
        {
          for (uint32_t k = 0; k < s->overloads.count; k++)
            generate_declaration(&g, s->overloads.data[k]);
        }
      }
    }
  }
  for (uint32_t ui = 0; ui < cu->compilation_unit.units.count; ui++)
  {
    Syntax_Node *u = cu->compilation_unit.units.data[ui];
    if (u->k == N_PKB)
      generate_expression_llvm(&g, u);
  }
  for (uint32_t i = 0; i < sm.ib.count; i++)
    generate_expression_llvm(&g, sm.ib.data[i]);
  emit_all_metadata(&g);
  fclose(o);
  Library_Unit *l = label_use_new(cu->compilation_unit.units.count > 0 ? cu->compilation_unit.units.data[0]->k : 0, nm, pth);
  l->is_compiled = true;
  l->timestamp = find_type_symbol(fp);
  lv(&symbol_manager->lu, l);
  return true;
}
int main(int ac, char **av)
{
  include_paths[include_path_count++] = ".";
  int ai = 1;
  while (ai < ac and av[ai][0] == '-')
  {
    if (av[ai][1] == 'I' and av[ai][2] == 0 and ai + 1 < ac)
      include_paths[include_path_count++] = av[++ai];
    else if (av[ai][1] == 'I')
      include_paths[include_path_count++] = av[ai] + 2;
    ai++;
  }
  if (ai >= ac)
  {
    fprintf(stderr, "u: %s [-Ipath...] f.adb\n", av[0]);
    return 1;
  }
  const char *inf = av[ai];
  char *src = read_file_contents(inf);
  if (not src)
  {
    fprintf(stderr, "e: %s\n", inf);
    return 1;
  }
  Parser p = parser_new(src, strlen(src), inf);
  Syntax_Node *cu = parse_compilation_unit(&p);
  if (p.error_count or not cu)
    return 1;
  Symbol_Manager sm;
  symbol_manager_init(&sm);
  {
    const char *asrc = lookup_path(&sm, STRING_LITERAL("ascii"));
    if (asrc)
      parse_package_specification(&sm, STRING_LITERAL("ascii"), asrc);
  }
  char sd[520] = {0};
  const char *sl = strrchr(inf, '/');
  if (sl)
  {
    size_t dl = sl - inf + 1;
    if (dl < 519)
    {
      memcpy(sd, inf, dl);
      sd[dl] = 0;
    }
  }
  if (cu->compilation_unit.context)
    for (uint32_t i = 0; i < cu->compilation_unit.context->context.with_clauses.count; i++)
    {
      Syntax_Node *w = cu->compilation_unit.context->context.with_clauses.data[i];
      char *ln = string_to_lowercase(w->with_clause.name);
      bool ld = 0;
      if (sd[0])
      {
        char pb[520];
        snprintf(pb, 520, "%s%s", sd, ln);
        if (label_compare(&sm, w->with_clause.name, (String_Slice){pb, strlen(pb)}))
          ld = 1;
      }
      for (int j = 0; j < include_path_count and not ld; j++)
      {
        char pb[520];
        snprintf(
            pb,
            520,
            "%s%s%s",
            include_paths[j],
            include_paths[j][0] and include_paths[j][strlen(include_paths[j]) - 1] != '/' ? "/"
                                                                                          : "",
            ln);
        if (label_compare(&sm, w->with_clause.name, (String_Slice){pb, strlen(pb)}))
          ld = 1;
      }
    }
  symbol_manager_use_clauses(&sm, cu);
  {
    char pth[520];
    strncpy(pth, inf, 512);
    char *dot = strrchr(pth, '.');
    if (dot)
      *dot = 0;
    read_ada_library_interface(&sm, pth);
  }
  char of[520];
  strncpy(of, inf, 512);
  char *dt = strrchr(of, '.');
  if (dt)
    *dt = 0;
  snprintf(of + strlen(of), 520 - strlen(of), ".ll");
  FILE *o = stdout;
  Code_Generator g = {o, 0, 0, 0, &sm, {0}, 0, {0}, 0, {0}, 0, {0}, 13, {0}, {0}, {0}, {0}, 0};
  generate_runtime_type(&g);
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = sm.sy[h]; s; s = s->next)
      if ((s->k == 0 or s->k == 2) and (s->level == 0 or s->parent) and not(s->parent and lfnd(&sm, s->parent->name))
          and not s->is_external)
      {
        Value_Kind k = s->type_info ? token_kind_to_value_kind(s->type_info) : VALUE_KIND_INTEGER;
        char nb[256];
        if (s->parent and (uintptr_t) s->parent > 4096 and s->parent->name.string)
        {
          int n = 0;
          for (uint32_t j = 0; j < s->parent->name.length; j++)
            nb[n++] = toupper(s->parent->name.string[j]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->parent->scope, s->parent->elaboration_level);
          for (uint32_t j = 0; j < s->name.length; j++)
            nb[n++] = toupper(s->name.string[j]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->name.length, s->name.string);
        if (s->k == 2 and s->definition and s->definition->k == N_STR)
        {
          uint32_t len = s->definition->string_value.length;
          fprintf(o, "@%s=linkonce_odr constant [%u x i8]c\"", nb, len + 1);
          for (uint32_t i = 0; i < len; i++)
          {
            char c = s->definition->string_value.string[i];
            if (c == '"')
              fprintf(o, "\\22");
            else if (c == '\\')
              fprintf(o, "\\5C");
            else if (c < 32 or c > 126)
              fprintf(o, "\\%02X", (unsigned char) c);
            else
              fprintf(o, "%c", c);
          }
          fprintf(o, "\\00\"\n");
        }
        else
        {
          char iv[64];
          if (k == VALUE_KIND_INTEGER and s->definition and s->definition->k == N_INT)
          {
            snprintf(iv, 64, "%ld", s->definition->integer_value);
          }
          else if (k == VALUE_KIND_INTEGER and s->value != 0)
          {
            snprintf(iv, 64, "%ld", s->value);
          }
          else
          {
            snprintf(
                iv,
                64,
                "%s",
                k == VALUE_KIND_POINTER ? "null"
                : k == VALUE_KIND_FLOAT ? "0.0"
                                        : "0");
          }
          Type_Info *at = s->type_info ? type_canonical_concrete(s->type_info) : 0;
          if (at and at->k == TYPE_ARRAY and at->low_bound <= at->high_bound)
          {
            int asz = (int) (at->high_bound - at->low_bound + 1);
            fprintf(
                o,
                "@%s=linkonce_odr %s [%d x %s] zeroinitializer\n",
                nb,
                s->k == 2 ? "constant" : "global",
                asz,
                ada_to_c_type_string(at->element_type));
          }
          else if (at and at->k == TYPE_ARRAY)
          {
            fprintf(
                o,
                "@%s=linkonce_odr %s {ptr,ptr} {ptr null,ptr null}\n",
                nb,
                s->k == 2 ? "constant" : "global");
          }
          else
          {
            fprintf(
                o,
                "@%s=linkonce_odr %s %s %s\n",
                nb,
                s->k == 2 ? "constant" : "global",
                value_llvm_type_string(k),
                iv);
          }
        }
      }
  print_forward_declarations(&g, &sm);
  for (uint32_t i = 0; i < sm.eo; i++)
    for (uint32_t j = 0; j < 4096; j++)
      for (Symbol *s = sm.sy[j]; s; s = s->next)
        if (s->elaboration_level == i and s->level == 0)
          for (uint32_t k = 0; k < s->overloads.count; k++)
            generate_declaration(&g, s->overloads.data[k]);
  for (uint32_t ui = 0; ui < cu->compilation_unit.units.count; ui++)
  {
    Syntax_Node *u = cu->compilation_unit.units.data[ui];
    if (u->k == N_PKB)
      generate_expression_llvm(&g, u);
  }
  for (uint32_t ui = 0; ui < cu->compilation_unit.units.count; ui++)
  {
    Syntax_Node *u = cu->compilation_unit.units.data[ui];
    if (u->k == N_PB or u->k == N_FB)
      generate_expression_llvm(&g, u);
  }
  for (uint32_t i = 0; i < sm.ib.count; i++)
    generate_expression_llvm(&g, sm.ib.data[i]);
  for (uint32_t ui = cu->compilation_unit.units.count; ui > 0; ui--)
  {
    Syntax_Node *u = cu->compilation_unit.units.data[ui - 1];
    if (u->k == N_PB)
    {
      Syntax_Node *sp = u->body.subprogram_spec;
      Symbol *ms = 0;
      for (int h = 0; h < 4096 and not ms; h++)
        for (Symbol *s = sm.sy[h]; s; s = s->next)
          if (s->level == 0 and string_equal_ignore_case(s->name, sp->subprogram.name))
          {
            ms = s;
            break;
          }
      char nb[256];
      encode_symbol_name(nb, 256, ms, sp->subprogram.name, sp->subprogram.parameters.count, sp);
      fprintf(
          o,
          "define i32 @main(){\n  call void @__ada_ss_init()\n  call void @\"%s\"()\n  ret "
          "i32 0\n}\n",
          nb);
      break;
    }
  }
  emit_all_metadata(&g);
  if (o != stdout)
    fclose(o);
  of[strlen(of) - 3] = 0;
  write_ada_library_interface(&sm, of, cu);
  return 0;
}
