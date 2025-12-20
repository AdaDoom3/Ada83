declare i32 @setjmp(ptr)
declare void @longjmp(ptr,i32)
declare ptr @malloc(i64)
@__eh_cur = global ptr null
@__ex_cur = global ptr null
define ptr @__ada_setjmp(){%p=call ptr @malloc(i64 200)ret ptr %p}
define void @__ada_raise(ptr %msg){store ptr %msg,ptr @__ex_cur call void @longjmp(ptr @__eh_cur,i32 1)ret void}
declare i32 @usleep(i32)
define void @__ada_delay(i64 %us){%t=trunc i64 %us to i32 call i32 @usleep(i32 %t)ret void}
define i64 @__ada_powi(i64 %a,i64 %b){%c=icmp slt i64 %b,0 br i1 %c,label %e,label %s
s:%p=phi i64[1,%0],[%p2,%l]%x=phi i64[%a,%0],[%n,%l]%i=phi i64[%b,%0],[%j,%l]%t=and i64 %i,1 %z=icmp eq i64 %t,0 br i1 %z,label %k,label %m
m:%r=mul i64 %p,%x br label %k
k:%p2=phi i64[%p,%s],[%r,%m]%j=lshr i64 %i,1 %n=mul i64 %x,%x %d=icmp eq i64 %j,0 br i1 %d,label %o,label %l
l:br label %s
o:ret i64 %p2
e:ret i64 0}
@.fmt_i64=constant[5 x i8]c"%lld\00"
@.fmt_f64=constant[3 x i8]c"%g\00"
@.str_nl=constant[2 x i8]c"\0A\00"
declare i32 @puts(ptr)
define void @__text_io_put_i64(i64 %v){%s=alloca[32 x i8]%p=getelementptr[32 x i8],ptr %s,i64 0,i64 0 call i32(ptr,ptr,...)@sprintf(ptr %p,ptr @.fmt_i64,i64 %v)call i32 @puts(ptr %p)ret void}
define void @__text_io_put_f64(double %v){%s=alloca[32 x i8]%p=getelementptr[32 x i8],ptr %s,i64 0,i64 0 call i32(ptr,ptr,...)@sprintf(ptr %p,ptr @.fmt_f64,double %v)call i32 @puts(ptr %p)ret void}
define void @__text_io_put_str(ptr %s){call i32 @puts(ptr %s)ret void}
define void @__text_io_newline(){call i32 @puts(ptr @.str_nl)ret void}
define void @TEXT_IO__PUT_LINE(ptr %s){call i32 @puts(ptr %s)ret void}
define void @TEXT_IO__NEW_LINE(){call i32 @puts(ptr @.str_nl)ret void}
declare i32 @sprintf(ptr,ptr,...)
