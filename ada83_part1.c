#define _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<stdint.h>
#include<stdbool.h>
#include<stdarg.h>
#include<setjmp.h>
#include<pthread.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<math.h>
typedef struct{char*b,*p,*e;}A;typedef struct{const char*s;uint32_t n;}S;typedef struct{uint32_t l,c;const char*f;}L;
#define Z(x)((S){x,sizeof(x)-1})
#define N ((S){0,0})
static A M={0};static int E=0;
static void*al(size_t n){n=(n+7)&~7;if(!M.b||M.p+n>M.e){size_t z=1<<24;M.b=M.p=malloc(z);M.e=M.b+z;}void*r=M.p;M.p+=n;return memset(r,0,n);}
static void ar(){if(M.b)M.p=M.b;}
static S sd(S s){char*p=al(s.n+1);memcpy(p,s.s,s.n);return(S){p,s.n};}
static bool se(S a,S b){return a.n==b.n&&!memcmp(a.s,b.s,a.n);}
static bool si(S a,S b){if(a.n!=b.n)return 0;for(uint32_t i=0;i<a.n;i++)if(tolower(a.s[i])!=tolower(b.s[i]))return 0;return 1;}
static char*LC(S s){static char b[8][256];static int i;char*p=b[i++&7];uint32_t n=s.n<255?s.n:255;for(uint32_t j=0;j<n;j++)p[j]=tolower(s.s[j]);p[n]=0;return p;}
static uint64_t sh(S s){uint64_t h=14695981039346656037ULL;for(uint32_t i=0;i<s.n;i++)h=(h^(uint8_t)tolower(s.s[i]))*1099511628211ULL;return h;}
static _Noreturn void die(L l,const char*f,...){va_list v;va_start(v,f);fprintf(stderr,"%s:%u:%u: ",l.f,l.l,l.c);vfprintf(stderr,f,v);fputc('\n',stderr);va_end(v);E++;exit(1);}
typedef enum{T_EOF=0,T_ERR,T_ID,T_INT,T_REAL,T_CHAR,T_STR,T_LP,T_RP,T_LB,T_RB,T_CM,T_DT,T_SC,T_CL,T_TK,T_AS,T_AR,T_DD,T_LL,T_GG,T_BX,T_BR,T_EQ,T_NE,T_LT,T_LE,T_GT,T_GE,T_PL,T_MN,T_ST,T_SL,T_AM,T_EX,T_AB,T_ABS,T_ACC,T_ACCS,T_ALITK,T_ALL,T_AND,T_ATHN,T_ARR,T_AT,T_BEG,T_BOD,T_CSE,T_CONST,T_DEC,T_DEL,T_DELTA,T_DIG,T_DO,T_ELSE,T_ELSIF,T_END,T_ENT,T_EXCP,T_EXIT,T_FOR,T_FUN,T_GEN,T_GOTO,T_IF,T_IN,T_IS,T_LIM,T_LOOP,T_MOD,T_NEW,T_NOT,T_NULL,T_OF,T_OR,T_OREL,T_OTH,T_OUT,T_PKG,T_PGM,T_PRV,T_PROC,T_RAS,T_RNG,T_REC,T_REM,T_REN,T_RET,T_REV,T_SEL,T_SEP,T_SUB,T_TSK,T_TER,T_THEN,T_TYP,T_USE,T_WHN,T_WHI,T_WITH,T_XOR,T_CNT}Tk;
enum{CHK_OVF=1,CHK_RNG=2,CHK_IDX=4,CHK_DSC=8,CHK_LEN=16,CHK_DIV=32,CHK_ELB=64,CHK_ACC=128,CHK_STG=256};
static const char*TN[T_CNT]={[T_EOF]="eof",[T_ID]="id",[T_INT]="int",[T_REAL]="real",[T_CHAR]="char",[T_STR]="str",[T_LP]="(",[T_RP]=")",[T_LB]="[",[T_RB]="]",[T_CM]=",",[T_DT]=".",[T_SC]=";",[T_CL]=":",[T_TK]="'",[T_AS]=":=",[T_AR]="=>",[T_DD]="..",[T_LL]="<<",[T_GG]=">>",[T_BX]="<>",[T_BR]="|",[T_EQ]="=",[T_NE]="/=",[T_LT]="<",[T_LE]="<=",[T_GT]=">",[T_GE]=">=",[T_PL]="+",[T_MN]="-",[T_ST]="*",[T_SL]="/",[T_AM]="&",[T_EX]="**",[T_AB]="ABORT",[T_ABS]="ABS",[T_ACC]="ACCEPT",[T_ACCS]="ACCESS",[T_ALITK]="ALIASED",[T_ALL]="ALL",[T_AND]="AND",[T_ATHN]="AND THEN",[T_ARR]="ARRAY",[T_AT]="AT",[T_BEG]="BEGIN",[T_BOD]="BODY",[T_CSE]="CASE",[T_CONST]="CONSTANT",[T_DEC]="DECLARE",[T_DEL]="DELAY",[T_DELTA]="DELTA",[T_DIG]="DIGITS",[T_DO]="DO",[T_ELSE]="ELSE",[T_ELSIF]="ELSIF",[T_END]="END",[T_ENT]="ENTRY",[T_EXCP]="EXCEPTION",[T_EXIT]="EXIT",[T_FOR]="FOR",[T_FUN]="FUNCTION",[T_GEN]="GENERIC",[T_GOTO]="GOTO",[T_IF]="IF",[T_IN]="IN",[T_IS]="IS",[T_LIM]="LIMITED",[T_LOOP]="LOOP",[T_MOD]="MOD",[T_NEW]="NEW",[T_NOT]="NOT",[T_NULL]="NULL",[T_OF]="OF",[T_OR]="OR",[T_OREL]="OR ELSE",[T_OTH]="OTHERS",[T_OUT]="OUT",[T_PKG]="PACKAGE",[T_PGM]="PRAGMA",[T_PRV]="PRIVATE",[T_PROC]="PROCEDURE",[T_RAS]="RAISE",[T_RNG]="RANGE",[T_REC]="RECORD",[T_REM]="REM",[T_REN]="RENAMES",[T_RET]="RETURN",[T_REV]="REVERSE",[T_SEL]="SELECT",[T_SEP]="SEPARATE",[T_SUB]="SUBTYPE",[T_TSK]="TASK",[T_TER]="TERMINATE",[T_THEN]="THEN",[T_TYP]="TYPE",[T_USE]="USE",[T_WHN]="WHEN",[T_WHI]="WHILE",[T_WITH]="WITH",[T_XOR]="XOR"};
typedef struct{Tk t;L l;S lit;int64_t iv;double fv;}Tn;typedef struct{const char*s,*c,*e;uint32_t ln,cl;const char*f;}Lx;
