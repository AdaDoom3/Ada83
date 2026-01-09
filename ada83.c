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
  char *b, *p, *e;
} A;
typedef struct
{
  const char *s;
  uint32_t n;
} String_Slice;
typedef struct
{
  uint32_t l, c;
  const char *f;
} Source_Location;
#define STRING_LITERAL(x) ((String_Slice){x, sizeof(x) - 1})
#define N ((String_Slice){0, 0})
static A main_arena = {0};
static int error_count = 0;
static String_Slice SEP_PKG = N;
static void *arena_allocate(size_t n)
{
  n = (n + 7) & ~7;
  if (not main_arena.b or main_arena.p + n > main_arena.e)
  {
    size_t z = 1 << 24;
    main_arena.b = main_arena.p = malloc(z);
    main_arena.e = main_arena.b + z;
  }
  void *r = main_arena.p;
  main_arena.p += n;
  return memset(r, 0, n);
}
static String_Slice string_duplicate(String_Slice s)
{
  char *p = arena_allocate(s.n + 1);
  memcpy(p, s.s, s.n);
  return (String_Slice){p, s.n};
}
static bool string_equal_ignore_case(String_Slice a, String_Slice b)
{
  if (a.n != b.n)
    return 0;
  for (uint32_t i = 0; i < a.n; i++)
    if (tolower(a.s[i]) != tolower(b.s[i]))
      return 0;
  return 1;
}
static char *string_to_lowercase(String_Slice s)
{
  static char b[8][256];
  static int i;
  char *p = b[i++ & 7];
  uint32_t n = s.n < 255 ? s.n : 255;
  for (uint32_t j = 0; j < n; j++)
    p[j] = tolower(s.s[j]);
  p[n] = 0;
  return p;
}
static uint64_t string_hash(String_Slice s)
{
  uint64_t h = 14695981039346656037ULL;
  for (uint32_t i = 0; i < s.n; i++)
    h = (h ^ (uint8_t) tolower(s.s[i])) * 1099511628211ULL;
  return h;
}
static _Noreturn void fatal_error(Source_Location l, const char *f, ...)
{
  va_list v;
  va_start(v, f);
  fprintf(stderr, "%s:%u:%u: ", l.f, l.l, l.c);
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
  Token_Kind t;
  Source_Location l;
  String_Slice lit;
  int64_t iv;
  double fv;
  Unsigned_Big_Integer *ui;
  Rational_Number *ur;
} Token;
typedef struct
{
  const char *s, *c, *e;
  uint32_t ln, cl;
  const char *f;
  Token_Kind pt;
} Lexer;
static struct
{
  String_Slice k;
  Token_Kind t;
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
static Token_Kind kl(String_Slice s)
{
  for (int i = 0; KW[i].k.s; i++)
    if (string_equal_ignore_case(s, KW[i].k))
      return KW[i].t;
  return T_ID;
}
static Lexer ln(const char *s, size_t z, const char *f)
{
  return (Lexer){s, s, s + z, 1, 1, f, T_EOF};
}
static char peek(Lexer *l, size_t off)
{
  return l->c + off < l->e ? l->c[off] : 0;
}
static char av(Lexer *l)
{
  if (l->c >= l->e)
    return 0;
  char c = *l->c++;
  if (c == '\n')
  {
    l->ln++;
    l->cl = 1;
  }
  else
    l->cl++;
  return c;
}
static void sw(Lexer *l)
{
  for (;;)
  {
    while (
        l->c < l->e
        and (*l->c == ' ' or *l->c == '\t' or *l->c == '\n' or *l->c == '\r' or *l->c == '\v' or *l->c == '\f'))
      av(l);
    if (l->c + 1 < l->e and l->c[0] == '-' and l->c[1] == '-')
    {
      while (l->c < l->e and *l->c != '\n')
        av(l);
    }
    else
      break;
  }
}
static Token mt(Token_Kind t, Source_Location lc, String_Slice lt)
{
  return (Token){t, lc, lt, 0, 0.0, 0, 0};
}
static Token scan_identifier(Lexer *l)
{
  Source_Location lc = {l->ln, l->cl, l->f};
  const char *s = l->c;
  while (isalnum(peek(l, 0)) or peek(l, 0) == '_')
    av(l);
  String_Slice lt = {s, l->c - s};
  Token_Kind t = kl(lt);
  if (t != T_ID and l->c < l->e and (isalnum(*l->c) or *l->c == '_'))
    return mt(T_ERR, lc, STRING_LITERAL("kw+x"));
  return mt(t, lc, lt);
}
static Token scan_number_literal(Lexer *l)
{
  Source_Location lc = {l->ln, l->cl, l->f};
  const char *s = l->c;
  const char *ms = 0, *me = 0, *es = 0;
  int b = 10;
  bool ir = false, bx = false, has_dot = false, has_exp = false;
  char bd = 0;
  while (isdigit(peek(l, 0)) or peek(l, 0) == '_')
    av(l);
  if (peek(l, 0) == '#' or (peek(l, 0) == ':' and isxdigit(peek(l, 1))))
  {
    bd = peek(l, 0);
    const char *be = l->c;
    av(l);
    char *bp = arena_allocate(32);
    int bi = 0;
    for (const char *p = s; p < be; p++)
      if (*p != '_')
        bp[bi++] = *p;
    bp[bi] = 0;
    b = atoi(bp);
    ms = l->c;
    while (isxdigit(peek(l, 0)) or peek(l, 0) == '_')
      av(l);
    if (peek(l, 0) == '.')
    {
      ir = true;
      av(l);
      while (isxdigit(peek(l, 0)) or peek(l, 0) == '_')
        av(l);
    }
    if (peek(l, 0) == bd)
    {
      me = l->c;
      av(l);
    }
    if (tolower(peek(l, 0)) == 'e')
    {
      bx = true;
      av(l);
      if (peek(l, 0) == '+' or peek(l, 0) == '-')
        av(l);
      es = l->c;
      while (isdigit(peek(l, 0)) or peek(l, 0) == '_')
        av(l);
    }
  }
  else
  {
    if (peek(l, 0) == '.')
    {
      if (peek(l, 1) != '.' and not isalpha(peek(l, 1)))
      {
        ir = true;
        has_dot = true;
        av(l);
        while (isdigit(peek(l, 0)) or peek(l, 0) == '_')
          av(l);
      }
    }
    if (tolower(peek(l, 0)) == 'e')
    {
      has_exp = true;
      av(l);
      if (peek(l, 0) == '+' or peek(l, 0) == '-')
        av(l);
      while (isdigit(peek(l, 0)) or peek(l, 0) == '_')
        av(l);
    }
  }
  if (isalpha(peek(l, 0)))
    return mt(T_ERR, lc, STRING_LITERAL("num+alpha"));
  Token tk =
      mt(bx ? (ir ? T_REAL : T_INT) : (ir ? T_REAL : T_INT), lc, (String_Slice){s, l->c - s});
  if (bx and es)
  {
    char *mp = arena_allocate(512);
    char *ep = arena_allocate(512);
    int mi = 0, ei = 0;
    for (const char *p = ms; p < me; p++)
      if (*p != '_' and *p != bd)
        mp[mi++] = *p;
    mp[mi] = 0;
    for (const char *p = es; p < l->c; p++)
      if (*p != '_')
        ep[ei++] = *p;
    ep[ei] = 0;
    double m = 0;
    int dp = -1;
    for (int i = 0; i < mi; i++)
    {
      if (mp[i] == '.')
      {
        dp = i;
        break;
      }
    }
    int fp = 0;
    for (int i = 0; i < mi; i++)
    {
      if (mp[i] == '.')
        continue;
      int dv = mp[i] >= 'A' and mp[i] <= 'F'   ? mp[i] - 'A' + 10
               : mp[i] >= 'a' and mp[i] <= 'f' ? mp[i] - 'a' + 10
                                               : mp[i] - '0';
      if (dp < 0 or i < dp)
        m = m * b + dv;
      else
      {
        fp++;
        m += dv / pow(b, fp);
      }
    }
    int ex = atoi(ep);
    double rv = m * pow(b, ex);
    if (ir)
      tk.fv = rv;
    else
    {
      if (rv > LLONG_MAX or rv < LLONG_MIN)
      {
        fprintf(
            stderr,
            "Error %d:%d: based integer constant out of range: %.*s\n",
            lc.l,
            lc.c,
            (int) (l->c - s),
            s);
        exit(1);
      }
      tk.iv = (int64_t) rv;
    }
  }
  else
  {
    char *tp = arena_allocate(512);
    int j = 0;
    const char *start = (bd and ms) ? ms : s;
    const char *end = (bd and me) ? me : l->c;
    for (const char *p = start; p < end; p++)
      if (*p != '_' and *p != '#' and *p != ':')
        tp[j++] = *p;
    tp[j] = 0;
    if (bd and not ir)
    {
      int64_t v = 0;
      for (int i = 0; i < j; i++)
      {
        int dv = tp[i] >= 'A' and tp[i] <= 'F'   ? tp[i] - 'A' + 10
                 : tp[i] >= 'a' and tp[i] <= 'f' ? tp[i] - 'a' + 10
                                                 : tp[i] - '0';
        v = v * b + dv;
      }
      tk.iv = v;
    }
    else if (bd and ir)
    {
      double m = 0;
      int dp = -1;
      for (int i = 0; i < j; i++)
      {
        if (tp[i] == '.')
        {
          dp = i;
          break;
        }
      }
      int fp = 0;
      for (int i = 0; i < j; i++)
      {
        if (tp[i] == '.')
          continue;
        int dv = tp[i] >= 'A' and tp[i] <= 'F'   ? tp[i] - 'A' + 10
                 : tp[i] >= 'a' and tp[i] <= 'f' ? tp[i] - 'a' + 10
                                                 : tp[i] - '0';
        if (dp < 0 or i < dp)
          m = m * b + dv;
        else
        {
          fp++;
          m += dv / pow(b, fp);
        }
      }
      tk.fv = m;
    }
    else
    {
      errno = 0;
      tk.fv = strtod(tp, 0);
      if (errno == ERANGE and (tk.fv == HUGE_VAL or tk.fv == -HUGE_VAL))
      {
        fprintf(stderr, "Warning %d:%d: float constant overflow to infinity: %s\n", lc.l, lc.c, tp);
      }
      tk.ui = unsigned_bigint_from_decimal(tp);
      tk.iv = (tk.ui->count == 1) ? tk.ui->digits[0] : 0;
      if (has_exp and not has_dot and tk.fv >= LLONG_MIN and tk.fv <= LLONG_MAX
          and tk.fv == (double) (int64_t) tk.fv)
      {
        tk.iv = (int64_t) tk.fv;
        tk.t = T_INT;
      }
      else if (ir or has_dot)
      {
        tk.t = T_REAL;
      }
      else if (tk.ui and tk.ui->count > 1)
      {
        fprintf(stderr, "Error %d:%d: integer constant too large for i64: %s\n", lc.l, lc.c, tp);
      }
    }
  }
  return tk;
}
static Token scan_character_literal(Lexer *l)
{
  Source_Location lc = {l->ln, l->cl, l->f};
  av(l);
  if (not peek(l, 0))
    return mt(T_ERR, lc, STRING_LITERAL("uc"));
  char c = peek(l, 0);
  av(l);
  if (peek(l, 0) != '\'')
    return mt(T_ERR, lc, STRING_LITERAL("uc"));
  av(l);
  Token tk = mt(T_CHAR, lc, (String_Slice){&c, 1});
  tk.iv = c;
  return tk;
}
static Token scan_string_literal(Lexer *l)
{
  Source_Location lc = {l->ln, l->cl, l->f};
  char d = peek(l, 0);
  av(l);
  const char *s = l->c;
  (void) s;
  char *b = arena_allocate(256), *p = b;
  int n = 0;
  while (peek(l, 0))
  {
    if (peek(l, 0) == d)
    {
      if (peek(l, 1) == d)
      {
        av(l);
        av(l);
        if (n < 255)
          *p++ = d;
        n++;
      }
      else
        break;
    }
    else
    {
      if (n < 255)
        *p++ = peek(l, 0);
      n++;
      av(l);
    }
  }
  if (peek(l, 0) == d)
    av(l);
  else
    return mt(T_ERR, lc, STRING_LITERAL("us"));
  *p = 0;
  String_Slice lt = {b, n};
  return mt(T_STR, lc, lt);
}
static Token lexer_next_token(Lexer *l)
{
  const char *pb = l->c;
  sw(l);
  bool ws = l->c != pb;
  Source_Location lc = {l->ln, l->cl, l->f};
  char c = peek(l, 0);
  if (not c)
  {
    l->pt = T_EOF;
    return mt(T_EOF, lc, N);
  }
  if (isalpha(c))
  {
    Token tk = scan_identifier(l);
    l->pt = tk.t;
    return tk;
  }
  if (isdigit(c))
  {
    Token tk = scan_number_literal(l);
    l->pt = tk.t;
    return tk;
  }
  if (c == '\'')
  {
    char c2 = peek(l, 1);
    char pc = l->c > l->s ? l->c[-1] : 0;
    bool id_attr = l->pt == T_ID and not ws and isalnum(pc);
    if (c2 and peek(l, 2) == '\'' and (l->c + 3 >= l->e or l->c[3] != '\'') and not id_attr)
    {
      l->pt = T_CHAR;
      return scan_character_literal(l);
    }
    av(l);
    l->pt = T_TK;
    return mt(T_TK, lc, STRING_LITERAL("'"));
  }
  if (c == '"' or c == '%')
  {
    Token tk = scan_string_literal(l);
    l->pt = tk.t;
    return tk;
  }
  av(l);
  Token_Kind tt;
  switch (c)
  {
  case '(':
    tt = T_LP;
    break;
  case ')':
    tt = T_RP;
    break;
  case '[':
    tt = T_LB;
    break;
  case ']':
    tt = T_RB;
    break;
  case ',':
    tt = T_CM;
    break;
  case ';':
    tt = T_SC;
    break;
  case '&':
    tt = T_AM;
    break;
  case '|':
  case '!':
    tt = T_BR;
    break;
  case '+':
    tt = T_PL;
    break;
  case '-':
    tt = T_MN;
    break;
  case '/':
    if (peek(l, 0) == '=')
    {
      av(l);
      tt = T_NE;
    }
    else
      tt = T_SL;
    break;
  case '*':
    if (peek(l, 0) == '*')
    {
      av(l);
      tt = T_EX;
    }
    else
      tt = T_ST;
    break;
  case '=':
    if (peek(l, 0) == '>')
    {
      av(l);
      tt = T_AR;
    }
    else
      tt = T_EQ;
    break;
  case ':':
    if (peek(l, 0) == '=')
    {
      av(l);
      tt = T_AS;
    }
    else
      tt = T_CL;
    break;
  case '.':
    if (peek(l, 0) == '.')
    {
      av(l);
      tt = T_DD;
    }
    else
      tt = T_DT;
    break;
  case '<':
    if (peek(l, 0) == '=')
    {
      av(l);
      tt = T_LE;
    }
    else if (peek(l, 0) == '<')
    {
      av(l);
      tt = T_LL;
    }
    else if (peek(l, 0) == '>')
    {
      av(l);
      tt = T_BX;
    }
    else
      tt = T_LT;
    break;
  case '>':
    if (peek(l, 0) == '=')
    {
      av(l);
      tt = T_GE;
    }
    else if (peek(l, 0) == '>')
    {
      av(l);
      tt = T_GG;
    }
    else
      tt = T_GT;
    break;
  default:
    tt = T_ERR;
    break;
  }
  l->pt = tt;
  return mt(tt, lc, tt == T_ERR ? STRING_LITERAL("ux") : N);
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
} Nk;
typedef struct Type_Info Type_Info;
typedef struct Syntax_Node Syntax_Node;
typedef struct Symbol Symbol;
typedef struct RC RC;
typedef struct LU LU;
typedef struct GT GT;
typedef struct LE LE;
struct LE
{
  String_Slice nm;
  int bb;
};
typedef struct
{
  Syntax_Node **d;
  uint32_t n, c;
} Node_Vector;
typedef struct
{
  Symbol **d;
  uint32_t n, c;
} Symbol_Vector;
typedef struct
{
  RC **d;
  uint32_t n, c;
} RV;
typedef struct
{
  LU **d;
  uint32_t n, c;
} LV;
typedef struct
{
  GT **d;
  uint32_t n, c;
} GV;
typedef struct
{
  FILE **d;
  uint32_t n, c;
} FV;
typedef struct
{
  String_Slice *d;
  uint32_t n, c;
} SLV;
typedef struct
{
  LE **d;
  uint32_t n, c;
} LEV;
#define VECPUSH(vtype, etype, fname)                                                               \
  static void fname(vtype *v, etype e)                                                             \
  {                                                                                                \
    if (v->n >= v->c)                                                                              \
    {                                                                                              \
      v->c = v->c ? v->c << 1 : 8;                                                                 \
      v->d = realloc(v->d, v->c * sizeof(etype));                                                  \
    }                                                                                              \
    v->d[v->n++] = e;                                                                              \
  }
VECPUSH(Node_Vector, Syntax_Node *, nv)
VECPUSH(Symbol_Vector, Symbol *, sv)
VECPUSH(LV, LU *, lv)
VECPUSH(GV, GT *, gv)
VECPUSH(LEV, LE *, lev)
VECPUSH(FV, FILE *, fv)
VECPUSH(SLV, String_Slice, slv)
struct Syntax_Node
{
  Nk k;
  Source_Location l;
  Type_Info *ty;
  Symbol *sy;
  union
  {
    String_Slice s;
    int64_t i;
    double f;
    struct
    {
      Token_Kind op;
      Syntax_Node *l, *r;
    } bn;
    struct
    {
      Token_Kind op;
      Syntax_Node *x;
    } un;
    struct
    {
      Syntax_Node *p;
      String_Slice at;
      Node_Vector ar;
    } at;
    struct
    {
      Syntax_Node *nm, *ag;
    } ql;
    struct
    {
      Syntax_Node *fn;
      Node_Vector ar;
    } cl;
    struct
    {
      Syntax_Node *p;
      Node_Vector ix;
    } ix;
    struct
    {
      Syntax_Node *p;
      Syntax_Node *lo, *hi;
    } sl;
    struct
    {
      Syntax_Node *p;
      String_Slice se;
    } se;
    struct
    {
      Syntax_Node *st;
      Syntax_Node *in;
    } alc;
    struct
    {
      Syntax_Node *lo, *hi;
    } rn;
    struct
    {
      Syntax_Node *rn;
      Node_Vector cs;
    } cn;
    struct
    {
      String_Slice nm;
      Syntax_Node *ty;
      Syntax_Node *in;
      bool al;
      uint32_t of, bt;
      Syntax_Node *dc;
      Syntax_Node *dsc;
    } cm;
    struct
    {
      Node_Vector ch;
      Node_Vector cmm;
    } vr;
    struct
    {
      Syntax_Node *ds;
      Node_Vector vrr;
      uint32_t sz;
    } vp;
    struct
    {
      String_Slice nm;
      Syntax_Node *ty;
      Syntax_Node *df;
      uint8_t md;
    } pm;
    struct
    {
      String_Slice nm;
      Node_Vector pmm;
      Syntax_Node *rt;
      String_Slice op;
    } sp;
    struct
    {
      Syntax_Node *sp;
      Node_Vector dc;
      Node_Vector st;
      Node_Vector hdd;
      int el;
      Symbol *pr;
      Node_Vector lk;
    } bd;
    struct
    {
      String_Slice nm;
      Node_Vector dc;
      Node_Vector pr;
      int el;
    } ps;
    struct
    {
      String_Slice nm;
      Node_Vector dc;
      Node_Vector st;
      Node_Vector hddd;
      int el;
    } pb;
    struct
    {
      Node_Vector id;
      Syntax_Node *ty;
      Syntax_Node *in;
      bool co;
    } od;
    struct
    {
      String_Slice nm;
      Syntax_Node *df;
      Syntax_Node *ds;
      bool nw;
      bool drv;
      Syntax_Node *prt;
      Node_Vector dsc;
    } td;
    struct
    {
      String_Slice nm;
      Syntax_Node *in;
      Syntax_Node *cn;
      Syntax_Node *rn;
    } sd;
    struct
    {
      Node_Vector id;
      Syntax_Node *rn;
    } ed;
    struct
    {
      String_Slice nm;
      Syntax_Node *rn;
    } re;
    struct
    {
      Syntax_Node *tg;
      Syntax_Node *vl;
    } as;
    struct
    {
      Syntax_Node *cd;
      Node_Vector th;
      Node_Vector ei;
      Node_Vector el;
    } if_;
    struct
    {
      Syntax_Node *ex;
      Node_Vector al;
    } cs;
    struct
    {
      String_Slice lb;
      Syntax_Node *it;
      bool rv;
      Node_Vector st;
      Node_Vector lk;
    } lp;
    struct
    {
      String_Slice lb;
      Node_Vector dc;
      Node_Vector st;
      Node_Vector hd;
    } bk;
    struct
    {
      String_Slice lb;
      Syntax_Node *cd;
    } ex;
    struct
    {
      Syntax_Node *vl;
    } rt;
    struct
    {
      String_Slice lb;
    } go;
    struct
    {
      Syntax_Node *ec;
    } rs;
    struct
    {
      Syntax_Node *nm;
      Node_Vector arr;
    } ct;
    struct
    {
      String_Slice nm;
      Node_Vector ixx;
      Node_Vector pmx;
      Node_Vector stx;
      Node_Vector hdx;
      Syntax_Node *gd;
    } acc;
    struct
    {
      Node_Vector al;
      Node_Vector el;
    } ss;
    struct
    {
      uint8_t kn;
      Syntax_Node *gd;
      Node_Vector sts;
    } sa;
    struct
    {
      String_Slice nm;
      Node_Vector en;
      bool it;
    } ts;
    struct
    {
      String_Slice nm;
      Node_Vector dc;
      Node_Vector st;
      Node_Vector hdz;
    } tb;
    struct
    {
      String_Slice nm;
      Node_Vector ixy;
      Node_Vector pmy;
      Syntax_Node *gd;
    } ent;
    struct
    {
      Node_Vector ec;
      Node_Vector stz;
    } hnd;
    struct
    {
      Node_Vector it;
    } ch;
    struct
    {
      Node_Vector ch;
      Syntax_Node *vl;
    } asc;
    struct
    {
      Node_Vector it;
    } lst;
    struct
    {
      Node_Vector wt;
      Node_Vector us;
    } cx;
    struct
    {
      String_Slice nm;
    } wt;
    struct
    {
      Syntax_Node *nm;
    } us;
    struct
    {
      String_Slice nm;
      Node_Vector ar;
    } pg;
    struct
    {
      Syntax_Node *cx;
      Node_Vector un;
    } cu;
    struct
    {
      Syntax_Node *x;
    } drf;
    struct
    {
      Syntax_Node *ty, *ex;
    } cvt;
    struct
    {
      Syntax_Node *ex;
      String_Slice ec;
    } chk;
    struct
    {
      Syntax_Node *bs;
      Node_Vector ops;
    } drv;
    struct
    {
      Node_Vector fp;
      Node_Vector dc;
      Syntax_Node *un;
    } gen;
    struct
    {
      String_Slice nm;
      String_Slice gn;
      Node_Vector ap;
    } gi;
    struct
    {
      Node_Vector it;
      Syntax_Node *lo, *hi;
      uint8_t dim;
    } ag;
  };
};
struct RC
{
  uint8_t k;
  Type_Info *ty;
  union
  {
    struct
    {
      String_Slice nm;
      uint32_t po;
    } er;
    struct
    {
      String_Slice nm;
      uint64_t ad;
    } ad;
    struct
    {
      String_Slice nm;
      Node_Vector cp;
    } rr;
    struct
    {
      String_Slice lang;
      String_Slice nm;
      String_Slice ext;
    } im;
  };
};
struct LU
{
  uint8_t k;
  String_Slice nm;
  String_Slice pth;
  Syntax_Node *sp;
  Syntax_Node *bd;
  LV wth;
  LV elb;
  uint64_t ts;
  bool cmpl;
};
struct GT
{
  String_Slice nm;
  Node_Vector fp;
  Node_Vector dc;
  Syntax_Node *un;
  Syntax_Node *bd;
};
static Syntax_Node *node_new(Nk k, Source_Location l)
{
  Syntax_Node *n = arena_allocate(sizeof(Syntax_Node));
  n->k = k;
  n->l = l;
  return n;
}
static RC *reference_counter_new(uint8_t k, Type_Info *t)
{
  RC *r = arena_allocate(sizeof(RC));
  r->k = k;
  r->ty = t;
  return r;
}
static LU *label_use_new(uint8_t k, String_Slice nm, String_Slice pth)
{
  LU *l = arena_allocate(sizeof(LU));
  l->k = k;
  l->nm = nm;
  l->pth = pth;
  return l;
}
static GT *generic_type_new(String_Slice nm)
{
  GT *g = arena_allocate(sizeof(GT));
  g->nm = nm;
  return g;
}
#define ND(k, l) node_new(N_##k, l)
typedef struct
{
  Lexer lx;
  Token cr, pk;
  int er;
  SLV lb;
} Parser;
static void parser_next(Parser *p)
{
  p->cr = p->pk;
  p->pk = lexer_next_token(&p->lx);
  if (p->cr.t == T_AND and p->pk.t == T_THEN)
  {
    p->cr.t = T_ATHN;
    p->pk = lexer_next_token(&p->lx);
  }
  if (p->cr.t == T_OR and p->pk.t == T_ELSE)
  {
    p->cr.t = T_OREL;
    p->pk = lexer_next_token(&p->lx);
  }
}
static bool parser_at(Parser *p, Token_Kind t)
{
  return p->cr.t == t;
}
static bool parser_match(Parser *p, Token_Kind t)
{
  if (parser_at(p, t))
  {
    parser_next(p);
    return 1;
  }
  return 0;
}
static void parser_expect(Parser *p, Token_Kind t)
{
  if (not parser_match(p, t))
    fatal_error(p->cr.l, "exp '%s' got '%s'", TN[t], TN[p->cr.t]);
}
static Source_Location parser_location(Parser *p)
{
  return p->cr.l;
}
static String_Slice parser_identifier(Parser *p)
{
  String_Slice s = string_duplicate(p->cr.lit);
  parser_expect(p, T_ID);
  return s;
}
static String_Slice parser_attribute(Parser *p)
{
  String_Slice s;
  if (parser_at(p, T_ID))
    s = parser_identifier(p);
  else if (parser_at(p, T_RNG))
  {
    s = STRING_LITERAL("RANGE");
    parser_next(p);
  }
  else if (parser_at(p, T_ACCS))
  {
    s = STRING_LITERAL("ACCESS");
    parser_next(p);
  }
  else if (parser_at(p, T_DIG))
  {
    s = STRING_LITERAL("DIGITS");
    parser_next(p);
  }
  else if (parser_at(p, T_DELTA))
  {
    s = STRING_LITERAL("DELTA");
    parser_next(p);
  }
  else if (parser_at(p, T_MOD))
  {
    s = STRING_LITERAL("MOD");
    parser_next(p);
  }
  else if (parser_at(p, T_REM))
  {
    s = STRING_LITERAL("REM");
    parser_next(p);
  }
  else if (parser_at(p, T_ABS))
  {
    s = STRING_LITERAL("ABS");
    parser_next(p);
  }
  else if (parser_at(p, T_NOT))
  {
    s = STRING_LITERAL("NOT");
    parser_next(p);
  }
  else if (parser_at(p, T_AND))
  {
    s = STRING_LITERAL("AND");
    parser_next(p);
  }
  else if (parser_at(p, T_OR))
  {
    s = STRING_LITERAL("OR");
    parser_next(p);
  }
  else if (parser_at(p, T_XOR))
  {
    s = STRING_LITERAL("XOR");
    parser_next(p);
  }
  else if (parser_at(p, T_PL))
  {
    s = STRING_LITERAL("+");
    parser_next(p);
  }
  else if (parser_at(p, T_MN))
  {
    s = STRING_LITERAL("-");
    parser_next(p);
  }
  else if (parser_at(p, T_ST))
  {
    s = STRING_LITERAL("*");
    parser_next(p);
  }
  else if (parser_at(p, T_SL))
  {
    s = STRING_LITERAL("/");
    parser_next(p);
  }
  else if (parser_at(p, T_EQ))
  {
    s = STRING_LITERAL("=");
    parser_next(p);
  }
  else if (parser_at(p, T_NE))
  {
    s = STRING_LITERAL("/=");
    parser_next(p);
  }
  else if (parser_at(p, T_LT))
  {
    s = STRING_LITERAL("<");
    parser_next(p);
  }
  else if (parser_at(p, T_LE))
  {
    s = STRING_LITERAL("<=");
    parser_next(p);
  }
  else if (parser_at(p, T_GT))
  {
    s = STRING_LITERAL(">");
    parser_next(p);
  }
  else if (parser_at(p, T_GE))
  {
    s = STRING_LITERAL(">=");
    parser_next(p);
  }
  else if (parser_at(p, T_AM))
  {
    s = STRING_LITERAL("&");
    parser_next(p);
  }
  else if (parser_at(p, T_EX))
  {
    s = STRING_LITERAL("**");
    parser_next(p);
  }
  else
    fatal_error(parser_location(p), "exp attr");
  return s;
}
static Syntax_Node *parse_name(Parser *p);
static Syntax_Node *parse_expression(Parser *p);
static Syntax_Node *parse_primary(Parser *p);
static Syntax_Node *parse_range(Parser *p);
static Node_Vector parse_statement(Parser *p);
static Node_Vector pdc(Parser *p);
static Node_Vector phd(Parser *p);
static Syntax_Node *ps(Parser *p);
static Syntax_Node *pgf(Parser *p);
static RC *prc(Parser *p);
static Syntax_Node *parse_primary(Parser *p)
{
  Source_Location lc = parser_location(p);
  if (parser_match(p, T_LP))
  {
    Node_Vector v = {0};
    do
    {
      Node_Vector ch = {0};
      Syntax_Node *e = parse_expression(p);
      nv(&ch, e);
      while (parser_match(p, T_BR))
        nv(&ch, parse_expression(p));
      if (parser_at(p, T_AR))
      {
        parser_next(p);
        Syntax_Node *vl = parse_expression(p);
        for (uint32_t i = 0; i < ch.n; i++)
        {
          Syntax_Node *a = ND(ASC, lc);
          nv(&a->asc.ch, ch.d[i]);
          a->asc.vl = vl;
          nv(&v, a);
        }
      }
      else if (ch.n == 1 and ch.d[0]->k == N_ID and parser_match(p, T_RNG))
      {
        Syntax_Node *rng = parse_range(p);
        parser_expect(p, T_AR);
        Syntax_Node *vl = parse_expression(p);
        Syntax_Node *si = ND(ST, lc);
        Syntax_Node *cn = ND(CN, lc);
        cn->cn.rn = rng;
        si->sd.in = ch.d[0];
        si->sd.cn = cn;
        Syntax_Node *a = ND(ASC, lc);
        nv(&a->asc.ch, si);
        a->asc.vl = vl;
        nv(&v, a);
      }
      else
      {
        if (ch.n == 1)
          nv(&v, ch.d[0]);
        else
          fatal_error(lc, "exp '=>'");
      }
    } while (parser_match(p, T_CM));
    parser_expect(p, T_RP);
    if (v.n == 1 and v.d[0]->k != N_ASC)
      return v.d[0];
    Syntax_Node *n = ND(AG, lc);
    n->ag.it = v;
    return n;
  }
  if (parser_match(p, T_NEW))
  {
    Syntax_Node *n = ND(ALC, lc);
    n->alc.st = parse_name(p);
    if (parser_match(p, T_TK))
    {
      parser_expect(p, T_LP);
      n->alc.in = parse_expression(p);
      parser_expect(p, T_RP);
    }
    return n;
  }
  if (parser_match(p, T_NULL))
    return ND(NULL, lc);
  if (parser_match(p, T_OTH))
  {
    Syntax_Node *n = ND(ID, lc);
    n->s = STRING_LITERAL("others");
    return n;
  }
  if (parser_at(p, T_INT))
  {
    Syntax_Node *n = ND(INT, lc);
    n->i = p->cr.iv;
    parser_next(p);
    return n;
  }
  if (parser_at(p, T_REAL))
  {
    Syntax_Node *n = ND(REAL, lc);
    n->f = p->cr.fv;
    parser_next(p);
    return n;
  }
  if (parser_at(p, T_CHAR))
  {
    Syntax_Node *n = ND(CHAR, lc);
    n->i = p->cr.iv;
    parser_next(p);
    return n;
  }
  if (parser_at(p, T_STR))
  {
    Syntax_Node *n = ND(STR, lc);
    n->s = string_duplicate(p->cr.lit);
    parser_next(p);
    for (;;)
    {
      if (parser_at(p, T_LP))
      {
        parser_next(p);
        Node_Vector v = {0};
        do
        {
          Node_Vector ch = {0};
          Syntax_Node *e = parse_expression(p);
          if (e->k == N_ID and parser_at(p, T_AR))
          {
            parser_next(p);
            Syntax_Node *a = ND(ASC, lc);
            nv(&a->asc.ch, e);
            a->asc.vl = parse_expression(p);
            nv(&v, a);
          }
          else
          {
            nv(&ch, e);
            while (parser_match(p, T_BR))
              nv(&ch, parse_expression(p));
            if (parser_at(p, T_AR))
            {
              parser_next(p);
              Syntax_Node *vl = parse_expression(p);
              for (uint32_t i = 0; i < ch.n; i++)
              {
                Syntax_Node *a = ND(ASC, lc);
                nv(&a->asc.ch, ch.d[i]);
                a->asc.vl = vl;
                nv(&v, a);
              }
            }
            else
            {
              if (ch.n == 1)
                nv(&v, ch.d[0]);
              else
                fatal_error(lc, "exp '=>'");
            }
          }
        } while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
        Syntax_Node *m = ND(CL, lc);
        m->cl.fn = n;
        m->cl.ar = v;
        n = m;
      }
      else
        break;
    }
    return n;
  }
  if (parser_at(p, T_ID))
    return parse_name(p);
  if (parser_match(p, T_NOT))
  {
    Syntax_Node *n = ND(UN, lc);
    n->un.op = T_NOT;
    n->un.x = parse_primary(p);
    return n;
  }
  if (parser_match(p, T_ABS))
  {
    Syntax_Node *n = ND(UN, lc);
    n->un.op = T_ABS;
    n->un.x = parse_primary(p);
    return n;
  }
  if (parser_match(p, T_ALL))
  {
    Syntax_Node *n = ND(DRF, lc);
    n->drf.x = parse_primary(p);
    return n;
  }
  fatal_error(lc, "exp expr");
}
static Syntax_Node *parse_name(Parser *p)
{
  Source_Location lc = parser_location(p);
  Syntax_Node *n = ND(ID, lc);
  n->s = parser_identifier(p);
  for (;;)
  {
    if (parser_match(p, T_DT))
    {
      if (parser_match(p, T_ALL))
      {
        Syntax_Node *m = ND(DRF, lc);
        m->drf.x = n;
        n = m;
      }
      else
      {
        Syntax_Node *m = ND(SEL, lc);
        m->se.p = n;
        if (parser_at(p, T_STR))
        {
          m->se.se = string_duplicate(p->cr.lit);
          parser_next(p);
        }
        else if (parser_at(p, T_CHAR))
        {
          char *c = arena_allocate(2);
          c[0] = p->cr.iv;
          c[1] = 0;
          m->se.se = (String_Slice){c, 1};
          parser_next(p);
        }
        else
          m->se.se = parser_identifier(p);
        n = m;
      }
    }
    else if (parser_match(p, T_TK))
    {
      if (parser_at(p, T_LP))
      {
        parser_next(p);
        Syntax_Node *m = ND(QL, lc);
        m->ql.nm = n;
        Node_Vector v = {0};
        do
        {
          Node_Vector ch = {0};
          Syntax_Node *e = parse_expression(p);
          nv(&ch, e);
          while (parser_match(p, T_BR))
            nv(&ch, parse_expression(p));
          if (parser_at(p, T_AR))
          {
            parser_next(p);
            Syntax_Node *vl = parse_expression(p);
            for (uint32_t i = 0; i < ch.n; i++)
            {
              Syntax_Node *a = ND(ASC, lc);
              nv(&a->asc.ch, ch.d[i]);
              a->asc.vl = vl;
              nv(&v, a);
            }
          }
          else
          {
            if (ch.n == 1)
              nv(&v, ch.d[0]);
            else
              fatal_error(lc, "exp '=>'");
          }
        } while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
        if (v.n == 1 and v.d[0]->k != N_ASC)
          m->ql.ag = v.d[0];
        else
        {
          Syntax_Node *ag = ND(AG, lc);
          ag->ag.it = v;
          m->ql.ag = ag;
        }
        n = m;
      }
      else
      {
        String_Slice at = parser_attribute(p);
        Syntax_Node *m = ND(AT, lc);
        m->at.p = n;
        m->at.at = at;
        if (parser_match(p, T_LP))
        {
          do
            nv(&m->at.ar, parse_expression(p));
          while (parser_match(p, T_CM));
          parser_expect(p, T_RP);
        }
        n = m;
      }
    }
    else if (parser_at(p, T_LP))
    {
      parser_next(p);
      if (parser_at(p, T_RP))
      {
        parser_expect(p, T_RP);
        Syntax_Node *m = ND(CL, lc);
        m->cl.fn = n;
        n = m;
      }
      else
      {
        Node_Vector v = {0};
        do
        {
          Node_Vector ch = {0};
          Syntax_Node *e = parse_expression(p);
          if (e->k == N_ID and parser_at(p, T_AR))
          {
            parser_next(p);
            Syntax_Node *a = ND(ASC, lc);
            nv(&a->asc.ch, e);
            a->asc.vl = parse_expression(p);
            nv(&v, a);
          }
          else
          {
            nv(&ch, e);
            while (parser_match(p, T_BR))
              nv(&ch, parse_expression(p));
            if (parser_at(p, T_AR))
            {
              parser_next(p);
              Syntax_Node *vl = parse_expression(p);
              for (uint32_t i = 0; i < ch.n; i++)
              {
                Syntax_Node *a = ND(ASC, lc);
                nv(&a->asc.ch, ch.d[i]);
                a->asc.vl = vl;
                nv(&v, a);
              }
            }
            else
            {
              if (ch.n == 1)
                nv(&v, ch.d[0]);
              else
                fatal_error(lc, "exp '=>'");
            }
          }
        } while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
        Syntax_Node *m = ND(CL, lc);
        m->cl.fn = n;
        m->cl.ar = v;
        n = m;
      }
    }
    else
      break;
  }
  return n;
}
static Syntax_Node *ppw(Parser *p)
{
  Syntax_Node *n = parse_primary(p);
  if (parser_match(p, T_EX))
  {
    Source_Location lc = parser_location(p);
    Syntax_Node *m = ND(BIN, lc);
    m->bn.op = T_EX;
    m->bn.l = n;
    m->bn.r = ppw(p);
    return m;
  }
  return n;
}
static Syntax_Node *pt(Parser *p)
{
  Syntax_Node *n = ppw(p);
  while (parser_at(p, T_ST) or parser_at(p, T_SL) or parser_at(p, T_MOD) or parser_at(p, T_REM))
  {
    Token_Kind op = p->cr.t;
    parser_next(p);
    Source_Location lc = parser_location(p);
    Syntax_Node *m = ND(BIN, lc);
    m->bn.op = op;
    m->bn.l = n;
    m->bn.r = ppw(p);
    n = m;
  }
  return n;
}
static Syntax_Node *psm(Parser *p)
{
  Source_Location lc = parser_location(p);
  Token_Kind un = 0;
  if (parser_match(p, T_MN))
    un = T_MN;
  else if (parser_match(p, T_PL))
    un = T_PL;
  Syntax_Node *n = pt(p);
  if (un)
  {
    Syntax_Node *m = ND(UN, lc);
    m->un.op = un;
    m->un.x = n;
    n = m;
  }
  while (parser_at(p, T_PL) or parser_at(p, T_MN) or parser_at(p, T_AM))
  {
    Token_Kind op = p->cr.t;
    parser_next(p);
    lc = parser_location(p);
    Syntax_Node *m = ND(BIN, lc);
    m->bn.op = op;
    m->bn.l = n;
    m->bn.r = pt(p);
    n = m;
  }
  return n;
}
static Syntax_Node *prl(Parser *p)
{
  Syntax_Node *n = psm(p);
  if (parser_match(p, T_DD))
  {
    Source_Location lc = parser_location(p);
    Syntax_Node *m = ND(RN, lc);
    m->rn.lo = n;
    m->rn.hi = psm(p);
    return m;
  }
  if (parser_at(p, T_EQ) or parser_at(p, T_NE) or parser_at(p, T_LT) or parser_at(p, T_LE)
      or parser_at(p, T_GT) or parser_at(p, T_GE) or parser_at(p, T_IN) or parser_at(p, T_NOT))
  {
    Token_Kind op = p->cr.t;
    parser_next(p);
    if (op == T_NOT)
      parser_expect(p, T_IN);
    Source_Location lc = parser_location(p);
    Syntax_Node *m = ND(BIN, lc);
    m->bn.op = op;
    m->bn.l = n;
    if (op == T_IN or op == T_NOT)
      m->bn.r = parse_range(p);
    else
      m->bn.r = psm(p);
    return m;
  }
  return n;
}
static Syntax_Node *pan(Parser *p)
{
  Syntax_Node *n = prl(p);
  while (parser_at(p, T_AND) or parser_at(p, T_ATHN))
  {
    Token_Kind op = p->cr.t;
    parser_next(p);
    Source_Location lc = parser_location(p);
    Syntax_Node *m = ND(BIN, lc);
    m->bn.op = op;
    m->bn.l = n;
    m->bn.r = prl(p);
    n = m;
  }
  return n;
}
static Syntax_Node *por(Parser *p)
{
  Syntax_Node *n = pan(p);
  while (parser_at(p, T_OR) or parser_at(p, T_OREL) or parser_at(p, T_XOR))
  {
    Token_Kind op = p->cr.t;
    parser_next(p);
    Source_Location lc = parser_location(p);
    Syntax_Node *m = ND(BIN, lc);
    m->bn.op = op;
    m->bn.l = n;
    m->bn.r = pan(p);
    n = m;
  }
  return n;
}
static Syntax_Node *parse_expression(Parser *p)
{
  return por(p);
}
static Syntax_Node *parse_range(Parser *p)
{
  Source_Location lc = parser_location(p);
  if (parser_match(p, T_BX))
  {
    Syntax_Node *n = ND(RN, lc);
    n->rn.lo = 0;
    n->rn.hi = 0;
    return n;
  }
  Syntax_Node *lo = psm(p);
  if (parser_match(p, T_DD))
  {
    Syntax_Node *m = ND(RN, lc);
    m->rn.lo = lo;
    m->rn.hi = psm(p);
    return m;
  }
  return lo;
}
static Syntax_Node *psi(Parser *p)
{
  Source_Location lc = parser_location(p);
  Syntax_Node *n = ND(ID, lc);
  n->s = parser_identifier(p);
  for (;;)
  {
    if (parser_match(p, T_DT))
    {
      if (parser_match(p, T_ALL))
      {
        Syntax_Node *m = ND(DRF, lc);
        m->drf.x = n;
        n = m;
      }
      else
      {
        Syntax_Node *m = ND(SEL, lc);
        m->se.p = n;
        m->se.se = parser_identifier(p);
        n = m;
      }
    }
    else if (parser_match(p, T_TK))
    {
      String_Slice at = parser_attribute(p);
      Syntax_Node *m = ND(AT, lc);
      m->at.p = n;
      m->at.at = at;
      if (parser_match(p, T_LP))
      {
        do
          nv(&m->at.ar, parse_expression(p));
        while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
      }
      n = m;
    }
    else
      break;
  }
  if (parser_match(p, T_DELTA))
  {
    psm(p);
  }
  if (parser_match(p, T_DIG))
  {
    parse_expression(p);
  }
  if (parser_match(p, T_RNG))
  {
    Source_Location lc = parser_location(p);
    Syntax_Node *c = ND(CN, lc);
    c->cn.rn = parse_range(p);
    Syntax_Node *m = ND(ST, lc);
    m->sd.in = n;
    m->sd.cn = c;
    return m;
  }
  if (parser_at(p, T_LP))
  {
    parser_next(p);
    Source_Location lc = parser_location(p);
    Syntax_Node *c = ND(CN, lc);
    do
    {
      Source_Location lc2 = parser_location(p);
      Node_Vector ch = {0};
      Syntax_Node *r = parse_range(p);
      nv(&ch, r);
      while (parser_match(p, T_BR))
        nv(&ch, parse_range(p));
      if (ch.n > 0 and ch.d[0]->k == N_ID and parser_match(p, T_RNG))
      {
        Syntax_Node *tn = ND(ID, lc2);
        tn->s = ch.d[0]->s;
        Syntax_Node *rng = parse_range(p);
        Syntax_Node *si = ND(ST, lc2);
        Syntax_Node *cn = ND(CN, lc2);
        cn->cn.rn = rng;
        si->sd.in = tn;
        si->sd.cn = cn;
        nv(&c->cn.cs, si);
      }
      else if (ch.n > 0 and ch.d[0]->k == N_ID and parser_match(p, T_AR))
      {
        Syntax_Node *vl = parse_expression(p);
        for (uint32_t i = 0; i < ch.n; i++)
        {
          Syntax_Node *a = ND(ASC, lc2);
          nv(&a->asc.ch, ch.d[i]);
          a->asc.vl = vl;
          nv(&c->cn.cs, a);
        }
      }
      else
      {
        if (ch.n > 0)
          nv(&c->cn.cs, ch.d[0]);
      }
    } while (parser_match(p, T_CM));
    parser_expect(p, T_RP);
    Syntax_Node *m = ND(ST, lc);
    m->sd.in = n;
    m->sd.cn = c;
    return m;
  }
  return n;
}
static Node_Vector ppm(Parser *p)
{
  Node_Vector v = {0};
  if (not parser_match(p, T_LP))
    return v;
  do
  {
    Source_Location lc = parser_location(p);
    Node_Vector id = {0};
    do
    {
      String_Slice nm = parser_identifier(p);
      Syntax_Node *i = ND(ID, lc);
      i->s = nm;
      nv(&id, i);
    } while (parser_match(p, T_CM));
    parser_expect(p, T_CL);
    uint8_t md = 0;
    if (parser_match(p, T_IN))
      md |= 1;
    if (parser_match(p, T_OUT))
      md |= 2;
    if (not md)
      md = 1;
    Syntax_Node *ty = parse_name(p);
    Syntax_Node *df = 0;
    if (parser_match(p, T_AS))
      df = parse_expression(p);
    for (uint32_t i = 0; i < id.n; i++)
    {
      Syntax_Node *n = ND(PM, lc);
      n->pm.nm = id.d[i]->s;
      n->pm.ty = ty;
      n->pm.df = df;
      n->pm.md = md;
      nv(&v, n);
    }
  } while (parser_match(p, T_SC));
  parser_expect(p, T_RP);
  return v;
}
static Syntax_Node *pps_(Parser *p)
{
  Source_Location lc = parser_location(p);
  parser_expect(p, T_PROC);
  Syntax_Node *n = ND(PS, lc);
  if (parser_at(p, T_STR))
  {
    n->sp.nm = string_duplicate(p->cr.lit);
    parser_next(p);
  }
  else
    n->sp.nm = parser_identifier(p);
  n->sp.pmm = ppm(p);
  return n;
}
static Syntax_Node *pfs(Parser *p)
{
  Source_Location lc = parser_location(p);
  parser_expect(p, T_FUN);
  Syntax_Node *n = ND(FS, lc);
  if (parser_at(p, T_STR))
  {
    n->sp.nm = string_duplicate(p->cr.lit);
    parser_next(p);
  }
  else
    n->sp.nm = parser_identifier(p);
  n->sp.pmm = ppm(p);
  parser_expect(p, T_RET);
  n->sp.rt = parse_name(p);
  return n;
}
static Syntax_Node *ptd(Parser *p);
static Node_Vector pgfp(Parser *p)
{
  Node_Vector v = {0};
  while (not parser_at(p, T_PROC) and not parser_at(p, T_FUN) and not parser_at(p, T_PKG))
  {
    if (parser_match(p, T_TYP))
    {
      Source_Location lc = parser_location(p);
      String_Slice nm = parser_identifier(p);
      bool isp = false;
      if (parser_match(p, T_LP))
      {
        while (not parser_at(p, T_RP))
          parser_next(p);
        parser_expect(p, T_RP);
      }
      if (parser_match(p, T_IS))
      {
        if (parser_match(p, T_DIG) or parser_match(p, T_DELTA) or parser_match(p, T_RNG))
        {
          parser_expect(p, T_BX);
        }
        else if (parser_match(p, T_LP))
        {
          parser_expect(p, T_BX);
          parser_expect(p, T_RP);
          isp = true;
          (void) isp;
        }
        else if (
            parser_match(p, T_LIM) or parser_at(p, T_ARR) or parser_at(p, T_REC)
            or parser_at(p, T_ACCS) or parser_at(p, T_PRV))
        {
          ptd(p);
        }
        else
          parse_expression(p);
      }
      Syntax_Node *n = ND(GTP, lc);
      n->td.nm = nm;
      nv(&v, n);
      parser_expect(p, T_SC);
    }
    else if (parser_match(p, T_WITH))
    {
      if (parser_at(p, T_PROC))
      {
        Syntax_Node *sp = pps_(p);
        sp->k = N_GSP;
        if (parser_match(p, T_IS))
        {
          if (not parser_match(p, T_BX))
          {
            while (not parser_at(p, T_SC))
              parser_next(p);
          }
        }
        nv(&v, sp);
      }
      else if (parser_at(p, T_FUN))
      {
        Syntax_Node *sp = pfs(p);
        sp->k = N_GSP;
        if (parser_match(p, T_IS))
        {
          if (not parser_match(p, T_BX))
          {
            while (not parser_at(p, T_SC))
              parser_next(p);
          }
        }
        nv(&v, sp);
      }
      else
      {
        Source_Location lc = parser_location(p);
        Node_Vector id = {0};
        do
        {
          String_Slice nm = parser_identifier(p);
          Syntax_Node *i = ND(ID, lc);
          i->s = nm;
          nv(&id, i);
        } while (parser_match(p, T_CM));
        parser_expect(p, T_CL);
        uint8_t md = 0;
        if (parser_match(p, T_IN))
          md |= 1;
        if (parser_match(p, T_OUT))
          md |= 2;
        if (not md)
          md = 1;
        Syntax_Node *ty = parse_name(p);
        parser_match(p, T_AS);
        if (not parser_at(p, T_SC))
          parse_expression(p);
        Syntax_Node *n = ND(GVL, lc);
        n->od.id = id;
        n->od.ty = ty;
        nv(&v, n);
      }
      parser_expect(p, T_SC);
    }
    else
    {
      Source_Location lc = parser_location(p);
      Node_Vector id = {0};
      do
      {
        String_Slice nm = parser_identifier(p);
        Syntax_Node *i = ND(ID, lc);
        i->s = nm;
        nv(&id, i);
      } while (parser_match(p, T_CM));
      parser_expect(p, T_CL);
      uint8_t md = 0;
      if (parser_match(p, T_IN))
        md |= 1;
      if (parser_match(p, T_OUT))
        md |= 2;
      if (not md)
        md = 1;
      Syntax_Node *ty = parse_name(p);
      parser_match(p, T_AS);
      if (not parser_at(p, T_SC))
        parse_expression(p);
      Syntax_Node *n = ND(GVL, lc);
      n->od.id = id;
      n->od.ty = ty;
      nv(&v, n);
      parser_expect(p, T_SC);
    }
  }
  return v;
}
static Syntax_Node *pgf(Parser *p)
{
  Source_Location lc = parser_location(p);
  parser_expect(p, T_GEN);
  Syntax_Node *n = ND(GEN, lc);
  n->gen.fp = pgfp(p);
  if (parser_at(p, T_PROC))
  {
    Syntax_Node *sp = pps_(p);
    parser_expect(p, T_SC);
    n->gen.un = ND(PD, lc);
    n->gen.un->bd.sp = sp;
    return n;
  }
  if (parser_at(p, T_FUN))
  {
    Syntax_Node *sp = pfs(p);
    parser_expect(p, T_SC);
    n->gen.un = ND(FD, lc);
    n->gen.un->bd.sp = sp;
    return n;
  }
  if (parser_match(p, T_PKG))
  {
    String_Slice nm = parser_identifier(p);
    parser_expect(p, T_IS);
    Node_Vector dc = pdc(p);
    if (parser_match(p, T_PRV))
    {
      Node_Vector pr = pdc(p);
      for (uint32_t i = 0; i < pr.n; i++)
        nv(&dc, pr.d[i]);
    }
    n->gen.dc = dc;
    parser_expect(p, T_END);
    if (parser_at(p, T_ID))
      parser_next(p);
    parser_expect(p, T_SC);
    Syntax_Node *pk = ND(PKS, lc);
    pk->ps.nm = nm;
    pk->ps.dc = n->gen.dc;
    n->gen.un = pk;
    return n;
  }
  return n;
}
static Syntax_Node *pif(Parser *p)
{
  Source_Location lc = parser_location(p);
  parser_expect(p, T_IF);
  Syntax_Node *n = ND(IF, lc);
  n->if_.cd = parse_expression(p);
  parser_expect(p, T_THEN);
  while (not parser_at(p, T_ELSIF) and not parser_at(p, T_ELSE) and not parser_at(p, T_END))
    nv(&n->if_.th, ps(p));
  while (parser_match(p, T_ELSIF))
  {
    Syntax_Node *e = ND(EL, lc);
    e->if_.cd = parse_expression(p);
    parser_expect(p, T_THEN);
    while (not parser_at(p, T_ELSIF) and not parser_at(p, T_ELSE) and not parser_at(p, T_END))
      nv(&e->if_.th, ps(p));
    nv(&n->if_.ei, e);
  }
  if (parser_match(p, T_ELSE))
    while (not parser_at(p, T_END))
      nv(&n->if_.el, ps(p));
  parser_expect(p, T_END);
  parser_expect(p, T_IF);
  parser_expect(p, T_SC);
  return n;
}
static Syntax_Node *pcs(Parser *p)
{
  Source_Location lc = parser_location(p);
  parser_expect(p, T_CSE);
  Syntax_Node *n = ND(CS, lc);
  n->cs.ex = parse_expression(p);
  parser_expect(p, T_IS);
  while (parser_at(p, T_PGM))
    prc(p);
  while (parser_match(p, T_WHN))
  {
    Syntax_Node *a = ND(WH, lc);
    do
    {
      Syntax_Node *e = parse_expression(p);
      if (e->k == N_ID and parser_match(p, T_RNG))
      {
        Syntax_Node *r = parse_range(p);
        nv(&a->ch.it, r);
      }
      else if (parser_match(p, T_DD))
      {
        Syntax_Node *r = ND(RN, lc);
        r->rn.lo = e;
        r->rn.hi = parse_expression(p);
        nv(&a->ch.it, r);
      }
      else
        nv(&a->ch.it, e);
    } while (parser_match(p, T_BR));
    parser_expect(p, T_AR);
    while (not parser_at(p, T_WHN) and not parser_at(p, T_END))
      nv(&a->hnd.stz, ps(p));
    nv(&n->cs.al, a);
  }
  parser_expect(p, T_END);
  parser_expect(p, T_CSE);
  parser_expect(p, T_SC);
  return n;
}
static Syntax_Node *plp(Parser *p, String_Slice lb)
{
  Source_Location lc = parser_location(p);
  Syntax_Node *n = ND(LP, lc);
  n->lp.lb = lb;
  if (parser_match(p, T_WHI))
    n->lp.it = parse_expression(p);
  else if (parser_match(p, T_FOR))
  {
    String_Slice vr = parser_identifier(p);
    parser_expect(p, T_IN);
    n->lp.rv = parser_match(p, T_REV);
    Syntax_Node *rng = parse_range(p);
    if (parser_match(p, T_RNG))
    {
      Syntax_Node *r = ND(RN, lc);
      r->rn.lo = psm(p);
      parser_expect(p, T_DD);
      r->rn.hi = psm(p);
      rng = r;
    }
    Syntax_Node *it = ND(BIN, lc);
    it->bn.op = T_IN;
    it->bn.l = ND(ID, lc);
    it->bn.l->s = vr;
    it->bn.r = rng;
    n->lp.it = it;
  }
  parser_expect(p, T_LOOP);
  while (not parser_at(p, T_END))
    nv(&n->lp.st, ps(p));
  parser_expect(p, T_END);
  parser_expect(p, T_LOOP);
  if (parser_at(p, T_ID))
    parser_next(p);
  parser_expect(p, T_SC);
  return n;
}
static Syntax_Node *pbk(Parser *p, String_Slice lb)
{
  Source_Location lc = parser_location(p);
  Syntax_Node *n = ND(BL, lc);
  n->bk.lb = lb;
  if (parser_match(p, T_DEC))
    n->bk.dc = pdc(p);
  parser_expect(p, T_BEG);
  while (not parser_at(p, T_EXCP) and not parser_at(p, T_END))
    nv(&n->bk.st, ps(p));
  if (parser_match(p, T_EXCP))
    n->bk.hd = phd(p);
  parser_expect(p, T_END);
  if (parser_at(p, T_ID))
    parser_next(p);
  parser_expect(p, T_SC);
  return n;
}
static Syntax_Node *psl(Parser *p)
{
  Source_Location lc = parser_location(p);
  parser_expect(p, T_SEL);
  Syntax_Node *n = ND(SA, lc);
  n->sa.kn = 0;
  if (parser_match(p, T_DEL))
  {
    n->sa.kn = 1;
    n->sa.gd = parse_expression(p);
    parser_expect(p, T_THEN);
    if (parser_match(p, T_AB))
      n->sa.kn = 3;
    while (not parser_at(p, T_OR) and not parser_at(p, T_ELSE) and not parser_at(p, T_END))
      nv(&n->sa.sts, ps(p));
  }
  else if (parser_at(p, T_WHN))
  {
    while (parser_match(p, T_WHN))
    {
      Syntax_Node *g = ND(WH, lc);
      do
        nv(&g->ch.it, parse_expression(p));
      while (parser_match(p, T_BR));
      parser_expect(p, T_AR);
      if (parser_match(p, T_ACC))
      {
        g->k = N_ACC;
        g->acc.nm = parser_identifier(p);
        if (parser_at(p, T_LP))
        {
          if (p->pk.t == T_ID)
          {
            Token scr = p->cr, spk = p->pk;
            Lexer slx = p->lx;
            parser_next(p);
            parser_next(p);
            if (p->cr.t == T_CM or p->cr.t == T_CL)
            {
              p->cr = scr;
              p->pk = spk;
              p->lx = slx;
              g->acc.pmx = ppm(p);
            }
            else
            {
              p->cr = scr;
              p->pk = spk;
              p->lx = slx;
              parser_expect(p, T_LP);
              do
                nv(&g->acc.ixx, parse_expression(p));
              while (parser_match(p, T_CM));
              parser_expect(p, T_RP);
              g->acc.pmx = ppm(p);
            }
          }
          else
          {
            parser_expect(p, T_LP);
            do
              nv(&g->acc.ixx, parse_expression(p));
            while (parser_match(p, T_CM));
            parser_expect(p, T_RP);
            g->acc.pmx = ppm(p);
          }
        }
        else
        {
          g->acc.pmx = ppm(p);
        }
        if (parser_match(p, T_DO))
        {
          while (not parser_at(p, T_END) and not parser_at(p, T_OR) and not parser_at(p, T_ELSE))
            nv(&g->acc.stx, ps(p));
          parser_expect(p, T_END);
          if (parser_at(p, T_ID))
            parser_next(p);
        }
        while (not parser_at(p, T_OR) and not parser_at(p, T_ELSE) and not parser_at(p, T_END)
               and not parser_at(p, T_WHN))
          nv(&g->hnd.stz, ps(p));
      }
      else if (parser_match(p, T_TER))
      {
        g->k = N_TRM;
      }
      else if (parser_match(p, T_DEL))
      {
        g->k = N_DL;
        g->ex.cd = parse_expression(p);
        parser_expect(p, T_THEN);
        while (not parser_at(p, T_OR) and not parser_at(p, T_ELSE) and not parser_at(p, T_END))
          nv(&g->hnd.stz, ps(p));
      }
      nv(&n->sa.sts, g);
    }
  }
  else
  {
    do
    {
      Syntax_Node *g = ND(WH, lc);
      if (parser_match(p, T_ACC))
      {
        g->k = N_ACC;
        g->acc.nm = parser_identifier(p);
        if (parser_at(p, T_LP))
        {
          if (p->pk.t == T_ID)
          {
            Token scr = p->cr, spk = p->pk;
            Lexer slx = p->lx;
            parser_next(p);
            parser_next(p);
            if (p->cr.t == T_CM or p->cr.t == T_CL)
            {
              p->cr = scr;
              p->pk = spk;
              p->lx = slx;
              g->acc.pmx = ppm(p);
            }
            else
            {
              p->cr = scr;
              p->pk = spk;
              p->lx = slx;
              parser_expect(p, T_LP);
              do
                nv(&g->acc.ixx, parse_expression(p));
              while (parser_match(p, T_CM));
              parser_expect(p, T_RP);
              g->acc.pmx = ppm(p);
            }
          }
          else
          {
            parser_expect(p, T_LP);
            do
              nv(&g->acc.ixx, parse_expression(p));
            while (parser_match(p, T_CM));
            parser_expect(p, T_RP);
            g->acc.pmx = ppm(p);
          }
        }
        else
        {
          g->acc.pmx = ppm(p);
        }
        if (parser_match(p, T_DO))
        {
          while (not parser_at(p, T_END) and not parser_at(p, T_OR) and not parser_at(p, T_ELSE))
            nv(&g->acc.stx, ps(p));
          parser_expect(p, T_END);
          if (parser_at(p, T_ID))
            parser_next(p);
        }
        parser_expect(p, T_SC);
        while (not parser_at(p, T_OR) and not parser_at(p, T_ELSE) and not parser_at(p, T_END))
          nv(&g->hnd.stz, ps(p));
      }
      else if (parser_match(p, T_DEL))
      {
        g->k = N_DL;
        g->ex.cd = parse_expression(p);
        parser_expect(p, T_SC);
      }
      else if (parser_match(p, T_TER))
      {
        g->k = N_TRM;
        parser_expect(p, T_SC);
      }
      else
      {
        while (not parser_at(p, T_OR) and not parser_at(p, T_ELSE) and not parser_at(p, T_END))
          nv(&g->hnd.stz, ps(p));
      }
      nv(&n->sa.sts, g);
    } while (parser_match(p, T_OR));
  }
  if (parser_match(p, T_ELSE))
    while (not parser_at(p, T_END))
      nv(&n->ss.el, ps(p));
  parser_expect(p, T_END);
  parser_expect(p, T_SEL);
  parser_expect(p, T_SC);
  return n;
}
static Syntax_Node *ps(Parser *p)
{
  Source_Location lc = parser_location(p);
  String_Slice lb = N;
  while (parser_at(p, T_LL))
  {
    parser_next(p);
    lb = parser_identifier(p);
    parser_expect(p, T_GG);
    slv(&p->lb, lb);
  }
  if (not lb.s and parser_at(p, T_ID) and p->pk.t == T_CL)
  {
    lb = parser_identifier(p);
    parser_expect(p, T_CL);
    slv(&p->lb, lb);
  }
  if (parser_at(p, T_IF))
    return pif(p);
  if (parser_at(p, T_CSE))
    return pcs(p);
  if (parser_at(p, T_SEL))
    return psl(p);
  if (parser_at(p, T_LOOP) or parser_at(p, T_WHI) or parser_at(p, T_FOR))
    return plp(p, lb);
  if (parser_at(p, T_DEC) or parser_at(p, T_BEG))
    return pbk(p, lb);
  if (lb.s)
  {
    Syntax_Node *bl = ND(BL, lc);
    bl->bk.lb = lb;
    Node_Vector st = {0};
    nv(&st, ps(p));
    bl->bk.st = st;
    return bl;
  }
  if (parser_match(p, T_ACC))
  {
    Syntax_Node *n = ND(ACC, lc);
    n->acc.nm = parser_identifier(p);
    if (parser_at(p, T_LP))
    {
      if (p->pk.t == T_ID)
      {
        Token scr = p->cr, spk = p->pk;
        Lexer slx = p->lx;
        parser_next(p);
        parser_next(p);
        if (p->cr.t == T_CM or p->cr.t == T_CL)
        {
          p->cr = scr;
          p->pk = spk;
          p->lx = slx;
          n->acc.pmx = ppm(p);
        }
        else
        {
          p->cr = scr;
          p->pk = spk;
          p->lx = slx;
          parser_expect(p, T_LP);
          do
            nv(&n->acc.ixx, parse_expression(p));
          while (parser_match(p, T_CM));
          parser_expect(p, T_RP);
          n->acc.pmx = ppm(p);
        }
      }
      else
      {
        parser_expect(p, T_LP);
        do
          nv(&n->acc.ixx, parse_expression(p));
        while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
        n->acc.pmx = ppm(p);
      }
    }
    else
    {
      n->acc.pmx = ppm(p);
    }
    if (parser_match(p, T_DO))
    {
      while (not parser_at(p, T_END))
        nv(&n->acc.stx, ps(p));
      parser_expect(p, T_END);
      if (parser_at(p, T_ID))
        parser_next(p);
    }
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_DEL))
  {
    Syntax_Node *n = ND(DL, lc);
    n->ex.cd = parse_expression(p);
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_AB))
  {
    Syntax_Node *n = ND(AB, lc);
    if (not parser_at(p, T_SC))
      n->rs.ec = parse_name(p);
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_RET))
  {
    Syntax_Node *n = ND(RT, lc);
    if (not parser_at(p, T_SC))
      n->rt.vl = parse_expression(p);
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_EXIT))
  {
    Syntax_Node *n = ND(EX, lc);
    if (parser_at(p, T_ID))
      n->ex.lb = parser_identifier(p);
    if (parser_match(p, T_WHN))
      n->ex.cd = parse_expression(p);
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_GOTO))
  {
    Syntax_Node *n = ND(GT, lc);
    n->go.lb = parser_identifier(p);
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_RAS))
  {
    Syntax_Node *n = ND(RS, lc);
    if (not parser_at(p, T_SC))
      n->rs.ec = parse_name(p);
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_NULL))
  {
    parser_expect(p, T_SC);
    return ND(NS, lc);
  }
  if (parser_match(p, T_PGM))
  {
    Syntax_Node *n = ND(PG, lc);
    n->pg.nm = parser_identifier(p);
    if (parser_match(p, T_LP))
    {
      do
        nv(&n->pg.ar, parse_expression(p));
      while (parser_match(p, T_CM));
      parser_expect(p, T_RP);
    }
    parser_expect(p, T_SC);
    return n;
  }
  Syntax_Node *e = parse_name(p);
  if (parser_match(p, T_AS))
  {
    Syntax_Node *n = ND(AS, lc);
    if (e and e->k == N_CL)
    {
      Syntax_Node *fn = e->cl.fn;
      Node_Vector ar = e->cl.ar;
      e->k = N_IX;
      e->ix.p = fn;
      e->ix.ix = ar;
    }
    n->as.tg = e;
    n->as.vl = parse_expression(p);
    parser_expect(p, T_SC);
    return n;
  }
  Syntax_Node *n = ND(CLT, lc);
  if (e->k == N_IX)
  {
    n->ct.nm = e->ix.p;
    n->ct.arr = e->ix.ix;
  }
  else if (e->k == N_CL)
  {
    n->ct.nm = e->cl.fn;
    n->ct.arr = e->cl.ar;
  }
  else
    n->ct.nm = e;
  parser_expect(p, T_SC);
  return n;
}
static Node_Vector parse_statement(Parser *p)
{
  Node_Vector v = {0};
  while (not parser_at(p, T_END) and not parser_at(p, T_EXCP) and not parser_at(p, T_ELSIF)
         and not parser_at(p, T_ELSE) and not parser_at(p, T_WHN) and not parser_at(p, T_OR))
    nv(&v, ps(p));
  return v;
}
static Node_Vector phd(Parser *p)
{
  Node_Vector v = {0};
  while (parser_match(p, T_WHN))
  {
    Source_Location lc = parser_location(p);
    Syntax_Node *h = ND(HD, lc);
    do
    {
      if (parser_match(p, T_OTH))
      {
        Syntax_Node *n = ND(ID, lc);
        n->s = STRING_LITERAL("others");
        nv(&h->hnd.ec, n);
      }
      else
        nv(&h->hnd.ec, parse_name(p));
    } while (parser_match(p, T_BR));
    parser_expect(p, T_AR);
    while (not parser_at(p, T_WHN) and not parser_at(p, T_END))
      nv(&h->hnd.stz, ps(p));
    nv(&v, h);
  }
  return v;
}
static Syntax_Node *ptd(Parser *p)
{
  Source_Location lc = parser_location(p);
  if (parser_match(p, T_LP))
  {
    Syntax_Node *n = ND(TE, lc);
    do
    {
      if (parser_at(p, T_CHAR))
      {
        Syntax_Node *c = ND(CHAR, lc);
        c->i = p->cr.iv;
        parser_next(p);
        nv(&n->lst.it, c);
      }
      else
      {
        String_Slice nm = parser_identifier(p);
        Syntax_Node *i = ND(ID, lc);
        i->s = nm;
        nv(&n->lst.it, i);
      }
    } while (parser_match(p, T_CM));
    parser_expect(p, T_RP);
    return n;
  }
  if (parser_match(p, T_RNG))
  {
    Syntax_Node *n = ND(TI, lc);
    if (parser_match(p, T_BX))
    {
      n->rn.lo = 0;
      n->rn.hi = 0;
    }
    else
    {
      n->rn.lo = psm(p);
      parser_expect(p, T_DD);
      n->rn.hi = psm(p);
    }
    return n;
  }
  if (parser_match(p, T_MOD))
  {
    Syntax_Node *n = ND(TI, lc);
    n->un.op = T_MOD;
    n->un.x = parse_expression(p);
    return n;
  }
  if (parser_match(p, T_DIG))
  {
    Syntax_Node *n = ND(TF, lc);
    if (parser_match(p, T_BX))
      n->un.x = 0;
    else
      n->un.x = parse_expression(p);
    if (parser_match(p, T_RNG))
    {
      n->rn.lo = psm(p);
      parser_expect(p, T_DD);
      n->rn.hi = psm(p);
    }
    return n;
  }
  if (parser_match(p, T_DELTA))
  {
    Syntax_Node *n = ND(TX, lc);
    if (parser_match(p, T_BX))
    {
      n->rn.lo = 0;
      n->rn.hi = 0;
      n->bn.r = 0;
    }
    else
    {
      n->rn.lo = parse_expression(p);
      parser_expect(p, T_RNG);
      n->rn.hi = psm(p);
      parser_expect(p, T_DD);
      n->bn.r = psm(p);
    }
    return n;
  }
  if (parser_match(p, T_ARR))
  {
    parser_expect(p, T_LP);
    Syntax_Node *n = ND(TA, lc);
    do
    {
      Syntax_Node *ix = parse_range(p);
      if (ix->k == N_ID and parser_match(p, T_RNG))
      {
        Syntax_Node *st = ND(ST, lc);
        st->sd.in = ix;
        Syntax_Node *cn = ND(CN, lc);
        cn->cn.rn = parse_range(p);
        st->sd.cn = cn;
        nv(&n->ix.ix, st);
      }
      else
        nv(&n->ix.ix, ix);
    } while (parser_match(p, T_CM));
    parser_expect(p, T_RP);
    parser_expect(p, T_OF);
    n->ix.p = psi(p);
    return n;
  }
  if (parser_match(p, T_REC))
  {
    Syntax_Node *n = ND(TR, lc);
    uint32_t of = 0;
    Node_Vector dc = {0};
    if (parser_match(p, T_LP))
    {
      do
      {
        String_Slice dn = parser_identifier(p);
        parser_expect(p, T_CL);
        Syntax_Node *dt = parse_name(p);
        Syntax_Node *dd = 0;
        if (parser_match(p, T_AS))
          dd = parse_expression(p);
        Syntax_Node *dp = ND(DS, lc);
        dp->pm.nm = dn;
        dp->pm.ty = dt;
        dp->pm.df = dd;
        nv(&dc, dp);
      } while (parser_match(p, T_SC));
      parser_expect(p, T_RP);
      if (not parser_at(p, T_IS))
        parser_expect(p, T_SC);
    }
    if (parser_match(p, T_IS))
    {
      parser_expect(p, T_REC);
    }
    while (not parser_at(p, T_END) and not parser_at(p, T_CSE) and not parser_at(p, T_NULL))
    {
      Node_Vector id = {0};
      do
      {
        String_Slice nm = parser_identifier(p);
        Syntax_Node *i = ND(ID, lc);
        i->s = nm;
        nv(&id, i);
      } while (parser_match(p, T_CM));
      parser_expect(p, T_CL);
      Syntax_Node *ty = psi(p);
      Syntax_Node *in = 0;
      if (parser_match(p, T_AS))
        in = parse_expression(p);
      parser_expect(p, T_SC);
      for (uint32_t i = 0; i < id.n; i++)
      {
        Syntax_Node *c = ND(CM, lc);
        c->cm.nm = id.d[i]->s;
        c->cm.ty = ty;
        c->cm.in = in;
        c->cm.of = of++;
        c->cm.dc = 0;
        c->cm.dsc = 0;
        if (dc.n > 0)
        {
          c->cm.dc = ND(LST, lc);
          c->cm.dc->lst.it = dc;
        }
        nv(&n->lst.it, c);
      }
    }
    if (parser_match(p, T_NULL))
    {
      parser_expect(p, T_SC);
    }
    if (parser_match(p, T_CSE))
    {
      Syntax_Node *vp = ND(VP, lc);
      vp->vp.ds = parse_name(p);
      parser_expect(p, T_IS);
      while (parser_match(p, T_WHN))
      {
        Syntax_Node *v = ND(VR, lc);
        do
        {
          Syntax_Node *e = parse_expression(p);
          if (parser_match(p, T_DD))
          {
            Syntax_Node *r = ND(RN, lc);
            r->rn.lo = e;
            r->rn.hi = parse_expression(p);
            e = r;
          }
          nv(&v->vr.ch, e);
        } while (parser_match(p, T_BR));
        parser_expect(p, T_AR);
        while (not parser_at(p, T_WHN) and not parser_at(p, T_END) and not parser_at(p, T_NULL))
        {
          Node_Vector id = {0};
          do
          {
            String_Slice nm = parser_identifier(p);
            Syntax_Node *i = ND(ID, lc);
            i->s = nm;
            nv(&id, i);
          } while (parser_match(p, T_CM));
          parser_expect(p, T_CL);
          Syntax_Node *ty = psi(p);
          Syntax_Node *in = 0;
          if (parser_match(p, T_AS))
            in = parse_expression(p);
          parser_expect(p, T_SC);
          for (uint32_t i = 0; i < id.n; i++)
          {
            Syntax_Node *c = ND(CM, lc);
            c->cm.nm = id.d[i]->s;
            c->cm.ty = ty;
            c->cm.in = in;
            c->cm.of = of++;
            c->cm.dc = 0;
            c->cm.dsc = 0;
            if (dc.n > 0)
            {
              c->cm.dc = ND(LST, lc);
              c->cm.dc->lst.it = dc;
            }
            nv(&v->vr.cmm, c);
          }
        }
        if (parser_match(p, T_NULL))
          parser_expect(p, T_SC);
        nv(&vp->vp.vrr, v);
      }
      vp->vp.sz = of;
      if (parser_match(p, T_NULL))
      {
        parser_expect(p, T_REC);
      }
      parser_expect(p, T_END);
      parser_expect(p, T_CSE);
      parser_expect(p, T_SC);
      nv(&n->lst.it, vp);
    }
    parser_expect(p, T_END);
    parser_expect(p, T_REC);
    return n;
  }
  if (parser_match(p, T_ACCS))
  {
    Syntax_Node *n = ND(TAC, lc);
    n->un.x = psi(p);
    return n;
  }
  if (parser_match(p, T_PRV))
    return ND(TP, lc);
  if (parser_match(p, T_LIM))
  {
    parser_match(p, T_PRV);
    return ND(TP, lc);
  }
  return psi(p);
}
static RC *prc(Parser *p)
{
  Source_Location lc = parser_location(p);
  (void) lc;
  if (parser_match(p, T_FOR))
  {
    parse_name(p);
    parser_expect(p, T_USE);
    if (parser_match(p, T_AT))
    {
      RC *r = reference_counter_new(2, 0);
      parse_expression(p);
      parser_expect(p, T_SC);
      return r;
    }
    if (parser_match(p, T_REC))
    {
      while (not parser_at(p, T_END))
      {
        parser_identifier(p);
        parser_expect(p, T_AT);
        parse_expression(p);
        parser_expect(p, T_RNG);
        parse_range(p);
        parser_expect(p, T_SC);
      }
      parser_expect(p, T_END);
      parser_expect(p, T_REC);
      parser_expect(p, T_SC);
      return 0;
    }
    parse_expression(p);
    parser_expect(p, T_SC);
    return 0;
  }
  if (parser_match(p, T_PGM))
  {
    String_Slice nm = parser_identifier(p);
    RC *r = 0;
    if (string_equal_ignore_case(nm, STRING_LITERAL("SUPPRESS")))
    {
      if (parser_at(p, T_LP))
      {
        parser_expect(p, T_LP);
        String_Slice ck = parser_identifier(p);
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
          r->ad.nm = parser_match(p, T_CM) ? parser_identifier(p) : N;
          while (parser_match(p, T_CM))
            parser_identifier(p);
          r->ad.ad = cm;
        }
        else
        {
          while (parser_match(p, T_CM))
            parser_identifier(p);
        }
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
      return r;
    }
    else if (string_equal_ignore_case(nm, STRING_LITERAL("PACK")))
    {
      if (parser_at(p, T_LP))
      {
        parser_expect(p, T_LP);
        Syntax_Node *tn = parse_name(p);
        while (parser_match(p, T_CM))
          parse_name(p);
        parser_expect(p, T_RP);
        r = reference_counter_new(5, 0);
        r->er.nm = tn and tn->k == N_ID ? tn->s : N;
      }
      parser_expect(p, T_SC);
      return r;
    }
    else if (string_equal_ignore_case(nm, STRING_LITERAL("INLINE")))
    {
      if (parser_at(p, T_LP))
      {
        parser_expect(p, T_LP);
        r = reference_counter_new(6, 0);
        r->er.nm = parser_identifier(p);
        while (parser_match(p, T_CM))
          parser_identifier(p);
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
      return r;
    }
    else if (string_equal_ignore_case(nm, STRING_LITERAL("CONTROLLED")))
    {
      if (parser_at(p, T_LP))
      {
        parser_expect(p, T_LP);
        r = reference_counter_new(7, 0);
        r->er.nm = parser_identifier(p);
        while (parser_match(p, T_CM))
          parser_identifier(p);
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
      return r;
    }
    else if (
        string_equal_ignore_case(nm, STRING_LITERAL("INTERFACE"))
        or string_equal_ignore_case(nm, STRING_LITERAL("IMPORT")))
    {
      if (parser_at(p, T_LP))
      {
        parser_expect(p, T_LP);
        r = reference_counter_new(8, 0);
        r->im.lang = parser_identifier(p);
        if (parser_match(p, T_CM))
        {
          r->im.nm = parser_identifier(p);
          if (parser_match(p, T_CM))
          {
            if (parser_at(p, T_STR))
            {
              r->im.ext = p->cr.lit;
              parser_next(p);
            }
            else
              r->im.ext = parser_identifier(p);
          }
          else
            r->im.ext = r->im.nm;
        }
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
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
      if (parser_match(p, T_LP))
      {
        do
          parse_expression(p);
        while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
      return 0;
    }
    else
    {
      if (string_equal_ignore_case(nm, STRING_LITERAL("ELABORATE"))
          or string_equal_ignore_case(nm, STRING_LITERAL("ELABORATE_ALL")))
      {
        parser_expect(p, T_LP);
        do
          parser_identifier(p);
        while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
      }
      else if (parser_match(p, T_LP))
      {
        do
          parser_identifier(p);
        while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
    }
  }
  return 0;
}
static Syntax_Node *pdl(Parser *p)
{
  Source_Location lc = parser_location(p);
  if (parser_at(p, T_GEN))
    return pgf(p);
  if (parser_match(p, T_TYP))
  {
    String_Slice nm = parser_identifier(p);
    Syntax_Node *n = ND(TD, lc);
    n->td.nm = nm;
    if (parser_match(p, T_LP))
    {
      Syntax_Node *ds = ND(LST, lc);
      do
      {
        Node_Vector dn = {0};
        do
        {
          String_Slice dnm = parser_identifier(p);
          Syntax_Node *di = ND(ID, lc);
          di->s = dnm;
          nv(&dn, di);
        } while (parser_match(p, T_CM));
        parser_expect(p, T_CL);
        Syntax_Node *dt = parse_name(p);
        Syntax_Node *dd = 0;
        if (parser_match(p, T_AS))
          dd = parse_expression(p);
        for (uint32_t i = 0; i < dn.n; i++)
        {
          Syntax_Node *dp = ND(DS, lc);
          dp->pm.nm = dn.d[i]->s;
          dp->pm.ty = dt;
          dp->pm.df = dd;
          nv(&ds->lst.it, dp);
        }
      } while (parser_match(p, T_SC));
      parser_expect(p, T_RP);
      n->td.dsc = ds->lst.it;
    }
    if (parser_match(p, T_IS))
    {
      n->td.nw = parser_match(p, T_NEW);
      n->td.drv = n->td.nw;
      if (n->td.drv)
      {
        n->td.prt = parse_name(p);
        n->td.df = n->td.prt;
        if (parser_match(p, T_DIG))
        {
          parse_expression(p);
          if (parser_match(p, T_RNG))
          {
            psm(p);
            parser_expect(p, T_DD);
            psm(p);
          }
        }
        else if (parser_match(p, T_DELTA))
        {
          parse_expression(p);
          parser_expect(p, T_RNG);
          psm(p);
          parser_expect(p, T_DD);
          psm(p);
        }
        else if (parser_match(p, T_RNG))
        {
          Syntax_Node *rn = ND(RN, lc);
          rn->rn.lo = psm(p);
          parser_expect(p, T_DD);
          rn->rn.hi = psm(p);
          n->td.df = rn;
        }
      }
      else
        n->td.df = ptd(p);
    }
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_SUB))
  {
    String_Slice nm = parser_identifier(p);
    parser_expect(p, T_IS);
    Syntax_Node *n = ND(SD, lc);
    n->sd.nm = nm;
    n->sd.in = psi(p);
    if (n->sd.in->k == N_ST)
      n->sd.rn = n->sd.in->sd.cn->cn.rn;
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_at(p, T_PROC))
  {
    Syntax_Node *sp = pps_(p);
    if (parser_match(p, T_REN))
    {
      parse_expression(p);
      parser_expect(p, T_SC);
      Syntax_Node *n = ND(PD, lc);
      n->bd.sp = sp;
      return n;
    }
    if (parser_match(p, T_IS))
    {
      if (parser_match(p, T_SEP))
      {
        parser_expect(p, T_SC);
        Syntax_Node *n = ND(PD, lc);
        n->bd.sp = sp;
        return n;
      }
      if (parser_match(p, T_NEW))
      {
        String_Slice gn = parser_identifier(p);
        Node_Vector ap = {0};
        if (parser_match(p, T_LP))
        {
          do
          {
            Syntax_Node *e = parse_expression(p);
            if (e->k == N_ID and parser_at(p, T_AR))
            {
              parser_next(p);
              Syntax_Node *a = ND(ASC, lc);
              nv(&a->asc.ch, e);
              a->asc.vl = parse_expression(p);
              nv(&ap, a);
            }
            else
            {
              nv(&ap, e);
            }
          } while (parser_match(p, T_CM));
          parser_expect(p, T_RP);
        }
        parser_expect(p, T_SC);
        Syntax_Node *n = ND(GINST, lc);
        n->gi.nm = sp->sp.nm;
        n->gi.gn = gn;
        n->gi.ap = ap;
        return n;
      }
      Syntax_Node *n = ND(PB, lc);
      n->bd.sp = sp;
      n->bd.dc = pdc(p);
      parser_expect(p, T_BEG);
      n->bd.st = parse_statement(p);
      if (parser_match(p, T_EXCP))
        n->bd.hdd = phd(p);
      parser_expect(p, T_END);
      if (parser_at(p, T_ID) or parser_at(p, T_STR))
        parser_next(p);
      parser_expect(p, T_SC);
      return n;
    }
    parser_expect(p, T_SC);
    Syntax_Node *n = ND(PD, lc);
    n->bd.sp = sp;
    return n;
  }
  if (parser_match(p, T_FUN))
  {
    String_Slice nm;
    if (parser_at(p, T_STR))
    {
      nm = p->cr.lit;
      parser_next(p);
    }
    else
      nm = parser_identifier(p);
    if (parser_match(p, T_IS) and parser_match(p, T_NEW))
    {
      String_Slice gn = parser_identifier(p);
      Node_Vector ap = {0};
      if (parser_match(p, T_LP))
      {
        do
        {
          Syntax_Node *e = parse_expression(p);
          if (e->k == N_ID and parser_at(p, T_AR))
          {
            parser_next(p);
            Syntax_Node *a = ND(ASC, lc);
            nv(&a->asc.ch, e);
            a->asc.vl = parse_expression(p);
            nv(&ap, a);
          }
          else
          {
            nv(&ap, e);
          }
        } while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
      Syntax_Node *n = ND(GINST, lc);
      n->gi.nm = nm;
      n->gi.gn = gn;
      n->gi.ap = ap;
      return n;
    }
    Syntax_Node *sp = ND(FS, lc);
    sp->sp.nm = nm;
    sp->sp.pmm = ppm(p);
    parser_expect(p, T_RET);
    sp->sp.rt = parse_name(p);
    if (parser_match(p, T_REN))
    {
      parse_expression(p);
      parser_expect(p, T_SC);
      Syntax_Node *n = ND(FD, lc);
      n->bd.sp = sp;
      return n;
    }
    if (parser_match(p, T_IS))
    {
      if (parser_match(p, T_SEP))
      {
        parser_expect(p, T_SC);
        Syntax_Node *n = ND(FD, lc);
        n->bd.sp = sp;
        return n;
      }
      Syntax_Node *n = ND(FB, lc);
      n->bd.sp = sp;
      n->bd.dc = pdc(p);
      parser_expect(p, T_BEG);
      n->bd.st = parse_statement(p);
      if (parser_match(p, T_EXCP))
        n->bd.hdd = phd(p);
      parser_expect(p, T_END);
      if (parser_at(p, T_ID) or parser_at(p, T_STR))
        parser_next(p);
      parser_expect(p, T_SC);
      return n;
    }
    parser_expect(p, T_SC);
    Syntax_Node *n = ND(FD, lc);
    n->bd.sp = sp;
    return n;
  }
  if (parser_match(p, T_PKG))
  {
    if (parser_match(p, T_BOD))
    {
      String_Slice nm = parser_identifier(p);
      parser_expect(p, T_IS);
      if (parser_match(p, T_SEP))
      {
        parser_expect(p, T_SC);
        Syntax_Node *n = ND(PKB, lc);
        n->pb.nm = nm;
        return n;
      }
      Syntax_Node *n = ND(PKB, lc);
      n->pb.nm = nm;
      n->pb.dc = pdc(p);
      if (parser_match(p, T_BEG))
      {
        n->pb.st = parse_statement(p);
        if (parser_match(p, T_EXCP))
          n->pb.hddd = phd(p);
      }
      parser_expect(p, T_END);
      if (parser_at(p, T_ID))
        parser_next(p);
      parser_expect(p, T_SC);
      return n;
    }
    String_Slice nm = parser_identifier(p);
    if (parser_match(p, T_REN))
    {
      Syntax_Node *rn = parse_expression(p);
      parser_expect(p, T_SC);
      Syntax_Node *n = ND(RE, lc);
      n->re.nm = nm;
      n->re.rn = rn;
      return n;
    }
    parser_expect(p, T_IS);
    if (parser_match(p, T_NEW))
    {
      String_Slice gn = parser_identifier(p);
      Node_Vector ap = {0};
      if (parser_match(p, T_LP))
      {
        do
        {
          Syntax_Node *e = parse_expression(p);
          if (e->k == N_ID and parser_at(p, T_AR))
          {
            parser_next(p);
            Syntax_Node *a = ND(ASC, lc);
            nv(&a->asc.ch, e);
            a->asc.vl = parse_expression(p);
            nv(&ap, a);
          }
          else
          {
            nv(&ap, e);
          }
        } while (parser_match(p, T_CM));
        parser_expect(p, T_RP);
      }
      parser_expect(p, T_SC);
      Syntax_Node *n = ND(GINST, lc);
      n->gi.nm = nm;
      n->gi.gn = gn;
      n->gi.ap = ap;
      return n;
    }
    Syntax_Node *n = ND(PKS, lc);
    n->ps.nm = nm;
    n->ps.dc = pdc(p);
    if (parser_match(p, T_PRV))
      n->ps.pr = pdc(p);
    parser_expect(p, T_END);
    if (parser_at(p, T_ID))
      parser_next(p);
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_TSK))
  {
    if (parser_match(p, T_BOD))
    {
      String_Slice nm = parser_identifier(p);
      parser_expect(p, T_IS);
      if (parser_match(p, T_SEP))
      {
        parser_expect(p, T_SC);
        Syntax_Node *n = ND(TKB, lc);
        n->tb.nm = nm;
        return n;
      }
      Syntax_Node *n = ND(TKB, lc);
      n->tb.nm = nm;
      n->tb.dc = pdc(p);
      parser_expect(p, T_BEG);
      n->tb.st = parse_statement(p);
      if (parser_match(p, T_EXCP))
        n->tb.hdz = phd(p);
      parser_expect(p, T_END);
      if (parser_at(p, T_ID))
        parser_next(p);
      parser_expect(p, T_SC);
      return n;
    }
    bool it = parser_match(p, T_TYP);
    String_Slice nm = parser_identifier(p);
    Syntax_Node *n = ND(TKS, lc);
    n->ts.nm = nm;
    n->ts.it = it;
    if (parser_match(p, T_IS))
    {
      while (not parser_at(p, T_END))
      {
        if (parser_match(p, T_ENT))
        {
          Syntax_Node *e = ND(ENT, lc);
          e->ent.nm = parser_identifier(p);
          if (parser_at(p, T_LP))
          {
            if (p->pk.t == T_ID or p->pk.t == T_INT or p->pk.t == T_CHAR)
            {
              Token scr = p->cr, spk = p->pk;
              Lexer slx = p->lx;
              parser_next(p);
              parser_next(p);
              if (p->cr.t == T_CM or p->cr.t == T_CL)
              {
                p->cr = scr;
                p->pk = spk;
                p->lx = slx;
                e->ent.pmy = ppm(p);
              }
              else
              {
                p->cr = scr;
                p->pk = spk;
                p->lx = slx;
                parser_expect(p, T_LP);
                Syntax_Node *ix = parse_range(p);
                if (ix->k != N_RN and parser_match(p, T_RNG))
                {
                  Syntax_Node *rng = parse_range(p);
                  Syntax_Node *si = ND(ST, lc);
                  Syntax_Node *cn = ND(CN, lc);
                  cn->cn.rn = rng;
                  si->sd.in = ix;
                  si->sd.cn = cn;
                  nv(&e->ent.ixy, si);
                }
                else
                {
                  nv(&e->ent.ixy, ix);
                }
                parser_expect(p, T_RP);
                e->ent.pmy = ppm(p);
              }
            }
            else
            {
              parser_expect(p, T_LP);
              Syntax_Node *ix = parse_range(p);
              if (ix->k != N_RN and parser_match(p, T_RNG))
              {
                Syntax_Node *rng = parse_range(p);
                Syntax_Node *si = ND(ST, lc);
                Syntax_Node *cn = ND(CN, lc);
                cn->cn.rn = rng;
                si->sd.in = ix;
                si->sd.cn = cn;
                nv(&e->ent.ixy, si);
              }
              else
              {
                nv(&e->ent.ixy, ix);
              }
              parser_expect(p, T_RP);
              e->ent.pmy = ppm(p);
            }
          }
          else
          {
            e->ent.pmy = ppm(p);
          }
          parser_expect(p, T_SC);
          nv(&n->ts.en, e);
        }
        else if (parser_match(p, T_PGM))
        {
          parser_identifier(p);
          if (parser_match(p, T_LP))
          {
            do
              parse_expression(p);
            while (parser_match(p, T_CM));
            parser_expect(p, T_RP);
          }
          parser_expect(p, T_SC);
        }
      }
      parser_expect(p, T_END);
      if (parser_at(p, T_ID))
        parser_next(p);
    }
    parser_expect(p, T_SC);
    return n;
  }
  if (parser_match(p, T_USE))
  {
    Node_Vector nms = {0};
    do
      nv(&nms, parse_name(p));
    while (parser_match(p, T_CM));
    parser_expect(p, T_SC);
    if (nms.n == 1)
    {
      Syntax_Node *n = ND(US, lc);
      n->us.nm = nms.d[0];
      return n;
    }
    Syntax_Node *lst = ND(LST, lc);
    for (uint32_t i = 0; i < nms.n; i++)
    {
      Syntax_Node *u = ND(US, lc);
      u->us.nm = nms.d[i];
      nv(&lst->lst.it, u);
    }
    return lst;
  }
  if (parser_match(p, T_PGM))
  {
    Syntax_Node *n = ND(PG, lc);
    n->pg.nm = parser_identifier(p);
    if (parser_match(p, T_LP))
    {
      do
        nv(&n->pg.ar, parse_expression(p));
      while (parser_match(p, T_CM));
      parser_expect(p, T_RP);
    }
    parser_expect(p, T_SC);
    return n;
  }
  {
    Node_Vector id = {0};
    do
    {
      String_Slice nm = parser_identifier(p);
      Syntax_Node *i = ND(ID, lc);
      i->s = nm;
      nv(&id, i);
    } while (parser_match(p, T_CM));
    parser_expect(p, T_CL);
    bool co = parser_match(p, T_CONST);
    if (parser_match(p, T_EXCP))
    {
      Syntax_Node *n = ND(ED, lc);
      n->ed.id = id;
      if (parser_match(p, T_REN))
        n->ed.rn = parse_expression(p);
      parser_expect(p, T_SC);
      return n;
    }
    Syntax_Node *ty = 0;
    if (not parser_at(p, T_AS))
    {
      if (parser_at(p, T_ARR) or parser_at(p, T_ACCS))
        ty = ptd(p);
      else
        ty = psi(p);
    }
    Syntax_Node *in = 0;
    if (parser_match(p, T_REN))
      in = parse_expression(p);
    else if (parser_match(p, T_AS))
      in = parse_expression(p);
    parser_expect(p, T_SC);
    Syntax_Node *n = ND(OD, lc);
    n->od.id = id;
    n->od.ty = ty;
    n->od.in = in;
    n->od.co = co;
    return n;
  }
}
static Node_Vector pdc(Parser *p)
{
  Node_Vector v = {0};
  while (not parser_at(p, T_BEG) and not parser_at(p, T_END) and not parser_at(p, T_PRV)
         and not parser_at(p, T_EOF) and not parser_at(p, T_ENT))
  {
    if (parser_at(p, T_FOR))
    {
      RC *r = prc(p);
      if (r)
      {
        Syntax_Node *n = ND(RRC, parser_location(p));
        memcpy(&n->ag.it.d, &r, sizeof(RC *));
        nv(&v, n);
      }
      continue;
    }
    if (parser_at(p, T_PGM))
    {
      RC *r = prc(p);
      if (r)
      {
        Syntax_Node *n = ND(RRC, parser_location(p));
        memcpy(&n->ag.it.d, &r, sizeof(RC *));
        nv(&v, n);
      }
      continue;
    }
    nv(&v, pdl(p));
  }
  return v;
}
static Syntax_Node *pcx(Parser *p)
{
  Source_Location lc = parser_location(p);
  Syntax_Node *cx = ND(CX, lc);
  while (parser_at(p, T_WITH) or parser_at(p, T_USE) or parser_at(p, T_PGM))
  {
    if (parser_match(p, T_WITH))
    {
      do
      {
        Syntax_Node *w = ND(WI, lc);
        w->wt.nm = parser_identifier(p);
        nv(&cx->cx.wt, w);
      } while (parser_match(p, T_CM));
      parser_expect(p, T_SC);
    }
    else if (parser_match(p, T_USE))
    {
      do
      {
        Syntax_Node *u = ND(US, lc);
        u->us.nm = parse_name(p);
        nv(&cx->cx.us, u);
      } while (parser_match(p, T_CM));
      parser_expect(p, T_SC);
    }
    else
    {
      Syntax_Node *pg = pdl(p);
      if (pg)
        nv(&cx->cx.us, pg);
    }
  }
  return cx;
}
static Syntax_Node *pcu(Parser *p)
{
  Source_Location lc = parser_location(p);
  Syntax_Node *n = ND(CU, lc);
  n->cu.cx = pcx(p);
  while (parser_at(p, T_WITH) or parser_at(p, T_USE) or parser_at(p, T_PROC) or parser_at(p, T_FUN)
         or parser_at(p, T_PKG) or parser_at(p, T_GEN) or parser_at(p, T_PGM)
         or parser_at(p, T_SEP))
  {
    if (parser_at(p, T_WITH) or parser_at(p, T_USE) or parser_at(p, T_PGM))
    {
      Syntax_Node *cx = pcx(p);
      for (uint32_t i = 0; i < cx->cx.wt.n; i++)
        nv(&n->cu.cx->cx.wt, cx->cx.wt.d[i]);
      for (uint32_t i = 0; i < cx->cx.us.n; i++)
        nv(&n->cu.cx->cx.us, cx->cx.us.d[i]);
    }
    else if (parser_at(p, T_SEP))
    {
      parser_expect(p, T_SEP);
      parser_expect(p, T_LP);
      Syntax_Node *pnm_ = parse_name(p);
      parser_expect(p, T_RP);
      String_Slice ppkg = pnm_->k == N_ID ? pnm_->s : pnm_->k == N_SEL ? pnm_->se.p->s : N;
      SEP_PKG = ppkg.s ? string_duplicate(ppkg) : N;
      if (ppkg.s)
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
                (int) ppkg.n,
                ppkg.s,
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
          Parser pp = {ln(psrc, sz, fn), {0}, {0}, 0, {0}};
          parser_next(&pp);
          parser_next(&pp);
          Syntax_Node *pcu_ = pcu(&pp);
          if (pcu_ and pcu_->cu.cx)
          {
            for (uint32_t i = 0; i < pcu_->cu.cx->cx.wt.n; i++)
              nv(&n->cu.cx->cx.wt, pcu_->cu.cx->cx.wt.d[i]);
            for (uint32_t i = 0; i < pcu_->cu.cx->cx.us.n; i++)
              nv(&n->cu.cx->cx.us, pcu_->cu.cx->cx.us.d[i]);
          }
        }
      }
      nv(&n->cu.un, pdl(p));
    }
    else
      nv(&n->cu.un, pdl(p));
  }
  return n;
}
static Parser pnw(const char *s, size_t z, const char *f)
{
  Parser p = {ln(s, z, f), {0}, {0}, 0, {0}};
  parser_next(&p);
  parser_next(&p);
  return p;
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
} Tk_;
struct Type_Info
{
  Tk_ k;
  String_Slice nm;
  Type_Info *bs, *el, *prt;
  Type_Info *ix;
  int64_t lo, hi;
  Node_Vector cm, dc;
  uint32_t sz, al;
  Symbol_Vector ev;
  RV rc;
  uint64_t ad;
  bool pk;
  Node_Vector ops;
  int64_t sm, lg;
  uint16_t sup;
  bool ctrl;
  uint8_t frz;
  Syntax_Node *fzn;
};
struct Symbol
{
  String_Slice nm;
  uint8_t k;
  Type_Info *ty;
  Syntax_Node *df;
  Symbol *nx, *pv;
  int sc;
  int ss;
  int64_t vl;
  uint32_t of;
  Node_Vector ol;
  Symbol_Vector us;
  int el;
  GT *gt;
  Symbol *pr;
  int lv;
  bool inl;
  bool shrd;
  bool ext;
  String_Slice ext_nm;
  String_Slice ext_lang;
  String_Slice mangled_nm;
  uint8_t frz;
  Syntax_Node *fzn;
  uint8_t vis;
  Symbol *hm;
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
  LV lu;
  GV gt;
  jmp_buf *eb[16];
  int ed;
  String_Slice ce[16];
  FV io;
  int fn;
  SLV lb;
  int lv;
  Node_Vector ib;
  Symbol *sst[256];
  int ssd;
  Symbol_Vector dps[256];
  int dpn;
  Symbol_Vector ex;
  uint64_t uv_vis[64];
  SLV eh;
  SLV ap;
  uint32_t uid_ctr;
} Symbol_Manager;
static uint32_t syh(String_Slice s)
{
  return string_hash(s) & 4095;
}
static Symbol *syn(String_Slice nm, uint8_t k, Type_Info *ty, Syntax_Node *df)
{
  Symbol *s = arena_allocate(sizeof(Symbol));
  s->nm = string_duplicate(nm);
  s->k = k;
  s->ty = ty;
  s->df = df;
  s->el = -1;
  s->lv = -1;
  return s;
}
static Symbol *symbol_add_overload(Symbol_Manager *SM, Symbol *s)
{
  uint32_t h = syh(s->nm);
  s->hm = SM->sy[h];
  s->nx = SM->sy[h];
  s->sc = SM->sc;
  s->ss = SM->ss;
  s->el = SM->eo++;
  s->lv = SM->lv;
  s->vis = 1;
  uint64_t u = string_hash(s->nm);
  if (s->pr)
  {
    u = u * 31 + string_hash(s->pr->nm);
    if (s->lv > 0)
    {
      u = u * 31 + s->sc;
      u = u * 31 + s->el;
    }
  }
  s->uid = (uint32_t) (u & 0xFFFFFFFF);
  SM->sy[h] = s;
  if (SM->ssd < 256)
    SM->sst[SM->ssd++] = s;
  return s;
}
static Symbol *symbol_find(Symbol_Manager *SM, String_Slice nm)
{
  Symbol *imm = 0, *pot = 0;
  uint32_t h = syh(nm);
  for (Symbol *s = SM->sy[h]; s; s = s->nx)
    if (string_equal_ignore_case(s->nm, nm))
    {
      if (s->vis & 1 and (not imm or s->sc > imm->sc))
        imm = s;
      if (s->vis & 2 and not pot)
        pot = s;
    }
  if (imm)
    return imm;
  if (pot)
    return pot;
  for (Symbol *s = SM->sy[h]; s; s = s->nx)
    if (string_equal_ignore_case(s->nm, nm) and (not imm or s->sc > imm->sc))
      imm = s;
  return imm;
}
static void sfu(Symbol_Manager *SM, Symbol *s, String_Slice nm)
{
  uint32_t h = syh(nm) & 63, b = 1ULL << (syh(nm) & 63);
  if (SM->uv_vis[h] & b)
    return;
  SM->uv_vis[h] |= b;
  for (Symbol *p = s; p; p = p->nx)
    if (string_equal_ignore_case(p->nm, nm) and p->k == 6 and p->df and p->df->k == N_PKS)
    {
      Syntax_Node *pk = p->df;
      for (uint32_t i = 0; i < pk->ps.dc.n; i++)
      {
        Syntax_Node *d = pk->ps.dc.d[i];
        if (d->sy)
        {
          sv(&s->us, d->sy);
          d->sy->vis |= 2;
        }
        else if (d->k == N_ED)
        {
          for (uint32_t j = 0; j < d->ed.id.n; j++)
          {
            Syntax_Node *e = d->ed.id.d[j];
            if (e->sy)
            {
              sv(&s->us, e->sy);
              e->sy->vis |= 2;
              sv(&SM->ex, e->sy);
            }
          }
        }
        else if (d->k == N_OD)
        {
          for (uint32_t j = 0; j < d->od.id.n; j++)
          {
            Syntax_Node *oid = d->od.id.d[j];
            if (oid->sy)
            {
              sv(&s->us, oid->sy);
              oid->sy->vis |= 2;
            }
          }
        }
      }
      if (SM->dpn < 256)
      {
        bool f = 0;
        for (int i = 0; i < SM->dpn; i++)
          if (SM->dps[i].n and string_equal_ignore_case(SM->dps[i].d[0]->nm, p->nm))
          {
            f = 1;
            break;
          }
        if (not f)
        {
          sv(&SM->dps[SM->dpn], p);
          SM->dpn++;
        }
      }
    }
  SM->uv_vis[h] &= ~b;
}
static GT *gfnd(Symbol_Manager *SM, String_Slice nm)
{
  for (uint32_t i = 0; i < SM->gt.n; i++)
  {
    GT *g = SM->gt.d[i];
    if (string_equal_ignore_case(g->nm, nm))
      return g;
  }
  return 0;
}
static int tysc(Type_Info *, Type_Info *, Type_Info *);
static Symbol *symbol_find_with_arity(Symbol_Manager *SM, String_Slice nm, int na, Type_Info *tx)
{
  Symbol_Vector cv = {0};
  int msc = -1;
  for (Symbol *s = SM->sy[syh(nm)]; s; s = s->nx)
    if (string_equal_ignore_case(s->nm, nm) and (s->vis & 3))
    {
      if (s->sc > msc)
      {
        cv.n = 0;
        msc = s->sc;
      }
      if (s->sc == msc)
        sv(&cv, s);
    }
  if (not cv.n)
    return 0;
  if (cv.n == 1)
    return cv.d[0];
  Symbol *br = 0;
  int bs = -1;
  for (uint32_t i = 0; i < cv.n; i++)
  {
    Symbol *c = cv.d[i];
    int sc = 0;
    if ((c->k == 4 or c->k == 5) and na >= 0)
    {
      if (c->ol.n > 0)
      {
        for (uint32_t j = 0; j < c->ol.n; j++)
        {
          Syntax_Node *b = c->ol.d[j];
          if (b->k == N_PB or b->k == N_FB)
          {
            int np = b->bd.sp->sp.pmm.n;
            if (np == na)
            {
              sc += 1000;
              if (tx and c->ty and c->ty->el)
              {
                int ts = tysc(c->ty->el, tx, 0);
                sc += ts;
              }
              for (uint32_t k = 0; k < b->bd.sp->sp.pmm.n and k < (uint32_t) na; k++)
              {
                Syntax_Node *p = b->bd.sp->sp.pmm.d[k];
                if (p->sy and p->sy->ty and tx)
                {
                  int ps = tysc(p->sy->ty, tx, 0);
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
            int ts = tysc(c->ty, tx, 0);
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
  return br ?: cv.d[0];
}
static Type_Info *tyn(Tk_ k, String_Slice nm)
{
  Type_Info *t = arena_allocate(sizeof(Type_Info));
  t->k = k;
  t->nm = string_duplicate(nm);
  t->sz = 8;
  t->al = 8;
  return t;
}
static Type_Info *TY_INT, *TY_BOOL, *TY_CHAR, *TY_STR, *TY_FLT, *TY_UINT, *TY_UFLT, *TY_FILE,
    *TY_NAT, *TY_POS;
static void smi(Symbol_Manager *SM)
{
  memset(SM, 0, sizeof(*SM));
  TY_INT = tyn(TYPE_INTEGER, STRING_LITERAL("INTEGER"));
  TY_INT->lo = -2147483648LL;
  TY_INT->hi = 2147483647LL;
  TY_NAT = tyn(TYPE_INTEGER, STRING_LITERAL("NATURAL"));
  TY_NAT->lo = 0;
  TY_NAT->hi = 2147483647LL;
  TY_POS = tyn(TYPE_INTEGER, STRING_LITERAL("POSITIVE"));
  TY_POS->lo = 1;
  TY_POS->hi = 2147483647LL;
  TY_BOOL = tyn(TYPE_BOOLEAN, STRING_LITERAL("BOOLEAN"));
  TY_CHAR = tyn(TYPE_CHARACTER, STRING_LITERAL("CHARACTER"));
  TY_CHAR->sz = 1;
  TY_STR = tyn(TYPE_ARRAY, STRING_LITERAL("STRING"));
  TY_STR->el = TY_CHAR;
  TY_STR->lo = 0;
  TY_STR->hi = -1;
  TY_STR->ix = TY_POS;
  TY_FLT = tyn(TYPE_FLOAT, STRING_LITERAL("FLOAT"));
  TY_UINT = tyn(TYPE_UNSIGNED_INTEGER, STRING_LITERAL("universal_integer"));
  TY_UFLT = tyn(TYPE_UNIVERSAL_FLOAT, STRING_LITERAL("universal_real"));
  TY_FILE = tyn(TYPE_FAT_POINTER, STRING_LITERAL("FILE_TYPE"));
  symbol_add_overload(SM, syn(STRING_LITERAL("INTEGER"), 1, TY_INT, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("NATURAL"), 1, TY_NAT, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("POSITIVE"), 1, TY_POS, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("BOOLEAN"), 1, TY_BOOL, 0));
  Symbol *st = symbol_add_overload(SM, syn(STRING_LITERAL("TRUE"), 2, TY_BOOL, 0));
  st->vl = 1;
  sv(&TY_BOOL->ev, st);
  Symbol *sf = symbol_add_overload(SM, syn(STRING_LITERAL("FALSE"), 2, TY_BOOL, 0));
  sf->vl = 0;
  sv(&TY_BOOL->ev, sf);
  symbol_add_overload(SM, syn(STRING_LITERAL("CHARACTER"), 1, TY_CHAR, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("STRING"), 1, TY_STR, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("FLOAT"), 1, TY_FLT, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("FILE_TYPE"), 1, TY_FILE, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("CONSTRAINT_ERROR"), 3, 0, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("PROGRAM_ERROR"), 3, 0, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("STORAGE_ERROR"), 3, 0, 0));
  symbol_add_overload(SM, syn(STRING_LITERAL("TASKING_ERROR"), 3, 0, 0));
  fv(&SM->io, stdin);
  fv(&SM->io, stdout);
  fv(&SM->io, stderr);
  SM->fn = 3;
}
static Syntax_Node *geq(Type_Info *t, Source_Location l)
{
  Syntax_Node *f = ND(FB, l);
  f->bd.sp = ND(FS, l);
  char b[256];
  snprintf(b, 256, "Oeq%.*s", (int) t->nm.n, t->nm.s);
  f->bd.sp->sp.nm = string_duplicate((String_Slice){b, strlen(b)});
  f->bd.sp->sp.op = STRING_LITERAL("=");
  Syntax_Node *p1 = ND(PM, l);
  p1->pm.nm = STRING_LITERAL("Source_Location");
  p1->pm.ty = ND(ID, l);
  p1->pm.ty->s = t->nm;
  p1->pm.md = 0;
  Syntax_Node *p2 = ND(PM, l);
  p2->pm.nm = STRING_LITERAL("Rational_Number");
  p2->pm.ty = ND(ID, l);
  p2->pm.ty->s = t->nm;
  p2->pm.md = 0;
  nv(&f->bd.sp->sp.pmm, p1);
  nv(&f->bd.sp->sp.pmm, p2);
  f->bd.sp->sp.rt = ND(ID, l);
  f->bd.sp->sp.rt->s = STRING_LITERAL("BOOLEAN");
  Syntax_Node *s = ND(RT, l);
  s->rt.vl = ND(BIN, l);
  s->rt.vl->bn.op = T_EQ;
  if (t->k == TYPE_RECORD)
  {
    s->rt.vl->bn.l = ND(BIN, l);
    s->rt.vl->bn.l->bn.op = T_AND;
    for (uint32_t i = 0; i < t->cm.n; i++)
    {
      Syntax_Node *c = t->cm.d[i];
      if (c->k != N_CM)
        continue;
      Syntax_Node *cmp = ND(BIN, l);
      cmp->bn.op = T_EQ;
      Syntax_Node *lf = ND(SEL, l);
      lf->se.p = ND(ID, l);
      lf->se.p->s = STRING_LITERAL("Source_Location");
      lf->se.se = c->cm.nm;
      Syntax_Node *rf = ND(SEL, l);
      rf->se.p = ND(ID, l);
      rf->se.p->s = STRING_LITERAL("Rational_Number");
      rf->se.se = c->cm.nm;
      cmp->bn.l = lf;
      cmp->bn.r = rf;
      if (not i)
        s->rt.vl->bn.l = cmp;
      else
      {
        Syntax_Node *a = ND(BIN, l);
        a->bn.op = T_AND;
        a->bn.l = s->rt.vl->bn.l;
        a->bn.r = cmp;
        s->rt.vl->bn.l = a;
      }
    }
  }
  else if (t->k == TYPE_ARRAY)
  {
    Syntax_Node *lp = ND(LP, l);
    lp->lp.it = ND(BIN, l);
    lp->lp.it->bn.op = T_IN;
    lp->lp.it->bn.l = ND(ID, l);
    lp->lp.it->bn.l->s = STRING_LITERAL("I");
    lp->lp.it->bn.r = ND(AT, l);
    lp->lp.it->bn.r->at.p = ND(ID, l);
    lp->lp.it->bn.r->at.p->s = STRING_LITERAL("Source_Location");
    lp->lp.it->bn.r->at.at = STRING_LITERAL("RANGE");
    Syntax_Node *cmp = ND(BIN, l);
    cmp->bn.op = T_NE;
    Syntax_Node *li = ND(IX, l);
    li->ix.p = ND(ID, l);
    li->ix.p->s = STRING_LITERAL("Source_Location");
    nv(&li->ix.ix, ND(ID, l));
    li->ix.ix.d[0]->s = STRING_LITERAL("I");
    Syntax_Node *ri = ND(IX, l);
    ri->ix.p = ND(ID, l);
    ri->ix.p->s = STRING_LITERAL("Rational_Number");
    nv(&ri->ix.ix, ND(ID, l));
    ri->ix.ix.d[0]->s = STRING_LITERAL("I");
    cmp->bn.l = li;
    cmp->bn.r = ri;
    Syntax_Node *rt = ND(RT, l);
    rt->rt.vl = ND(ID, l);
    rt->rt.vl->s = STRING_LITERAL("FALSE");
    Syntax_Node *ifs = ND(IF, l);
    ifs->if_.cd = cmp;
    nv(&ifs->if_.th, rt);
    nv(&lp->lp.st, ifs);
    nv(&f->bd.st, lp);
    s->rt.vl = ND(ID, l);
    s->rt.vl->s = STRING_LITERAL("TRUE");
  }
  nv(&f->bd.st, s);
  return f;
}
static Syntax_Node *gas(Type_Info *t, Source_Location l)
{
  Syntax_Node *p = ND(PB, l);
  p->bd.sp = ND(PS, l);
  char b[256];
  snprintf(b, 256, "Oas%.*s", (int) t->nm.n, t->nm.s);
  p->bd.sp->sp.nm = string_duplicate((String_Slice){b, strlen(b)});
  p->bd.sp->sp.op = STRING_LITERAL(":=");
  Syntax_Node *p1 = ND(PM, l);
  p1->pm.nm = STRING_LITERAL("T");
  p1->pm.ty = ND(ID, l);
  p1->pm.ty->s = t->nm;
  p1->pm.md = 3;
  Syntax_Node *p2 = ND(PM, l);
  p2->pm.nm = STRING_LITERAL("String_Slice");
  p2->pm.ty = ND(ID, l);
  p2->pm.ty->s = t->nm;
  p2->pm.md = 0;
  nv(&p->bd.sp->sp.pmm, p1);
  nv(&p->bd.sp->sp.pmm, p2);
  if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->cm.n; i++)
    {
      Syntax_Node *c = t->cm.d[i];
      if (c->k != N_CM)
        continue;
      Syntax_Node *as = ND(AS, l);
      Syntax_Node *lt = ND(SEL, l);
      lt->se.p = ND(ID, l);
      lt->se.p->s = STRING_LITERAL("T");
      lt->se.se = c->cm.nm;
      Syntax_Node *rs = ND(SEL, l);
      rs->se.p = ND(ID, l);
      rs->se.p->s = STRING_LITERAL("String_Slice");
      rs->se.se = c->cm.nm;
      as->as.tg = lt;
      as->as.vl = rs;
      nv(&p->bd.st, as);
    }
  }
  else if (t->k == TYPE_ARRAY)
  {
    Syntax_Node *lp = ND(LP, l);
    lp->lp.it = ND(BIN, l);
    lp->lp.it->bn.op = T_IN;
    lp->lp.it->bn.l = ND(ID, l);
    lp->lp.it->bn.l->s = STRING_LITERAL("I");
    lp->lp.it->bn.r = ND(AT, l);
    lp->lp.it->bn.r->at.p = ND(ID, l);
    lp->lp.it->bn.r->at.p->s = STRING_LITERAL("T");
    lp->lp.it->bn.r->at.at = STRING_LITERAL("RANGE");
    Syntax_Node *as = ND(AS, l);
    Syntax_Node *ti = ND(IX, l);
    ti->ix.p = ND(ID, l);
    ti->ix.p->s = STRING_LITERAL("T");
    nv(&ti->ix.ix, ND(ID, l));
    ti->ix.ix.d[0]->s = STRING_LITERAL("I");
    Syntax_Node *si = ND(IX, l);
    si->ix.p = ND(ID, l);
    si->ix.p->s = STRING_LITERAL("String_Slice");
    nv(&si->ix.ix, ND(ID, l));
    si->ix.ix.d[0]->s = STRING_LITERAL("I");
    as->as.tg = ti;
    as->as.vl = si;
    nv(&lp->lp.st, as);
    nv(&p->bd.st, lp);
  }
  return p;
}
static Syntax_Node *gin(Type_Info *t, Source_Location l)
{
  Syntax_Node *f = ND(FB, l);
  f->bd.sp = ND(FS, l);
  char b[256];
  snprintf(b, 256, "Oin%.*s", (int) t->nm.n, t->nm.s);
  f->bd.sp->sp.nm = string_duplicate((String_Slice){b, strlen(b)});
  f->bd.sp->sp.rt = ND(ID, l);
  f->bd.sp->sp.rt->s = t->nm;
  Syntax_Node *ag = ND(AG, l);
  if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->cm.n; i++)
    {
      Syntax_Node *c = t->cm.d[i];
      if (c->k != N_CM or not c->cm.in)
        continue;
      Syntax_Node *a = ND(ASC, l);
      nv(&a->asc.ch, ND(ID, l));
      a->asc.ch.d[0]->s = c->cm.nm;
      a->asc.vl = c->cm.in;
      nv(&ag->ag.it, a);
    }
  }
  Syntax_Node *rt = ND(RT, l);
  rt->rt.vl = ag;
  nv(&f->bd.st, rt);
  return ag->ag.it.n > 0 ? f : 0;
}
static void fty(Symbol_Manager *SM, Type_Info *t, Source_Location l)
{
  if (not t or t->frz)
    return;
  if (t->k == TY_PT and t->prt and not t->prt->frz)
    return;
  t->frz = 1;
  t->fzn = ND(ERR, l);
  if (t->bs and t->bs != t and not t->bs->frz)
    fty(SM, t->bs, l);
  if (t->prt and not t->prt->frz)
    fty(SM, t->prt, l);
  if (t->el and not t->el->frz)
    fty(SM, t->el, l);
  if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->cm.n; i++)
      if (t->cm.d[i]->sy and t->cm.d[i]->sy->ty)
        fty(SM, t->cm.d[i]->sy->ty, l);
    uint32_t of = 0, mx = 1;
    for (uint32_t i = 0; i < t->cm.n; i++)
    {
      Syntax_Node *c = t->cm.d[i];
      if (c->k != N_CM)
        continue;
      Type_Info *ct = c->cm.ty ? c->cm.ty->ty : 0;
      uint32_t ca = ct and ct->al ? ct->al : 8, cs = ct and ct->sz ? ct->sz : 8;
      if (ca > mx)
        mx = ca;
      of = (of + ca - 1) & ~(ca - 1);
      c->cm.of = of;
      of += cs;
    }
    t->sz = (of + mx - 1) & ~(mx - 1);
    t->al = mx;
  }
  if (t->k == TYPE_ARRAY and t->el)
  {
    Type_Info *et = t->el;
    uint32_t ea = et->al ? et->al : 8, es = et->sz ? et->sz : 8;
    int64_t n = (t->hi - t->lo + 1);
    t->sz = n > 0 ? n * es : 0;
    t->al = ea;
  }
  if ((t->k == TYPE_RECORD or t->k == TYPE_ARRAY) and t->nm.s and t->nm.n)
  {
    Syntax_Node *eq = geq(t, l);
    if (eq)
      nv(&t->ops, eq);
    Syntax_Node *as = gas(t, l);
    if (as)
      nv(&t->ops, as);
    Syntax_Node *in = gin(t, l);
    if (in)
      nv(&t->ops, in);
  }
}
static void fsy(Symbol_Manager *SM, Symbol *s, Source_Location l)
{
  if (not s or s->frz)
    return;
  s->frz = 1;
  s->fzn = ND(ERR, l);
  if (s->ty and not s->ty->frz)
    fty(SM, s->ty, l);
}
static void fal(Symbol_Manager *SM, Source_Location l)
{
  for (int i = 0; i < 4096; i++)
    for (Symbol *s = SM->sy[i]; s; s = s->nx)
      if (s->sc == SM->sc and not s->frz)
      {
        if (s->ty and s->ty->k == TY_PT and s->ty->prt and not s->ty->prt->frz)
          continue;
        if (s->ty)
          fty(SM, s->ty, l);
        fsy(SM, s, l);
      }
}
static void scp(Symbol_Manager *SM)
{
  SM->sc++;
  SM->ss++;
  if (SM->ssd < 256)
  {
    int m = SM->ssd;
    SM->ssd++;
    SM->sst[m] = 0;
  }
}
static void sco(Symbol_Manager *SM)
{
  fal(SM, (Source_Location){0, 0, ""});
  for (int i = 0; i < 4096; i++)
    for (Symbol *s = SM->sy[i]; s; s = s->nx)
    {
      if (s->sc == SM->sc)
      {
        s->vis &= ~1;
        if (s->k == 6)
          s->vis = 3;
      }
      if (s->vis & 2 and s->pr and s->pr->sc >= SM->sc)
        s->vis &= ~2;
    }
  if (SM->ssd > 0)
    SM->ssd--;
  SM->sc--;
}
static Type_Info *resolve_subtype(Symbol_Manager *SM, Syntax_Node *n);
static void resolve_expression(Symbol_Manager *SM, Syntax_Node *n, Type_Info *tx);
static void resolve_statement_sequence(Symbol_Manager *SM, Syntax_Node *n);
static void resolve_declaration(Symbol_Manager *SM, Syntax_Node *n);
static void rrc_(Symbol_Manager *SM, RC *r);
static Syntax_Node *gcl(Symbol_Manager *SM, Syntax_Node *n);
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
  if ((t->k == TYPE_DERIVED or t->k == TY_PT) and t->prt)
    return type_canonical_concrete(t->prt);
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
  for (Type_Info *p = t; p; p = p->bs ? p->bs : p->prt)
  {
    if (not p->bs and not p->prt)
      return p;
    if (p->k == TYPE_DERIVED and p->prt)
      return semantic_base(p->prt);
    if (p->k == TYPE_UNSIGNED_INTEGER)
      return TY_INT;
    if (p->k == TYPE_UNIVERSAL_FLOAT or p->k == TYPE_FIXED_POINT)
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
  for (Type_Info *p = t; p; p = p->bs)
  {
    if (p->sup & kind)
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
  if (a == TY_STR and b->k == TYPE_ARRAY and b->el and b->el->k == TYPE_CHARACTER)
    return COMP_ARRAY_ELEMENT;
  if (b == TY_STR and a->k == TYPE_ARRAY and a->el and a->el->k == TYPE_CHARACTER)
    return COMP_ARRAY_ELEMENT;
  if (a->prt == b or b->prt == a)
    return COMP_DERIVED;
  if (a->bs == b or b->bs == a)
    return COMP_BASED_ON;
  if ((a->k == TYPE_INTEGER or a->k == TYPE_UNSIGNED_INTEGER)
      and (b->k == TYPE_INTEGER or b->k == TYPE_UNSIGNED_INTEGER))
    return COMP_SAME;
  if ((a->k == TYPE_FLOAT or a->k == TYPE_UNIVERSAL_FLOAT)
      and (b->k == TYPE_FLOAT or b->k == TYPE_UNIVERSAL_FLOAT))
    return COMP_SAME;
  if (a->k == TYPE_ARRAY and b->k == TYPE_ARRAY and type_compat_kind(a->el, b->el) != COMP_NONE)
    return COMP_ARRAY_ELEMENT;
  if (a->k == TYPE_ACCESS and b->k == TYPE_ACCESS)
    return type_compat_kind(a->el, b->el) != COMP_NONE ? COMP_ACCESS_DESIGNATED : COMP_NONE;
  if (a->k == TYPE_DERIVED)
    return type_compat_kind(a->prt, b);
  if (b->k == TYPE_DERIVED)
    return type_compat_kind(a, b->prt);
  return COMP_NONE;
}
static int tysc(Type_Info *a, Type_Info *b, Type_Info *tx)
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
    return 600 + tysc(a->el, b->el, tx);
  case COMP_ACCESS_DESIGNATED:
    return 500 + tysc(a->el, b->el, 0);
  default:
    break;
  }
  if (tx and a and a->el and b == tx)
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
static Type_Info *resolve_subtype(Symbol_Manager *SM, Syntax_Node *n)
{
  if (not n)
    return TY_INT;
  if (n->k == N_ID)
  {
    Symbol *s = symbol_find(SM, n->s);
    if (s and s->ty)
      return s->ty;
    return TY_INT;
  }
  if (n->k == N_SEL)
  {
    Syntax_Node *p = n->se.p;
    if (p->k == N_ID)
    {
      Symbol *ps = symbol_find(SM, p->s);
      if (ps and ps->k == 6 and ps->df and ps->df->k == N_PKS)
      {
        Syntax_Node *pk = ps->df;
        for (uint32_t i = 0; i < pk->ps.pr.n; i++)
        {
          Syntax_Node *d = pk->ps.pr.d[i];
          if (d->sy and string_equal_ignore_case(d->sy->nm, n->se.se) and d->sy->ty)
            return d->sy->ty;
          if (d->k == N_TD and string_equal_ignore_case(d->td.nm, n->se.se))
            return resolve_subtype(SM, d->td.df);
        }
        for (uint32_t i = 0; i < pk->ps.dc.n; i++)
        {
          Syntax_Node *d = pk->ps.dc.d[i];
          if (d->sy and string_equal_ignore_case(d->sy->nm, n->se.se) and d->sy->ty)
            return d->sy->ty;
          if (d->k == N_TD and string_equal_ignore_case(d->td.nm, n->se.se))
            return resolve_subtype(SM, d->td.df);
        }
      }
      return resolve_subtype(SM, p);
    }
    return TY_INT;
  }
  if (n->k == N_ST)
  {
    Type_Info *bt = resolve_subtype(SM, n->sd.in);
    Syntax_Node *cn = n->sd.cn ? n->sd.cn : n->sd.rn;
    if (cn and bt)
    {
      Type_Info *t = tyn(bt->k, N);
      t->bs = bt;
      t->el = bt->el;
      t->cm = bt->cm;
      t->dc = bt->dc;
      t->sz = bt->sz;
      t->al = bt->al;
      t->ad = bt->ad;
      t->pk = bt->pk;
      t->ix = bt->ix;
      if (cn->k == 27 and cn->cn.cs.n > 0 and cn->cn.cs.d[0] and cn->cn.cs.d[0]->k == 26)
      {
        Syntax_Node *rn = cn->cn.cs.d[0];
        resolve_expression(SM, rn->rn.lo, 0);
        resolve_expression(SM, rn->rn.hi, 0);
        Syntax_Node *lo = rn->rn.lo;
        Syntax_Node *hi = rn->rn.hi;
        int64_t lov = lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_INT ? -lo->un.x->i
                      : lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -lo->un.x->f})
                                .i
                      : lo->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = lo->f})
                                              .i
                      : lo->k == N_ID and lo->sy and lo->sy->k == 2 ? lo->sy->vl
                                                                    : lo->i;
        int64_t hiv = hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_INT ? -hi->un.x->i
                      : hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -hi->un.x->f})
                                .i
                      : hi->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = hi->f})
                                              .i
                      : hi->k == N_ID and hi->sy and hi->sy->k == 2 ? hi->sy->vl
                                                                    : hi->i;
        t->lo = lov;
        t->hi = hiv;
        return t;
      }
      else if (cn->k == 27 and cn->cn.rn)
      {
        resolve_expression(SM, cn->cn.rn->rn.lo, 0);
        resolve_expression(SM, cn->cn.rn->rn.hi, 0);
        Syntax_Node *lo = cn->cn.rn->rn.lo;
        Syntax_Node *hi = cn->cn.rn->rn.hi;
        int64_t lov = lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_INT ? -lo->un.x->i
                      : lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -lo->un.x->f})
                                .i
                      : lo->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = lo->f})
                                              .i
                      : lo->k == N_ID and lo->sy and lo->sy->k == 2 ? lo->sy->vl
                                                                    : lo->i;
        int64_t hiv = hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_INT ? -hi->un.x->i
                      : hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -hi->un.x->f})
                                .i
                      : hi->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = hi->f})
                                              .i
                      : hi->k == N_ID and hi->sy and hi->sy->k == 2 ? hi->sy->vl
                                                                    : hi->i;
        t->lo = lov;
        t->hi = hiv;
        return t;
      }
      else if (cn->k == N_RN)
      {
        resolve_expression(SM, cn->rn.lo, 0);
        resolve_expression(SM, cn->rn.hi, 0);
        Syntax_Node *lo = cn->rn.lo;
        Syntax_Node *hi = cn->rn.hi;
        int64_t lov = lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_INT ? -lo->un.x->i
                      : lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -lo->un.x->f})
                                .i
                      : lo->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = lo->f})
                                              .i
                      : lo->k == N_ID and lo->sy and lo->sy->k == 2 ? lo->sy->vl
                                                                    : lo->i;
        int64_t hiv = hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_INT ? -hi->un.x->i
                      : hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_REAL
                          ? ((union {
                              double d;
                              int64_t i;
                            }){.d = -hi->un.x->f})
                                .i
                      : hi->k == N_REAL ? ((union {
                                            double d;
                                            int64_t i;
                                          }){.d = hi->f})
                                              .i
                      : hi->k == N_ID and hi->sy and hi->sy->k == 2 ? hi->sy->vl
                                                                    : hi->i;
        t->lo = lov;
        t->hi = hiv;
        return t;
      }
    }
    return bt;
  }
  if (n->k == N_TI)
  {
    resolve_expression(SM, n->rn.lo, 0);
    resolve_expression(SM, n->rn.hi, 0);
    Type_Info *t = tyn(TYPE_INTEGER, N);
    if (n->rn.lo and n->rn.lo->k == N_INT)
      t->lo = n->rn.lo->i;
    else if (
        n->rn.lo and n->rn.lo->k == N_UN and n->rn.lo->un.op == T_MN and n->rn.lo->un.x->k == N_INT)
      t->lo = -n->rn.lo->un.x->i;
    if (n->rn.hi and n->rn.hi->k == N_INT)
      t->hi = n->rn.hi->i;
    else if (
        n->rn.hi and n->rn.hi->k == N_UN and n->rn.hi->un.op == T_MN and n->rn.hi->un.x->k == N_INT)
      t->hi = -n->rn.hi->un.x->i;
    return t;
  }
  if (n->k == N_TX)
  {
    Type_Info *t = tyn(TYPE_FIXED_POINT, N);
    double d = 1.0;
    if (n->rn.lo and n->rn.lo->k == N_REAL)
      d = n->rn.lo->f;
    else if (n->rn.lo and n->rn.lo->k == N_INT)
      d = n->rn.lo->i;
    t->sm = (int64_t) (1.0 / d);
    if (n->rn.hi and n->rn.hi->k == N_INT)
      t->lo = n->rn.hi->i;
    if (n->bn.r and n->bn.r->k == N_INT)
      t->hi = n->bn.r->i;
    return t;
  }
  if (n->k == N_TE)
    return tyn(TYPE_INTEGER, N);
  if (n->k == N_TF)
  {
    Type_Info *t = tyn(TYPE_FLOAT, N);
    if (n->un.x)
    {
      resolve_expression(SM, n->un.x, 0);
      if (n->un.x->k == N_INT)
        t->sm = n->un.x->i;
    }
    return t;
  }
  if (n->k == N_TA)
  {
    Type_Info *t = tyn(TYPE_ARRAY, N);
    t->el = resolve_subtype(SM, n->ix.p);
    if (n->ix.ix.n == 1)
    {
      Syntax_Node *r = n->ix.ix.d[0];
      if (r and r->k == N_RN)
      {
        resolve_expression(SM, r->rn.lo, 0);
        resolve_expression(SM, r->rn.hi, 0);
        Syntax_Node *lo = r->rn.lo;
        Syntax_Node *hi = r->rn.hi;
        if (lo and lo->k == N_INT)
          t->lo = lo->i;
        else if (lo and lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_INT)
          t->lo = -lo->un.x->i;
        if (hi and hi->k == N_INT)
          t->hi = hi->i;
        else if (hi and hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_INT)
          t->hi = -hi->un.x->i;
      }
    }
    return t;
  }
  if (n->k == N_TR)
    return tyn(TYPE_RECORD, N);
  if (n->k == N_TP)
    return tyn(TY_PT, N);
  if (n->k == N_TAC)
  {
    Type_Info *t = tyn(TYPE_ACCESS, N);
    t->el = resolve_subtype(SM, n->un.x);
    return t;
  }
  if (n->k == N_IX)
  {
    Type_Info *bt = resolve_subtype(SM, n->ix.p);
    if (bt and bt->k == TYPE_ARRAY and bt->lo == 0 and bt->hi == -1 and n->ix.ix.n == 1)
    {
      Syntax_Node *r = n->ix.ix.d[0];
      if (r and r->k == N_RN)
      {
        resolve_expression(SM, r->rn.lo, 0);
        resolve_expression(SM, r->rn.hi, 0);
        Type_Info *t = tyn(TYPE_ARRAY, N);
        t->el = bt->el;
        t->ix = bt->ix;
        t->bs = bt;
        if (r->rn.lo and r->rn.lo->k == N_INT)
          t->lo = r->rn.lo->i;
        else if (
            r->rn.lo and r->rn.lo->k == N_UN and r->rn.lo->un.op == T_MN
            and r->rn.lo->un.x->k == N_INT)
          t->lo = -r->rn.lo->un.x->i;
        if (r->rn.hi and r->rn.hi->k == N_INT)
          t->hi = r->rn.hi->i;
        else if (
            r->rn.hi and r->rn.hi->k == N_UN and r->rn.hi->un.op == T_MN
            and r->rn.hi->un.x->k == N_INT)
          t->hi = -r->rn.hi->un.x->i;
        return t;
      }
    }
    return bt;
  }
  if (n->k == N_RN)
  {
    resolve_expression(SM, n->rn.lo, 0);
    resolve_expression(SM, n->rn.hi, 0);
    Type_Info *t = tyn(TYPE_INTEGER, N);
    if (n->rn.lo and n->rn.lo->k == N_INT)
      t->lo = n->rn.lo->i;
    else if (
        n->rn.lo and n->rn.lo->k == N_UN and n->rn.lo->un.op == T_MN and n->rn.lo->un.x->k == N_INT)
      t->lo = -n->rn.lo->un.x->i;
    if (n->rn.hi and n->rn.hi->k == N_INT)
      t->hi = n->rn.hi->i;
    else if (
        n->rn.hi and n->rn.hi->k == N_UN and n->rn.hi->un.op == T_MN and n->rn.hi->un.x->k == N_INT)
      t->hi = -n->rn.hi->un.x->i;
    return t;
  }
  if (n->k == N_CL)
  {
    Type_Info *bt = resolve_subtype(SM, n->cl.fn);
    if (bt and bt->k == TYPE_ARRAY and bt->lo == 0 and bt->hi == -1 and n->cl.ar.n == 1)
    {
      Syntax_Node *r = n->cl.ar.d[0];
      if (r and r->k == N_RN)
      {
        resolve_expression(SM, r->rn.lo, 0);
        resolve_expression(SM, r->rn.hi, 0);
        Type_Info *t = tyn(TYPE_ARRAY, N);
        t->el = bt->el;
        t->ix = bt->ix;
        t->bs = bt;
        if (r->rn.lo and r->rn.lo->k == N_INT)
          t->lo = r->rn.lo->i;
        else if (
            r->rn.lo and r->rn.lo->k == N_UN and r->rn.lo->un.op == T_MN
            and r->rn.lo->un.x->k == N_INT)
          t->lo = -r->rn.lo->un.x->i;
        if (r->rn.hi and r->rn.hi->k == N_INT)
          t->hi = r->rn.hi->i;
        else if (
            r->rn.hi and r->rn.hi->k == N_UN and r->rn.hi->un.op == T_MN
            and r->rn.hi->un.x->k == N_INT)
          t->hi = -r->rn.hi->un.x->i;
        return t;
      }
    }
    return bt;
  }
  return TY_INT;
}
static Symbol *scl(Symbol_Manager *SM, char c, Type_Info *tx)
{
  if (tx and tx->k == TYPE_ENUMERATION)
  {
    for (uint32_t i = 0; i < tx->ev.n; i++)
    {
      Symbol *e = tx->ev.d[i];
      if (e->nm.n == 1 and tolower(e->nm.s[0]) == tolower(c))
        return e;
    }
  }
  if (tx and tx->k == TYPE_DERIVED and tx->prt)
    return scl(SM, c, tx->prt);
  for (Symbol *s = SM->sy[syh((String_Slice){&c, 1})]; s; s = s->nx)
    if (s->nm.n == 1 and tolower(s->nm.s[0]) == tolower(c) and s->k == 2 and s->ty
        and (s->ty->k == TYPE_ENUMERATION or (s->ty->k == TYPE_DERIVED and s->ty->prt and s->ty->prt->k == TYPE_ENUMERATION)))
      return s;
  return 0;
}
static inline Syntax_Node *mk_chk(Syntax_Node *ex, String_Slice ec, Source_Location l)
{
  Syntax_Node *c = ND(CHK, l);
  c->chk.ex = ex;
  c->chk.ec = ec;
  c->ty = ex->ty;
  return c;
}
static inline bool is_unconstrained_array(Type_Info *t)
{
  return t and t->k == TYPE_ARRAY and t->lo == 0 and t->hi == -1;
}
static Type_Info *base_scalar(Type_Info *t)
{
  if (not t)
    return TY_INT;
  for (Type_Info *p = t; p; p = p->bs)
    if (not p->bs
        or (p->k != TYPE_INTEGER and p->k != TYPE_ENUMERATION and p->k != TYPE_DERIVED
            and p->k != TYPE_CHARACTER and p->k != TYPE_FLOAT))
      return p;
  return t;
}
static inline bool is_unc_scl(Type_Info *t)
{
  if (not t or not(is_discrete(t) or is_real_type(t)))
    return 0;
  Type_Info *b = base_scalar(t);
  return t->lo == b->lo and t->hi == b->hi;
}
static inline double ty_bd_d(int64_t b)
{
  union
  {
    int64_t i;
    double d;
  } u;
  u.i = b;
  return u.d;
}
static bool dsc_cf(Type_Info *t, Type_Info *s)
{
  if (not t or not s or t->dc.n == 0 or s->dc.n == 0)
    return 0;
  uint32_t n = t->dc.n < s->dc.n ? t->dc.n : s->dc.n;
  for (uint32_t i = 0; i < n; i++)
  {
    Syntax_Node *ad = t->dc.d[i], *bd = s->dc.d[i];
    if (not(ad and bd and ad->k == N_DS and bd->k == N_DS and ad->pm.df and bd->pm.df
            and ad->pm.df->k == N_INT and bd->pm.df->k == N_INT))
      continue;
    if (ad->pm.df->i != bd->pm.df->i)
      return 1;
  }
  return 0;
}
static Syntax_Node *chk(Symbol_Manager *SM, Syntax_Node *n, Source_Location l)
{
  (void) SM;
  if (not n or not n->ty)
    return n;
  Type_Info *t = type_canonical_concrete(n->ty);
  if ((is_discrete(t) or is_real_type(t)) and (n->ty->lo != TY_INT->lo or n->ty->hi != TY_INT->hi)
      and not is_check_suppressed(n->ty, CHK_RNG))
    return mk_chk(n, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  if (t->k == TYPE_RECORD and dsc_cf(t, n->ty) and not is_check_suppressed(t, CHK_DSC))
    return mk_chk(n, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  if (t->k == TYPE_ARRAY and n->ty and n->ty->k == TYPE_ARRAY and n->ty->ix
      and (n->ty->lo < n->ty->ix->lo or n->ty->hi > n->ty->ix->hi))
    return mk_chk(n, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  if (t->k == TYPE_ARRAY and n->ty and n->ty->k == TYPE_ARRAY and not is_unconstrained_array(t)
      and not is_check_suppressed(t, CHK_IDX) and (t->lo != n->ty->lo or t->hi != n->ty->hi))
    return mk_chk(n, STRING_LITERAL("CONSTRAINT_ERROR"), l);
  return n;
}
static inline int64_t range_size(int64_t lo, int64_t hi)
{
  return hi >= lo ? hi - lo + 1 : 0;
}
static inline bool is_static(Syntax_Node *n)
{
  return n
         and (n->k == N_INT or (n->k == N_UN and n->un.op == T_MN and n->un.x and n->un.x->k == N_INT));
}
static int foth(Syntax_Node *ag)
{
  if (not ag or ag->k != N_AG)
    return -1;
  for (uint32_t i = 0; i < ag->ag.it.n; i++)
  {
    Syntax_Node *e = ag->ag.it.d[i];
    if (e->k == N_ASC and e->asc.ch.n == 1 and e->asc.ch.d[0]->k == N_ID
        and string_equal_ignore_case(e->asc.ch.d[0]->s, STRING_LITERAL("others")))
      return i;
  }
  return -1;
}
static void normalize_array_aggregate(Symbol_Manager *SM, Type_Info *at, Syntax_Node *ag)
{
  (void) SM;
  if (not ag or not at or at->k != TYPE_ARRAY)
    return;
  int64_t asz = range_size(at->lo, at->hi);
  if (asz > 4096)
    return;
  Node_Vector xv = {0};
  bool *cov = calloc(asz, 1);
  int oi = foth(ag);
  uint32_t px = 0;
  for (uint32_t i = 0; i < ag->ag.it.n; i++)
  {
    if ((int) i == oi)
      continue;
    Syntax_Node *e = ag->ag.it.d[i];
    if (e->k == N_ASC)
    {
      for (uint32_t j = 0; j < e->asc.ch.n; j++)
      {
        Syntax_Node *ch = e->asc.ch.d[j];
        int64_t idx = -1;
        if (ch->k == N_INT)
          idx = ch->i - at->lo;
        else if (ch->k == N_RN)
        {
          for (int64_t k = ch->rn.lo->i; k <= ch->rn.hi->i; k++)
          {
            int64_t ridx = k - at->lo;
            if (ridx >= 0 and ridx < asz)
            {
              if (cov[ridx] and error_count < 99)
                fatal_error(ag->l, "dup ag");
              cov[ridx] = 1;
              while (xv.n <= (uint32_t) ridx)
                nv(&xv, ND(INT, ag->l));
              xv.d[ridx] = e->asc.vl;
            }
          }
          continue;
        }
        if (idx >= 0 and idx < asz)
        {
          if (cov[idx] and error_count < 99)
            fatal_error(ag->l, "dup ag");
          cov[idx] = 1;
          while (xv.n <= (uint32_t) idx)
            nv(&xv, ND(INT, ag->l));
          xv.d[idx] = e->asc.vl;
        }
      }
    }
    else
    {
      if (px < (uint32_t) asz)
      {
        if (cov[px] and error_count < 99)
          fatal_error(ag->l, "dup ag");
        cov[px] = 1;
        while (xv.n <= px)
          nv(&xv, ND(INT, ag->l));
        xv.d[px] = e;
      }
      px++;
    }
  }
  if (oi >= 0)
  {
    Syntax_Node *oe = ag->ag.it.d[oi];
    for (int64_t i = 0; i < asz; i++)
      if (not cov[i])
      {
        while (xv.n <= (uint32_t) i)
          nv(&xv, ND(INT, ag->l));
        xv.d[i] = oe->asc.vl;
        cov[i] = 1;
      }
  }
  for (int64_t i = 0; i < asz; i++)
    if (not cov[i] and error_count < 99)
      fatal_error(ag->l, "ag gap %lld", i);
  ag->ag.it = xv;
  free(cov);
}
static void normalize_record_aggregate(Symbol_Manager *SM, Type_Info *rt, Syntax_Node *ag)
{
  (void) SM;
  if (not ag or not rt or rt->k != TYPE_RECORD)
    return;
  bool cov[256] = {0};
  for (uint32_t i = 0; i < ag->ag.it.n; i++)
  {
    Syntax_Node *e = ag->ag.it.d[i];
    if (e->k != N_ASC)
      continue;
    for (uint32_t j = 0; j < e->asc.ch.n; j++)
    {
      Syntax_Node *ch = e->asc.ch.d[j];
      if (ch->k == N_ID)
      {
        if (string_equal_ignore_case(ch->s, STRING_LITERAL("others")))
        {
          for (uint32_t k = 0; k < rt->cm.n; k++)
            if (not cov[k])
              cov[k] = 1;
          continue;
        }
        for (uint32_t k = 0; k < rt->cm.n; k++)
        {
          Syntax_Node *c = rt->cm.d[k];
          if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, ch->s))
          {
            if (cov[c->cm.of] and error_count < 99)
              fatal_error(ag->l, "dup cm");
            cov[c->cm.of] = 1;
            break;
          }
        }
      }
    }
  }
}
static Type_Info *ucag(Type_Info *at, Syntax_Node *ag)
{
  if (not at or not ag or at->k != TYPE_ARRAY or ag->k != N_AG)
    return at;
  if (at->lo != 0 or at->hi != -1)
    return at;
  int asz = ag->ag.it.n;
  Type_Info *nt = tyn(TYPE_ARRAY, N);
  nt->el = at->el;
  nt->ix = at->ix;
  nt->lo = 1;
  nt->hi = asz;
  return nt;
}
static void icv(Type_Info *t, Syntax_Node *n)
{
  if (not t or not n)
    return;
  if (n->k == N_CL)
  {
    for (uint32_t i = 0; i < n->cl.ar.n; i++)
      resolve_expression(0, n->cl.ar.d[i], 0);
  }
  else if (n->k == N_AG and t->k == TYPE_ARRAY)
  {
    normalize_array_aggregate(0, type_canonical_concrete(t), n);
  }
  else if (n->k == N_AG and t->k == TYPE_RECORD)
  {
    normalize_record_aggregate(0, type_canonical_concrete(t), n);
  }
}
static bool hrs(Node_Vector *v)
{
  for (uint32_t i = 0; i < v->n; i++)
    if (v->d[i]->k != N_PG)
      return 1;
  return 0;
}
static void resolve_expression(Symbol_Manager *SM, Syntax_Node *n, Type_Info *tx)
{
  if (not n)
    return;
  switch (n->k)
  {
  case N_ID:
  {
    Type_Info *_tx = tx and tx->k == TYPE_DERIVED ? type_canonical_concrete(tx) : tx;
    if (_tx and _tx->k == TYPE_ENUMERATION)
    {
      for (uint32_t i = 0; i < tx->ev.n; i++)
      {
        Symbol *e = tx->ev.d[i];
        if (string_equal_ignore_case(e->nm, n->s))
        {
          n->ty = tx;
          n->sy = e;
          return;
        }
      }
    }
    Symbol *s = symbol_find(SM, n->s);
    if (s)
    {
      n->ty = s->ty;
      n->sy = s;
      if (s->k == 5)
      {
        Symbol *s0 = symbol_find_with_arity(SM, n->s, 0, tx);
        if (s0 and s0->ty and s0->ty->k == TYPE_STRING and s0->ty->el)
        {
          n->ty = s0->ty->el;
          n->sy = s0;
        }
      }
      if (s->k == 2 and s->df)
      {
        if (s->df->k == N_INT)
        {
          n->k = N_INT;
          n->i = s->df->i;
          n->ty = TY_UINT;
        }
        else if (s->df->k == N_REAL)
        {
          n->k = N_REAL;
          n->f = s->df->f;
          n->ty = TY_UFLT;
        }
      }
    }
    else
    {
      if (error_count < 99 and not string_equal_ignore_case(n->s, STRING_LITERAL("others")))
        fatal_error(n->l, "undef '%.*s'", (int) n->s.n, n->s.s);
      n->ty = TY_INT;
    }
  }
  break;
  case N_INT:
    n->ty = TY_UINT;
    break;
  case N_REAL:
    n->ty = TY_UFLT;
    break;
  case N_CHAR:
  {
    Symbol *s = scl(SM, n->i, tx);
    if (s)
    {
      n->ty = s->ty;
      n->sy = s;
      n->k = N_ID;
      n->s = s->nm;
    }
    else
      n->ty = TY_CHAR;
  }
  break;
  case N_STR:
    n->ty =
        tx and (tx->k == TYPE_ARRAY or type_canonical_concrete(tx)->k == TYPE_ARRAY) ? tx : TY_STR;
    break;
  case N_NULL:
    n->ty = tx and tx->k == TYPE_ACCESS ? tx : TY_INT;
    break;
  case N_BIN:
    resolve_expression(SM, n->bn.l, tx);
    resolve_expression(SM, n->bn.r, tx);
    if (n->bn.op == T_ATHN or n->bn.op == T_OREL)
    {
      n->ty = TY_BOOL;
      break;
    }
    if (n->bn.op == T_AND or n->bn.op == T_OR or n->bn.op == T_XOR)
    {
      n->bn.l = chk(SM, n->bn.l, n->l);
      n->bn.r = chk(SM, n->bn.r, n->l);
      Type_Info *lt = n->bn.l->ty ? type_canonical_concrete(n->bn.l->ty) : 0;
      n->ty = lt and lt->k == TYPE_ARRAY ? lt : TY_BOOL;
      break;
    }
    if (n->bn.op == T_IN)
    {
      n->bn.l = chk(SM, n->bn.l, n->l);
      n->bn.r = chk(SM, n->bn.r, n->l);
      n->ty = TY_BOOL;
      break;
    }
    if (n->bn.l->k == N_INT and n->bn.r->k == N_INT
        and (n->bn.op == T_PL or n->bn.op == T_MN or n->bn.op == T_ST or n->bn.op == T_SL or n->bn.op == T_MOD or n->bn.op == T_REM))
    {
      int64_t a = n->bn.l->i, b = n->bn.r->i, r = 0;
      if (n->bn.op == T_PL)
        r = a + b;
      else if (n->bn.op == T_MN)
        r = a - b;
      else if (n->bn.op == T_ST)
        r = a * b;
      else if (n->bn.op == T_SL and b != 0)
        r = a / b;
      else if ((n->bn.op == T_MOD or n->bn.op == T_REM) and b != 0)
        r = a % b;
      n->k = N_INT;
      n->i = r;
      n->ty = TY_UINT;
    }
    else if (
        (n->bn.l->k == N_REAL or n->bn.r->k == N_REAL)
        and (n->bn.op == T_PL or n->bn.op == T_MN or n->bn.op == T_ST or n->bn.op == T_SL or n->bn.op == T_EX))
    {
      double a = n->bn.l->k == N_INT ? (double) n->bn.l->i : n->bn.l->f,
             b = n->bn.r->k == N_INT ? (double) n->bn.r->i : n->bn.r->f, r = 0;
      if (n->bn.op == T_PL)
        r = a + b;
      else if (n->bn.op == T_MN)
        r = a - b;
      else if (n->bn.op == T_ST)
        r = a * b;
      else if (n->bn.op == T_SL and b != 0)
        r = a / b;
      else if (n->bn.op == T_EX)
        r = pow(a, b);
      n->k = N_REAL;
      n->f = r;
      n->ty = TY_UFLT;
    }
    else
    {
      n->ty = type_canonical_concrete(n->bn.l->ty);
    }
    if (n->bn.op >= T_EQ and n->bn.op <= T_GE)
      n->ty = TY_BOOL;
    break;
  case N_UN:
    resolve_expression(SM, n->un.x, tx);
    if (n->un.op == T_MN and n->un.x->k == N_INT)
    {
      n->k = N_INT;
      n->i = -n->un.x->i;
      n->ty = TY_UINT;
    }
    else if (n->un.op == T_MN and n->un.x->k == N_REAL)
    {
      n->k = N_REAL;
      n->f = -n->un.x->f;
      n->ty = TY_UFLT;
    }
    else if (n->un.op == T_PL and (n->un.x->k == N_INT or n->un.x->k == N_REAL))
    {
      n->k = n->un.x->k;
      if (n->k == N_INT)
      {
        n->i = n->un.x->i;
        n->ty = TY_UINT;
      }
      else
      {
        n->f = n->un.x->f;
        n->ty = TY_UFLT;
      }
    }
    else
    {
      n->ty = type_canonical_concrete(n->un.x->ty);
    }
    if (n->un.op == T_NOT)
    {
      Type_Info *xt = n->un.x->ty ? type_canonical_concrete(n->un.x->ty) : 0;
      n->ty = xt and xt->k == TYPE_ARRAY ? xt : TY_BOOL;
    }
    break;
  case N_IX:
    resolve_expression(SM, n->ix.p, 0);
    for (uint32_t i = 0; i < n->ix.ix.n; i++)
    {
      resolve_expression(SM, n->ix.ix.d[i], 0);
      n->ix.ix.d[i] = chk(SM, n->ix.ix.d[i], n->l);
    }
    if (n->ix.p->ty and n->ix.p->ty->k == TYPE_ARRAY)
      n->ty = type_canonical_concrete(n->ix.p->ty->el);
    else
      n->ty = TY_INT;
    break;
  case N_SL:
    resolve_expression(SM, n->sl.p, 0);
    resolve_expression(SM, n->sl.lo, 0);
    resolve_expression(SM, n->sl.hi, 0);
    if (n->sl.p->ty and n->sl.p->ty->k == TYPE_ARRAY)
      n->ty = n->sl.p->ty;
    else
      n->ty = TY_INT;
    break;
  case N_SEL:
  {
    resolve_expression(SM, n->se.p, 0);
    Syntax_Node *p = n->se.p;
    if (p->k == N_ID)
    {
      Symbol *ps = symbol_find(SM, p->s);
      if (ps and ps->k == 6 and ps->df and ps->df->k == N_PKS)
      {
        Syntax_Node *pk = ps->df;
        for (uint32_t i = 0; i < pk->ps.dc.n; i++)
        {
          Syntax_Node *d = pk->ps.dc.d[i];
          if (d->sy and string_equal_ignore_case(d->sy->nm, n->se.se))
          {
            n->ty = d->sy->ty ? d->sy->ty : TY_INT;
            n->sy = d->sy;
            if (d->sy->k == 5 and d->sy->ty and d->sy->ty->k == TYPE_STRING and d->sy->ty->el)
            {
              n->ty = d->sy->ty->el;
            }
            if (d->sy->k == 2 and d->sy->df)
            {
              Syntax_Node *df = d->sy->df;
              if (df->k == N_CHK)
                df = df->chk.ex;
              if (df->k == N_INT)
              {
                n->k = N_INT;
                n->i = df->i;
                n->ty = TY_UINT;
              }
              else if (df->k == N_REAL)
              {
                n->k = N_REAL;
                n->f = df->f;
                n->ty = TY_UFLT;
              }
            }
            return;
          }
          if (d->k == N_ED)
          {
            for (uint32_t j = 0; j < d->ed.id.n; j++)
            {
              Syntax_Node *eid = d->ed.id.d[j];
              if (eid->sy and string_equal_ignore_case(eid->sy->nm, n->se.se))
              {
                n->ty = eid->sy->ty ? eid->sy->ty : TY_INT;
                n->sy = eid->sy;
                return;
              }
            }
          }
          if (d->k == N_OD)
          {
            for (uint32_t j = 0; j < d->od.id.n; j++)
            {
              Syntax_Node *oid = d->od.id.d[j];
              if (oid->sy and string_equal_ignore_case(oid->sy->nm, n->se.se))
              {
                n->ty = oid->sy->ty ? oid->sy->ty : TY_INT;
                n->sy = oid->sy;
                if (oid->sy->k == 2 and oid->sy->df)
                {
                  Syntax_Node *df = oid->sy->df;
                  if (df->k == N_CHK)
                    df = df->chk.ex;
                  if (df->k == N_INT)
                  {
                    n->k = N_INT;
                    n->i = df->i;
                    n->ty = TY_UINT;
                  }
                  else if (df->k == N_REAL)
                  {
                    n->k = N_REAL;
                    n->f = df->f;
                    n->ty = TY_UFLT;
                  }
                }
                return;
              }
            }
          }
        }
        for (uint32_t i = 0; i < pk->ps.dc.n; i++)
        {
          Syntax_Node *d = pk->ps.dc.d[i];
          if (d->k == N_TD and d->sy and d->sy->ty)
          {
            Type_Info *et = d->sy->ty;
            if (et->k == TYPE_ENUMERATION)
            {
              for (uint32_t j = 0; j < et->ev.n; j++)
              {
                Symbol *e = et->ev.d[j];
                if (string_equal_ignore_case(e->nm, n->se.se))
                {
                  n->ty = et;
                  n->sy = e;
                  return;
                }
              }
            }
          }
          if (d->sy and string_equal_ignore_case(d->sy->nm, n->se.se))
          {
            n->ty = d->sy->ty;
            n->sy = d->sy;
            return;
          }
        }
        for (int h = 0; h < 4096; h++)
          for (Symbol *s = SM->sy[h]; s; s = s->nx)
            if (s->pr == ps and string_equal_ignore_case(s->nm, n->se.se))
            {
              n->ty = s->ty;
              n->sy = s;
              if (s->k == 5 and s->ty and s->ty->k == TYPE_STRING and s->ty->el)
              {
                n->ty = s->ty->el;
              }
              if (s->k == 2 and s->df)
              {
                Syntax_Node *df = s->df;
                if (df->k == N_CHK)
                  df = df->chk.ex;
                if (df->k == N_INT)
                {
                  n->k = N_INT;
                  n->i = df->i;
                  n->ty = TY_UINT;
                }
                else if (df->k == N_REAL)
                {
                  n->k = N_REAL;
                  n->f = df->f;
                  n->ty = TY_UFLT;
                }
              }
              return;
            }
        if (error_count < 99)
          fatal_error(n->l, "?'%.*s' in pkg", (int) n->se.se.n, n->se.se.s);
      }
    }
    if (p->ty)
    {
      Type_Info *pt = type_canonical_concrete(p->ty);
      if (pt->k == TYPE_RECORD)
      {
        for (uint32_t i = 0; i < pt->cm.n; i++)
        {
          Syntax_Node *c = pt->cm.d[i];
          if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, n->se.se))
          {
            n->ty = resolve_subtype(SM, c->cm.ty);
            return;
          }
        }
        for (uint32_t i = 0; i < pt->dc.n; i++)
        {
          Syntax_Node *d = pt->dc.d[i];
          if (d->k == N_DS and string_equal_ignore_case(d->pm.nm, n->se.se))
          {
            n->ty = resolve_subtype(SM, d->pm.ty);
            return;
          }
        }
        for (uint32_t i = 0; i < pt->cm.n; i++)
        {
          Syntax_Node *c = pt->cm.d[i];
          if (c->k == N_VP)
          {
            for (uint32_t j = 0; j < c->vp.vrr.n; j++)
            {
              Syntax_Node *v = c->vp.vrr.d[j];
              for (uint32_t k = 0; k < v->vr.cmm.n; k++)
              {
                Syntax_Node *vc = v->vr.cmm.d[k];
                if (string_equal_ignore_case(vc->cm.nm, n->se.se))
                {
                  n->ty = resolve_subtype(SM, vc->cm.ty);
                  return;
                }
              }
            }
          }
        }
        if (error_count < 99)
          fatal_error(n->l, "?fld '%.*s'", (int) n->se.se.n, n->se.se.s);
      }
    }
    n->ty = TY_INT;
  }
  break;
  case N_AT:
    resolve_expression(SM, n->at.p, 0);
    for (uint32_t i = 0; i < n->at.ar.n; i++)
      resolve_expression(SM, n->at.ar.d[i], 0);
    {
      Type_Info *pt = n->at.p ? n->at.p->ty : 0;
      Type_Info *ptc = pt ? type_canonical_concrete(pt) : 0;
      String_Slice a = n->at.at;
      if (string_equal_ignore_case(a, STRING_LITERAL("FIRST"))
          or string_equal_ignore_case(a, STRING_LITERAL("LAST")))
        n->ty = ptc and ptc->el                                     ? ptc->el
                : (ptc and (is_discrete(ptc) or is_real_type(ptc))) ? pt
                                                                    : TY_INT;
      else if (string_equal_ignore_case(a, STRING_LITERAL("ADDRESS")))
      {
        Syntax_Node *sel = ND(SEL, n->l);
        sel->se.p = ND(ID, n->l);
        sel->se.p->s = STRING_LITERAL("SYSTEM");
        sel->se.se = STRING_LITERAL("ADDRESS");
        n->ty = resolve_subtype(SM, sel);
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
        n->ty = TY_INT;
      else if (
          string_equal_ignore_case(a, STRING_LITERAL("DELTA"))
          or string_equal_ignore_case(a, STRING_LITERAL("EPSILON"))
          or string_equal_ignore_case(a, STRING_LITERAL("SMALL"))
          or string_equal_ignore_case(a, STRING_LITERAL("LARGE"))
          or string_equal_ignore_case(a, STRING_LITERAL("SAFE_LARGE"))
          or string_equal_ignore_case(a, STRING_LITERAL("SAFE_SMALL")))
        n->ty = TY_FLT;
      else if (
          string_equal_ignore_case(a, STRING_LITERAL("CALLABLE"))
          or string_equal_ignore_case(a, STRING_LITERAL("TERMINATED"))
          or string_equal_ignore_case(a, STRING_LITERAL("CONSTRAINED"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_OVERFLOWS"))
          or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_ROUNDS")))
        n->ty = TY_BOOL;
      else if (string_equal_ignore_case(a, STRING_LITERAL("ACCESS")))
        n->ty = tyn(TYPE_ACCESS, N);
      else if (string_equal_ignore_case(a, STRING_LITERAL("IMAGE")))
        n->ty = TY_STR;
      else if (
          string_equal_ignore_case(a, STRING_LITERAL("VALUE"))
          or string_equal_ignore_case(a, STRING_LITERAL("SUCC"))
          or string_equal_ignore_case(a, STRING_LITERAL("PRED"))
          or string_equal_ignore_case(a, STRING_LITERAL("VAL")))
        n->ty = pt ?: TY_INT;
      else if (string_equal_ignore_case(a, STRING_LITERAL("RANGE")))
        n->ty = TY_INT;
      else if (string_equal_ignore_case(a, STRING_LITERAL("BASE")))
        n->ty = pt and pt->bs ? pt->bs : pt;
      else
        n->ty = TY_INT;
      if (string_equal_ignore_case(a, STRING_LITERAL("POS")) and n->at.ar.n > 0
          and n->at.ar.d[0]->k == N_INT)
      {
        if (ptc and is_integer_type(ptc))
        {
          n->k = N_INT;
          n->i = n->at.ar.d[0]->i;
          n->ty = TY_UINT;
        }
      }
      if (string_equal_ignore_case(a, STRING_LITERAL("VAL")) and n->at.ar.n > 0
          and n->at.ar.d[0]->k == N_INT)
      {
        int64_t pos = n->at.ar.d[0]->i;
        if (ptc == TY_CHAR and pos >= 0 and pos <= 127)
        {
          n->k = N_CHAR;
          n->i = pos;
          n->ty = TY_CHAR;
        }
        else if (
            ptc and ptc->k == TYPE_ENUMERATION and pos >= ptc->lo and pos <= ptc->hi
            and (uint32_t) pos < ptc->ev.n)
        {
          Symbol *e = ptc->ev.d[pos];
          n->k = N_ID;
          n->s = e->nm;
          n->ty = pt;
          n->sy = e;
        }
      }
    }
    break;
  case N_QL:
  {
    Type_Info *qt = resolve_subtype(SM, n->ql.nm);
    resolve_expression(SM, n->ql.ag, qt);
    n->ty = qt;
    if (n->ql.ag->ty and qt)
    {
      icv(qt, n->ql.ag);
      n->ql.ag->ty = qt;
    }
  }
  break;
  case N_CL:
  {
    resolve_expression(SM, n->cl.fn, 0);
    for (uint32_t i = 0; i < n->cl.ar.n; i++)
      resolve_expression(SM, n->cl.ar.d[i], 0);
    Type_Info *ft = n->cl.fn ? n->cl.fn->ty : 0;
    if (ft and ft->k == TYPE_ARRAY)
    {
      Syntax_Node *fn = n->cl.fn;
      Node_Vector ar = n->cl.ar;
      n->k = N_IX;
      n->ix.p = fn;
      n->ix.ix = ar;
      resolve_expression(SM, n, tx);
      break;
    }
    if (n->cl.fn->k == N_ID or n->cl.fn->k == N_STR)
    {
      String_Slice fnm = n->cl.fn->k == N_ID ? n->cl.fn->s : n->cl.fn->s;
      Symbol *s = n->cl.fn->sy;
      if (not s)
        s = symbol_find_with_arity(SM, fnm, n->cl.ar.n, tx);
      if (s)
      {
        n->cl.fn->sy = s;
        if (s->ty and s->ty->k == TYPE_STRING and s->ty->el)
        {
          n->ty = s->ty->el;
          n->sy = s;
        }
        else if (s->k == 1)
        {
          Syntax_Node *cv = ND(CVT, n->l);
          cv->cvt.ty = n->cl.fn;
          cv->cvt.ex = n->cl.ar.n > 0 ? n->cl.ar.d[0] : 0;
          n->k = N_CVT;
          n->cvt = cv->cvt;
          n->ty = s->ty ? s->ty : TY_INT;
        }
        else
          n->ty = TY_INT;
      }
      else
        n->ty = TY_INT;
    }
    else
      n->ty = TY_INT;
  }
  break;
  case N_AG:
    for (uint32_t i = 0; i < n->ag.it.n; i++)
      resolve_expression(SM, n->ag.it.d[i], tx);
    n->ty = tx ?: TY_INT;
    icv(tx, n);
    break;
  case N_ALC:
    n->ty = tyn(TYPE_ACCESS, N);
    n->ty->el = resolve_subtype(SM, n->alc.st);
    if (n->alc.in)
    {
      Type_Info *et = n->ty->el ? type_canonical_concrete(n->ty->el) : 0;
      if (et and et->k == TYPE_RECORD and et->dc.n > 0)
      {
        for (uint32_t i = 0; i < et->dc.n; i++)
        {
          Syntax_Node *d = et->dc.d[i];
          if (d->k == N_DS and d->pm.df)
          {
            resolve_expression(SM, d->pm.df, resolve_subtype(SM, d->pm.ty));
          }
        }
      }
      resolve_expression(SM, n->alc.in, n->ty->el);
      if (tx and tx->k == TYPE_ACCESS and tx->el)
      {
        Type_Info *ct = type_canonical_concrete(tx->el);
        if (ct and ct->k == TYPE_RECORD and ct->dc.n > 0)
        {
          bool hcd = 0;
          for (uint32_t i = 0; i < ct->dc.n; i++)
            if (ct->dc.d[i]->k == N_DS and ct->dc.d[i]->pm.df)
              hcd = 1;
          if (hcd and et and et->dc.n > 0)
          {
            for (uint32_t i = 0; i < ct->dc.n and i < et->dc.n; i++)
            {
              Syntax_Node *cd = ct->dc.d[i], *ed = et->dc.d[i];
              if (cd->k == N_DS and cd->pm.df and ed->k == N_DS)
              {
                bool mtch = cd->pm.df->k == N_INT and ed->pm.df and ed->pm.df->k == N_INT
                            and cd->pm.df->i == ed->pm.df->i;
                if (not mtch)
                {
                  n->alc.in = chk(SM, n->alc.in, n->l);
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
    resolve_expression(SM, n->rn.lo, tx);
    resolve_expression(SM, n->rn.hi, tx);
    n->rn.lo = chk(SM, n->rn.lo, n->l);
    n->rn.hi = chk(SM, n->rn.hi, n->l);
    n->ty = type_canonical_concrete(n->rn.lo->ty);
    break;
  case N_ASC:
    if (n->asc.vl)
    {
      Type_Info *vt = tx and tx->k == TYPE_ARRAY ? tx->el : tx;
      resolve_expression(SM, n->asc.vl, vt);
    }
    break;
  case N_DRF:
    resolve_expression(SM, n->drf.x, 0);
    {
      Type_Info *dty = n->drf.x->ty ? type_canonical_concrete(n->drf.x->ty) : 0;
      if (dty and dty->k == TYPE_ACCESS)
        n->ty = dty->el;
      else
      {
        if (error_count < 99 and n->drf.x->ty)
          fatal_error(n->l, ".all non-ac");
        n->ty = TY_INT;
      }
    }
    break;
  case N_CVT:
    resolve_expression(SM, n->cvt.ex, 0);
    n->ty = resolve_subtype(SM, n->cvt.ty);
    break;
  case N_CHK:
    resolve_expression(SM, n->chk.ex, tx);
    n->ty = n->chk.ex->ty;
    break;
  default:
    break;
  }
}
static void resolve_statement_sequence(Symbol_Manager *SM, Syntax_Node *n)
{
  if (not n)
    return;
  switch (n->k)
  {
  case N_AS:
    resolve_expression(SM, n->as.tg, 0);
    resolve_expression(SM, n->as.vl, n->as.tg->ty);
    {
      Type_Info *tgt = n->as.tg->ty ? type_canonical_concrete(n->as.tg->ty) : 0;
      Type_Info *vlt = n->as.vl->ty ? type_canonical_concrete(n->as.vl->ty) : 0;
      if (error_count < 99 and tgt and vlt and not type_covers(tgt, vlt))
      {
        Type_Info *tgb = semantic_base(tgt);
        Type_Info *vlb = semantic_base(vlt);
        if (not type_covers(tgb, vlb) and not(tgt->k == TYPE_BOOLEAN and is_discrete(vlt))
            and not(is_discrete(tgt) and is_discrete(vlt)))
          fatal_error(n->l, "typ mis");
      }
    }
    break;
    if (n->as.tg->ty)
      n->as.vl->ty = n->as.tg->ty;
    n->as.vl = chk(SM, n->as.vl, n->l);
    break;
  case N_IF:
    resolve_expression(SM, n->if_.cd, TY_BOOL);
    if (n->if_.th.n > 0 and not hrs(&n->if_.th))
      fatal_error(n->l, "seq needs stmt");
    for (uint32_t i = 0; i < n->if_.th.n; i++)
      resolve_statement_sequence(SM, n->if_.th.d[i]);
    for (uint32_t i = 0; i < n->if_.ei.n; i++)
    {
      Syntax_Node *e = n->if_.ei.d[i];
      resolve_expression(SM, e->if_.cd, TY_BOOL);
      if (e->if_.th.n > 0 and not hrs(&e->if_.th))
        fatal_error(e->l, "seq needs stmt");
      for (uint32_t j = 0; j < e->if_.th.n; j++)
        resolve_statement_sequence(SM, e->if_.th.d[j]);
    }
    if (n->if_.el.n > 0 and not hrs(&n->if_.el))
      fatal_error(n->l, "seq needs stmt");
    for (uint32_t i = 0; i < n->if_.el.n; i++)
      resolve_statement_sequence(SM, n->if_.el.d[i]);
    break;
  case N_CS:
    resolve_expression(SM, n->cs.ex, 0);
    for (uint32_t i = 0; i < n->cs.al.n; i++)
    {
      Syntax_Node *a = n->cs.al.d[i];
      for (uint32_t j = 0; j < a->ch.it.n; j++)
        resolve_expression(SM, a->ch.it.d[j], n->cs.ex->ty);
      if (a->hnd.stz.n > 0 and not hrs(&a->hnd.stz))
        fatal_error(a->l, "seq needs stmt");
      for (uint32_t j = 0; j < a->hnd.stz.n; j++)
        resolve_statement_sequence(SM, a->hnd.stz.d[j]);
    }
    break;
  case N_LP:
    if (n->lp.lb.s)
    {
      Symbol *lbs = symbol_add_overload(SM, syn(n->lp.lb, 10, 0, n));
      (void) lbs;
    }
    if (n->lp.it)
    {
      if (n->lp.it->k == N_BIN and n->lp.it->bn.op == T_IN)
      {
        Syntax_Node *v = n->lp.it->bn.l;
        if (v->k == N_ID)
        {
          Type_Info *rt = n->lp.it->bn.r->ty;
          Symbol *lvs = syn(v->s, 0, rt ?: TY_INT, 0);
          symbol_add_overload(SM, lvs);
          lvs->lv = -1;
          v->sy = lvs;
        }
      }
      resolve_expression(SM, n->lp.it, TY_BOOL);
    }
    if (n->lp.st.n > 0 and not hrs(&n->lp.st))
      fatal_error(n->l, "seq needs stmt");
    for (uint32_t i = 0; i < n->lp.st.n; i++)
      resolve_statement_sequence(SM, n->lp.st.d[i]);
    break;
  case N_BL:
    if (n->bk.lb.s)
    {
      Symbol *lbs = symbol_add_overload(SM, syn(n->bk.lb, 10, 0, n));
      (void) lbs;
    }
    scp(SM);
    for (uint32_t i = 0; i < n->bk.dc.n; i++)
      resolve_declaration(SM, n->bk.dc.d[i]);
    for (uint32_t i = 0; i < n->bk.st.n; i++)
      resolve_statement_sequence(SM, n->bk.st.d[i]);
    if (n->bk.hd.n > 0)
    {
      for (uint32_t i = 0; i < n->bk.hd.n; i++)
      {
        Syntax_Node *h = n->bk.hd.d[i];
        for (uint32_t j = 0; j < h->hnd.ec.n; j++)
        {
          Syntax_Node *e = h->hnd.ec.d[j];
          if (e->k == N_ID and not string_equal_ignore_case(e->s, STRING_LITERAL("others")))
            slv(&SM->eh, e->s);
        }
        for (uint32_t j = 0; j < h->hnd.stz.n; j++)
          resolve_statement_sequence(SM, h->hnd.stz.d[j]);
      }
    }
    sco(SM);
    break;
  case N_RT:
    if (n->rt.vl)
      resolve_expression(SM, n->rt.vl, 0);
    break;
  case N_EX:
    if (n->ex.cd)
      resolve_expression(SM, n->ex.cd, TY_BOOL);
    break;
  case N_RS:
    if (n->rs.ec and n->rs.ec->k == N_ID)
      slv(&SM->eh, n->rs.ec->s);
    else
      slv(&SM->eh, STRING_LITERAL("PROGRAM_ERROR"));
    if (n->rs.ec)
      resolve_expression(SM, n->rs.ec, 0);
    break;
  case N_CLT:
    resolve_expression(SM, n->ct.nm, 0);
    for (uint32_t i = 0; i < n->ct.arr.n; i++)
      resolve_expression(SM, n->ct.arr.d[i], 0);
    break;
  case N_ACC:
    scp(SM);
    {
      Symbol *ens = symbol_add_overload(SM, syn(n->acc.nm, 9, 0, n));
      (void) ens;
      for (uint32_t i = 0; i < n->acc.pmx.n; i++)
      {
        Syntax_Node *p = n->acc.pmx.d[i];
        Type_Info *pt = resolve_subtype(SM, p->pm.ty);
        Symbol *ps = symbol_add_overload(SM, syn(p->pm.nm, 0, pt, p));
        p->sy = ps;
      }
      if (n->acc.stx.n > 0 and not hrs(&n->acc.stx))
        fatal_error(n->l, "seq needs stmt");
      for (uint32_t i = 0; i < n->acc.stx.n; i++)
        resolve_statement_sequence(SM, n->acc.stx.d[i]);
    }
    sco(SM);
    break;
  case N_SA:
    if (n->sa.gd)
      resolve_expression(SM, n->sa.gd, 0);
    for (uint32_t i = 0; i < n->sa.sts.n; i++)
    {
      Syntax_Node *s = n->sa.sts.d[i];
      if (s->k == N_ACC)
      {
        for (uint32_t j = 0; j < s->acc.pmx.n; j++)
          resolve_expression(SM, s->acc.pmx.d[j], 0);
        if (s->acc.stx.n > 0 and not hrs(&s->acc.stx))
          fatal_error(s->l, "seq needs stmt");
        for (uint32_t j = 0; j < s->acc.stx.n; j++)
          resolve_statement_sequence(SM, s->acc.stx.d[j]);
      }
      else if (s->k == N_DL)
        resolve_expression(SM, s->ex.cd, 0);
    }
    break;
  case N_DL:
    resolve_expression(SM, n->ex.cd, 0);
    break;
  case N_AB:
    if (n->rs.ec and n->rs.ec->k == N_ID)
      slv(&SM->eh, n->rs.ec->s);
    else
      slv(&SM->eh, STRING_LITERAL("TASKING_ERROR"));
    if (n->rs.ec)
      resolve_expression(SM, n->rs.ec, 0);
    break;
  case N_US:
    if (n->us.nm->k == N_ID)
    {
      Symbol *s = symbol_find(SM, n->us.nm->s);
      if (s)
        sfu(SM, s, n->us.nm->s);
    }
    break;
  default:
    break;
  }
}
static void rrc_(Symbol_Manager *SM, RC *r)
{
  if (not r)
    return;
  switch (r->k)
  {
  case 1:
  {
    Symbol *ts = symbol_find(SM, r->er.nm);
    if (ts and ts->ty)
    {
      Type_Info *t = type_canonical_concrete(ts->ty);
      for (uint32_t i = 0; i < r->rr.cp.n; i++)
      {
        Syntax_Node *e = r->rr.cp.d[i];
        for (uint32_t j = 0; j < t->ev.n; j++)
        {
          Symbol *ev = t->ev.d[j];
          if (string_equal_ignore_case(ev->nm, e->s))
          {
            ev->vl = e->i;
            break;
          }
        }
      }
    }
  }
  break;
  case 2:
  {
    Symbol *s = symbol_find(SM, r->ad.nm);
    if (s and s->ty)
    {
      Type_Info *t = type_canonical_concrete(s->ty);
      t->ad = r->ad.ad;
    }
  }
  break;
  case 3:
  {
    Symbol *s = symbol_find(SM, r->rr.nm);
    if (s and s->ty)
    {
      Type_Info *t = type_canonical_concrete(s->ty);
      uint32_t bt = 0;
      for (uint32_t i = 0; i < r->rr.cp.n; i++)
      {
        Syntax_Node *cp = r->rr.cp.d[i];
        for (uint32_t j = 0; j < t->cm.n; j++)
        {
          Syntax_Node *c = t->cm.d[j];
          if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, cp->cm.nm))
          {
            c->cm.of = cp->cm.of;
            c->cm.bt = cp->cm.bt;
            bt += cp->cm.bt;
            break;
          }
        }
      }
      t->sz = (bt + 7) / 8;
      t->pk = true;
    }
  }
  break;
  case 4:
  {
    Symbol *s = r->ad.nm.n > 0 ? symbol_find(SM, r->ad.nm) : 0;
    if (s and s->ty)
    {
      Type_Info *t = type_canonical_concrete(s->ty);
      t->sup |= r->ad.ad;
    }
  }
  break;
  case 5:
  {
    Symbol *s = symbol_find(SM, r->er.nm);
    if (s and s->ty)
    {
      Type_Info *t = type_canonical_concrete(s->ty);
      t->pk = true;
    }
  }
  break;
  case 6:
  {
    Symbol *s = symbol_find(SM, r->er.nm);
    if (s)
      s->inl = true;
  }
  break;
  case 7:
  {
    Symbol *s = symbol_find(SM, r->er.nm);
    if (s and s->ty)
    {
      Type_Info *t = type_canonical_concrete(s->ty);
      t->ctrl = true;
    }
  }
  break;
  case 8:
  {
    Symbol *s = symbol_find(SM, r->im.nm);
    if (s)
    {
      s->ext = true;
      s->ext_nm = r->im.ext.n > 0 ? string_duplicate(r->im.ext) : string_duplicate(r->im.nm);
      s->ext_lang = string_duplicate(r->im.lang);
    }
  }
  break;
  }
}
static void ihop(Type_Info *dt, Type_Info *pt)
{
  if (not dt or not pt)
    return;
  for (uint32_t i = 0; i < pt->ops.n; i++)
  {
    Syntax_Node *op = pt->ops.d[i];
    if (op->k == N_FB or op->k == N_PB)
    {
      Syntax_Node *nop = node_new(op->k, op->l);
      nop->bd = op->bd;
      Syntax_Node *nsp = node_new(N_FS, op->l);
      nsp->sp = op->bd.sp->sp;
      nsp->sp.nm = string_duplicate(op->bd.sp->sp.nm);
      nsp->sp.pmm = op->bd.sp->sp.pmm;
      if (op->k == N_FB)
        nsp->sp.rt = op->bd.sp->sp.rt;
      nop->bd.sp = nsp;
      nop->bd.el = -1;
      nv(&dt->ops, nop);
    }
  }
}
static int ncp_depth = 0;
#define MAX_NCP_DEPTH 1000
static bool mfp(Syntax_Node *f, String_Slice nm);
static Syntax_Node *ncs(Syntax_Node *n, Node_Vector *fp, Node_Vector *ap);
static const char *lkp(Symbol_Manager *SM, String_Slice nm);
static Syntax_Node *pks2(Symbol_Manager *SM, String_Slice nm, const char *src);
static void pks(Symbol_Manager *SM, String_Slice nm, const char *src);
static void rap(Symbol_Manager *SM, Node_Vector *fp, Node_Vector *ap)
{
  if (not fp or not ap)
    return;
  for (uint32_t i = 0; i < fp->n and i < ap->n; i++)
  {
    Syntax_Node *f = fp->d[i];
    Syntax_Node *a = ap->d[i];
    if (f->k == N_GSP and a->k == N_STR)
    {
      int pc = f->sp.pmm.n;
      Type_Info *rt = 0;
      if (f->sp.rt)
      {
        if (f->sp.rt->k == N_ID)
        {
          String_Slice tn = f->sp.rt->s;
          for (uint32_t j = 0; j < fp->n; j++)
          {
            Syntax_Node *tf = fp->d[j];
            if (tf->k == N_GTP and string_equal_ignore_case(tf->td.nm, tn) and j < ap->n)
            {
              Syntax_Node *ta = ap->d[j];
              if (ta->k == N_ID)
              {
                Symbol *ts = symbol_find(SM, ta->s);
                if (ts and ts->ty)
                  rt = ts->ty;
              }
              break;
            }
          }
        }
      }
      Symbol *s = symbol_find_with_arity(SM, a->s, pc, rt);
      if (s)
      {
        a->k = N_ID;
        a->sy = s;
      }
    }
  }
}
static void ncsv(Node_Vector *d, Node_Vector *s, Node_Vector *fp, Node_Vector *ap)
{
  if (not s)
  {
    d->n = 0;
    d->c = 0;
    d->d = 0;
    return;
  }
  Syntax_Node **orig_d = d->d;
  (void) orig_d;
  uint32_t sn = s->n;
  if (sn == 0 or not s->d)
  {
    d->n = 0;
    d->c = 0;
    d->d = 0;
    return;
  }
  if (sn > 100000)
  {
    d->n = 0;
    d->c = 0;
    d->d = 0;
    return;
  }
  Syntax_Node **sd = malloc(sn * sizeof(Syntax_Node *));
  if (not sd)
  {
    d->n = 0;
    d->c = 0;
    d->d = 0;
    return;
  }
  memcpy(sd, s->d, sn * sizeof(Syntax_Node *));
  d->n = 0;
  d->c = 0;
  d->d = 0;
  for (uint32_t i = 0; i < sn; i++)
  {
    Syntax_Node *node = sd[i];
    if (node)
      nv(d, ncs(node, fp, ap));
  }
  free(sd);
}
static Syntax_Node *ncs(Syntax_Node *n, Node_Vector *fp, Node_Vector *ap)
{
  if (not n)
    return 0;
  if (ncp_depth++ > MAX_NCP_DEPTH)
  {
    ncp_depth--;
    return n;
  }
  if (fp and n->k == N_ID)
  {
    for (uint32_t i = 0; i < fp->n; i++)
      if (mfp(fp->d[i], n->s))
      {
        if (i < ap->n)
        {
          Syntax_Node *a = ap->d[i];
          Syntax_Node *r = a->k == N_ASC and a->asc.vl ? ncs(a->asc.vl, 0, 0) : ncs(a, 0, 0);
          ncp_depth--;
          return r;
        }
        Syntax_Node *r = ncs(n, 0, 0);
        ncp_depth--;
        return r;
      }
  }
  if (fp and n->k == N_STR)
  {
    for (uint32_t i = 0; i < fp->n; i++)
      if (mfp(fp->d[i], n->s))
      {
        if (i < ap->n)
        {
          Syntax_Node *a = ap->d[i];
          Syntax_Node *r = a->k == N_ASC and a->asc.vl ? ncs(a->asc.vl, 0, 0) : ncs(a, 0, 0);
          ncp_depth--;
          return r;
        }
        Syntax_Node *r = ncs(n, 0, 0);
        ncp_depth--;
        return r;
      }
  }
  Syntax_Node *c = node_new(n->k, n->l);
  c->ty = 0;
  c->sy = (n->k == N_ID and n->sy) ? n->sy : 0;
  switch (n->k)
  {
  case N_ID:
    c->s = n->s.s ? string_duplicate(n->s) : n->s;
    break;
  case N_INT:
    c->i = n->i;
    break;
  case N_REAL:
    c->f = n->f;
    break;
  case N_CHAR:
    c->i = n->i;
    break;
  case N_STR:
    c->s = n->s.s ? string_duplicate(n->s) : n->s;
    break;
  case N_NULL:
    break;
  case N_BIN:
    c->bn.op = n->bn.op;
    c->bn.l = ncs(n->bn.l, fp, ap);
    c->bn.r = ncs(n->bn.r, fp, ap);
    break;
  case N_UN:
    c->un.op = n->un.op;
    c->un.x = ncs(n->un.x, fp, ap);
    break;
  case N_AT:
    c->at.p = ncs(n->at.p, fp, ap);
    c->at.at = n->at.at;
    ncsv(&c->at.ar, &n->at.ar, fp, ap);
    break;
  case N_QL:
    c->ql.nm = ncs(n->ql.nm, fp, ap);
    c->ql.ag = ncs(n->ql.ag, fp, ap);
    break;
  case N_CL:
    c->cl.fn = ncs(n->cl.fn, fp, ap);
    ncsv(&c->cl.ar, &n->cl.ar, fp, ap);
    break;
  case N_IX:
    c->ix.p = ncs(n->ix.p, fp, ap);
    ncsv(&c->ix.ix, &n->ix.ix, fp, ap);
    break;
  case N_SL:
    c->sl.p = ncs(n->sl.p, fp, ap);
    c->sl.lo = ncs(n->sl.lo, fp, ap);
    c->sl.hi = ncs(n->sl.hi, fp, ap);
    break;
  case N_SEL:
    c->se.p = ncs(n->se.p, fp, ap);
    c->se.se = n->se.se;
    break;
  case N_ALC:
    c->alc.st = ncs(n->alc.st, fp, ap);
    c->alc.in = ncs(n->alc.in, fp, ap);
    break;
  case N_RN:
    c->rn.lo = ncs(n->rn.lo, fp, ap);
    c->rn.hi = ncs(n->rn.hi, fp, ap);
    break;
  case N_CN:
    c->cn.rn = ncs(n->cn.rn, fp, ap);
    ncsv(&c->cn.cs, &n->cn.cs, fp, ap);
    break;
  case N_CM:
    c->cm.nm = n->cm.nm;
    c->cm.ty = ncs(n->cm.ty, fp, ap);
    c->cm.in = ncs(n->cm.in, fp, ap);
    c->cm.al = n->cm.al;
    c->cm.of = n->cm.of;
    c->cm.bt = n->cm.bt;
    c->cm.dc = ncs(n->cm.dc, fp, ap);
    c->cm.dsc = ncs(n->cm.dsc, fp, ap);
    break;
  case N_VR:
    ncsv(&c->vr.ch, &n->vr.ch, fp, ap);
    ncsv(&c->vr.cmm, &n->vr.cmm, fp, ap);
    break;
  case N_VP:
    c->vp.ds = ncs(n->vp.ds, fp, ap);
    ncsv(&c->vp.vrr, &n->vp.vrr, fp, ap);
    c->vp.sz = n->vp.sz;
    break;
  case N_DS:
  case N_PM:
    c->pm.nm = n->pm.nm;
    c->pm.ty = ncs(n->pm.ty, fp, ap);
    c->pm.df = ncs(n->pm.df, fp, ap);
    c->pm.md = n->pm.md;
    break;
  case N_PS:
  case N_FS:
  case N_GSP:
    c->sp.nm = n->sp.nm;
    ncsv(&c->sp.pmm, &n->sp.pmm, fp, ap);
    c->sp.rt = ncs(n->sp.rt, fp, ap);
    c->sp.op = n->sp.op;
    break;
  case N_PD:
  case N_PB:
  case N_FD:
  case N_FB:
    c->bd.sp = ncs(n->bd.sp, fp, ap);
    ncsv(&c->bd.dc, &n->bd.dc, fp, ap);
    ncsv(&c->bd.st, &n->bd.st, fp, ap);
    ncsv(&c->bd.hdd, &n->bd.hdd, fp, ap);
    c->bd.el = n->bd.el;
    c->bd.pr = 0;
    ncsv(&c->bd.lk, &n->bd.lk, fp, ap);
    break;
  case N_PKS:
    c->ps.nm = n->ps.nm;
    ncsv(&c->ps.dc, &n->ps.dc, fp, ap);
    ncsv(&c->ps.pr, &n->ps.pr, fp, ap);
    c->ps.el = n->ps.el;
    break;
  case N_PKB:
    c->pb.nm = n->pb.nm;
    ncsv(&c->pb.dc, &n->pb.dc, fp, ap);
    ncsv(&c->pb.st, &n->pb.st, fp, ap);
    ncsv(&c->pb.hddd, &n->pb.hddd, fp, ap);
    c->pb.el = n->pb.el;
    break;
  case N_OD:
  case N_GVL:
    ncsv(&c->od.id, &n->od.id, fp, ap);
    c->od.ty = ncs(n->od.ty, fp, ap);
    c->od.in = ncs(n->od.in, fp, ap);
    c->od.co = n->od.co;
    break;
  case N_TD:
  case N_GTP:
    c->td.nm = n->td.nm;
    c->td.df = ncs(n->td.df, fp, ap);
    c->td.ds = ncs(n->td.ds, fp, ap);
    c->td.nw = n->td.nw;
    c->td.drv = n->td.drv;
    c->td.prt = ncs(n->td.prt, fp, ap);
    ncsv(&c->td.dsc, &n->td.dsc, fp, ap);
    break;
  case N_SD:
    c->sd.nm = n->sd.nm;
    c->sd.in = ncs(n->sd.in, fp, ap);
    c->sd.cn = ncs(n->sd.cn, fp, ap);
    c->sd.rn = ncs(n->sd.rn, fp, ap);
    break;
  case N_ED:
    ncsv(&c->ed.id, &n->ed.id, fp, ap);
    c->ed.rn = ncs(n->ed.rn, fp, ap);
    break;
  case N_RE:
    c->re.nm = n->re.nm;
    c->re.rn = ncs(n->re.rn, fp, ap);
    break;
  case N_AS:
    c->as.tg = ncs(n->as.tg, fp, ap);
    c->as.vl = ncs(n->as.vl, fp, ap);
    break;
  case N_IF:
  case N_EL:
    c->if_.cd = ncs(n->if_.cd, fp, ap);
    ncsv(&c->if_.th, &n->if_.th, fp, ap);
    ncsv(&c->if_.ei, &n->if_.ei, fp, ap);
    ncsv(&c->if_.el, &n->if_.el, fp, ap);
    break;
  case N_CS:
    c->cs.ex = ncs(n->cs.ex, fp, ap);
    ncsv(&c->cs.al, &n->cs.al, fp, ap);
    break;
  case N_LP:
    c->lp.lb = n->lp.lb;
    c->lp.it = ncs(n->lp.it, fp, ap);
    c->lp.rv = n->lp.rv;
    ncsv(&c->lp.st, &n->lp.st, fp, ap);
    ncsv(&c->lp.lk, &n->lp.lk, fp, ap);
    break;
  case N_BL:
    c->bk.lb = n->bk.lb;
    ncsv(&c->bk.dc, &n->bk.dc, fp, ap);
    ncsv(&c->bk.st, &n->bk.st, fp, ap);
    ncsv(&c->bk.hd, &n->bk.hd, fp, ap);
    break;
  case N_EX:
    c->ex.lb = n->ex.lb;
    c->ex.cd = ncs(n->ex.cd, fp, ap);
    break;
  case N_RT:
    c->rt.vl = ncs(n->rt.vl, fp, ap);
    break;
  case N_GT:
    c->go.lb = n->go.lb;
    break;
  case N_RS:
  case N_AB:
    c->rs.ec = ncs(n->rs.ec, fp, ap);
    break;
  case N_NS:
    break;
  case N_CLT:
    c->ct.nm = ncs(n->ct.nm, fp, ap);
    ncsv(&c->ct.arr, &n->ct.arr, fp, ap);
    break;
  case N_ACC:
    c->acc.nm = n->acc.nm;
    ncsv(&c->acc.ixx, &n->acc.ixx, fp, ap);
    ncsv(&c->acc.pmx, &n->acc.pmx, fp, ap);
    ncsv(&c->acc.stx, &n->acc.stx, fp, ap);
    ncsv(&c->acc.hdx, &n->acc.hdx, fp, ap);
    c->acc.gd = ncs(n->acc.gd, fp, ap);
    break;
  case N_SLS:
    ncsv(&c->ss.al, &n->ss.al, fp, ap);
    ncsv(&c->ss.el, &n->ss.el, fp, ap);
    break;
  case N_SA:
    c->sa.kn = n->sa.kn;
    c->sa.gd = ncs(n->sa.gd, fp, ap);
    ncsv(&c->sa.sts, &n->sa.sts, fp, ap);
    break;
  case N_TKS:
    c->ts.nm = n->ts.nm;
    ncsv(&c->ts.en, &n->ts.en, fp, ap);
    c->ts.it = n->ts.it;
    break;
  case N_TKB:
    c->tb.nm = n->tb.nm;
    ncsv(&c->tb.dc, &n->tb.dc, fp, ap);
    ncsv(&c->tb.st, &n->tb.st, fp, ap);
    ncsv(&c->tb.hdz, &n->tb.hdz, fp, ap);
    break;
  case N_ENT:
    c->ent.nm = n->ent.nm;
    ncsv(&c->ent.ixy, &n->ent.ixy, fp, ap);
    ncsv(&c->ent.pmy, &n->ent.pmy, fp, ap);
    c->ent.gd = ncs(n->ent.gd, fp, ap);
    break;
  case N_HD:
  case N_WH:
  case N_DL:
  case N_TRM:
    ncsv(&c->hnd.ec, &n->hnd.ec, fp, ap);
    ncsv(&c->hnd.stz, &n->hnd.stz, fp, ap);
    break;
  case N_CH:
    ncsv(&c->ch.it, &n->ch.it, fp, ap);
    break;
  case N_ASC:
    ncsv(&c->asc.ch, &n->asc.ch, fp, ap);
    c->asc.vl = ncs(n->asc.vl, fp, ap);
    break;
  case N_CX:
    ncsv(&c->cx.wt, &n->cx.wt, fp, ap);
    ncsv(&c->cx.us, &n->cx.us, fp, ap);
    break;
  case N_WI:
    c->wt.nm = n->wt.nm;
    break;
  case N_US:
    c->us.nm = ncs(n->us.nm, fp, ap);
    break;
  case N_PG:
    c->pg.nm = n->pg.nm;
    ncsv(&c->pg.ar, &n->pg.ar, fp, ap);
    break;
  case N_CU:
    c->cu.cx = ncs(n->cu.cx, fp, ap);
    ncsv(&c->cu.un, &n->cu.un, fp, ap);
    break;
  case N_DRF:
    c->drf.x = ncs(n->drf.x, fp, ap);
    break;
  case N_CVT:
    c->cvt.ty = ncs(n->cvt.ty, fp, ap);
    c->cvt.ex = ncs(n->cvt.ex, fp, ap);
    break;
  case N_CHK:
    c->chk.ex = ncs(n->chk.ex, fp, ap);
    c->chk.ec = n->chk.ec;
    break;
  case N_DRV:
    c->drv.bs = ncs(n->drv.bs, fp, ap);
    ncsv(&c->drv.ops, &n->drv.ops, fp, ap);
    break;
  case N_GEN:
    ncsv(&c->gen.fp, &n->gen.fp, fp, ap);
    ncsv(&c->gen.dc, &n->gen.dc, fp, ap);
    c->gen.un = ncs(n->gen.un, fp, ap);
    break;
  case N_GINST:
    c->gi.nm = n->gi.nm.s ? string_duplicate(n->gi.nm) : n->gi.nm;
    c->gi.gn = n->gi.gn.s ? string_duplicate(n->gi.gn) : n->gi.gn;
    ncsv(&c->gi.ap, &n->gi.ap, fp, ap);
    break;
  case N_AG:
    ncsv(&c->ag.it, &n->ag.it, fp, ap);
    c->ag.lo = ncs(n->ag.lo, fp, ap);
    c->ag.hi = ncs(n->ag.hi, fp, ap);
    c->ag.dim = n->ag.dim;
    break;
  case N_TA:
    ncsv(&c->ix.ix, &n->ix.ix, fp, ap);
    c->ix.p = ncs(n->ix.p, fp, ap);
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
    ncsv(&c->lst.it, &n->lst.it, fp, ap);
    break;
  default:
    break;
  }
  ncp_depth--;
  return c;
}
static bool mfp(Syntax_Node *f, String_Slice nm)
{
  if (f->k == N_GTP)
    return string_equal_ignore_case(f->td.nm, nm);
  if (f->k == N_GSP)
    return string_equal_ignore_case(f->sp.nm, nm);
  if (f->k == N_GVL)
    for (uint32_t j = 0; j < f->od.id.n; j++)
      if (string_equal_ignore_case(f->od.id.d[j]->s, nm))
        return 1;
  return 0;
}
static Syntax_Node *gcl(Symbol_Manager *SM, Syntax_Node *n)
{
  if (not n)
    return 0;
  if (n->k == N_GEN)
  {
    String_Slice nm = n->gen.un ? (n->gen.un->k == N_PKS ? n->gen.un->ps.nm
                                   : n->gen.un->bd.sp    ? n->gen.un->bd.sp->sp.nm
                                                         : N)
                                : N;
    GT *g = gfnd(SM, nm);
    if (not g)
    {
      g = generic_type_new(nm);
      g->fp = n->gen.fp;
      g->dc = n->gen.dc;
      g->un = n->gen.un;
      gv(&SM->gt, g);
      if (g->nm.s and g->nm.n)
      {
        Symbol *gs = syn(g->nm, 11, 0, n);
        gs->gt = g;
        if (g->un and g->un->k == N_PKS)
          gs->df = g->un;
        symbol_add_overload(SM, gs);
      }
    }
  }
  else if (n->k == N_GINST)
  {
    GT *g = gfnd(SM, n->gi.gn);
    if (g)
    {
      rap(SM, &g->fp, &n->gi.ap);
      Syntax_Node *inst = ncs(g->un, &g->fp, &n->gi.ap);
      if (inst)
      {
        if (inst->k == N_PB or inst->k == N_FB or inst->k == N_PD or inst->k == N_FD)
        {
          inst->bd.sp->sp.nm = n->gi.nm;
        }
        else if (inst->k == N_PKS)
        {
          inst->ps.nm = n->gi.nm;
        }
        return inst;
      }
    }
  }
  return 0;
}
static Symbol *get_pkg_sym(Symbol_Manager *SM, Syntax_Node *pk)
{
  if (not pk or not pk->sy)
    return 0;
  String_Slice nm = pk->k == N_PKS ? pk->ps.nm : pk->sy->nm;
  if (not nm.s or nm.n == 0)
    return 0;
  uint32_t h = syh(nm);
  for (Symbol *s = SM->sy[h]; s; s = s->nx)
    if (s->k == 6 and string_equal_ignore_case(s->nm, nm) and s->lv == 0)
      return s;
  return pk->sy;
}
static void resolve_declaration(Symbol_Manager *SM, Syntax_Node *n)
{
  if (not n)
    return;
  switch (n->k)
  {
  case N_GINST:
  {
    Syntax_Node *inst = gcl(SM, n);
    if (inst)
    {
      resolve_declaration(SM, inst);
      if (inst->k == N_PKS)
      {
        GT *g = gfnd(SM, n->gi.gn);
        if (g and g->bd)
        {
          Syntax_Node *bd = ncs(g->bd, &g->fp, &n->gi.ap);
          if (bd)
          {
            bd->pb.nm = n->gi.nm;
            resolve_declaration(SM, bd);
            nv(&SM->ib, bd);
          }
        }
      }
    }
  }
  break;
  case N_RRC:
  {
    RC *r = *(RC **) &n->ag.it.d;
    rrc_(SM, r);
  }
  break;
  case N_GVL:
  case N_OD:
  {
    Type_Info *t = resolve_subtype(SM, n->od.ty);
    for (uint32_t i = 0; i < n->od.id.n; i++)
    {
      Syntax_Node *id = n->od.id.d[i];
      Type_Info *ct = ucag(t, n->od.in);
      Symbol *x = symbol_find(SM, id->s);
      Symbol *s = 0;
      if (x and x->sc == SM->sc and x->ss == SM->ss and x->k != 11)
      {
        if (x->k == 2 and x->df and x->df->k == N_OD and not((Syntax_Node *) x->df)->od.in
            and n->od.co and n->od.in)
        {
          s = x;
        }
        else if (error_count < 99)
          fatal_error(n->l, "dup '%.*s'", (int) id->s.n, id->s.s);
      }
      if (not s)
      {
        s = symbol_add_overload(SM, syn(id->s, n->od.co ? 2 : 0, ct, n));
        s->pr = SM->pk ? get_pkg_sym(SM, SM->pk) : SEP_PKG.s ? symbol_find(SM, SEP_PKG) : 0;
      }
      id->sy = s;
      if (n->od.in)
      {
        resolve_expression(SM, n->od.in, t);
        n->od.in->ty = t;
        n->od.in = chk(SM, n->od.in, n->l);
        if (t and t->dc.n > 0 and n->od.in and n->od.in->ty)
        {
          Type_Info *it = type_canonical_concrete(n->od.in->ty);
          if (it and it->dc.n > 0)
          {
            for (uint32_t di = 0; di < t->dc.n and di < it->dc.n; di++)
            {
              Syntax_Node *td = t->dc.d[di];
              Syntax_Node *id = it->dc.d[di];
              if (td->k == N_DS and id->k == N_DS and td->pm.df and id->pm.df)
              {
                if (td->pm.df->k == N_INT and id->pm.df->k == N_INT
                    and td->pm.df->i != id->pm.df->i)
                {
                  Syntax_Node *dc = ND(CHK, n->l);
                  dc->chk.ex = n->od.in;
                  dc->chk.ec = STRING_LITERAL("CONSTRAINT_ERROR");
                  n->od.in = dc;
                  break;
                }
              }
            }
          }
        }
        s->df = n->od.in;
        if (n->od.co and n->od.in->k == N_INT)
          s->vl = n->od.in->i;
        else if (n->od.co and n->od.in->k == N_ID and n->od.in->sy and n->od.in->sy->k == 2)
          s->vl = n->od.in->sy->vl;
        else if (n->od.co and n->od.in->k == N_AT)
        {
          Type_Info *pt = n->od.in->at.p ? type_canonical_concrete(n->od.in->at.p->ty) : 0;
          String_Slice a = n->od.in->at.at;
          if (pt and string_equal_ignore_case(a, STRING_LITERAL("FIRST")))
            s->vl = pt->lo;
          else if (pt and string_equal_ignore_case(a, STRING_LITERAL("LAST")))
            s->vl = pt->hi;
        }
        else if (n->od.co and n->od.in->k == N_QL and n->od.in->ql.ag)
        {
          Syntax_Node *ag = n->od.in->ql.ag;
          if (ag->k == N_ID and ag->sy and ag->sy->k == 2)
            s->vl = ag->sy->vl;
          else if (ag->k == N_INT)
            s->vl = ag->i;
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
    if (n->td.drv and n->td.prt)
    {
      Type_Info *pt = resolve_subtype(SM, n->td.prt);
      if (n->td.prt->k == N_TAC and error_count < 99)
        fatal_error(n->l, "der acc ty");
      t = tyn(TYPE_DERIVED, n->td.nm);
      t->prt = pt;
      if (pt)
      {
        t->lo = pt->lo;
        t->hi = pt->hi;
        t->el = pt->el;
        t->dc = pt->dc;
        t->sz = pt->sz;
        t->al = pt->al;
        {
          Type_Info *_ept = pt;
          while (_ept and _ept->ev.n == 0 and (_ept->bs or _ept->prt))
            _ept = _ept->bs ? _ept->bs : _ept->prt;
          t->ev.n = 0;
          t->ev.d = 0;
          if (_ept)
            for (uint32_t _i = 0; _i < _ept->ev.n; _i++)
            {
              Symbol *_pe = _ept->ev.d[_i];
              Symbol *_ne = symbol_add_overload(SM, syn(_pe->nm, 2, t, n));
              _ne->vl = _pe->vl;
              sv(&t->ev, _ne);
            }
        }
        ihop(t, pt);
      }
      if (n->td.df and n->td.df->k == N_RN)
      {
        resolve_expression(SM, n->td.df->rn.lo, 0);
        resolve_expression(SM, n->td.df->rn.hi, 0);
        t->lo = n->td.df->rn.lo->i;
        t->hi = n->td.df->rn.hi->i;
      }
    }
    else
    {
      Symbol *px = symbol_find(SM, n->td.nm);
      if (px and px->k == 1 and px->ty and (px->ty->k == TYPE_INTEGER or px->ty->k == TY_PT)
          and n->td.df)
      {
        if (px->ty->k == TY_PT)
        {
          t = px->ty;
          t->prt = resolve_subtype(SM, n->td.df);
        }
        else
          t = px->ty;
      }
      else if (n->td.df)
      {
        t = resolve_subtype(SM, n->td.df);
      }
      else
      {
        t = tyn(n->td.df or n->td.drv ? TYPE_INTEGER : TY_PT, n->td.nm);
        if (t and n->td.nm.s)
        {
          Symbol *s = symbol_add_overload(SM, syn(n->td.nm, 1, t, n));
          n->sy = s;
        }
        break;
      }
    }
    if (t and n->td.nm.s and n->td.nm.n > 0 and not t->nm.s)
      t->nm = n->td.nm;
    if (n->td.dsc.n > 0)
    {
      for (uint32_t i = 0; i < n->td.dsc.n; i++)
      {
        Syntax_Node *d = n->td.dsc.d[i];
        if (d->k == N_DS)
        {
          Symbol *ds = symbol_add_overload(SM, syn(d->pm.nm, 8, resolve_subtype(SM, d->pm.ty), d));
          if (d->pm.df)
            resolve_expression(SM, d->pm.df, ds->ty);
        }
      }
      t->dc = n->td.dsc;
    }
    if (n->td.nm.s and n->td.nm.n > 0)
    {
      Symbol *px2 = symbol_find(SM, n->td.nm);
      if (px2 and px2->k == 1 and px2->ty == t)
      {
        n->sy = px2;
        if (n->td.df)
          px2->df = n;
      }
      else
      {
        Symbol *s = symbol_add_overload(SM, syn(n->td.nm, 1, t, n));
        n->sy = s;
      }
    }
    if (n->td.df and n->td.df->k == N_TE)
    {
      t->k = TYPE_ENUMERATION;
      int vl = 0;
      for (uint32_t i = 0; i < n->td.df->lst.it.n; i++)
      {
        Syntax_Node *it = n->td.df->lst.it.d[i];
        Symbol *es = symbol_add_overload(
            SM, syn(it->k == N_CHAR ? (String_Slice){(const char *) &it->i, 1} : it->s, 2, t, n));
        es->vl = vl++;
        sv(&t->ev, es);
      }
      t->lo = 0;
      t->hi = vl - 1;
    }
    if (n->td.df and n->td.df->k == N_TR)
    {
      t->k = TYPE_RECORD;
      of = 0;
      for (uint32_t i = 0; i < n->td.df->lst.it.n; i++)
      {
        Syntax_Node *c = n->td.df->lst.it.d[i];
        if (c->k == N_CM)
        {
          c->cm.of = of++;
          Type_Info *ct = resolve_subtype(SM, c->cm.ty);
          if (ct)
            c->cm.ty->ty = ct;
          if (c->cm.dc)
          {
            for (uint32_t j = 0; j < c->cm.dc->lst.it.n; j++)
            {
              Syntax_Node *dc = c->cm.dc->lst.it.d[j];
              if (dc->k == N_DS and dc->pm.df)
                resolve_expression(SM, dc->pm.df, resolve_subtype(SM, dc->pm.ty));
            }
          }
          if (c->cm.dsc)
          {
            for (uint32_t j = 0; j < c->cm.dsc->lst.it.n; j++)
            {
              Syntax_Node *dc = c->cm.dsc->lst.it.d[j];
              if (dc->k == N_DS)
              {
                Symbol *ds =
                    symbol_add_overload(SM, syn(dc->pm.nm, 8, resolve_subtype(SM, dc->pm.ty), dc));
                if (dc->pm.df)
                  resolve_expression(SM, dc->pm.df, ds->ty);
              }
            }
            c->cm.dc = c->cm.dsc;
          }
        }
        else if (c->k == N_VP)
        {
          for (uint32_t j = 0; j < c->vp.vrr.n; j++)
          {
            Syntax_Node *v = c->vp.vrr.d[j];
            for (uint32_t k = 0; k < v->vr.cmm.n; k++)
            {
              Syntax_Node *vc = v->vr.cmm.d[k];
              vc->cm.of = of++;
              Type_Info *vct = resolve_subtype(SM, vc->cm.ty);
              if (vct)
                vc->cm.ty->ty = vct;
              if (vc->cm.dc)
              {
                for (uint32_t m = 0; m < vc->cm.dc->lst.it.n; m++)
                {
                  Syntax_Node *dc = vc->cm.dc->lst.it.d[m];
                  if (dc->k == N_DS and dc->pm.df)
                    resolve_expression(SM, dc->pm.df, resolve_subtype(SM, dc->pm.ty));
                }
              }
              if (vc->cm.dsc)
              {
                for (uint32_t m = 0; m < vc->cm.dsc->lst.it.n; m++)
                {
                  Syntax_Node *dc = vc->cm.dsc->lst.it.d[m];
                  if (dc->k == N_DS)
                  {
                    Symbol *ds = symbol_add_overload(
                        SM, syn(dc->pm.nm, 8, resolve_subtype(SM, dc->pm.ty), dc));
                    if (dc->pm.df)
                      resolve_expression(SM, dc->pm.df, ds->ty);
                  }
                }
                vc->cm.dc = vc->cm.dsc;
              }
            }
          }
        }
      }
    }
    t->cm = n->td.df->lst.it;
    t->sz = of * 8;
  }
  break;
  case N_SD:
  {
    Type_Info *b = resolve_subtype(SM, n->sd.in);
    Type_Info *t = tyn(b ? b->k : TYPE_INTEGER, n->sd.nm);
    if (b)
    {
      t->bs = b;
      t->el = b->el;
      t->cm = b->cm;
      t->dc = b->dc;
      t->sz = b->sz;
      t->al = b->al;
      t->ad = b->ad;
      t->pk = b->pk;
      t->lo = b->lo;
      t->hi = b->hi;
      t->prt = b->prt ? b->prt : b;
    }
    if (n->sd.rn)
    {
      resolve_expression(SM, n->sd.rn->rn.lo, 0);
      resolve_expression(SM, n->sd.rn->rn.hi, 0);
      Syntax_Node *lo = n->sd.rn->rn.lo;
      Syntax_Node *hi = n->sd.rn->rn.hi;
      int64_t lov = lo->k == N_UN and lo->un.op == T_MN and lo->un.x->k == N_INT ? -lo->un.x->i
                    : lo->k == N_ID and lo->sy and lo->sy->k == 2                ? lo->sy->vl
                                                                                 : lo->i;
      int64_t hiv = hi->k == N_UN and hi->un.op == T_MN and hi->un.x->k == N_INT ? -hi->un.x->i
                    : hi->k == N_ID and hi->sy and hi->sy->k == 2                ? hi->sy->vl
                                                                                 : hi->i;
      t->lo = lov;
      t->hi = hiv;
    }
    symbol_add_overload(SM, syn(n->sd.nm, 1, t, n));
  }
  break;
  case N_ED:
    for (uint32_t i = 0; i < n->ed.id.n; i++)
    {
      Syntax_Node *id = n->ed.id.d[i];
      if (n->ed.rn)
      {
        resolve_expression(SM, n->ed.rn, 0);
        Symbol *tgt = n->ed.rn->sy;
        if (tgt and tgt->k == 3)
        {
          Symbol *al = symbol_add_overload(SM, syn(id->s, 3, 0, n));
          al->df = n->ed.rn;
          id->sy = al;
        }
        else if (error_count < 99)
          fatal_error(n->l, "renames must be exception");
      }
      else
      {
        id->sy = symbol_add_overload(SM, syn(id->s, 3, 0, n));
      }
    }
    break;
  case N_GSP:
  {
    Type_Info *ft = tyn(TYPE_STRING, n->sp.nm);
    if (n->sp.rt)
    {
      Type_Info *rt = resolve_subtype(SM, n->sp.rt);
      ft->el = rt;
      Symbol *s = symbol_add_overload(SM, syn(n->sp.nm, 5, ft, n));
      nv(&s->ol, n);
      n->sy = s;
      s->pr = SM->pk ? get_pkg_sym(SM, SM->pk) : SEP_PKG.s ? symbol_find(SM, SEP_PKG) : 0;
      nv(&ft->ops, n);
    }
    else
    {
      Symbol *s = symbol_add_overload(SM, syn(n->sp.nm, 4, ft, n));
      nv(&s->ol, n);
      n->sy = s;
      s->pr = SM->pk ? get_pkg_sym(SM, SM->pk) : SEP_PKG.s ? symbol_find(SM, SEP_PKG) : 0;
      nv(&ft->ops, n);
    }
  }
  break;
  case N_PD:
  case N_PB:
  {
    Syntax_Node *sp = n->bd.sp;
    Type_Info *ft = tyn(TYPE_STRING, sp->sp.nm);
    Symbol *s = symbol_add_overload(SM, syn(sp->sp.nm, 4, ft, n));
    nv(&s->ol, n);
    n->sy = s;
    n->bd.el = s->el;
    s->pr = SM->pk ? get_pkg_sym(SM, SM->pk) : SEP_PKG.s ? symbol_find(SM, SEP_PKG) : 0;
    nv(&ft->ops, n);
    if (n->k == N_PB)
    {
      SM->lv++;
      scp(SM);
      n->bd.pr = s;
      GT *gt = gfnd(SM, sp->sp.nm);
      if (gt)
        for (uint32_t i = 0; i < gt->fp.n; i++)
          resolve_declaration(SM, gt->fp.d[i]);
      for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
      {
        Syntax_Node *p = sp->sp.pmm.d[i];
        Type_Info *pt = resolve_subtype(SM, p->pm.ty);
        Symbol *ps = symbol_add_overload(SM, syn(p->pm.nm, 0, pt, p));
        p->sy = ps;
      }
      for (uint32_t i = 0; i < n->bd.dc.n; i++)
        resolve_declaration(SM, n->bd.dc.d[i]);
      for (uint32_t i = 0; i < n->bd.st.n; i++)
        resolve_statement_sequence(SM, n->bd.st.d[i]);
      sco(SM);
      SM->lv--;
    }
  }
  break;
  case N_FB:
  {
    Syntax_Node *sp = n->bd.sp;
    Type_Info *rt = resolve_subtype(SM, sp->sp.rt);
    Type_Info *ft = tyn(TYPE_STRING, sp->sp.nm);
    ft->el = rt;
    Symbol *s = symbol_add_overload(SM, syn(sp->sp.nm, 5, ft, n));
    nv(&s->ol, n);
    n->sy = s;
    n->bd.el = s->el;
    s->pr = SM->pk ? get_pkg_sym(SM, SM->pk) : SEP_PKG.s ? symbol_find(SM, SEP_PKG) : 0;
    nv(&ft->ops, n);
    if (n->k == N_FB)
    {
      SM->lv++;
      scp(SM);
      n->bd.pr = s;
      GT *gt = gfnd(SM, sp->sp.nm);
      if (gt)
        for (uint32_t i = 0; i < gt->fp.n; i++)
          resolve_declaration(SM, gt->fp.d[i]);
      for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
      {
        Syntax_Node *p = sp->sp.pmm.d[i];
        Type_Info *pt = resolve_subtype(SM, p->pm.ty);
        Symbol *ps = symbol_add_overload(SM, syn(p->pm.nm, 0, pt, p));
        p->sy = ps;
      }
      for (uint32_t i = 0; i < n->bd.dc.n; i++)
        resolve_declaration(SM, n->bd.dc.d[i]);
      for (uint32_t i = 0; i < n->bd.st.n; i++)
        resolve_statement_sequence(SM, n->bd.st.d[i]);
      sco(SM);
      SM->lv--;
    }
  }
  break;
  case N_FD:
  {
    Syntax_Node *sp = n->bd.sp;
    Type_Info *rt = resolve_subtype(SM, sp->sp.rt);
    Type_Info *ft = tyn(TYPE_STRING, sp->sp.nm);
    ft->el = rt;
    Symbol *s = symbol_add_overload(SM, syn(sp->sp.nm, 5, ft, n));
    nv(&s->ol, n);
    n->sy = s;
    n->bd.el = s->el;
    s->pr = SM->pk ? get_pkg_sym(SM, SM->pk) : SEP_PKG.s ? symbol_find(SM, SEP_PKG) : 0;
    nv(&ft->ops, n);
  }
  break;
  case N_PKS:
  {
    Type_Info *t = tyn(TY_P, n->ps.nm);
    Symbol *s = symbol_add_overload(SM, syn(n->ps.nm, 6, t, n));
    n->sy = s;
    n->ps.el = s->el;
    SM->pk = n;
    scp(SM);
    for (uint32_t i = 0; i < n->ps.dc.n; i++)
      resolve_declaration(SM, n->ps.dc.d[i]);
    for (uint32_t i = 0; i < n->ps.pr.n; i++)
      resolve_declaration(SM, n->ps.pr.d[i]);
    sco(SM);
    SM->pk = 0;
  }
  break;
  case N_PKB:
  {
    Symbol *ps = symbol_find(SM, n->pb.nm);
    GT *gt = 0;
    if (ps and ps->k == 11)
    {
      gt = ps->gt ? ps->gt : gfnd(SM, n->pb.nm);
    }
    if (gt)
    {
      gt->bd = n;
      Syntax_Node *pk = gt->un and gt->un->k == N_PKS ? gt->un : 0;
      if (not pk and ps and ps->df and ps->df->k == N_PKS)
        pk = ps->df;
      if (pk)
      {
        scp(SM);
        SM->pk = pk;
        for (uint32_t i = 0; i < pk->ps.dc.n; i++)
          resolve_declaration(SM, pk->ps.dc.d[i]);
        for (uint32_t i = 0; i < gt->fp.n; i++)
          resolve_declaration(SM, gt->fp.d[i]);
        for (uint32_t i = 0; i < n->pb.dc.n; i++)
          resolve_declaration(SM, n->pb.dc.d[i]);
        for (uint32_t i = 0; i < n->pb.st.n; i++)
          resolve_statement_sequence(SM, n->pb.st.d[i]);
        sco(SM);
        SM->pk = 0;
      }
      break;
    }
    scp(SM);
    {
      const char *_src = lkp(SM, n->pb.nm);
      if (_src)
      {
        char _af[512];
        snprintf(_af, 512, "%.*s.ads", (int) n->pb.nm.n, n->pb.nm.s);
        Parser _p = pnw(_src, strlen(_src), _af);
        Syntax_Node *_cu = pcu(&_p);
        if (_cu)
          for (uint32_t _i = 0; _i < _cu->cu.un.n; _i++)
          {
            Syntax_Node *_u = _cu->cu.un.d[_i];
            Syntax_Node *_pk = _u->k == N_PKS ? _u
                               : _u->k == N_GEN and _u->gen.un and _u->gen.un->k == N_PKS
                                   ? _u->gen.un
                                   : 0;
            if (_pk and string_equal_ignore_case(_pk->ps.nm, n->pb.nm))
            {
              SM->pk = _pk;
              for (uint32_t _j = 0; _j < _pk->ps.dc.n; _j++)
                resolve_declaration(SM, _pk->ps.dc.d[_j]);
              for (uint32_t _j = 0; _j < _pk->ps.pr.n; _j++)
                resolve_declaration(SM, _pk->ps.pr.d[_j]);
              SM->pk = 0;
              break;
            }
          }
      }
    }
    ps = symbol_find(SM, n->pb.nm);
    if (not ps or not ps->df)
    {
      Type_Info *t = tyn(TY_P, n->pb.nm);
      ps = symbol_add_overload(SM, syn(n->pb.nm, 6, t, 0));
      ps->el = SM->eo++;
      Syntax_Node *pk = ND(PKS, n->l);
      pk->ps.nm = n->pb.nm;
      pk->sy = ps;
      ps->df = pk;
      n->sy = ps;
    }
    if (ps)
    {
      sv(&SM->uv, ps);
      n->pb.el = ps->el;
      SM->pk = ps->df;
      if (ps->df and ps->df->k == N_PKS)
      {
        Syntax_Node *pk = ps->df;
        for (uint32_t i = 0; i < pk->ps.dc.n; i++)
        {
          Syntax_Node *d = pk->ps.dc.d[i];
          if (d->sy)
          {
            d->sy->vis |= 2;
            sv(&SM->uv, d->sy);
          }
          if (d->k == N_ED)
            for (uint32_t j = 0; j < d->ed.id.n; j++)
              if (d->ed.id.d[j]->sy)
              {
                d->ed.id.d[j]->sy->vis |= 2;
                sv(&SM->uv, d->ed.id.d[j]->sy);
              }
        }
        for (uint32_t i = 0; i < pk->ps.pr.n; i++)
        {
          Syntax_Node *d = pk->ps.pr.d[i];
          if (d->sy)
          {
            d->sy->vis |= 2;
            sv(&SM->uv, d->sy);
          }
          if (d->k == N_ED)
            for (uint32_t j = 0; j < d->ed.id.n; j++)
              if (d->ed.id.d[j]->sy)
              {
                d->ed.id.d[j]->sy->vis |= 2;
                sv(&SM->uv, d->ed.id.d[j]->sy);
              }
        }
        for (uint32_t i = 0; i < pk->ps.dc.n; i++)
        {
          Syntax_Node *d = pk->ps.dc.d[i];
          if (d->sy)
            sv(&SM->uv, d->sy);
          else if (d->k == N_ED)
          {
            for (uint32_t j = 0; j < d->ed.id.n; j++)
            {
              Syntax_Node *eid = d->ed.id.d[j];
              if (eid->sy)
                sv(&SM->uv, eid->sy);
            }
          }
          else if (d->k == N_OD)
          {
            for (uint32_t j = 0; j < d->od.id.n; j++)
            {
              Syntax_Node *oid = d->od.id.d[j];
              if (oid->sy)
                sv(&SM->uv, oid->sy);
            }
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->pb.dc.n; i++)
      resolve_declaration(SM, n->pb.dc.d[i]);
    for (uint32_t i = 0; i < n->pb.st.n; i++)
      resolve_statement_sequence(SM, n->pb.st.d[i]);
    sco(SM);
    SM->pk = 0;
  }
  break;
  case N_TKS:
  {
    Type_Info *t = tyn(TY_T, n->ts.nm);
    t->cm = n->ts.en;
    Symbol *s = symbol_add_overload(SM, syn(n->ts.nm, 7, t, n));
    n->sy = s;
    s->pr = SM->pk ? get_pkg_sym(SM, SM->pk) : SEP_PKG.s ? symbol_find(SM, SEP_PKG) : 0;
  }
  break;
  case N_TKB:
  {
    Symbol *ts = symbol_find(SM, n->tb.nm);
    scp(SM);
    if (ts and ts->ty and ts->ty->cm.n > 0)
    {
      for (uint32_t i = 0; i < ts->ty->cm.n; i++)
      {
        Syntax_Node *en = ts->ty->cm.d[i];
        if (en and en->k == N_ENT)
        {
          Symbol *ens = symbol_add_overload(SM, syn(en->ent.nm, 9, 0, en));
          (void) ens;
        }
      }
    }
    for (uint32_t i = 0; i < n->tb.dc.n; i++)
      resolve_declaration(SM, n->tb.dc.d[i]);
    for (uint32_t i = 0; i < n->tb.st.n; i++)
      resolve_statement_sequence(SM, n->tb.st.d[i]);
    sco(SM);
  }
  break;
  case N_GEN:
    gcl(SM, n);
    break;
  case N_US:
    resolve_statement_sequence(SM, n);
    break;
  default:
    break;
  }
}
static int elc(Symbol_Manager *SM, Symbol_Vector *ev, Syntax_Node *n)
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
    Symbol *s = n->sy;
    if (s and s->el < 0)
      s->el = SM->eo;
    if (s)
    {
      sv(ev, s);
      if (s->el > mx)
        mx = s->el;
    }
  }
  break;
  case N_OD:
    for (uint32_t i = 0; i < n->od.id.n; i++)
    {
      Syntax_Node *id = n->od.id.d[i];
      if (id->sy)
      {
        sv(ev, id->sy);
        if (id->sy->el > mx)
          mx = id->sy->el;
      }
    }
    break;
  default:
    break;
  }
  return mx;
}
static char *rdf(const char *p)
{
  FILE *f = fopen(p, "rb");
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
static void rali(Symbol_Manager *SM, const char *pth)
{
  char a[512];
  snprintf(a, 512, "%s.ali", pth);
  char *ali = rdf(a);
  if (not ali)
    return;
  Source_Location ll = {0, 0, pth};
  SLV ws = {0}, ds = {0};
  for (char *l = ali; *l;)
  {
    if (*l == 'W' and l[1] == ' ')
    {
      char *e = l + 2;
      while (*e and *e != ' ' and *e != '\n')
        e++;
      String_Slice wn = {l + 2, e - (l + 2)};
      char *c = arena_allocate(wn.n + 1);
      memcpy(c, wn.s, wn.n);
      c[wn.n] = 0;
      slv(&ws, (String_Slice){c, wn.n});
    }
    else if (*l == 'D' and l[1] == ' ')
    {
      char *e = l + 2;
      while (*e and *e != ' ' and *e != '\n')
        e++;
      String_Slice dn = {l + 2, e - (l + 2)};
      char *c = arena_allocate(dn.n + 1);
      memcpy(c, dn.s, dn.n);
      c[dn.n] = 0;
      slv(&ds, (String_Slice){c, dn.n});
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
      for (uint32_t i = 0; i < sn.n and i < 250;)
        *m++ = sn.s[i++];
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
        Symbol *s = symbol_add_overload(SM, syn(sn, 0, vt, n));
        s->ext = 1;
        s->lv = 0;
        s->ext_nm = string_duplicate(msn);
        s->mangled_nm = string_duplicate(sn);
        n->sy = s;
      }
      else
      {
        Syntax_Node *n = node_new(isp ? N_PD : N_FD, ll), *sp = node_new(N_FS, ll);
        sp->sp.nm = string_duplicate(msn);
        for (int i = 1; i < pc; i++)
        {
          Syntax_Node *p = node_new(N_PM, ll);
          p->pm.nm = STRING_LITERAL("p");
          nv(&sp->sp.pmm, p);
        }
        sp->sp.rt = 0;
        n->bd.sp = sp;
        Symbol *s = symbol_add_overload(SM, syn(msn, isp ? 4 : 5, tyn(TYPE_STRING, msn), n));
        s->el = SM->eo++;
        nv(&s->ol, n);
        n->sy = s;
        s->mangled_nm = string_duplicate(sn);
      }
    }
    while (*l and *l != '\n')
      l++;
    if (*l)
      l++;
  }
  free(ali);
}
static const char *lkp(Symbol_Manager *SM, String_Slice nm)
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
        (int) nm.n,
        nm.s);
    for (char *p = pf + strlen(include_paths[i]); *p; p++)
      *p = tolower(*p);
    rali(SM, pf);
    snprintf(af, 512, "%s.ads", pf);
    const char *s = rdf(af);
    if (s)
      return s;
  }
  return 0;
}
static Syntax_Node *pks2(Symbol_Manager *SM, String_Slice nm, const char *src)
{
  if (not src)
    return 0;
  char af[512];
  snprintf(af, 512, "%.*s.ads", (int) nm.n, nm.s);
  Parser p = pnw(src, strlen(src), af);
  Syntax_Node *cu = pcu(&p);
  if (cu and cu->cu.cx)
  {
    for (uint32_t i = 0; i < cu->cu.cx->cx.wt.n; i++)
      pks2(SM, cu->cu.cx->cx.wt.d[i]->wt.nm, lkp(SM, cu->cu.cx->cx.wt.d[i]->wt.nm));
  }
  for (uint32_t i = 0; cu and i < cu->cu.un.n; i++)
  {
    Syntax_Node *u = cu->cu.un.d[i];
    if (u->k == N_PKS)
    {
      Type_Info *t = tyn(TY_P, nm);
      Symbol *ps = symbol_add_overload(SM, syn(nm, 6, t, u));
      ps->lv = 0;
      u->sy = ps;
      u->ps.el = ps->el;
      Syntax_Node *oldpk = SM->pk;
      int oldlv = SM->lv;
      SM->pk = u;
      SM->lv = 0;
      for (uint32_t j = 0; j < u->ps.dc.n; j++)
        resolve_declaration(SM, u->ps.dc.d[j]);
      for (uint32_t j = 0; j < u->ps.pr.n; j++)
        resolve_declaration(SM, u->ps.pr.d[j]);
      SM->lv = oldlv;
      SM->pk = oldpk;
    }
    else if (u->k == N_GEN)
      resolve_declaration(SM, u);
  }
  return cu;
}
static void pks(Symbol_Manager *SM, String_Slice nm, const char *src)
{
  Symbol *ps = symbol_find(SM, nm);
  if (ps and ps->k == 6)
    return;
  pks2(SM, nm, src);
}
static void smu(Symbol_Manager *SM, Syntax_Node *n)
{
  if (n->k != N_CU)
    return;
  for (uint32_t i = 0; i < n->cu.cx->cx.wt.n; i++)
    pks(SM, n->cu.cx->cx.wt.d[i]->wt.nm, lkp(SM, n->cu.cx->cx.wt.d[i]->wt.nm));
  for (uint32_t i = 0; i < n->cu.cx->cx.us.n; i++)
  {
    Syntax_Node *u = n->cu.cx->cx.us.d[i];
    if (u and u->k == N_US and u->us.nm and u->us.nm->k == N_ID)
    {
      Symbol *ps = symbol_find(SM, u->us.nm->s);
      if (ps and ps->k == 6 and ps->df and ps->df->k == N_PKS)
      {
        Syntax_Node *pk = ps->df;
        for (uint32_t j = 0; j < pk->ps.dc.n; j++)
        {
          Syntax_Node *d = pk->ps.dc.d[j];
          if (d->k == N_ED)
          {
            for (uint32_t k = 0; k < d->ed.id.n; k++)
              if (d->ed.id.d[k]->sy)
              {
                d->ed.id.d[k]->sy->vis |= 2;
                sv(&SM->uv, d->ed.id.d[k]->sy);
              }
          }
          else if (d->k == N_OD)
          {
            for (uint32_t k = 0; k < d->od.id.n; k++)
              if (d->od.id.d[k]->sy)
              {
                d->od.id.d[k]->sy->vis |= 2;
                sv(&SM->uv, d->od.id.d[k]->sy);
              }
          }
          else if (d->sy)
          {
            d->sy->vis |= 2;
            sv(&SM->uv, d->sy);
          }
        }
      }
    }
  }
  for (uint32_t i = 0; i < n->cu.un.n; i++)
  {
    Symbol_Vector eo = {0};
    int mx = 0;
    if (n->cu.un.d[i]->k == N_PKS)
      for (uint32_t j = 0; j < n->cu.un.d[i]->ps.dc.n; j++)
      {
        int e = elc(SM, &eo, n->cu.un.d[i]->ps.dc.d[j]);
        mx = e > mx ? e : mx;
      }
    else if (n->cu.un.d[i]->k == N_PKB)
      for (uint32_t j = 0; j < n->cu.un.d[i]->pb.dc.n; j++)
      {
        int e = elc(SM, &eo, n->cu.un.d[i]->pb.dc.d[j]);
        mx = e > mx ? e : mx;
      }
    for (uint32_t j = 0; j < eo.n; j++)
      if (eo.d[j]->k == 6 and eo.d[j]->df and eo.d[j]->df->k == N_PKS)
        for (uint32_t k = 0; k < ((Syntax_Node *) eo.d[j]->df)->ps.dc.n; k++)
          resolve_declaration(SM, ((Syntax_Node *) eo.d[j]->df)->ps.dc.d[k]);
    resolve_declaration(SM, n->cu.un.d[i]);
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
} V;
typedef struct TH TH;
typedef struct TK TK;
typedef struct PT PT;
struct TH
{
  String_Slice ec;
  jmp_buf jb;
  TH *nx;
};
struct TK
{
  pthread_t th;
  int id;
  String_Slice nm;
  Node_Vector en;
  PT *pt;
  bool ac, tm;
};
struct PT
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
  TH *eh;
  TK *tk[64];
  int tn;
  PT *pt[64];
  int pn;
  SLV lbs;
  SLV exs;
  SLV dcl;
  LEV ltb;
  uint8_t lopt[64];
} Code_Generator;
static int new_temporary_register(Code_Generator *g)
{
  return g->tm++;
}
static int new_label_block(Code_Generator *g)
{
  return g->lb++;
}
static int nm(Code_Generator *g)
{
  return ++g->md;
}
static void lmd(FILE *o, int id)
{
  fprintf(o, ", not llvm.loop !%d", id);
}
static void emd(Code_Generator *g)
{
  for (int i = 1; i <= g->md; i++)
  {
    fprintf(g->o, "!%d = distinct !{!%d", i, i);
    if (i < 64 and (g->lopt[i] & 1))
      fprintf(g->o, ", !%d", g->md + 1);
    if (i < 64 and (g->lopt[i] & 2))
      fprintf(g->o, ", !%d", g->md + 2);
    if (i < 64 and (g->lopt[i] & 4))
      fprintf(g->o, ", !%d", g->md + 3);
    fprintf(g->o, "}\n");
  }
  if (g->md > 0)
  {
    fprintf(g->o, "!%d = !{!\"llvm.loop.unroll.enable\"}\n", g->md + 1);
    fprintf(g->o, "!%d = !{!\"llvm.loop.vectorize.enable\"}\n", g->md + 2);
    fprintf(g->o, "!%d = !{!\"llvm.loop.distribute.enable\"}\n", g->md + 3);
  }
}
static int find_label(Code_Generator *g, String_Slice lb)
{
  for (uint32_t i = 0; i < g->lbs.n; i++)
    if (string_equal_ignore_case(g->lbs.d[i], lb))
      return i;
  return -1;
}
static void emit_exception(Code_Generator *g, String_Slice ex)
{
  for (uint32_t i = 0; i < g->exs.n; i++)
    if (string_equal_ignore_case(g->exs.d[i], ex))
      return;
  slv(&g->exs, ex);
}
static inline void lbl(Code_Generator *g, int l);
static int get_or_create_label_basic_block(Code_Generator *g, String_Slice nm)
{
  for (uint32_t i = 0; i < g->ltb.n; i++)
    if (string_equal_ignore_case(g->ltb.d[i]->nm, nm))
      return g->ltb.d[i]->bb;
  LE *e = malloc(sizeof(LE));
  e->nm = nm;
  e->bb = new_label_block(g);
  lev(&g->ltb, e);
  return e->bb;
}
static void emit_label_definition(Code_Generator *g, String_Slice nm)
{
  int bb = get_or_create_label_basic_block(g, nm);
  lbl(g, bb);
}
static bool add_declaration(Code_Generator *g, const char *fn)
{
  String_Slice fns = {fn, strlen(fn)};
  for (uint32_t i = 0; i < g->dcl.n; i++)
    if (string_equal_ignore_case(g->dcl.d[i], fns))
      return false;
  char *cp = malloc(fns.n + 1);
  memcpy(cp, fn, fns.n);
  cp[fns.n] = 0;
  slv(&g->dcl, (String_Slice){cp, fns.n});
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
    int64_t lo = tc->lo, hi = tc->hi;
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
    return tc->sz == 32 ? "float" : "double";
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
    h = h * 31 + (unsigned long) t->lo;
    h = h * 31 + (unsigned long) t->hi;
    h = h * 31 + type_hash(t->el);
  }
  else if (t->k == TYPE_RECORD)
  {
    for (uint32_t i = 0; i < t->dc.n and i < 8; i++)
      if (t->dc.d[i])
        h = h * 31;
  }
  else
  {
    h = h * 31 + (unsigned long) t->lo;
    h = h * 31 + (unsigned long) t->hi;
  }
  return h;
}
static int encode_symbol_name(char *b, int sz, Symbol *s, String_Slice nm, int pc, Syntax_Node *sp)
{
  if (s and s->ext and s->ext_nm.s)
  {
    int n = 0;
    for (uint32_t i = 0; i < s->ext_nm.n and i < sz - 1; i++)
      b[n++] = s->ext_nm.s[i];
    b[n] = 0;
    return n;
  }
  int n = 0;
  unsigned long uid = s ? s->uid : 0;
  if (s and s->pr and s->pr->nm.s and (uintptr_t) s->pr->nm.s > 4096)
  {
    for (uint32_t i = 0; i < s->pr->nm.n and i < 256; i++)
    {
      char c = s->pr->nm.s[i];
      if (not c)
        break;
      if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9'))
        b[n++] = toupper(c);
      else
        n += snprintf(b + n, sz - n, "_%02X", (unsigned char) c);
    }
    b[n++] = '_';
    b[n++] = '_';
    if (nm.s and (uintptr_t) nm.s > 4096)
    {
      for (uint32_t i = 0; i < nm.n and i < 256; i++)
      {
        char c = nm.s[i];
        if (not c)
          break;
        if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9'))
          b[n++] = toupper(c);
        else
          n += snprintf(b + n, sz - n, "_%02X", (unsigned char) c);
      }
    }
    if (sp and sp->sp.pmm.n > 0 and sp->sp.pmm.n < 64)
    {
      unsigned long h = 0, pnh = 0;
      for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
      {
        Syntax_Node *p = sp->sp.pmm.d[i];
        if (p and p->pm.ty)
          h = h * 31 + type_hash(p->pm.ty->ty ? p->pm.ty->ty : 0);
        if (p and p->pm.nm.s)
          pnh = pnh * 31 + string_hash(p->pm.nm);
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
  if (nm.s and (uintptr_t) nm.s > 4096)
  {
    for (uint32_t i = 0; i < nm.n and i < 256; i++)
    {
      char c = nm.s[i];
      if (not c)
        break;
      if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '_')
        b[n++] = c;
      else
        n += snprintf(b + n, sz - n, "_%02X", (unsigned char) c);
    }
  }
  if (sp and sp->sp.pmm.n > 0 and sp->sp.pmm.n < 64)
  {
    unsigned long h = 0, pnh = 0;
    for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
    {
      Syntax_Node *p = sp->sp.pmm.d[i];
      if (p and p->pm.ty)
        h = h * 31 + type_hash(p->pm.ty->ty ? p->pm.ty->ty : 0);
      if (p and p->pm.nm.s)
        pnh = pnh * 31 + string_hash(p->pm.nm);
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
  for (uint32_t i = 0; i < st->n; i++)
  {
    Syntax_Node *n = st->d[i];
    if (not n)
      continue;
    if (n->k == N_BL and has_nested_function(&n->bk.dc, &n->bk.st))
      return 1;
    if (n->k == N_IF)
    {
      if (has_nested_function_in_stmts(&n->if_.th) or has_nested_function_in_stmts(&n->if_.el))
        return 1;
      for (uint32_t j = 0; j < n->if_.ei.n; j++)
        if (n->if_.ei.d[j] and has_nested_function_in_stmts(&n->if_.ei.d[j]->if_.th))
          return 1;
    }
    if (n->k == N_CS)
    {
      for (uint32_t j = 0; j < n->cs.al.n; j++)
        if (n->cs.al.d[j] and has_nested_function_in_stmts(&n->cs.al.d[j]->hnd.stz))
          return 1;
    }
    if (n->k == N_LP and has_nested_function_in_stmts(&n->lp.st))
      return 1;
  }
  return 0;
}
static bool has_nested_function(Node_Vector *dc, Node_Vector *st)
{
  for (uint32_t i = 0; i < dc->n; i++)
    if (dc->d[i] and (dc->d[i]->k == N_PB or dc->d[i]->k == N_FB))
      return 1;
  return has_nested_function_in_stmts(st);
}
static V generate_expression(Code_Generator *g, Syntax_Node *n);
static void generate_statement_sequence(Code_Generator *g, Syntax_Node *n);
static void generate_block_frame(Code_Generator *g)
{
  int mx = 0;
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = g->sm->sy[h]; s; s = s->nx)
      if (s->k == 0 and s->el >= 0 and s->el > mx)
        mx = s->el;
  if (mx > 0)
    fprintf(g->o, "  %%__frame = alloca [%d x ptr]\n", mx + 1);
}
static void generate_declaration(Code_Generator *g, Syntax_Node *n);
static inline void lbl(Code_Generator *g, int l)
{
  fprintf(g->o, "Source_Location%d:\n", l);
}
static inline void emit_branch(Code_Generator *g, int l)
{
  fprintf(g->o, "  br label %%Source_Location%d\n", l);
}
static inline void emit_conditional_branch(Code_Generator *g, int c, int lt, int lf)
{
  fprintf(g->o, "  br i1 %%t%d, label %%Source_Location%d, label %%Source_Location%d\n", c, lt, lf);
}
static void
generate_index_constraint_check(Code_Generator *g, int idx, const char *lo_s, const char *hi_s)
{
  FILE *o = g->o;
  int lok = new_label_block(g), hik = new_label_block(g), erl = new_label_block(g),
      dn = new_label_block(g), lc = new_temporary_register(g);
  fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %s\n", lc, idx, lo_s);
  emit_conditional_branch(g, lc, lok, erl);
  lbl(g, lok);
  int hc = new_temporary_register(g);
  fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %s\n", hc, idx, hi_s);
  emit_conditional_branch(g, hc, hik, erl);
  lbl(g, hik);
  emit_branch(g, dn);
  lbl(g, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
  lbl(g, dn);
}
static V value_cast(Code_Generator *g, V v, Value_Kind k)
{
  if (v.k == k)
    return v;
  V r = {new_temporary_register(g), k};
  if (v.k == VALUE_KIND_INTEGER and k == VALUE_KIND_FLOAT)
    fprintf(g->o, "  %%t%d = sitofp i64 %%t%d to double\n", r.id, v.id);
  else if (v.k == VALUE_KIND_FLOAT and k == VALUE_KIND_INTEGER)
    fprintf(g->o, "  %%t%d = fptosi double %%t%d to i64\n", r.id, v.id);
  else if (v.k == VALUE_KIND_POINTER and k == VALUE_KIND_INTEGER)
    fprintf(g->o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", r.id, v.id);
  else if (v.k == VALUE_KIND_INTEGER and k == VALUE_KIND_POINTER)
    fprintf(g->o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", r.id, v.id);
  else if (v.k == VALUE_KIND_POINTER and k == VALUE_KIND_FLOAT)
  {
    int tmp = new_temporary_register(g);
    fprintf(g->o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", tmp, v.id);
    fprintf(g->o, "  %%t%d = sitofp i64 %%t%d to double\n", r.id, tmp);
  }
  else if (v.k == VALUE_KIND_FLOAT and k == VALUE_KIND_POINTER)
  {
    int tmp = new_temporary_register(g);
    fprintf(g->o, "  %%t%d = fptosi double %%t%d to i64\n", tmp, v.id);
    fprintf(g->o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", r.id, tmp);
  }
  else
    fprintf(
        g->o,
        "  %%t%d = bitcast %s %%t%d to %s\n",
        r.id,
        value_llvm_type_string(v.k),
        v.id,
        value_llvm_type_string(k));
  return r;
}
static V
generate_float_range_check(Code_Generator *g, V e, Type_Info *t, String_Slice ec, Value_Kind rk)
{
  if (not t or (t->lo == 0 and t->hi == 0))
    return value_cast(g, e, rk);
  FILE *o = g->o;
  V ef = value_cast(g, e, VALUE_KIND_FLOAT);
  union
  {
    int64_t i;
    double d;
  } ulo, uhi;
  ulo.i = t->lo;
  uhi.i = t->hi;
  int lok = new_label_block(g), hik = new_label_block(g), erl = new_label_block(g),
      dn = new_label_block(g), lc = new_temporary_register(g);
  fprintf(o, "  %%t%d = fcmp oge double %%t%d, %e\n", lc, ef.id, ulo.d);
  emit_conditional_branch(g, lc, lok, erl);
  lbl(g, lok);
  int hc = new_temporary_register(g);
  fprintf(o, "  %%t%d = fcmp ole double %%t%d, %e\n", hc, ef.id, uhi.d);
  emit_conditional_branch(g, hc, hik, erl);
  lbl(g, hik);
  emit_branch(g, dn);
  lbl(g, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.%.*s)\n", (int) ec.n, ec.s);
  fprintf(o, "  unreachable\n");
  lbl(g, dn);
  return value_cast(g, e, rk);
}
static V generate_array_bounds_check(
    Code_Generator *g, V e, Type_Info *t, Type_Info *et, String_Slice ec, Value_Kind rk)
{
  FILE *o = g->o;
  int lok = new_label_block(g), hik = new_label_block(g), erl = new_label_block(g),
      dn = new_label_block(g), tlo = new_temporary_register(g);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, (long long) t->lo);
  int thi = new_temporary_register(g);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", thi, (long long) t->hi);
  int elo = new_temporary_register(g);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", elo, et ? (long long) et->lo : 0LL);
  int ehi = new_temporary_register(g);
  fprintf(o, "  %%t%d = add i64 0, %lld\n", ehi, et ? (long long) et->hi : -1LL);
  int lc = new_temporary_register(g);
  fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", lc, elo, tlo);
  emit_conditional_branch(g, lc, lok, erl);
  lbl(g, lok);
  int hc = new_temporary_register(g);
  fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", hc, ehi, thi);
  emit_conditional_branch(g, hc, hik, erl);
  lbl(g, hik);
  emit_branch(g, dn);
  lbl(g, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.%.*s)\n", (int) ec.n, ec.s);
  fprintf(o, "  unreachable\n");
  lbl(g, dn);
  return value_cast(g, e, rk);
}
static V
generate_discrete_range_check(Code_Generator *g, V e, Type_Info *t, String_Slice ec, Value_Kind rk)
{
  FILE *o = g->o;
  int lok = new_label_block(g), hik = new_label_block(g), erl = new_label_block(g),
      dn = new_label_block(g), lc = new_temporary_register(g);
  fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %lld\n", lc, e.id, (long long) t->lo);
  emit_conditional_branch(g, lc, lok, erl);
  lbl(g, lok);
  int hc = new_temporary_register(g);
  fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %lld\n", hc, e.id, (long long) t->hi);
  emit_conditional_branch(g, hc, hik, erl);
  lbl(g, hik);
  emit_branch(g, dn);
  lbl(g, erl);
  fprintf(o, "  call void @__ada_raise(ptr @.ex.%.*s)\n", (int) ec.n, ec.s);
  fprintf(o, "  unreachable\n");
  lbl(g, dn);
  return value_cast(g, e, rk);
}
static V value_to_boolean(Code_Generator *g, V v)
{
  if (v.k != VALUE_KIND_INTEGER)
    v = value_cast(g, v, VALUE_KIND_INTEGER);
  int t = new_temporary_register(g);
  V c = {new_temporary_register(g), VALUE_KIND_INTEGER};
  fprintf(g->o, "  %%t%d = icmp ne i64 %%t%d, 0\n", t, v.id);
  fprintf(g->o, "  %%t%d = zext i1 %%t%d to i64\n", c.id, t);
  return c;
}
static V value_compare(Code_Generator *g, const char *op, V a, V b, Value_Kind k)
{
  a = value_cast(g, a, k);
  b = value_cast(g, b, k);
  int c = new_temporary_register(g);
  V r = {new_temporary_register(g), VALUE_KIND_INTEGER};
  if (k == VALUE_KIND_INTEGER)
    fprintf(g->o, "  %%t%d = icmp %s i64 %%t%d, %%t%d\n", c, op, a.id, b.id);
  else
    fprintf(g->o, "  %%t%d = fcmp %s double %%t%d, %%t%d\n", c, op, a.id, b.id);
  fprintf(g->o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, c);
  return r;
}
static V value_compare_integer(Code_Generator *g, const char *op, V a, V b)
{
  return value_compare(g, op, a, b, VALUE_KIND_INTEGER);
}
static V value_compare_float(Code_Generator *g, const char *op, V a, V b)
{
  return value_compare(g, op, a, b, VALUE_KIND_FLOAT);
}
static void generate_fat_pointer(Code_Generator *g, int fp, int d, int lo, int hi)
{
  FILE *o = g->o;
  fprintf(o, "  %%t%d = alloca {ptr,ptr}\n", fp);
  int bd = new_temporary_register(g);
  fprintf(o, "  %%t%d = alloca {i64,i64}\n", bd);
  fprintf(o, "  %%_lo%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 0\n", fp, bd);
  fprintf(o, "  store i64 %%t%d, ptr %%_lo%d\n", lo, fp);
  fprintf(o, "  %%_hi%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 1\n", fp, bd);
  fprintf(o, "  store i64 %%t%d, ptr %%_hi%d\n", hi, fp);
  int dp = new_temporary_register(g);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 0\n", dp, fp);
  fprintf(o, "  store ptr %%t%d, ptr %%t%d\n", d, dp);
  int bp = new_temporary_register(g);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 1\n", bp, fp);
  fprintf(o, "  store ptr %%t%d, ptr %%t%d\n", bd, bp);
}
static V get_fat_pointer_data(Code_Generator *g, int fp)
{
  V r = {new_temporary_register(g), VALUE_KIND_POINTER};
  FILE *o = g->o;
  int dp = new_temporary_register(g);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 0\n", dp, fp);
  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", r.id, dp);
  return r;
}
static void get_fat_pointer_bounds(Code_Generator *g, int fp, int *lo, int *hi)
{
  FILE *o = g->o;
  int bp = new_temporary_register(g);
  fprintf(o, "  %%t%d = getelementptr {ptr,ptr}, ptr %%t%d, i32 0, i32 1\n", bp, fp);
  int bv = new_temporary_register(g);
  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", bv, bp);
  *lo = new_temporary_register(g);
  fprintf(o, "  %%t%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 0\n", *lo, bv);
  int lov = new_temporary_register(g);
  fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lov, *lo);
  *lo = lov;
  *hi = new_temporary_register(g);
  fprintf(o, "  %%t%d = getelementptr {i64,i64}, ptr %%t%d, i32 0, i32 1\n", *hi, bv);
  int hiv = new_temporary_register(g);
  fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", hiv, *hi);
  *hi = hiv;
}
static V value_power(Code_Generator *g, V a, V b, Value_Kind k)
{
  a = value_cast(g, a, k);
  b = value_cast(g, b, k);
  V r = {new_temporary_register(g), k};
  if (k == VALUE_KIND_INTEGER)
    fprintf(g->o, "  %%t%d = call i64 @__ada_powi(i64 %%t%d, i64 %%t%d)\n", r.id, a.id, b.id);
  else
    fprintf(g->o, "  %%t%d = call double @pow(double %%t%d, double %%t%d)\n", r.id, a.id, b.id);
  return r;
}
static V value_power_integer(Code_Generator *g, V a, V b)
{
  return value_power(g, a, b, VALUE_KIND_INTEGER);
}
static V value_power_float(Code_Generator *g, V a, V b)
{
  return value_power(g, a, b, VALUE_KIND_FLOAT);
}
static V generate_aggregate(Code_Generator *g, Syntax_Node *n, Type_Info *ty)
{
  FILE *o = g->o;
  V r = {new_temporary_register(g), VALUE_KIND_POINTER};
  Type_Info *t = ty ? type_canonical_concrete(ty) : 0;
  if (t and t->k == TYPE_ARRAY and n->k == N_AG)
    normalize_array_aggregate(g->sm, t, n);
  if (t and t->k == TYPE_RECORD and n->k == N_AG)
    normalize_record_aggregate(g->sm, t, n);
  if (not t or t->k != TYPE_RECORD or t->pk)
  {
    int sz = n->ag.it.n ? n->ag.it.n : 1;
    int p = new_temporary_register(g);
    int by = new_temporary_register(g);
    fprintf(o, "  %%t%d = add i64 0, %d\n", by, sz * 8);
    fprintf(o, "  %%t%d = call ptr @__ada_ss_allocate(i64 %%t%d)\n", p, by);
    uint32_t ix = 0;
    for (uint32_t i = 0; i < n->ag.it.n; i++)
    {
      Syntax_Node *el = n->ag.it.d[i];
      if (el->k == N_ASC)
      {
        if (el->asc.ch.d[0]->k == N_ID
            and string_equal_ignore_case(el->asc.ch.d[0]->s, STRING_LITERAL("others")))
        {
          for (; ix < (uint32_t) sz; ix++)
          {
            V v = value_cast(g, generate_expression(g, el->asc.vl), VALUE_KIND_INTEGER);
            int ep = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p, ix);
            fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
          }
        }
        else
        {
          V v = value_cast(g, generate_expression(g, el->asc.vl), VALUE_KIND_INTEGER);
          for (uint32_t j = 0; j < el->asc.ch.n; j++)
          {
            Syntax_Node *ch = el->asc.ch.d[j];
            if (ch->k == N_ID and ch->sy and ch->sy->k == 1 and ch->sy->ty)
            {
              Type_Info *cht = type_canonical_concrete(ch->sy->ty);
              if (cht->k == TYPE_ENUMERATION)
              {
                for (uint32_t ei = 0; ei < cht->ev.n; ei++)
                {
                  V cv = {new_temporary_register(g), VALUE_KIND_INTEGER};
                  fprintf(o, "  %%t%d = add i64 0, %ld\n", cv.id, (long) cht->ev.d[ei]->vl);
                  int ep = new_temporary_register(g);
                  fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", ep, p, cv.id);
                  fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
                }
              }
              else if ((cht->lo != 0 or cht->hi != 0) and cht->k == TYPE_INTEGER)
              {
                for (int64_t ri = cht->lo; ri <= cht->hi; ri++)
                {
                  V cv = {new_temporary_register(g), VALUE_KIND_INTEGER};
                  fprintf(o, "  %%t%d = add i64 0, %ld\n", cv.id, (long) ri);
                  int ep = new_temporary_register(g);
                  fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", ep, p, cv.id);
                  fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
                }
              }
            }
            else
            {
              V ci = value_cast(g, generate_expression(g, ch), VALUE_KIND_INTEGER);
              int ep = new_temporary_register(g);
              fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", ep, p, ci.id);
              fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
            }
          }
          ix++;
        }
      }
      else
      {
        V v = value_cast(g, generate_expression(g, el), VALUE_KIND_INTEGER);
        int ep = new_temporary_register(g);
        fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p, ix);
        fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
        ix++;
      }
    }
    r.id = p;
  }
  else
  {
    uint32_t sz = t->sz / 8;
    int p = new_temporary_register(g);
    int by = new_temporary_register(g);
    fprintf(o, "  %%t%d = add i64 0, %u\n", by, sz * 8);
    fprintf(o, "  %%t%d = call ptr @__ada_ss_allocate(i64 %%t%d)\n", p, by);
    uint32_t ix = 0;
    for (uint32_t i = 0; i < n->ag.it.n; i++)
    {
      Syntax_Node *el = n->ag.it.d[i];
      if (el->k == N_ASC)
      {
        for (uint32_t j = 0; j < el->asc.ch.n; j++)
        {
          Syntax_Node *ch = el->asc.ch.d[j];
          if (ch->k == N_ID)
          {
            for (uint32_t k = 0; k < t->cm.n; k++)
            {
              Syntax_Node *c = t->cm.d[k];
              if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, ch->s))
              {
                V v = value_cast(g, generate_expression(g, el->asc.vl), VALUE_KIND_INTEGER);
                int ep = new_temporary_register(g);
                fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p, c->cm.of);
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
        V v = value_cast(g, generate_expression(g, el), VALUE_KIND_INTEGER);
        int ep = new_temporary_register(g);
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
  if (not s or s->ol.n == 0)
    return 0;
  for (uint32_t i = 0; i < s->ol.n; i++)
  {
    Syntax_Node *b = s->ol.d[i];
    if ((b->k == N_PB or b->k == N_FB) and b->bd.el == el)
      return b;
  }
  return 0;
}
static Syntax_Node *symbol_spec(Symbol *s)
{
  if (not s or s->ol.n == 0)
    return 0;
  Syntax_Node *b = symbol_body(s, s->el);
  if (b and b->bd.sp)
    return b->bd.sp;
  for (uint32_t i = 0; i < s->ol.n; i++)
  {
    Syntax_Node *d = s->ol.d[i];
    if (d->k == N_PD or d->k == N_FD)
      return d->bd.sp;
  }
  return 0;
}
static const char *get_attribute_name(String_Slice attr, String_Slice tnm)
{
  static char fnm[256];
  int pos = 0;
  pos += snprintf(fnm + pos, 256 - pos, "@__attr_");
  for (uint32_t i = 0; i < attr.n and pos < 250; i++)
    fnm[pos++] = attr.s[i];
  fnm[pos++] = '_';
  for (uint32_t i = 0; i < tnm.n and pos < 255; i++)
    fnm[pos++] = toupper(tnm.s[i]);
  fnm[pos] = 0;
  return fnm;
}
static V generate_expression(Code_Generator *g, Syntax_Node *n)
{
  FILE *o = g->o;
  if (not n)
    return (V){0, VALUE_KIND_INTEGER};
  V r = {new_temporary_register(g), token_kind_to_value_kind(n->ty)};
  switch (n->k)
  {
  case N_INT:
    r.k = VALUE_KIND_INTEGER;
    fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) n->i);
    break;
  case N_REAL:
    r.k = VALUE_KIND_FLOAT;
    fprintf(o, "  %%t%d = fadd double 0.0, %e\n", r.id, n->f);
    break;
  case N_CHAR:
    r.k = VALUE_KIND_INTEGER;
    fprintf(o, "  %%t%d = add i64 0, %d\n", r.id, (int) n->i);
    break;
  case N_STR:
    r.k = VALUE_KIND_POINTER;
    {
      int p = new_temporary_register(g);
      uint32_t sz = n->s.n + 1;
      fprintf(o, "  %%t%d = alloca [%u x i8]\n", p, sz);
      for (uint32_t i = 0; i < n->s.n; i++)
      {
        int ep = new_temporary_register(g);
        fprintf(o, "  %%t%d = getelementptr [%u x i8], ptr %%t%d, i64 0, i64 %u\n", ep, sz, p, i);
        fprintf(o, "  store i8 %d, ptr %%t%d\n", (int) (unsigned char) n->s.s[i], ep);
      }
      int zp = new_temporary_register(g);
      fprintf(
          o, "  %%t%d = getelementptr [%u x i8], ptr %%t%d, i64 0, i64 %u\n", zp, sz, p, n->s.n);
      fprintf(o, "  store i8 0, ptr %%t%d\n", zp);
      int dp = new_temporary_register(g);
      fprintf(o, "  %%t%d = getelementptr [%u x i8], ptr %%t%d, i64 0, i64 0\n", dp, sz, p);
      int lo_id = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 0, 1\n", lo_id);
      int hi_id = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 0, %u\n", hi_id, n->s.n);
      r.id = new_temporary_register(g);
      generate_fat_pointer(g, r.id, dp, lo_id, hi_id);
    }
    break;
  case N_NULL:
    r.k = VALUE_KIND_POINTER;
    fprintf(o, "  %%t%d = inttoptr i64 0 to ptr\n", r.id);
    break;
  case N_ID:
  {
    Symbol *s = n->sy ? n->sy : symbol_find(g->sm, n->s);
    if (not s and n->sy)
    {
      s = n->sy;
    }
    int gen_0p_call = 0;
    Value_Kind fn_ret_type = r.k;
    if (s and s->k == 5)
    {
      Symbol *s0 = symbol_find_with_arity(g->sm, n->s, 0, n->ty);
      if (s0)
      {
        s = s0;
        gen_0p_call = 1;
      }
    }
    if (s and s->k == 2
        and not(s->ty and is_unconstrained_array(type_canonical_concrete(s->ty)) and s->lv > 0))
    {
      if (s->df and s->df->k == N_STR)
      {
        r.k = VALUE_KIND_POINTER;
        char nb[256];
        if (s->pr and (uintptr_t) s->pr > 4096 and s->pr->nm.s)
        {
          int n = 0;
          for (uint32_t j = 0; j < s->pr->nm.n; j++)
            nb[n++] = toupper(s->pr->nm.s[j]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->pr->sc, s->pr->el);
          for (uint32_t j = 0; j < s->nm.n; j++)
            nb[n++] = toupper(s->nm.s[j]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->nm.n, s->nm.s);
        fprintf(o, "  %%t%d = bitcast ptr @%s to ptr\n", r.id, nb);
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) s->vl);
      }
    }
    else
    {
      Value_Kind k = VALUE_KIND_INTEGER;
      if (s and s->ty)
        k = token_kind_to_value_kind(s->ty);
      r.k = k;
      if (s and s->lv == 0)
      {
        char nb[256];
        if (s->ext and s->ext_nm.s)
        {
          snprintf(nb, 256, "%s", s->ext_nm.s);
        }
        else if (s->pr and (uintptr_t) s->pr > 4096 and s->pr->nm.s)
        {
          int n = 0;
          for (uint32_t i = 0; i < s->pr->nm.n; i++)
            nb[n++] = toupper(s->pr->nm.s[i]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->pr->sc, s->pr->el);
          for (uint32_t i = 0; i < s->nm.n; i++)
            nb[n++] = toupper(s->nm.s[i]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->nm.n, s->nm.s);
        if (s->k == 5)
        {
          if (gen_0p_call)
          {
            char fnb[256];
            encode_symbol_name(fnb, 256, s, n->s, 0, 0);
            fprintf(
                o, "  %%t%d = call %s @\"%s\"()\n", r.id, value_llvm_type_string(fn_ret_type), fnb);
            r.k = fn_ret_type;
          }
          else
          {
            Syntax_Node *b = symbol_body(s, s->el);
            Syntax_Node *sp = symbol_spec(s);
            if ((sp and sp->sp.pmm.n == 0) or (not b))
            {
              char fnb[256];
              encode_symbol_name(fnb, 256, s, n->s, 0, sp);
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
      else if (s and s->lv >= 0 and s->lv < g->sm->lv)
      {
        if (s->k == 5)
        {
          Syntax_Node *b = symbol_body(s, s->el);
          Syntax_Node *sp = symbol_spec(s);
          if (sp and sp->sp.pmm.n == 0)
          {
            Value_Kind rk = sp and sp->sp.rt
                                ? token_kind_to_value_kind(resolve_subtype(g->sm, sp->sp.rt))
                                : VALUE_KIND_INTEGER;
            char fnb[256];
            encode_symbol_name(fnb, 256, s, n->s, 0, sp);
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
            int level_diff = g->sm->lv - s->lv - 1;
            int slnk_ptr;
            if (level_diff == 0)
            {
              slnk_ptr = new_temporary_register(g);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
            }
            else
            {
              slnk_ptr = new_temporary_register(g);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
              for (int hop = 0; hop < level_diff; hop++)
              {
                int next_slnk = new_temporary_register(g);
                fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
                int loaded_slnk = new_temporary_register(g);
                fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
                slnk_ptr = loaded_slnk;
              }
            }
            int p = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->el);
            int a = new_temporary_register(g);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
            fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", r.id, value_llvm_type_string(k), a);
          }
        }
        else
        {
          Type_Info *vat = s and s->ty ? type_canonical_concrete(s->ty) : 0;
          int p = new_temporary_register(g);
          int level_diff = g->sm->lv - s->lv - 1;
          int slnk_ptr;
          if (level_diff == 0)
          {
            slnk_ptr = new_temporary_register(g);
            fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
          }
          else
          {
            slnk_ptr = new_temporary_register(g);
            fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
            for (int hop = 0; hop < level_diff; hop++)
            {
              int next_slnk = new_temporary_register(g);
              fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
              int loaded_slnk = new_temporary_register(g);
              fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
              slnk_ptr = loaded_slnk;
            }
          }
          fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->el);
          int a = new_temporary_register(g);
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
          Syntax_Node *b = symbol_body(s, s->el);
          Syntax_Node *sp = symbol_spec(s);
          if (sp and sp->sp.pmm.n == 0)
          {
            Value_Kind rk = sp and sp->sp.rt
                                ? token_kind_to_value_kind(resolve_subtype(g->sm, sp->sp.rt))
                                : VALUE_KIND_INTEGER;
            char fnb[256];
            encode_symbol_name(fnb, 256, s, n->s, 0, sp);
            if (s->lv >= g->sm->lv)
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
            Type_Info *vat = s and s->ty ? type_canonical_concrete(s->ty) : 0;
            if (vat and vat->k == TYPE_ARRAY)
            {
              if (vat->lo == 0 and vat->hi == -1)
              {
                fprintf(
                    o,
                    "  %%t%d = load ptr, ptr %%v.%s.sc%u.%u\n",
                    r.id,
                    string_to_lowercase(n->s),
                    s ? s->sc : 0,
                    s ? s->el : 0);
              }
              else
              {
                fprintf(
                    o,
                    "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                    r.id,
                    string_to_lowercase(n->s),
                    s ? s->sc : 0,
                    s ? s->el : 0);
              }
            }
            else
            {
              fprintf(
                  o,
                  "  %%t%d = load %s, ptr %%v.%s.sc%u.%u\n",
                  r.id,
                  value_llvm_type_string(k),
                  string_to_lowercase(n->s),
                  s ? s->sc : 0,
                  s ? s->el : 0);
            }
          }
        }
        else
        {
          Type_Info *vat = s and s->ty ? type_canonical_concrete(s->ty) : 0;
          if (vat and vat->k == TYPE_ARRAY)
          {
            if (vat->lo == 0 and vat->hi == -1)
            {
              fprintf(
                  o,
                  "  %%t%d = load ptr, ptr %%v.%s.sc%u.%u\n",
                  r.id,
                  string_to_lowercase(n->s),
                  s ? s->sc : 0,
                  s ? s->el : 0);
            }
            else
            {
              fprintf(
                  o,
                  "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                  r.id,
                  string_to_lowercase(n->s),
                  s ? s->sc : 0,
                  s ? s->el : 0);
            }
          }
          else
          {
            fprintf(
                o,
                "  %%t%d = load %s, ptr %%v.%s.sc%u.%u\n",
                r.id,
                value_llvm_type_string(k),
                string_to_lowercase(n->s),
                s ? s->sc : 0,
                s ? s->el : 0);
          }
        }
      }
    }
  }
  break;
  case N_BIN:
  {
    Token_Kind op = n->bn.op;
    if (op == T_ATHN or op == T_OREL)
    {
      V lv = value_to_boolean(g, generate_expression(g, n->bn.l));
      int c = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c, lv.id);
      int lt = new_label_block(g), lf = new_label_block(g), ld = new_label_block(g);
      if (op == T_ATHN)
        emit_conditional_branch(g, c, lt, lf);
      else
        emit_conditional_branch(g, c, lf, lt);
      lbl(g, lt);
      V rv = value_to_boolean(g, generate_expression(g, n->bn.r));
      emit_branch(g, ld);
      lbl(g, lf);
      emit_branch(g, ld);
      lbl(g, ld);
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
      Type_Info *lt = n->bn.l->ty ? type_canonical_concrete(n->bn.l->ty) : 0;
      Type_Info *rt = n->bn.r->ty ? type_canonical_concrete(n->bn.r->ty) : 0;
      if (lt and rt and lt->k == TYPE_ARRAY and rt->k == TYPE_ARRAY)
      {
        int sz = lt->hi >= lt->lo ? lt->hi - lt->lo + 1 : 1;
        int p = new_temporary_register(g);
        fprintf(o, "  %%t%d = alloca [%d x i64]\n", p, sz);
        V la = generate_expression(g, n->bn.l);
        V ra = generate_expression(g, n->bn.r);
        for (int i = 0; i < sz; i++)
        {
          int ep1 = new_temporary_register(g);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep1, la.id, i);
          int lv = new_temporary_register(g);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lv, ep1);
          int ep2 = new_temporary_register(g);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep2, ra.id, i);
          int rv = new_temporary_register(g);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", rv, ep2);
          int res = new_temporary_register(g);
          fprintf(
              o,
              "  %%t%d = %s i64 %%t%d, %%t%d\n",
              res,
              op == T_AND  ? "and"
              : op == T_OR ? "or"
                           : "xor",
              lv,
              rv);
          int ep3 = new_temporary_register(g);
          fprintf(
              o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 %d\n", ep3, sz, p, i);
          fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", res, ep3);
        }
        r.k = VALUE_KIND_POINTER;
        fprintf(o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 0\n", r.id, sz, p);
        break;
      }
      V a = value_to_boolean(g, generate_expression(g, n->bn.l));
      V b = value_to_boolean(g, generate_expression(g, n->bn.r));
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
      V x = value_cast(g, generate_expression(g, n->bn.l), VALUE_KIND_INTEGER);
      Syntax_Node *rr = n->bn.r;
      while (rr and rr->k == N_CHK)
        rr = rr->chk.ex;
      if (rr and rr->k == N_RN)
      {
        V lo = value_cast(g, generate_expression(g, rr->rn.lo), VALUE_KIND_INTEGER),
          hi = value_cast(g, generate_expression(g, rr->rn.hi), VALUE_KIND_INTEGER);
        V ge = value_compare_integer(g, "sge", x, lo);
        V le = value_compare_integer(g, "sle", x, hi);
        V b1 = value_to_boolean(g, ge), b2 = value_to_boolean(g, le);
        int c1 = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
        int c2 = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
        int a1 = new_temporary_register(g);
        fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", a1, c1, c2);
        int xr = new_temporary_register(g);
        fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", xr, a1);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = xor i64 %%t%d, 1\n", r.id, xr);
      }
      else if (rr and rr->k == N_ID)
      {
        Symbol *s = rr->sy ? rr->sy : symbol_find(g->sm, rr->s);
        if (s and s->ty)
        {
          Type_Info *t = type_canonical_concrete(s->ty);
          if (t)
          {
            int tlo = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, (long long) t->lo);
            int thi = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", thi, (long long) t->hi);
            V lo = {tlo, VALUE_KIND_INTEGER}, hi = {thi, VALUE_KIND_INTEGER};
            V ge = value_compare_integer(g, "sge", x, lo);
            V le = value_compare_integer(g, "sle", x, hi);
            V b1 = value_to_boolean(g, ge), b2 = value_to_boolean(g, le);
            int c1 = new_temporary_register(g);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
            int c2 = new_temporary_register(g);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
            int a1 = new_temporary_register(g);
            fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", a1, c1, c2);
            int xr = new_temporary_register(g);
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
      V x = value_cast(g, generate_expression(g, n->bn.l), VALUE_KIND_INTEGER);
      Syntax_Node *rr = n->bn.r;
      while (rr and rr->k == N_CHK)
        rr = rr->chk.ex;
      if (rr and rr->k == N_RN)
      {
        V lo = value_cast(g, generate_expression(g, rr->rn.lo), VALUE_KIND_INTEGER),
          hi = value_cast(g, generate_expression(g, rr->rn.hi), VALUE_KIND_INTEGER);
        V ge = value_compare_integer(g, "sge", x, lo);
        V le = value_compare_integer(g, "sle", x, hi);
        V b1 = value_to_boolean(g, ge), b2 = value_to_boolean(g, le);
        int c1 = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
        int c2 = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
        int a1 = new_temporary_register(g);
        fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", a1, c1, c2);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, a1);
      }
      else if (rr and rr->k == N_ID)
      {
        Symbol *s = rr->sy ? rr->sy : symbol_find(g->sm, rr->s);
        if (s and s->ty)
        {
          Type_Info *t = type_canonical_concrete(s->ty);
          if (t)
          {
            int tlo = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, (long long) t->lo);
            int thi = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", thi, (long long) t->hi);
            V lo = {tlo, VALUE_KIND_INTEGER}, hi = {thi, VALUE_KIND_INTEGER};
            V ge = value_compare_integer(g, "sge", x, lo);
            V le = value_compare_integer(g, "sle", x, hi);
            V b1 = value_to_boolean(g, ge), b2 = value_to_boolean(g, le);
            int c1 = new_temporary_register(g);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c1, b1.id);
            int c2 = new_temporary_register(g);
            fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", c2, b2.id);
            int a1 = new_temporary_register(g);
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
    V a = generate_expression(g, n->bn.l), b = generate_expression(g, n->bn.r);
    if (op == T_EQ or op == T_NE)
    {
      Type_Info *lt = n->bn.l->ty ? type_canonical_concrete(n->bn.l->ty) : 0;
      Type_Info *rt = n->bn.r->ty ? type_canonical_concrete(n->bn.r->ty) : 0;
      if (lt and rt and lt->k == TYPE_ARRAY and rt->k == TYPE_ARRAY)
      {
        int sz = lt->hi >= lt->lo ? lt->hi - lt->lo + 1 : 1;
        r.k = VALUE_KIND_INTEGER;
        int res = new_temporary_register(g);
        fprintf(o, "  %%t%d = add i64 0, 1\n", res);
        for (int i = 0; i < sz; i++)
        {
          int ep1 = new_temporary_register(g);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep1, a.id, i);
          int lv = new_temporary_register(g);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lv, ep1);
          int ep2 = new_temporary_register(g);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep2, b.id, i);
          int rv = new_temporary_register(g);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", rv, ep2);
          int cmp = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", cmp, lv, rv);
          int ec = new_temporary_register(g);
          fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", ec, cmp);
          int nres = new_temporary_register(g);
          fprintf(o, "  %%t%d = and i64 %%t%d, %%t%d\n", nres, res, ec);
          res = nres;
        }
        int cmp_tmp = new_temporary_register(g);
        fprintf(o, "  %%t%d = %s i64 %%t%d, 1\n", cmp_tmp, op == T_EQ ? "icmp eq" : "icmp ne", res);
        fprintf(o, "  %%t%d = zext i1 %%t%d to i64\n", r.id, cmp_tmp);
        break;
      }
    }
    if (op == T_EX)
    {
      if (b.k == VALUE_KIND_FLOAT)
      {
        int bc = new_temporary_register(g);
        fprintf(o, "  %%t%d = fptosi double %%t%d to i64\n", bc, b.id);
        b.id = bc;
        b.k = VALUE_KIND_INTEGER;
      }
      V bi = value_cast(g, b, VALUE_KIND_INTEGER);
      int cf = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp slt i64 %%t%d, 0\n", cf, bi.id);
      int lt = new_label_block(g), lf = new_label_block(g);
      emit_conditional_branch(g, cf, lt, lf);
      lbl(g, lt);
      fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\nL%d:\n", lf);
      if (token_kind_to_value_kind(n->ty) == VALUE_KIND_FLOAT)
      {
        r = value_power_float(g, a, b);
      }
      else
      {
        r = value_power_integer(g, a, bi);
      }
      break;
    }
    if (op == T_PL or op == T_MN or op == T_ST or op == T_SL)
    {
      if (a.k == VALUE_KIND_FLOAT or b.k == VALUE_KIND_FLOAT)
      {
        a = value_cast(g, a, VALUE_KIND_FLOAT);
        b = value_cast(g, b, VALUE_KIND_FLOAT);
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
        a = value_cast(g, a, VALUE_KIND_INTEGER);
        b = value_cast(g, b, VALUE_KIND_INTEGER);
        if (op == T_SL)
        {
          int zc = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp eq i64 %%t%d, 0\n", zc, b.id);
          int ze = new_label_block(g), zd = new_label_block(g);
          emit_conditional_branch(g, zc, ze, zd);
          lbl(g, ze);
          fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
          lbl(g, zd);
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
      a = value_cast(g, a, VALUE_KIND_INTEGER);
      b = value_cast(g, b, VALUE_KIND_INTEGER);
      int zc = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp eq i64 %%t%d, 0\n", zc, b.id);
      int ze = new_label_block(g), zd = new_label_block(g);
      emit_conditional_branch(g, zc, ze, zd);
      lbl(g, ze);
      fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
      lbl(g, zd);
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = srem i64 %%t%d, %%t%d\n", r.id, a.id, b.id);
      break;
    }
    if (op == T_EQ or op == T_NE or op == T_LT or op == T_LE or op == T_GT or op == T_GE)
    {
      if((op==T_EQ or op==T_NE) and (n->bn.l->k==N_STR or n->bn.r->k==N_STR or (n->bn.l->ty and type_canonical_concrete(n->bn.l->ty)->el and type_canonical_concrete(n->bn.l->ty)->el->k==TYPE_CHARACTER) or (n->bn.r->ty and type_canonical_concrete(n->bn.r->ty)->el and type_canonical_concrete(n->bn.r->ty)->el->k==TYPE_CHARACTER)))
      {
        V ap = a, bp = b;
        if (ap.k == VALUE_KIND_INTEGER)
        {
          int p1 = new_temporary_register(g);
          fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", p1, ap.id);
          ap.id = p1;
          ap.k = VALUE_KIND_POINTER;
        }
        if (bp.k == VALUE_KIND_INTEGER)
        {
          int p2 = new_temporary_register(g);
          fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", p2, bp.id);
          bp.id = p2;
          bp.k = VALUE_KIND_POINTER;
        }
        int cmp = new_temporary_register(g);
        fprintf(o, "  %%t%d = call i32 @strcmp(ptr %%t%d, ptr %%t%d)\n", cmp, ap.id, bp.id);
        int eq = new_temporary_register(g);
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
        r = value_compare_float(g, cc, a, b);
      }
      else
      {
        const char *cc = op == T_EQ   ? "eq"
                         : op == T_NE ? "ne"
                         : op == T_LT ? "slt"
                         : op == T_LE ? "sle"
                         : op == T_GT ? "sgt"
                                      : "sge";
        r = value_compare_integer(g, cc, a, b);
      }
      break;
    }
    if (op == T_AM and (a.k == VALUE_KIND_POINTER or b.k == VALUE_KIND_POINTER))
    {
      Type_Info *lt = n->bn.l->ty ? type_canonical_concrete(n->bn.l->ty) : 0;
      Type_Info *rt = n->bn.r->ty ? type_canonical_concrete(n->bn.r->ty) : 0;
      int alo, ahi, blo, bhi;
      V ad, bd;
      bool la_fp = lt and lt->k == TYPE_ARRAY and lt->lo == 0 and lt->hi == -1;
      bool lb_fp = rt and rt->k == TYPE_ARRAY and rt->lo == 0 and rt->hi == -1;
      if (la_fp)
      {
        ad = get_fat_pointer_data(g, a.id);
        get_fat_pointer_bounds(g, a.id, &alo, &ahi);
      }
      else
      {
        ad = value_cast(g, a, VALUE_KIND_POINTER);
        alo = new_temporary_register(g);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            alo,
            lt and lt->k == TYPE_ARRAY ? (long long) lt->lo : 1LL);
        ahi = new_temporary_register(g);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            ahi,
            lt and lt->k == TYPE_ARRAY ? (long long) lt->hi : 0LL);
      }
      if (lb_fp)
      {
        bd = get_fat_pointer_data(g, b.id);
        get_fat_pointer_bounds(g, b.id, &blo, &bhi);
      }
      else
      {
        bd = value_cast(g, b, VALUE_KIND_POINTER);
        blo = new_temporary_register(g);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            blo,
            rt and rt->k == TYPE_ARRAY ? (long long) rt->lo : 1LL);
        bhi = new_temporary_register(g);
        fprintf(
            o,
            "  %%t%d = add i64 0, %lld\n",
            bhi,
            rt and rt->k == TYPE_ARRAY ? (long long) rt->hi : 0LL);
      }
      int alen = new_temporary_register(g);
      fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", alen, ahi, alo);
      int alen1 = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", alen1, alen);
      int blen = new_temporary_register(g);
      fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", blen, bhi, blo);
      int blen1 = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", blen1, blen);
      int tlen = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 %%t%d, %%t%d\n", tlen, alen1, blen1);
      int tlen1 = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", tlen1, tlen);
      int np = new_temporary_register(g);
      fprintf(o, "  %%t%d = call ptr @malloc(i64 %%t%d)\n", np, tlen1);
      fprintf(
          o,
          "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %%t%d, i1 false)\n",
          np,
          ad.id,
          alen1);
      int dp = new_temporary_register(g);
      fprintf(o, "  %%t%d = getelementptr i8, ptr %%t%d, i64 %%t%d\n", dp, np, alen1);
      fprintf(
          o,
          "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %%t%d, i1 false)\n",
          dp,
          bd.id,
          blen1);
      int zp = new_temporary_register(g);
      fprintf(o, "  %%t%d = getelementptr i8, ptr %%t%d, i64 %%t%d\n", zp, np, tlen);
      fprintf(o, "  store i8 0, ptr %%t%d\n", zp);
      int nlo = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 0, 1\n", nlo);
      int nhi = new_temporary_register(g);
      fprintf(o, "  %%t%d = sub i64 %%t%d, 1\n", nhi, tlen);
      r.k = VALUE_KIND_POINTER;
      r.id = new_temporary_register(g);
      generate_fat_pointer(g, r.id, np, nlo, nhi);
      break;
    }
    r.k = VALUE_KIND_INTEGER;
    {
      V ai = value_cast(g, a, VALUE_KIND_INTEGER), bi = value_cast(g, b, VALUE_KIND_INTEGER);
      fprintf(o, "  %%t%d = add i64 %%t%d, %%t%d\n", r.id, ai.id, bi.id);
    }
    break;
  }
  break;
  case N_UN:
  {
    V x = generate_expression(g, n->un.x);
    if (n->un.op == T_MN)
    {
      if (x.k == VALUE_KIND_FLOAT)
      {
        r.k = VALUE_KIND_FLOAT;
        fprintf(o, "  %%t%d = fsub double 0.0, %%t%d\n", r.id, x.id);
      }
      else
      {
        x = value_cast(g, x, VALUE_KIND_INTEGER);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = sub i64 0, %%t%d\n", r.id, x.id);
      }
      break;
    }
    if (n->un.op == T_NOT)
    {
      Type_Info *xt = n->un.x->ty ? type_canonical_concrete(n->un.x->ty) : 0;
      if (xt and xt->k == TYPE_ARRAY)
      {
        int sz = xt->hi >= xt->lo ? xt->hi - xt->lo + 1 : 1;
        int p = new_temporary_register(g);
        fprintf(o, "  %%t%d = alloca [%d x i64]\n", p, sz);
        for (int i = 0; i < sz; i++)
        {
          int ep1 = new_temporary_register(g);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %d\n", ep1, x.id, i);
          int lv = new_temporary_register(g);
          fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", lv, ep1);
          int res = new_temporary_register(g);
          fprintf(o, "  %%t%d = xor i64 %%t%d, 1\n", res, lv);
          int ep2 = new_temporary_register(g);
          fprintf(
              o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 %d\n", ep2, sz, p, i);
          fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", res, ep2);
        }
        r.k = VALUE_KIND_POINTER;
        fprintf(o, "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 0\n", r.id, sz, p);
      }
      else
      {
        V b = value_to_boolean(g, x);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = xor i64 %%t%d, 1\n", r.id, b.id);
      }
      break;
    }
    if (n->un.op == T_ABS)
    {
      if (x.k == VALUE_KIND_FLOAT)
      {
        V z = value_cast(g, x, VALUE_KIND_FLOAT);
        int c = new_temporary_register(g);
        fprintf(o, "  %%t%d = fcmp olt double %%t%d, 0.0\n", c, z.id);
        V ng = {new_temporary_register(g), VALUE_KIND_FLOAT};
        fprintf(o, "  %%t%d = fsub double 0.0, %%t%d\n", ng.id, z.id);
        r.k = VALUE_KIND_FLOAT;
        fprintf(o, "  %%t%d = select i1 %%t%d, double %%t%d, double %%t%d\n", r.id, c, ng.id, z.id);
      }
      else
      {
        V z = value_cast(g, x, VALUE_KIND_INTEGER);
        int c = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp slt i64 %%t%d, 0\n", c, z.id);
        V ng = {new_temporary_register(g), VALUE_KIND_INTEGER};
        fprintf(o, "  %%t%d = sub i64 0, %%t%d\n", ng.id, z.id);
        r.k = VALUE_KIND_INTEGER;
        fprintf(o, "  %%t%d = select i1 %%t%d, i64 %%t%d, i64 %%t%d\n", r.id, c, ng.id, z.id);
      }
      break;
    }
    r = value_cast(g, x, r.k);
    break;
  }
  break;
  case N_IX:
  {
    V p = generate_expression(g, n->ix.p);
    Type_Info *pt = n->ix.p->ty ? type_canonical_concrete(n->ix.p->ty) : 0;
    Type_Info *et = n->ty ? type_canonical_concrete(n->ty) : 0;
    bool is_char = et and et->k == TYPE_CHARACTER;
    int dp = p.id;
    if (pt and pt->k == TYPE_ARRAY and pt->lo == 0 and pt->hi == -1)
    {
      dp = get_fat_pointer_data(g, p.id).id;
      int blo, bhi;
      get_fat_pointer_bounds(g, p.id, &blo, &bhi);
      V i0 = value_cast(g, generate_expression(g, n->ix.ix.d[0]), VALUE_KIND_INTEGER);
      int adj = new_temporary_register(g);
      fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", adj, i0.id, blo);
      char lb[32], hb[32];
      snprintf(lb, 32, "%%t%d", blo);
      snprintf(hb, 32, "%%t%d", bhi);
      generate_index_constraint_check(g, i0.id, lb, hb);
      int ep = new_temporary_register(g);
      fprintf(
          o,
          "  %%t%d = getelementptr %s, ptr %%t%d, i64 %%t%d\n",
          ep,
          is_char ? "i8" : "i64",
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
          int lv = new_temporary_register(g);
          fprintf(o, "  %%t%d = load i8, ptr %%t%d\n", lv, ep);
          fprintf(o, "  %%t%d = zext i8 %%t%d to i64\n", r.id, lv);
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
        int pp = new_temporary_register(g);
        fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, p.id);
        dp = pp;
      }
      V i0 = value_cast(g, generate_expression(g, n->ix.ix.d[0]), VALUE_KIND_INTEGER);
      Type_Info *at = pt;
      int adj_idx = i0.id;
      if (at and at->k == TYPE_ARRAY and at->lo != 0)
      {
        int adj = new_temporary_register(g);
        fprintf(o, "  %%t%d = sub i64 %%t%d, %lld\n", adj, i0.id, (long long) at->lo);
        adj_idx = adj;
      }
      if (at and at->k == TYPE_ARRAY and not(at->sup & CHK_IDX) and (at->lo != 0 or at->hi != -1))
      {
        char lb[32], hb[32];
        snprintf(lb, 32, "%lld", (long long) at->lo);
        snprintf(hb, 32, "%lld", (long long) at->hi);
        generate_index_constraint_check(g, i0.id, lb, hb);
      }
      int ep = new_temporary_register(g);
      if (at and at->k == TYPE_ARRAY and at->hi >= at->lo)
      {
        int asz = (int) (at->hi - at->lo + 1);
        fprintf(
            o,
            "  %%t%d = getelementptr [%d x %s], ptr %%t%d, i64 0, i64 %%t%d\n",
            ep,
            asz,
            is_char ? "i8" : "i64",
            dp,
            adj_idx);
      }
      else
      {
        fprintf(
            o,
            "  %%t%d = getelementptr %s, ptr %%t%d, i64 %%t%d\n",
            ep,
            is_char ? "i8" : "i64",
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
          int lv = new_temporary_register(g);
          fprintf(o, "  %%t%d = load i8, ptr %%t%d\n", lv, ep);
          fprintf(o, "  %%t%d = zext i8 %%t%d to i64\n", r.id, lv);
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
    V p = generate_expression(g, n->sl.p);
    V lo = value_cast(g, generate_expression(g, n->sl.lo), VALUE_KIND_INTEGER);
    V hi = value_cast(g, generate_expression(g, n->sl.hi), VALUE_KIND_INTEGER);
    int ln = new_temporary_register(g);
    fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", ln, hi.id, lo.id);
    int sz = new_temporary_register(g);
    fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", sz, ln);
    int sl = new_temporary_register(g);
    fprintf(o, "  %%t%d = mul i64 %%t%d, 8\n", sl, sz);
    int ap = new_temporary_register(g);
    fprintf(o, "  %%t%d = alloca i8, i64 %%t%d\n", ap, sl);
    int sp = new_temporary_register(g);
    fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", sp, p.id, lo.id);
    fprintf(
        o,
        "  call void @llvm.memcpy.p0.p0.i64(ptr %%t%d, ptr %%t%d, i64 %%t%d, i1 false)\n",
        ap,
        sp,
        sl);
    r.k = VALUE_KIND_POINTER;
    r.id = new_temporary_register(g);
    generate_fat_pointer(g, r.id, ap, lo.id, hi.id);
  }
  break;
  case N_SEL:
  {
    Type_Info *pt = n->se.p->ty ? type_canonical_concrete(n->se.p->ty) : 0;
    V p = {new_temporary_register(g), VALUE_KIND_POINTER};
    if (n->se.p->k == N_ID)
    {
      Symbol *s = n->se.p->sy ? n->se.p->sy : symbol_find(g->sm, n->se.p->s);
      if (s and s->k != 6)
      {
        Type_Info *vty = s and s->ty ? type_canonical_concrete(s->ty) : 0;
        bool has_nested = false;
        if (vty and vty->k == TYPE_RECORD)
        {
          for (uint32_t ci = 0; ci < vty->dc.n; ci++)
          {
            Syntax_Node *fd = vty->dc.d[ci];
            Type_Info *fty =
                fd and fd->k == N_DS and fd->pm.ty ? resolve_subtype(g->sm, fd->pm.ty) : 0;
            if (fty and (fty->k == TYPE_RECORD or fty->k == TYPE_ARRAY))
            {
              has_nested = true;
              break;
            }
          }
          if (not has_nested)
          {
            for (uint32_t ci = 0; ci < vty->cm.n; ci++)
            {
              Syntax_Node *fc = vty->cm.d[ci];
              Type_Info *fty =
                  fc and fc->k == N_CM and fc->cm.ty ? resolve_subtype(g->sm, fc->cm.ty) : 0;
              if (fty and (fty->k == TYPE_RECORD or fty->k == TYPE_ARRAY))
              {
                has_nested = true;
                break;
              }
            }
          }
        }
        if (s->lv >= 0 and s->lv < g->sm->lv)
        {
          if (has_nested)
          {
            int tp = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", tp, s->el);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", p.id, tp);
          }
          else
            fprintf(
                o,
                "  %%t%d = bitcast ptr %%lnk.%d.%.*s to ptr\n",
                p.id,
                s->lv,
                (int) n->se.p->s.n,
                n->se.p->s.s);
        }
        else
        {
          if (has_nested)
            fprintf(
                o,
                "  %%t%d = load ptr, ptr %%v.%s.sc%u.%u\n",
                p.id,
                string_to_lowercase(n->se.p->s),
                s ? s->sc : 0,
                s ? s->el : 0);
          else
            fprintf(
                o,
                "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                p.id,
                string_to_lowercase(n->se.p->s),
                s ? s->sc : 0,
                s ? s->el : 0);
        }
      }
    }
    else
    {
      p = generate_expression(g, n->se.p);
    }
    if (pt and pt->k == TYPE_RECORD)
    {
      for (uint32_t i = 0; i < pt->dc.n; i++)
      {
        Syntax_Node *d = pt->dc.d[i];
        String_Slice dn = d->k == N_DS ? d->pm.nm : d->cm.nm;
        if (string_equal_ignore_case(dn, n->se.se))
        {
          int ep = new_temporary_register(g);
          fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, i);
          Type_Info *fty = d->k == N_DS ? resolve_subtype(g->sm, d->pm.ty) : 0;
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
      if (pt->pk)
      {
        for (uint32_t i = 0; i < pt->cm.n; i++)
        {
          Syntax_Node *c = pt->cm.d[i];
          if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, n->se.se))
          {
            int bp = new_temporary_register(g);
            fprintf(o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", bp, p.id);
            int bo = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 %%t%d, %u\n", bo, bp, c->cm.of / 8);
            int pp = new_temporary_register(g);
            fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, bo);
            int vp = new_temporary_register(g);
            fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", vp, pp);
            int sh = new_temporary_register(g);
            fprintf(o, "  %%t%d = lshr i64 %%t%d, %u\n", sh, vp, c->cm.of % 8);
            uint64_t mk = (1ULL << c->cm.bt) - 1;
            r.k = VALUE_KIND_INTEGER;
            fprintf(o, "  %%t%d = and i64 %%t%d, %llu\n", r.id, sh, (unsigned long long) mk);
            break;
          }
        }
      }
      else
      {
        for (uint32_t i = 0; i < pt->cm.n; i++)
        {
          Syntax_Node *c = pt->cm.d[i];
          if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, n->se.se))
          {
            int ep = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, c->cm.of);
            Type_Info *fty = resolve_subtype(g->sm, c->cm.ty);
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
      for (uint32_t i = 0; i < pt->cm.n; i++)
      {
        Syntax_Node *c = pt->cm.d[i];
        if (c->k == N_VP)
        {
          for (uint32_t j = 0; j < c->vp.vrr.n; j++)
          {
            Syntax_Node *v = c->vp.vrr.d[j];
            for (uint32_t k = 0; k < v->vr.cmm.n; k++)
            {
              Syntax_Node *vc = v->vr.cmm.d[k];
              if (string_equal_ignore_case(vc->cm.nm, n->se.se))
              {
                int ep = new_temporary_register(g);
                fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, vc->cm.of);
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
    else if (n->sy and n->sy->k == 5)
    {
      Syntax_Node *sp = symbol_spec(n->sy);
      if (sp and sp->sp.pmm.n == 0)
      {
        Value_Kind rk = sp and sp->sp.rt
                            ? token_kind_to_value_kind(resolve_subtype(g->sm, sp->sp.rt))
                            : VALUE_KIND_INTEGER;
        char fnb[256];
        encode_symbol_name(fnb, 256, n->sy, n->se.se, 0, sp);
        fprintf(o, "  %%t%d = call %s @\"%s\"()\n", r.id, value_llvm_type_string(rk), fnb);
        r.k = rk;
      }
    }
    else if (n->sy and n->sy->k == 2)
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) n->sy->vl);
    }
    else
    {
      r = value_cast(g, p, r.k);
    }
  }
  break;
  case N_AT:
  {
    String_Slice a = n->at.at;
    Type_Info *t = (n->at.p and n->at.p->ty) ? type_canonical_concrete(n->at.p->ty) : 0;
    if (string_equal_ignore_case(a, STRING_LITERAL("ADDRESS")))
    {
      if (n->at.p and n->at.p->k == N_ID)
      {
        Symbol *s = n->at.p->sy ? n->at.p->sy : symbol_find(g->sm, n->at.p->s);
        if (s)
        {
          r.k = VALUE_KIND_INTEGER;
          if (s->lv == 0)
          {
            char nb[256];
            if (s->pr and (uintptr_t) s->pr > 4096 and s->pr->nm.s)
            {
              int n = 0;
              for (uint32_t j = 0; j < s->pr->nm.n; j++)
                nb[n++] = toupper(s->pr->nm.s[j]);
              n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->pr->sc, s->pr->el);
              for (uint32_t j = 0; j < s->nm.n; j++)
                nb[n++] = toupper(s->nm.s[j]);
              nb[n] = 0;
            }
            else
              snprintf(nb, 256, "%.*s", (int) s->nm.n, s->nm.s);
            int p = new_temporary_register(g);
            fprintf(o, "  %%t%d = ptrtoint ptr @%s to i64\n", p, nb);
            r.id = p;
          }
          else if (s->lv >= 0 and s->lv < g->sm->lv)
          {
            int p = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", p, s->el);
            int a = new_temporary_register(g);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
            fprintf(o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", r.id, a);
          }
          else
          {
            int p = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 0, 1\n", p);
            r.id = p;
          }
        }
        else
        {
          V p = generate_expression(g, n->at.p);
          fprintf(
              o,
              "  %%t%d = ptrtoint ptr %%t%d to i64\n",
              r.id,
              value_cast(g, p, VALUE_KIND_POINTER).id);
        }
      }
      else if (n->at.p and n->at.p->k == N_AT)
      {
        Syntax_Node *ap = n->at.p;
        String_Slice ia = ap->at.at;
        if (string_equal_ignore_case(ia, STRING_LITERAL("PRED"))
            or string_equal_ignore_case(ia, STRING_LITERAL("SUCC"))
            or string_equal_ignore_case(ia, STRING_LITERAL("POS"))
            or string_equal_ignore_case(ia, STRING_LITERAL("VAL"))
            or string_equal_ignore_case(ia, STRING_LITERAL("IMAGE"))
            or string_equal_ignore_case(ia, STRING_LITERAL("VALUE")))
        {
          Type_Info *pt = ap->at.p ? type_canonical_concrete(ap->at.p->ty) : 0;
          String_Slice pnm =
              ap->at.p and ap->at.p->k == N_ID ? ap->at.p->s : STRING_LITERAL("TYPE");
          const char *afn = get_attribute_name(ia, pnm);
          r.k = VALUE_KIND_INTEGER;
          int p = new_temporary_register(g);
          fprintf(o, "  %%t%d = ptrtoint ptr %s to i64\n", p, afn);
          r.id = p;
        }
        else
        {
          V p = generate_expression(g, n->at.p);
          r.k = VALUE_KIND_INTEGER;
          fprintf(
              o,
              "  %%t%d = ptrtoint ptr %%t%d to i64\n",
              r.id,
              value_cast(g, p, VALUE_KIND_POINTER).id);
        }
      }
      else
      {
        V p = generate_expression(g, n->at.p);
        r.k = VALUE_KIND_INTEGER;
        fprintf(
            o,
            "  %%t%d = ptrtoint ptr %%t%d to i64\n",
            r.id,
            value_cast(g, p, VALUE_KIND_POINTER).id);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("SIZE")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) (t->sz * 8) : 64LL);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("FIRST"))
        or string_equal_ignore_case(a, STRING_LITERAL("LAST"))
        or string_equal_ignore_case(a, STRING_LITERAL("LENGTH")))
    {
      V pv = {0, VALUE_KIND_INTEGER};
      bool is_typ = n->at.p and n->at.p->k == N_ID and n->at.p->sy and n->at.p->sy->k == 1;
      if (n->at.p and not is_typ)
        pv = generate_expression(g, n->at.p);
      if (n->at.ar.n > 0)
        generate_expression(g, n->at.ar.d[0]);
      int64_t lo = 0, hi = -1;
      if (t and t->k == TYPE_ARRAY)
      {
        if (t->lo == 0 and t->hi == -1 and n->at.p and not is_typ)
        {
          int blo, bhi;
          get_fat_pointer_bounds(g, pv.id, &blo, &bhi);
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
            r.id = new_temporary_register(g);
            fprintf(o, "  %%t%d = sub i64 %%t%d, %%t%d\n", r.id, bhi, blo);
            int tmp = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", tmp, r.id);
            r.id = tmp;
          }
          break;
        }
        lo = t->lo;
        hi = t->hi;
      }
      else if (t and (is_integer_type(t) or t->k == TYPE_ENUMERATION))
      {
        lo = t->lo;
        hi = t->hi;
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
      V x = generate_expression(g, n->at.ar.d[0]);
      if (t
          and (t->k == TYPE_ENUMERATION or t->k == TYPE_INTEGER or t->k == TYPE_UNSIGNED_INTEGER or t->k == TYPE_DERIVED))
      {
        r = value_cast(g, x, VALUE_KIND_INTEGER);
      }
      else
      {
        r.k = VALUE_KIND_INTEGER;
        int tlo = new_temporary_register(g);
        fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, t ? (long long) t->lo : 0LL);
        fprintf(
            o,
            "  %%t%d = sub i64 %%t%d, %%t%d\n",
            r.id,
            value_cast(g, x, VALUE_KIND_INTEGER).id,
            tlo);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("VAL")))
    {
      V x = generate_expression(g, n->at.ar.d[0]);
      r.k = VALUE_KIND_INTEGER;
      int tlo = new_temporary_register(g);
      fprintf(o, "  %%t%d = add i64 0, %lld\n", tlo, t ? (long long) t->lo : 0LL);
      fprintf(
          o,
          "  %%t%d = add i64 %%t%d, %%t%d\n",
          r.id,
          value_cast(g, x, VALUE_KIND_INTEGER).id,
          tlo);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("SUCC"))
        or string_equal_ignore_case(a, STRING_LITERAL("PRED")))
    {
      V x = generate_expression(g, n->at.ar.d[0]);
      r.k = VALUE_KIND_INTEGER;
      fprintf(
          o,
          "  %%t%d = %s i64 %%t%d, 1\n",
          r.id,
          string_equal_ignore_case(a, STRING_LITERAL("SUCC")) ? "add" : "sub",
          value_cast(g, x, VALUE_KIND_INTEGER).id);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("IMAGE")))
    {
      V x = generate_expression(g, n->at.ar.d[0]);
      r.k = VALUE_KIND_POINTER;
      if (t and t->k == TYPE_ENUMERATION)
      {
        fprintf(
            o,
            "  %%t%d = call ptr @__ada_image_enum(i64 %%t%d, i64 %lld, i64 %lld)\n",
            r.id,
            value_cast(g, x, VALUE_KIND_INTEGER).id,
            t ? (long long) t->lo : 0LL,
            t ? (long long) t->hi : 127LL);
      }
      else
      {
        fprintf(
            o,
            "  %%t%d = call ptr @__ada_image_int(i64 %%t%d)\n",
            r.id,
            value_cast(g, x, VALUE_KIND_INTEGER).id);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("VALUE")))
    {
      V x = generate_expression(g, n->at.ar.d[0]);
      r.k = VALUE_KIND_INTEGER;
      if (t and t->k == TYPE_ENUMERATION)
      {
        V buf = get_fat_pointer_data(g, x.id);
        int fnd = new_temporary_register(g);
        fprintf(o, "  %%t%d = add i64 0, -1\n", fnd);
        for (uint32_t i = 0; i < t->ev.n; i++)
        {
          Symbol *e = t->ev.d[i];
          int sz = e->nm.n + 1;
          int p = new_temporary_register(g);
          fprintf(o, "  %%t%d = alloca [%d x i8]\n", p, sz);
          for (uint32_t j = 0; j < e->nm.n; j++)
          {
            int ep = new_temporary_register(g);
            fprintf(
                o, "  %%t%d = getelementptr [%d x i8], ptr %%t%d, i64 0, i64 %u\n", ep, sz, p, j);
            fprintf(o, "  store i8 %d, ptr %%t%d\n", (int) (unsigned char) e->nm.s[j], ep);
          }
          int zp = new_temporary_register(g);
          fprintf(
              o,
              "  %%t%d = getelementptr [%d x i8], ptr %%t%d, i64 0, i64 %u\n",
              zp,
              sz,
              p,
              (unsigned int) e->nm.n);
          fprintf(o, "  store i8 0, ptr %%t%d\n", zp);
          int sp = new_temporary_register(g);
          fprintf(o, "  %%t%d = getelementptr [%d x i8], ptr %%t%d, i64 0, i64 0\n", sp, sz, p);
          int cmp = new_temporary_register(g);
          fprintf(o, "  %%t%d = call i32 @strcmp(ptr %%t%d, ptr %%t%d)\n", cmp, buf.id, sp);
          int eq = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", eq, cmp);
          int nfnd = new_temporary_register(g);
          fprintf(
              o, "  %%t%d = select i1 %%t%d, i64 %lld, i64 %%t%d\n", nfnd, eq, (long long) i, fnd);
          fnd = nfnd;
        }
        int chk = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp slt i64 %%t%d, 0\n", chk, fnd);
        int le = new_label_block(g), ld = new_label_block(g);
        emit_conditional_branch(g, chk, le, ld);
        lbl(g, le);
        fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
        lbl(g, ld);
        fprintf(o, "  %%t%d = add i64 %%t%d, %lld\n", r.id, fnd, t ? (long long) t->lo : 0LL);
      }
      else
      {
        fprintf(
            o,
            "  %%t%d = call i64 @__ada_value_int(ptr %%t%d)\n",
            r.id,
            value_cast(g, x, VALUE_KIND_POINTER).id);
      }
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("DIGITS")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) t->sm : 15LL);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("DELTA")))
    {
      r.k = VALUE_KIND_FLOAT;
      fprintf(
          o, "  %%t%d = fadd double 0.0, %e\n", r.id, t ? 1.0 / pow(2.0, (double) t->sm) : 0.01);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("SMALL"))
        or string_equal_ignore_case(a, STRING_LITERAL("LARGE"))
        or string_equal_ignore_case(a, STRING_LITERAL("EPSILON")))
    {
      r.k = VALUE_KIND_FLOAT;
      double v = string_equal_ignore_case(a, STRING_LITERAL("SMALL")) ? pow(2.0, -126.0)
                 : string_equal_ignore_case(a, STRING_LITERAL("LARGE"))
                     ? (t and t->sm > 0 ? (pow(2.0, ceil((double) t->sm * log2(10.0)) + 1.0) - 1.0)
                                              * pow(2.0, 63.0)
                                        : 1.0e308)
                     : pow(2.0, t ? -(double) t->sm : -52.0);
      fprintf(o, "  %%t%d = fadd double 0.0, %e\n", r.id, v);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("MANTISSA"))
        or string_equal_ignore_case(a, STRING_LITERAL("MACHINE_MANTISSA")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) t->sm : 53LL);
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
      if (t and t->sm > 0)
      {
        while (pow(10.0, (double) dg) * pow(2.0, -(double) t->sm) < 1.0)
          dg++;
      }
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) dg);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("FORE")))
    {
      r.k = VALUE_KIND_INTEGER;
      int64_t fw = 2;
      if (t and t->hi > 0)
      {
        int64_t mx = t->hi;
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
          for (uint32_t i = 0; i < t->ev.n; i++)
          {
            Symbol *e = t->ev.d[i];
            int64_t ln = e->nm.n;
            if (ln > wd)
              wd = ln;
          }
        }
        else
        {
          int64_t mx = t->hi > -t->lo ? t->hi : -t->lo;
          while (mx >= 10)
          {
            mx /= 10;
            wd++;
          }
          if (t->lo < 0)
            wd++;
        }
      }
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, (long long) wd);
    }
    else if (string_equal_ignore_case(a, STRING_LITERAL("STORAGE_SIZE")))
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, %lld\n", r.id, t ? (long long) (t->sz * 8) : 0LL);
    }
    else if (
        string_equal_ignore_case(a, STRING_LITERAL("POSITION"))
        or string_equal_ignore_case(a, STRING_LITERAL("FIRST_BIT"))
        or string_equal_ignore_case(a, STRING_LITERAL("LAST_BIT")))
    {
      r.k = VALUE_KIND_INTEGER;
      int64_t v = 0;
      if (n->at.p and n->at.p->k == N_SEL)
      {
        Type_Info *pt = n->at.p->se.p->ty ? type_canonical_concrete(n->at.p->se.p->ty) : 0;
        if (pt and pt->k == TYPE_RECORD)
        {
          for (uint32_t i = 0; i < pt->cm.n; i++)
          {
            Syntax_Node *c = pt->cm.d[i];
            if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, n->at.p->se.se))
            {
              if (pt->pk)
              {
                if (string_equal_ignore_case(a, STRING_LITERAL("POSITION")))
                  v = c->cm.of / 8;
                else if (string_equal_ignore_case(a, STRING_LITERAL("FIRST_BIT")))
                  v = c->cm.of % 8;
                else if (string_equal_ignore_case(a, STRING_LITERAL("LAST_BIT")))
                  v = (c->cm.of % 8) + c->cm.bt - 1;
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
      V p = generate_expression(g, n->at.p);
      r = value_cast(g, p, VALUE_KIND_POINTER);
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
    V q = generate_expression(g, n->ql.ag);
    r = value_cast(g, q, r.k);
  }
  break;
  case N_CL:
  {
    if (not n->cl.fn or (uintptr_t) n->cl.fn < 4096)
    {
      r.k = VALUE_KIND_INTEGER;
      fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
      break;
    }
    if (n->cl.fn->k == N_ID)
    {
      Symbol *s = symbol_find_with_arity(g->sm, n->cl.fn->s, n->cl.ar.n, n->ty);
      if (s)
      {
        if (s->pr and string_equal_ignore_case(s->pr->nm, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->nm, STRING_LITERAL("CREATE")) or string_equal_ignore_case(s->nm, STRING_LITERAL("OPEN"))))
        {
          r.k = VALUE_KIND_POINTER;
          int md = n->cl.ar.n > 1 ? generate_expression(g, n->cl.ar.d[1]).id : 0;
          fprintf(
              o,
              "  %%t%d = call ptr @__text_io_%s(i64 %d, ptr %%t%d)\n",
              r.id,
              string_equal_ignore_case(s->nm, STRING_LITERAL("CREATE")) ? "create" : "open",
              md,
              n->cl.ar.n > 2 ? generate_expression(g, n->cl.ar.d[2]).id : 0);
          break;
        }
        if (s->pr and string_equal_ignore_case(s->pr->nm, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->nm, STRING_LITERAL("CLOSE")) or string_equal_ignore_case(s->nm, STRING_LITERAL("DELETE"))))
        {
          if (n->cl.ar.n > 0)
          {
            V f = generate_expression(g, n->cl.ar.d[0]);
            fprintf(
                o,
                "  call void @__text_io_%s(ptr %%t%d)\n",
                string_equal_ignore_case(s->nm, STRING_LITERAL("CLOSE")) ? "close" : "delete",
                f.id);
          }
          break;
        }
        if (s->pr and string_equal_ignore_case(s->pr->nm, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->nm, STRING_LITERAL("GET")) or string_equal_ignore_case(s->nm, STRING_LITERAL("GET_LINE"))))
        {
          if (n->cl.ar.n > 1)
          {
            V f = generate_expression(g, n->cl.ar.d[0]);
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
        if (s->pr and string_equal_ignore_case(s->pr->nm, STRING_LITERAL("TEXT_IO"))
            and (string_equal_ignore_case(s->nm, STRING_LITERAL("PUT")) or string_equal_ignore_case(s->nm, STRING_LITERAL("PUT_LINE"))))
        {
          if (n->cl.ar.n > 1)
          {
            V f = generate_expression(g, n->cl.ar.d[0]);
            V v = generate_expression(g, n->cl.ar.d[1]);
            fprintf(
                o,
                "  call void @__text_io_%s(ptr %%t%d, ",
                string_equal_ignore_case(s->nm, STRING_LITERAL("PUT")) ? "put" : "put_line",
                f.id);
            if (v.k == VALUE_KIND_INTEGER)
              fprintf(o, "i64 %%t%d)\n", v.id);
            else if (v.k == VALUE_KIND_FLOAT)
              fprintf(o, "double %%t%d)\n", v.id);
            else
              fprintf(o, "ptr %%t%d)\n", v.id);
          }
          else
          {
            V v = generate_expression(g, n->cl.ar.d[0]);
            fprintf(
                o,
                "  call void @__text_io_%s(ptr @stdout, ",
                string_equal_ignore_case(s->nm, STRING_LITERAL("PUT")) ? "put" : "put_line");
            if (v.k == VALUE_KIND_INTEGER)
              fprintf(o, "i64 %%t%d)\n", v.id);
            else if (v.k == VALUE_KIND_FLOAT)
              fprintf(o, "double %%t%d)\n", v.id);
            else
              fprintf(o, "ptr %%t%d)\n", v.id);
          }
          break;
        }
        if (s->ty and s->ty->k == TYPE_STRING)
        {
          Value_Kind rk = token_kind_to_value_kind(s->ty->el);
          r.k = rk;
          Syntax_Node *b = symbol_body(s, s->el);
          Syntax_Node *sp = symbol_spec(s);
          int arid[64];
          Value_Kind ark[64];
          int arp[64];
          for (uint32_t i = 0; i < n->cl.ar.n and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->sp.pmm.n ? sp->sp.pmm.d[i] : 0;
            Syntax_Node *arg = n->cl.ar.d[i];
            V av = {0, VALUE_KIND_INTEGER};
            Value_Kind ek = VALUE_KIND_INTEGER;
            bool rf = false;
            if (pm)
            {
              if (pm->sy and pm->sy->ty)
                ek = token_kind_to_value_kind(pm->sy->ty);
              else if (pm->pm.ty)
              {
                Type_Info *pt = resolve_subtype(g->sm, pm->pm.ty);
                ek = token_kind_to_value_kind(pt);
              }
              if (pm->pm.md & 2 and arg->k == N_ID)
              {
                rf = true;
                Symbol *as = arg->sy ? arg->sy : symbol_find(g->sm, arg->s);
                if (as and as->lv == 0)
                {
                  char nb[256];
                  if (as->pr and as->pr->nm.s)
                  {
                    int n = 0;
                    for (uint32_t j = 0; j < as->pr->nm.n; j++)
                      nb[n++] = toupper(as->pr->nm.s[j]);
                    n += snprintf(nb + n, 256 - n, "_S%dE%d__", as->pr->sc, as->pr->el);
                    for (uint32_t j = 0; j < as->nm.n; j++)
                      nb[n++] = toupper(as->nm.s[j]);
                    nb[n] = 0;
                  }
                  else
                    snprintf(nb, 256, "%.*s", (int) as->nm.n, as->nm.s);
                  av.id = new_temporary_register(g);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = bitcast ptr @%s to ptr\n", av.id, nb);
                }
                else if (as and as->lv >= 0 and as->lv < g->sm->lv)
                {
                  av.id = new_temporary_register(g);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", av.id, as->el);
                  int a2 = new_temporary_register(g);
                  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a2, av.id);
                  av.id = a2;
                }
                else
                {
                  av.id = new_temporary_register(g);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(
                      o,
                      "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                      av.id,
                      string_to_lowercase(arg->s),
                      as ? as->sc : 0,
                      as ? as->el : 0);
                }
                ek = VALUE_KIND_POINTER;
              }
              else if (pm->pm.md & 2)
              {
                rf = true;
                av = generate_expression(g, arg);
                int ap = new_temporary_register(g);
                fprintf(o, "  %%t%d = alloca %s\n", ap, value_llvm_type_string(av.k));
                fprintf(
                    o, "  store %s %%t%d, ptr %%t%d\n", value_llvm_type_string(av.k), av.id, ap);
                av.id = ap;
                av.k = VALUE_KIND_POINTER;
                ek = VALUE_KIND_POINTER;
              }
              else
              {
                av = generate_expression(g, arg);
              }
            }
            else
            {
              av = generate_expression(g, arg);
            }
            if (not rf and ek != VALUE_KIND_INTEGER)
            {
              V cv = value_cast(g, av, ek);
              av = cv;
            }
            arid[i] = av.id;
            ark[i] = ek != VALUE_KIND_INTEGER ? ek : av.k;
            arp[i] = i < sp->sp.pmm.n and sp->sp.pmm.d[i]->pm.md & 2 ? 1 : 0;
          }
          char nb[256];
          encode_symbol_name(nb, 256, s, n->cl.fn->s, n->cl.ar.n, sp);
          fprintf(o, "  %%t%d = call %s @\"%s\"(", r.id, value_llvm_type_string(rk), nb);
          for (uint32_t i = 0; i < n->cl.ar.n; i++)
          {
            if (i)
              fprintf(o, ", ");
            fprintf(o, "%s %%t%d", value_llvm_type_string(ark[i]), arid[i]);
          }
          if (s->lv > 0)
          {
            if (n->cl.ar.n > 0)
              fprintf(o, ", ");
            if (s->lv >= g->sm->lv)
              fprintf(o, "ptr %%__frame");
            else
              fprintf(o, "ptr %%__slnk");
          }
          fprintf(o, ")\n");
          for (uint32_t i = 0; i < n->cl.ar.n and i < 64; i++)
          {
            if (arp[i])
            {
              int lv = new_temporary_register(g);
              fprintf(
                  o,
                  "  %%t%d = load %s, ptr %%t%d\n",
                  lv,
                  value_llvm_type_string(
                      ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]),
                  arid[i]);
              V rv = {lv, ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]};
              V cv = value_cast(g, rv, token_kind_to_value_kind(n->cl.ar.d[i]->ty));
              Syntax_Node *tg = n->cl.ar.d[i];
              if (tg->k == N_ID)
              {
                Symbol *ts = tg->sy ? tg->sy : symbol_find(g->sm, tg->s);
                if (ts)
                {
                  if (ts->lv >= 0 and ts->lv < g->sm->lv)
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        ts->lv,
                        string_to_lowercase(tg->s));
                  else
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        string_to_lowercase(tg->s),
                        ts->sc,
                        ts->el);
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
    return generate_aggregate(g, n, n->ty);
  case N_ALC:
  {
    r.k = VALUE_KIND_POINTER;
    Type_Info *et = n->ty and n->ty->el ? type_canonical_concrete(n->ty->el) : 0;
    uint32_t asz = 64;
    if (et and et->dc.n > 0)
      asz += et->dc.n * 8;
    fprintf(o, "  %%t%d = call ptr @malloc(i64 %u)\n", r.id, asz);
    if (n->alc.in)
    {
      V v = generate_expression(g, n->alc.in);
      v = value_cast(g, v, VALUE_KIND_INTEGER);
      int op = new_temporary_register(g);
      fprintf(
          o,
          "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n",
          op,
          r.id,
          et and et->dc.n > 0 ? (uint32_t) et->dc.n : 0);
      fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, op);
    }
  }
  break;
  case N_DRF:
  {
    V p = generate_expression(g, n->drf.x);
    if (p.k == VALUE_KIND_INTEGER)
    {
      int pp = new_temporary_register(g);
      fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, p.id);
      p.id = pp;
      p.k = VALUE_KIND_POINTER;
    }
    Type_Info *dt = n->drf.x->ty ? type_canonical_concrete(n->drf.x->ty) : 0;
    dt = dt and dt->el ? type_canonical_concrete(dt->el) : 0;
    r.k = dt ? token_kind_to_value_kind(dt) : VALUE_KIND_INTEGER;
    V pc = value_cast(g, p, VALUE_KIND_INTEGER);
    int nc = new_temporary_register(g);
    fprintf(o, "  %%t%d = icmp eq i64 %%t%d, 0\n", nc, pc.id);
    int ne = new_label_block(g), nd = new_label_block(g);
    emit_conditional_branch(g, nc, ne, nd);
    lbl(g, ne);
    fprintf(o, "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
    lbl(g, nd);
    fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", r.id, value_llvm_type_string(r.k), p.id);
  }
  break;
  case N_CVT:
  {
    V e = generate_expression(g, n->cvt.ex);
    r = value_cast(g, e, r.k);
  }
  break;
  case N_CHK:
  {
    V e = generate_expression(g, n->chk.ex);
    Type_Info *t = n->chk.ex->ty ? type_canonical_concrete(n->chk.ex->ty) : 0;
    if (t and t->k == TYPE_FLOAT and (t->lo != TY_INT->lo or t->hi != TY_INT->hi))
      r = generate_float_range_check(g, e, t, n->chk.ec, r.k);
    else if (t and t->k == TYPE_ARRAY and (t->lo != 0 or t->hi != -1))
    {
      Type_Info *et = n->chk.ex->ty;
      r = generate_array_bounds_check(g, e, t, et, n->chk.ec, r.k);
    }
    else if (
        t
        and (t->k == TYPE_INTEGER or t->k == TYPE_ENUMERATION or t->k == TYPE_DERIVED or t->k == TYPE_CHARACTER)
        and (t->lo != TY_INT->lo or t->hi != TY_INT->hi))
      r = generate_discrete_range_check(g, e, t, n->chk.ec, r.k);
    else
      r = value_cast(g, e, r.k);
  }
  break;
  case N_RN:
  {
    V lo = generate_expression(g, n->rn.lo);
    r = value_cast(g, lo, r.k);
  }
  break;
  default:
    r.k = VALUE_KIND_INTEGER;
    fprintf(o, "  %%t%d = add i64 0, 0\n", r.id);
  }
  return r;
}
static void generate_statement_sequence(Code_Generator *g, Syntax_Node *n)
{
  FILE *o = g->o;
  if (not n)
    return;
  switch (n->k)
  {
  case N_NS:
    fprintf(o, "  ; null\n");
    break;
  case N_AS:
  {
    V v = generate_expression(g, n->as.vl);
    if (n->as.tg->k == N_ID)
    {
      Symbol *s = n->as.tg->sy;
      Value_Kind k = s and s->ty ? token_kind_to_value_kind(s->ty) : VALUE_KIND_INTEGER;
      v = value_cast(g, v, k);
      if (s and s->lv == 0)
      {
        char nb[256];
        if (s->pr and (uintptr_t) s->pr > 4096 and s->pr->nm.s)
        {
          int n = 0;
          for (uint32_t i = 0; i < s->pr->nm.n; i++)
            nb[n++] = toupper(s->pr->nm.s[i]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->pr->sc, s->pr->el);
          for (uint32_t i = 0; i < s->nm.n; i++)
            nb[n++] = toupper(s->nm.s[i]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->nm.n, s->nm.s);
        fprintf(o, "  store %s %%t%d, ptr @%s\n", value_llvm_type_string(k), v.id, nb);
      }
      else if (s and s->lv >= 0 and s->lv < g->sm->lv)
      {
        int p = new_temporary_register(g);
        int level_diff = g->sm->lv - s->lv - 1;
        int slnk_ptr;
        if (level_diff == 0)
        {
          slnk_ptr = new_temporary_register(g);
          fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
        }
        else
        {
          slnk_ptr = new_temporary_register(g);
          fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
          for (int hop = 0; hop < level_diff; hop++)
          {
            int next_slnk = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
            int loaded_slnk = new_temporary_register(g);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
            slnk_ptr = loaded_slnk;
          }
        }
        fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->el);
        int a = new_temporary_register(g);
        fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a, p);
        fprintf(o, "  store %s %%t%d, ptr %%t%d\n", value_llvm_type_string(k), v.id, a);
      }
      else
        fprintf(
            o,
            "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
            value_llvm_type_string(k),
            v.id,
            string_to_lowercase(n->as.tg->s),
            s ? s->sc : 0,
            s ? s->el : 0);
    }
    else if (n->as.tg->k == N_IX)
    {
      V p = generate_expression(g, n->as.tg->ix.p);
      if (p.k == VALUE_KIND_INTEGER)
      {
        int pp = new_temporary_register(g);
        fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, p.id);
        p.id = pp;
        p.k = VALUE_KIND_POINTER;
      }
      V i0 = value_cast(g, generate_expression(g, n->as.tg->ix.ix.d[0]), VALUE_KIND_INTEGER);
      Type_Info *at = n->as.tg->ix.p->ty ? type_canonical_concrete(n->as.tg->ix.p->ty) : 0;
      int adj_idx = i0.id;
      if (at and at->k == TYPE_ARRAY and at->lo != 0)
      {
        int adj = new_temporary_register(g);
        fprintf(o, "  %%t%d = sub i64 %%t%d, %lld\n", adj, i0.id, (long long) at->lo);
        adj_idx = adj;
      }
      int ep = new_temporary_register(g);
      if (at and at->k == TYPE_ARRAY and at->hi >= at->lo)
      {
        int asz = (int) (at->hi - at->lo + 1);
        fprintf(
            o,
            "  %%t%d = getelementptr [%d x i64], ptr %%t%d, i64 0, i64 %%t%d\n",
            ep,
            asz,
            p.id,
            adj_idx);
      }
      else
      {
        fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %%t%d\n", ep, p.id, adj_idx);
      }
      v = value_cast(g, v, VALUE_KIND_INTEGER);
      fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
    }
    else if (n->as.tg->k == N_SEL)
    {
      Type_Info *pt = n->as.tg->se.p->ty ? type_canonical_concrete(n->as.tg->se.p->ty) : 0;
      V p = {new_temporary_register(g), VALUE_KIND_POINTER};
      if (n->as.tg->se.p->k == N_ID)
      {
        Symbol *s = n->as.tg->se.p->sy ? n->as.tg->se.p->sy : symbol_find(g->sm, n->as.tg->se.p->s);
        if (s and s->lv >= 0 and s->lv < g->sm->lv)
          fprintf(
              o,
              "  %%t%d = bitcast ptr %%lnk.%d.%.*s to ptr\n",
              p.id,
              s->lv,
              (int) n->as.tg->se.p->s.n,
              n->as.tg->se.p->s.s);
        else
          fprintf(
              o,
              "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
              p.id,
              string_to_lowercase(n->as.tg->se.p->s),
              s ? s->sc : 0,
              s ? s->el : 0);
      }
      else
      {
        p = generate_expression(g, n->as.tg->se.p);
      }
      if (pt and pt->k == TYPE_RECORD)
      {
        if (pt->pk)
        {
          for (uint32_t i = 0; i < pt->cm.n; i++)
          {
            Syntax_Node *c = pt->cm.d[i];
            if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, n->as.tg->se.se))
            {
              v = value_cast(g, v, VALUE_KIND_INTEGER);
              int bp = new_temporary_register(g);
              fprintf(o, "  %%t%d = ptrtoint ptr %%t%d to i64\n", bp, p.id);
              int bo = new_temporary_register(g);
              fprintf(o, "  %%t%d = add i64 %%t%d, %u\n", bo, bp, c->cm.of / 8);
              int pp = new_temporary_register(g);
              fprintf(o, "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, bo);
              int ov = new_temporary_register(g);
              fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", ov, pp);
              uint64_t mk = (1ULL << c->cm.bt) - 1;
              int sh = new_temporary_register(g);
              fprintf(o, "  %%t%d = shl i64 %%t%d, %u\n", sh, v.id, c->cm.of % 8);
              int ms = new_temporary_register(g);
              fprintf(
                  o,
                  "  %%t%d = and i64 %%t%d, %llu\n",
                  ms,
                  sh,
                  (unsigned long long) (mk << (c->cm.of % 8)));
              uint64_t cmk = ~(mk << (c->cm.of % 8));
              int cl = new_temporary_register(g);
              fprintf(o, "  %%t%d = and i64 %%t%d, %llu\n", cl, ov, (unsigned long long) cmk);
              int nv_ = new_temporary_register(g);
              fprintf(o, "  %%t%d = or i64 %%t%d, %%t%d\n", nv_, cl, ms);
              fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", nv_, pp);
              break;
            }
          }
        }
        else
        {
          for (uint32_t i = 0; i < pt->cm.n; i++)
          {
            Syntax_Node *c = pt->cm.d[i];
            if (c->k == N_CM and string_equal_ignore_case(c->cm.nm, n->as.tg->se.se))
            {
              int ep = new_temporary_register(g);
              fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ep, p.id, c->cm.of);
              v = value_cast(g, v, VALUE_KIND_INTEGER);
              fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", v.id, ep);
              break;
            }
          }
        }
      }
    }
    else
    {
      v = value_cast(g, v, VALUE_KIND_INTEGER);
      fprintf(o, "  ; store to complex lvalue\n");
    }
  }
  break;
  case N_IF:
  {
    V c = value_to_boolean(g, generate_expression(g, n->if_.cd));
    int ct = new_temporary_register(g);
    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
    int lt = new_label_block(g), lf = new_label_block(g), ld = new_label_block(g);
    emit_conditional_branch(g, ct, lt, lf);
    lbl(g, lt);
    for (uint32_t i = 0; i < n->if_.th.n; i++)
      generate_statement_sequence(g, n->if_.th.d[i]);
    emit_branch(g, ld);
    lbl(g, lf);
    if (n->if_.ei.n > 0)
    {
      for (uint32_t i = 0; i < n->if_.ei.n; i++)
      {
        Syntax_Node *e = n->if_.ei.d[i];
        V ec = value_to_boolean(g, generate_expression(g, e->if_.cd));
        int ect = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ect, ec.id);
        int let = new_label_block(g), lef = new_label_block(g);
        emit_conditional_branch(g, ect, let, lef);
        lbl(g, let);
        for (uint32_t j = 0; j < e->if_.th.n; j++)
          generate_statement_sequence(g, e->if_.th.d[j]);
        emit_branch(g, ld);
        lbl(g, lef);
      }
    }
    if (n->if_.el.n > 0)
    {
      for (uint32_t i = 0; i < n->if_.el.n; i++)
        generate_statement_sequence(g, n->if_.el.d[i]);
    }
    emit_branch(g, ld);
    lbl(g, ld);
  }
  break;
  case N_CS:
  {
    V ex = generate_expression(g, n->cs.ex);
    int ld = new_label_block(g);
    Node_Vector lb = {0};
    for (uint32_t i = 0; i < n->cs.al.n; i++)
    {
      Syntax_Node *a = n->cs.al.d[i];
      int la = new_label_block(g);
      nv(&lb, ND(INT, n->l));
      lb.d[i]->i = la;
    }
    for (uint32_t i = 0; i < n->cs.al.n; i++)
    {
      Syntax_Node *a = n->cs.al.d[i];
      int la = lb.d[i]->i;
      for (uint32_t j = 0; j < a->ch.it.n; j++)
      {
        Syntax_Node *ch = a->ch.it.d[j];
        if (ch->k == N_ID and string_equal_ignore_case(ch->s, STRING_LITERAL("others")))
        {
          emit_branch(g, la);
          goto cs_sw_done;
        }
        Type_Info *cht = ch->ty ? type_canonical_concrete(ch->ty) : 0;
        if (ch->k == N_ID and cht and (cht->lo != 0 or cht->hi != 0))
        {
          int lo_id = new_temporary_register(g);
          fprintf(o, "  %%t%d = add i64 0, %lld\n", lo_id, (long long) cht->lo);
          int hi_id = new_temporary_register(g);
          fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_id, (long long) cht->hi);
          int cge = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %%t%d\n", cge, ex.id, lo_id);
          int cle = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", cle, ex.id, hi_id);
          int ca = new_temporary_register(g);
          fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", ca, cge, cle);
          int lnx = i + 1 < n->cs.al.n ? lb.d[i + 1]->i : ld;
          emit_conditional_branch(g, ca, la, lnx);
          continue;
        }
        V cv = generate_expression(g, ch);
        cv = value_cast(g, cv, ex.k);
        if (ch->k == N_RN)
        {
          V lo = generate_expression(g, ch->rn.lo);
          lo = value_cast(g, lo, ex.k);
          V hi = generate_expression(g, ch->rn.hi);
          hi = value_cast(g, hi, ex.k);
          int cge = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp sge i64 %%t%d, %%t%d\n", cge, ex.id, lo.id);
          int cle = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", cle, ex.id, hi.id);
          int ca = new_temporary_register(g);
          fprintf(o, "  %%t%d = and i1 %%t%d, %%t%d\n", ca, cge, cle);
          int lnx = i + 1 < n->cs.al.n ? lb.d[i + 1]->i : ld;
          emit_conditional_branch(g, ca, la, lnx);
        }
        else
        {
          int ceq = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", ceq, ex.id, cv.id);
          int lnx = i + 1 < n->cs.al.n ? lb.d[i + 1]->i : ld;
          emit_conditional_branch(g, ceq, la, lnx);
        }
      }
    }
  cs_sw_done:;
    for (uint32_t i = 0; i < n->cs.al.n; i++)
    {
      Syntax_Node *a = n->cs.al.d[i];
      int la = lb.d[i]->i;
      lbl(g, la);
      for (uint32_t j = 0; j < a->hnd.stz.n; j++)
        generate_statement_sequence(g, a->hnd.stz.d[j]);
      emit_branch(g, ld);
    }
    lbl(g, ld);
  }
  break;
  case N_LP:
  {
    int lb = new_label_block(g), lc = new_label_block(g), le = new_label_block(g);
    if (g->ls < 64)
      g->ll[g->ls++] = le;
    if (n->lp.lb.s)
    {
      slv(&g->lbs, n->lp.lb);
      nv(&n->lp.lk, ND(INT, n->l));
      n->lp.lk.d[n->lp.lk.n - 1]->i = le;
    }
    Syntax_Node *fv = 0;
    Type_Info *ft = 0;
    int hi_var = -1;
    if (n->lp.it and n->lp.it->k == N_BIN and n->lp.it->bn.op == T_IN and n->lp.it->bn.l->k == N_ID)
    {
      fv = n->lp.it->bn.l;
      ft = n->lp.it->bn.r->ty;
      if (ft)
      {
        Symbol *vs = fv->sy;
        if (vs)
        {
          fprintf(o, "  %%v.%s.sc%u.%u = alloca i64\n", string_to_lowercase(fv->s), vs->sc, vs->el);
          Syntax_Node *rng = n->lp.it->bn.r;
          hi_var = new_temporary_register(g);
          fprintf(o, "  %%v.__for_hi_%d = alloca i64\n", hi_var);
          int ti = new_temporary_register(g);
          if (rng and rng->k == N_RN)
          {
            V lo = value_cast(g, generate_expression(g, rng->rn.lo), VALUE_KIND_INTEGER);
            fprintf(o, "  %%t%d = add i64 %%t%d, 0\n", ti, lo.id);
            V hi = value_cast(g, generate_expression(g, rng->rn.hi), VALUE_KIND_INTEGER);
            fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi.id, hi_var);
          }
          else if (
              rng and rng->k == N_AT
              and string_equal_ignore_case(rng->at.at, STRING_LITERAL("RANGE")))
          {
            Type_Info *at = rng->at.p ? type_canonical_concrete(rng->at.p->ty) : 0;
            if (at and at->k == TYPE_ARRAY)
            {
              if (at->lo == 0 and at->hi == -1 and rng->at.p)
              {
                V pv = generate_expression(g, rng->at.p);
                int blo, bhi;
                get_fat_pointer_bounds(g, pv.id, &blo, &bhi);
                fprintf(o, "  %%t%d = add i64 0, 0\n", ti);
                fprintf(
                    o,
                    "  store i64 %%t%d, ptr %%v.%s.sc%u.%u\n",
                    blo,
                    string_to_lowercase(fv->s),
                    vs->sc,
                    vs->el);
                fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", bhi, hi_var);
                ti = blo;
              }
              else
              {
                fprintf(o, "  %%t%d = add i64 0, %lld\n", ti, (long long) at->lo);
                int hi_t = new_temporary_register(g);
                fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_t, (long long) at->hi);
                fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi_t, hi_var);
              }
            }
            else
            {
              fprintf(o, "  %%t%d = add i64 0, %lld\n", ti, (long long) ft->lo);
              int hi_t = new_temporary_register(g);
              fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_t, (long long) ft->hi);
              fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi_t, hi_var);
            }
          }
          else
          {
            fprintf(o, "  %%t%d = add i64 0, %lld\n", ti, (long long) ft->lo);
            int hi_t = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", hi_t, (long long) ft->hi);
            fprintf(o, "  store i64 %%t%d, ptr %%v.__for_hi_%d\n", hi_t, hi_var);
          }
          if (vs->lv == 0)
          {
            char nb[256];
            if (vs->pr and vs->pr->nm.s)
            {
              int n = 0;
              for (uint32_t i = 0; i < vs->pr->nm.n; i++)
                nb[n++] = toupper(vs->pr->nm.s[i]);
              n += snprintf(nb + n, 256 - n, "_S%dE%d__", vs->pr->sc, vs->pr->el);
              for (uint32_t i = 0; i < vs->nm.n; i++)
                nb[n++] = toupper(vs->nm.s[i]);
              nb[n] = 0;
            }
            else
              snprintf(nb, 256, "%.*s", (int) vs->nm.n, vs->nm.s);
            fprintf(o, "  store i64 %%t%d, ptr @%s\n", ti, nb);
          }
          else
            fprintf(
                o,
                "  store i64 %%t%d, ptr %%v.%s.sc%u.%u\n",
                ti,
                string_to_lowercase(fv->s),
                vs->sc,
                vs->el);
        }
      }
    }
    emit_branch(g, lb);
    lbl(g, lb);
    if (n->lp.it)
    {
      if (fv and ft and hi_var >= 0)
      {
        Symbol *vs = fv->sy;
        int cv = new_temporary_register(g);
        if (vs->lv == 0)
        {
          char nb[256];
          if (vs->pr and vs->pr->nm.s)
          {
            int n = 0;
            for (uint32_t i = 0; i < vs->pr->nm.n; i++)
              nb[n++] = toupper(vs->pr->nm.s[i]);
            n += snprintf(nb + n, 256 - n, "_S%dE%d__", vs->pr->sc, vs->pr->el);
            for (uint32_t i = 0; i < vs->nm.n; i++)
              nb[n++] = toupper(vs->nm.s[i]);
            nb[n] = 0;
          }
          else
            snprintf(nb, 256, "%.*s", (int) vs->nm.n, vs->nm.s);
          fprintf(o, "  %%t%d = load i64, ptr @%s\n", cv, nb);
        }
        else
        {
          fprintf(
              o,
              "  %%t%d = load i64, ptr %%v.%s.sc%u.%u\n",
              cv,
              string_to_lowercase(fv->s),
              vs->sc,
              vs->el);
        }
        int hv = new_temporary_register(g);
        fprintf(o, "  %%t%d = load i64, ptr %%v.__for_hi_%d\n", hv, hi_var);
        int cmp = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", cmp, cv, hv);
        emit_conditional_branch(g, cmp, lc, le);
      }
      else
      {
        V c = value_to_boolean(g, generate_expression(g, n->lp.it));
        int ct = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
        emit_conditional_branch(g, ct, lc, le);
      }
    }
    else
      emit_branch(g, lc);
    lbl(g, lc);
    for (uint32_t i = 0; i < n->lp.st.n; i++)
      generate_statement_sequence(g, n->lp.st.d[i]);
    if (fv and ft)
    {
      Symbol *vs = fv->sy;
      if (vs)
      {
        int cv = new_temporary_register(g);
        if (vs->lv == 0)
        {
          char nb[256];
          if (vs->pr and vs->pr->nm.s)
          {
            int n = 0;
            for (uint32_t i = 0; i < vs->pr->nm.n; i++)
              nb[n++] = toupper(vs->pr->nm.s[i]);
            n += snprintf(nb + n, 256 - n, "_S%dE%d__", vs->pr->sc, vs->pr->el);
            for (uint32_t i = 0; i < vs->nm.n; i++)
              nb[n++] = toupper(vs->nm.s[i]);
            nb[n] = 0;
          }
          else
            snprintf(nb, 256, "%.*s", (int) vs->nm.n, vs->nm.s);
          fprintf(o, "  %%t%d = load i64, ptr @%s\n", cv, nb);
          int nv = new_temporary_register(g);
          fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", nv, cv);
          fprintf(o, "  store i64 %%t%d, ptr @%s\n", nv, nb);
        }
        else
        {
          fprintf(
              o,
              "  %%t%d = load i64, ptr %%v.%s.sc%u.%u\n",
              cv,
              string_to_lowercase(fv->s),
              vs->sc,
              vs->el);
          int nv = new_temporary_register(g);
          fprintf(o, "  %%t%d = add i64 %%t%d, 1\n", nv, cv);
          fprintf(
              o,
              "  store i64 %%t%d, ptr %%v.%s.sc%u.%u\n",
              nv,
              string_to_lowercase(fv->s),
              vs->sc,
              vs->el);
        }
      }
    }
    int lmd_id = 0;
    if (g->ls <= 64)
    {
      lmd_id = nm(g);
      g->lopt[lmd_id] = n->lp.rv ? 7 : 0;
    }
    if (lmd_id)
    {
      fprintf(o, "  br label %%Source_Location%d", lb);
      lmd(o, lmd_id);
      fprintf(o, "\n");
    }
    else
      emit_branch(g, lb);
    lbl(g, le);
    if (g->ls > 0)
      g->ls--;
  }
  break;
  case N_EX:
  {
    if (n->ex.lb.s)
    {
      int li = find_label(g, n->ex.lb);
      if (li >= 0)
      {
        int le = g->ll[li];
        if (n->ex.cd)
        {
          V c = value_to_boolean(g, generate_expression(g, n->ex.cd));
          int ct = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
          int lc = new_label_block(g);
          emit_conditional_branch(g, ct, le, lc);
          lbl(g, lc);
        }
        else
          emit_branch(g, le);
      }
      else
      {
        if (n->ex.cd)
        {
          V c = value_to_boolean(g, generate_expression(g, n->ex.cd));
          int ct = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
          int le = g->ls > 0 ? g->ll[g->ls - 1] : new_label_block(g), lc = new_label_block(g);
          emit_conditional_branch(g, ct, le, lc);
          lbl(g, lc);
        }
        else
        {
          int le = g->ls > 0 ? g->ll[g->ls - 1] : new_label_block(g);
          emit_branch(g, le);
        }
      }
    }
    else
    {
      if (n->ex.cd)
      {
        V c = value_to_boolean(g, generate_expression(g, n->ex.cd));
        int ct = new_temporary_register(g);
        fprintf(o, "  %%t%d = icmp ne i64 %%t%d, 0\n", ct, c.id);
        int le = g->ls > 0 ? g->ll[g->ls - 1] : new_label_block(g), lc = new_label_block(g);
        emit_conditional_branch(g, ct, le, lc);
        lbl(g, lc);
      }
      else
      {
        int le = g->ls > 0 ? g->ll[g->ls - 1] : new_label_block(g);
        emit_branch(g, le);
      }
    }
  }
  break;
  case N_GT:
  {
    int bb = get_or_create_label_basic_block(g, n->go.lb);
    fprintf(o, "  br label %%Source_Location%d\n", bb);
    int ul = new_label_block(g);
    lbl(g, ul);
    fprintf(o, "  unreachable\n");
  }
  break;
  case N_RT:
  {
    if (n->rt.vl)
    {
      V v = generate_expression(g, n->rt.vl);
      fprintf(o, "  ret %s %%t%d\n", value_llvm_type_string(v.k), v.id);
    }
    else
      fprintf(o, "  ret void\n");
  }
  break;
  case N_RS:
  {
    String_Slice ec =
        n->rs.ec and n->rs.ec->k == N_ID ? n->rs.ec->s : STRING_LITERAL("PROGRAM_ERROR");
    emit_exception(g, ec);
    int exh = new_temporary_register(g);
    fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", exh);
    fprintf(o, "  store ptr @.ex.%.*s, ptr @__ex_cur\n", (int) ec.n, ec.s);
    fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", exh);
    fprintf(o, "  unreachable\n");
  }
  break;
  case N_CLT:
  {
    if (n->ct.nm->k == N_ID)
    {
      Symbol *s = symbol_find_with_arity(g->sm, n->ct.nm->s, n->ct.arr.n, 0);
      if (s)
      {
        Syntax_Node *b = symbol_body(s, s->el);
        if (b)
        {
          int arid[64];
          Value_Kind ark[64];
          int arp[64];
          Syntax_Node *sp = symbol_spec(s);
          for (uint32_t i = 0; i < n->ct.arr.n and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->sp.pmm.n ? sp->sp.pmm.d[i] : 0;
            Syntax_Node *arg = n->ct.arr.d[i];
            V av = {0, VALUE_KIND_INTEGER};
            Value_Kind ek = VALUE_KIND_INTEGER;
            bool rf = false;
            if (pm)
            {
              if (pm->sy and pm->sy->ty)
                ek = token_kind_to_value_kind(pm->sy->ty);
              else if (pm->pm.ty)
              {
                Type_Info *pt = resolve_subtype(g->sm, pm->pm.ty);
                ek = token_kind_to_value_kind(pt);
              }
              if (pm->pm.md & 2 and arg->k == N_ID)
              {
                rf = true;
                Symbol *as = arg->sy ? arg->sy : symbol_find(g->sm, arg->s);
                if (as and as->lv == 0)
                {
                  char nb[256];
                  if (as->pr and as->pr->nm.s)
                  {
                    int n = 0;
                    for (uint32_t j = 0; j < as->pr->nm.n; j++)
                      nb[n++] = toupper(as->pr->nm.s[j]);
                    n += snprintf(nb + n, 256 - n, "_S%dE%d__", as->pr->sc, as->pr->el);
                    for (uint32_t j = 0; j < as->nm.n; j++)
                      nb[n++] = toupper(as->nm.s[j]);
                    nb[n] = 0;
                  }
                  else
                    snprintf(nb, 256, "%.*s", (int) as->nm.n, as->nm.s);
                  av.id = new_temporary_register(g);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = bitcast ptr @%s to ptr\n", av.id, nb);
                }
                else if (as and as->lv >= 0 and as->lv < g->sm->lv)
                {
                  av.id = new_temporary_register(g);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__slnk, i64 %u\n", av.id, as->el);
                  int a2 = new_temporary_register(g);
                  fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", a2, av.id);
                  av.id = a2;
                }
                else
                {
                  av.id = new_temporary_register(g);
                  av.k = VALUE_KIND_POINTER;
                  fprintf(
                      o,
                      "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                      av.id,
                      string_to_lowercase(arg->s),
                      as ? as->sc : 0,
                      as ? as->el : 0);
                }
                ek = VALUE_KIND_POINTER;
              }
              else if (pm->pm.md & 2)
              {
                rf = true;
                av = generate_expression(g, arg);
                int ap = new_temporary_register(g);
                fprintf(o, "  %%t%d = alloca %s\n", ap, value_llvm_type_string(av.k));
                fprintf(
                    o, "  store %s %%t%d, ptr %%t%d\n", value_llvm_type_string(av.k), av.id, ap);
                av.id = ap;
                av.k = VALUE_KIND_POINTER;
                ek = VALUE_KIND_POINTER;
              }
              else
              {
                av = generate_expression(g, arg);
              }
            }
            else
            {
              av = generate_expression(g, arg);
            }
            if (not rf and ek != VALUE_KIND_INTEGER)
            {
              V cv = value_cast(g, av, ek);
              av = cv;
            }
            arid[i] = av.id;
            ark[i] = ek != VALUE_KIND_INTEGER ? ek : av.k;
            arp[i] = rf ? 1 : 0;
          }
          char nb[256];
          if (s->ext)
            snprintf(nb, 256, "%.*s", (int) s->ext_nm.n, s->ext_nm.s);
          else
            encode_symbol_name(nb, 256, s, n->ct.nm->s, n->ct.arr.n, sp);
          fprintf(o, "  call void @\"%s\"(", nb);
          for (uint32_t i = 0; i < n->ct.arr.n; i++)
          {
            if (i)
              fprintf(o, ", ");
            fprintf(o, "%s %%t%d", value_llvm_type_string(ark[i]), arid[i]);
          }
          if (s->lv > 0 and not s->ext)
          {
            if (n->ct.arr.n > 0)
              fprintf(o, ", ");
            if (s->lv >= g->sm->lv)
              fprintf(o, "ptr %%__frame");
            else
              fprintf(o, "ptr %%__slnk");
          }
          fprintf(o, ")\n");
          for (uint32_t i = 0; i < n->ct.arr.n and i < 64; i++)
          {
            if (arp[i])
            {
              int lv = new_temporary_register(g);
              fprintf(
                  o,
                  "  %%t%d = load %s, ptr %%t%d\n",
                  lv,
                  value_llvm_type_string(
                      ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]),
                  arid[i]);
              V rv = {lv, ark[i] == VALUE_KIND_POINTER ? VALUE_KIND_INTEGER : ark[i]};
              V cv = value_cast(g, rv, token_kind_to_value_kind(n->ct.arr.d[i]->ty));
              Syntax_Node *tg = n->ct.arr.d[i];
              if (tg->k == N_ID)
              {
                Symbol *ts = tg->sy ? tg->sy : symbol_find(g->sm, tg->s);
                if (ts)
                {
                  if (ts->lv >= 0 and ts->lv < g->sm->lv)
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        ts->lv,
                        string_to_lowercase(tg->s));
                  else
                    fprintf(
                        o,
                        "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                        value_llvm_type_string(cv.k),
                        cv.id,
                        string_to_lowercase(tg->s),
                        ts->sc,
                        ts->el);
                }
              }
            }
          }
        }
        else if (s->ext)
        {
          char nb[256];
          snprintf(nb, 256, "%.*s", (int) s->ext_nm.n, s->ext_nm.s);
          fprintf(o, "  call void @\"%s\"(", nb);
          for (uint32_t i = 0; i < n->ct.arr.n; i++)
          {
            if (i)
              fprintf(o, ", ");
            V av = generate_expression(g, n->ct.arr.d[i]);
            fprintf(o, "i64 %%t%d", av.id);
          }
          fprintf(o, ")\n");
        }
        else if (s->k == 4 or s->k == 5)
        {
          Syntax_Node *sp = symbol_spec(s);
          if (not sp and s->ty and s->ty->ops.n > 0)
          {
            sp = s->ty->ops.d[0]->bd.sp;
          }
          int arid[64];
          Value_Kind ark[64];
          int arpt[64];
          for (uint32_t i = 0; i < n->ct.arr.n and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->sp.pmm.n ? sp->sp.pmm.d[i] : 0;
            arpt[i] = pm and (pm->pm.md & 2) ? 1 : 0;
          }
          for (uint32_t i = 0; i < n->ct.arr.n and i < 64; i++)
          {
            Syntax_Node *pm = sp and i < sp->sp.pmm.n ? sp->sp.pmm.d[i] : 0;
            Syntax_Node *arg = n->ct.arr.d[i];
            V av = {0, VALUE_KIND_INTEGER};
            Value_Kind ek = VALUE_KIND_INTEGER;
            if (pm)
            {
              if (pm->sy and pm->sy->ty)
                ek = token_kind_to_value_kind(pm->sy->ty);
              else if (pm->pm.ty)
              {
                Type_Info *pt = resolve_subtype(g->sm, pm->pm.ty);
                ek = token_kind_to_value_kind(pt);
              }
            }
            if (arpt[i] and arg->k == N_ID)
            {
              ek = VALUE_KIND_POINTER;
              Symbol *as = arg->sy ? arg->sy : symbol_find(g->sm, arg->s);
              av.id = new_temporary_register(g);
              av.k = VALUE_KIND_POINTER;
              if (as and as->lv >= 0 and as->lv < g->sm->lv)
                fprintf(
                    o,
                    "  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\n",
                    av.id,
                    as->lv,
                    string_to_lowercase(arg->s));
              else
                fprintf(
                    o,
                    "  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\n",
                    av.id,
                    string_to_lowercase(arg->s),
                    as ? as->sc : 0,
                    as ? as->el : 0);
            }
            else
            {
              av = generate_expression(g, arg);
              V cv = value_cast(g, av, ek);
              av = cv;
            }
            arid[i] = av.id;
            ark[i] = ek;
          }
          char nb[256];
          if (s->ext)
            snprintf(nb, 256, "%.*s", (int) s->ext_nm.n, s->ext_nm.s);
          else
            encode_symbol_name(nb, 256, s, n->ct.nm->s, n->ct.arr.n, sp);
          if (s->k == 5 and sp and sp->sp.rt)
          {
            Type_Info *rt = resolve_subtype(g->sm, sp->sp.rt);
            Value_Kind rk = token_kind_to_value_kind(rt);
            int rid = new_temporary_register(g);
            fprintf(o, "  %%t%d = call %s @\"%s\"(", rid, value_llvm_type_string(rk), nb);
          }
          else
            fprintf(o, "  call void @\"%s\"(", nb);
          for (uint32_t i = 0; i < n->ct.arr.n; i++)
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
    if (n->bk.lb.s)
    {
      lblbb = get_or_create_label_basic_block(g, n->bk.lb);
      emit_branch(g, lblbb);
      lbl(g, lblbb);
    }
    int sj = new_temporary_register(g);
    int prev_eh = new_temporary_register(g);
    fprintf(o, "  %%t%d = load ptr, ptr @__eh_cur\n", prev_eh);
    fprintf(o, "  %%t%d = call ptr @__ada_setjmp()\n", sj);
    fprintf(o, "  store ptr %%t%d, ptr %%ej\n", sj);
    int sjb = new_temporary_register(g);
    fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", sjb);
    int sv = new_temporary_register(g);
    fprintf(o, "  %%t%d = call i32 @setjmp(ptr %%t%d)\n", sv, sjb);
    fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", sjb);
    int ze = new_temporary_register(g);
    fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", ze, sv);
    int ln = new_label_block(g), lh = new_label_block(g);
    emit_conditional_branch(g, ze, ln, lh);
    lbl(g, ln);
    for (uint32_t i = 0; i < n->bk.dc.n; i++)
    {
      Syntax_Node *d = n->bk.dc.d[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PKB and d->k != N_PD and d->k != N_FD)
        generate_declaration(g, d);
    }
    for (uint32_t i = 0; i < n->bk.dc.n; i++)
    {
      Syntax_Node *d = n->bk.dc.d[i];
      if (d and d->k == N_OD and d->od.in)
      {
        for (uint32_t j = 0; j < d->od.id.n; j++)
        {
          Syntax_Node *id = d->od.id.d[j];
          if (id->sy)
          {
            Value_Kind k = d->od.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, d->od.ty))
                                    : VALUE_KIND_INTEGER;
            Symbol *s = id->sy;
            Type_Info *at = d->od.ty ? resolve_subtype(g->sm, d->od.ty) : 0;
            if (at and at->k == TYPE_RECORD and at->dc.n > 0 and d->od.in and d->od.in->ty)
            {
              Type_Info *it = type_canonical_concrete(d->od.in->ty);
              if (it and it->k == TYPE_RECORD and it->dc.n > 0)
              {
                for (uint32_t di = 0; di < at->dc.n and di < it->dc.n; di++)
                {
                  Syntax_Node *td = at->dc.d[di];
                  Syntax_Node *id_d = it->dc.d[di];
                  if (td->k == N_DS and id_d->k == N_DS and td->pm.df and td->pm.df->k == N_INT)
                  {
                    int tdi = new_temporary_register(g);
                    fprintf(o, "  %%t%d = add i64 0, %lld\n", tdi, (long long) td->pm.df->i);
                    V iv = generate_expression(g, d->od.in);
                    int ivd = new_temporary_register(g);
                    fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ivd, iv.id, di);
                    int dvl = new_temporary_register(g);
                    fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", dvl, ivd);
                    int cmp = new_temporary_register(g);
                    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", cmp, dvl, tdi);
                    int lok = new_label_block(g), lf = new_label_block(g);
                    emit_conditional_branch(g, cmp, lok, lf);
                    lbl(g, lok);
                    fprintf(
                        o,
                        "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n "
                        " unreachable\n");
                    lbl(g, lf);
                  }
                }
              }
            }
            {
              V v = generate_expression(g, d->od.in);
              v = value_cast(g, v, k);
              if (s and s->lv >= 0 and s->lv < g->sm->lv)
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                    value_llvm_type_string(k),
                    v.id,
                    s->lv,
                    string_to_lowercase(id->s));
              else
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                    value_llvm_type_string(k),
                    v.id,
                    string_to_lowercase(id->s),
                    s ? s->sc : 0,
                    s ? s->el : 0);
            }
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->bk.dc.n; i++)
    {
      Syntax_Node *d = n->bk.dc.d[i];
      if (d and d->k == N_OD and not d->od.in)
        for (uint32_t j = 0; j < d->od.id.n; j++)
        {
          Syntax_Node *id = d->od.id.d[j];
          Symbol *s = id->sy;
          if (not s)
            continue;
          Type_Info *at = d->od.ty ? resolve_subtype(g->sm, d->od.ty) : 0;
          if (not at or at->k != TYPE_RECORD or not at->dc.n)
            continue;
          uint32_t di;
          for (di = 0; di < at->dc.n; di++)
            if (at->dc.d[di]->k != N_DS or not at->dc.d[di]->pm.df)
              goto nx;
          for (uint32_t ci = 0; ci < at->cm.n; ci++)
          {
            Syntax_Node *cm = at->cm.d[ci];
            if (cm->k != N_CM or not cm->cm.ty)
              continue;
            Type_Info *cty = cm->cm.ty->ty;
            if (not cty or cty->k != TYPE_ARRAY or not cty->ix)
              continue;
            for (di = 0; di < at->dc.n; di++)
            {
              Syntax_Node *dc = at->dc.d[di];
              if (dc->k == N_DS and dc->pm.df and dc->pm.df->k == N_INT)
              {
                int64_t dv = dc->pm.df->i;
                if (dv < cty->ix->lo or dv > cty->ix->hi)
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
          for (di = 0; di < at->dc.n; di++)
          {
            Syntax_Node *dc = at->dc.d[di];
            int dv = new_temporary_register(g);
            fprintf(o, "  %%t%d = add i64 0, %lld\n", dv, (long long) dc->pm.df->i);
            int dp = new_temporary_register(g);
            if (s->lv >= 0 and s->lv < g->sm->lv)
              fprintf(
                  o,
                  "  %%t%d = getelementptr i64, ptr %%lnk.%d.%s, i64 %u\n",
                  dp,
                  s->lv,
                  string_to_lowercase(id->s),
                  di);
            else
              fprintf(
                  o,
                  "  %%t%d = getelementptr i64, ptr %%v.%s.sc%u.%u, i64 %u\n",
                  dp,
                  string_to_lowercase(id->s),
                  s->sc,
                  s->el,
                  di);
            fprintf(o, "  store i64 %%t%d, ptr %%t%d\n", dv, dp);
          }
        nx:;
        }
    }
    for (uint32_t i = 0; i < n->bk.st.n; i++)
      generate_statement_sequence(g, n->bk.st.d[i]);
    int ld = new_label_block(g);
    emit_branch(g, ld);
    lbl(g, lh);
    if (n->bk.hd.n > 0)
    {
      for (uint32_t i = 0; i < n->bk.hd.n; i++)
      {
        Syntax_Node *h = n->bk.hd.d[i];
        int lhm = new_label_block(g), lhn = new_label_block(g);
        for (uint32_t j = 0; j < h->hnd.ec.n; j++)
        {
          Syntax_Node *e = h->hnd.ec.d[j];
          if (e->k == N_ID and string_equal_ignore_case(e->s, STRING_LITERAL("others")))
          {
            emit_branch(g, lhm);
            break;
          }
          int ec = new_temporary_register(g);
          fprintf(o, "  %%t%d = load ptr, ptr @__ex_cur\n", ec);
          int cm = new_temporary_register(g);
          char eb[256];
          for (uint32_t ei = 0; ei < e->s.n and ei < 255; ei++)
            eb[ei] = toupper(e->s.s[ei]);
          eb[e->s.n < 255 ? e->s.n : 255] = 0;
          fprintf(o, "  %%t%d = call i32 @strcmp(ptr %%t%d, ptr @.ex.%s)\n", cm, ec, eb);
          int eq = new_temporary_register(g);
          fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", eq, cm);
          emit_conditional_branch(g, eq, lhm, lhn);
          lbl(g, lhn);
        }
        emit_branch(g, ld);
        lbl(g, lhm);
        for (uint32_t j = 0; j < h->hnd.stz.n; j++)
          generate_statement_sequence(g, h->hnd.stz.d[j]);
        emit_branch(g, ld);
      }
    }
    else
    {
      int nc = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp eq ptr %%t%d, null\n", nc, prev_eh);
      int ex = new_label_block(g), lj = new_label_block(g);
      emit_conditional_branch(g, nc, ex, lj);
      lbl(g, ex);
      emit_branch(g, ld);
      lbl(g, lj);
      fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", prev_eh);
      fprintf(o, "  unreachable\n");
    }
    lbl(g, ld);
    fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", prev_eh);
    ;
  }
  break;
  case N_DL:
  {
    V d = generate_expression(g, n->ex.cd);
    d = value_cast(g, d, VALUE_KIND_INTEGER);
    fprintf(o, "  call void @__ada_delay(i64 %%t%d)\n", d.id);
  }
  break;
  case N_SA:
  {
    if (n->sa.kn == 1 or n->sa.kn == 3)
    {
      V gd = generate_expression(g, n->sa.gd);
      int ld = new_label_block(g);
      fprintf(o, "  call void @__ada_delay(i64 %%t%d)\n", gd.id);
      if (n->sa.kn == 3)
        fprintf(o, "  call void @__ada_raise(ptr @.ex.TASKING_ERROR)\n  unreachable\n");
      for (uint32_t i = 0; i < n->sa.sts.n; i++)
        generate_statement_sequence(g, n->sa.sts.d[i]);
      emit_branch(g, ld);
      lbl(g, ld);
    }
    else
    {
      int ld = new_label_block(g);
      for (uint32_t i = 0; i < n->sa.sts.n; i++)
      {
        Syntax_Node *st = n->sa.sts.d[i];
        if (st->k == N_ACC)
        {
          for (uint32_t j = 0; j < st->acc.stx.n; j++)
            generate_statement_sequence(g, st->acc.stx.d[j]);
        }
        else if (st->k == N_DL)
        {
          V d = generate_expression(g, st->ex.cd);
          fprintf(o, "  call void @__ada_delay(i64 %%t%d)\n", d.id);
          for (uint32_t j = 0; j < st->hnd.stz.n; j++)
            generate_statement_sequence(g, st->hnd.stz.d[j]);
        }
      }
      if (n->ss.el.n > 0)
        for (uint32_t i = 0; i < n->ss.el.n; i++)
          generate_statement_sequence(g, n->ss.el.d[i]);
      emit_branch(g, ld);
      lbl(g, ld);
    }
  }
  break;
  default:
    break;
  }
  }
}
static bool isrt(const char *n)
{
  return not strcmp(n, "__text_io_new_line") or not strcmp(n, "__text_io_put_char")
         or not strcmp(n, "__text_io_put") or not strcmp(n, "__text_io_put_line")
         or not strcmp(n, "__text_io_get_char") or not strcmp(n, "__text_io_get_line")
         or not strcmp(n, "__ada_ss_init") or not strcmp(n, "__ada_ss_mark")
         or not strcmp(n, "__ada_ss_release") or not strcmp(n, "__ada_ss_allocate")
         or not strcmp(n, "__ada_setjmp") or not strcmp(n, "__ada_raise")
         or not strcmp(n, "__ada_delay") or not strcmp(n, "__ada_powi")
         or not strcmp(n, "__ada_finalize") or not strcmp(n, "__ada_finalize_all")
         or not strcmp(n, "__ada_image_enum") or not strcmp(n, "__ada_value_int")
         or not strcmp(n, "__ada_image_int");
}
static bool hlb(Node_Vector *sl)
{
  for (uint32_t i = 0; i < sl->n; i++)
  {
    Syntax_Node *s = sl->d[i];
    if (not s)
      continue;
    if (s->k == N_BL and s->bk.lb.s)
      return 1;
    if (s->k == N_GT)
      return 1;
    if (s->k == N_BL and hlb(&s->bk.st))
      return 1;
    if (s->k == N_IF and (hlb(&s->if_.th) or hlb(&s->if_.el)))
      return 1;
    for (uint32_t j = 0; s->k == N_IF and j < s->if_.ei.n; j++)
      if (s->if_.ei.d[j] and hlb(&s->if_.ei.d[j]->if_.th))
        return 1;
    if (s->k == N_CS)
    {
      for (uint32_t j = 0; j < s->cs.al.n; j++)
        if (s->cs.al.d[j] and hlb(&s->cs.al.d[j]->hnd.stz))
          return 1;
    }
    if (s->k == N_LP and hlb(&s->lp.st))
      return 1;
  }
  return 0;
}
static void elb_r(Code_Generator *g, Syntax_Node *s, Node_Vector *lbs);
static void elb(Code_Generator *g, Node_Vector *sl)
{
  Node_Vector lbs = {0};
  for (uint32_t i = 0; i < sl->n; i++)
    if (sl->d[i])
      elb_r(g, sl->d[i], &lbs);
  FILE *o = g->o;
  for (uint32_t i = 0; i < lbs.n; i++)
  {
    String_Slice *lb = (String_Slice *) lbs.d[i];
    fprintf(o, "lbl.%.*s:\n", (int) lb->n, lb->s);
  }
}
static void elb_r(Code_Generator *g, Syntax_Node *s, Node_Vector *lbs)
{
  if (not s)
    return;
  if (s->k == N_GT)
  {
    bool found = 0;
    for (uint32_t i = 0; i < lbs->n; i++)
    {
      String_Slice *lb = (String_Slice *) lbs->d[i];
      if (string_equal_ignore_case(*lb, s->go.lb))
      {
        found = 1;
        break;
      }
    }
    if (not found)
    {
      String_Slice *lb = malloc(sizeof(String_Slice));
      *lb = s->go.lb;
      nv(lbs, (Syntax_Node *) lb);
    }
  }
  if (s->k == N_IF)
  {
    for (uint32_t i = 0; i < s->if_.th.n; i++)
      elb_r(g, s->if_.th.d[i], lbs);
    for (uint32_t i = 0; i < s->if_.el.n; i++)
      elb_r(g, s->if_.el.d[i], lbs);
    for (uint32_t i = 0; i < s->if_.ei.n; i++)
      if (s->if_.ei.d[i])
        for (uint32_t j = 0; j < s->if_.ei.d[i]->if_.th.n; j++)
          elb_r(g, s->if_.ei.d[i]->if_.th.d[j], lbs);
  }
  if (s->k == N_BL)
  {
    if (s->bk.lb.s)
    {
      bool found = 0;
      for (uint32_t i = 0; i < lbs->n; i++)
      {
        String_Slice *lb = (String_Slice *) lbs->d[i];
        if (string_equal_ignore_case(*lb, s->bk.lb))
        {
          found = 1;
          break;
        }
      }
      if (not found)
      {
        String_Slice *lb = malloc(sizeof(String_Slice));
        *lb = s->bk.lb;
        nv(lbs, (Syntax_Node *) lb);
      }
    }
    for (uint32_t i = 0; i < s->bk.st.n; i++)
      elb_r(g, s->bk.st.d[i], lbs);
  }
  if (s->k == N_LP)
  {
    if (s->lp.lb.s)
    {
      bool found = 0;
      for (uint32_t i = 0; i < lbs->n; i++)
      {
        String_Slice *lb = (String_Slice *) lbs->d[i];
        if (string_equal_ignore_case(*lb, s->lp.lb))
        {
          found = 1;
          break;
        }
      }
      if (not found)
      {
        String_Slice *lb = malloc(sizeof(String_Slice));
        *lb = s->lp.lb;
        nv(lbs, (Syntax_Node *) lb);
      }
    }
    for (uint32_t i = 0; i < s->lp.st.n; i++)
      elb_r(g, s->lp.st.d[i], lbs);
  }
  if (s->k == N_CS)
    for (uint32_t i = 0; i < s->cs.al.n; i++)
      if (s->cs.al.d[i])
        for (uint32_t j = 0; j < s->cs.al.d[i]->hnd.stz.n; j++)
          elb_r(g, s->cs.al.d[i]->hnd.stz.d[j], lbs);
}
static void generate_declaration(Code_Generator *g, Syntax_Node *n);
static void hbl(Code_Generator *g, Node_Vector *sl)
{
  for (uint32_t i = 0; i < sl->n; i++)
  {
    Syntax_Node *s = sl->d[i];
    if (not s)
      continue;
    if (s->k == N_BL)
    {
      for (uint32_t j = 0; j < s->bk.dc.n; j++)
      {
        Syntax_Node *d = s->bk.dc.d[j];
        if (d and (d->k == N_PB or d->k == N_FB))
          generate_declaration(g, d);
      }
      hbl(g, &s->bk.st);
    }
    else if (s->k == N_IF)
    {
      hbl(g, &s->if_.th);
      hbl(g, &s->if_.el);
      for (uint32_t j = 0; j < s->if_.ei.n; j++)
        if (s->if_.ei.d[j])
          hbl(g, &s->if_.ei.d[j]->if_.th);
    }
    else if (s->k == N_CS)
    {
      for (uint32_t j = 0; j < s->cs.al.n; j++)
        if (s->cs.al.d[j])
          hbl(g, &s->cs.al.d[j]->hnd.stz);
    }
    else if (s->k == N_LP)
      hbl(g, &s->lp.st);
  }
}
static void generate_declaration(Code_Generator *g, Syntax_Node *n)
{
  FILE *o = g->o;
  if (not n)
    return;
  switch (n->k)
  {
  case N_OD:
  {
    for (uint32_t j = 0; j < n->od.id.n; j++)
    {
      Syntax_Node *id = n->od.id.d[j];
      Symbol *s = id->sy;
      if (s)
      {
        if (s->k == 0 or s->k == 2)
        {
          Value_Kind k =
              s->ty ? token_kind_to_value_kind(s->ty)
                    : (n->od.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, n->od.ty))
                                : VALUE_KIND_INTEGER);
          Type_Info *at = s->ty ? type_canonical_concrete(s->ty)
                                : (n->od.ty ? resolve_subtype(g->sm, n->od.ty) : 0);
          if (s->k == 0 or s->k == 2)
          {
            Type_Info *bt = at;
            while (bt and bt->k == TYPE_ARRAY and bt->el)
              bt = type_canonical_concrete(bt->el);
            int asz = -1;
            if (n->od.in and n->od.in->k == N_AG and at and at->k == TYPE_ARRAY)
              asz = (int) n->od.in->ag.it.n;
            if (at and at->k == TYPE_ARRAY and at->lo == 0 and at->hi == -1 and asz < 0)
            {
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca {ptr,ptr}\n",
                  string_to_lowercase(id->s),
                  s->sc,
                  s->el);
            }
            else if (at and at->k == TYPE_ARRAY and asz > 0)
            {
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca [%d x %s]\n",
                  string_to_lowercase(id->s),
                  s->sc,
                  s->el,
                  asz,
                  ada_to_c_type_string(bt));
            }
            else if (at and at->k == TYPE_ARRAY and at->hi >= at->lo)
            {
              asz = (int) (at->hi - at->lo + 1);
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca [%d x %s]\n",
                  string_to_lowercase(id->s),
                  s->sc,
                  s->el,
                  asz,
                  ada_to_c_type_string(bt));
            }
            else
            {
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca %s\n",
                  string_to_lowercase(id->s),
                  s->sc,
                  s->el,
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
    Syntax_Node *sp = n->bd.sp;
    char nb[256];
    if (n->sy and n->sy->ext)
      snprintf(nb, 256, "%.*s", (int) n->sy->ext_nm.n, n->sy->ext_nm.s);
    else
    {
      encode_symbol_name(nb, 256, n->sy, sp->sp.nm, sp->sp.pmm.n, sp);
    }
    if (not add_declaration(g, nb))
      break;
    if (isrt(nb))
      break;
    bool has_body = false;
    if (n->sy)
      for (uint32_t i = 0; i < n->sy->ol.n; i++)
        if (n->sy->ol.d[i]->k == N_PB)
        {
          has_body = true;
          break;
        }
    if (has_body)
      break;
    fprintf(o, "declare void @\"%s\"(", nb);
    for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
    {
      if (i)
        fprintf(o, ",");
      Syntax_Node *p = sp->sp.pmm.d[i];
      Type_Info *pt = p->pm.ty ? resolve_subtype(g->sm, p->pm.ty) : 0;
      Value_Kind k = VALUE_KIND_INTEGER;
      if (pt)
      {
        Type_Info *ptc = type_canonical_concrete(pt);
        if (n->sy and n->sy->ext and ptc and ptc->k == TYPE_ARRAY and not(p->pm.md & 2))
          k = VALUE_KIND_INTEGER;
        else
          k = token_kind_to_value_kind(pt);
      }
      if (p->pm.md & 2)
        fprintf(o, "ptr");
      else
        fprintf(o, "%s", value_llvm_type_string(k));
    }
    fprintf(o, ")\n");
  }
  break;
  case N_FD:
  {
    Syntax_Node *sp = n->bd.sp;
    char nb[256];
    if (n->sy and n->sy->ext)
      snprintf(nb, 256, "%.*s", (int) n->sy->ext_nm.n, n->sy->ext_nm.s);
    else
    {
      encode_symbol_name(nb, 256, n->sy, sp->sp.nm, sp->sp.pmm.n, sp);
    }
    if (not add_declaration(g, nb))
      break;
    if (isrt(nb))
      break;
    bool has_body = false;
    if (n->sy)
      for (uint32_t i = 0; i < n->sy->ol.n; i++)
        if (n->sy->ol.d[i]->k == N_FB)
        {
          has_body = true;
          break;
        }
    if (has_body)
      break;
    Value_Kind rk = sp->sp.rt ? token_kind_to_value_kind(resolve_subtype(g->sm, sp->sp.rt))
                              : VALUE_KIND_INTEGER;
    fprintf(o, "declare %s @\"%s\"(", value_llvm_type_string(rk), nb);
    for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
    {
      if (i)
        fprintf(o, ",");
      Syntax_Node *p = sp->sp.pmm.d[i];
      Type_Info *pt = p->pm.ty ? resolve_subtype(g->sm, p->pm.ty) : 0;
      Value_Kind k = VALUE_KIND_INTEGER;
      if (pt)
      {
        Type_Info *ptc = type_canonical_concrete(pt);
        if (n->sy and n->sy->ext and ptc and ptc->k == TYPE_ARRAY and not(p->pm.md & 2))
          k = VALUE_KIND_INTEGER;
        else
          k = token_kind_to_value_kind(pt);
      }
      if (p->pm.md & 2)
        fprintf(o, "ptr");
      else
        fprintf(o, "%s", value_llvm_type_string(k));
    }
    fprintf(o, ")\n");
  }
  break;
  case N_BL:
    for (uint32_t i = 0; i < n->bk.dc.n; i++)
    {
      Syntax_Node *d = n->bk.dc.d[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PD and d->k != N_FD)
        generate_declaration(g, d);
    }
    for (uint32_t i = 0; i < n->bk.dc.n; i++)
    {
      Syntax_Node *d = n->bk.dc.d[i];
      if (d and (d->k == N_PB or d->k == N_FB))
        generate_declaration(g, d);
    }
    break;
  case N_PB:
  {
    Syntax_Node *sp = n->bd.sp;
    GT *gt = gfnd(g->sm, sp->sp.nm);
    if (gt)
      break;
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and d->k == N_PKB)
        generate_declaration(g, d);
    }
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and (d->k == N_PB or d->k == N_FB))
        generate_declaration(g, d);
    }
    hbl(g, &n->bd.st);
    char nb[256];
    if (n->sy and n->sy->mangled_nm.s)
    {
      snprintf(nb, 256, "%.*s", (int) n->sy->mangled_nm.n, n->sy->mangled_nm.s);
    }
    else
    {
      encode_symbol_name(nb, 256, n->sy, sp->sp.nm, sp->sp.pmm.n, sp);
      if (n->sy)
      {
        n->sy->mangled_nm.s = arena_allocate(strlen(nb) + 1);
        memcpy((char *) n->sy->mangled_nm.s, nb, strlen(nb) + 1);
        n->sy->mangled_nm.n = strlen(nb);
      }
    }
    fprintf(o, "define linkonce_odr void @\"%s\"(", nb);
    int np = sp->sp.pmm.n;
    if (n->sy and n->sy->lv > 0)
      np++;
    for (int i = 0; i < np; i++)
    {
      if (i)
        fprintf(o, ", ");
      if (i < (int) sp->sp.pmm.n)
      {
        Syntax_Node *p = sp->sp.pmm.d[i];
        Value_Kind k = p->pm.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, p->pm.ty))
                                : VALUE_KIND_INTEGER;
        if (p->pm.md & 2)
          fprintf(o, "ptr %%p.%s", string_to_lowercase(p->pm.nm));
        else
          fprintf(o, "%s %%p.%s", value_llvm_type_string(k), string_to_lowercase(p->pm.nm));
      }
      else
        fprintf(o, "ptr %%__slnk");
    }
    fprintf(o, ")%s{\n", n->sy and n->sy->inl ? " alwaysinline " : " ");
    fprintf(o, "  %%ej = alloca ptr\n");
    int sv = g->sm->lv;
    g->sm->lv = n->sy ? n->sy->lv + 1 : 0;
    if (n->sy and n->sy->lv > 0)
    {
      generate_block_frame(g);
      int mx = 0;
      for (int h = 0; h < 4096; h++)
        for (Symbol *s = g->sm->sy[h]; s; s = s->nx)
          if (s->k == 0 and s->el >= 0 and s->el > mx)
            mx = s->el;
      if (mx == 0)
        fprintf(o, "  %%__frame = bitcast ptr %%__slnk to ptr\n");
    }
    if (n->sy and n->sy->lv > 0)
    {
      int fp0 = new_temporary_register(g);
      fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__frame, i64 0\n", fp0);
      fprintf(o, "  store ptr %%__slnk, ptr %%t%d\n", fp0);
    }
    for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
    {
      Syntax_Node *p = sp->sp.pmm.d[i];
      Value_Kind k = p->pm.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, p->pm.ty))
                              : VALUE_KIND_INTEGER;
      Symbol *ps = p->sy;
      if (ps and ps->lv >= 0 and ps->lv < g->sm->lv)
        fprintf(
            o,
            "  %%lnk.%d.%s = alloca %s\n",
            ps->lv,
            string_to_lowercase(p->pm.nm),
            value_llvm_type_string(k));
      else
        fprintf(
            o,
            "  %%v.%s.sc%u.%u = alloca %s\n",
            string_to_lowercase(p->pm.nm),
            ps ? ps->sc : 0,
            ps ? ps->el : 0,
            value_llvm_type_string(k));
      if (p->pm.md & 2)
      {
        int lv = new_temporary_register(g);
        fprintf(
            o,
            "  %%t%d = load %s, ptr %%p.%s\n",
            lv,
            value_llvm_type_string(k),
            string_to_lowercase(p->pm.nm));
        if (ps and ps->lv >= 0 and ps->lv < g->sm->lv)
          fprintf(
              o,
              "  store %s %%t%d, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              lv,
              ps->lv,
              string_to_lowercase(p->pm.nm));
        else
          fprintf(
              o,
              "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              lv,
              string_to_lowercase(p->pm.nm),
              ps ? ps->sc : 0,
              ps ? ps->el : 0);
      }
      else
      {
        if (ps and ps->lv >= 0 and ps->lv < g->sm->lv)
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->pm.nm),
              ps->lv,
              string_to_lowercase(p->pm.nm));
        else
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->pm.nm),
              string_to_lowercase(p->pm.nm),
              ps ? ps->sc : 0,
              ps ? ps->el : 0);
      }
    }
    if (n->sy and n->sy->lv > 0)
    {
      for (int h = 0; h < 4096; h++)
      {
        for (Symbol *s = g->sm->sy[h]; s; s = s->nx)
        {
          if (s->k == 0 and s->lv >= 0 and s->lv < g->sm->lv and not(s->df and s->df->k == N_GVL))
          {
            Value_Kind k = s->ty ? token_kind_to_value_kind(s->ty) : VALUE_KIND_INTEGER;
            Type_Info *at = s->ty ? type_canonical_concrete(s->ty) : 0;
            if (at and at->k == TYPE_ARRAY and at->hi >= at->lo)
            {
              int asz = (int) (at->hi - at->lo + 1);
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca [%d x %s]\n",
                  string_to_lowercase(s->nm),
                  s->sc,
                  s->el,
                  asz,
                  ada_to_c_type_string(at->el));
            }
            else
            {
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca %s\n",
                  string_to_lowercase(s->nm),
                  s->sc,
                  s->el,
                  value_llvm_type_string(k));
            }
            int level_diff = g->sm->lv - s->lv - 1;
            int slnk_ptr;
            if (level_diff == 0)
            {
              slnk_ptr = new_temporary_register(g);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
            }
            else
            {
              slnk_ptr = new_temporary_register(g);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
              for (int hop = 0; hop < level_diff; hop++)
              {
                int next_slnk = new_temporary_register(g);
                fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
                int loaded_slnk = new_temporary_register(g);
                fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
                slnk_ptr = loaded_slnk;
              }
            }
            int p = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->el);
            int ptr_id = new_temporary_register(g);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", ptr_id, p);
            int v = new_temporary_register(g);
            fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", v, value_llvm_type_string(k), ptr_id);
            fprintf(
                o,
                "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                value_llvm_type_string(k),
                v,
                string_to_lowercase(s->nm),
                s->sc,
                s->el);
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PKB and d->k != N_PD and d->k != N_FD)
        generate_declaration(g, d);
    }
    if (n->sy and n->sy->lv > 0)
    {
      for (int h = 0; h < 4096; h++)
      {
        for (Symbol *s = g->sm->sy[h]; s; s = s->nx)
        {
          if (s->k == 0 and s->el >= 0 and n->sy->sc >= 0 and s->sc == (uint32_t) (n->sy->sc + 1)
              and s->lv == g->sm->lv and s->pr == n->sy)
          {
            bool is_pm = false;
            for (uint32_t pi = 0; pi < sp->sp.pmm.n; pi++)
            {
              if (sp->sp.pmm.d[pi]->sy == s)
              {
                is_pm = true;
                break;
              }
            }
            if (not is_pm)
            {
              int fp = new_temporary_register(g);
              int mx = g->sm->eo;
              fprintf(
                  o,
                  "  %%t%d = getelementptr [%d x ptr], ptr %%__frame, i64 0, i64 "
                  "%u\n",
                  fp,
                  mx,
                  s->el);
              fprintf(
                  o,
                  "  store ptr %%v.%s.sc%u.%u, ptr %%t%d\n",
                  string_to_lowercase(s->nm),
                  s->sc,
                  s->el,
                  fp);
            }
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and d->k == N_OD and d->od.in)
      {
        for (uint32_t j = 0; j < d->od.id.n; j++)
        {
          Syntax_Node *id = d->od.id.d[j];
          if (id->sy)
          {
            Value_Kind k = d->od.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, d->od.ty))
                                    : VALUE_KIND_INTEGER;
            Symbol *s = id->sy;
            Type_Info *at = d->od.ty ? resolve_subtype(g->sm, d->od.ty) : 0;
            if (at and at->k == TYPE_RECORD and at->dc.n > 0 and d->od.in and d->od.in->ty)
            {
              Type_Info *it = type_canonical_concrete(d->od.in->ty);
              if (it and it->k == TYPE_RECORD and it->dc.n > 0)
              {
                for (uint32_t di = 0; di < at->dc.n and di < it->dc.n; di++)
                {
                  Syntax_Node *td = at->dc.d[di];
                  Syntax_Node *id_d = it->dc.d[di];
                  if (td->k == N_DS and id_d->k == N_DS and td->pm.df and td->pm.df->k == N_INT)
                  {
                    int tdi = new_temporary_register(g);
                    fprintf(o, "  %%t%d = add i64 0, %lld\n", tdi, (long long) td->pm.df->i);
                    V iv = generate_expression(g, d->od.in);
                    int ivd = new_temporary_register(g);
                    fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ivd, iv.id, di);
                    int dvl = new_temporary_register(g);
                    fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", dvl, ivd);
                    int cmp = new_temporary_register(g);
                    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", cmp, dvl, tdi);
                    int lok = new_label_block(g), lf = new_label_block(g);
                    emit_conditional_branch(g, cmp, lok, lf);
                    lbl(g, lok);
                    fprintf(
                        o,
                        "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n "
                        " unreachable\n");
                    lbl(g, lf);
                  }
                }
              }
            }
            {
              V v = generate_expression(g, d->od.in);
              v = value_cast(g, v, k);
              if (s and s->lv >= 0 and s->lv < g->sm->lv)
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                    value_llvm_type_string(k),
                    v.id,
                    s->lv,
                    string_to_lowercase(id->s));
              else
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                    value_llvm_type_string(k),
                    v.id,
                    string_to_lowercase(id->s),
                    s ? s->sc : 0,
                    s ? s->el : 0);
            }
          }
        }
      }
    }
    if (hlb(&n->bd.st))
    {
      int sj = new_temporary_register(g);
      int peh = new_temporary_register(g);
      fprintf(o, "  %%t%d = load ptr, ptr @__eh_cur\n", peh);
      fprintf(o, "  %%t%d = call ptr @__ada_setjmp()\n", sj);
      fprintf(o, "  store ptr %%t%d, ptr %%ej\n", sj);
      int sjb = new_temporary_register(g);
      fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", sjb);
      int sv = new_temporary_register(g);
      fprintf(o, "  %%t%d = call i32 @setjmp(ptr %%t%d)\n", sv, sjb);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", sjb);
      int ze = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", ze, sv);
      int ln = new_label_block(g), lh = new_label_block(g);
      emit_conditional_branch(g, ze, ln, lh);
      lbl(g, ln);
      for (uint32_t i = 0; i < n->bd.st.n; i++)
        generate_statement_sequence(g, n->bd.st.d[i]);
      int ld = new_label_block(g);
      emit_branch(g, ld);
      lbl(g, lh);
      int nc = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp eq ptr %%t%d, null\n", nc, peh);
      int ex = new_label_block(g), lj = new_label_block(g);
      emit_conditional_branch(g, nc, ex, lj);
      lbl(g, ex);
      emit_branch(g, ld);
      lbl(g, lj);
      fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", peh);
      fprintf(o, "  unreachable\n");
      lbl(g, ld);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", peh);
    }
    else
    {
      for (uint32_t i = 0; i < n->bd.st.n; i++)
        generate_statement_sequence(g, n->bd.st.d[i]);
    }
    g->sm->lv = sv;
    fprintf(o, "  ret void\n}\n");
  }
  break;
  case N_FB:
  {
    Syntax_Node *sp = n->bd.sp;
    GT *gt = gfnd(g->sm, sp->sp.nm);
    if (gt)
      break;
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and d->k == N_PKB)
        generate_declaration(g, d);
    }
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and (d->k == N_PB or d->k == N_FB))
        generate_declaration(g, d);
    }
    hbl(g, &n->bd.st);
    Value_Kind rk = sp->sp.rt ? token_kind_to_value_kind(resolve_subtype(g->sm, sp->sp.rt))
                              : VALUE_KIND_INTEGER;
    char nb[256];
    if (n->sy and n->sy->mangled_nm.s)
    {
      snprintf(nb, 256, "%.*s", (int) n->sy->mangled_nm.n, n->sy->mangled_nm.s);
    }
    else
    {
      encode_symbol_name(nb, 256, n->sy, sp->sp.nm, sp->sp.pmm.n, sp);
      if (n->sy)
      {
        n->sy->mangled_nm.s = arena_allocate(strlen(nb) + 1);
        memcpy((char *) n->sy->mangled_nm.s, nb, strlen(nb) + 1);
        n->sy->mangled_nm.n = strlen(nb);
      }
    }
    fprintf(o, "define linkonce_odr %s @\"%s\"(", value_llvm_type_string(rk), nb);
    int np = sp->sp.pmm.n;
    if (n->sy and n->sy->lv > 0)
      np++;
    for (int i = 0; i < np; i++)
    {
      if (i)
        fprintf(o, ", ");
      if (i < (int) sp->sp.pmm.n)
      {
        Syntax_Node *p = sp->sp.pmm.d[i];
        Value_Kind k = p->pm.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, p->pm.ty))
                                : VALUE_KIND_INTEGER;
        if (p->pm.md & 2)
          fprintf(o, "ptr %%p.%s", string_to_lowercase(p->pm.nm));
        else
          fprintf(o, "%s %%p.%s", value_llvm_type_string(k), string_to_lowercase(p->pm.nm));
      }
      else
        fprintf(o, "ptr %%__slnk");
    }
    fprintf(o, ")%s{\n", n->sy and n->sy->inl ? " alwaysinline " : " ");
    fprintf(o, "  %%ej = alloca ptr\n");
    int sv = g->sm->lv;
    g->sm->lv = n->sy ? n->sy->lv + 1 : 0;
    if (n->sy and n->sy->lv > 0)
    {
      generate_block_frame(g);
      int mx = 0;
      for (int h = 0; h < 4096; h++)
        for (Symbol *s = g->sm->sy[h]; s; s = s->nx)
          if (s->k == 0 and s->el >= 0 and s->el > mx)
            mx = s->el;
      if (mx == 0)
        fprintf(o, "  %%__frame = bitcast ptr %%__slnk to ptr\n");
    }
    if (n->sy and n->sy->lv > 0)
    {
      int fp0 = new_temporary_register(g);
      fprintf(o, "  %%t%d = getelementptr ptr, ptr %%__frame, i64 0\n", fp0);
      fprintf(o, "  store ptr %%__slnk, ptr %%t%d\n", fp0);
    }
    for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
    {
      Syntax_Node *p = sp->sp.pmm.d[i];
      Value_Kind k = p->pm.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, p->pm.ty))
                              : VALUE_KIND_INTEGER;
      Symbol *ps = p->sy;
      if (ps and ps->lv >= 0 and ps->lv < g->sm->lv)
        fprintf(
            o,
            "  %%lnk.%d.%s = alloca %s\n",
            ps->lv,
            string_to_lowercase(p->pm.nm),
            value_llvm_type_string(k));
      else
        fprintf(
            o,
            "  %%v.%s.sc%u.%u = alloca %s\n",
            string_to_lowercase(p->pm.nm),
            ps ? ps->sc : 0,
            ps ? ps->el : 0,
            value_llvm_type_string(k));
      if (p->pm.md & 2)
      {
        int lv = new_temporary_register(g);
        fprintf(
            o,
            "  %%t%d = load %s, ptr %%p.%s\n",
            lv,
            value_llvm_type_string(k),
            string_to_lowercase(p->pm.nm));
        if (ps and ps->lv >= 0 and ps->lv < g->sm->lv)
          fprintf(
              o,
              "  store %s %%t%d, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              lv,
              ps->lv,
              string_to_lowercase(p->pm.nm));
        else
          fprintf(
              o,
              "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              lv,
              string_to_lowercase(p->pm.nm),
              ps ? ps->sc : 0,
              ps ? ps->el : 0);
      }
      else
      {
        if (ps and ps->lv >= 0 and ps->lv < g->sm->lv)
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%lnk.%d.%s\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->pm.nm),
              ps->lv,
              string_to_lowercase(p->pm.nm));
        else
          fprintf(
              o,
              "  store %s %%p.%s, ptr %%v.%s.sc%u.%u\n",
              value_llvm_type_string(k),
              string_to_lowercase(p->pm.nm),
              string_to_lowercase(p->pm.nm),
              ps ? ps->sc : 0,
              ps ? ps->el : 0);
      }
    }
    if (n->sy and n->sy->lv > 0)
    {
      for (int h = 0; h < 4096; h++)
      {
        for (Symbol *s = g->sm->sy[h]; s; s = s->nx)
        {
          if (s->k == 0 and s->lv >= 0 and s->lv < g->sm->lv and not(s->df and s->df->k == N_GVL))
          {
            Value_Kind k = s->ty ? token_kind_to_value_kind(s->ty) : VALUE_KIND_INTEGER;
            Type_Info *at = s->ty ? type_canonical_concrete(s->ty) : 0;
            if (at and at->k == TYPE_ARRAY and at->hi >= at->lo)
            {
              int asz = (int) (at->hi - at->lo + 1);
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca [%d x %s]\n",
                  string_to_lowercase(s->nm),
                  s->sc,
                  s->el,
                  asz,
                  ada_to_c_type_string(at->el));
            }
            else
            {
              fprintf(
                  o,
                  "  %%v.%s.sc%u.%u = alloca %s\n",
                  string_to_lowercase(s->nm),
                  s->sc,
                  s->el,
                  value_llvm_type_string(k));
            }
            int level_diff = g->sm->lv - s->lv - 1;
            int slnk_ptr;
            if (level_diff == 0)
            {
              slnk_ptr = new_temporary_register(g);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
            }
            else
            {
              slnk_ptr = new_temporary_register(g);
              fprintf(o, "  %%t%d = bitcast ptr %%__slnk to ptr\n", slnk_ptr);
              for (int hop = 0; hop < level_diff; hop++)
              {
                int next_slnk = new_temporary_register(g);
                fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 0\n", next_slnk, slnk_ptr);
                int loaded_slnk = new_temporary_register(g);
                fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", loaded_slnk, next_slnk);
                slnk_ptr = loaded_slnk;
              }
            }
            int p = new_temporary_register(g);
            fprintf(o, "  %%t%d = getelementptr ptr, ptr %%t%d, i64 %u\n", p, slnk_ptr, s->el);
            int ptr_id = new_temporary_register(g);
            fprintf(o, "  %%t%d = load ptr, ptr %%t%d\n", ptr_id, p);
            int v = new_temporary_register(g);
            fprintf(o, "  %%t%d = load %s, ptr %%t%d\n", v, value_llvm_type_string(k), ptr_id);
            fprintf(
                o,
                "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                value_llvm_type_string(k),
                v,
                string_to_lowercase(s->nm),
                s->sc,
                s->el);
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and d->k != N_PB and d->k != N_FB and d->k != N_PKB and d->k != N_PD and d->k != N_FD)
        generate_declaration(g, d);
    }
    for (int h = 0; h < 4096; h++)
    {
      for (Symbol *s = g->sm->sy[h]; s; s = s->nx)
      {
        if (s->k == 0 and s->el >= 0 and n->sy->sc >= 0 and s->sc == (uint32_t) (n->sy->sc + 1)
            and s->lv == g->sm->lv and s->pr == n->sy)
        {
          bool is_pm = false;
          for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
          {
            if (sp->sp.pmm.d[i]->sy == s)
            {
              is_pm = true;
              break;
            }
          }
          if (not is_pm)
          {
            int fp = new_temporary_register(g);
            int mx = g->sm->eo;
            fprintf(
                o,
                "  %%t%d = getelementptr [%d x ptr], ptr %%__frame, i64 0, i64 %u\n",
                fp,
                mx,
                s->el);
            fprintf(
                o,
                "  store ptr %%v.%s.sc%u.%u, ptr %%t%d\n",
                string_to_lowercase(s->nm),
                s->sc,
                s->el,
                fp);
          }
        }
      }
    }
    for (uint32_t i = 0; i < n->bd.dc.n; i++)
    {
      Syntax_Node *d = n->bd.dc.d[i];
      if (d and d->k == N_OD and d->od.in)
      {
        for (uint32_t j = 0; j < d->od.id.n; j++)
        {
          Syntax_Node *id = d->od.id.d[j];
          if (id->sy)
          {
            Value_Kind k = d->od.ty ? token_kind_to_value_kind(resolve_subtype(g->sm, d->od.ty))
                                    : VALUE_KIND_INTEGER;
            Symbol *s = id->sy;
            Type_Info *at = d->od.ty ? resolve_subtype(g->sm, d->od.ty) : 0;
            if (at and at->k == TYPE_RECORD and at->dc.n > 0 and d->od.in and d->od.in->ty)
            {
              Type_Info *it = type_canonical_concrete(d->od.in->ty);
              if (it and it->k == TYPE_RECORD and it->dc.n > 0)
              {
                for (uint32_t di = 0; di < at->dc.n and di < it->dc.n; di++)
                {
                  Syntax_Node *td = at->dc.d[di];
                  Syntax_Node *id_d = it->dc.d[di];
                  if (td->k == N_DS and id_d->k == N_DS and td->pm.df and td->pm.df->k == N_INT)
                  {
                    int tdi = new_temporary_register(g);
                    fprintf(o, "  %%t%d = add i64 0, %lld\n", tdi, (long long) td->pm.df->i);
                    V iv = generate_expression(g, d->od.in);
                    int ivd = new_temporary_register(g);
                    fprintf(o, "  %%t%d = getelementptr i64, ptr %%t%d, i64 %u\n", ivd, iv.id, di);
                    int dvl = new_temporary_register(g);
                    fprintf(o, "  %%t%d = load i64, ptr %%t%d\n", dvl, ivd);
                    int cmp = new_temporary_register(g);
                    fprintf(o, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", cmp, dvl, tdi);
                    int lok = new_label_block(g), lf = new_label_block(g);
                    emit_conditional_branch(g, cmp, lok, lf);
                    lbl(g, lok);
                    fprintf(
                        o,
                        "  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n "
                        " unreachable\n");
                    lbl(g, lf);
                  }
                }
              }
            }
            {
              V v = generate_expression(g, d->od.in);
              v = value_cast(g, v, k);
              if (s and s->lv >= 0 and s->lv < g->sm->lv)
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%lnk.%d.%s\n",
                    value_llvm_type_string(k),
                    v.id,
                    s->lv,
                    string_to_lowercase(id->s));
              else
                fprintf(
                    o,
                    "  store %s %%t%d, ptr %%v.%s.sc%u.%u\n",
                    value_llvm_type_string(k),
                    v.id,
                    string_to_lowercase(id->s),
                    s ? s->sc : 0,
                    s ? s->el : 0);
            }
          }
        }
      }
    }
    hbl(g, &n->bd.st);
    if (hlb(&n->bd.st))
    {
      int sj = new_temporary_register(g);
      int peh = new_temporary_register(g);
      fprintf(o, "  %%t%d = load ptr, ptr @__eh_cur\n", peh);
      fprintf(o, "  %%t%d = call ptr @__ada_setjmp()\n", sj);
      fprintf(o, "  store ptr %%t%d, ptr %%ej\n", sj);
      int sjb = new_temporary_register(g);
      fprintf(o, "  %%t%d = load ptr, ptr %%ej\n", sjb);
      int sv = new_temporary_register(g);
      fprintf(o, "  %%t%d = call i32 @setjmp(ptr %%t%d)\n", sv, sjb);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", sjb);
      int ze = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp eq i32 %%t%d, 0\n", ze, sv);
      int ln = new_label_block(g), lh = new_label_block(g);
      emit_conditional_branch(g, ze, ln, lh);
      lbl(g, ln);
      for (uint32_t i = 0; i < n->bd.st.n; i++)
        generate_statement_sequence(g, n->bd.st.d[i]);
      int ld = new_label_block(g);
      emit_branch(g, ld);
      lbl(g, lh);
      int nc = new_temporary_register(g);
      fprintf(o, "  %%t%d = icmp eq ptr %%t%d, null\n", nc, peh);
      int ex = new_label_block(g), lj = new_label_block(g);
      emit_conditional_branch(g, nc, ex, lj);
      lbl(g, ex);
      emit_branch(g, ld);
      lbl(g, lj);
      fprintf(o, "  call void @longjmp(ptr %%t%d, i32 1)\n", peh);
      fprintf(o, "  unreachable\n");
      lbl(g, ld);
      fprintf(o, "  store ptr %%t%d, ptr @__eh_cur\n", peh);
    }
    else
    {
      for (uint32_t i = 0; i < n->bd.st.n; i++)
        generate_statement_sequence(g, n->bd.st.d[i]);
    }
    g->sm->lv = sv;
    if (rk == VALUE_KIND_POINTER)
    {
      fprintf(o, "  ret ptr null\n}\n");
    }
    else
    {
      fprintf(o, "  ret %s 0\n}\n", value_llvm_type_string(rk));
    }
  }
  break;
  case N_PKB:
    for (uint32_t i = 0; i < n->pb.dc.n; i++)
      if (n->pb.dc.d[i] and (n->pb.dc.d[i]->k == N_PB or n->pb.dc.d[i]->k == N_FB))
        generate_declaration(g, n->pb.dc.d[i]);
    break;
  default:
    break;
  }
}
static void gel(Code_Generator *g, Syntax_Node *n)
{
  if (n and n->k == N_PKB and n->pb.st.n > 0)
  {
    Symbol *ps = symbol_find(g->sm, n->pb.nm);
    if (ps and ps->k == 11)
      return;
    char nb[256];
    snprintf(nb, 256, "%.*s__elab", (int) n->pb.nm.n, n->pb.nm.s);
    fprintf(g->o, "define void @\"%s\"() {\n", nb);
    for (uint32_t i = 0; i < n->pb.st.n; i++)
      generate_statement_sequence(g, n->pb.st.d[i]);
    fprintf(g->o, "  ret void\n}\n");
    fprintf(
        g->o,
        "@llvm.global_ctors=appending global[1 x {i32,ptr,ptr}][{i32,ptr,ptr}{i32 65535,ptr "
        "@\"%s\",ptr null}]\n",
        nb);
  }
}
static void grt(Code_Generator *g)
{
  FILE *o = g->o;
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
      "i8]c\"%%s\\00\"\n");
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
static char *rf(const char *p)
{
  FILE *f = fopen(p, "rb");
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
static void prf(Code_Generator *g, Symbol_Manager *sm)
{
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = sm->sy[h]; s; s = s->nx)
      if (s->lv == 0 and not s->ext)
        for (uint32_t k = 0; k < s->ol.n; k++)
        {
          Syntax_Node *n = s->ol.d[k];
          if (n and (n->k == N_PB or n->k == N_FB))
          {
            Syntax_Node *sp = n->bd.sp;
            char nb[256];
            encode_symbol_name(nb, 256, n->sy, sp->sp.nm, sp->sp.pmm.n, sp);
            add_declaration(g, nb);
          }
        }
}
static LU *lfnd(Symbol_Manager *SM, String_Slice nm)
{
  for (uint32_t i = 0; i < SM->lu.n; i++)
  {
    LU *l = SM->lu.d[i];
    if (string_equal_ignore_case(l->nm, nm))
      return l;
  }
  return 0;
}
static uint64_t fts(const char *p)
{
  struct stat s;
  if (stat(p, &s))
    return 0;
  return s.st_mtime;
}
static void wali(Symbol_Manager *SM, const char *fn, Syntax_Node *cu)
{
  if (not cu or cu->cu.un.n == 0)
    return;
  Syntax_Node *u0 = cu->cu.un.d[0];
  String_Slice nm = u0->k == N_PKS ? u0->ps.nm : u0->k == N_PKB ? u0->pb.nm : N;
  char alp[520];
  if (nm.s and nm.n > 0)
  {
    const char *sl = strrchr(fn, '/');
    int pos = 0;
    if (sl)
    {
      int dl = sl - fn + 1;
      for (int i = 0; i < dl and pos < 519; i++)
        alp[pos++] = fn[i];
    }
    for (uint32_t i = 0; i < nm.n and pos < 519; i++)
      alp[pos++] = tolower(nm.s[i]);
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
  fprintf(f, "Unsigned_Big_Integer %.*s\n", (int) nm.n, nm.s);
  if (cu->cu.cx)
    for (uint32_t i = 0; i < cu->cu.cx->cx.wt.n; i++)
    {
      Syntax_Node *w = cu->cu.cx->cx.wt.d[i];
      char pf[256];
      int n = snprintf(pf, 256, "%.*s", (int) w->wt.nm.n, w->wt.nm.s);
      for (int j = 0; j < n; j++)
        pf[j] = tolower(pf[j]);
      uint64_t ts = fts(pf);
      fprintf(f, "W %.*s %lu\n", (int) w->wt.nm.n, w->wt.nm.s, (unsigned long) ts);
    }
  for (int i = 0; i < SM->dpn; i++)
    if (SM->dps[i].n and SM->dps[i].d[0])
      fprintf(f, "D %.*s\n", (int) SM->dps[i].d[0]->nm.n, SM->dps[i].d[0]->nm.s);
  for (int h = 0; h < 4096; h++)
  {
    for (Symbol *s = SM->sy[h]; s; s = s->nx)
    {
      if ((s->k == 4 or s->k == 5) and s->pr and string_equal_ignore_case(s->pr->nm, nm))
      {
        Syntax_Node *sp = s->ol.n > 0 and s->ol.d[0]->bd.sp ? s->ol.d[0]->bd.sp : 0;
        char nb[256];
        if (s->mangled_nm.s)
        {
          snprintf(nb, 256, "%.*s", (int) s->mangled_nm.n, s->mangled_nm.s);
        }
        else
        {
          encode_symbol_name(nb, 256, s, s->nm, sp ? sp->sp.pmm.n : 0, sp);
          if (s)
          {
            s->mangled_nm.s = arena_allocate(strlen(nb) + 1);
            memcpy((char *) s->mangled_nm.s, nb, strlen(nb) + 1);
            s->mangled_nm.n = strlen(nb);
          }
        }
        fprintf(f, "X %s", nb);
        if (s->k == 4)
        {
          fprintf(f, " void");
          if (sp)
            for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
            {
              Syntax_Node *p = sp->sp.pmm.d[i];
              Value_Kind k = p->pm.ty ? token_kind_to_value_kind(resolve_subtype(SM, p->pm.ty))
                                      : VALUE_KIND_INTEGER;
              fprintf(f, " %s", value_llvm_type_string(k));
            }
        }
        else
        {
          Type_Info *rt = sp and sp->sp.rt ? resolve_subtype(SM, sp->sp.rt) : 0;
          Value_Kind rk = token_kind_to_value_kind(rt);
          fprintf(f, " %s", value_llvm_type_string(rk));
          if (sp)
            for (uint32_t i = 0; i < sp->sp.pmm.n; i++)
            {
              Syntax_Node *p = sp->sp.pmm.d[i];
              Value_Kind k = p->pm.ty ? token_kind_to_value_kind(resolve_subtype(SM, p->pm.ty))
                                      : VALUE_KIND_INTEGER;
              fprintf(f, " %s", value_llvm_type_string(k));
            }
        }
        fprintf(f, "\n");
      }
      else if (
          (s->k == 0 or s->k == 2) and s->lv == 0 and s->pr
          and string_equal_ignore_case(s->pr->nm, nm))
      {
        char nb[256];
        if (s->pr and (uintptr_t) s->pr > 4096 and s->pr->nm.s)
        {
          int n = 0;
          for (uint32_t j = 0; j < s->pr->nm.n; j++)
            nb[n++] = toupper(s->pr->nm.s[j]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->pr->sc, s->pr->el);
          for (uint32_t j = 0; j < s->nm.n; j++)
            nb[n++] = toupper(s->nm.s[j]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->nm.n, s->nm.s);
        Value_Kind k = s->ty ? token_kind_to_value_kind(s->ty) : VALUE_KIND_INTEGER;
        fprintf(f, "X %s %s\n", nb, value_llvm_type_string(k));
      }
    }
  }
  for (uint32_t i = 0; i < SM->eh.n; i++)
  {
    bool f2 = 0;
    for (uint32_t j = 0; j < i; j++)
      if (string_equal_ignore_case(SM->eh.d[j], SM->eh.d[i]))
      {
        f2 = 1;
        break;
      }
    if (not f2)
      fprintf(f, "H %.*s\n", (int) SM->eh.d[i].n, SM->eh.d[i].s);
  }
  if (SM->eo > 0)
    fprintf(f, "E %d\n", SM->eo);
  fclose(f);
}
static bool lcmp(Symbol_Manager *SM, String_Slice nm, String_Slice pth)
{
  LU *ex = lfnd(SM, nm);
  if (ex and ex->cmpl)
    return true;
  char fp[512];
  snprintf(fp, 512, "%.*s.adb", (int) pth.n, pth.s);
  char *src = rf(fp);
  if (not src)
  {
    snprintf(fp, 512, "%.*s.ads", (int) pth.n, pth.s);
    src = rf(fp);
  }
  if (not src)
    return false;
  Parser p = pnw(src, strlen(src), fp);
  Syntax_Node *cu = pcu(&p);
  if (not cu)
    return false;
  Symbol_Manager sm;
  smi(&sm);
  sm.lu = SM->lu;
  sm.gt = SM->gt;
  smu(&sm, cu);
  char op[512];
  snprintf(op, 512, "%.*s.ll", (int) pth.n, pth.s);
  FILE *o = fopen(op, "w");
  Code_Generator g = {o, 0, 0, 0, &sm, {0}, 0, {0}, 0, {0}, 0, {0}, 0, {0}, {0}, {0}};
  grt(&g);
  prf(&g, &sm);
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = sm.sy[h]; s; s = s->nx)
      if ((s->k == 0 or s->k == 2) and s->lv == 0 and s->pr and not s->ext and s->ol.n == 0)
      {
        Value_Kind k = s->ty ? token_kind_to_value_kind(s->ty) : VALUE_KIND_INTEGER;
        char nb[256];
        int n = 0;
        for (uint32_t j = 0; j < s->pr->nm.n; j++)
          nb[n++] = toupper(s->pr->nm.s[j]);
        n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->pr->sc, s->pr->el);
        for (uint32_t j = 0; j < s->nm.n; j++)
          nb[n++] = toupper(s->nm.s[j]);
        nb[n] = 0;
        if (s->k == 2 and s->df and s->df->k == N_STR)
        {
          uint32_t len = s->df->s.n;
          fprintf(o, "@%s=linkonce_odr constant [%u x i8]c\"", nb, len + 1);
          for (uint32_t i = 0; i < len; i++)
          {
            char c = s->df->s.s[i];
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
          if (k == VALUE_KIND_INTEGER and s->df and s->df->k == N_INT)
          {
            snprintf(iv, 64, "%ld", s->df->i);
          }
          else if (k == VALUE_KIND_INTEGER and s->vl != 0)
          {
            snprintf(iv, 64, "%ld", s->vl);
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
          Type_Info *at = s->ty ? type_canonical_concrete(s->ty) : 0;
          if (at and at->k == TYPE_ARRAY and at->lo <= at->hi)
          {
            int asz = (int) (at->hi - at->lo + 1);
            fprintf(
                o,
                "@%s=linkonce_odr %s [%d x %s] zeroinitializer\n",
                nb,
                s->k == 2 ? "constant" : "global",
                asz,
                ada_to_c_type_string(at->el));
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
      for (Symbol *s = sm.sy[j]; s; s = s->nx)
      {
        if (s->el == i)
        {
          for (uint32_t k = 0; k < s->ol.n; k++)
            generate_declaration(&g, s->ol.d[k]);
        }
      }
    }
  }
  for (uint32_t ui = 0; ui < cu->cu.un.n; ui++)
  {
    Syntax_Node *u = cu->cu.un.d[ui];
    if (u->k == N_PKB)
      gel(&g, u);
  }
  for (uint32_t i = 0; i < sm.ib.n; i++)
    gel(&g, sm.ib.d[i]);
  emd(&g);
  fclose(o);
  LU *l = label_use_new(cu->cu.un.n > 0 ? cu->cu.un.d[0]->k : 0, nm, pth);
  l->cmpl = true;
  l->ts = fts(fp);
  lv(&SM->lu, l);
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
  char *src = rf(inf);
  if (not src)
  {
    fprintf(stderr, "e: %s\n", inf);
    return 1;
  }
  Parser p = pnw(src, strlen(src), inf);
  Syntax_Node *cu = pcu(&p);
  if (p.er or not cu)
    return 1;
  Symbol_Manager sm;
  smi(&sm);
  {
    const char *asrc = lkp(&sm, STRING_LITERAL("ascii"));
    if (asrc)
      pks(&sm, STRING_LITERAL("ascii"), asrc);
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
  if (cu->cu.cx)
    for (uint32_t i = 0; i < cu->cu.cx->cx.wt.n; i++)
    {
      Syntax_Node *w = cu->cu.cx->cx.wt.d[i];
      char *ln = string_to_lowercase(w->wt.nm);
      bool ld = 0;
      if (sd[0])
      {
        char pb[520];
        snprintf(pb, 520, "%s%s", sd, ln);
        if (lcmp(&sm, w->wt.nm, (String_Slice){pb, strlen(pb)}))
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
        if (lcmp(&sm, w->wt.nm, (String_Slice){pb, strlen(pb)}))
          ld = 1;
      }
    }
  smu(&sm, cu);
  {
    char pth[520];
    strncpy(pth, inf, 512);
    char *dot = strrchr(pth, '.');
    if (dot)
      *dot = 0;
    rali(&sm, pth);
  }
  char of[520];
  strncpy(of, inf, 512);
  char *dt = strrchr(of, '.');
  if (dt)
    *dt = 0;
  snprintf(of + strlen(of), 520 - strlen(of), ".ll");
  FILE *o = stdout;
  Code_Generator g = {o, 0, 0, 0, &sm, {0}, 0, {0}, 0, {0}, 0, {0}, 13, {0}, {0}, {0}};
  grt(&g);
  for (int h = 0; h < 4096; h++)
    for (Symbol *s = sm.sy[h]; s; s = s->nx)
      if ((s->k == 0 or s->k == 2) and (s->lv == 0 or s->pr) and not(s->pr and lfnd(&sm, s->pr->nm))
          and not s->ext)
      {
        Value_Kind k = s->ty ? token_kind_to_value_kind(s->ty) : VALUE_KIND_INTEGER;
        char nb[256];
        if (s->pr and (uintptr_t) s->pr > 4096 and s->pr->nm.s)
        {
          int n = 0;
          for (uint32_t j = 0; j < s->pr->nm.n; j++)
            nb[n++] = toupper(s->pr->nm.s[j]);
          n += snprintf(nb + n, 256 - n, "_S%dE%d__", s->pr->sc, s->pr->el);
          for (uint32_t j = 0; j < s->nm.n; j++)
            nb[n++] = toupper(s->nm.s[j]);
          nb[n] = 0;
        }
        else
          snprintf(nb, 256, "%.*s", (int) s->nm.n, s->nm.s);
        if (s->k == 2 and s->df and s->df->k == N_STR)
        {
          uint32_t len = s->df->s.n;
          fprintf(o, "@%s=linkonce_odr constant [%u x i8]c\"", nb, len + 1);
          for (uint32_t i = 0; i < len; i++)
          {
            char c = s->df->s.s[i];
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
          if (k == VALUE_KIND_INTEGER and s->df and s->df->k == N_INT)
          {
            snprintf(iv, 64, "%ld", s->df->i);
          }
          else if (k == VALUE_KIND_INTEGER and s->vl != 0)
          {
            snprintf(iv, 64, "%ld", s->vl);
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
          Type_Info *at = s->ty ? type_canonical_concrete(s->ty) : 0;
          if (at and at->k == TYPE_ARRAY and at->lo <= at->hi)
          {
            int asz = (int) (at->hi - at->lo + 1);
            fprintf(
                o,
                "@%s=linkonce_odr %s [%d x %s] zeroinitializer\n",
                nb,
                s->k == 2 ? "constant" : "global",
                asz,
                ada_to_c_type_string(at->el));
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
  prf(&g, &sm);
  for (uint32_t i = 0; i < sm.eo; i++)
    for (uint32_t j = 0; j < 4096; j++)
      for (Symbol *s = sm.sy[j]; s; s = s->nx)
        if (s->el == i and s->lv == 0)
          for (uint32_t k = 0; k < s->ol.n; k++)
            generate_declaration(&g, s->ol.d[k]);
  for (uint32_t ui = 0; ui < cu->cu.un.n; ui++)
  {
    Syntax_Node *u = cu->cu.un.d[ui];
    if (u->k == N_PKB)
      gel(&g, u);
  }
  for (uint32_t ui = 0; ui < cu->cu.un.n; ui++)
  {
    Syntax_Node *u = cu->cu.un.d[ui];
    if (u->k == N_PB or u->k == N_FB)
      gel(&g, u);
  }
  for (uint32_t i = 0; i < sm.ib.n; i++)
    gel(&g, sm.ib.d[i]);
  for (uint32_t ui = cu->cu.un.n; ui > 0; ui--)
  {
    Syntax_Node *u = cu->cu.un.d[ui - 1];
    if (u->k == N_PB)
    {
      Syntax_Node *sp = u->bd.sp;
      Symbol *ms = 0;
      for (int h = 0; h < 4096 and not ms; h++)
        for (Symbol *s = sm.sy[h]; s; s = s->nx)
          if (s->lv == 0 and string_equal_ignore_case(s->nm, sp->sp.nm))
          {
            ms = s;
            break;
          }
      char nb[256];
      encode_symbol_name(nb, 256, ms, sp->sp.nm, sp->sp.pmm.n, sp);
      fprintf(
          o,
          "define i32 @main(){\n  call void @__ada_ss_init()\n  call void @\"%s\"()\n  ret "
          "i32 0\n}\n",
          nb);
      break;
    }
  }
  emd(&g);
  if (o != stdout)
    fclose(o);
  of[strlen(of) - 3] = 0;
  wali(&sm, of, cu);
  return 0;
}
