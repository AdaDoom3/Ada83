declare i32 @strlen(ptr)
declare ptr @malloc(i64)
declare void @llvm.memcpy.p0.p0.i64(ptr,ptr,i64,i1)
declare void @__text_io_put_str(ptr)
declare void @__text_io_newline()
declare void @TEXT_IO__PUT_LINE(ptr)
declare void @TEXT_IO__NEW_LINE()
@REPORT__F = global i64 0
define void @REPORT__TEST(ptr %p.NAME, ptr %p.DESC) {
  %t1 = alloca [6 x i8]
  store [6 x i8] c"TEST \00", ptr %t1
  %t2 = getelementptr [6 x i8], ptr %t1, i64 0, i64 0
  %t3 = call i64 @strlen(ptr %t2)
  %t4 = call i64 @strlen(ptr %p.NAME)
  %t5 = add i64 %t3, %t4
  %t6 = add i64 %t5, 1
  %t7 = call ptr @malloc(i64 %t6)
  call void @llvm.memcpy.p0.p0.i64(ptr %t7, ptr %t2, i64 %t3, i1 false)
  %t8 = getelementptr i8, ptr %t7, i64 %t3
  call void @llvm.memcpy.p0.p0.i64(ptr %t8, ptr %p.NAME, i64 %t4, i1 false)
  %t9 = getelementptr i8, ptr %t7, i64 %t5
  store i8 0, ptr %t9
  %t10 = alloca [3 x i8]
  store [3 x i8] c": \00", ptr %t10
  %t11 = getelementptr [3 x i8], ptr %t10, i64 0, i64 0
  %t12 = call i64 @strlen(ptr %t7)
  %t13 = call i64 @strlen(ptr %t11)
  %t14 = add i64 %t12, %t13
  %t15 = add i64 %t14, 1
  %t16 = call ptr @malloc(i64 %t15)
  call void @llvm.memcpy.p0.p0.i64(ptr %t16, ptr %t7, i64 %t12, i1 false)
  %t17 = getelementptr i8, ptr %t16, i64 %t12
  call void @llvm.memcpy.p0.p0.i64(ptr %t17, ptr %t11, i64 %t13, i1 false)
  %t18 = getelementptr i8, ptr %t16, i64 %t14
  store i8 0, ptr %t18
  %t19 = call i64 @strlen(ptr %t16)
  %t20 = call i64 @strlen(ptr %p.DESC)
  %t21 = add i64 %t19, %t20
  %t22 = add i64 %t21, 1
  %t23 = call ptr @malloc(i64 %t22)
  call void @llvm.memcpy.p0.p0.i64(ptr %t23, ptr %t16, i64 %t19, i1 false)
  %t24 = getelementptr i8, ptr %t23, i64 %t19
  call void @llvm.memcpy.p0.p0.i64(ptr %t24, ptr %p.DESC, i64 %t20, i1 false)
  %t25 = getelementptr i8, ptr %t23, i64 %t21
  store i8 0, ptr %t25
  call void @__text_io_put_str(ptr %t23)
  call void @__text_io_newline()
  store i64 0, ptr @REPORT__F
  ret void
}
define void @REPORT__FAILED(ptr %p.MSG) {
  %t1 = alloca [9 x i8]
  store [9 x i8] c"FAILED: \00", ptr %t1
  %t2 = getelementptr [9 x i8], ptr %t1, i64 0, i64 0
  %t3 = call i64 @strlen(ptr %t2)
  %t4 = call i64 @strlen(ptr %p.MSG)
  %t5 = add i64 %t3, %t4
  %t6 = add i64 %t5, 1
  %t7 = call ptr @malloc(i64 %t6)
  call void @llvm.memcpy.p0.p0.i64(ptr %t7, ptr %t2, i64 %t3, i1 false)
  %t8 = getelementptr i8, ptr %t7, i64 %t3
  call void @llvm.memcpy.p0.p0.i64(ptr %t8, ptr %p.MSG, i64 %t4, i1 false)
  %t9 = getelementptr i8, ptr %t7, i64 %t5
  store i8 0, ptr %t9
  call void @__text_io_put_str(ptr %t7)
  call void @__text_io_newline()
  store i64 1, ptr @REPORT__F
  ret void
}
define void @REPORT__RESULT() {
  %t1 = load i64, ptr @REPORT__F
  %t2 = icmp eq i64 %t1, 0
  br i1 %t2, label %L0, label %L1
L0:
  %t3 = alloca [7 x i8]
  store [7 x i8] c"PASSED\00", ptr %t3
  %t4 = getelementptr [7 x i8], ptr %t3, i64 0, i64 0
  call void @__text_io_put_str(ptr %t4)
  call void @__text_io_newline()
  br label %L2
L1:
  %t5 = alloca [7 x i8]
  store [7 x i8] c"FAILED\00", ptr %t5
  %t6 = getelementptr [7 x i8], ptr %t5, i64 0, i64 0
  call void @__text_io_put_str(ptr %t6)
  call void @__text_io_newline()
  br label %L2
L2:
  ret void
}
define i64 @REPORT__IDENT_INT(i64 %p.X) {
  ret i64 %p.X
}
define i64 @REPORT__IDENT_BOOL(i64 %p.X) {
  ret i64 %p.X
}
define i64 @REPORT__IDENT_CHAR(i64 %p.X) {
  ret i64 %p.X
}
define ptr @REPORT__IDENT_STR(ptr %p.X) {
  ret ptr %p.X
}
