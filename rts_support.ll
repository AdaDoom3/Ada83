; Minimal Ada83 runtime support
declare i32 @setjmp(ptr)
declare void @longjmp(ptr,i32)
declare ptr @malloc(i64)
declare i32 @usleep(i32)
declare i32 @puts(ptr)
declare i32 @sprintf(ptr,ptr,...)
declare double @pow(double,double)

; Exception handling globals
@__eh_cur = global ptr null
@__ex_cur = global ptr null

; Exception handling functions
define ptr @__ada_setjmp(){
  %p=call ptr @malloc(i64 200)
  ret ptr %p
}

define void @__ada_raise(ptr %msg){
  store ptr %msg,ptr @__ex_cur
  call void @longjmp(ptr @__eh_cur,i32 1)
  ret void
}

; Delay
define void @__ada_delay(i64 %us){
  %t=trunc i64 %us to i32
  call i32 @usleep(i32 %t)
  ret void
}

; Integer power
define i64 @__ada_powi(i64 %a,i64 %b){
  %c=icmp slt i64 %b,0
  br i1 %c,label %e,label %s
s:
  %p=phi i64[1,%0],[%p2,%l]
  %x=phi i64[%a,%0],[%n,%l]
  %i=phi i64[%b,%0],[%j,%l]
  %t=and i64 %i,1
  %z=icmp eq i64 %t,0
  br i1 %z,label %k,label %m
m:
  %r=mul i64 %p,%x
  br label %k
k:
  %p2=phi i64[%p,%s],[%r,%m]
  %j=lshr i64 %i,1
  %n=mul i64 %x,%x
  %d=icmp eq i64 %j,0
  br i1 %d,label %o,label %l
l:
  br label %s
o:
  ret i64 %p2
e:
  ret i64 0
}

; TEXT_IO implementation
define void @TEXT_IO__PUT_LINE(ptr %s){
  call i32 @puts(ptr %s)
  ret void
}

define void @TEXT_IO__NEW_LINE(){
  %nl=alloca[2 x i8]
  store[2 x i8]c"\0A\00",ptr %nl
  call i32 @puts(ptr %nl)
  ret void
}

; REPORT package global state
@REPORT__F = global i64 0

@.str_test=constant[6 x i8]c"TEST \00"
@.str_colon_space=constant[3 x i8]c": \00"
@.str_failed_prefix=constant[9 x i8]c"FAILED: \00"
@.str_passed=constant[7 x i8]c"PASSED\00"
@.str_failed=constant[7 x i8]c"FAILED\00"
@.fmt_str=constant[3 x i8]c"%s\00"

declare i32 @printf(ptr,...)

; REPORT__TEST
define void @REPORT__TEST(ptr %name,ptr %desc){
  call i32(ptr,...)@printf(ptr @.fmt_str,ptr @.str_test)
  call i32(ptr,...)@printf(ptr @.fmt_str,ptr %name)
  call i32(ptr,...)@printf(ptr @.fmt_str,ptr @.str_colon_space)
  call i32 @puts(ptr %desc)
  store i64 0,ptr @REPORT__F
  ret void
}

; REPORT__FAILED
define void @REPORT__FAILED(ptr %msg){
  call i32(ptr,...)@printf(ptr @.fmt_str,ptr @.str_failed_prefix)
  call i32 @puts(ptr %msg)
  store i64 1,ptr @REPORT__F
  ret void
}

; REPORT__RESULT
define void @REPORT__RESULT(){
  %f=load i64,ptr @REPORT__F
  %t=icmp eq i64 %f,0
  br i1 %t,label %pass,label %fail
pass:
  call i32 @puts(ptr @.str_passed)
  ret void
fail:
  call i32 @puts(ptr @.str_failed)
  ret void
}

; REPORT identity functions
define i64 @REPORT__IDENT_INT(i64 %x){ret i64 %x}
define i64 @REPORT__IDENT_BOOL(i64 %x){ret i64 %x}
define i64 @REPORT__IDENT_CHAR(i64 %x){ret i64 %x}
define ptr @REPORT__IDENT_STR(ptr %x){ret ptr %x}
