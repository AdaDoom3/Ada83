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
define linkonce_odr i64 @__ada_integer_value({ ptr, { i64, i64 } } %str) {
entry:
  %data = extractvalue { ptr, { i64, i64 } } %str, 0
  %low = extractvalue { ptr, { i64, i64 } } %str, 1, 0
  %high = extractvalue { ptr, { i64, i64 } } %str, 1, 1
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
define linkonce_odr double @__ada_float_value({ ptr, { i64, i64 } } %str) {
entry:
  %data = extractvalue { ptr, { i64, i64 } } %str, 0
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

define linkonce_odr { ptr, { i64, i64 } } @__text_io_get_line() {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 256)
  %inp = load ptr, ptr @stdin
  %res = call ptr @fgets(ptr %buf, i32 255, ptr %inp)
  %iseof = icmp eq ptr %res, null
  br i1 %iseof, label %empty, label %gotline
empty:
  %e1 = insertvalue { ptr, { i64, i64 } } undef, ptr %buf, 0
  %e2 = insertvalue { ptr, { i64, i64 } } %e1, i64 1, 1, 0
  %e3 = insertvalue { ptr, { i64, i64 } } %e2, i64 0, 1, 1
  ret { ptr, { i64, i64 } } %e3
gotline:
  %len = call i64 @strlen(ptr %buf)
  ; Strip trailing newline if present
  %lastidx = sub i64 %len, 1
  %lastptr = getelementptr i8, ptr %buf, i64 %lastidx
  %lastch = load i8, ptr %lastptr
  %isnl = icmp eq i8 %lastch, 10
  %adjlen = select i1 %isnl, i64 %lastidx, i64 %len
  %f1 = insertvalue { ptr, { i64, i64 } } undef, ptr %buf, 0
  %f2 = insertvalue { ptr, { i64, i64 } } %f1, i64 1, 1, 0
  %f3 = insertvalue { ptr, { i64, i64 } } %f2, i64 %adjlen, 1, 1
  ret { ptr, { i64, i64 } } %f3
}

; Attribute runtime support
declare i32 @snprintf(ptr, i64, ptr, ...)
declare i64 @strlen(ptr)
@.img_fmt_d = linkonce_odr constant [5 x i8] c"%lld\00"
@.img_fmt_c = linkonce_odr constant [3 x i8] c"%c\00"
@.img_fmt_f = linkonce_odr constant [5 x i8] c"%.6g\00"

define linkonce_odr { ptr, { i64, i64 } } @__ada_integer_image(i64 %val) {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 24)
  %len = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 24, ptr @.img_fmt_d, i64 %val)
  %len64 = sext i32 %len to i64
  %high = sub i64 %len64, 1
  %fat1 = insertvalue { ptr, { i64, i64 } } undef, ptr %buf, 0
  %fat2 = insertvalue { ptr, { i64, i64 } } %fat1, i64 1, 1, 0
  %fat3 = insertvalue { ptr, { i64, i64 } } %fat2, i64 %len64, 1, 1
  ret { ptr, { i64, i64 } } %fat3
}

define linkonce_odr { ptr, { i64, i64 } } @__ada_character_image(i8 %val) {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 4)
  %p0 = getelementptr i8, ptr %buf, i64 0
  store i8 39, ptr %p0  ; single quote
  %p1 = getelementptr i8, ptr %buf, i64 1
  store i8 %val, ptr %p1
  %p2 = getelementptr i8, ptr %buf, i64 2
  store i8 39, ptr %p2  ; single quote
  %fat1 = insertvalue { ptr, { i64, i64 } } undef, ptr %buf, 0
  %fat2 = insertvalue { ptr, { i64, i64 } } %fat1, i64 1, 1, 0
  %fat3 = insertvalue { ptr, { i64, i64 } } %fat2, i64 3, 1, 1
  ret { ptr, { i64, i64 } } %fat3
}

define linkonce_odr { ptr, { i64, i64 } } @__ada_float_image(double %val) {
entry:
  %buf = call ptr @__ada_sec_stack_alloc(i64 32)
  %len = call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 32, ptr @.img_fmt_f, double %val)
  %len64 = sext i32 %len to i64
  %fat1 = insertvalue { ptr, { i64, i64 } } undef, ptr %buf, 0
  %fat2 = insertvalue { ptr, { i64, i64 } } %fat1, i64 1, 1, 0
  %fat3 = insertvalue { ptr, { i64, i64 } } %fat2, i64 %len64, 1, 1
  ret { ptr, { i64, i64 } } %fat3
}


; Implicit equality for type STRING
define linkonce_odr i1 @_ada_eq_STRING_1({ ptr, { i64, i64 } } %0, { ptr, { i64, i64 } } %1) {
entry:
  %left_low = extractvalue { ptr, { i64, i64 } } %0, 1, 0
  %left_high = extractvalue { ptr, { i64, i64 } } %0, 1, 1
  %left_len = sub i64 %left_high, %left_low
  %left_len1 = add i64 %left_len, 1
  %right_low = extractvalue { ptr, { i64, i64 } } %1, 1, 0
  %right_high = extractvalue { ptr, { i64, i64 } } %1, 1, 1
  %right_len = sub i64 %right_high, %right_low
  %right_len1 = add i64 %right_len, 1
  %len_eq = icmp eq i64 %left_len1, %right_len1
  %left_data = extractvalue { ptr, { i64, i64 } } %0, 0
  %right_data = extractvalue { ptr, { i64, i64 } } %1, 0
  %byte_size = mul i64 %left_len1, 1
  %memcmp_res = call i32 @memcmp(ptr %left_data, ptr %right_data, i64 %byte_size)
  %data_eq = icmp eq i32 %memcmp_res, 0
  %result = and i1 %len_eq, %data_eq
  ret i1 %result
}

@report__legal_file_name.data = linkonce_odr constant [13 x i8] c"test_file.tmp"
@report__legal_file_name = linkonce_odr constant { ptr, { i64, i64 } } { ptr @report__legal_file_name.data, { i64, i64 } { i64 1, i64 13 } }
@report__legal_file_name_2.data = linkonce_odr constant [15 x i8] c"test_file_2.tmp"
@report__legal_file_name_2 = linkonce_odr constant { ptr, { i64, i64 } } { ptr @report__legal_file_name_2.data, { i64, i64 } { i64 1, i64 15 } }
@report__max_in_len = linkonce_odr constant i64 1000
@report__variable_address = linkonce_odr constant i64 0
@report__f = linkonce_odr global i64 0
define void @report__test({ ptr, { i64, i64 } } %p0, { ptr, { i64, i64 } } %p1) {
entry:
  %name_s157 = alloca { ptr, { i64, i64 } }
  store { ptr, { i64, i64 } } %p0, ptr %name_s157
  %desc_s158 = alloca { ptr, { i64, i64 } }
  store { ptr, { i64, i64 } } %p1, ptr %desc_s158
  %t1 = add i64 0, 0
  store i64 %t1, ptr @report__f
  %t2 = add i64 0, 84
  %t3 = add i64 0, -2147483648  ; literal bound
  %t4 = add i64 0, 2147483647  ; literal bound
  %t5 = icmp slt i64 %t2, %t3
  br i1 %t5, label %L2, label %L1
L1:
  %t6 = icmp sgt i64 %t2, %t4
  br i1 %t6, label %L2, label %L3
L2:  ; raise CONSTRAINT_ERROR
  %t7 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t7)
  unreachable
L3:
  call void @__text_io_put_char(i64 %t2)
  %t9 = add i64 0, 69
  %t10 = add i64 0, -2147483648  ; literal bound
  %t11 = add i64 0, 2147483647  ; literal bound
  %t12 = icmp slt i64 %t9, %t10
  br i1 %t12, label %L5, label %L4
L4:
  %t13 = icmp sgt i64 %t9, %t11
  br i1 %t13, label %L5, label %L6
L5:  ; raise CONSTRAINT_ERROR
  %t14 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t14)
  unreachable
L6:
  call void @__text_io_put_char(i64 %t9)
  %t16 = add i64 0, 83
  %t17 = add i64 0, -2147483648  ; literal bound
  %t18 = add i64 0, 2147483647  ; literal bound
  %t19 = icmp slt i64 %t16, %t17
  br i1 %t19, label %L8, label %L7
L7:
  %t20 = icmp sgt i64 %t16, %t18
  br i1 %t20, label %L8, label %L9
L8:  ; raise CONSTRAINT_ERROR
  %t21 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t21)
  unreachable
L9:
  call void @__text_io_put_char(i64 %t16)
  %t23 = add i64 0, 84
  %t24 = add i64 0, -2147483648  ; literal bound
  %t25 = add i64 0, 2147483647  ; literal bound
  %t26 = icmp slt i64 %t23, %t24
  br i1 %t26, label %L11, label %L10
L10:
  %t27 = icmp sgt i64 %t23, %t25
  br i1 %t27, label %L11, label %L12
L11:  ; raise CONSTRAINT_ERROR
  %t28 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t28)
  unreachable
L12:
  call void @__text_io_put_char(i64 %t23)
  %t30 = add i64 0, 32
  %t31 = add i64 0, -2147483648  ; literal bound
  %t32 = add i64 0, 2147483647  ; literal bound
  %t33 = icmp slt i64 %t30, %t31
  br i1 %t33, label %L14, label %L13
L13:
  %t34 = icmp sgt i64 %t30, %t32
  br i1 %t34, label %L14, label %L15
L14:  ; raise CONSTRAINT_ERROR
  %t35 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t35)
  unreachable
L15:
  call void @__text_io_put_char(i64 %t30)
  %i_s159 = alloca i64
  %t37 = load { ptr, { i64, i64 } }, ptr %name_s157
  %t38 = extractvalue { ptr, { i64, i64 } } %t37, 1, 0
  %t39 = extractvalue { ptr, { i64, i64 } } %t37, 1, 1
  store i64 %t38, ptr %i_s159
  br label %L16
L16:
  %t40 = load i64, ptr %i_s159
  %t41 = icmp sle i64 %t40, %t39
  br i1 %t41, label %L17, label %L18
L17:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t43 = load { ptr, { i64, i64 } }, ptr %name_s157
  %t44 = extractvalue { ptr, { i64, i64 } } %t43, 0
  %t45 = extractvalue { ptr, { i64, i64 } } %t43, 1, 0
  %t46 = load i64, ptr %i_s159
  %t47 = sub i64 %t46, %t45  ; adjust for dynamic low bound
  %t48 = getelementptr i8, ptr %t44, i64 %t47
  %t49 = load i8, ptr %t48
  %t50 = sext i8 %t49 to i64
  %t51 = add i64 0, -2147483648  ; literal bound
  %t52 = add i64 0, 2147483647  ; literal bound
  %t53 = icmp slt i64 %t50, %t51
  br i1 %t53, label %L20, label %L19
L19:
  %t54 = icmp sgt i64 %t50, %t52
  br i1 %t54, label %L20, label %L21
L20:  ; raise CONSTRAINT_ERROR
  %t55 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t55)
  unreachable
L21:
  call void @__text_io_put_char(i64 %t50)
  %t57 = add i64 %t40, 1
  store i64 %t57, ptr %i_s159
  br label %L16
L18:
  %t58 = add i64 0, 58
  %t59 = add i64 0, -2147483648  ; literal bound
  %t60 = add i64 0, 2147483647  ; literal bound
  %t61 = icmp slt i64 %t58, %t59
  br i1 %t61, label %L23, label %L22
L22:
  %t62 = icmp sgt i64 %t58, %t60
  br i1 %t62, label %L23, label %L24
L23:  ; raise CONSTRAINT_ERROR
  %t63 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t63)
  unreachable
L24:
  call void @__text_io_put_char(i64 %t58)
  %t65 = add i64 0, 32
  %t66 = add i64 0, -2147483648  ; literal bound
  %t67 = add i64 0, 2147483647  ; literal bound
  %t68 = icmp slt i64 %t65, %t66
  br i1 %t68, label %L26, label %L25
L25:
  %t69 = icmp sgt i64 %t65, %t67
  br i1 %t69, label %L26, label %L27
L26:  ; raise CONSTRAINT_ERROR
  %t70 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t70)
  unreachable
L27:
  call void @__text_io_put_char(i64 %t65)
  %i_s160 = alloca i64
  %t72 = load { ptr, { i64, i64 } }, ptr %desc_s158
  %t73 = extractvalue { ptr, { i64, i64 } } %t72, 1, 0
  %t74 = extractvalue { ptr, { i64, i64 } } %t72, 1, 1
  store i64 %t73, ptr %i_s160
  br label %L28
L28:
  %t75 = load i64, ptr %i_s160
  %t76 = icmp sle i64 %t75, %t74
  br i1 %t76, label %L29, label %L30
L29:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t78 = load { ptr, { i64, i64 } }, ptr %desc_s158
  %t79 = extractvalue { ptr, { i64, i64 } } %t78, 0
  %t80 = extractvalue { ptr, { i64, i64 } } %t78, 1, 0
  %t81 = load i64, ptr %i_s160
  %t82 = sub i64 %t81, %t80  ; adjust for dynamic low bound
  %t83 = getelementptr i8, ptr %t79, i64 %t82
  %t84 = load i8, ptr %t83
  %t85 = sext i8 %t84 to i64
  %t86 = add i64 0, -2147483648  ; literal bound
  %t87 = add i64 0, 2147483647  ; literal bound
  %t88 = icmp slt i64 %t85, %t86
  br i1 %t88, label %L32, label %L31
L31:
  %t89 = icmp sgt i64 %t85, %t87
  br i1 %t89, label %L32, label %L33
L32:  ; raise CONSTRAINT_ERROR
  %t90 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t90)
  unreachable
L33:
  call void @__text_io_put_char(i64 %t85)
  %t92 = add i64 %t75, 1
  store i64 %t92, ptr %i_s160
  br label %L28
L30:
  call void @__text_io_new_line()
  ret void
}

define void @report__failed({ ptr, { i64, i64 } } %p0) {
entry:
  %msg_s161 = alloca { ptr, { i64, i64 } }
  store { ptr, { i64, i64 } } %p0, ptr %msg_s161
  %t93 = add i64 0, 70
  %t94 = add i64 0, -2147483648  ; literal bound
  %t95 = add i64 0, 2147483647  ; literal bound
  %t96 = icmp slt i64 %t93, %t94
  br i1 %t96, label %L35, label %L34
L34:
  %t97 = icmp sgt i64 %t93, %t95
  br i1 %t97, label %L35, label %L36
L35:  ; raise CONSTRAINT_ERROR
  %t98 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t98)
  unreachable
L36:
  call void @__text_io_put_char(i64 %t93)
  %t100 = add i64 0, 65
  %t101 = add i64 0, -2147483648  ; literal bound
  %t102 = add i64 0, 2147483647  ; literal bound
  %t103 = icmp slt i64 %t100, %t101
  br i1 %t103, label %L38, label %L37
L37:
  %t104 = icmp sgt i64 %t100, %t102
  br i1 %t104, label %L38, label %L39
L38:  ; raise CONSTRAINT_ERROR
  %t105 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t105)
  unreachable
L39:
  call void @__text_io_put_char(i64 %t100)
  %t107 = add i64 0, 73
  %t108 = add i64 0, -2147483648  ; literal bound
  %t109 = add i64 0, 2147483647  ; literal bound
  %t110 = icmp slt i64 %t107, %t108
  br i1 %t110, label %L41, label %L40
L40:
  %t111 = icmp sgt i64 %t107, %t109
  br i1 %t111, label %L41, label %L42
L41:  ; raise CONSTRAINT_ERROR
  %t112 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t112)
  unreachable
L42:
  call void @__text_io_put_char(i64 %t107)
  %t114 = add i64 0, 76
  %t115 = add i64 0, -2147483648  ; literal bound
  %t116 = add i64 0, 2147483647  ; literal bound
  %t117 = icmp slt i64 %t114, %t115
  br i1 %t117, label %L44, label %L43
L43:
  %t118 = icmp sgt i64 %t114, %t116
  br i1 %t118, label %L44, label %L45
L44:  ; raise CONSTRAINT_ERROR
  %t119 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t119)
  unreachable
L45:
  call void @__text_io_put_char(i64 %t114)
  %t121 = add i64 0, 69
  %t122 = add i64 0, -2147483648  ; literal bound
  %t123 = add i64 0, 2147483647  ; literal bound
  %t124 = icmp slt i64 %t121, %t122
  br i1 %t124, label %L47, label %L46
L46:
  %t125 = icmp sgt i64 %t121, %t123
  br i1 %t125, label %L47, label %L48
L47:  ; raise CONSTRAINT_ERROR
  %t126 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t126)
  unreachable
L48:
  call void @__text_io_put_char(i64 %t121)
  %t128 = add i64 0, 68
  %t129 = add i64 0, -2147483648  ; literal bound
  %t130 = add i64 0, 2147483647  ; literal bound
  %t131 = icmp slt i64 %t128, %t129
  br i1 %t131, label %L50, label %L49
L49:
  %t132 = icmp sgt i64 %t128, %t130
  br i1 %t132, label %L50, label %L51
L50:  ; raise CONSTRAINT_ERROR
  %t133 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t133)
  unreachable
L51:
  call void @__text_io_put_char(i64 %t128)
  %t135 = add i64 0, 58
  %t136 = add i64 0, -2147483648  ; literal bound
  %t137 = add i64 0, 2147483647  ; literal bound
  %t138 = icmp slt i64 %t135, %t136
  br i1 %t138, label %L53, label %L52
L52:
  %t139 = icmp sgt i64 %t135, %t137
  br i1 %t139, label %L53, label %L54
L53:  ; raise CONSTRAINT_ERROR
  %t140 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t140)
  unreachable
L54:
  call void @__text_io_put_char(i64 %t135)
  %t142 = add i64 0, 32
  %t143 = add i64 0, -2147483648  ; literal bound
  %t144 = add i64 0, 2147483647  ; literal bound
  %t145 = icmp slt i64 %t142, %t143
  br i1 %t145, label %L56, label %L55
L55:
  %t146 = icmp sgt i64 %t142, %t144
  br i1 %t146, label %L56, label %L57
L56:  ; raise CONSTRAINT_ERROR
  %t147 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t147)
  unreachable
L57:
  call void @__text_io_put_char(i64 %t142)
  %i_s162 = alloca i64
  %t149 = load { ptr, { i64, i64 } }, ptr %msg_s161
  %t150 = extractvalue { ptr, { i64, i64 } } %t149, 1, 0
  %t151 = extractvalue { ptr, { i64, i64 } } %t149, 1, 1
  store i64 %t150, ptr %i_s162
  br label %L58
L58:
  %t152 = load i64, ptr %i_s162
  %t153 = icmp sle i64 %t152, %t151
  br i1 %t153, label %L59, label %L60
L59:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t155 = load { ptr, { i64, i64 } }, ptr %msg_s161
  %t156 = extractvalue { ptr, { i64, i64 } } %t155, 0
  %t157 = extractvalue { ptr, { i64, i64 } } %t155, 1, 0
  %t158 = load i64, ptr %i_s162
  %t159 = sub i64 %t158, %t157  ; adjust for dynamic low bound
  %t160 = getelementptr i8, ptr %t156, i64 %t159
  %t161 = load i8, ptr %t160
  %t162 = sext i8 %t161 to i64
  %t163 = add i64 0, -2147483648  ; literal bound
  %t164 = add i64 0, 2147483647  ; literal bound
  %t165 = icmp slt i64 %t162, %t163
  br i1 %t165, label %L62, label %L61
L61:
  %t166 = icmp sgt i64 %t162, %t164
  br i1 %t166, label %L62, label %L63
L62:  ; raise CONSTRAINT_ERROR
  %t167 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t167)
  unreachable
L63:
  call void @__text_io_put_char(i64 %t162)
  %t169 = add i64 %t152, 1
  store i64 %t169, ptr %i_s162
  br label %L58
L60:
  call void @__text_io_new_line()
  %t170 = add i64 0, 1
  store i64 %t170, ptr @report__f
  ret void
}

define void @report__result() {
entry:
  %t171 = load i64, ptr @report__f
  %t172 = add i64 0, 0
  %t173 = icmp ne i64 %t171, %t172
  br i1 %t173, label %L64, label %L65
L64:
  %t174 = add i64 0, 70
  %t175 = add i64 0, -2147483648  ; literal bound
  %t176 = add i64 0, 2147483647  ; literal bound
  %t177 = icmp slt i64 %t174, %t175
  br i1 %t177, label %L68, label %L67
L67:
  %t178 = icmp sgt i64 %t174, %t176
  br i1 %t178, label %L68, label %L69
L68:  ; raise CONSTRAINT_ERROR
  %t179 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t179)
  unreachable
L69:
  call void @__text_io_put_char(i64 %t174)
  %t181 = add i64 0, 65
  %t182 = add i64 0, -2147483648  ; literal bound
  %t183 = add i64 0, 2147483647  ; literal bound
  %t184 = icmp slt i64 %t181, %t182
  br i1 %t184, label %L71, label %L70
L70:
  %t185 = icmp sgt i64 %t181, %t183
  br i1 %t185, label %L71, label %L72
L71:  ; raise CONSTRAINT_ERROR
  %t186 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t186)
  unreachable
L72:
  call void @__text_io_put_char(i64 %t181)
  %t188 = add i64 0, 73
  %t189 = add i64 0, -2147483648  ; literal bound
  %t190 = add i64 0, 2147483647  ; literal bound
  %t191 = icmp slt i64 %t188, %t189
  br i1 %t191, label %L74, label %L73
L73:
  %t192 = icmp sgt i64 %t188, %t190
  br i1 %t192, label %L74, label %L75
L74:  ; raise CONSTRAINT_ERROR
  %t193 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t193)
  unreachable
L75:
  call void @__text_io_put_char(i64 %t188)
  %t195 = add i64 0, 76
  %t196 = add i64 0, -2147483648  ; literal bound
  %t197 = add i64 0, 2147483647  ; literal bound
  %t198 = icmp slt i64 %t195, %t196
  br i1 %t198, label %L77, label %L76
L76:
  %t199 = icmp sgt i64 %t195, %t197
  br i1 %t199, label %L77, label %L78
L77:  ; raise CONSTRAINT_ERROR
  %t200 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t200)
  unreachable
L78:
  call void @__text_io_put_char(i64 %t195)
  %t202 = add i64 0, 69
  %t203 = add i64 0, -2147483648  ; literal bound
  %t204 = add i64 0, 2147483647  ; literal bound
  %t205 = icmp slt i64 %t202, %t203
  br i1 %t205, label %L80, label %L79
L79:
  %t206 = icmp sgt i64 %t202, %t204
  br i1 %t206, label %L80, label %L81
L80:  ; raise CONSTRAINT_ERROR
  %t207 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t207)
  unreachable
L81:
  call void @__text_io_put_char(i64 %t202)
  %t209 = add i64 0, 68
  %t210 = add i64 0, -2147483648  ; literal bound
  %t211 = add i64 0, 2147483647  ; literal bound
  %t212 = icmp slt i64 %t209, %t210
  br i1 %t212, label %L83, label %L82
L82:
  %t213 = icmp sgt i64 %t209, %t211
  br i1 %t213, label %L83, label %L84
L83:  ; raise CONSTRAINT_ERROR
  %t214 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t214)
  unreachable
L84:
  call void @__text_io_put_char(i64 %t209)
  br label %L66
L65:
  %t216 = add i64 0, 80
  %t217 = add i64 0, -2147483648  ; literal bound
  %t218 = add i64 0, 2147483647  ; literal bound
  %t219 = icmp slt i64 %t216, %t217
  br i1 %t219, label %L86, label %L85
L85:
  %t220 = icmp sgt i64 %t216, %t218
  br i1 %t220, label %L86, label %L87
L86:  ; raise CONSTRAINT_ERROR
  %t221 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t221)
  unreachable
L87:
  call void @__text_io_put_char(i64 %t216)
  %t223 = add i64 0, 65
  %t224 = add i64 0, -2147483648  ; literal bound
  %t225 = add i64 0, 2147483647  ; literal bound
  %t226 = icmp slt i64 %t223, %t224
  br i1 %t226, label %L89, label %L88
L88:
  %t227 = icmp sgt i64 %t223, %t225
  br i1 %t227, label %L89, label %L90
L89:  ; raise CONSTRAINT_ERROR
  %t228 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t228)
  unreachable
L90:
  call void @__text_io_put_char(i64 %t223)
  %t230 = add i64 0, 83
  %t231 = add i64 0, -2147483648  ; literal bound
  %t232 = add i64 0, 2147483647  ; literal bound
  %t233 = icmp slt i64 %t230, %t231
  br i1 %t233, label %L92, label %L91
L91:
  %t234 = icmp sgt i64 %t230, %t232
  br i1 %t234, label %L92, label %L93
L92:  ; raise CONSTRAINT_ERROR
  %t235 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t235)
  unreachable
L93:
  call void @__text_io_put_char(i64 %t230)
  %t237 = add i64 0, 83
  %t238 = add i64 0, -2147483648  ; literal bound
  %t239 = add i64 0, 2147483647  ; literal bound
  %t240 = icmp slt i64 %t237, %t238
  br i1 %t240, label %L95, label %L94
L94:
  %t241 = icmp sgt i64 %t237, %t239
  br i1 %t241, label %L95, label %L96
L95:  ; raise CONSTRAINT_ERROR
  %t242 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t242)
  unreachable
L96:
  call void @__text_io_put_char(i64 %t237)
  %t244 = add i64 0, 69
  %t245 = add i64 0, -2147483648  ; literal bound
  %t246 = add i64 0, 2147483647  ; literal bound
  %t247 = icmp slt i64 %t244, %t245
  br i1 %t247, label %L98, label %L97
L97:
  %t248 = icmp sgt i64 %t244, %t246
  br i1 %t248, label %L98, label %L99
L98:  ; raise CONSTRAINT_ERROR
  %t249 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t249)
  unreachable
L99:
  call void @__text_io_put_char(i64 %t244)
  %t251 = add i64 0, 68
  %t252 = add i64 0, -2147483648  ; literal bound
  %t253 = add i64 0, 2147483647  ; literal bound
  %t254 = icmp slt i64 %t251, %t252
  br i1 %t254, label %L101, label %L100
L100:
  %t255 = icmp sgt i64 %t251, %t253
  br i1 %t255, label %L101, label %L102
L101:  ; raise CONSTRAINT_ERROR
  %t256 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t256)
  unreachable
L102:
  call void @__text_io_put_char(i64 %t251)
  br label %L66
L66:
  call void @__text_io_new_line()
  ret void
}

define void @report__comment({ ptr, { i64, i64 } } %p0) {
entry:
  %msg_s163 = alloca { ptr, { i64, i64 } }
  store { ptr, { i64, i64 } } %p0, ptr %msg_s163
  %t258 = add i64 0, 67
  %t259 = add i64 0, -2147483648  ; literal bound
  %t260 = add i64 0, 2147483647  ; literal bound
  %t261 = icmp slt i64 %t258, %t259
  br i1 %t261, label %L104, label %L103
L103:
  %t262 = icmp sgt i64 %t258, %t260
  br i1 %t262, label %L104, label %L105
L104:  ; raise CONSTRAINT_ERROR
  %t263 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t263)
  unreachable
L105:
  call void @__text_io_put_char(i64 %t258)
  %t265 = add i64 0, 79
  %t266 = add i64 0, -2147483648  ; literal bound
  %t267 = add i64 0, 2147483647  ; literal bound
  %t268 = icmp slt i64 %t265, %t266
  br i1 %t268, label %L107, label %L106
L106:
  %t269 = icmp sgt i64 %t265, %t267
  br i1 %t269, label %L107, label %L108
L107:  ; raise CONSTRAINT_ERROR
  %t270 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t270)
  unreachable
L108:
  call void @__text_io_put_char(i64 %t265)
  %t272 = add i64 0, 77
  %t273 = add i64 0, -2147483648  ; literal bound
  %t274 = add i64 0, 2147483647  ; literal bound
  %t275 = icmp slt i64 %t272, %t273
  br i1 %t275, label %L110, label %L109
L109:
  %t276 = icmp sgt i64 %t272, %t274
  br i1 %t276, label %L110, label %L111
L110:  ; raise CONSTRAINT_ERROR
  %t277 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t277)
  unreachable
L111:
  call void @__text_io_put_char(i64 %t272)
  %t279 = add i64 0, 77
  %t280 = add i64 0, -2147483648  ; literal bound
  %t281 = add i64 0, 2147483647  ; literal bound
  %t282 = icmp slt i64 %t279, %t280
  br i1 %t282, label %L113, label %L112
L112:
  %t283 = icmp sgt i64 %t279, %t281
  br i1 %t283, label %L113, label %L114
L113:  ; raise CONSTRAINT_ERROR
  %t284 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t284)
  unreachable
L114:
  call void @__text_io_put_char(i64 %t279)
  %t286 = add i64 0, 69
  %t287 = add i64 0, -2147483648  ; literal bound
  %t288 = add i64 0, 2147483647  ; literal bound
  %t289 = icmp slt i64 %t286, %t287
  br i1 %t289, label %L116, label %L115
L115:
  %t290 = icmp sgt i64 %t286, %t288
  br i1 %t290, label %L116, label %L117
L116:  ; raise CONSTRAINT_ERROR
  %t291 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t291)
  unreachable
L117:
  call void @__text_io_put_char(i64 %t286)
  %t293 = add i64 0, 78
  %t294 = add i64 0, -2147483648  ; literal bound
  %t295 = add i64 0, 2147483647  ; literal bound
  %t296 = icmp slt i64 %t293, %t294
  br i1 %t296, label %L119, label %L118
L118:
  %t297 = icmp sgt i64 %t293, %t295
  br i1 %t297, label %L119, label %L120
L119:  ; raise CONSTRAINT_ERROR
  %t298 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t298)
  unreachable
L120:
  call void @__text_io_put_char(i64 %t293)
  %t300 = add i64 0, 84
  %t301 = add i64 0, -2147483648  ; literal bound
  %t302 = add i64 0, 2147483647  ; literal bound
  %t303 = icmp slt i64 %t300, %t301
  br i1 %t303, label %L122, label %L121
L121:
  %t304 = icmp sgt i64 %t300, %t302
  br i1 %t304, label %L122, label %L123
L122:  ; raise CONSTRAINT_ERROR
  %t305 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t305)
  unreachable
L123:
  call void @__text_io_put_char(i64 %t300)
  %t307 = add i64 0, 58
  %t308 = add i64 0, -2147483648  ; literal bound
  %t309 = add i64 0, 2147483647  ; literal bound
  %t310 = icmp slt i64 %t307, %t308
  br i1 %t310, label %L125, label %L124
L124:
  %t311 = icmp sgt i64 %t307, %t309
  br i1 %t311, label %L125, label %L126
L125:  ; raise CONSTRAINT_ERROR
  %t312 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t312)
  unreachable
L126:
  call void @__text_io_put_char(i64 %t307)
  %t314 = add i64 0, 32
  %t315 = add i64 0, -2147483648  ; literal bound
  %t316 = add i64 0, 2147483647  ; literal bound
  %t317 = icmp slt i64 %t314, %t315
  br i1 %t317, label %L128, label %L127
L127:
  %t318 = icmp sgt i64 %t314, %t316
  br i1 %t318, label %L128, label %L129
L128:  ; raise CONSTRAINT_ERROR
  %t319 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t319)
  unreachable
L129:
  call void @__text_io_put_char(i64 %t314)
  %i_s164 = alloca i64
  %t321 = load { ptr, { i64, i64 } }, ptr %msg_s163
  %t322 = extractvalue { ptr, { i64, i64 } } %t321, 1, 0
  %t323 = extractvalue { ptr, { i64, i64 } } %t321, 1, 1
  store i64 %t322, ptr %i_s164
  br label %L130
L130:
  %t324 = load i64, ptr %i_s164
  %t325 = icmp sle i64 %t324, %t323
  br i1 %t325, label %L131, label %L132
L131:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t327 = load { ptr, { i64, i64 } }, ptr %msg_s163
  %t328 = extractvalue { ptr, { i64, i64 } } %t327, 0
  %t329 = extractvalue { ptr, { i64, i64 } } %t327, 1, 0
  %t330 = load i64, ptr %i_s164
  %t331 = sub i64 %t330, %t329  ; adjust for dynamic low bound
  %t332 = getelementptr i8, ptr %t328, i64 %t331
  %t333 = load i8, ptr %t332
  %t334 = sext i8 %t333 to i64
  %t335 = add i64 0, -2147483648  ; literal bound
  %t336 = add i64 0, 2147483647  ; literal bound
  %t337 = icmp slt i64 %t334, %t335
  br i1 %t337, label %L134, label %L133
L133:
  %t338 = icmp sgt i64 %t334, %t336
  br i1 %t338, label %L134, label %L135
L134:  ; raise CONSTRAINT_ERROR
  %t339 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t339)
  unreachable
L135:
  call void @__text_io_put_char(i64 %t334)
  %t341 = add i64 %t324, 1
  store i64 %t341, ptr %i_s164
  br label %L130
L132:
  call void @__text_io_new_line()
  ret void
}

define i64 @report__ident_int(i64 %p0) {
entry:
  %x_s165 = alloca i64
  store i64 %p0, ptr %x_s165
  %t342 = load i64, ptr %x_s165
  ret i64 %t342
}

define i1 @report__ident_bool(i1 %p0) {
entry:
  %x_s166 = alloca i1
  store i1 %p0, ptr %x_s166
  %t343 = load i1, ptr %x_s166
  %t344 = zext i1 %t343 to i64
  %t345 = icmp ne i64 %t344, 0
  ret i1 %t345
}

define i8 @report__ident_char(i8 %p0) {
entry:
  %x_s167 = alloca i8
  store i8 %p0, ptr %x_s167
  %t346 = load i8, ptr %x_s167
  %t347 = sext i8 %t346 to i64
  %t348 = trunc i64 %t347 to i8
  ret i8 %t348
}

define { ptr, { i64, i64 } } @report__ident_str({ ptr, { i64, i64 } } %p0) {
entry:
  %x_s168 = alloca { ptr, { i64, i64 } }
  store { ptr, { i64, i64 } } %p0, ptr %x_s168
  %t349 = load { ptr, { i64, i64 } }, ptr %x_s168
  ret { ptr, { i64, i64 } } %t349
}

define i1 @report__equal(i64 %p0, i64 %p1) {
entry:
  %x_s169 = alloca i64
  store i64 %p0, ptr %x_s169
  %y_s170 = alloca i64
  store i64 %p1, ptr %y_s170
  %t351 = load i64, ptr %x_s169
  %t352 = load i64, ptr %y_s170
  %t353 = icmp eq i64 %t351, %t352
  ret i1 %t353
}

define void @report__not_applicable({ ptr, { i64, i64 } } %p0) {
entry:
  %descr_s171 = alloca { ptr, { i64, i64 } }
  store { ptr, { i64, i64 } } %p0, ptr %descr_s171
  %t354 = add i64 0, 78
  %t355 = add i64 0, -2147483648  ; literal bound
  %t356 = add i64 0, 2147483647  ; literal bound
  %t357 = icmp slt i64 %t354, %t355
  br i1 %t357, label %L137, label %L136
L136:
  %t358 = icmp sgt i64 %t354, %t356
  br i1 %t358, label %L137, label %L138
L137:  ; raise CONSTRAINT_ERROR
  %t359 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t359)
  unreachable
L138:
  call void @__text_io_put_char(i64 %t354)
  %t361 = add i64 0, 79
  %t362 = add i64 0, -2147483648  ; literal bound
  %t363 = add i64 0, 2147483647  ; literal bound
  %t364 = icmp slt i64 %t361, %t362
  br i1 %t364, label %L140, label %L139
L139:
  %t365 = icmp sgt i64 %t361, %t363
  br i1 %t365, label %L140, label %L141
L140:  ; raise CONSTRAINT_ERROR
  %t366 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t366)
  unreachable
L141:
  call void @__text_io_put_char(i64 %t361)
  %t368 = add i64 0, 84
  %t369 = add i64 0, -2147483648  ; literal bound
  %t370 = add i64 0, 2147483647  ; literal bound
  %t371 = icmp slt i64 %t368, %t369
  br i1 %t371, label %L143, label %L142
L142:
  %t372 = icmp sgt i64 %t368, %t370
  br i1 %t372, label %L143, label %L144
L143:  ; raise CONSTRAINT_ERROR
  %t373 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t373)
  unreachable
L144:
  call void @__text_io_put_char(i64 %t368)
  %t375 = add i64 0, 32
  %t376 = add i64 0, -2147483648  ; literal bound
  %t377 = add i64 0, 2147483647  ; literal bound
  %t378 = icmp slt i64 %t375, %t376
  br i1 %t378, label %L146, label %L145
L145:
  %t379 = icmp sgt i64 %t375, %t377
  br i1 %t379, label %L146, label %L147
L146:  ; raise CONSTRAINT_ERROR
  %t380 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t380)
  unreachable
L147:
  call void @__text_io_put_char(i64 %t375)
  %t382 = add i64 0, 65
  %t383 = add i64 0, -2147483648  ; literal bound
  %t384 = add i64 0, 2147483647  ; literal bound
  %t385 = icmp slt i64 %t382, %t383
  br i1 %t385, label %L149, label %L148
L148:
  %t386 = icmp sgt i64 %t382, %t384
  br i1 %t386, label %L149, label %L150
L149:  ; raise CONSTRAINT_ERROR
  %t387 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t387)
  unreachable
L150:
  call void @__text_io_put_char(i64 %t382)
  %t389 = add i64 0, 80
  %t390 = add i64 0, -2147483648  ; literal bound
  %t391 = add i64 0, 2147483647  ; literal bound
  %t392 = icmp slt i64 %t389, %t390
  br i1 %t392, label %L152, label %L151
L151:
  %t393 = icmp sgt i64 %t389, %t391
  br i1 %t393, label %L152, label %L153
L152:  ; raise CONSTRAINT_ERROR
  %t394 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t394)
  unreachable
L153:
  call void @__text_io_put_char(i64 %t389)
  %t396 = add i64 0, 80
  %t397 = add i64 0, -2147483648  ; literal bound
  %t398 = add i64 0, 2147483647  ; literal bound
  %t399 = icmp slt i64 %t396, %t397
  br i1 %t399, label %L155, label %L154
L154:
  %t400 = icmp sgt i64 %t396, %t398
  br i1 %t400, label %L155, label %L156
L155:  ; raise CONSTRAINT_ERROR
  %t401 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t401)
  unreachable
L156:
  call void @__text_io_put_char(i64 %t396)
  %t403 = add i64 0, 76
  %t404 = add i64 0, -2147483648  ; literal bound
  %t405 = add i64 0, 2147483647  ; literal bound
  %t406 = icmp slt i64 %t403, %t404
  br i1 %t406, label %L158, label %L157
L157:
  %t407 = icmp sgt i64 %t403, %t405
  br i1 %t407, label %L158, label %L159
L158:  ; raise CONSTRAINT_ERROR
  %t408 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t408)
  unreachable
L159:
  call void @__text_io_put_char(i64 %t403)
  %t410 = add i64 0, 73
  %t411 = add i64 0, -2147483648  ; literal bound
  %t412 = add i64 0, 2147483647  ; literal bound
  %t413 = icmp slt i64 %t410, %t411
  br i1 %t413, label %L161, label %L160
L160:
  %t414 = icmp sgt i64 %t410, %t412
  br i1 %t414, label %L161, label %L162
L161:  ; raise CONSTRAINT_ERROR
  %t415 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t415)
  unreachable
L162:
  call void @__text_io_put_char(i64 %t410)
  %t417 = add i64 0, 67
  %t418 = add i64 0, -2147483648  ; literal bound
  %t419 = add i64 0, 2147483647  ; literal bound
  %t420 = icmp slt i64 %t417, %t418
  br i1 %t420, label %L164, label %L163
L163:
  %t421 = icmp sgt i64 %t417, %t419
  br i1 %t421, label %L164, label %L165
L164:  ; raise CONSTRAINT_ERROR
  %t422 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t422)
  unreachable
L165:
  call void @__text_io_put_char(i64 %t417)
  %t424 = add i64 0, 65
  %t425 = add i64 0, -2147483648  ; literal bound
  %t426 = add i64 0, 2147483647  ; literal bound
  %t427 = icmp slt i64 %t424, %t425
  br i1 %t427, label %L167, label %L166
L166:
  %t428 = icmp sgt i64 %t424, %t426
  br i1 %t428, label %L167, label %L168
L167:  ; raise CONSTRAINT_ERROR
  %t429 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t429)
  unreachable
L168:
  call void @__text_io_put_char(i64 %t424)
  %t431 = add i64 0, 66
  %t432 = add i64 0, -2147483648  ; literal bound
  %t433 = add i64 0, 2147483647  ; literal bound
  %t434 = icmp slt i64 %t431, %t432
  br i1 %t434, label %L170, label %L169
L169:
  %t435 = icmp sgt i64 %t431, %t433
  br i1 %t435, label %L170, label %L171
L170:  ; raise CONSTRAINT_ERROR
  %t436 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t436)
  unreachable
L171:
  call void @__text_io_put_char(i64 %t431)
  %t438 = add i64 0, 76
  %t439 = add i64 0, -2147483648  ; literal bound
  %t440 = add i64 0, 2147483647  ; literal bound
  %t441 = icmp slt i64 %t438, %t439
  br i1 %t441, label %L173, label %L172
L172:
  %t442 = icmp sgt i64 %t438, %t440
  br i1 %t442, label %L173, label %L174
L173:  ; raise CONSTRAINT_ERROR
  %t443 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t443)
  unreachable
L174:
  call void @__text_io_put_char(i64 %t438)
  %t445 = add i64 0, 69
  %t446 = add i64 0, -2147483648  ; literal bound
  %t447 = add i64 0, 2147483647  ; literal bound
  %t448 = icmp slt i64 %t445, %t446
  br i1 %t448, label %L176, label %L175
L175:
  %t449 = icmp sgt i64 %t445, %t447
  br i1 %t449, label %L176, label %L177
L176:  ; raise CONSTRAINT_ERROR
  %t450 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t450)
  unreachable
L177:
  call void @__text_io_put_char(i64 %t445)
  %t452 = add i64 0, 58
  %t453 = add i64 0, -2147483648  ; literal bound
  %t454 = add i64 0, 2147483647  ; literal bound
  %t455 = icmp slt i64 %t452, %t453
  br i1 %t455, label %L179, label %L178
L178:
  %t456 = icmp sgt i64 %t452, %t454
  br i1 %t456, label %L179, label %L180
L179:  ; raise CONSTRAINT_ERROR
  %t457 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t457)
  unreachable
L180:
  call void @__text_io_put_char(i64 %t452)
  %t459 = add i64 0, 32
  %t460 = add i64 0, -2147483648  ; literal bound
  %t461 = add i64 0, 2147483647  ; literal bound
  %t462 = icmp slt i64 %t459, %t460
  br i1 %t462, label %L182, label %L181
L181:
  %t463 = icmp sgt i64 %t459, %t461
  br i1 %t463, label %L182, label %L183
L182:  ; raise CONSTRAINT_ERROR
  %t464 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t464)
  unreachable
L183:
  call void @__text_io_put_char(i64 %t459)
  %i_s172 = alloca i64
  %t466 = load { ptr, { i64, i64 } }, ptr %descr_s171
  %t467 = extractvalue { ptr, { i64, i64 } } %t466, 1, 0
  %t468 = extractvalue { ptr, { i64, i64 } } %t466, 1, 1
  store i64 %t467, ptr %i_s172
  br label %L184
L184:
  %t469 = load i64, ptr %i_s172
  %t470 = icmp sle i64 %t469, %t468
  br i1 %t470, label %L185, label %L186
L185:
  ; DEBUG ARRAY INDEX: using fat pointer path (unconstrained=1, dynamic=0)
  %t472 = load { ptr, { i64, i64 } }, ptr %descr_s171
  %t473 = extractvalue { ptr, { i64, i64 } } %t472, 0
  %t474 = extractvalue { ptr, { i64, i64 } } %t472, 1, 0
  %t475 = load i64, ptr %i_s172
  %t476 = sub i64 %t475, %t474  ; adjust for dynamic low bound
  %t477 = getelementptr i8, ptr %t473, i64 %t476
  %t478 = load i8, ptr %t477
  %t479 = sext i8 %t478 to i64
  %t480 = add i64 0, -2147483648  ; literal bound
  %t481 = add i64 0, 2147483647  ; literal bound
  %t482 = icmp slt i64 %t479, %t480
  br i1 %t482, label %L188, label %L187
L187:
  %t483 = icmp sgt i64 %t479, %t481
  br i1 %t483, label %L188, label %L189
L188:  ; raise CONSTRAINT_ERROR
  %t484 = ptrtoint ptr @__exc.constraint_error to i64
  call void @__ada_raise(i64 %t484)
  unreachable
L189:
  call void @__text_io_put_char(i64 %t479)
  %t486 = add i64 %t469, 1
  store i64 %t486, ptr %i_s172
  br label %L184
L186:
  call void @__text_io_new_line()
  ret void
}

