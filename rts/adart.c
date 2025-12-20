#include<stdio.h>
#include<stdlib.h>
#include<setjmp.h>
#include<unistd.h>
#include<math.h>
void*__eh_cur=0;
void*__ex_cur=0;
void*__ada_setjmp(){return malloc(200);}
void __ada_raise(void*msg){__ex_cur=msg;longjmp(__eh_cur,1);}
long long __ada_powi(long long a,long long b){if(b<0)__ada_raise("CONSTRAINT_ERROR");if(b==0)return 1;long long r=1;while(b){if(b&1)r*=a;a*=a;b>>=1;}return r;}
void __ada_delay(long long us){usleep((unsigned)us);}
void __text_io_put_i64(long long v){printf("%lld",v);}
void __text_io_put_f64(double v){printf("%g",v);}
void __text_io_put_str(char*s){puts(s);}
void __text_io_newline(){puts("");}
