target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
declare i64 @strlen(ptr)
declare void @REPORT__TEST(ptr, i64, ptr, i64)
declare void @REPORT__FAILED(ptr, i64)
declare void @REPORT__RESULT()
declare void @REPORT__COMMENT(ptr, i64)
declare i64 @REPORT__IDENT_INT(i64)
declare i64 @REPORT__IDENT_BOOL(i64)
declare i64 @REPORT__IDENT_CHAR(i64)
declare i64 @REPORT__EQUAL(i64, i64)

define void @"REPORT__TEST.2"(ptr %n, ptr %d) {
  %nl = call i64 @strlen(ptr %n)
  %dl = call i64 @strlen(ptr %d)
  call void @REPORT__TEST(ptr %n, i64 %nl, ptr %d, i64 %dl)
  ret void
}
define void @"REPORT__FAILED.1"(ptr %m) {
  %ml = call i64 @strlen(ptr %m)
  call void @REPORT__FAILED(ptr %m, i64 %ml)
  ret void
}
define void @"REPORT__RESULT.0"() {
  call void @REPORT__RESULT()
  ret void
}
define void @"REPORT__COMMENT.1"(ptr %m) {
  %ml = call i64 @strlen(ptr %m)
  call void @REPORT__COMMENT(ptr %m, i64 %ml)
  ret void
}
define i64 @"REPORT__IDENT_5FINT.1"(i64 %x) {
  %r = call i64 @REPORT__IDENT_INT(i64 %x)
  ret i64 %r
}
define i64 @"REPORT__IDENT_5FBOOL.1"(i64 %x) {
  %r = call i64 @REPORT__IDENT_BOOL(i64 %x)
  ret i64 %r
}
define i64 @"REPORT__IDENT_5FCHAR.1"(i64 %x) {
  %r = call i64 @REPORT__IDENT_CHAR(i64 %x)
  ret i64 %r
}
define ptr @"REPORT__IDENT_5FSTR.1"(ptr %s) {
  ret ptr %s
}
define i64 @"REPORT__EQUAL.2"(i64 %x, i64 %y) {
  %r = call i64 @REPORT__EQUAL(i64 %x, i64 %y)
  ret i64 %r
}
