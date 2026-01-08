///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                    L L V M   I R   C O D E   G E N E R A T O R            --
///                                                                           --
///  This module implements LLVM IR code generation for Ada83 programs.       --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_codegen.h"
#include "ada83_string.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>


///-----------------------------------------------------------------------------
///                   I N T E R N A L   H E L P E R S
///-----------------------------------------------------------------------------

/// @brief Emit formatted output to the code generator
///
static void emit(Codegen_Context *ctx, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(ctx->output, fmt, args);
    va_end(args);
}


/// @brief Convert a String_Slice to uppercase C string
///
static char *to_upper_cstr(String_Slice s)
{
    static char buf[256];
    size_t len = s.length < 255 ? s.length : 255;
    for (size_t i = 0; i < len; i++) {
        buf[i] = toupper((unsigned char)s.data[i]);
    }
    buf[len] = '\0';
    return buf;
}


/// @brief Mangle a name for LLVM IR
///
static char *mangle_name(String_Slice name, uint32_t scope_id)
{
    static char buf[512];
    snprintf(buf, sizeof(buf), "%.*s.%u.%u.1",
             (int)name.length, name.data, scope_id,
             (unsigned)(name.length > 0 ? name.data[0] : 0));
    return buf;
}


///-----------------------------------------------------------------------------
///                   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

void Codegen_Init(Codegen_Context *ctx, FILE *output, Semantic_Context *sem)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->output = output;
    ctx->sem = sem;
    ctx->temp_counter = 1;
    ctx->label_counter = 0;
    ctx->string_counter = 0;
}


void Codegen_Cleanup(Codegen_Context *ctx)
{
    /// Free forward declarations
    if (ctx->forward_decls.data) {
        for (uint32_t i = 0; i < ctx->forward_decls.count; i++) {
            free(ctx->forward_decls.data[i]);
        }
        free(ctx->forward_decls.data);
    }

    /// Free string pool
    if (ctx->string_pool.data) {
        for (uint32_t i = 0; i < ctx->string_pool.count; i++) {
            free(ctx->string_pool.data[i]);
        }
        free(ctx->string_pool.data);
    }
}


///-----------------------------------------------------------------------------
///                   R U N T I M E   P R E L U D E
///-----------------------------------------------------------------------------

/// @brief Emit external declarations for C library functions
///
static void emit_extern_decls(Codegen_Context *ctx)
{
    emit(ctx,
        "declare i32 @setjmp(ptr)\n"
        "declare void @longjmp(ptr,i32)\n"
        "declare void @exit(i32)\n"
        "declare i32 @pthread_create(ptr,ptr,ptr,ptr)\n"
        "declare i32 @pthread_join(i64,ptr)\n"
        "declare i32 @pthread_mutex_init(ptr,ptr)\n"
        "declare i32 @pthread_mutex_lock(ptr)\n"
        "declare i32 @pthread_mutex_unlock(ptr)\n"
        "declare i32 @pthread_cond_init(ptr,ptr)\n"
        "declare i32 @pthread_cond_wait(ptr,ptr)\n"
        "declare i32 @pthread_cond_signal(ptr)\n"
        "declare i32 @pthread_cond_broadcast(ptr)\n"
        "declare i32 @usleep(i32)\n"
        "declare ptr @malloc(i64)\n"
        "declare ptr @realloc(ptr,i64)\n"
        "declare void @free(ptr)\n"
        "declare i32 @printf(ptr,...)\n"
        "declare i32 @puts(ptr)\n"
        "declare i32 @sprintf(ptr,ptr,...)\n"
        "declare i32 @snprintf(ptr,i64,ptr,...)\n"
        "declare i32 @strcmp(ptr,ptr)\n"
        "declare ptr @strcpy(ptr,ptr)\n"
        "declare i64 @strlen(ptr)\n"
        "declare ptr @memcpy(ptr,ptr,i64)\n"
        "declare ptr @memset(ptr,i32,i64)\n"
        "declare double @pow(double,double)\n"
        "declare double @sqrt(double)\n"
        "declare double @sin(double)\n"
        "declare double @cos(double)\n"
        "declare double @exp(double)\n"
        "declare double @log(double)\n"
        "declare void @llvm.memcpy.p0.p0.i64(ptr,ptr,i64,i1)\n"
    );
}


/// @brief Emit Ada string to C string conversion helper
///
static void emit_string_helpers(Codegen_Context *ctx)
{
    emit(ctx,
        "define linkonce_odr ptr @__ada_i64str_to_cstr(ptr %%p,i64 %%lo,i64 %%hi){\n"
        "%%ln=sub i64 %%hi,%%lo\n"
        "%%sz=add i64 %%ln,2\n"
        "%%buf=call ptr @malloc(i64 %%sz)\n"
        "br label %%loop\n"
        "loop:\n"
        "%%i=phi i64[0,%%0],[%%ni,%%body]\n"
        "%%cmp=icmp slt i64 %%i,%%sz\n"
        "br i1 %%cmp,label %%body,label %%done\n"
        "body:\n"
        "%%idx=add i64 %%i,%%lo\n"
        "%%adj=sub i64 %%idx,1\n"
        "%%ep=getelementptr i64,ptr %%p,i64 %%adj\n"
        "%%cv=load i64,ptr %%ep\n"
        "%%ch=trunc i64 %%cv to i8\n"
        "%%bp=getelementptr i8,ptr %%buf,i64 %%i\n"
        "store i8 %%ch,ptr %%bp\n"
        "%%ni=add i64 %%i,1\n"
        "br label %%loop\n"
        "done:\n"
        "%%zp=getelementptr i8,ptr %%buf,i64 %%ln\n"
        "store i8 0,ptr %%zp\n"
        "ret ptr %%buf}\n"
    );
}


/// @brief Emit global runtime variables
///
static void emit_runtime_globals(Codegen_Context *ctx)
{
    emit(ctx,
        "@stdin=external global ptr\n"
        "@stdout=external global ptr\n"
        "@stderr=external global ptr\n"
        "@__ss_ptr=linkonce_odr global i64 0\n"
        "@__ss_base=linkonce_odr global ptr null\n"
        "@__ss_size=linkonce_odr global i64 0\n"
        "@__eh_cur=linkonce_odr global ptr null\n"
        "@__ex_cur=linkonce_odr global ptr null\n"
        "@__fin_list=linkonce_odr global ptr null\n"
    );
}


/// @brief Emit exception name constants
///
static void emit_exception_constants(Codegen_Context *ctx)
{
    emit(ctx,
        "@.ex.CONSTRAINT_ERROR=linkonce_odr constant[17 x i8]c\"CONSTRAINT_ERROR\\00\"\n"
        "@.ex.PROGRAM_ERROR=linkonce_odr constant[14 x i8]c\"PROGRAM_ERROR\\00\"\n"
        "@.ex.STORAGE_ERROR=linkonce_odr constant[14 x i8]c\"STORAGE_ERROR\\00\"\n"
        "@.ex.TASKING_ERROR=linkonce_odr constant[14 x i8]c\"TASKING_ERROR\\00\"\n"
        "@.ex.USE_ERROR=linkonce_odr constant[10 x i8]c\"USE_ERROR\\00\"\n"
        "@.ex.NAME_ERROR=linkonce_odr constant[11 x i8]c\"NAME_ERROR\\00\"\n"
        "@.ex.STATUS_ERROR=linkonce_odr constant[13 x i8]c\"STATUS_ERROR\\00\"\n"
        "@.ex.MODE_ERROR=linkonce_odr constant[11 x i8]c\"MODE_ERROR\\00\"\n"
        "@.ex.END_ERROR=linkonce_odr constant[10 x i8]c\"END_ERROR\\00\"\n"
        "@.ex.DATA_ERROR=linkonce_odr constant[11 x i8]c\"DATA_ERROR\\00\"\n"
        "@.ex.DEVICE_ERROR=linkonce_odr constant[13 x i8]c\"DEVICE_ERROR\\00\"\n"
        "@.ex.LAYOUT_ERROR=linkonce_odr constant[13 x i8]c\"LAYOUT_ERROR\\00\"\n"
    );
}


/// @brief Emit secondary stack management functions
///
static void emit_ss_functions(Codegen_Context *ctx)
{
    emit(ctx,
        "define linkonce_odr void @__ada_ss_init(){\n"
        "%%p=call ptr @malloc(i64 1048576)\n"
        "store ptr %%p,ptr @__ss_base\n"
        "store i64 1048576,ptr @__ss_size\n"
        "store i64 0,ptr @__ss_ptr\n"
        "ret void}\n"
        "define linkonce_odr i64 @__ada_ss_mark(){\n"
        "%%m=load i64,ptr @__ss_ptr\n"
        "ret i64 %%m}\n"
        "define linkonce_odr void @__ada_ss_release(i64 %%m){\n"
        "store i64 %%m,ptr @__ss_ptr\n"
        "ret void}\n"
        "define linkonce_odr ptr @__ada_ss_allocate(i64 %%sz){\n"
        "%%1=load ptr,ptr @__ss_base\n"
        "%%2=icmp eq ptr %%1,null\n"
        "br i1 %%2,label %%init,label %%alloc\n"
        "init:\n"
        "call void @__ada_ss_init()\n"
        "%%3=load ptr,ptr @__ss_base\n"
        "br label %%alloc\n"
        "alloc:\n"
        "%%p=phi ptr[%%1,%%0],[%%3,%%init]\n"
        "%%4=load i64,ptr @__ss_ptr\n"
        "%%5=add i64 %%sz,7\n"
        "%%6=and i64 %%5,-8\n"
        "%%7=add i64 %%4,%%6\n"
        "%%8=load i64,ptr @__ss_size\n"
        "%%9=icmp ult i64 %%7,%%8\n"
        "br i1 %%9,label %%ok,label %%grow\n"
        "grow:\n"
        "%%10=mul i64 %%8,2\n"
        "store i64 %%10,ptr @__ss_size\n"
        "%%11=call ptr @realloc(ptr %%p,i64 %%10)\n"
        "store ptr %%11,ptr @__ss_base\n"
        "br label %%ok\n"
        "ok:\n"
        "%%12=phi ptr[%%p,%%alloc],[%%11,%%grow]\n"
        "%%13=getelementptr i8,ptr %%12,i64 %%4\n"
        "store i64 %%7,ptr @__ss_ptr\n"
        "ret ptr %%13}\n"
    );
}


/// @brief Emit exception handling functions
///
static void emit_exception_functions(Codegen_Context *ctx)
{
    emit(ctx,
        "define linkonce_odr ptr @__ada_setjmp(){\n"
        "%%p=call ptr @malloc(i64 200)\n"
        "ret ptr %%p}\n"
        "define linkonce_odr void @__ada_push_handler(ptr %%h){\n"
        "%%1=load ptr,ptr @__eh_cur\n"
        "store ptr %%1,ptr %%h\n"
        "store ptr %%h,ptr @__eh_cur\n"
        "ret void}\n"
        "define linkonce_odr void @__ada_pop_handler(){\n"
        "%%1=load ptr,ptr @__eh_cur\n"
        "%%2=icmp eq ptr %%1,null\n"
        "br i1 %%2,label %%done,label %%pop\n"
        "pop:\n"
        "%%3=load ptr,ptr %%1\n"
        "store ptr %%3,ptr @__eh_cur\n"
        "br label %%done\n"
        "done:\n"
        "ret void}\n"
        "@.fmt_ue=linkonce_odr constant[25 x i8]c\"Unhandled exception: %%s\\0A\\00\"\n"
        "define linkonce_odr void @__ada_raise(ptr %%msg){\n"
        "store ptr %%msg,ptr @__ex_cur\n"
        "%%jb=load ptr,ptr @__eh_cur\n"
        "call void @longjmp(ptr %%jb,i32 1)\n"
        "ret void}\n"
    );
}


/// @brief Emit TEXT_IO runtime functions
///
static void emit_text_io_functions(Codegen_Context *ctx)
{
    emit(ctx,
        "@.fmt_d=linkonce_odr constant[5 x i8]c\"%%lld\\00\"\n"
        "@.fmt_s=linkonce_odr constant[3 x i8]c\"%%s\\00\"\n"
        "declare i32 @putchar(i32)\n"
        "declare i32 @getchar()\n"
        "define linkonce_odr void @__text_io_new_line(){\n"
        "call i32 @putchar(i32 10)\n"
        "ret void}\n"
        "define linkonce_odr void @__text_io_put_char(i64 %%c){\n"
        "%%1=trunc i64 %%c to i32\n"
        "call i32 @putchar(i32 %%1)\n"
        "ret void}\n"
        "define linkonce_odr void @__text_io_put(ptr %%s){\n"
        "entry:\n"
        "%%len=call i64 @strlen(ptr %%s)\n"
        "br label %%loop\n"
        "loop:\n"
        "%%i=phi i64[0,%%entry],[%%next,%%body]\n"
        "%%cmp=icmp slt i64 %%i,%%len\n"
        "br i1 %%cmp,label %%body,label %%done\n"
        "body:\n"
        "%%charptr=getelementptr i8,ptr %%s,i64 %%i\n"
        "%%ch8=load i8,ptr %%charptr\n"
        "%%ch=sext i8 %%ch8 to i32\n"
        "call i32 @putchar(i32 %%ch)\n"
        "%%next=add i64 %%i,1\n"
        "br label %%loop\n"
        "done:\n"
        "ret void}\n"
        "define linkonce_odr void @__text_io_put_line(ptr %%s){\n"
        "call void @__text_io_put(ptr %%s)\n"
        "call void @__text_io_new_line()\n"
        "ret void}\n"
        "define linkonce_odr void @__text_io_get_char(ptr %%p){\n"
        "%%1=call i32 @getchar()\n"
        "%%2=icmp eq i32 %%1,-1\n"
        "%%3=sext i32 %%1 to i64\n"
        "%%4=select i1 %%2,i64 0,i64 %%3\n"
        "store i64 %%4,ptr %%p\n"
        "ret void}\n"
        "define linkonce_odr void @__text_io_get_line(ptr %%b,ptr %%n){\n"
        "store i64 0,ptr %%n\n"
        "ret void}\n"
    );
}


/// @brief Emit attribute functions
///
static void emit_attribute_functions(Codegen_Context *ctx)
{
    emit(ctx,
        "define linkonce_odr i64 @__attr_PRED_INTEGER(i64 %%x){\n"
        "  %%t0 = sub i64 %%x, 1\n"
        "  ret i64 %%t0\n}\n"
        "define linkonce_odr i64 @__attr_SUCC_INTEGER(i64 %%x){\n"
        "  %%t0 = add i64 %%x, 1\n"
        "  ret i64 %%t0\n}\n"
        "define linkonce_odr i64 @__attr_POS_INTEGER(i64 %%x){\n"
        "  ret i64 %%x\n}\n"
        "define linkonce_odr i64 @__attr_VAL_INTEGER(i64 %%x){\n"
        "  ret i64 %%x\n}\n"
        "define linkonce_odr i64 @__attr_PRED_BOOLEAN(i64 %%x){\n"
        "  %%t0 = sub i64 %%x, 1\n"
        "  ret i64 %%t0\n}\n"
        "define linkonce_odr i64 @__attr_SUCC_BOOLEAN(i64 %%x){\n"
        "  %%t0 = add i64 %%x, 1\n"
        "  ret i64 %%t0\n}\n"
        "define linkonce_odr i64 @__attr_POS_BOOLEAN(i64 %%x){\n"
        "  ret i64 %%x\n}\n"
        "define linkonce_odr i64 @__attr_VAL_BOOLEAN(i64 %%x){\n"
        "  ret i64 %%x\n}\n"
    );
}


/// @brief Emit power and range check functions
///
static void emit_utility_functions(Codegen_Context *ctx)
{
    emit(ctx,
        "define linkonce_odr i64 @__ada_powi(i64 %%base,i64 %%exp){\n"
        "entry:\n"
        "%%result=alloca i64\n"
        "store i64 1,ptr %%result\n"
        "%%e=alloca i64\n"
        "store i64 %%exp,ptr %%e\n"
        "br label %%loop\n"
        "loop:\n"
        "%%ev=load i64,ptr %%e\n"
        "%%cmp=icmp sgt i64 %%ev,0\n"
        "br i1 %%cmp,label %%body,label %%done\n"
        "body:\n"
        "%%rv=load i64,ptr %%result\n"
        "%%nv=mul i64 %%rv,%%base\n"
        "store i64 %%nv,ptr %%result\n"
        "%%ev2=load i64,ptr %%e\n"
        "%%ev3=sub i64 %%ev2,1\n"
        "store i64 %%ev3,ptr %%e\n"
        "br label %%loop\n"
        "done:\n"
        "%%final=load i64,ptr %%result\n"
        "ret i64 %%final}\n"
        "define linkonce_odr void @__ada_check_range(i64 %%v,i64 %%lo,i64 %%hi){\n"
        "%%1=icmp sge i64 %%v,%%lo\n"
        "br i1 %%1,label %%ok1,label %%err\n"
        "ok1:\n"
        "%%2=icmp sle i64 %%v,%%hi\n"
        "br i1 %%2,label %%ok2,label %%err\n"
        "ok2:\n"
        "ret void\n"
        "err:\n"
        "call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n"
        "unreachable}\n"
        "define linkonce_odr void @__ada_delay(i64 %%us){\n"
        "%%t=trunc i64 %%us to i32\n"
        "%%r=call i32 @usleep(i32 %%t)\n"
        "ret void}\n"
    );
}


/// @brief Emit IMAGE and VALUE attribute functions
///
static void emit_image_value_functions(Codegen_Context *ctx)
{
    emit(ctx,
        "define linkonce_odr ptr @__ada_image_int(i64 %%v){\n"
        "%%buf=alloca[32 x i8]\n"
        "%%1=getelementptr[32 x i8],ptr %%buf,i64 0,i64 0\n"
        "%%fmt=getelementptr[5 x i8],ptr @.fmt_d,i64 0,i64 0\n"
        "%%2=call i32(ptr,ptr,...)@sprintf(ptr %%1,ptr %%fmt,i64 %%v)\n"
        "%%n=sext i32 %%2 to i64\n"
        "%%sz=add i64 %%n,1\n"
        "%%rsz=mul i64 %%sz,8\n"
        "%%r=call ptr @malloc(i64 %%rsz)\n"
        "store i64 %%n,ptr %%r\n"
        "br label %%loop\n"
        "loop:\n"
        "%%i=phi i64[0,%%0],[%%8,%%body]\n"
        "%%3=icmp slt i64 %%i,%%n\n"
        "br i1 %%3,label %%body,label %%done\n"
        "body:\n"
        "%%4=getelementptr[32 x i8],ptr %%buf,i64 0,i64 %%i\n"
        "%%5=load i8,ptr %%4\n"
        "%%6=sext i8 %%5 to i64\n"
        "%%7=add i64 %%i,1\n"
        "%%idx=getelementptr i64,ptr %%r,i64 %%7\n"
        "store i64 %%6,ptr %%idx\n"
        "%%8=add i64 %%i,1\n"
        "br label %%loop\n"
        "done:\n"
        "ret ptr %%r}\n"
        "declare i64 @strtoll(ptr,ptr,i32,...)\n"
        "define linkonce_odr i64 @__ada_value_int(ptr %%s){\n"
        "%%pn=load i64,ptr %%s\n"
        "%%buf=call ptr @malloc(i64 %%pn)\n"
        "br label %%copy\n"
        "copy:\n"
        "%%ci=phi i64[0,%%0],[%%next,%%cbody]\n"
        "%%1=icmp slt i64 %%ci,%%pn\n"
        "br i1 %%1,label %%cbody,label %%parse\n"
        "cbody:\n"
        "%%idx=add i64 %%ci,1\n"
        "%%sptr=getelementptr i64,ptr %%s,i64 %%idx\n"
        "%%charval=load i64,ptr %%sptr\n"
        "%%ch=trunc i64 %%charval to i8\n"
        "%%bptr=getelementptr i8,ptr %%buf,i64 %%ci\n"
        "store i8 %%ch,ptr %%bptr\n"
        "%%next=add i64 %%ci,1\n"
        "br label %%copy\n"
        "parse:\n"
        "%%null=getelementptr i8,ptr %%buf,i64 %%pn\n"
        "store i8 0,ptr %%null\n"
        "%%result=call i64(ptr,ptr,i32,...)@strtoll(ptr %%buf,ptr null,i32 10)\n"
        "call void @free(ptr %%buf)\n"
        "ret i64 %%result}\n"
        "define linkonce_odr ptr @__attr_IMAGE_INTEGER(i64 %%x){\n"
        "  %%t0 = call ptr @__ada_image_int(i64 %%x)\n"
        "  ret ptr %%t0\n}\n"
        "define linkonce_odr i64 @__attr_VALUE_INTEGER(ptr %%x){\n"
        "  %%t0 = call i64 @__ada_value_int(ptr %%x)\n"
        "  ret i64 %%t0\n}\n"
    );
}


/// @brief Emit predefined Boolean constants
///
static void emit_boolean_constants(Codegen_Context *ctx)
{
    emit(ctx,
        "@FALSE=linkonce_odr constant i64 0\n"
        "@TRUE=linkonce_odr constant i64 1\n"
    );
}


void Codegen_Emit_Prelude(Codegen_Context *ctx)
{
    emit_extern_decls(ctx);
    emit_string_helpers(ctx);
    emit_runtime_globals(ctx);
    emit_exception_constants(ctx);
    emit_ss_functions(ctx);
    emit_exception_functions(ctx);
    emit_text_io_functions(ctx);
    emit_attribute_functions(ctx);
    emit_utility_functions(ctx);
    emit_image_value_functions(ctx);
    emit_boolean_constants(ctx);
}


void Codegen_Emit_Epilogue(Codegen_Context *ctx, const char *main_name)
{
    if (main_name) {
        emit(ctx, "define i32 @main(){\n");
        emit(ctx, "  call void @__ada_ss_init()\n");
        emit(ctx, "  call void @\"%s\"()\n", main_name);
        emit(ctx, "  ret i32 0\n");
        emit(ctx, "}\n");
    }
}


///-----------------------------------------------------------------------------
///                   T Y P E   E M I S S I O N
///-----------------------------------------------------------------------------

const char *Codegen_Type_String(Type_Descriptor *type)
{
    if (!type) return "i64";

    switch (type->kind) {
        case TY_INTEGER:
        case TY_BOOLEAN:
        case TY_CHARACTER:
        case TY_ENUMERATION:
        case TY_UNIVERSAL_INT:
            return "i64";

        case TY_FLOAT:
        case TY_UNIVERSAL_REAL:
            return "double";

        case TY_ARRAY:
        case TY_RECORD:
        case TY_ACCESS:
        case TY_FILE:
            return "ptr";

        default:
            return "i64";
    }
}


void Codegen_Emit_Type_Def(Codegen_Context *ctx, Type_Descriptor *type)
{
    if (!type || type->kind != TY_RECORD) return;

    /// Emit record type definition
    emit(ctx, "%%%.*s = type { ", (int)type->name.length, type->name.data);

    /// Record uses components Node_Vector
    for (uint32_t i = 0; i < type->components.count; i++) {
        if (i > 0) emit(ctx, ", ");
        emit(ctx, "i64");  /// Simplified: all components as i64
    }

    emit(ctx, " }\n");
}


///-----------------------------------------------------------------------------
///                   E X P R E S S I O N   G E N E R A T I O N
///-----------------------------------------------------------------------------

/// Forward declarations
static Gen_Value gen_expr(Codegen_Context *ctx, AST_Node *expr);
static Gen_Value gen_name(Codegen_Context *ctx, AST_Node *name);


/// @brief Cast a value to a specific kind
///
static Gen_Value cast_value(Codegen_Context *ctx, Gen_Value v, CG_Value_Kind target)
{
    if (v.kind == target) return v;

    Gen_Value result = { Codegen_New_Temp(ctx), target };

    if (v.kind == CG_VK_INTEGER && target == CG_VK_FLOAT) {
        emit(ctx, "  %%t%d = sitofp i64 %%t%d to double\n", result.id, v.id);
    }
    else if (v.kind == CG_VK_FLOAT && target == CG_VK_INTEGER) {
        emit(ctx, "  %%t%d = fptosi double %%t%d to i64\n", result.id, v.id);
    }
    else if (v.kind == CG_VK_POINTER && target == CG_VK_INTEGER) {
        emit(ctx, "  %%t%d = ptrtoint ptr %%t%d to i64\n", result.id, v.id);
    }
    else {
        /// No conversion needed or unknown conversion
        return v;
    }

    return result;
}


/// @brief Generate code for a binary operation
///
static Gen_Value gen_binary(Codegen_Context *ctx, AST_Node *expr)
{
    Gen_Value left = gen_expr(ctx, expr->binary.left);
    Gen_Value right = gen_expr(ctx, expr->binary.right);

    /// Determine result type based on operands
    CG_Value_Kind result_kind = CG_VK_INTEGER;
    if (left.kind == CG_VK_FLOAT || right.kind == CG_VK_FLOAT) {
        result_kind = CG_VK_FLOAT;
        left = cast_value(ctx, left, CG_VK_FLOAT);
        right = cast_value(ctx, right, CG_VK_FLOAT);
    }

    Gen_Value result = { Codegen_New_Temp(ctx), result_kind };
    const char *op_str = NULL;

    Token_Kind op = expr->binary.op;

    if (result_kind == CG_VK_INTEGER) {
        switch (op) {
            case TK_PLUS:        op_str = "add"; break;
            case TK_MINUS:       op_str = "sub"; break;
            case TK_STAR:        op_str = "mul"; break;
            case TK_SLASH:       op_str = "sdiv"; break;
            case TK_MOD:         op_str = "srem"; break;
            case TK_REM:         op_str = "srem"; break;
            case TK_AND:         op_str = "and"; break;
            case TK_OR:          op_str = "or"; break;
            case TK_XOR:         op_str = "xor"; break;
            default: break;
        }

        if (op_str) {
            emit(ctx, "  %%t%d = %s i64 %%t%d, %%t%d\n",
                 result.id, op_str, left.id, right.id);
            return result;
        }
    }
    else {
        /// Floating-point operations
        switch (op) {
            case TK_PLUS:        op_str = "fadd"; break;
            case TK_MINUS:       op_str = "fsub"; break;
            case TK_STAR:        op_str = "fmul"; break;
            case TK_SLASH:       op_str = "fdiv"; break;
            default: break;
        }

        if (op_str) {
            emit(ctx, "  %%t%d = %s double %%t%d, %%t%d\n",
                 result.id, op_str, left.id, right.id);
            return result;
        }
    }

    /// Comparison operations (return i64 boolean)
    result.kind = CG_VK_INTEGER;
    const char *cmp_op = NULL;
    bool is_float_cmp = (left.kind == CG_VK_FLOAT);

    switch (op) {
        case TK_EQUAL:
            cmp_op = is_float_cmp ? "fcmp oeq" : "icmp eq";
            break;
        case TK_NOT_EQUAL:
            cmp_op = is_float_cmp ? "fcmp one" : "icmp ne";
            break;
        case TK_LESS_THAN:
            cmp_op = is_float_cmp ? "fcmp olt" : "icmp slt";
            break;
        case TK_LESS_EQUAL:
            cmp_op = is_float_cmp ? "fcmp ole" : "icmp sle";
            break;
        case TK_GREATER_THAN:
            cmp_op = is_float_cmp ? "fcmp ogt" : "icmp sgt";
            break;
        case TK_GREATER_EQUAL:
            cmp_op = is_float_cmp ? "fcmp oge" : "icmp sge";
            break;
        default:
            break;
    }

    if (cmp_op) {
        int cmp_temp = Codegen_New_Temp(ctx);
        const char *type_str = is_float_cmp ? "double" : "i64";
        emit(ctx, "  %%t%d = %s %s %%t%d, %%t%d\n",
             cmp_temp, cmp_op, type_str, left.id, right.id);
        emit(ctx, "  %%t%d = zext i1 %%t%d to i64\n", result.id, cmp_temp);
        return result;
    }

    /// Exponentiation
    if (op == TK_DOUBLE_STAR) {
        if (result_kind == CG_VK_INTEGER) {
            emit(ctx, "  %%t%d = call i64 @__ada_powi(i64 %%t%d, i64 %%t%d)\n",
                 result.id, left.id, right.id);
        } else {
            emit(ctx, "  %%t%d = call double @pow(double %%t%d, double %%t%d)\n",
                 result.id, left.id, right.id);
        }
        return result;
    }

    /// Fallback - shouldn't reach here
    emit(ctx, "  %%t%d = add i64 0, 0  ; unknown binary op\n", result.id);
    return result;
}


/// @brief Generate code for a unary operation
///
static Gen_Value gen_unary(Codegen_Context *ctx, AST_Node *expr)
{
    Gen_Value operand = gen_expr(ctx, expr->unary.operand);
    Gen_Value result = { Codegen_New_Temp(ctx), operand.kind };

    Token_Kind op = expr->unary.op;

    switch (op) {
        case TK_MINUS:
            if (operand.kind == CG_VK_FLOAT) {
                emit(ctx, "  %%t%d = fneg double %%t%d\n", result.id, operand.id);
            } else {
                emit(ctx, "  %%t%d = sub i64 0, %%t%d\n", result.id, operand.id);
            }
            break;

        case TK_PLUS:
            /// Unary plus is a no-op
            return operand;

        case TK_NOT:
            emit(ctx, "  %%t%d = xor i64 %%t%d, 1\n", result.id, operand.id);
            break;

        case TK_ABS:
            if (operand.kind == CG_VK_FLOAT) {
                emit(ctx, "  %%t%d = call double @llvm.fabs.f64(double %%t%d)\n",
                     result.id, operand.id);
            } else {
                /// Integer ABS: (x ^ (x >> 63)) - (x >> 63)
                int t1 = Codegen_New_Temp(ctx);
                int t2 = Codegen_New_Temp(ctx);
                emit(ctx, "  %%t%d = ashr i64 %%t%d, 63\n", t1, operand.id);
                emit(ctx, "  %%t%d = xor i64 %%t%d, %%t%d\n", t2, operand.id, t1);
                emit(ctx, "  %%t%d = sub i64 %%t%d, %%t%d\n", result.id, t2, t1);
            }
            break;

        default:
            emit(ctx, "  %%t%d = add i64 %%t%d, 0  ; unknown unary op\n",
                 result.id, operand.id);
            break;
    }

    return result;
}


/// @brief Generate code for an integer literal
///
static Gen_Value gen_integer(Codegen_Context *ctx, AST_Node *expr)
{
    Gen_Value result = { Codegen_New_Temp(ctx), CG_VK_INTEGER };
    emit(ctx, "  %%t%d = add i64 0, %lld\n", result.id, (long long)expr->integer_val);
    return result;
}


/// @brief Generate code for a real literal
///
static Gen_Value gen_real(Codegen_Context *ctx, AST_Node *expr)
{
    Gen_Value result = { Codegen_New_Temp(ctx), CG_VK_FLOAT };
    emit(ctx, "  %%t%d = fadd double 0.0, %f\n", result.id, expr->real_val);
    return result;
}


/// @brief Generate code for a string literal
///
static Gen_Value gen_string(Codegen_Context *ctx, AST_Node *expr)
{
    String_Slice str = expr->string_val;
    int str_id = ctx->string_counter++;

    /// Emit string constant
    emit(ctx, "@.str.%d = private constant [%u x i8] c\"",
         str_id, (unsigned)(str.length + 1));
    for (size_t i = 0; i < str.length; i++) {
        unsigned char c = str.data[i];
        if (c >= 32 && c < 127 && c != '"' && c != '\\') {
            emit(ctx, "%c", c);
        } else {
            emit(ctx, "\\%02X", c);
        }
    }
    emit(ctx, "\\00\"\n");

    /// Return pointer to string
    Gen_Value result = { Codegen_New_Temp(ctx), CG_VK_POINTER };
    emit(ctx, "  %%t%d = getelementptr [%u x i8], ptr @.str.%d, i64 0, i64 0\n",
         result.id, (unsigned)(str.length + 1), str_id);
    return result;
}


/// @brief Generate code for an identifier reference
///
static Gen_Value gen_identifier(Codegen_Context *ctx, AST_Node *expr)
{
    String_Slice name = expr->string_val;
    Symbol_Entry *sym = Symbol_Find(ctx->sem, name);

    Gen_Value result = { Codegen_New_Temp(ctx), CG_VK_INTEGER };

    if (sym && sym->kind == SK_ENUMERATION_LITERAL) {
        /// Enumeration literal - emit its position value
        emit(ctx, "  %%t%d = load i64, ptr @%.*s\n",
             result.id, (int)name.length, name.data);
    }
    else if (sym && sym->kind == SK_CONSTANT) {
        /// Constant - emit its value directly
        /// For now, load from global
        emit(ctx, "  %%t%d = load i64, ptr @%s\n",
             result.id, to_upper_cstr(name));
    }
    else {
        /// Variable - load from its storage location
        emit(ctx, "  %%t%d = load i64, ptr %%%.*s\n",
             result.id, (int)name.length, name.data);
    }

    return result;
}


/// @brief Generate code for a function call
///
static Gen_Value gen_call(Codegen_Context *ctx, AST_Node *expr)
{
    AST_Node *callee = expr->call.callee;
    Node_Vector *args = &expr->call.args;

    Gen_Value result = { Codegen_New_Temp(ctx), CG_VK_INTEGER };

    /// Get function name
    String_Slice func_name = {0};
    if (callee && callee->kind == N_ID) {
        func_name = callee->string_val;
    }

    /// Generate argument values
    int arg_temps[32] = {0};
    for (uint32_t i = 0; i < args->count && i < 32; i++) {
        Gen_Value arg = gen_expr(ctx, args->data[i]);
        arg_temps[i] = arg.id;
    }

    /// Emit call
    emit(ctx, "  %%t%d = call i64 @\"%.*s\"(",
         result.id, (int)func_name.length, func_name.data);

    for (uint32_t i = 0; i < args->count && i < 32; i++) {
        if (i > 0) emit(ctx, ", ");
        emit(ctx, "i64 %%t%d", arg_temps[i]);
    }

    emit(ctx, ")\n");

    return result;
}


/// @brief Main expression generation dispatcher
///
static Gen_Value gen_expr(Codegen_Context *ctx, AST_Node *expr)
{
    if (!expr) {
        Gen_Value null_val = { 0, CG_VK_INTEGER };
        return null_val;
    }

    switch (expr->kind) {
        case N_INT:
            return gen_integer(ctx, expr);

        case N_REAL:
            return gen_real(ctx, expr);

        case N_STR:
            return gen_string(ctx, expr);

        case N_ID:
            return gen_identifier(ctx, expr);

        case N_BIN:
            return gen_binary(ctx, expr);

        case N_UN:
            return gen_unary(ctx, expr);

        case N_CL:
            return gen_call(ctx, expr);

        default: {
            Gen_Value zero = { Codegen_New_Temp(ctx), CG_VK_INTEGER };
            emit(ctx, "  %%t%d = add i64 0, 0  ; unhandled expr kind %d\n",
                 zero.id, expr->kind);
            return zero;
        }
    }
}


Gen_Value Codegen_Expr(Codegen_Context *ctx, AST_Node *expr)
{
    return gen_expr(ctx, expr);
}


Gen_Value Codegen_Load(Codegen_Context *ctx, Gen_Value addr, Type_Descriptor *type)
{
    Gen_Value result = { Codegen_New_Temp(ctx), CG_VK_INTEGER };
    const char *type_str = Codegen_Type_String(type);

    if (type && type->kind == TY_FLOAT) {
        result.kind = CG_VK_FLOAT;
    }

    emit(ctx, "  %%t%d = load %s, ptr %%t%d\n", result.id, type_str, addr.id);
    return result;
}


void Codegen_Store(Codegen_Context *ctx, Gen_Value addr, Gen_Value value,
                   Type_Descriptor *type)
{
    const char *type_str = Codegen_Type_String(type);
    emit(ctx, "  store %s %%t%d, ptr %%t%d\n", type_str, value.id, addr.id);
}


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   G E N E R A T I O N
///-----------------------------------------------------------------------------

/// Forward declaration
static void gen_statement(Codegen_Context *ctx, AST_Node *stmt);


/// @brief Generate code for an assignment statement
///
static void gen_assignment(Codegen_Context *ctx, AST_Node *stmt)
{
    AST_Node *target = stmt->assignment.target;
    AST_Node *value = stmt->assignment.value;

    Gen_Value rhs = gen_expr(ctx, value);

    /// Get target variable name
    if (target && target->kind == N_ID) {
        String_Slice name = target->string_val;
        emit(ctx, "  store i64 %%t%d, ptr %%%.*s\n",
             rhs.id, (int)name.length, name.data);
    }
}


/// @brief Generate code for an if statement
///
static void gen_if(Codegen_Context *ctx, AST_Node *stmt)
{
    Gen_Value cond = gen_expr(ctx, stmt->if_stmt.condition);

    int then_label = Codegen_New_Label(ctx);
    int else_label = Codegen_New_Label(ctx);
    int end_label = Codegen_New_Label(ctx);

    /// Convert i64 to i1 for branch
    int cond_i1 = Codegen_New_Temp(ctx);
    emit(ctx, "  %%t%d = icmp ne i64 %%t%d, 0\n", cond_i1, cond.id);

    bool has_else = stmt->if_stmt.else_stmts.count > 0;
    Codegen_Emit_Cond_Branch(ctx, cond_i1, then_label,
                             has_else ? else_label : end_label);

    /// Then part
    Codegen_Emit_Label(ctx, then_label);
    for (uint32_t i = 0; i < stmt->if_stmt.then_stmts.count; i++) {
        gen_statement(ctx, stmt->if_stmt.then_stmts.data[i]);
    }
    Codegen_Emit_Branch(ctx, end_label);

    /// Else part
    if (has_else) {
        Codegen_Emit_Label(ctx, else_label);
        for (uint32_t i = 0; i < stmt->if_stmt.else_stmts.count; i++) {
            gen_statement(ctx, stmt->if_stmt.else_stmts.data[i]);
        }
        Codegen_Emit_Branch(ctx, end_label);
    }

    Codegen_Emit_Label(ctx, end_label);
}


/// @brief Generate code for a while loop
///
static void gen_loop(Codegen_Context *ctx, AST_Node *stmt)
{
    int test_label = Codegen_New_Label(ctx);
    int body_label = Codegen_New_Label(ctx);
    int end_label = Codegen_New_Label(ctx);

    /// Push loop labels for EXIT statement
    ctx->loop_labels[ctx->loop_depth++] = end_label;

    Codegen_Emit_Branch(ctx, test_label);
    Codegen_Emit_Label(ctx, test_label);

    /// Loop condition comes from iteration scheme (if any)
    AST_Node *iter = stmt->loop_stmt.iteration;
    if (iter) {
        /// Has iteration scheme - treat as condition
        Gen_Value cond = gen_expr(ctx, iter);
        int cond_i1 = Codegen_New_Temp(ctx);
        emit(ctx, "  %%t%d = icmp ne i64 %%t%d, 0\n", cond_i1, cond.id);
        Codegen_Emit_Cond_Branch(ctx, cond_i1, body_label, end_label);
    } else {
        /// Basic loop (no condition)
        Codegen_Emit_Branch(ctx, body_label);
    }

    Codegen_Emit_Label(ctx, body_label);

    /// Loop body
    for (uint32_t i = 0; i < stmt->loop_stmt.stmts.count; i++) {
        gen_statement(ctx, stmt->loop_stmt.stmts.data[i]);
    }

    Codegen_Emit_Branch(ctx, test_label);
    Codegen_Emit_Label(ctx, end_label);

    ctx->loop_depth--;
}


/// @brief Generate code for a for loop (via gen_loop since iteration is in iteration scheme)
///
static void gen_for_loop(Codegen_Context *ctx, AST_Node *stmt)
{
    /// For loops are handled via gen_loop with iteration scheme
    /// The iteration scheme contains the loop variable and range
    /// For now, just emit a basic loop
    gen_loop(ctx, stmt);
}


/// @brief Generate code for a return statement
///
static void gen_return(Codegen_Context *ctx, AST_Node *stmt)
{
    if (stmt->return_stmt.value) {
        Gen_Value val = gen_expr(ctx, stmt->return_stmt.value);
        emit(ctx, "  ret i64 %%t%d\n", val.id);
    } else {
        emit(ctx, "  ret void\n");
    }
}


/// @brief Generate code for a procedure call statement
///
static void gen_proc_call(Codegen_Context *ctx, AST_Node *stmt)
{
    AST_Node *callee = stmt->call.callee;
    Node_Vector *args = &stmt->call.args;

    String_Slice proc_name = {0};
    if (callee && callee->kind == N_ID) {
        proc_name = callee->string_val;
    }

    /// Generate argument values
    int arg_temps[32] = {0};
    for (uint32_t i = 0; i < args->count && i < 32; i++) {
        Gen_Value arg = gen_expr(ctx, args->data[i]);
        arg_temps[i] = arg.id;
    }

    /// Emit call (void return)
    emit(ctx, "  call void @\"%.*s\"(", (int)proc_name.length, proc_name.data);

    for (uint32_t i = 0; i < args->count && i < 32; i++) {
        if (i > 0) emit(ctx, ", ");
        emit(ctx, "i64 %%t%d", arg_temps[i]);
    }

    emit(ctx, ")\n");
}


/// @brief Generate code for null statement
///
static void gen_null(Codegen_Context *ctx)
{
    /// Emit a no-op
    (void)ctx;
}


/// @brief Generate code for exit statement
///
static void gen_exit(Codegen_Context *ctx, AST_Node *stmt)
{
    if (ctx->loop_depth > 0) {
        int exit_label = ctx->loop_labels[ctx->loop_depth - 1];

        if (stmt->exit_stmt.condition) {
            Gen_Value cond = gen_expr(ctx, stmt->exit_stmt.condition);
            int cond_i1 = Codegen_New_Temp(ctx);
            int cont_label = Codegen_New_Label(ctx);

            emit(ctx, "  %%t%d = icmp ne i64 %%t%d, 0\n", cond_i1, cond.id);
            Codegen_Emit_Cond_Branch(ctx, cond_i1, exit_label, cont_label);
            Codegen_Emit_Label(ctx, cont_label);
        } else {
            Codegen_Emit_Branch(ctx, exit_label);
        }
    }
}


/// @brief Main statement generation dispatcher
///
static void gen_statement(Codegen_Context *ctx, AST_Node *stmt)
{
    if (!stmt) return;

    switch (stmt->kind) {
        case N_AS:
            gen_assignment(ctx, stmt);
            break;

        case N_IF:
            gen_if(ctx, stmt);
            break;

        case N_LP:
            /// Use gen_loop for all loops - iteration scheme determines type
            gen_loop(ctx, stmt);
            break;

        case N_RT:
            gen_return(ctx, stmt);
            break;

        case N_CLT:  /// Procedure call
            gen_proc_call(ctx, stmt);
            break;

        case N_CL:   /// Function call as statement
            gen_expr(ctx, stmt);
            break;

        case N_NULL:
            gen_null(ctx);
            break;

        case N_EX:
            gen_exit(ctx, stmt);
            break;

        case N_BL:
            /// Block statement - process declarations then statements
            for (uint32_t i = 0; i < stmt->block_stmt.decls.count; i++) {
                Codegen_Declaration(ctx, stmt->block_stmt.decls.data[i]);
            }
            for (uint32_t i = 0; i < stmt->block_stmt.stmts.count; i++) {
                gen_statement(ctx, stmt->block_stmt.stmts.data[i]);
            }
            break;

        default:
            emit(ctx, "  ; unhandled statement kind %d\n", stmt->kind);
            break;
    }
}


void Codegen_Statement(Codegen_Context *ctx, AST_Node *stmt)
{
    gen_statement(ctx, stmt);
}


void Codegen_Statement_List(Codegen_Context *ctx, Node_Vector *stmts)
{
    for (uint32_t i = 0; i < stmts->count; i++) {
        gen_statement(ctx, stmts->data[i]);
    }
}


///-----------------------------------------------------------------------------
///                   D E C L A R A T I O N   G E N E R A T I O N
///-----------------------------------------------------------------------------

void Codegen_Declaration(Codegen_Context *ctx, AST_Node *decl)
{
    if (!decl) return;

    switch (decl->kind) {
        case N_OD: {
            /// Object declaration - allocate local variables
            /// names is a Node_Vector of identifiers
            for (uint32_t j = 0; j < decl->object_decl.names.count; j++) {
                AST_Node *name_node = decl->object_decl.names.data[j];
                if (!name_node) continue;
                String_Slice name = name_node->string_val;
                emit(ctx, "  %%%.*s = alloca i64\n",
                     (int)name.length, name.data);

                /// Initialize if there's an initial value
                if (decl->object_decl.init_value) {
                    Gen_Value init = gen_expr(ctx, decl->object_decl.init_value);
                    emit(ctx, "  store i64 %%t%d, ptr %%%.*s\n",
                         init.id, (int)name.length, name.data);
                }
            }
            break;
        }

        case N_TD:
            /// Type declaration - type definitions handled at global level
            /// The type_def member is an AST_Node, not Type_Descriptor
            break;

        case N_SD:
            /// Subtype declaration - no code generation needed
            break;

        default:
            emit(ctx, "  ; unhandled declaration kind %d\n", decl->kind);
            break;
    }
}


void Codegen_Declarative_Part(Codegen_Context *ctx, Node_Vector *decls)
{
    for (uint32_t i = 0; i < decls->count; i++) {
        Codegen_Declaration(ctx, decls->data[i]);
    }
}


///-----------------------------------------------------------------------------
///                   S U B P R O G R A M   G E N E R A T I O N
///-----------------------------------------------------------------------------

void Codegen_Subprogram_Body(Codegen_Context *ctx, AST_Node *body)
{
    if (!body) return;

    AST_Node *spec = body->subprog_body.spec;
    if (!spec) return;

    String_Slice name = spec->subprog_spec.name;
    bool is_function = (spec->kind == N_FS);
    Node_Vector *params = &spec->subprog_spec.params;

    /// Emit function signature
    emit(ctx, "define linkonce_odr %s @\"%.*s\"(",
         is_function ? "i64" : "void",
         (int)name.length, name.data);

    /// Emit parameters
    for (uint32_t i = 0; i < params->count; i++) {
        if (i > 0) emit(ctx, ", ");
        AST_Node *param = params->data[i];
        emit(ctx, "i64 %%%.*s.arg",
             (int)param->param.param_name.length,
             param->param.param_name.data);
    }

    emit(ctx, ") {\n");

    /// Copy parameters to local variables
    for (uint32_t i = 0; i < params->count; i++) {
        AST_Node *param = params->data[i];
        String_Slice pname = param->param.param_name;
        emit(ctx, "  %%%.*s = alloca i64\n", (int)pname.length, pname.data);
        emit(ctx, "  store i64 %%%.*s.arg, ptr %%%.*s\n",
             (int)pname.length, pname.data,
             (int)pname.length, pname.data);
    }

    /// Emit declarations
    Codegen_Declarative_Part(ctx, &body->subprog_body.decls);

    /// Emit statements
    Codegen_Statement_List(ctx, &body->subprog_body.stmts);

    /// Emit default return if function hasn't returned
    if (is_function) {
        emit(ctx, "  ret i64 0\n");
    } else {
        emit(ctx, "  ret void\n");
    }

    emit(ctx, "}\n");
}


void Codegen_Subprogram_Decl(Codegen_Context *ctx, AST_Node *spec)
{
    if (!spec) return;

    String_Slice name = spec->subprog_spec.name;
    bool is_function = (spec->kind == N_FS);
    Node_Vector *params = &spec->subprog_spec.params;

    emit(ctx, "declare %s @\"%.*s\"(",
         is_function ? "i64" : "void",
         (int)name.length, name.data);

    for (uint32_t i = 0; i < params->count; i++) {
        if (i > 0) emit(ctx, ", ");
        emit(ctx, "i64");
    }

    emit(ctx, ")\n");
}


///-----------------------------------------------------------------------------
///                   P A C K A G E   G E N E R A T I O N
///-----------------------------------------------------------------------------

void Codegen_Package_Spec(Codegen_Context *ctx, AST_Node *spec)
{
    if (!spec) return;

    /// Process visible declarations
    for (uint32_t i = 0; i < spec->package_spec.visible_decls.count; i++) {
        AST_Node *decl = spec->package_spec.visible_decls.data[i];

        if (decl->kind == N_PS || decl->kind == N_FS) {
            Codegen_Subprogram_Decl(ctx, decl);
        }
    }
}


void Codegen_Package_Body(Codegen_Context *ctx, AST_Node *body)
{
    if (!body) return;

    /// Process declarations
    for (uint32_t i = 0; i < body->package_body.decls.count; i++) {
        AST_Node *decl = body->package_body.decls.data[i];

        if (decl->kind == N_PB || decl->kind == N_FB) {
            Codegen_Subprogram_Body(ctx, decl);
        }
    }

    /// Emit elaboration code if there are statements
    if (body->package_body.stmts.count > 0) {
        String_Slice name = body->package_body.name;
        emit(ctx, "define void @\"%.*s__elab\"() {\n",
             (int)name.length, name.data);
        Codegen_Statement_List(ctx, &body->package_body.stmts);
        emit(ctx, "  ret void\n}\n");

        /// Register as global constructor
        emit(ctx, "@llvm.global_ctors=appending global[1 x {i32,ptr,ptr}]"
                  "[{i32,ptr,ptr}{i32 65535,ptr @\"%.*s__elab\",ptr null}]\n",
             (int)name.length, name.data);
    }
}


///-----------------------------------------------------------------------------
///                   C O M P I L A T I O N   U N I T
///-----------------------------------------------------------------------------

void Codegen_Compilation_Unit(Codegen_Context *ctx, AST_Node *unit)
{
    if (!unit || unit->kind != N_CU) return;

    /// Emit prelude
    Codegen_Emit_Prelude(ctx);

    /// Find main procedure name
    const char *main_name = NULL;

    /// Process all library units
    for (uint32_t i = 0; i < unit->comp_unit.units.count; i++) {
        AST_Node *lib_unit = unit->comp_unit.units.data[i];

        switch (lib_unit->kind) {
            case N_PB:
                /// Procedure body - potential main
                Codegen_Subprogram_Body(ctx, lib_unit);
                if (lib_unit->subprog_body.spec) {
                    AST_Node *spec = lib_unit->subprog_body.spec;
                    if (spec->subprog_spec.params.count == 0) {
                        /// Parameterless procedure is main candidate
                        static char main_buf[256];
                        snprintf(main_buf, sizeof(main_buf), "%.*s",
                                 (int)spec->subprog_spec.name.length,
                                 spec->subprog_spec.name.data);
                        main_name = main_buf;
                    }
                }
                break;

            case N_FB:
                Codegen_Subprogram_Body(ctx, lib_unit);
                break;

            case N_PKS:
                Codegen_Package_Spec(ctx, lib_unit);
                break;

            case N_PKB:
                Codegen_Package_Body(ctx, lib_unit);
                break;

            default:
                emit(ctx, "; unhandled library unit kind %d\n", lib_unit->kind);
                break;
        }
    }

    /// Emit epilogue with main function
    Codegen_Emit_Epilogue(ctx, main_name);
}


///-----------------------------------------------------------------------------
///                                  E N D                                    --
///-----------------------------------------------------------------------------
