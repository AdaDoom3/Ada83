; Ada83 Compiler Output
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; External declarations
declare i32 @memcmp(ptr, ptr, i64)
declare i32 @strncasecmp(ptr, ptr, i64)
declare i32 @setjmp(ptr)
declare void @longjmp(ptr, i32)
declare void @exit(i32)
declare ptr @malloc(i64)
declare ptr @realloc(ptr, i64)
declare void @free(ptr)
declare i32 @usleep(i32)
declare i32 @pthread_create(ptr, ptr, ptr, ptr)
declare i32 @pthread_join(ptr, ptr)
declare i32 @printf(ptr, ...)
declare i32 @putchar(i32)
declare i32 @getchar()
declare ptr @fopen(ptr, ptr)
declare i32 @fclose(ptr)
declare i32 @fputc(i32, ptr)
declare i32 @fgetc(ptr)
declare i32 @ungetc(i32, ptr)
declare i32 @feof(ptr)
declare i32 @fflush(ptr)
declare i32 @remove(ptr)
declare i64 @ftell(ptr)
declare i32 @fseek(ptr, i64, i32)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare double @llvm.pow.f64(double, double)

; Integer'VALUE helper
define linkonce_odr i64 @__ada_integer_value({ ptr, ptr } %str) {
entry:
  %data = extractvalue { ptr, ptr } %str, 0
  %str_bptr = extractvalue { ptr, ptr } %str, 1
  %low_gep = getelementptr { i64, i64 }, ptr %str_bptr, i32 0, i32 0
  %low = load i64, ptr %low_gep
  %high_gep = getelementptr { i64, i64 }, ptr %str_bptr, i32 0, i32 1
  %high = load i64, ptr %high_gep
  br label %loop
loop:
  %result = phi i64 [ 0, %entry ], [ %next_result, %cont ]
  %idx = phi i64 [ %low, %entry ], [ %next_idx, %cont ]
  %neg = phi i1 [ false, %entry ], [ %next_neg, %cont ]
  %done = icmp sgt i64 %idx, %high
  br i1 %done, label %finish, label %body
body:
  %adj_idx = sub i64 %idx, %low
  %ptr = getelementptr i8, ptr %data, i64 %adj_idx
  %ch = load i8, ptr %ptr
  %is_minus = icmp eq i8 %ch, 45  ; '-'
  br i1 %is_minus, label %set_neg, label %check_digit
set_neg:
  br label %cont_neg
check_digit:
  %is_digit = icmp uge i8 %ch, 48  ; '0'
  %is_digit2 = icmp ule i8 %ch, 57  ; '9'
  %is_dig_both = and i1 %is_digit, %is_digit2
  br i1 %is_dig_both, label %add_digit, label %cont
add_digit:
  %digit = sub i8 %ch, 48
  %digit64 = zext i8 %digit to i64
  %mul = mul i64 %result, 10
  %new_val = add i64 %mul, %digit64
  br label %cont
cont_neg:
  %next_neg_v = phi i1 [ true, %set_neg ]
  br label %cont
cont:
  %next_result = phi i64 [ %result, %check_digit ], [ %new_val, %add_digit ], [ %result, %cont_neg ]
  %next_neg = phi i1 [ %neg, %check_digit ], [ %neg, %add_digit ], [ %next_neg_v, %cont_neg ]
  %next_idx = add i64 %idx, 1
  br label %loop
finish:
  %final = select i1 %neg, i64 0, i64 %result
  %neg_result = sub i64 0, %result
  %ret = select i1 %neg, i64 %neg_result, i64 %result
  ret i64 %ret
}

; Float'VALUE declaration (to be implemented)
declare double @strtod(ptr, ptr)
define linkonce_odr double @__ada_float_value({ ptr, ptr } %str) {
entry:
  %data = extractvalue { ptr, ptr } %str, 0
  %str_bptr = extractvalue { ptr, ptr } %str, 1
  %fv_low_gep = getelementptr { i64, i64 }, ptr %str_bptr, i32 0, i32 0
  %fv_low = load i64, ptr %fv_low_gep
  %fv_high_gep = getelementptr { i64, i64 }, ptr %str_bptr, i32 0, i32 1
  %fv_high = load i64, ptr %fv_high_gep
  %result = call double @strtod(ptr %data, ptr null)
  ret double %result
}

; Integer exponentiation helper
define linkonce_odr i64 @__ada_integer_pow(i64 %base, i64 %exp) {
entry:
  %is_neg = icmp slt i64 %exp, 0
  br i1 %is_neg, label %neg_exp, label %pos_exp
neg_exp:
  ret i64 0  ; negative exponent for integer = 0
pos_exp:
  %is_zero = icmp eq i64 %exp, 0
  br i1 %is_zero, label %ret_one, label %loop
ret_one:
  ret i64 1
loop:
  %result = phi i64 [ 1, %pos_exp ], [ %new_result, %loop ]
  %i = phi i64 [ 0, %pos_exp ], [ %next_i, %loop ]
  %new_result = mul i64 %result, %base
  %next_i = add i64 %i, 1
  %done = icmp eq i64 %next_i, %exp
  br i1 %done, label %exit, label %loop
exit:
  ret i64 %new_result
}

; Runtime globals
@__ss_base = linkonce_odr global ptr null
@__ss_ptr = linkonce_odr global i64 0
@__ss_size = linkonce_odr global i64 0
@__eh_cur = linkonce_odr global ptr null
@__ex_cur = linkonce_odr global i64 0
@__fin_list = linkonce_odr global ptr null
@__entry_queue = linkonce_odr global ptr null
@.fmt_ue = linkonce_odr constant [27 x i8] c"Unhandled exception: %lld\0A\00"

; Standard exception identities
@__exc.constraint_error = linkonce_odr constant i64 1
@__exc.numeric_error = linkonce_odr constant i64 2
@__exc.program_error = linkonce_odr constant i64 3
@__exc.storage_error = linkonce_odr constant i64 4
@__exc.tasking_error = linkonce_odr constant i64 5

; Secondary stack runtime
define linkonce_odr void @__ada_ss_init() {
  %p = call ptr @malloc(i64 1048576)
  store ptr %p, ptr @__ss_base
  store i64 1048576, ptr @__ss_size
  store i64 0, ptr @__ss_ptr
  ret void
}

define linkonce_odr i64 @__ada_sec_stack_mark() {
  %m = load i64, ptr @__ss_ptr
  ret i64 %m
}

define linkonce_odr void @__ada_sec_stack_release(i64 %m) {
  store i64 %m, ptr @__ss_ptr
  ret void
}

define linkonce_odr ptr @__ada_sec_stack_alloc(i64 %sz) {
entry:
  %1 = load ptr, ptr @__ss_base
  %2 = icmp eq ptr %1, null
  br i1 %2, label %init, label %alloc
init:
  call void @__ada_ss_init()
  %3 = load ptr, ptr @__ss_base
  br label %alloc
alloc:
  %p = phi ptr [%1, %entry], [%3, %init]
  %4 = load i64, ptr @__ss_ptr
  %5 = add i64 %sz, 7
  %6 = and i64 %5, -8
  %7 = add i64 %4, %6
  %8 = load i64, ptr @__ss_size
  %9 = icmp ult i64 %7, %8
  br i1 %9, label %ok, label %grow
grow:
  %10 = mul i64 %8, 2
  store i64 %10, ptr @__ss_size
  %11 = call ptr @realloc(ptr %p, i64 %10)
  store ptr %11, ptr @__ss_base
  br label %ok
ok:
  %12 = phi ptr [%p, %alloc], [%11, %grow]
  %13 = getelementptr i8, ptr %12, i64 %4
  store i64 %7, ptr @__ss_ptr
  ret ptr %13
}

; Exception handling runtime
define linkonce_odr void @__ada_push_handler(ptr %h) {
  %old = load ptr, ptr @__eh_cur
  %link = getelementptr { ptr, [200 x i8] }, ptr %h, i32 0, i32 0
  store ptr %old, ptr %link
  store ptr %h, ptr @__eh_cur
  ret void
}

define linkonce_odr void @__ada_pop_handler() {
  %cur = load ptr, ptr @__eh_cur
  %is_null = icmp eq ptr %cur, null
  br i1 %is_null, label %done, label %pop
pop:
  %link = getelementptr { ptr, [200 x i8] }, ptr %cur, i32 0, i32 0
  %prev = load ptr, ptr %link
  store ptr %prev, ptr @__eh_cur
  br label %done
done:
  ret void
}

define linkonce_odr void @__ada_raise(i64 %exc_id) {
  store i64 %exc_id, ptr @__ex_cur
  %frame = load ptr, ptr @__eh_cur
  %is_null = icmp eq ptr %frame, null
  br i1 %is_null, label %unhandled, label %jump
jump:
  %jb = getelementptr { ptr, [200 x i8] }, ptr %frame, i32 0, i32 1
  call void @longjmp(ptr %jb, i32 1)
  unreachable
unhandled:
  call i32 (ptr, ...) @printf(ptr @.fmt_ue, i64 %exc_id)
  call void @exit(i32 1)
  unreachable
}

define linkonce_odr void @__ada_reraise() {
  %exc = load i64, ptr @__ex_cur
  call void @__ada_raise(i64 %exc)
  unreachable
}

define linkonce_odr i64 @__ada_current_exception() {
  %exc = load i64, ptr @__ex_cur
  ret i64 %exc
}

; Arithmetic runtime
define linkonce_odr i64 @__ada_powi(i64 %base, i64 %exp) {
entry:
  %result = alloca i64
  store i64 1, ptr %result
  %e = alloca i64
  store i64 %exp, ptr %e
  br label %loop
loop:
  %ev = load i64, ptr %e
  %cmp = icmp sgt i64 %ev, 0
  br i1 %cmp, label %body, label %done
body:
  %rv = load i64, ptr %result
  %nv = mul i64 %rv, %base
  store i64 %nv, ptr %result
  %ev2 = load i64, ptr %e
  %ev3 = sub i64 %ev2, 1
  store i64 %ev3, ptr %e
  br label %loop
done:
  %final = load i64, ptr %result
  ret i64 %final
}

; Tasking runtime
define linkonce_odr void @__ada_delay(i64 %us) {
  %t = trunc i64 %us to i32
  call i32 @usleep(i32 %t)
  ret void
}

define linkonce_odr void @__ada_task_abort(ptr %task) {
entry:
  %1 = icmp eq ptr %task, null
  br i1 %1, label %done, label %abort
abort:
  ; In full impl: set abort flag, signal condition
  store i8 1, ptr %task  ; Mark abort pending
  br label %done
done:
  ret void
}

define linkonce_odr void @__ada_task_terminate() {
  ; Check if master task is complete, if so exit
  call void @exit(i32 0)
  unreachable
}

define linkonce_odr ptr @__ada_task_start(ptr %task_func, ptr %parent_frame) {
entry:
  %tid = alloca ptr
  %_rc = call i32 @pthread_create(ptr %tid, ptr null, ptr %task_func, ptr %parent_frame)
  %handle = load ptr, ptr %tid
  ret ptr %handle
}

define linkonce_odr void @__ada_entry_call(ptr %task, i64 %entry_idx, ptr %params) {
entry:
  ; Allocate rendezvous record: { task_ptr, entry_idx, params, complete_flag, next }
  %rv = call ptr @malloc(i64 40)
  store ptr %task, ptr %rv
  %1 = getelementptr i64, ptr %rv, i64 1
  store i64 %entry_idx, ptr %1
  %2 = getelementptr ptr, ptr %rv, i64 2
  store ptr %params, ptr %2
  %3 = getelementptr i8, ptr %rv, i64 32
  store i8 0, ptr %3  ; complete = false
  ; Enqueue to task's entry queue (append to @__entry_queue)
  %4 = load ptr, ptr @__entry_queue
  %5 = getelementptr ptr, ptr %rv, i64 4
  store ptr %4, ptr %5
  store ptr %rv, ptr @__entry_queue
  br label %wait
wait:
  ; Spin-wait for complete flag (yield to scheduler)
  %_u1 = call i32 @usleep(i32 100)
  %6 = load i8, ptr %3
  %7 = icmp eq i8 %6, 0
  br i1 %7, label %wait, label %done
done:
  call void @free(ptr %rv)
  ret void
}

define linkonce_odr ptr @__ada_accept_wait(i64 %entry_idx) {
entry:
  br label %wait
wait:
  ; Scan entry queue for matching entry index
  %q = load ptr, ptr @__entry_queue
  %is_empty = icmp eq ptr %q, null
  br i1 %is_empty, label %spin, label %check
spin:
  %_u2 = call i32 @usleep(i32 100)
  br label %wait
check:
  ; Check if first entry matches
  %1 = getelementptr i64, ptr %q, i64 1
  %2 = load i64, ptr %1
  %3 = icmp eq i64 %2, %entry_idx
  br i1 %3, label %found, label %spin
found:
  ; Dequeue and return caller's parameter block
  %4 = getelementptr ptr, ptr %q, i64 4
  %5 = load ptr, ptr %4
  store ptr %5, ptr @__entry_queue
  %6 = getelementptr ptr, ptr %q, i64 2
  %params = load ptr, ptr %6
  ret ptr %q
}

define linkonce_odr ptr @__ada_accept_try(i64 %entry_idx) {
entry:
  %q = load ptr, ptr @__entry_queue
  %is_empty = icmp eq ptr %q, null
  br i1 %is_empty, label %none, label %check
check:
  %1 = getelementptr i64, ptr %q, i64 1
  %2 = load i64, ptr %1
  %3 = icmp eq i64 %2, %entry_idx
  br i1 %3, label %found, label %none
found:
  ; Dequeue and return caller's parameter block
  %4 = getelementptr ptr, ptr %q, i64 4
  %5 = load ptr, ptr %4
  store ptr %5, ptr @__entry_queue
  ret ptr %q
none:
  ret ptr null
}

define linkonce_odr void @__ada_accept_complete(ptr %rv) {
entry:
  %1 = getelementptr i8, ptr %rv, i64 32
  store i8 1, ptr %1  ; complete = true
  ret void
}

; Finalization runtime
define linkonce_odr void @__ada_finalize(ptr %obj, ptr %fn) {
  %1 = call ptr @malloc(i64 24)
  %2 = getelementptr ptr, ptr %1, i64 0
  store ptr %obj, ptr %2
  %3 = getelementptr ptr, ptr %1, i64 1
  store ptr %fn, ptr %3
  %4 = load ptr, ptr @__fin_list
  %5 = getelementptr ptr, ptr %1, i64 2
  store ptr %4, ptr %5
  store ptr %1, ptr @__fin_list
  ret void
}

define linkonce_odr void @__ada_finalize_all() {
entry:
  %1 = load ptr, ptr @__fin_list
  br label %loop
loop:
  %p = phi ptr [%1, %entry], [%9, %fin]
  %2 = icmp eq ptr %p, null
  br i1 %2, label %done, label %fin
fin:
  %3 = getelementptr ptr, ptr %p, i64 0
  %4 = load ptr, ptr %3
  %5 = getelementptr ptr, ptr %p, i64 1
  %6 = load ptr, ptr %5
  call void %6(ptr %4)
  %8 = getelementptr ptr, ptr %p, i64 2
  %9 = load ptr, ptr %8
  call void @free(ptr %p)
  br label %loop
done:
  ret void
}

; TEXT_IO runtime
@stdin = external global ptr
@stdout = external global ptr
@stderr = external global ptr
declare i32 @fputs(ptr, ptr)
declare ptr @fgets(ptr, i32, ptr)
declare i32 @fprintf(ptr, ptr, ...)
@.fmt_d = linkonce_odr constant [5 x i8] c"%lld\00"
@.fmt_s = linkonce_odr constant [3 x i8] c"%s\00"
@.fmt_f = linkonce_odr constant [3 x i8] c"%g\00"
@.fmt_c = linkonce_odr constant [3 x i8] c"%c\00"

define linkonce_odr void @__text_io_new_line() {
  %out = load ptr, ptr @stdout
  call i32 @fputc(i32 10, ptr %out)
  ret void
}

define linkonce_odr void @__text_io_put_char(i8 %c) {
  %out = load ptr, ptr @stdout
  %ci = zext i8 %c to i32
  call i32 @fputc(i32 %ci, ptr %out)
  ret void
}

define linkonce_odr void @__text_io_put(ptr %data, i64 %lo, i64 %hi) {
entry:
  %out = load ptr, ptr @stdout
  %i.init = sub i64 %lo, 1
  br label %loop
loop:
  %i = phi i64 [ %i.init, %entry ], [ %i.next, %body ]
  %i.next = add i64 %i, 1
  %done = icmp sgt i64 %i.next, %hi
  br i1 %done, label %exit, label %body
body:
  %idx = sub i64 %i.next, %lo
  %ptr = getelementptr i8, ptr %data, i64 %idx
  %ch = load i8, ptr %ptr
  %chi = zext i8 %ch to i32
  call i32 @fputc(i32 %chi, ptr %out)
  br label %loop
exit:
  ret void
}

define linkonce_odr void @__text_io_put_line(ptr %data, i64 %lo, i64 %hi) {
  call void @__text_io_put(ptr %data, i64 %lo, i64 %hi)
  call void @__text_io_new_line()
  ret void
}

define linkonce_odr void @__text_io_put_int(i64 %val, i32 %width) {
  %out = load ptr, ptr @stdout
  call i32 (ptr, ptr, ...) @fprintf(ptr %out, ptr @.fmt_d, i64 %val)
  ret void
}

define linkonce_odr void @__text_io_put_float(double %val) {
  %out = load ptr, ptr @stdout
  call i32 (ptr, ptr, ...) @fprintf(ptr %out, ptr @.fmt_f, double %val)
  ret void
}

define linkonce_odr i8 @__text_io_get_char() {
  %inp = load ptr, ptr @stdin
  %c = call i32 @fgetc(ptr %inp)
  %c8 = trunc i32 %c to i8
  ret i8 %c8
}

define linkonce_odr { ptr, ptr } @__text_io_get_line() {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 256)
  %inp = load ptr, ptr @stdin
  %res = call ptr @fgets(ptr %buf, i32 255, ptr %inp)
  %iseof = icmp eq ptr %res, null
  br i1 %iseof, label %empty, label %gotline
empty:
  %e_bnd = call ptr @__ada_sec_stack_alloc(i64 16)
  %e_lo_gep = getelementptr { i64, i64 }, ptr %e_bnd, i32 0, i32 0
  store i64 1, ptr %e_lo_gep
  %e_hi_gep = getelementptr { i64, i64 }, ptr %e_bnd, i32 0, i32 1
  store i64 0, ptr %e_hi_gep
  %e1 = insertvalue { ptr, ptr } undef, ptr %buf, 0
  %e2 = insertvalue { ptr, ptr } %e1, ptr %e_bnd, 1
  ret { ptr, ptr } %e2
gotline:
  %len = call i64 @strlen(ptr %buf)
  ; Strip trailing newline if present
  %lastidx = sub i64 %len, 1
  %lastptr = getelementptr i8, ptr %buf, i64 %lastidx
  %lastch = load i8, ptr %lastptr
  %isnl = icmp eq i8 %lastch, 10
  %adjlen = select i1 %isnl, i64 %lastidx, i64 %len
  %f_bnd = call ptr @__ada_sec_stack_alloc(i64 16)
  %f_lo_gep = getelementptr { i64, i64 }, ptr %f_bnd, i32 0, i32 0
  store i64 1, ptr %f_lo_gep
  %f_hi_gep = getelementptr { i64, i64 }, ptr %f_bnd, i32 0, i32 1
  store i64 %adjlen, ptr %f_hi_gep
  %f1 = insertvalue { ptr, ptr } undef, ptr %buf, 0
  %f2 = insertvalue { ptr, ptr } %f1, ptr %f_bnd, 1
  ret { ptr, ptr } %f2
}

; Attribute runtime support
declare i32 @snprintf(ptr, i64, ptr, ...)
declare i64 @strlen(ptr)
@.img_fmt_d = linkonce_odr constant [5 x i8] c"%lld\00"
@.img_fmt_c = linkonce_odr constant [3 x i8] c"%c\00"
@.img_fmt_f = linkonce_odr constant [5 x i8] c"%.6g\00"

define linkonce_odr { ptr, ptr } @__ada_integer_image(i64 %val) {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 24)
  %len32 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 24, ptr @.img_fmt_d, i64 %val)
  %len = sext i32 %len32 to i64
  %fat_bnd = call ptr @__ada_sec_stack_alloc(i64 16)
  %fat_lo_gep = getelementptr { i64, i64 }, ptr %fat_bnd, i32 0, i32 0
  store i64 1, ptr %fat_lo_gep
  %fat_hi_gep = getelementptr { i64, i64 }, ptr %fat_bnd, i32 0, i32 1
  store i64 %len, ptr %fat_hi_gep
  %fat1 = insertvalue { ptr, ptr } undef, ptr %buf, 0
  %fat2 = insertvalue { ptr, ptr } %fat1, ptr %fat_bnd, 1
  ret { ptr, ptr } %fat2
}

define linkonce_odr { ptr, ptr } @__ada_character_image(i8 %val) {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 4)
  %p0 = getelementptr i8, ptr %buf, i64 0
  store i8 39, ptr %p0  ; single quote
  %p1 = getelementptr i8, ptr %buf, i64 1
  store i8 %val, ptr %p1
  %p2 = getelementptr i8, ptr %buf, i64 2
  store i8 39, ptr %p2  ; single quote
  %fat_bnd = call ptr @__ada_sec_stack_alloc(i64 16)
  %fat_lo_gep = getelementptr { i64, i64 }, ptr %fat_bnd, i32 0, i32 0
  store i64 1, ptr %fat_lo_gep
  %fat_hi_gep = getelementptr { i64, i64 }, ptr %fat_bnd, i32 0, i32 1
  store i64 3, ptr %fat_hi_gep
  %fat1 = insertvalue { ptr, ptr } undef, ptr %buf, 0
  %fat2 = insertvalue { ptr, ptr } %fat1, ptr %fat_bnd, 1
  ret { ptr, ptr } %fat2
}

define linkonce_odr { ptr, ptr } @__ada_float_image(double %val) {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 32)
  %len32 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 32, ptr @.img_fmt_f, double %val)
  %len = sext i32 %len32 to i64
  %fat_bnd = call ptr @__ada_sec_stack_alloc(i64 16)
  %fat_lo_gep = getelementptr { i64, i64 }, ptr %fat_bnd, i32 0, i32 0
  store i64 1, ptr %fat_lo_gep
  %fat_hi_gep = getelementptr { i64, i64 }, ptr %fat_bnd, i32 0, i32 1
  store i64 %len, ptr %fat_hi_gep
  %fat1 = insertvalue { ptr, ptr } undef, ptr %buf, 0
  %fat2 = insertvalue { ptr, ptr } %fat1, ptr %fat_bnd, 1
  ret { ptr, ptr } %fat2
}


; Implicit equality for type STRING
define linkonce_odr i1 @_ada_eq_STRING_1({ ptr, ptr } %0, { ptr, ptr } %1) {
entry:
  %left_data = extractvalue { ptr, ptr } %0, 0
  %right_data = extractvalue { ptr, ptr } %1, 0
  %left_bptr = extractvalue { ptr, ptr } %0, 1
  %right_bptr = extractvalue { ptr, ptr } %1, 1
  %left_lo_gep = getelementptr { i64, i64 }, ptr %left_bptr, i32 0, i32 0
  %left_low = load i64, ptr %left_lo_gep
  %left_hi_gep = getelementptr { i64, i64 }, ptr %left_bptr, i32 0, i32 1
  %left_high = load i64, ptr %left_hi_gep
  %left_len = sub i64 %left_high, %left_low
  %left_len1 = add i64 %left_len, 1
  %right_lo_gep = getelementptr { i64, i64 }, ptr %right_bptr, i32 0, i32 0
  %right_low = load i64, ptr %right_lo_gep
  %right_hi_gep = getelementptr { i64, i64 }, ptr %right_bptr, i32 0, i32 1
  %right_high = load i64, ptr %right_hi_gep
  %right_len = sub i64 %right_high, %right_low
  %right_len1 = add i64 %right_len, 1
  %len_eq = icmp eq i64 %left_len1, %right_len1
  %byte_size = mul i64 %left_len1, 1
  %memcmp_res = call i32 @memcmp(ptr %left_data, ptr %right_data, i64 %byte_size)
  %data_eq = icmp eq i32 %memcmp_res, 0
  %result = and i1 %len_eq, %data_eq
  ret i1 %result
}

@report__f = linkonce_odr global i64 0
define void @report__test({ ptr, ptr } %p0, { ptr, ptr } %p1) {
entry:
  %name_s143 = alloca { ptr, ptr }
  store { ptr, ptr } %p0, ptr %name_s143
  %desc_s144 = alloca { ptr, ptr }
  store { ptr, ptr } %p1, ptr %desc_s144
  %t1 = add i64 0, 0
  %t2 = add i64 0, -2147483648  ; literal bound
  %t3 = add i64 0, 2147483647  ; literal bound
  %t4 = icmp slt i64 %t1, %t2
  br i1 %t4, label %L1, label %L2
L2:
  %t5 = icmp sgt i64 %t1, %t3
  br i1 %t5, label %L1, label %L3
L1:  ; raise CONSTRAINT_ERROR
  %t6 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t6)  ; bound check
  unreachable
L3:
  store i64 %t1, ptr @report__f
  %t7 = add i64 0, 84
  %t8 = add i64 0, -2147483648  ; literal bound
  %t9 = add i64 0, 2147483647  ; literal bound
  %t10 = icmp slt i64 %t7, %t8
  br i1 %t10, label %L4, label %L5
L5:
  %t11 = icmp sgt i64 %t7, %t9
  br i1 %t11, label %L4, label %L6
L4:  ; raise CONSTRAINT_ERROR
  %t12 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t12)  ; bound check
  unreachable
L6:
  call void @__text_io_put_char(i64 %t7)
  %t14 = add i64 0, 69
  %t15 = add i64 0, -2147483648  ; literal bound
  %t16 = add i64 0, 2147483647  ; literal bound
  %t17 = icmp slt i64 %t14, %t15
  br i1 %t17, label %L7, label %L8
L8:
  %t18 = icmp sgt i64 %t14, %t16
  br i1 %t18, label %L7, label %L9
L7:  ; raise CONSTRAINT_ERROR
  %t19 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t19)  ; bound check
  unreachable
L9:
  call void @__text_io_put_char(i64 %t14)
  %t21 = add i64 0, 83
  %t22 = add i64 0, -2147483648  ; literal bound
  %t23 = add i64 0, 2147483647  ; literal bound
  %t24 = icmp slt i64 %t21, %t22
  br i1 %t24, label %L10, label %L11
L11:
  %t25 = icmp sgt i64 %t21, %t23
  br i1 %t25, label %L10, label %L12
L10:  ; raise CONSTRAINT_ERROR
  %t26 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t26)  ; bound check
  unreachable
L12:
  call void @__text_io_put_char(i64 %t21)
  %t28 = add i64 0, 84
  %t29 = add i64 0, -2147483648  ; literal bound
  %t30 = add i64 0, 2147483647  ; literal bound
  %t31 = icmp slt i64 %t28, %t29
  br i1 %t31, label %L13, label %L14
L14:
  %t32 = icmp sgt i64 %t28, %t30
  br i1 %t32, label %L13, label %L15
L13:  ; raise CONSTRAINT_ERROR
  %t33 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t33)  ; bound check
  unreachable
L15:
  call void @__text_io_put_char(i64 %t28)
  %t35 = add i64 0, 32
  %t36 = add i64 0, -2147483648  ; literal bound
  %t37 = add i64 0, 2147483647  ; literal bound
  %t38 = icmp slt i64 %t35, %t36
  br i1 %t38, label %L16, label %L17
L17:
  %t39 = icmp sgt i64 %t35, %t37
  br i1 %t39, label %L16, label %L18
L16:  ; raise CONSTRAINT_ERROR
  %t40 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t40)  ; bound check
  unreachable
L18:
  call void @__text_io_put_char(i64 %t35)
  %i_s145 = alloca i64
  %t42 = load { ptr, ptr }, ptr %name_s143
  %t43 = extractvalue { ptr, ptr } %t42, 1
  %t44 = getelementptr { i64, i64 }, ptr %t43, i32 0, i32 0
  %t45 = load i64, ptr %t44
  %t46 = trunc i64 %t45 to i32
  %t47 = extractvalue { ptr, ptr } %t42, 1
  %t48 = getelementptr { i64, i64 }, ptr %t47, i32 0, i32 1
  %t49 = load i64, ptr %t48
  %t50 = trunc i64 %t49 to i32
  %t51 = sext i32 %t46 to i64
  %t52 = sext i32 %t50 to i64
  store i64 %t51, ptr %i_s145
  br label %L19
L19:
  %t53 = load i64, ptr %i_s145
  %t54 = icmp sle i64 %t53, %t52
  br i1 %t54, label %L20, label %L21
L20:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t56 = load { ptr, ptr }, ptr %name_s143
  %t57 = extractvalue { ptr, ptr } %t56, 0
  %t58 = extractvalue { ptr, ptr } %t56, 1
  %t59 = getelementptr { i64, i64 }, ptr %t58, i32 0, i32 0
  %t60 = load i64, ptr %t59
  %t61 = trunc i64 %t60 to i32
  %t62 = load i64, ptr %i_s145
  %t63 = sext i32 %t61 to i64
  %t64 = sub i64 %t62, %t63  ; adjust for dynamic low bound
  %t65 = getelementptr i8, ptr %t57, i64 %t64
  %t66 = load i8, ptr %t65
  %t67 = sext i8 %t66 to i64
  %t68 = add i64 0, -2147483648  ; literal bound
  %t69 = add i64 0, 2147483647  ; literal bound
  %t70 = icmp slt i64 %t67, %t68
  br i1 %t70, label %L22, label %L23
L23:
  %t71 = icmp sgt i64 %t67, %t69
  br i1 %t71, label %L22, label %L24
L22:  ; raise CONSTRAINT_ERROR
  %t72 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t72)  ; bound check
  unreachable
L24:
  call void @__text_io_put_char(i64 %t67)
  %t74 = add i64 %t53, 1
  store i64 %t74, ptr %i_s145
  br label %L19
L21:
  %t75 = add i64 0, 58
  %t76 = add i64 0, -2147483648  ; literal bound
  %t77 = add i64 0, 2147483647  ; literal bound
  %t78 = icmp slt i64 %t75, %t76
  br i1 %t78, label %L25, label %L26
L26:
  %t79 = icmp sgt i64 %t75, %t77
  br i1 %t79, label %L25, label %L27
L25:  ; raise CONSTRAINT_ERROR
  %t80 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t80)  ; bound check
  unreachable
L27:
  call void @__text_io_put_char(i64 %t75)
  %t82 = add i64 0, 32
  %t83 = add i64 0, -2147483648  ; literal bound
  %t84 = add i64 0, 2147483647  ; literal bound
  %t85 = icmp slt i64 %t82, %t83
  br i1 %t85, label %L28, label %L29
L29:
  %t86 = icmp sgt i64 %t82, %t84
  br i1 %t86, label %L28, label %L30
L28:  ; raise CONSTRAINT_ERROR
  %t87 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t87)  ; bound check
  unreachable
L30:
  call void @__text_io_put_char(i64 %t82)
  %i_s146 = alloca i64
  %t89 = load { ptr, ptr }, ptr %desc_s144
  %t90 = extractvalue { ptr, ptr } %t89, 1
  %t91 = getelementptr { i64, i64 }, ptr %t90, i32 0, i32 0
  %t92 = load i64, ptr %t91
  %t93 = trunc i64 %t92 to i32
  %t94 = extractvalue { ptr, ptr } %t89, 1
  %t95 = getelementptr { i64, i64 }, ptr %t94, i32 0, i32 1
  %t96 = load i64, ptr %t95
  %t97 = trunc i64 %t96 to i32
  %t98 = sext i32 %t93 to i64
  %t99 = sext i32 %t97 to i64
  store i64 %t98, ptr %i_s146
  br label %L31
L31:
  %t100 = load i64, ptr %i_s146
  %t101 = icmp sle i64 %t100, %t99
  br i1 %t101, label %L32, label %L33
L32:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t103 = load { ptr, ptr }, ptr %desc_s144
  %t104 = extractvalue { ptr, ptr } %t103, 0
  %t105 = extractvalue { ptr, ptr } %t103, 1
  %t106 = getelementptr { i64, i64 }, ptr %t105, i32 0, i32 0
  %t107 = load i64, ptr %t106
  %t108 = trunc i64 %t107 to i32
  %t109 = load i64, ptr %i_s146
  %t110 = sext i32 %t108 to i64
  %t111 = sub i64 %t109, %t110  ; adjust for dynamic low bound
  %t112 = getelementptr i8, ptr %t104, i64 %t111
  %t113 = load i8, ptr %t112
  %t114 = sext i8 %t113 to i64
  %t115 = add i64 0, -2147483648  ; literal bound
  %t116 = add i64 0, 2147483647  ; literal bound
  %t117 = icmp slt i64 %t114, %t115
  br i1 %t117, label %L34, label %L35
L35:
  %t118 = icmp sgt i64 %t114, %t116
  br i1 %t118, label %L34, label %L36
L34:  ; raise CONSTRAINT_ERROR
  %t119 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t119)  ; bound check
  unreachable
L36:
  call void @__text_io_put_char(i64 %t114)
  %t121 = add i64 %t100, 1
  store i64 %t121, ptr %i_s146
  br label %L31
L33:
  call void @__text_io_new_line()
  ret void
}

define void @report__failed({ ptr, ptr } %p0) {
entry:
  %msg_s147 = alloca { ptr, ptr }
  store { ptr, ptr } %p0, ptr %msg_s147
  %t122 = add i64 0, 70
  %t123 = add i64 0, -2147483648  ; literal bound
  %t124 = add i64 0, 2147483647  ; literal bound
  %t125 = icmp slt i64 %t122, %t123
  br i1 %t125, label %L37, label %L38
L38:
  %t126 = icmp sgt i64 %t122, %t124
  br i1 %t126, label %L37, label %L39
L37:  ; raise CONSTRAINT_ERROR
  %t127 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t127)  ; bound check
  unreachable
L39:
  call void @__text_io_put_char(i64 %t122)
  %t129 = add i64 0, 65
  %t130 = add i64 0, -2147483648  ; literal bound
  %t131 = add i64 0, 2147483647  ; literal bound
  %t132 = icmp slt i64 %t129, %t130
  br i1 %t132, label %L40, label %L41
L41:
  %t133 = icmp sgt i64 %t129, %t131
  br i1 %t133, label %L40, label %L42
L40:  ; raise CONSTRAINT_ERROR
  %t134 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t134)  ; bound check
  unreachable
L42:
  call void @__text_io_put_char(i64 %t129)
  %t136 = add i64 0, 73
  %t137 = add i64 0, -2147483648  ; literal bound
  %t138 = add i64 0, 2147483647  ; literal bound
  %t139 = icmp slt i64 %t136, %t137
  br i1 %t139, label %L43, label %L44
L44:
  %t140 = icmp sgt i64 %t136, %t138
  br i1 %t140, label %L43, label %L45
L43:  ; raise CONSTRAINT_ERROR
  %t141 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t141)  ; bound check
  unreachable
L45:
  call void @__text_io_put_char(i64 %t136)
  %t143 = add i64 0, 76
  %t144 = add i64 0, -2147483648  ; literal bound
  %t145 = add i64 0, 2147483647  ; literal bound
  %t146 = icmp slt i64 %t143, %t144
  br i1 %t146, label %L46, label %L47
L47:
  %t147 = icmp sgt i64 %t143, %t145
  br i1 %t147, label %L46, label %L48
L46:  ; raise CONSTRAINT_ERROR
  %t148 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t148)  ; bound check
  unreachable
L48:
  call void @__text_io_put_char(i64 %t143)
  %t150 = add i64 0, 69
  %t151 = add i64 0, -2147483648  ; literal bound
  %t152 = add i64 0, 2147483647  ; literal bound
  %t153 = icmp slt i64 %t150, %t151
  br i1 %t153, label %L49, label %L50
L50:
  %t154 = icmp sgt i64 %t150, %t152
  br i1 %t154, label %L49, label %L51
L49:  ; raise CONSTRAINT_ERROR
  %t155 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t155)  ; bound check
  unreachable
L51:
  call void @__text_io_put_char(i64 %t150)
  %t157 = add i64 0, 68
  %t158 = add i64 0, -2147483648  ; literal bound
  %t159 = add i64 0, 2147483647  ; literal bound
  %t160 = icmp slt i64 %t157, %t158
  br i1 %t160, label %L52, label %L53
L53:
  %t161 = icmp sgt i64 %t157, %t159
  br i1 %t161, label %L52, label %L54
L52:  ; raise CONSTRAINT_ERROR
  %t162 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t162)  ; bound check
  unreachable
L54:
  call void @__text_io_put_char(i64 %t157)
  %t164 = add i64 0, 58
  %t165 = add i64 0, -2147483648  ; literal bound
  %t166 = add i64 0, 2147483647  ; literal bound
  %t167 = icmp slt i64 %t164, %t165
  br i1 %t167, label %L55, label %L56
L56:
  %t168 = icmp sgt i64 %t164, %t166
  br i1 %t168, label %L55, label %L57
L55:  ; raise CONSTRAINT_ERROR
  %t169 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t169)  ; bound check
  unreachable
L57:
  call void @__text_io_put_char(i64 %t164)
  %t171 = add i64 0, 32
  %t172 = add i64 0, -2147483648  ; literal bound
  %t173 = add i64 0, 2147483647  ; literal bound
  %t174 = icmp slt i64 %t171, %t172
  br i1 %t174, label %L58, label %L59
L59:
  %t175 = icmp sgt i64 %t171, %t173
  br i1 %t175, label %L58, label %L60
L58:  ; raise CONSTRAINT_ERROR
  %t176 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t176)  ; bound check
  unreachable
L60:
  call void @__text_io_put_char(i64 %t171)
  %i_s148 = alloca i64
  %t178 = load { ptr, ptr }, ptr %msg_s147
  %t179 = extractvalue { ptr, ptr } %t178, 1
  %t180 = getelementptr { i64, i64 }, ptr %t179, i32 0, i32 0
  %t181 = load i64, ptr %t180
  %t182 = trunc i64 %t181 to i32
  %t183 = extractvalue { ptr, ptr } %t178, 1
  %t184 = getelementptr { i64, i64 }, ptr %t183, i32 0, i32 1
  %t185 = load i64, ptr %t184
  %t186 = trunc i64 %t185 to i32
  %t187 = sext i32 %t182 to i64
  %t188 = sext i32 %t186 to i64
  store i64 %t187, ptr %i_s148
  br label %L61
L61:
  %t189 = load i64, ptr %i_s148
  %t190 = icmp sle i64 %t189, %t188
  br i1 %t190, label %L62, label %L63
L62:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t192 = load { ptr, ptr }, ptr %msg_s147
  %t193 = extractvalue { ptr, ptr } %t192, 0
  %t194 = extractvalue { ptr, ptr } %t192, 1
  %t195 = getelementptr { i64, i64 }, ptr %t194, i32 0, i32 0
  %t196 = load i64, ptr %t195
  %t197 = trunc i64 %t196 to i32
  %t198 = load i64, ptr %i_s148
  %t199 = sext i32 %t197 to i64
  %t200 = sub i64 %t198, %t199  ; adjust for dynamic low bound
  %t201 = getelementptr i8, ptr %t193, i64 %t200
  %t202 = load i8, ptr %t201
  %t203 = sext i8 %t202 to i64
  %t204 = add i64 0, -2147483648  ; literal bound
  %t205 = add i64 0, 2147483647  ; literal bound
  %t206 = icmp slt i64 %t203, %t204
  br i1 %t206, label %L64, label %L65
L65:
  %t207 = icmp sgt i64 %t203, %t205
  br i1 %t207, label %L64, label %L66
L64:  ; raise CONSTRAINT_ERROR
  %t208 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t208)  ; bound check
  unreachable
L66:
  call void @__text_io_put_char(i64 %t203)
  %t210 = add i64 %t189, 1
  store i64 %t210, ptr %i_s148
  br label %L61
L63:
  call void @__text_io_new_line()
  %t211 = add i64 0, 1
  %t212 = add i64 0, -2147483648  ; literal bound
  %t213 = add i64 0, 2147483647  ; literal bound
  %t214 = icmp slt i64 %t211, %t212
  br i1 %t214, label %L67, label %L68
L68:
  %t215 = icmp sgt i64 %t211, %t213
  br i1 %t215, label %L67, label %L69
L67:  ; raise CONSTRAINT_ERROR
  %t216 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t216)  ; bound check
  unreachable
L69:
  store i64 %t211, ptr @report__f
  ret void
}

define void @report__result() {
entry:
  %t217 = load i64, ptr @report__f
  %t218 = add i64 0, 0
  %t219 = icmp ne i64 %t217, %t218
  %t220 = zext i1 %t219 to i64
  %t221 = icmp ne i64 %t220, 0
  br i1 %t221, label %L70, label %L71
L70:
  %t222 = add i64 0, 70
  %t223 = add i64 0, -2147483648  ; literal bound
  %t224 = add i64 0, 2147483647  ; literal bound
  %t225 = icmp slt i64 %t222, %t223
  br i1 %t225, label %L73, label %L74
L74:
  %t226 = icmp sgt i64 %t222, %t224
  br i1 %t226, label %L73, label %L75
L73:  ; raise CONSTRAINT_ERROR
  %t227 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t227)  ; bound check
  unreachable
L75:
  call void @__text_io_put_char(i64 %t222)
  %t229 = add i64 0, 65
  %t230 = add i64 0, -2147483648  ; literal bound
  %t231 = add i64 0, 2147483647  ; literal bound
  %t232 = icmp slt i64 %t229, %t230
  br i1 %t232, label %L76, label %L77
L77:
  %t233 = icmp sgt i64 %t229, %t231
  br i1 %t233, label %L76, label %L78
L76:  ; raise CONSTRAINT_ERROR
  %t234 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t234)  ; bound check
  unreachable
L78:
  call void @__text_io_put_char(i64 %t229)
  %t236 = add i64 0, 73
  %t237 = add i64 0, -2147483648  ; literal bound
  %t238 = add i64 0, 2147483647  ; literal bound
  %t239 = icmp slt i64 %t236, %t237
  br i1 %t239, label %L79, label %L80
L80:
  %t240 = icmp sgt i64 %t236, %t238
  br i1 %t240, label %L79, label %L81
L79:  ; raise CONSTRAINT_ERROR
  %t241 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t241)  ; bound check
  unreachable
L81:
  call void @__text_io_put_char(i64 %t236)
  %t243 = add i64 0, 76
  %t244 = add i64 0, -2147483648  ; literal bound
  %t245 = add i64 0, 2147483647  ; literal bound
  %t246 = icmp slt i64 %t243, %t244
  br i1 %t246, label %L82, label %L83
L83:
  %t247 = icmp sgt i64 %t243, %t245
  br i1 %t247, label %L82, label %L84
L82:  ; raise CONSTRAINT_ERROR
  %t248 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t248)  ; bound check
  unreachable
L84:
  call void @__text_io_put_char(i64 %t243)
  %t250 = add i64 0, 69
  %t251 = add i64 0, -2147483648  ; literal bound
  %t252 = add i64 0, 2147483647  ; literal bound
  %t253 = icmp slt i64 %t250, %t251
  br i1 %t253, label %L85, label %L86
L86:
  %t254 = icmp sgt i64 %t250, %t252
  br i1 %t254, label %L85, label %L87
L85:  ; raise CONSTRAINT_ERROR
  %t255 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t255)  ; bound check
  unreachable
L87:
  call void @__text_io_put_char(i64 %t250)
  %t257 = add i64 0, 68
  %t258 = add i64 0, -2147483648  ; literal bound
  %t259 = add i64 0, 2147483647  ; literal bound
  %t260 = icmp slt i64 %t257, %t258
  br i1 %t260, label %L88, label %L89
L89:
  %t261 = icmp sgt i64 %t257, %t259
  br i1 %t261, label %L88, label %L90
L88:  ; raise CONSTRAINT_ERROR
  %t262 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t262)  ; bound check
  unreachable
L90:
  call void @__text_io_put_char(i64 %t257)
  br label %L72
L71:
  %t264 = add i64 0, 80
  %t265 = add i64 0, -2147483648  ; literal bound
  %t266 = add i64 0, 2147483647  ; literal bound
  %t267 = icmp slt i64 %t264, %t265
  br i1 %t267, label %L91, label %L92
L92:
  %t268 = icmp sgt i64 %t264, %t266
  br i1 %t268, label %L91, label %L93
L91:  ; raise CONSTRAINT_ERROR
  %t269 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t269)  ; bound check
  unreachable
L93:
  call void @__text_io_put_char(i64 %t264)
  %t271 = add i64 0, 65
  %t272 = add i64 0, -2147483648  ; literal bound
  %t273 = add i64 0, 2147483647  ; literal bound
  %t274 = icmp slt i64 %t271, %t272
  br i1 %t274, label %L94, label %L95
L95:
  %t275 = icmp sgt i64 %t271, %t273
  br i1 %t275, label %L94, label %L96
L94:  ; raise CONSTRAINT_ERROR
  %t276 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t276)  ; bound check
  unreachable
L96:
  call void @__text_io_put_char(i64 %t271)
  %t278 = add i64 0, 83
  %t279 = add i64 0, -2147483648  ; literal bound
  %t280 = add i64 0, 2147483647  ; literal bound
  %t281 = icmp slt i64 %t278, %t279
  br i1 %t281, label %L97, label %L98
L98:
  %t282 = icmp sgt i64 %t278, %t280
  br i1 %t282, label %L97, label %L99
L97:  ; raise CONSTRAINT_ERROR
  %t283 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t283)  ; bound check
  unreachable
L99:
  call void @__text_io_put_char(i64 %t278)
  %t285 = add i64 0, 83
  %t286 = add i64 0, -2147483648  ; literal bound
  %t287 = add i64 0, 2147483647  ; literal bound
  %t288 = icmp slt i64 %t285, %t286
  br i1 %t288, label %L100, label %L101
L101:
  %t289 = icmp sgt i64 %t285, %t287
  br i1 %t289, label %L100, label %L102
L100:  ; raise CONSTRAINT_ERROR
  %t290 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t290)  ; bound check
  unreachable
L102:
  call void @__text_io_put_char(i64 %t285)
  %t292 = add i64 0, 69
  %t293 = add i64 0, -2147483648  ; literal bound
  %t294 = add i64 0, 2147483647  ; literal bound
  %t295 = icmp slt i64 %t292, %t293
  br i1 %t295, label %L103, label %L104
L104:
  %t296 = icmp sgt i64 %t292, %t294
  br i1 %t296, label %L103, label %L105
L103:  ; raise CONSTRAINT_ERROR
  %t297 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t297)  ; bound check
  unreachable
L105:
  call void @__text_io_put_char(i64 %t292)
  %t299 = add i64 0, 68
  %t300 = add i64 0, -2147483648  ; literal bound
  %t301 = add i64 0, 2147483647  ; literal bound
  %t302 = icmp slt i64 %t299, %t300
  br i1 %t302, label %L106, label %L107
L107:
  %t303 = icmp sgt i64 %t299, %t301
  br i1 %t303, label %L106, label %L108
L106:  ; raise CONSTRAINT_ERROR
  %t304 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t304)  ; bound check
  unreachable
L108:
  call void @__text_io_put_char(i64 %t299)
  br label %L72
L72:
  call void @__text_io_new_line()
  ret void
}

define void @report__comment({ ptr, ptr } %p0) {
entry:
  %msg_s149 = alloca { ptr, ptr }
  store { ptr, ptr } %p0, ptr %msg_s149
  %t306 = add i64 0, 67
  %t307 = add i64 0, -2147483648  ; literal bound
  %t308 = add i64 0, 2147483647  ; literal bound
  %t309 = icmp slt i64 %t306, %t307
  br i1 %t309, label %L109, label %L110
L110:
  %t310 = icmp sgt i64 %t306, %t308
  br i1 %t310, label %L109, label %L111
L109:  ; raise CONSTRAINT_ERROR
  %t311 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t311)  ; bound check
  unreachable
L111:
  call void @__text_io_put_char(i64 %t306)
  %t313 = add i64 0, 79
  %t314 = add i64 0, -2147483648  ; literal bound
  %t315 = add i64 0, 2147483647  ; literal bound
  %t316 = icmp slt i64 %t313, %t314
  br i1 %t316, label %L112, label %L113
L113:
  %t317 = icmp sgt i64 %t313, %t315
  br i1 %t317, label %L112, label %L114
L112:  ; raise CONSTRAINT_ERROR
  %t318 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t318)  ; bound check
  unreachable
L114:
  call void @__text_io_put_char(i64 %t313)
  %t320 = add i64 0, 77
  %t321 = add i64 0, -2147483648  ; literal bound
  %t322 = add i64 0, 2147483647  ; literal bound
  %t323 = icmp slt i64 %t320, %t321
  br i1 %t323, label %L115, label %L116
L116:
  %t324 = icmp sgt i64 %t320, %t322
  br i1 %t324, label %L115, label %L117
L115:  ; raise CONSTRAINT_ERROR
  %t325 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t325)  ; bound check
  unreachable
L117:
  call void @__text_io_put_char(i64 %t320)
  %t327 = add i64 0, 77
  %t328 = add i64 0, -2147483648  ; literal bound
  %t329 = add i64 0, 2147483647  ; literal bound
  %t330 = icmp slt i64 %t327, %t328
  br i1 %t330, label %L118, label %L119
L119:
  %t331 = icmp sgt i64 %t327, %t329
  br i1 %t331, label %L118, label %L120
L118:  ; raise CONSTRAINT_ERROR
  %t332 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t332)  ; bound check
  unreachable
L120:
  call void @__text_io_put_char(i64 %t327)
  %t334 = add i64 0, 69
  %t335 = add i64 0, -2147483648  ; literal bound
  %t336 = add i64 0, 2147483647  ; literal bound
  %t337 = icmp slt i64 %t334, %t335
  br i1 %t337, label %L121, label %L122
L122:
  %t338 = icmp sgt i64 %t334, %t336
  br i1 %t338, label %L121, label %L123
L121:  ; raise CONSTRAINT_ERROR
  %t339 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t339)  ; bound check
  unreachable
L123:
  call void @__text_io_put_char(i64 %t334)
  %t341 = add i64 0, 78
  %t342 = add i64 0, -2147483648  ; literal bound
  %t343 = add i64 0, 2147483647  ; literal bound
  %t344 = icmp slt i64 %t341, %t342
  br i1 %t344, label %L124, label %L125
L125:
  %t345 = icmp sgt i64 %t341, %t343
  br i1 %t345, label %L124, label %L126
L124:  ; raise CONSTRAINT_ERROR
  %t346 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t346)  ; bound check
  unreachable
L126:
  call void @__text_io_put_char(i64 %t341)
  %t348 = add i64 0, 84
  %t349 = add i64 0, -2147483648  ; literal bound
  %t350 = add i64 0, 2147483647  ; literal bound
  %t351 = icmp slt i64 %t348, %t349
  br i1 %t351, label %L127, label %L128
L128:
  %t352 = icmp sgt i64 %t348, %t350
  br i1 %t352, label %L127, label %L129
L127:  ; raise CONSTRAINT_ERROR
  %t353 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t353)  ; bound check
  unreachable
L129:
  call void @__text_io_put_char(i64 %t348)
  %t355 = add i64 0, 58
  %t356 = add i64 0, -2147483648  ; literal bound
  %t357 = add i64 0, 2147483647  ; literal bound
  %t358 = icmp slt i64 %t355, %t356
  br i1 %t358, label %L130, label %L131
L131:
  %t359 = icmp sgt i64 %t355, %t357
  br i1 %t359, label %L130, label %L132
L130:  ; raise CONSTRAINT_ERROR
  %t360 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t360)  ; bound check
  unreachable
L132:
  call void @__text_io_put_char(i64 %t355)
  %t362 = add i64 0, 32
  %t363 = add i64 0, -2147483648  ; literal bound
  %t364 = add i64 0, 2147483647  ; literal bound
  %t365 = icmp slt i64 %t362, %t363
  br i1 %t365, label %L133, label %L134
L134:
  %t366 = icmp sgt i64 %t362, %t364
  br i1 %t366, label %L133, label %L135
L133:  ; raise CONSTRAINT_ERROR
  %t367 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t367)  ; bound check
  unreachable
L135:
  call void @__text_io_put_char(i64 %t362)
  %i_s150 = alloca i64
  %t369 = load { ptr, ptr }, ptr %msg_s149
  %t370 = extractvalue { ptr, ptr } %t369, 1
  %t371 = getelementptr { i64, i64 }, ptr %t370, i32 0, i32 0
  %t372 = load i64, ptr %t371
  %t373 = trunc i64 %t372 to i32
  %t374 = extractvalue { ptr, ptr } %t369, 1
  %t375 = getelementptr { i64, i64 }, ptr %t374, i32 0, i32 1
  %t376 = load i64, ptr %t375
  %t377 = trunc i64 %t376 to i32
  %t378 = sext i32 %t373 to i64
  %t379 = sext i32 %t377 to i64
  store i64 %t378, ptr %i_s150
  br label %L136
L136:
  %t380 = load i64, ptr %i_s150
  %t381 = icmp sle i64 %t380, %t379
  br i1 %t381, label %L137, label %L138
L137:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t383 = load { ptr, ptr }, ptr %msg_s149
  %t384 = extractvalue { ptr, ptr } %t383, 0
  %t385 = extractvalue { ptr, ptr } %t383, 1
  %t386 = getelementptr { i64, i64 }, ptr %t385, i32 0, i32 0
  %t387 = load i64, ptr %t386
  %t388 = trunc i64 %t387 to i32
  %t389 = load i64, ptr %i_s150
  %t390 = sext i32 %t388 to i64
  %t391 = sub i64 %t389, %t390  ; adjust for dynamic low bound
  %t392 = getelementptr i8, ptr %t384, i64 %t391
  %t393 = load i8, ptr %t392
  %t394 = sext i8 %t393 to i64
  %t395 = add i64 0, -2147483648  ; literal bound
  %t396 = add i64 0, 2147483647  ; literal bound
  %t397 = icmp slt i64 %t394, %t395
  br i1 %t397, label %L139, label %L140
L140:
  %t398 = icmp sgt i64 %t394, %t396
  br i1 %t398, label %L139, label %L141
L139:  ; raise CONSTRAINT_ERROR
  %t399 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t399)  ; bound check
  unreachable
L141:
  call void @__text_io_put_char(i64 %t394)
  %t401 = add i64 %t380, 1
  store i64 %t401, ptr %i_s150
  br label %L136
L138:
  call void @__text_io_new_line()
  ret void
}

define i64 @report__ident_int(i64 %p0) {
entry:
  %x_s151 = alloca i64
  store i64 %p0, ptr %x_s151
  %t402 = load i64, ptr %x_s151
  ret i64 %t402
}

define i1 @report__ident_bool(i1 %p0) {
entry:
  %x_s152 = alloca i1
  store i1 %p0, ptr %x_s152
  %t403 = load i1, ptr %x_s152
  %t404 = sext i1 %t403 to i64
  %t405 = icmp ne i64 %t404, 0
  ret i1 %t405
}

define i8 @report__ident_char(i8 %p0) {
entry:
  %x_s153 = alloca i8
  store i8 %p0, ptr %x_s153
  %t406 = load i8, ptr %x_s153
  %t407 = sext i8 %t406 to i64
  %t408 = trunc i64 %t407 to i8
  ret i8 %t408
}

define { ptr, ptr } @report__ident_str({ ptr, ptr } %p0) {
entry:
  %x_s154 = alloca { ptr, ptr }
  store { ptr, ptr } %p0, ptr %x_s154
  %t409 = load { ptr, ptr }, ptr %x_s154
  ret { ptr, ptr } %t409
}

define i1 @report__equal(i64 %p0, i64 %p1) {
entry:
  %x_s155 = alloca i64
  store i64 %p0, ptr %x_s155
  %y_s156 = alloca i64
  store i64 %p1, ptr %y_s156
  %t410 = load i64, ptr %x_s155
  %t411 = load i64, ptr %y_s156
  %t412 = icmp eq i64 %t410, %t411
  %t413 = zext i1 %t412 to i64
  %t414 = icmp ne i64 %t413, 0
  ret i1 %t414
}

define void @report__not_applicable({ ptr, ptr } %p0) {
entry:
  %descr_s157 = alloca { ptr, ptr }
  store { ptr, ptr } %p0, ptr %descr_s157
  %t415 = add i64 0, 78
  %t416 = add i64 0, -2147483648  ; literal bound
  %t417 = add i64 0, 2147483647  ; literal bound
  %t418 = icmp slt i64 %t415, %t416
  br i1 %t418, label %L142, label %L143
L143:
  %t419 = icmp sgt i64 %t415, %t417
  br i1 %t419, label %L142, label %L144
L142:  ; raise CONSTRAINT_ERROR
  %t420 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t420)  ; bound check
  unreachable
L144:
  call void @__text_io_put_char(i64 %t415)
  %t422 = add i64 0, 79
  %t423 = add i64 0, -2147483648  ; literal bound
  %t424 = add i64 0, 2147483647  ; literal bound
  %t425 = icmp slt i64 %t422, %t423
  br i1 %t425, label %L145, label %L146
L146:
  %t426 = icmp sgt i64 %t422, %t424
  br i1 %t426, label %L145, label %L147
L145:  ; raise CONSTRAINT_ERROR
  %t427 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t427)  ; bound check
  unreachable
L147:
  call void @__text_io_put_char(i64 %t422)
  %t429 = add i64 0, 84
  %t430 = add i64 0, -2147483648  ; literal bound
  %t431 = add i64 0, 2147483647  ; literal bound
  %t432 = icmp slt i64 %t429, %t430
  br i1 %t432, label %L148, label %L149
L149:
  %t433 = icmp sgt i64 %t429, %t431
  br i1 %t433, label %L148, label %L150
L148:  ; raise CONSTRAINT_ERROR
  %t434 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t434)  ; bound check
  unreachable
L150:
  call void @__text_io_put_char(i64 %t429)
  %t436 = add i64 0, 32
  %t437 = add i64 0, -2147483648  ; literal bound
  %t438 = add i64 0, 2147483647  ; literal bound
  %t439 = icmp slt i64 %t436, %t437
  br i1 %t439, label %L151, label %L152
L152:
  %t440 = icmp sgt i64 %t436, %t438
  br i1 %t440, label %L151, label %L153
L151:  ; raise CONSTRAINT_ERROR
  %t441 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t441)  ; bound check
  unreachable
L153:
  call void @__text_io_put_char(i64 %t436)
  %t443 = add i64 0, 65
  %t444 = add i64 0, -2147483648  ; literal bound
  %t445 = add i64 0, 2147483647  ; literal bound
  %t446 = icmp slt i64 %t443, %t444
  br i1 %t446, label %L154, label %L155
L155:
  %t447 = icmp sgt i64 %t443, %t445
  br i1 %t447, label %L154, label %L156
L154:  ; raise CONSTRAINT_ERROR
  %t448 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t448)  ; bound check
  unreachable
L156:
  call void @__text_io_put_char(i64 %t443)
  %t450 = add i64 0, 80
  %t451 = add i64 0, -2147483648  ; literal bound
  %t452 = add i64 0, 2147483647  ; literal bound
  %t453 = icmp slt i64 %t450, %t451
  br i1 %t453, label %L157, label %L158
L158:
  %t454 = icmp sgt i64 %t450, %t452
  br i1 %t454, label %L157, label %L159
L157:  ; raise CONSTRAINT_ERROR
  %t455 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t455)  ; bound check
  unreachable
L159:
  call void @__text_io_put_char(i64 %t450)
  %t457 = add i64 0, 80
  %t458 = add i64 0, -2147483648  ; literal bound
  %t459 = add i64 0, 2147483647  ; literal bound
  %t460 = icmp slt i64 %t457, %t458
  br i1 %t460, label %L160, label %L161
L161:
  %t461 = icmp sgt i64 %t457, %t459
  br i1 %t461, label %L160, label %L162
L160:  ; raise CONSTRAINT_ERROR
  %t462 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t462)  ; bound check
  unreachable
L162:
  call void @__text_io_put_char(i64 %t457)
  %t464 = add i64 0, 76
  %t465 = add i64 0, -2147483648  ; literal bound
  %t466 = add i64 0, 2147483647  ; literal bound
  %t467 = icmp slt i64 %t464, %t465
  br i1 %t467, label %L163, label %L164
L164:
  %t468 = icmp sgt i64 %t464, %t466
  br i1 %t468, label %L163, label %L165
L163:  ; raise CONSTRAINT_ERROR
  %t469 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t469)  ; bound check
  unreachable
L165:
  call void @__text_io_put_char(i64 %t464)
  %t471 = add i64 0, 73
  %t472 = add i64 0, -2147483648  ; literal bound
  %t473 = add i64 0, 2147483647  ; literal bound
  %t474 = icmp slt i64 %t471, %t472
  br i1 %t474, label %L166, label %L167
L167:
  %t475 = icmp sgt i64 %t471, %t473
  br i1 %t475, label %L166, label %L168
L166:  ; raise CONSTRAINT_ERROR
  %t476 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t476)  ; bound check
  unreachable
L168:
  call void @__text_io_put_char(i64 %t471)
  %t478 = add i64 0, 67
  %t479 = add i64 0, -2147483648  ; literal bound
  %t480 = add i64 0, 2147483647  ; literal bound
  %t481 = icmp slt i64 %t478, %t479
  br i1 %t481, label %L169, label %L170
L170:
  %t482 = icmp sgt i64 %t478, %t480
  br i1 %t482, label %L169, label %L171
L169:  ; raise CONSTRAINT_ERROR
  %t483 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t483)  ; bound check
  unreachable
L171:
  call void @__text_io_put_char(i64 %t478)
  %t485 = add i64 0, 65
  %t486 = add i64 0, -2147483648  ; literal bound
  %t487 = add i64 0, 2147483647  ; literal bound
  %t488 = icmp slt i64 %t485, %t486
  br i1 %t488, label %L172, label %L173
L173:
  %t489 = icmp sgt i64 %t485, %t487
  br i1 %t489, label %L172, label %L174
L172:  ; raise CONSTRAINT_ERROR
  %t490 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t490)  ; bound check
  unreachable
L174:
  call void @__text_io_put_char(i64 %t485)
  %t492 = add i64 0, 66
  %t493 = add i64 0, -2147483648  ; literal bound
  %t494 = add i64 0, 2147483647  ; literal bound
  %t495 = icmp slt i64 %t492, %t493
  br i1 %t495, label %L175, label %L176
L176:
  %t496 = icmp sgt i64 %t492, %t494
  br i1 %t496, label %L175, label %L177
L175:  ; raise CONSTRAINT_ERROR
  %t497 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t497)  ; bound check
  unreachable
L177:
  call void @__text_io_put_char(i64 %t492)
  %t499 = add i64 0, 76
  %t500 = add i64 0, -2147483648  ; literal bound
  %t501 = add i64 0, 2147483647  ; literal bound
  %t502 = icmp slt i64 %t499, %t500
  br i1 %t502, label %L178, label %L179
L179:
  %t503 = icmp sgt i64 %t499, %t501
  br i1 %t503, label %L178, label %L180
L178:  ; raise CONSTRAINT_ERROR
  %t504 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t504)  ; bound check
  unreachable
L180:
  call void @__text_io_put_char(i64 %t499)
  %t506 = add i64 0, 69
  %t507 = add i64 0, -2147483648  ; literal bound
  %t508 = add i64 0, 2147483647  ; literal bound
  %t509 = icmp slt i64 %t506, %t507
  br i1 %t509, label %L181, label %L182
L182:
  %t510 = icmp sgt i64 %t506, %t508
  br i1 %t510, label %L181, label %L183
L181:  ; raise CONSTRAINT_ERROR
  %t511 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t511)  ; bound check
  unreachable
L183:
  call void @__text_io_put_char(i64 %t506)
  %t513 = add i64 0, 58
  %t514 = add i64 0, -2147483648  ; literal bound
  %t515 = add i64 0, 2147483647  ; literal bound
  %t516 = icmp slt i64 %t513, %t514
  br i1 %t516, label %L184, label %L185
L185:
  %t517 = icmp sgt i64 %t513, %t515
  br i1 %t517, label %L184, label %L186
L184:  ; raise CONSTRAINT_ERROR
  %t518 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t518)  ; bound check
  unreachable
L186:
  call void @__text_io_put_char(i64 %t513)
  %t520 = add i64 0, 32
  %t521 = add i64 0, -2147483648  ; literal bound
  %t522 = add i64 0, 2147483647  ; literal bound
  %t523 = icmp slt i64 %t520, %t521
  br i1 %t523, label %L187, label %L188
L188:
  %t524 = icmp sgt i64 %t520, %t522
  br i1 %t524, label %L187, label %L189
L187:  ; raise CONSTRAINT_ERROR
  %t525 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t525)  ; bound check
  unreachable
L189:
  call void @__text_io_put_char(i64 %t520)
  %i_s158 = alloca i64
  %t527 = load { ptr, ptr }, ptr %descr_s157
  %t528 = extractvalue { ptr, ptr } %t527, 1
  %t529 = getelementptr { i64, i64 }, ptr %t528, i32 0, i32 0
  %t530 = load i64, ptr %t529
  %t531 = trunc i64 %t530 to i32
  %t532 = extractvalue { ptr, ptr } %t527, 1
  %t533 = getelementptr { i64, i64 }, ptr %t532, i32 0, i32 1
  %t534 = load i64, ptr %t533
  %t535 = trunc i64 %t534 to i32
  %t536 = sext i32 %t531 to i64
  %t537 = sext i32 %t535 to i64
  store i64 %t536, ptr %i_s158
  br label %L190
L190:
  %t538 = load i64, ptr %i_s158
  %t539 = icmp sle i64 %t538, %t537
  br i1 %t539, label %L191, label %L192
L191:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t541 = load { ptr, ptr }, ptr %descr_s157
  %t542 = extractvalue { ptr, ptr } %t541, 0
  %t543 = extractvalue { ptr, ptr } %t541, 1
  %t544 = getelementptr { i64, i64 }, ptr %t543, i32 0, i32 0
  %t545 = load i64, ptr %t544
  %t546 = trunc i64 %t545 to i32
  %t547 = load i64, ptr %i_s158
  %t548 = sext i32 %t546 to i64
  %t549 = sub i64 %t547, %t548  ; adjust for dynamic low bound
  %t550 = getelementptr i8, ptr %t542, i64 %t549
  %t551 = load i8, ptr %t550
  %t552 = sext i8 %t551 to i64
  %t553 = add i64 0, -2147483648  ; literal bound
  %t554 = add i64 0, 2147483647  ; literal bound
  %t555 = icmp slt i64 %t552, %t553
  br i1 %t555, label %L193, label %L194
L194:
  %t556 = icmp sgt i64 %t552, %t554
  br i1 %t556, label %L193, label %L195
L193:  ; raise CONSTRAINT_ERROR
  %t557 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t557)  ; bound check
  unreachable
L195:
  call void @__text_io_put_char(i64 %t552)
  %t559 = add i64 %t538, 1
  store i64 %t559, ptr %i_s158
  br label %L190
L192:
  call void @__text_io_new_line()
  ret void
}

