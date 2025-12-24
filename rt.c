#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<setjmp.h>
#include<unistd.h>
#include<stdint.h>
#include<math.h>
typedef int64_t I;typedef double F;typedef char*P;typedef struct{jmp_buf j;P m;}X;typedef struct FL{void(*f)(void*);void*d;struct FL*n;}FL;
X*__ex_cur,__ex_root;FL*__fin_list;jmp_buf*__eh_cur,__eh_root;P __ss_base,__ss_ptr;I __ss_size=1048576;
void __ada_raise(P m){if(__ex_cur)longjmp(__ex_cur->j,1);fprintf(stderr,"%s\n",m);exit(1);}
I __ada_powi(I b,I e){if(e<0)return 0;I r=1;while(e){if(e&1)r*=b;b*=b;e>>=1;}return r;}
I __ada_setjmp(jmp_buf*b){__eh_cur=b;return setjmp(*b);}
void __ada_ss_init(){__ss_base=__ss_ptr=malloc(__ss_size);if(!__ss_base)__ada_raise("SS");}
P __ada_ss_allocate(I sz){P p=__ss_ptr;__ss_ptr+=sz;if(__ss_ptr-__ss_base>__ss_size)__ada_raise("SS_OVF");return p;}
P __ada_ss_mark(){return __ss_ptr;}
void __ada_ss_release(P m){__ss_ptr=m;}
void __ada_finalize(void(*f)(void*),void*d){FL*l=malloc(sizeof(FL));l->f=f;l->d=d;l->n=__fin_list;__fin_list=l;}
void __ada_finalize_all(){while(__fin_list){__fin_list->f(__fin_list->d);FL*n=__fin_list->n;free(__fin_list);__fin_list=n;}}
I __ada_value_int(P s,I n){I v=0,g=1,i=0;while(i<n&&s[i]==' ')i++;if(i<n&&s[i]=='-'){g=-1;i++;}else if(i<n&&s[i]=='+')i++;while(i<n&&s[i]>='0'&&s[i]<='9')v=v*10+(s[i++]-'0');return v*g;}
void __ada_image_int(I v,P b,I*n){if(v<0){b[0]='-';I m=sprintf(b+1,"%lld",(long long)-v);*n=m+1;}else{I m=sprintf(b,"%lld",(long long)v);*n=m;}}
void __ada_image_enum(I v,P*t,I nt,P b,I*n){if(v>=0&&v<nt){strcpy(b,t[v]);*n=strlen(t[v]);}else{*n=0;}}
void __ada_delay(F d){usleep((unsigned)(d*1e6));}
void __text_io_new_line(){putchar('\n');}
void __text_io_put_char(I c){putchar((int)c);}
void __text_io_put_line(P s){puts(s);}
void __text_io_get_char(P p){int c=getchar();*p=(c==EOF)?0:c;}
void __text_io_get_line(P b,I*n){if(!fgets(b,1024,stdin)){*n=0;return;}I m=strlen(b);if(m>0&&b[m-1]=='\n')b[--m]=0;*n=m;}
static int __rf;static char __rn[256],__rd[256];
void REPORT__TEST(P n,I nl,P d,I dl){memcpy(__rn,n,nl<255?nl:255);__rn[nl<255?nl:255]=0;memcpy(__rd,d,dl<255?dl:255);__rd[dl<255?dl:255]=0;__rf=0;printf("TEST %s: %s\n",__rn,__rd);}
void REPORT__FAILED(P m,I ml){char b[256];memcpy(b,m,ml<255?ml:255);b[ml<255?ml:255]=0;printf("FAILED: %s\n",b);__rf=1;}
void REPORT__RESULT(){printf(__rf?"FAILED\n":"PASSED\n");}
void REPORT__COMMENT(P m,I ml){char b[256];memcpy(b,m,ml<255?ml:255);b[ml<255?ml:255]=0;printf("COMMENT: %s\n",b);}
I REPORT__IDENT_INT(I x){return x;}
I REPORT__IDENT_BOOL(I x){return x;}
I REPORT__IDENT_CHAR(I x){return x;}
void REPORT__IDENT_STR(P r,P s,I n){memcpy(r,s,n);}
I REPORT__EQUAL(I x,I y){return x==y?1:0;}
