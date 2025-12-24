; ModuleID = 'rt.c'
source_filename = "rt.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.X = type { [1 x %struct.__jmp_buf_tag], ptr }
%struct.__jmp_buf_tag = type { [8 x i64], i32, %struct.__sigset_t }
%struct.__sigset_t = type { [16 x i64] }
%struct.FL = type { ptr, ptr, ptr }

@__ss_size = dso_local local_unnamed_addr global i64 1048576, align 8
@__ex_cur = dso_local local_unnamed_addr global ptr null, align 8
@stderr = external local_unnamed_addr global ptr, align 8
@.str = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@__eh_cur = dso_local local_unnamed_addr global ptr null, align 8
@__ss_ptr = dso_local local_unnamed_addr global ptr null, align 8
@__ss_base = dso_local local_unnamed_addr global ptr null, align 8
@.str.1 = private unnamed_addr constant [3 x i8] c"SS\00", align 1
@.str.2 = private unnamed_addr constant [7 x i8] c"SS_OVF\00", align 1
@__fin_list = dso_local local_unnamed_addr global ptr null, align 8
@.str.3 = private unnamed_addr constant [5 x i8] c"%lld\00", align 1
@stdin = external local_unnamed_addr global ptr, align 8
@__rn = internal global [256 x i8] zeroinitializer, align 16
@__rd = internal global [256 x i8] zeroinitializer, align 16
@__rf = internal unnamed_addr global i1 false, align 4
@.str.4 = private unnamed_addr constant [13 x i8] c"TEST %s: %s\0A\00", align 1
@.str.5 = private unnamed_addr constant [12 x i8] c"FAILED: %s\0A\00", align 1
@.str.6 = private unnamed_addr constant [8 x i8] c"FAILED\0A\00", align 1
@.str.7 = private unnamed_addr constant [8 x i8] c"PASSED\0A\00", align 1
@.str.8 = private unnamed_addr constant [13 x i8] c"COMMENT: %s\0A\00", align 1
@__ex_root = dso_local local_unnamed_addr global %struct.X zeroinitializer, align 8
@__eh_root = dso_local local_unnamed_addr global [1 x %struct.__jmp_buf_tag] zeroinitializer, align 16
@stdout = external local_unnamed_addr global ptr, align 8

; Function Attrs: noreturn nounwind uwtable
define dso_local void @__ada_raise(ptr noundef %0) local_unnamed_addr #0 {
  %2 = load ptr, ptr @__ex_cur, align 8, !tbaa !5
  %3 = icmp eq ptr %2, null
  br i1 %3, label %5, label %4

4:                                                ; preds = %1
  tail call void @longjmp(ptr noundef nonnull %2, i32 noundef 1) #22
  unreachable

5:                                                ; preds = %1
  %6 = load ptr, ptr @stderr, align 8, !tbaa !5
  %7 = tail call i32 (ptr, ptr, ...) @fprintf(ptr noundef %6, ptr noundef nonnull @.str, ptr noundef %0) #23
  tail call void @exit(i32 noundef 1) #22
  unreachable
}

; Function Attrs: noreturn nounwind
declare void @longjmp(ptr noundef, i32 noundef) local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @fprintf(ptr nocapture noundef, ptr nocapture noundef readonly, ...) local_unnamed_addr #2

; Function Attrs: noreturn nounwind
declare void @exit(i32 noundef) local_unnamed_addr #1

; Function Attrs: nofree norecurse nosync nounwind memory(none) uwtable
define dso_local i64 @__ada_powi(i64 noundef %0, i64 noundef %1) local_unnamed_addr #3 {
  %3 = icmp slt i64 %1, 0
  br i1 %3, label %17, label %4

4:                                                ; preds = %2
  %5 = icmp eq i64 %1, 0
  br i1 %5, label %17, label %6

6:                                                ; preds = %4, %6
  %7 = phi i64 [ %13, %6 ], [ 1, %4 ]
  %8 = phi i64 [ %15, %6 ], [ %1, %4 ]
  %9 = phi i64 [ %14, %6 ], [ %0, %4 ]
  %10 = and i64 %8, 1
  %11 = icmp eq i64 %10, 0
  %12 = select i1 %11, i64 1, i64 %9
  %13 = mul nsw i64 %12, %7
  %14 = mul nsw i64 %9, %9
  %15 = lshr i64 %8, 1
  %16 = icmp ult i64 %8, 2
  br i1 %16, label %17, label %6, !llvm.loop !9

17:                                               ; preds = %6, %4, %2
  %18 = phi i64 [ 0, %2 ], [ 1, %4 ], [ %13, %6 ]
  ret i64 %18
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #4

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #4

; Function Attrs: nounwind uwtable
define dso_local i64 @__ada_setjmp(ptr noundef %0) local_unnamed_addr #5 {
  store ptr %0, ptr @__eh_cur, align 8, !tbaa !5
  %2 = call i32 @_setjmp(ptr noundef %0) #24
  %3 = sext i32 %2 to i64
  ret i64 %3
}

; Function Attrs: nounwind returns_twice
declare i32 @_setjmp(ptr noundef) local_unnamed_addr #6

; Function Attrs: nounwind uwtable
define dso_local void @__ada_ss_init() local_unnamed_addr #5 {
  %1 = load i64, ptr @__ss_size, align 8, !tbaa !11
  %2 = tail call noalias ptr @malloc(i64 noundef %1) #25
  store ptr %2, ptr @__ss_ptr, align 8, !tbaa !5
  store ptr %2, ptr @__ss_base, align 8, !tbaa !5
  %3 = icmp eq ptr %2, null
  br i1 %3, label %4, label %11

4:                                                ; preds = %0
  %5 = load ptr, ptr @__ex_cur, align 8, !tbaa !5
  %6 = icmp eq ptr %5, null
  br i1 %6, label %8, label %7

7:                                                ; preds = %4
  tail call void @longjmp(ptr noundef nonnull %5, i32 noundef 1) #22
  unreachable

8:                                                ; preds = %4
  %9 = load ptr, ptr @stderr, align 8, !tbaa !5
  %10 = tail call i32 (ptr, ptr, ...) @fprintf(ptr noundef %9, ptr noundef nonnull @.str, ptr noundef nonnull @.str.1) #23
  tail call void @exit(i32 noundef 1) #22
  unreachable

11:                                               ; preds = %0
  ret void
}

; Function Attrs: mustprogress nofree nounwind willreturn allockind("alloc,uninitialized") allocsize(0) memory(inaccessiblemem: readwrite)
declare noalias noundef ptr @malloc(i64 noundef) local_unnamed_addr #7

; Function Attrs: nounwind uwtable
define dso_local ptr @__ada_ss_allocate(i64 noundef %0) local_unnamed_addr #5 {
  %2 = load ptr, ptr @__ss_ptr, align 8, !tbaa !5
  %3 = getelementptr inbounds i8, ptr %2, i64 %0
  store ptr %3, ptr @__ss_ptr, align 8, !tbaa !5
  %4 = load ptr, ptr @__ss_base, align 8, !tbaa !5
  %5 = ptrtoint ptr %3 to i64
  %6 = ptrtoint ptr %4 to i64
  %7 = sub i64 %5, %6
  %8 = load i64, ptr @__ss_size, align 8, !tbaa !11
  %9 = icmp sgt i64 %7, %8
  br i1 %9, label %10, label %17

10:                                               ; preds = %1
  %11 = load ptr, ptr @__ex_cur, align 8, !tbaa !5
  %12 = icmp eq ptr %11, null
  br i1 %12, label %14, label %13

13:                                               ; preds = %10
  tail call void @longjmp(ptr noundef nonnull %11, i32 noundef 1) #22
  unreachable

14:                                               ; preds = %10
  %15 = load ptr, ptr @stderr, align 8, !tbaa !5
  %16 = tail call i32 (ptr, ptr, ...) @fprintf(ptr noundef %15, ptr noundef nonnull @.str, ptr noundef nonnull @.str.2) #23
  tail call void @exit(i32 noundef 1) #22
  unreachable

17:                                               ; preds = %1
  ret ptr %2
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(read, argmem: none, inaccessiblemem: none) uwtable
define dso_local ptr @__ada_ss_mark() local_unnamed_addr #8 {
  %1 = load ptr, ptr @__ss_ptr, align 8, !tbaa !5
  ret ptr %1
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(write, argmem: none, inaccessiblemem: none) uwtable
define dso_local void @__ada_ss_release(ptr noundef %0) local_unnamed_addr #9 {
  store ptr %0, ptr @__ss_ptr, align 8, !tbaa !5
  ret void
}

; Function Attrs: mustprogress nofree nounwind willreturn memory(readwrite, argmem: none) uwtable
define dso_local void @__ada_finalize(ptr noundef %0, ptr noundef %1) local_unnamed_addr #10 {
  %3 = tail call noalias dereferenceable_or_null(24) ptr @malloc(i64 noundef 24) #25
  store ptr %0, ptr %3, align 8, !tbaa !13
  %4 = getelementptr inbounds %struct.FL, ptr %3, i64 0, i32 1
  store ptr %1, ptr %4, align 8, !tbaa !15
  %5 = load ptr, ptr @__fin_list, align 8, !tbaa !5
  %6 = getelementptr inbounds %struct.FL, ptr %3, i64 0, i32 2
  store ptr %5, ptr %6, align 8, !tbaa !16
  store ptr %3, ptr @__fin_list, align 8, !tbaa !5
  ret void
}

; Function Attrs: nounwind uwtable
define dso_local void @__ada_finalize_all() local_unnamed_addr #5 {
  %1 = load ptr, ptr @__fin_list, align 8, !tbaa !5
  %2 = icmp eq ptr %1, null
  br i1 %2, label %12, label %3

3:                                                ; preds = %0, %3
  %4 = phi ptr [ %10, %3 ], [ %1, %0 ]
  %5 = load ptr, ptr %4, align 8, !tbaa !13
  %6 = getelementptr inbounds %struct.FL, ptr %4, i64 0, i32 1
  %7 = load ptr, ptr %6, align 8, !tbaa !15
  tail call void %5(ptr noundef %7) #26
  %8 = load ptr, ptr @__fin_list, align 8, !tbaa !5
  %9 = getelementptr inbounds %struct.FL, ptr %8, i64 0, i32 2
  %10 = load ptr, ptr %9, align 8, !tbaa !16
  tail call void @free(ptr noundef %8) #26
  store ptr %10, ptr @__fin_list, align 8, !tbaa !5
  %11 = icmp eq ptr %10, null
  br i1 %11, label %12, label %3, !llvm.loop !17

12:                                               ; preds = %3, %0
  ret void
}

; Function Attrs: mustprogress nounwind willreturn allockind("free") memory(argmem: readwrite, inaccessiblemem: readwrite)
declare void @free(ptr allocptr nocapture noundef) local_unnamed_addr #11

; Function Attrs: nofree norecurse nosync nounwind memory(argmem: read) uwtable
define dso_local i64 @__ada_value_int(ptr nocapture noundef readonly %0, i64 noundef %1) local_unnamed_addr #12 {
  %3 = icmp sgt i64 %1, 0
  br i1 %3, label %4, label %17

4:                                                ; preds = %2, %8
  %5 = phi i64 [ %9, %8 ], [ 0, %2 ]
  %6 = getelementptr inbounds i8, ptr %0, i64 %5
  %7 = load i8, ptr %6, align 1, !tbaa !18
  switch i8 %7, label %13 [
    i8 32, label %8
    i8 45, label %11
  ]

8:                                                ; preds = %4
  %9 = add nuw nsw i64 %5, 1
  %10 = icmp eq i64 %9, %1
  br i1 %10, label %34, label %4, !llvm.loop !19

11:                                               ; preds = %4
  %12 = add nuw nsw i64 %5, 1
  br label %17

13:                                               ; preds = %4
  %14 = icmp eq i8 %7, 43
  %15 = zext i1 %14 to i64
  %16 = add nuw nsw i64 %5, %15
  br label %17

17:                                               ; preds = %2, %13, %11
  %18 = phi i64 [ -1, %11 ], [ 1, %13 ], [ 1, %2 ]
  %19 = phi i64 [ %12, %11 ], [ %16, %13 ], [ 0, %2 ]
  %20 = icmp slt i64 %19, %1
  br i1 %20, label %21, label %34

21:                                               ; preds = %17, %28
  %22 = phi i64 [ %30, %28 ], [ %19, %17 ]
  %23 = phi i64 [ %32, %28 ], [ 0, %17 ]
  %24 = getelementptr inbounds i8, ptr %0, i64 %22
  %25 = load i8, ptr %24, align 1, !tbaa !18
  %26 = add i8 %25, -48
  %27 = icmp ult i8 %26, 10
  br i1 %27, label %28, label %34

28:                                               ; preds = %21
  %29 = mul nsw i64 %23, 10
  %30 = add i64 %22, 1
  %31 = zext nneg i8 %26 to i64
  %32 = add nsw i64 %29, %31
  %33 = icmp eq i64 %30, %1
  br i1 %33, label %34, label %21, !llvm.loop !20

34:                                               ; preds = %8, %28, %21, %17
  %35 = phi i64 [ %18, %17 ], [ %18, %21 ], [ %18, %28 ], [ 1, %8 ]
  %36 = phi i64 [ 0, %17 ], [ %32, %28 ], [ %23, %21 ], [ 0, %8 ]
  %37 = mul nsw i64 %36, %35
  ret i64 %37
}

; Function Attrs: nofree nounwind uwtable
define dso_local void @__ada_image_int(i64 noundef %0, ptr nocapture noundef writeonly %1, ptr nocapture noundef writeonly %2) local_unnamed_addr #13 {
  %4 = icmp slt i64 %0, 0
  br i1 %4, label %5, label %11

5:                                                ; preds = %3
  store i8 45, ptr %1, align 1, !tbaa !18
  %6 = getelementptr inbounds i8, ptr %1, i64 1
  %7 = sub nsw i64 0, %0
  %8 = tail call i32 (ptr, ptr, ...) @sprintf(ptr noundef nonnull dereferenceable(1) %6, ptr noundef nonnull dereferenceable(1) @.str.3, i64 noundef %7) #26
  %9 = sext i32 %8 to i64
  %10 = add nsw i64 %9, 1
  br label %14

11:                                               ; preds = %3
  %12 = tail call i32 (ptr, ptr, ...) @sprintf(ptr noundef nonnull dereferenceable(1) %1, ptr noundef nonnull dereferenceable(1) @.str.3, i64 noundef %0) #26
  %13 = sext i32 %12 to i64
  br label %14

14:                                               ; preds = %11, %5
  %15 = phi i64 [ %13, %11 ], [ %10, %5 ]
  store i64 %15, ptr %2, align 8, !tbaa !11
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @sprintf(ptr noalias nocapture noundef writeonly, ptr nocapture noundef readonly, ...) local_unnamed_addr #2

; Function Attrs: mustprogress nofree nounwind willreturn memory(readwrite, inaccessiblemem: none) uwtable
define dso_local void @__ada_image_enum(i64 noundef %0, ptr nocapture noundef readonly %1, i64 noundef %2, ptr noundef %3, ptr nocapture noundef writeonly %4) local_unnamed_addr #14 {
  %6 = icmp sgt i64 %0, -1
  %7 = icmp slt i64 %0, %2
  %8 = and i1 %6, %7
  br i1 %8, label %9, label %15

9:                                                ; preds = %5
  %10 = getelementptr inbounds ptr, ptr %1, i64 %0
  %11 = load ptr, ptr %10, align 8, !tbaa !5
  %12 = tail call ptr @strcpy(ptr noundef nonnull dereferenceable(1) %3, ptr noundef nonnull dereferenceable(1) %11) #26
  %13 = load ptr, ptr %10, align 8, !tbaa !5
  %14 = tail call i64 @strlen(ptr noundef nonnull dereferenceable(1) %13) #27
  br label %15

15:                                               ; preds = %5, %9
  %16 = phi i64 [ %14, %9 ], [ 0, %5 ]
  store i64 %16, ptr %4, align 8, !tbaa !11
  ret void
}

; Function Attrs: mustprogress nofree nounwind willreturn memory(argmem: readwrite)
declare ptr @strcpy(ptr noalias noundef returned writeonly, ptr noalias nocapture noundef readonly) local_unnamed_addr #15

; Function Attrs: mustprogress nofree nounwind willreturn memory(argmem: read)
declare i64 @strlen(ptr nocapture noundef) local_unnamed_addr #16

; Function Attrs: nounwind uwtable
define dso_local void @__ada_delay(double noundef %0) local_unnamed_addr #5 {
  %2 = fmul double %0, 1.000000e+06
  %3 = fptoui double %2 to i32
  %4 = tail call i32 @usleep(i32 noundef %3) #26
  ret void
}

declare i32 @usleep(i32 noundef) local_unnamed_addr #17

; Function Attrs: nofree nounwind uwtable
define dso_local void @__text_io_new_line() local_unnamed_addr #13 {
  %1 = load ptr, ptr @stdout, align 8, !tbaa !5
  %2 = tail call i32 @putc(i32 noundef 10, ptr noundef %1)
  ret void
}

; Function Attrs: nofree nounwind uwtable
define dso_local void @__text_io_put_char(i64 noundef %0) local_unnamed_addr #13 {
  %2 = trunc i64 %0 to i32
  %3 = load ptr, ptr @stdout, align 8, !tbaa !5
  %4 = tail call i32 @putc(i32 noundef %2, ptr noundef %3)
  ret void
}

; Function Attrs: nofree nounwind uwtable
define dso_local void @__text_io_put_line(ptr nocapture noundef readonly %0) local_unnamed_addr #13 {
  %2 = tail call i32 @puts(ptr noundef nonnull dereferenceable(1) %0)
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @puts(ptr nocapture noundef readonly) local_unnamed_addr #2

; Function Attrs: nofree nounwind uwtable
define dso_local void @__text_io_get_char(ptr nocapture noundef writeonly %0) local_unnamed_addr #13 {
  %2 = load ptr, ptr @stdin, align 8, !tbaa !5
  %3 = tail call i32 @getc(ptr noundef %2)
  %4 = icmp eq i32 %3, -1
  %5 = trunc i32 %3 to i8
  %6 = select i1 %4, i8 0, i8 %5
  store i8 %6, ptr %0, align 1, !tbaa !18
  ret void
}

; Function Attrs: nofree nounwind uwtable
define dso_local void @__text_io_get_line(ptr noundef %0, ptr nocapture noundef writeonly %1) local_unnamed_addr #13 {
  %3 = load ptr, ptr @stdin, align 8, !tbaa !5
  %4 = tail call ptr @fgets(ptr noundef %0, i32 noundef 1024, ptr noundef %3)
  %5 = icmp eq ptr %4, null
  br i1 %5, label %17, label %6

6:                                                ; preds = %2
  %7 = tail call i64 @strlen(ptr noundef nonnull dereferenceable(1) %0) #27
  %8 = icmp sgt i64 %7, 0
  br i1 %8, label %9, label %17

9:                                                ; preds = %6
  %10 = getelementptr i8, ptr %0, i64 %7
  %11 = getelementptr i8, ptr %10, i64 -1
  %12 = load i8, ptr %11, align 1, !tbaa !18
  %13 = icmp eq i8 %12, 10
  br i1 %13, label %14, label %17

14:                                               ; preds = %9
  %15 = add nsw i64 %7, -1
  %16 = getelementptr inbounds i8, ptr %0, i64 %15
  store i8 0, ptr %16, align 1, !tbaa !18
  br label %17

17:                                               ; preds = %6, %9, %14, %2
  %18 = phi i64 [ 0, %2 ], [ %15, %14 ], [ %7, %9 ], [ %7, %6 ]
  store i64 %18, ptr %1, align 8, !tbaa !11
  ret void
}

; Function Attrs: nofree nounwind
declare noundef ptr @fgets(ptr noundef, i32 noundef, ptr nocapture noundef) local_unnamed_addr #2

; Function Attrs: nofree nounwind uwtable
define dso_local void @REPORT__TEST(ptr nocapture noundef readonly %0, i64 noundef %1, ptr nocapture noundef readonly %2, i64 noundef %3) local_unnamed_addr #13 {
  %5 = tail call i64 @llvm.smin.i64(i64 %1, i64 255)
  tail call void @llvm.memcpy.p0.p0.i64(ptr nonnull align 16 @__rn, ptr align 1 %0, i64 %5, i1 false)
  %6 = getelementptr inbounds [256 x i8], ptr @__rn, i64 0, i64 %5
  store i8 0, ptr %6, align 1, !tbaa !18
  %7 = tail call i64 @llvm.smin.i64(i64 %3, i64 255)
  tail call void @llvm.memcpy.p0.p0.i64(ptr nonnull align 16 @__rd, ptr align 1 %2, i64 %7, i1 false)
  %8 = getelementptr inbounds [256 x i8], ptr @__rd, i64 0, i64 %7
  store i8 0, ptr %8, align 1, !tbaa !18
  store i1 false, ptr @__rf, align 4
  %9 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, ptr noundef nonnull @__rn, ptr noundef nonnull @__rd)
  ret void
}

; Function Attrs: mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #18

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #2

; Function Attrs: nofree nounwind uwtable
define dso_local void @REPORT__FAILED(ptr nocapture noundef readonly %0, i64 noundef %1) local_unnamed_addr #13 {
  %3 = alloca [256 x i8], align 16
  call void @llvm.lifetime.start.p0(i64 256, ptr nonnull %3) #26
  %4 = tail call i64 @llvm.smin.i64(i64 %1, i64 255)
  call void @llvm.memcpy.p0.p0.i64(ptr nonnull align 16 %3, ptr align 1 %0, i64 %4, i1 false)
  %5 = getelementptr inbounds [256 x i8], ptr %3, i64 0, i64 %4
  store i8 0, ptr %5, align 1, !tbaa !18
  %6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, ptr noundef nonnull %3)
  store i1 true, ptr @__rf, align 4
  call void @llvm.lifetime.end.p0(i64 256, ptr nonnull %3) #26
  ret void
}

; Function Attrs: nofree nounwind uwtable
define dso_local void @REPORT__RESULT() local_unnamed_addr #13 {
  %1 = load i1, ptr @__rf, align 4
  %2 = select i1 %1, ptr @.str.6, ptr @.str.7
  %3 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) %2)
  ret void
}

; Function Attrs: nofree nounwind uwtable
define dso_local void @REPORT__COMMENT(ptr nocapture noundef readonly %0, i64 noundef %1) local_unnamed_addr #13 {
  %3 = alloca [256 x i8], align 16
  call void @llvm.lifetime.start.p0(i64 256, ptr nonnull %3) #26
  %4 = tail call i64 @llvm.smin.i64(i64 %1, i64 255)
  call void @llvm.memcpy.p0.p0.i64(ptr nonnull align 16 %3, ptr align 1 %0, i64 %4, i1 false)
  %5 = getelementptr inbounds [256 x i8], ptr %3, i64 0, i64 %4
  store i8 0, ptr %5, align 1, !tbaa !18
  %6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.8, ptr noundef nonnull %3)
  call void @llvm.lifetime.end.p0(i64 256, ptr nonnull %3) #26
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local noundef i64 @REPORT__IDENT_INT(i64 noundef returned %0) local_unnamed_addr #19 {
  ret i64 %0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local noundef i64 @REPORT__IDENT_BOOL(i64 noundef returned %0) local_unnamed_addr #19 {
  ret i64 %0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local noundef i64 @REPORT__IDENT_CHAR(i64 noundef returned %0) local_unnamed_addr #19 {
  ret i64 %0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable
define dso_local void @REPORT__IDENT_STR(ptr nocapture noundef writeonly %0, ptr nocapture noundef readonly %1, i64 noundef %2) local_unnamed_addr #20 {
  tail call void @llvm.memcpy.p0.p0.i64(ptr align 1 %0, ptr align 1 %1, i64 %2, i1 false)
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local noundef i64 @REPORT__EQUAL(i64 noundef %0, i64 noundef %1) local_unnamed_addr #19 {
  %3 = icmp eq i64 %0, %1
  %4 = zext i1 %3 to i64
  ret i64 %4
}

; Function Attrs: nofree nounwind
declare noundef i32 @putc(i32 noundef, ptr nocapture noundef) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare noundef i32 @getc(ptr nocapture noundef) local_unnamed_addr #2

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.smin.i64(i64, i64) #21

attributes #0 = { noreturn nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { noreturn nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nofree norecurse nosync nounwind memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #5 = { nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind returns_twice "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { mustprogress nofree nounwind willreturn allockind("alloc,uninitialized") allocsize(0) memory(inaccessiblemem: readwrite) "alloc-family"="malloc" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { mustprogress nofree norecurse nosync nounwind willreturn memory(read, argmem: none, inaccessiblemem: none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { mustprogress nofree norecurse nosync nounwind willreturn memory(write, argmem: none, inaccessiblemem: none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { mustprogress nofree nounwind willreturn memory(readwrite, argmem: none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #11 = { mustprogress nounwind willreturn allockind("free") memory(argmem: readwrite, inaccessiblemem: readwrite) "alloc-family"="malloc" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #12 = { nofree norecurse nosync nounwind memory(argmem: read) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #13 = { nofree nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #14 = { mustprogress nofree nounwind willreturn memory(readwrite, inaccessiblemem: none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #15 = { mustprogress nofree nounwind willreturn memory(argmem: readwrite) "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #16 = { mustprogress nofree nounwind willreturn memory(argmem: read) "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #17 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #18 = { mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #19 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #20 = { mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #21 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #22 = { noreturn nounwind }
attributes #23 = { cold }
attributes #24 = { nounwind returns_twice }
attributes #25 = { nounwind allocsize(0) }
attributes #26 = { nounwind }
attributes #27 = { nounwind willreturn memory(read) }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 18.1.3 (1ubuntu1)"}
!5 = !{!6, !6, i64 0}
!6 = !{!"any pointer", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C/C++ TBAA"}
!9 = distinct !{!9, !10}
!10 = !{!"llvm.loop.mustprogress"}
!11 = !{!12, !12, i64 0}
!12 = !{!"long", !7, i64 0}
!13 = !{!14, !6, i64 0}
!14 = !{!"FL", !6, i64 0, !6, i64 8, !6, i64 16}
!15 = !{!14, !6, i64 8}
!16 = !{!14, !6, i64 16}
!17 = distinct !{!17, !10}
!18 = !{!7, !7, i64 0}
!19 = distinct !{!19, !10}
!20 = distinct !{!20, !10}
