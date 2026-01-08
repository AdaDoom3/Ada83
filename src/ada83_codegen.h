///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                    L L V M   I R   C O D E   G E N E R A T O R            --
///                                                                           --
///  This module generates LLVM IR from the Ada83 AST. It produces textual    --
///  LLVM IR that can be linked with the runtime system and executed using    --
///  llvm-link and lli.                                                       --
///                                                                           --
///  The code generator supports:                                             --
///    - All Ada83 types (scalar, composite, access)                          --
///    - All expressions and operators                                        --
///    - All statements (if, case, loop, etc.)                                --
///    - Subprograms (procedures and functions)                               --
///    - Packages (specifications and bodies)                                 --
///    - Exception handling (raise, begin..exception..end)                    --
///    - Tasking (task types, entries, accept, select)                        --
///                                                                           --
///  LLVM IR representation:                                                  --
///    - INTEGER types -> i64                                                 --
///    - FLOAT types -> double                                                --
///    - BOOLEAN -> i64 (0=FALSE, 1=TRUE)                                     --
///    - CHARACTER -> i64 (8-bit value stored in 64-bit)                      --
///    - Arrays -> {ptr, {i64, i64}} (data pointer + bounds)                  --
///    - Records -> named struct types                                        --
///    - Access types -> ptr                                                  --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_CODEGEN_H
#define ADA83_CODEGEN_H

#include "ada83_common.h"
#include "ada83_ast.h"
#include "ada83_types.h"
#include "ada83_symbols.h"

#include <stdio.h>


///-----------------------------------------------------------------------------
///                   V A L U E   R E P R E S E N T A T I O N
///-----------------------------------------------------------------------------

/// @brief Value kinds for code generation
typedef enum {
    CG_VK_INTEGER,     ///< i64 integer value
    CG_VK_FLOAT,       ///< double floating-point value
    CG_VK_POINTER,     ///< ptr (pointer/address)
    CG_VK_VOID         ///< No value (statement result)
} CG_Value_Kind;


/// @brief Generated value reference
typedef struct {
    int id;             ///< Temporary ID (%t<id>)
    CG_Value_Kind kind; ///< Value kind
} Gen_Value;


///-----------------------------------------------------------------------------
///                   G E N E R A T O R   C O N T E X T
///-----------------------------------------------------------------------------

/// @brief Code generator state
typedef struct {
    FILE *output;               ///< Output file for LLVM IR
    Semantic_Context *sem;      ///< Semantic context (symbol tables)

    /// Counter state
    int temp_counter;           ///< Next temporary number
    int label_counter;          ///< Next label number
    int string_counter;         ///< Next string literal number

    /// Current scope
    int loop_depth;             ///< Current loop nesting depth
    int loop_labels[64];        ///< Loop exit label stack

    /// Exception handling
    int handler_depth;          ///< Exception handler nesting depth

    /// Deferred declarations
    struct {
        char **data;
        uint32_t count;
        uint32_t capacity;
    } forward_decls;

    /// String literals pool
    struct {
        char **data;
        uint32_t count;
        uint32_t capacity;
    } string_pool;

} Codegen_Context;


///-----------------------------------------------------------------------------
///                   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

/// @brief Initialize the code generator
/// @param ctx Code generator context to initialize
/// @param output Output file for LLVM IR
/// @param sem Semantic context from analysis phase
///
void Codegen_Init(Codegen_Context *ctx, FILE *output, Semantic_Context *sem);


/// @brief Cleanup code generator resources
/// @param ctx Code generator context
///
void Codegen_Cleanup(Codegen_Context *ctx);


///-----------------------------------------------------------------------------
///                   P R E L U D E   A N D   E P I L O G U E
///-----------------------------------------------------------------------------

/// @brief Emit LLVM IR prelude (declarations, runtime functions)
/// @param ctx Code generator context
///
void Codegen_Emit_Prelude(Codegen_Context *ctx);


/// @brief Emit LLVM IR epilogue (main function, etc.)
/// @param ctx Code generator context
/// @param main_name Name of main procedure (or NULL)
///
void Codegen_Emit_Epilogue(Codegen_Context *ctx, const char *main_name);


///-----------------------------------------------------------------------------
///                   T Y P E   E M I S S I O N
///-----------------------------------------------------------------------------

/// @brief Get LLVM IR type string for an Ada type
/// @param type Ada type descriptor
/// @return LLVM IR type string (e.g., "i64", "double", "ptr")
///
const char *Codegen_Type_String(Type_Descriptor *type);


/// @brief Emit type definition for a composite type
/// @param ctx Code generator context
/// @param type Type to emit definition for
///
void Codegen_Emit_Type_Def(Codegen_Context *ctx, Type_Descriptor *type);


///-----------------------------------------------------------------------------
///                   E X P R E S S I O N   G E N E R A T I O N
///-----------------------------------------------------------------------------

/// @brief Generate code for an expression
/// @param ctx Code generator context
/// @param expr Expression AST node
/// @return Generated value reference
///
Gen_Value Codegen_Expr(Codegen_Context *ctx, AST_Node *expr);


/// @brief Generate code to load a value from an address
/// @param ctx Code generator context
/// @param addr Address value
/// @param type Type of value to load
/// @return Loaded value reference
///
Gen_Value Codegen_Load(Codegen_Context *ctx, Gen_Value addr, Type_Descriptor *type);


/// @brief Generate code to store a value to an address
/// @param ctx Code generator context
/// @param addr Address to store to
/// @param value Value to store
/// @param type Type of value
///
void Codegen_Store(Codegen_Context *ctx, Gen_Value addr, Gen_Value value,
                   Type_Descriptor *type);


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   G E N E R A T I O N
///-----------------------------------------------------------------------------

/// @brief Generate code for a statement
/// @param ctx Code generator context
/// @param stmt Statement AST node
///
void Codegen_Statement(Codegen_Context *ctx, AST_Node *stmt);


/// @brief Generate code for a sequence of statements
/// @param ctx Code generator context
/// @param stmts Statement list
///
void Codegen_Statement_List(Codegen_Context *ctx, Node_Vector *stmts);


///-----------------------------------------------------------------------------
///                   D E C L A R A T I O N   G E N E R A T I O N
///-----------------------------------------------------------------------------

/// @brief Generate code for a declaration
/// @param ctx Code generator context
/// @param decl Declaration AST node
///
void Codegen_Declaration(Codegen_Context *ctx, AST_Node *decl);


/// @brief Generate code for a declarative part
/// @param ctx Code generator context
/// @param decls Declaration list
///
void Codegen_Declarative_Part(Codegen_Context *ctx, Node_Vector *decls);


///-----------------------------------------------------------------------------
///                   S U B P R O G R A M   G E N E R A T I O N
///-----------------------------------------------------------------------------

/// @brief Generate code for a subprogram body
/// @param ctx Code generator context
/// @param body Subprogram body AST node
///
void Codegen_Subprogram_Body(Codegen_Context *ctx, AST_Node *body);


/// @brief Generate code for a subprogram specification (forward declaration)
/// @param ctx Code generator context
/// @param spec Subprogram specification AST node
///
void Codegen_Subprogram_Decl(Codegen_Context *ctx, AST_Node *spec);


///-----------------------------------------------------------------------------
///                   P A C K A G E   G E N E R A T I O N
///-----------------------------------------------------------------------------

/// @brief Generate code for a package specification
/// @param ctx Code generator context
/// @param spec Package specification AST node
///
void Codegen_Package_Spec(Codegen_Context *ctx, AST_Node *spec);


/// @brief Generate code for a package body
/// @param ctx Code generator context
/// @param body Package body AST node
///
void Codegen_Package_Body(Codegen_Context *ctx, AST_Node *body);


///-----------------------------------------------------------------------------
///                   C O M P I L A T I O N   U N I T
///-----------------------------------------------------------------------------

/// @brief Generate code for a complete compilation unit
/// @param ctx Code generator context
/// @param unit Compilation unit AST node
///
void Codegen_Compilation_Unit(Codegen_Context *ctx, AST_Node *unit);


///-----------------------------------------------------------------------------
///                   H E L P E R   F U N C T I O N S
///-----------------------------------------------------------------------------

/// @brief Allocate a new temporary
/// @param ctx Code generator context
/// @return New temporary ID
///
static inline int Codegen_New_Temp(Codegen_Context *ctx)
{
    return ctx->temp_counter++;
}


/// @brief Allocate a new label
/// @param ctx Code generator context
/// @return New label ID
///
static inline int Codegen_New_Label(Codegen_Context *ctx)
{
    return ctx->label_counter++;
}


/// @brief Emit a label
/// @param ctx Code generator context
/// @param label Label ID
///
static inline void Codegen_Emit_Label(Codegen_Context *ctx, int label)
{
    fprintf(ctx->output, "L%d:\n", label);
}


/// @brief Emit an unconditional branch
/// @param ctx Code generator context
/// @param label Target label ID
///
static inline void Codegen_Emit_Branch(Codegen_Context *ctx, int label)
{
    fprintf(ctx->output, "  br label %%L%d\n", label);
}


/// @brief Emit a conditional branch
/// @param ctx Code generator context
/// @param cond Condition temporary ID
/// @param true_label Label if true
/// @param false_label Label if false
///
static inline void Codegen_Emit_Cond_Branch(Codegen_Context *ctx, int cond,
                                            int true_label, int false_label)
{
    fprintf(ctx->output, "  br i1 %%t%d, label %%L%d, label %%L%d\n",
            cond, true_label, false_label);
}


#endif // ADA83_CODEGEN_H
