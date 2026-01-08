///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                    S Y M B O L   T A B L E                                --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module implements the symbol table for the Ada83 interpreter.       --
///  The symbol table provides name resolution services, managing the         --
///  visibility and lifetime of declared identifiers.                         --
///                                                                           --
///  Ada83's visibility rules (LRM Chapter 8) are complex:                    --
///    - Scope nesting with shadowing                                         --
///    - Package visibility (with/use clauses)                                --
///    - Overloading of subprogram names                                      --
///    - Direct visibility vs. use visibility                                 --
///                                                                           --
///  The symbol table uses hash chaining with scope-aware lookup.             --
///  Reference: GNAT's Sem_Res and Sem_Ch8 packages provide similar services  --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_SYMBOLS_H
#define ADA83_SYMBOLS_H

#include "ada83_common.h"
#include "ada83_ast.h"
#include "ada83_types.h"


///-----------------------------------------------------------------------------
///                   S Y M B O L   K I N D
///-----------------------------------------------------------------------------
///
///  Classification of symbol table entries by the kind of entity they
///  represent. Corresponds to Ada's "entities" as described in GNAT's Einfo.
///
///-----------------------------------------------------------------------------

typedef enum {
    SK_UNKNOWN = 0,         /// Uninitialized/error
    SK_TYPE,                /// Type declaration
    SK_ENUMERATION_LITERAL, /// Enumeration literal
    SK_EXCEPTION,           /// Exception declaration
    SK_PROCEDURE,           /// Procedure declaration
    SK_FUNCTION,            /// Function declaration
    SK_PACKAGE,             /// Package declaration
    SK_VARIABLE,            /// Variable declaration
    SK_CONSTANT,            /// Constant declaration
    SK_PARAMETER,           /// Formal parameter
    SK_LOOP_VARIABLE,       /// Loop parameter
    SK_COMPONENT,           /// Record component
    SK_DISCRIMINANT,        /// Discriminant
    SK_ENTRY,               /// Task entry
    SK_TASK_TYPE,           /// Task type
    SK_LABEL,               /// Statement label
    SK_GENERIC,             /// Generic unit
    SK_GENERIC_FORMAL       /// Generic formal parameter
} Symbol_Kind;


///-----------------------------------------------------------------------------
///                   S Y M B O L   E N T R Y
///-----------------------------------------------------------------------------
///
///  A symbol table entry contains all information about a declared entity.
///
///  Key fields:
///    - name: Identifier string
///    - kind: What kind of entity
///    - type: Associated type (if applicable)
///    - scope/scope_serial: For scope-aware lookup
///    - overloads: For overloaded subprograms
///
///-----------------------------------------------------------------------------

struct Symbol_Entry {
    //-------------------------------------------------------------------------
    // Identification
    //-------------------------------------------------------------------------
    String_Slice     name;           /// Identifier name
    Symbol_Kind      kind;           /// Entity classification
    Type_Descriptor *type;           /// Associated type

    //-------------------------------------------------------------------------
    // Scope Information
    //-------------------------------------------------------------------------
    Symbol_Entry    *next;           /// Hash chain link
    Symbol_Entry    *prev;           /// Previous in chain
    int              scope;          /// Scope nesting level
    int              scope_serial;   /// Serial number within scope
    int              elaboration;    /// Elaboration order number
    int              nesting_level;  /// Lexical nesting depth
    Symbol_Entry    *homonym;        /// Same-name entry in hash bucket

    //-------------------------------------------------------------------------
    // Value and Definition
    //-------------------------------------------------------------------------
    AST_Node        *definition;     /// Defining declaration
    int64_t          value;          /// Compile-time value (constants, enums)
    uint32_t         offset;         /// Memory offset (for variables)

    //-------------------------------------------------------------------------
    // Overloading Support
    //-------------------------------------------------------------------------
    Node_Vector      overloads;      /// Overloaded bodies (subprograms)
    Symbol_Vector    use_visible;    /// Use-visible symbols (packages)

    //-------------------------------------------------------------------------
    // Parent/Context
    //-------------------------------------------------------------------------
    Symbol_Entry    *parent;         /// Enclosing scope symbol
    Generic_Template *generic;       /// Generic template (if any)

    //-------------------------------------------------------------------------
    // Special Flags
    //-------------------------------------------------------------------------
    bool             is_inline;      /// pragma Inline
    bool             is_shared;      /// pragma Shared
    bool             is_external;    /// pragma Interface/Import

    //-------------------------------------------------------------------------
    // External Binding
    //-------------------------------------------------------------------------
    String_Slice     external_name;  /// External name (if different)
    String_Slice     external_lang;  /// Language convention
    String_Slice     mangled_name;   /// Mangled name for linking

    //-------------------------------------------------------------------------
    // Freeze and Visibility
    //-------------------------------------------------------------------------
    uint8_t          freeze_state;   /// Symbol freeze state
    AST_Node        *freeze_node;    /// Freeze point
    uint8_t          visibility;     /// Visibility flags (bit mask)
    uint32_t         uid;            /// Unique identifier for this symbol
};


///-----------------------------------------------------------------------------
///                   H A S H   T A B L E   S I Z E
///-----------------------------------------------------------------------------
///
///  The symbol table uses a fixed-size hash table with chaining.
///  4096 buckets provides good performance for typical programs.
///
///-----------------------------------------------------------------------------

#define SYMBOL_HASH_SIZE 4096


///-----------------------------------------------------------------------------
///                   S E M A N T I C   C O N T E X T
///-----------------------------------------------------------------------------
///
///  The Semantic_Context structure maintains all state needed during
///  semantic analysis and execution. This includes:
///    - Symbol table
///    - Scope stack
///    - Current compilation context
///    - Runtime state (for interpretation)
///
///  This corresponds to GNAT's "semantic tables" concept.
///
///-----------------------------------------------------------------------------

typedef struct {
    //-------------------------------------------------------------------------
    // Symbol Table
    //-------------------------------------------------------------------------
    Symbol_Entry *hash_table[SYMBOL_HASH_SIZE];   /// Hash buckets
    int           current_scope;                   /// Current scope level
    int           scope_serial;                    /// Serial number in scope
    int           elaboration_order;               /// Next elaboration number

    //-------------------------------------------------------------------------
    // Current Context
    //-------------------------------------------------------------------------
    AST_Node     *current_discriminants;           /// Current discriminant part
    AST_Node     *current_package;                 /// Enclosing package

    //-------------------------------------------------------------------------
    // Use Clause Tracking
    //-------------------------------------------------------------------------
    Symbol_Vector use_visible;                     /// Use-visible packages
    uint64_t      use_visibility_bits[64];         /// Fast visibility check

    //-------------------------------------------------------------------------
    // Exception Handling
    //-------------------------------------------------------------------------
    jmp_buf      *exception_handlers[16];          /// Handler stack
    int           exception_depth;                 /// Current handler depth
    String_Slice  current_exception[16];           /// Exception being handled

    //-------------------------------------------------------------------------
    // I/O State
    //-------------------------------------------------------------------------
    FILE        **io_files;                        /// Open file handles
    uint32_t      io_file_count;                   /// Number of files
    uint32_t      io_file_capacity;                /// Allocated capacity

    //-------------------------------------------------------------------------
    // Label Management
    //-------------------------------------------------------------------------
    String_Slice *labels;                          /// Declared labels
    uint32_t      label_count;                     /// Number of labels
    uint32_t      label_capacity;                  /// Allocated capacity

    //-------------------------------------------------------------------------
    // Scope Stack
    //-------------------------------------------------------------------------
    Symbol_Entry *scope_stack[256];                /// Symbols in current scope
    int           scope_stack_depth;               /// Current stack position

    //-------------------------------------------------------------------------
    // Dependency Tracking
    //-------------------------------------------------------------------------
    Symbol_Vector dependencies[256];               /// Package dependencies
    int           dependency_depth;                /// Current dependency level

    //-------------------------------------------------------------------------
    // Exception Declarations
    //-------------------------------------------------------------------------
    Symbol_Vector exceptions;                      /// All declared exceptions

    //-------------------------------------------------------------------------
    // Unique ID Counter
    //-------------------------------------------------------------------------
    uint32_t      uid_counter;                     /// For unique IDs

} Semantic_Context;


///-----------------------------------------------------------------------------
///                   S Y M B O L   T A B L E   O P E R A T I O N S
///-----------------------------------------------------------------------------

/// @brief Compute hash for a symbol name
/// @param name Symbol name
/// @return Hash bucket index (0..SYMBOL_HASH_SIZE-1)
///
/// Uses case-insensitive hashing per Ada's identifier rules.
///
uint32_t Symbol_Hash(String_Slice name);


/// @brief Create a new symbol entry
/// @param name Symbol name
/// @param kind Entity classification
/// @param type Associated type (or NULL)
/// @param def  Defining declaration (or NULL)
/// @return Newly allocated symbol entry
///
Symbol_Entry *Symbol_New(String_Slice     name,
                         Symbol_Kind      kind,
                         Type_Descriptor *type,
                         AST_Node        *def);


/// @brief Add a symbol to the symbol table
/// @param ctx    Semantic context
/// @param symbol Symbol to add
/// @return The added symbol (for chaining)
///
/// Adds the symbol at the current scope level.
/// Does NOT check for duplicate declarations (caller must handle).
///
Symbol_Entry *Symbol_Add(Semantic_Context *ctx, Symbol_Entry *symbol);


/// @brief Look up a symbol by name
/// @param ctx  Semantic context
/// @param name Name to look up
/// @return Found symbol, or NULL if not found
///
/// Searches from innermost to outermost scope, returning the first match.
/// Considers both direct visibility and use-visibility.
///
Symbol_Entry *Symbol_Find(Semantic_Context *ctx, String_Slice name);


/// @brief Look up a symbol with overload resolution hints
/// @param ctx           Semantic context
/// @param name          Name to look up
/// @param argument_count Number of arguments (for subprogram overloads)
/// @param expected_type Expected result type (for functions)
/// @return Best matching symbol, or NULL if not found
///
/// Used for resolving overloaded subprogram calls.
///
Symbol_Entry *Symbol_Find_Overload(Semantic_Context  *ctx,
                                   String_Slice       name,
                                   int                argument_count,
                                   Type_Descriptor   *expected_type);


/// @brief Apply use clause visibility
/// @param ctx     Semantic context
/// @param package Package symbol being "used"
/// @param name    Specific name in package
///
/// Makes declarations from package use-visible per LRM 8.4.
///
void Symbol_Apply_Use(Semantic_Context *ctx, Symbol_Entry *package, String_Slice name);


///-----------------------------------------------------------------------------
///                   S C O P E   M A N A G E M E N T
///-----------------------------------------------------------------------------

/// @brief Enter a new scope level
/// @param ctx Semantic context
///
/// Increments scope nesting level. All subsequently added symbols
/// will be at this new scope level.
///
void Scope_Enter(Semantic_Context *ctx);


/// @brief Exit the current scope level
/// @param ctx Semantic context
///
/// Decrements scope nesting level. Symbols at the exited level
/// remain in the table but become less visible.
///
void Scope_Exit(Semantic_Context *ctx);


///-----------------------------------------------------------------------------
///                   G E N E R I C   T E M P L A T E
///-----------------------------------------------------------------------------
///
///  Generic template storage for generic unit instantiation.
///  Corresponds to Ada83 LRM Chapter 12.
///
///-----------------------------------------------------------------------------

struct Generic_Template {
    String_Slice  name;             /// Generic unit name
    Node_Vector   formal_params;    /// Formal parameters
    Node_Vector   declarations;     /// Declarations in generic
    AST_Node     *unit;             /// Unit (package/subprogram) AST
    AST_Node     *body;             /// Body AST (if available)
};


/// @brief Find a generic template by name
/// @param ctx  Semantic context
/// @param name Generic unit name
/// @return Template, or NULL if not found
///
Generic_Template *Generic_Find(Semantic_Context *ctx, String_Slice name);


///-----------------------------------------------------------------------------
///                   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

/// @brief Initialize a semantic context
/// @param ctx Context to initialize
///
/// Clears all tables and initializes predefined symbols.
///
void Semantic_Init(Semantic_Context *ctx);


#endif // ADA83_SYMBOLS_H
