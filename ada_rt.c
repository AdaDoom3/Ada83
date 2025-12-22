#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<setjmp.h>
#include<pthread.h>
#include<unistd.h>
#include<stdio.h>
typedef struct{int64_t*d;size_t c,n;}V;typedef struct{void*o;void(*f)(void*);}F;typedef struct L{F f;struct L*n;}L;
static __thread jmp_buf*E;static __thread char*X;static __thread L*FL;static uint8_t*S;static size_t SP,SC;static pthread_mutex_t M=PTHREAD_MUTEX_INITIALIZER;
static inline void*A(size_t z){if(!S){S=malloc(SC=1<<20);SP=0;}if(SP+z>SC){SC<<=1;S=realloc(S,SC);}void*p=S+SP;SP=(SP+z+7)&~7;return p;}
static inline void R(size_t m){SP=m;}static inline size_t MK(){return SP;}
V VN(size_t n){V v={malloc(n*8),n,n};return v;}void VP(V*v,int64_t x){if(v->n>=v->c){v->c=v->c?v->c*2:8;v->d=realloc(v->d,v->c*8);}v->d[v->n++]=x;}
int64_t*VG(V*v,size_t i){return i<v->n?v->d+i:0;}void VF(V*v){free(v->d);v->d=0;v->n=v->c=0;}
static inline int64_t PW(int64_t b,int64_t e){int64_t r=1;while(e){if(e&1)r*=b;b*=b;e>>=1;}return r;}
static inline int64_t GC(int64_t a,int64_t b){while(b){int64_t t=b;b=a%b;a=t;}return a;}
static inline void SW(int64_t*a,int64_t*b){int64_t t=*a;*a=*b;*b=t;}
void QS(int64_t*a,int l,int h){if(l<h){int64_t p=a[h],i=l-1,j;for(j=l;j<h;j++)if(a[j]<p)SW(a+(++i),a+j);SW(a+(i+1),a+h);int pi=i+1;QS(a,l,pi-1);QS(a,pi+1,h);}}
void MS(int64_t*a,int l,int m,int h){int n1=m-l+1,n2=h-m,*L=A(n1*8),*R=A(n2*8),i,j,k;memcpy(L,a+l,n1*8);memcpy(R,a+m+1,n2*8);for(i=j=0,k=l;i<n1&&j<n2;)a[k++]=L[i]<=R[j]?L[i++]:R[j++];while(i<n1)a[k++]=L[i++];while(j<n2)a[k++]=R[j++];}
void MG(int64_t*a,int l,int h){if(l<h){int m=l+(h-l)/2;MG(a,l,m);MG(a,m+1,h);MS(a,l,m,h);}}
int64_t BS(int64_t*a,int n,int64_t x){int l=0,h=n-1;while(l<=h){int m=l+(h-l)/2;if(a[m]==x)return m;if(a[m]<x)l=m+1;else h=m-1;}return-1;}
typedef struct H{int64_t k,v;struct H*n;}H;typedef struct{H**b;size_t m;}HT;
HT*HC(size_t m){HT*h=malloc(sizeof(HT));h->b=calloc(m,sizeof(H*));h->m=m;return h;}
void HP(HT*h,int64_t k,int64_t v){size_t i=k%h->m;H*e=malloc(sizeof(H));e->k=k;e->v=v;e->n=h->b[i];h->b[i]=e;}
int64_t*HG(HT*h,int64_t k){for(H*e=h->b[k%h->m];e;e=e->n)if(e->k==k)return&e->v;return 0;}
void HF(HT*h){for(size_t i=0;i<h->m;i++){for(H*e=h->b[i],*t;e;e=t){t=e->n;free(e);}}free(h->b);free(h);}
typedef struct N{int64_t d;struct N*l,*r;}N;N*IN(N*n,int64_t d){if(!n){n=malloc(sizeof(N));n->d=d;n->l=n->r=0;}else if(d<n->d)n->l=IN(n->l,d);else if(d>n->d)n->r=IN(n->r,d);return n;}
N*FN(N*n,int64_t d){if(!n)return 0;if(d<n->d)return FN(n->l,d);if(d>n->d)return FN(n->r,d);return n;}
void TF(N*n){if(n){TF(n->l);TF(n->r);free(n);}}
typedef struct{int64_t*d;int f,r,c;}Q;Q*QC(int c){Q*q=malloc(sizeof(Q));q->d=malloc(c*8);q->f=q->r=0;q->c=c;return q;}
void QE(Q*q,int64_t x){q->d[q->r++]=x;if(q->r==q->c)q->r=0;}int64_t QD(Q*q){int64_t x=q->d[q->f++];if(q->f==q->c)q->f=0;return x;}
int QZ(Q*q){return q->f==q->r;}void QF_(Q*q){free(q->d);free(q);}
typedef struct{int64_t*d;int t;}ST;ST*SC_(int c){ST*s=malloc(sizeof(ST));s->d=malloc(c*8);s->t=-1;return s;}
void SPU(ST*s,int64_t x){s->d[++s->t]=x;}int64_t SPO(ST*s){return s->d[s->t--];}int SZ(ST*s){return s->t<0;}void SF(ST*s){free(s->d);free(s);}
typedef struct U{int p,r;}U;U*UF[1024];void UM(){for(int i=0;i<1024;i++){UF[i]=malloc(sizeof(U));UF[i]->p=i;UF[i]->r=0;}}
int UG(int x){if(UF[x]->p!=x)UF[x]->p=UG(UF[x]->p);return UF[x]->p;}void UU(int x,int y){int rx=UG(x),ry=UG(y);if(rx!=ry){if(UF[rx]->r<UF[ry]->r)UF[rx]->p=ry;else if(UF[rx]->r>UF[ry]->r)UF[ry]->p=rx;else{UF[ry]->p=rx;UF[rx]->r++;}}}
void RS(char*e){X=e;longjmp(*E,1);}void FA(void*o,void(*f)(void*)){L*n=malloc(sizeof(L));n->f.o=o;n->f.f=f;n->n=FL;FL=n;}
void FC(){while(FL){FL->f.f(FL->f.o);L*t=FL;FL=FL->n;free(t);}}void DL(int64_t us){usleep(us);}
typedef struct{pthread_t t;void*(*f)(void*);void*a;}T;T*TC(void*(*f)(void*),void*a){T*t=malloc(sizeof(T));t->f=f;t->a=a;pthread_create(&t->t,0,f,a);return t;}
void TJ(T*t){pthread_join(t->t,0);free(t);}
char*IM(int64_t v){char*b=malloc(32);snprintf(b,32,"%lld",v);return b;}
char*SA(char*a,char*b){size_t la=strlen(a),lb=strlen(b);char*r=malloc(la+lb+1);memcpy(r,a,la);memcpy(r+la,b,lb+1);return r;}
void*MC(void*d,void*s,size_t n){return memcpy(d,s,n);}void*MZ(void*p,int c,size_t n){return memset(p,c,n);}
static inline int64_t MI(int64_t a,int64_t b){return a<b?a:b;}static inline int64_t MX(int64_t a,int64_t b){return a>b?a:b;}
static inline int64_t AB(int64_t x){return x<0?-x:x;}static inline int64_t SG(int64_t x){return x>0?1:x<0?-1:0;}
void PR(char*s){printf("%s\n",s);}void PI(int64_t v){printf("%lld\n",v);}void PC(char c){putchar(c);}
