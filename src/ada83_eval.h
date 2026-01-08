///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///          S E M A N T I C   A N A L Y S I S   A N D   E V A L U A T I O N  --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module implements semantic analysis and evaluation (interpretation)  --
///  of Ada83 programs. It combines:                                          --
///                                                                           --
///    - Name resolution (binding identifiers to declarations)                --
///    - Type checking (verifying type compatibility)                         --
///    - Constraint checking (range, index, discriminant checks)              --
///    - Expression evaluation (computing values at runtime)                  --
///    - Statement execution (assignment, control flow, etc.)                 --
///                                                                           --
///  The design follows GNAT's Sem and Exp packages conceptually.             --
///  Reference: Ada83 LRM Chapters 4-6, 8-11                                  --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_EVAL_H
#define ADA83_EVAL_H

#include "ada83_common.h"
#include "ada83_ast.h"
#include "ada83_types.h"
#include "ada83_symbols.h"


///-----------------------------------------------------------------------------
///                   R U N T I M E   V A L U E
///-----------------------------------------------------------------------------
///
///  Runtime values are represented in a tagged union. The tag (kind) indicates
///  which field of the union is active. This corresponds to the runtime
///  representation of Ada objects.
///
///  Values can be:
///    - Integers (including Boolean, Character, enumeration)
///    - Reals (floating point and fixed point)
///    - Access (pointers)
///    - Records (aggregate of named components)
///    - Arrays (indexed collection)
///    - Tasks (concurrent units)
///
///-----------------------------------------------------------------------------

typedef enum {
    VK_NONE = 0,        /// Uninitialized
    VK_INTEGER,         /// Integer types (incl. Boolean, Character, Enum)
    VK_REAL,            /// Floating and fixed point
    VK_ACCESS,          /// Access (pointer) type
    VK_ARRAY,           /// Array value
    VK_RECORD,          /// Record value
    VK_STRING,          /// String value (special array of Character)
    VK_FILE,            /// File handle
    VK_TASK,            /// Task value
    VK_EXCEPTION        /// Exception occurrence
} Value_Kind;


typedef struct Runtime_Value Runtime_Value;

typedef struct {
    Runtime_Value  *elements;   /// Array of elements
    int64_t         low;        /// Lower bound
    int64_t         high;       /// Upper bound
    uint32_t        count;      /// Number of elements
} Array_Value;

typedef struct {
    String_Slice    name;       /// Component name
    Runtime_Value  *value;      /// Component value
} Record_Component_Value;

typedef struct {
    Record_Component_Value *components;  /// Component values
    uint32_t                count;       /// Number of components
} Record_Value;

struct Runtime_Value {
    Value_Kind       kind;      /// Value kind tag
    Type_Descriptor *type;      /// Associated type
    union {
        int64_t       integer;  /// Integer/Boolean/Character/Enum value
        double        real;     /// Float/Fixed value
        void         *access;   /// Access (pointer) value
        Array_Value   array;    /// Array value
        Record_Value  record;   /// Record value
        String_Slice  string;   /// String value
        FILE         *file;     /// File handle
        void         *task;     /// Task control block
        String_Slice  exception;/// Exception name
    };
};


///-----------------------------------------------------------------------------
///                   E V A L U A T I O N   C O N T E X T
///-----------------------------------------------------------------------------
///
///  The evaluation context maintains runtime state during program execution.
///  This includes:
///    - Variable bindings (symbol -> value mapping)
///    - Call stack (for subprogram invocation)
///    - Exception handling state
///    - I/O state
///
///  This corresponds to the runtime environment in a traditional interpreter.
///
///-----------------------------------------------------------------------------

typedef struct {
    Symbol_Entry   *symbol;     /// Symbol being bound
    Runtime_Value   value;      /// Current value
} Binding;

typedef struct {
    Binding        *bindings;   /// Variable bindings
    uint32_t        count;      /// Number of bindings
    uint32_t        capacity;   /// Allocated capacity
} Binding_Frame;

typedef struct {
    AST_Node       *subprogram; /// Subprogram being executed
    Binding_Frame   locals;     /// Local variable bindings
    Runtime_Value   return_value; /// Return value (for functions)
    bool            has_returned; /// True if RETURN executed
} Call_Frame;

typedef struct {
    Semantic_Context  *sem;         /// Semantic context (symbol table)

    //-------------------------------------------------------------------------
    // Call Stack
    //-------------------------------------------------------------------------
    Call_Frame        *call_stack;  /// Stack of active calls
    uint32_t           call_depth;  /// Current call depth
    uint32_t           call_capacity; /// Allocated stack size

    //-------------------------------------------------------------------------
    // Control Flow State
    //-------------------------------------------------------------------------
    bool               exit_loop;   /// EXIT statement executed
    String_Slice       exit_label;  /// Target loop label (if any)
    bool               goto_active; /// GOTO statement executed
    String_Slice       goto_label;  /// Target label

    //-------------------------------------------------------------------------
    // Exception Handling
    //-------------------------------------------------------------------------
    jmp_buf           *exception_handler; /// Current exception handler
    String_Slice       current_exception; /// Currently raised exception
    bool               exception_raised;  /// Exception in progress

    //-------------------------------------------------------------------------
    // I/O State
    //-------------------------------------------------------------------------
    FILE              *current_input;   /// Current input file
    FILE              *current_output;  /// Current output file

    //-------------------------------------------------------------------------
    // Global Bindings
    //-------------------------------------------------------------------------
    Binding_Frame      globals;     /// Global variable bindings

} Eval_Context;


///-----------------------------------------------------------------------------
///                   C O N T E X T   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

/// @brief Initialize an evaluation context
/// @param ctx  Evaluation context to initialize
/// @param sem  Semantic context (symbol table)
///
/// Sets up runtime state with predefined values for standard types.
///
void Eval_Init(Eval_Context *ctx, Semantic_Context *sem);


/// @brief Clean up evaluation context
/// @param ctx Evaluation context to clean up
///
void Eval_Cleanup(Eval_Context *ctx);


///-----------------------------------------------------------------------------
///                   S E M A N T I C   A N A L Y S I S
///-----------------------------------------------------------------------------
///
///  Semantic analysis resolves names, checks types, and annotates the AST
///  with type information. This prepares the AST for evaluation.
///
///-----------------------------------------------------------------------------

/// @brief Resolve a type from a subtype indication
/// @param sem  Semantic context
/// @param node Subtype indication or type name AST node
/// @return Resolved type descriptor
///
Type_Descriptor *Resolve_Subtype(Semantic_Context *sem, AST_Node *node);


/// @brief Analyze and type-check an expression
/// @param sem      Semantic context
/// @param expr     Expression AST node
/// @param expected Expected type (or NULL if any)
///
/// Resolves names, checks types, and sets expr->type.
///
void Analyze_Expression(Semantic_Context *sem, AST_Node *expr,
                       Type_Descriptor *expected);


/// @brief Analyze a statement
/// @param sem  Semantic context
/// @param stmt Statement AST node
///
/// Checks statement validity and analyzes subexpressions.
///
void Analyze_Statement(Semantic_Context *sem, AST_Node *stmt);


/// @brief Analyze a declaration
/// @param sem  Semantic context
/// @param decl Declaration AST node
///
/// Adds declarations to symbol table and analyzes initializers.
///
void Analyze_Declaration(Semantic_Context *sem, AST_Node *decl);


/// @brief Analyze a compilation unit
/// @param sem  Semantic context
/// @param unit Compilation unit AST node
///
/// Entry point for semantic analysis of a complete program.
///
void Analyze_Compilation_Unit(Semantic_Context *sem, AST_Node *unit);


///-----------------------------------------------------------------------------
///                   E X P R E S S I O N   E V A L U A T I O N
///-----------------------------------------------------------------------------

/// @brief Evaluate an expression to a runtime value
/// @param ctx  Evaluation context
/// @param expr Expression AST node (must be analyzed)
/// @return Runtime value
///
Runtime_Value Eval_Expression(Eval_Context *ctx, AST_Node *expr);


/// @brief Evaluate expression as integer
/// @param ctx  Evaluation context
/// @param expr Expression AST node
/// @return Integer value
///
/// Convenience function that asserts integer type.
///
int64_t Eval_Integer(Eval_Context *ctx, AST_Node *expr);


/// @brief Evaluate expression as real
/// @param ctx  Evaluation context
/// @param expr Expression AST node
/// @return Real value
///
double Eval_Real(Eval_Context *ctx, AST_Node *expr);


/// @brief Evaluate expression as boolean
/// @param ctx  Evaluation context
/// @param expr Expression AST node
/// @return Boolean value
///
bool Eval_Boolean(Eval_Context *ctx, AST_Node *expr);


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   E X E C U T I O N
///-----------------------------------------------------------------------------

/// @brief Execute a statement
/// @param ctx  Evaluation context
/// @param stmt Statement AST node
///
void Exec_Statement(Eval_Context *ctx, AST_Node *stmt);


/// @brief Execute a sequence of statements
/// @param ctx   Evaluation context
/// @param stmts Statement vector
///
void Exec_Statements(Eval_Context *ctx, Node_Vector *stmts);


/// @brief Execute a subprogram call
/// @param ctx       Evaluation context
/// @param subprogram Subprogram body AST node
/// @param arguments  Actual arguments
/// @return Return value (for functions)
///
Runtime_Value Exec_Call(Eval_Context *ctx, AST_Node *subprogram,
                       Node_Vector *arguments);


///-----------------------------------------------------------------------------
///                   D E C L A R A T I O N   E L A B O R A T I O N
///-----------------------------------------------------------------------------

/// @brief Elaborate a declaration (create binding)
/// @param ctx  Evaluation context
/// @param decl Declaration AST node
///
void Elaborate_Declaration(Eval_Context *ctx, AST_Node *decl);


/// @brief Elaborate a compilation unit
/// @param ctx  Evaluation context
/// @param unit Compilation unit AST node
///
void Elaborate_Compilation_Unit(Eval_Context *ctx, AST_Node *unit);


///-----------------------------------------------------------------------------
///                   P R E D E F I N E D   O P E R A T I O N S
///-----------------------------------------------------------------------------
///
///  These implement the predefined operations for standard types as
///  specified in Ada83 LRM Chapter 4 and Annex A.
///
///-----------------------------------------------------------------------------

/// @brief Evaluate a binary operation
/// @param ctx  Evaluation context
/// @param op   Operator token kind
/// @param left Left operand value
/// @param right Right operand value
/// @param result_type Result type
/// @return Result value
///
Runtime_Value Eval_Binary_Op(Eval_Context *ctx, Token_Kind op,
                            Runtime_Value left, Runtime_Value right,
                            Type_Descriptor *result_type);


/// @brief Evaluate a unary operation
/// @param ctx    Evaluation context
/// @param op     Operator token kind
/// @param operand Operand value
/// @param result_type Result type
/// @return Result value
///
Runtime_Value Eval_Unary_Op(Eval_Context *ctx, Token_Kind op,
                           Runtime_Value operand,
                           Type_Descriptor *result_type);


/// @brief Evaluate an attribute
/// @param ctx      Evaluation context
/// @param prefix   Prefix value/type
/// @param attr     Attribute name
/// @param args     Attribute arguments
/// @return Attribute value
///
Runtime_Value Eval_Attribute(Eval_Context *ctx, AST_Node *prefix,
                            String_Slice attr, Node_Vector *args);


///-----------------------------------------------------------------------------
///                   R U N T I M E   C H E C K S
///-----------------------------------------------------------------------------
///
///  Ada83 requires various runtime checks. These can be suppressed by
///  pragma Suppress, but by default they are active (LRM 11.7).
///
///-----------------------------------------------------------------------------

/// @brief Check that a value is within a type's range
/// @param ctx   Evaluation context
/// @param value Value to check
/// @param type  Type with constraints
/// @param loc   Source location for error message
///
void Check_Range(Eval_Context *ctx, Runtime_Value value,
                Type_Descriptor *type, Source_Location loc);


/// @brief Check array index is within bounds
/// @param ctx   Evaluation context
/// @param index Index value
/// @param array Array type
/// @param loc   Source location for error message
///
void Check_Index(Eval_Context *ctx, int64_t index,
                Type_Descriptor *array, Source_Location loc);


/// @brief Check discriminant constraint
/// @param ctx    Evaluation context
/// @param record Record value
/// @param type   Constrained record type
/// @param loc    Source location for error message
///
void Check_Discriminant(Eval_Context *ctx, Runtime_Value record,
                       Type_Descriptor *type, Source_Location loc);


/// @brief Raise an exception
/// @param ctx            Evaluation context
/// @param exception_name Exception name
/// @param loc            Source location
///
_Noreturn void Raise_Exception(Eval_Context *ctx, String_Slice exception_name,
                              Source_Location loc);


///-----------------------------------------------------------------------------
///                   S T A N D A R D   P A C K A G E S
///-----------------------------------------------------------------------------
///
///  These functions implement the predefined packages required by Ada83.
///
///-----------------------------------------------------------------------------

/// @brief Execute TEXT_IO.PUT operation
/// @param ctx  Evaluation context
/// @param item Item to output
///
void Text_IO_Put(Eval_Context *ctx, Runtime_Value item);


/// @brief Execute TEXT_IO.GET operation
/// @param ctx  Evaluation context
/// @param type Type of item to read
/// @return Value read
///
Runtime_Value Text_IO_Get(Eval_Context *ctx, Type_Descriptor *type);


/// @brief Execute TEXT_IO.NEW_LINE operation
/// @param ctx   Evaluation context
/// @param count Number of lines
///
void Text_IO_New_Line(Eval_Context *ctx, int64_t count);


#endif // ADA83_EVAL_H
