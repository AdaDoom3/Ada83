declare i32 @puts(ptr)
declare i32 @printf(ptr,...)
@.fail_prefix=private constant[9 x i8]c"FAILED: \00"
@.pass_msg=private constant[7 x i8]c"PASSED\00"

define void @"REPORT__TEST"(i64 %name,i64 %desc){
  ret void
}
define void @"REPORT__FAILED"(i64 %msg){
  %s=inttoptr i64 %msg to ptr
  call i32 @puts(ptr @.fail_prefix)
  call i32 @puts(ptr %s)
  ret void
}
define void @"REPORT__RESULT"(){
  call i32 @puts(ptr @.pass_msg)
  ret void
}
define i64 @"REPORT__IDENT_INT"(i64 %x){
  ret i64 %x
}
define i64 @"REPORT__IDENT_BOOL"(i64 %x){
  ret i64 %x
}
define i64 @"REPORT__IDENT_CHAR"(i64 %x){
  ret i64 %x
}
define i64 @"REPORT__IDENT_STR"(i64 %x){
  ret i64 %x
}
