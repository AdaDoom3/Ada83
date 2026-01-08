///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///               A B S T R A C T   S Y N T A X   T R E E                     --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module defines the Abstract Syntax Tree (AST) node types for the    --
///  Ada83 interpreter. Each node kind corresponds to a syntactic construct   --
///  from the Ada83 Language Reference Manual.                                --
///                                                                           --
///  The AST representation follows GNAT's approach of using discriminated    --
///  unions (implemented here via C unions with a kind discriminant).         --
///                                                                           --
///  Node categories roughly follow the Ada83 LRM chapter organization:       --
///    - Names and Expressions (LRM Chapter 4)                                --
///    - Statements (LRM Chapter 5)                                           --
///    - Type Declarations (LRM Chapter 3)                                    --
///    - Subprogram Declarations (LRM Chapter 6)                              --
///    - Package Declarations (LRM Chapter 7)                                 --
///    - Task Declarations (LRM Chapter 9)                                    --
///    - Generic Units (LRM Chapter 12)                                       --
///    - Compilation Units (LRM Chapter 10)                                   --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_AST_H
#define ADA83_AST_H

#include "ada83_common.h"
#include "ada83_lexer.h"


///-----------------------------------------------------------------------------
///                   N O D E   V E C T O R   T Y P E
///-----------------------------------------------------------------------------
///
///  Dynamic array of AST node pointers, used for lists of declarations,
///  statements, parameters, etc.
///
///-----------------------------------------------------------------------------

typedef struct {
    AST_Node **data;        /// Array of node pointers
    uint32_t   count;       /// Number of elements
    uint32_t   capacity;    /// Allocated capacity
} Node_Vector;


///-----------------------------------------------------------------------------
///                   A S T   N O D E   K I N D
///-----------------------------------------------------------------------------
///
///  Enumeration of all AST node types. The naming convention uses uppercase
///  abbreviations similar to GNAT's node kind names.
///
///  Categories:
///    N_ERR..N_NULL     : Special and literal nodes
///    N_AG..N_ALC       : Aggregates and allocators
///    N_BIN..N_AT       : Expressions
///    N_RN..N_DS        : Type-related constructs
///    N_PM..N_FB        : Subprogram-related constructs
///    N_PKS..N_PKD      : Package-related constructs
///    N_IF..N_CLT       : Statements
///    N_TKS..N_ENT      : Task-related constructs
///    N_GEN..N_GINST    : Generic-related constructs
///    N_CX..N_CU        : Compilation unit constructs
///
///-----------------------------------------------------------------------------

typedef enum {
    //-------------------------------------------------------------------------
    // Special Nodes
    //-------------------------------------------------------------------------
    N_ERR = 0,              /// Error node (malformed syntax)

    //-------------------------------------------------------------------------
    // Primary Expressions (LRM 4.1, 4.4)
    //-------------------------------------------------------------------------
    N_ID,                   /// Identifier (simple name)
    N_INT,                  /// Integer literal
    N_REAL,                 /// Real literal
    N_CHAR,                 /// Character literal
    N_STR,                  /// String literal
    N_NULL,                 /// Null literal (access null)

    //-------------------------------------------------------------------------
    // Aggregate Expressions (LRM 4.3)
    //-------------------------------------------------------------------------
    N_AG,                   /// Aggregate (record or array)
    N_ASC,                  /// Association (name => value) in aggregate

    //-------------------------------------------------------------------------
    // Compound Expressions (LRM 4.4, 4.5)
    //-------------------------------------------------------------------------
    N_BIN,                  /// Binary operation (A op B)
    N_UN,                   /// Unary operation (op A)
    N_AT,                   /// Attribute reference (X'Attr)
    N_QL,                   /// Qualified expression (Type'(Expr))
    N_CL,                   /// Function/procedure call (Name(Args))
    N_IX,                   /// Indexed component (A(I, J))
    N_SL,                   /// Slice (A(Lo..Hi))
    N_SEL,                  /// Selected component (R.Field)
    N_ALC,                  /// Allocator (new Type)
    N_DRF,                  /// Dereference (Ptr.all)
    N_CVT,                  /// Type conversion
    N_CHK,                  /// Runtime check node

    //-------------------------------------------------------------------------
    // Type-Related Constructs (LRM Chapter 3)
    //-------------------------------------------------------------------------
    N_TI,                   /// Integer/discrete type indication
    N_TE,                   /// Enumeration type definition
    N_TF,                   /// Floating point type definition
    N_TX,                   /// Fixed point type definition
    N_TA,                   /// Array type definition
    N_TR,                   /// Record type definition
    N_TAC,                  /// Access type definition
    N_TP,                   /// Private type definition
    N_RN,                   /// Range constraint (Lo..Hi)
    N_CN,                   /// Subtype constraint (Name with constraints)
    N_ST,                   /// Subtype indication (Name'Range or Name(X))

    //-------------------------------------------------------------------------
    // Record Components (LRM 3.7)
    //-------------------------------------------------------------------------
    N_CM,                   /// Component declaration
    N_VR,                   /// Variant (when X => components)
    N_VP,                   /// Variant part (case ... is when ...)
    N_DS,                   /// Discriminant specification

    //-------------------------------------------------------------------------
    // Parameter and Subprogram Constructs (LRM Chapter 6)
    //-------------------------------------------------------------------------
    N_PM,                   /// Parameter specification
    N_PS,                   /// Procedure specification
    N_FS,                   /// Function specification
    N_PB,                   /// Procedure body
    N_FB,                   /// Function body
    N_PD,                   /// Procedure declaration (spec only)
    N_FD,                   /// Function declaration (spec only)

    //-------------------------------------------------------------------------
    // Package Constructs (LRM Chapter 7)
    //-------------------------------------------------------------------------
    N_PKS,                  /// Package specification
    N_PKB,                  /// Package body
    N_PKD,                  /// Package declaration (spec only)

    //-------------------------------------------------------------------------
    // Declaration Constructs (LRM Chapter 3)
    //-------------------------------------------------------------------------
    N_OD,                   /// Object declaration (variables, constants)
    N_ND,                   /// Number declaration
    N_TD,                   /// Type declaration
    N_SD,                   /// Subtype declaration
    N_ED,                   /// Exception declaration
    N_RE,                   /// Renaming declaration

    //-------------------------------------------------------------------------
    // Statements (LRM Chapter 5)
    //-------------------------------------------------------------------------
    N_AS,                   /// Assignment statement
    N_IF,                   /// If statement
    N_CS,                   /// Case statement
    N_LP,                   /// Loop statement
    N_BL,                   /// Block statement
    N_EX,                   /// Exit statement
    N_RT,                   /// Return statement
    N_GT,                   /// Goto statement
    N_RS,                   /// Raise statement
    N_NS,                   /// Null statement
    N_CLT,                  /// Procedure call statement
    N_DL,                   /// Delay statement
    N_AB,                   /// Abort statement
    N_LBL,                  /// Label

    //-------------------------------------------------------------------------
    // Control Flow Helpers
    //-------------------------------------------------------------------------
    N_EL,                   /// Elsif clause
    N_WH,                   /// When clause (case alternative)
    N_HD,                   /// Exception handler
    N_CH,                   /// Choice list

    //-------------------------------------------------------------------------
    // Task Constructs (LRM Chapter 9)
    //-------------------------------------------------------------------------
    N_TKS,                  /// Task specification
    N_TKB,                  /// Task body
    N_TKD,                  /// Task type declaration
    N_ENT,                  /// Entry declaration
    N_EI,                   /// Entry index specification
    N_ACC,                  /// Accept statement
    N_SLS,                  /// Select statement
    N_SA,                   /// Select alternative
    N_TRM,                  /// Terminate alternative

    //-------------------------------------------------------------------------
    // Representation Clauses (LRM Chapter 13)
    //-------------------------------------------------------------------------
    N_RRC,                  /// Representation record clause
    N_ERC,                  /// Enumeration representation clause
    N_LNC,                  /// Length clause
    N_ADC,                  /// Address clause

    //-------------------------------------------------------------------------
    // Generic Constructs (LRM Chapter 12)
    //-------------------------------------------------------------------------
    N_GEN,                  /// Generic declaration
    N_GINST,                /// Generic instantiation
    N_GTP,                  /// Generic type parameter
    N_GVL,                  /// Generic value parameter
    N_GSP,                  /// Generic subprogram parameter

    //-------------------------------------------------------------------------
    // Compilation Unit Constructs (LRM Chapter 10)
    //-------------------------------------------------------------------------
    N_CX,                   /// Context clause (with/use)
    N_WI,                   /// With clause
    N_US,                   /// Use clause
    N_PG,                   /// Pragma
    N_CU,                   /// Compilation unit

    //-------------------------------------------------------------------------
    // Miscellaneous
    //-------------------------------------------------------------------------
    N_LST,                  /// Generic list node
    N_OPID,                 /// Operator as identifier ("/=", "+", etc.)
    N_DRV,                  /// Derived type derivation record

    N_COUNT                 /// Sentinel for array sizing
} AST_Node_Kind;


///-----------------------------------------------------------------------------
///                   A S T   N O D E   S T R U C T U R E
///-----------------------------------------------------------------------------
///
///  The AST_Node structure uses a tagged union (discriminated record)
///  pattern. The 'kind' field determines which union variant is active.
///
///  All nodes share common fields:
///    - kind: The node type discriminant
///    - location: Source location for error reporting
///    - type: Resolved type (set during semantic analysis)
///    - symbol: Associated symbol (set during semantic analysis)
///
///  The union contains variant-specific data for each node kind.
///
///-----------------------------------------------------------------------------

struct AST_Node {
    AST_Node_Kind       kind;       /// Node type discriminant
    Source_Location     location;   /// Source position
    Type_Descriptor    *type;       /// Resolved type (semantic phase)
    Symbol_Entry       *symbol;     /// Associated symbol (semantic phase)

    /// Node-specific data organized by category
    union {
        //---------------------------------------------------------------------
        // Simple value: identifier, integer, real, character
        //---------------------------------------------------------------------
        String_Slice string_val;        /// N_ID, N_STR
        int64_t      integer_val;       /// N_INT, N_CHAR
        double       real_val;          /// N_REAL

        //---------------------------------------------------------------------
        // Binary operation: left op right
        //---------------------------------------------------------------------
        struct {
            Token_Kind   op;            /// Operator token
            AST_Node    *left;          /// Left operand
            AST_Node    *right;         /// Right operand
        } binary;

        //---------------------------------------------------------------------
        // Unary operation: op operand
        //---------------------------------------------------------------------
        struct {
            Token_Kind   op;            /// Operator token
            AST_Node    *operand;       /// Single operand
        } unary;

        //---------------------------------------------------------------------
        // Attribute reference: prefix'attribute(args)
        //---------------------------------------------------------------------
        struct {
            AST_Node    *prefix;        /// Object or type name
            String_Slice attribute;     /// Attribute name
            Node_Vector  args;          /// Attribute arguments (if any)
        } attr;

        //---------------------------------------------------------------------
        // Qualified expression: type_name'(expression)
        //---------------------------------------------------------------------
        struct {
            AST_Node    *type_name;     /// Type mark
            AST_Node    *expression;    /// Enclosed expression
        } qualified;

        //---------------------------------------------------------------------
        // Call expression: name(arguments)
        //---------------------------------------------------------------------
        struct {
            AST_Node    *callee;        /// Function/procedure name
            Node_Vector  args;          /// Actual parameters
        } call;

        //---------------------------------------------------------------------
        // Indexed component: prefix(index_list)
        //---------------------------------------------------------------------
        struct {
            AST_Node    *prefix;        /// Array or entry family
            Node_Vector  indices;       /// Index expressions
        } indexed;

        //---------------------------------------------------------------------
        // Slice: prefix(lo..hi)
        //---------------------------------------------------------------------
        struct {
            AST_Node    *prefix;        /// Array object
            AST_Node    *low_bound;     /// Slice low bound
            AST_Node    *high_bound;    /// Slice high bound
        } slice;

        //---------------------------------------------------------------------
        // Selected component: prefix.selector
        //---------------------------------------------------------------------
        struct {
            AST_Node    *prefix;        /// Record or package
            String_Slice selector;      /// Component/entity name
        } selected;

        //---------------------------------------------------------------------
        // Allocator: new type_or_qualified_expression
        //---------------------------------------------------------------------
        struct {
            AST_Node    *subtype;       /// Allocated type
            AST_Node    *init_value;    /// Initial value (if any)
        } allocator;

        //---------------------------------------------------------------------
        // Aggregate: (associations)
        //---------------------------------------------------------------------
        struct {
            Node_Vector  items;         /// Component associations
            AST_Node    *low_bound;     /// Array bounds (if explicit)
            AST_Node    *high_bound;
            uint8_t      dimension;     /// Array dimension (1 for vector)
        } aggregate;

        //---------------------------------------------------------------------
        // Association: choices => value
        //---------------------------------------------------------------------
        struct {
            Node_Vector  choices;       /// Choice list
            AST_Node    *value;         /// Associated value
        } association;

        //---------------------------------------------------------------------
        // Range: low_bound .. high_bound
        //---------------------------------------------------------------------
        struct {
            AST_Node    *low_bound;     /// Range lower bound
            AST_Node    *high_bound;    /// Range upper bound
        } range;

        //---------------------------------------------------------------------
        // Constraint: (constraint_list)
        //---------------------------------------------------------------------
        struct {
            AST_Node    *range_constraint;  /// Range if present
            Node_Vector  constraints;       /// Discriminant/index constraints
        } constraint;

        //---------------------------------------------------------------------
        // Subtype indication: type_mark [constraint]
        //---------------------------------------------------------------------
        struct {
            AST_Node    *type_mark;         /// Type name
            AST_Node    *constraint;        /// Optional constraint
        } subtype;

        //---------------------------------------------------------------------
        // Index constraint: (ranges)
        //---------------------------------------------------------------------
        struct {
            Node_Vector  ranges;            /// Index ranges/types
        } index_constraint;

        //---------------------------------------------------------------------
        // Enumeration type definition: (literals)
        //---------------------------------------------------------------------
        struct {
            Node_Vector  literals;          /// Enumeration literals
        } enumeration;

        //---------------------------------------------------------------------
        // Array type definition
        //---------------------------------------------------------------------
        struct {
            Node_Vector  indices;           /// Index types/constraints
            AST_Node    *element_type;      /// Component type
            bool         is_constrained;    /// True if constrained
        } array_type;

        //---------------------------------------------------------------------
        // Record type definition
        //---------------------------------------------------------------------
        struct {
            Node_Vector  components;        /// Component declarations
            AST_Node    *variant;           /// Variant part (if any)
        } record_type;

        //---------------------------------------------------------------------
        // Component declaration
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Component name
            AST_Node    *comp_type;     /// Component type
            AST_Node    *init_value;    /// Default value (if any)
            bool         is_aliased;    /// Aliased component
            uint32_t     offset;        /// Byte offset in record
            uint32_t     bit_offset;    /// Bit offset within byte
            AST_Node    *disc_constraint; /// Discriminant constraint (if any)
            AST_Node    *disc_value;    /// Discriminant value (if any)
        } component;

        //---------------------------------------------------------------------
        // Variant record part
        //---------------------------------------------------------------------
        struct {
            Node_Vector  choices;       /// Discriminant values
            Node_Vector  components;    /// Components in this variant
        } variant;

        //---------------------------------------------------------------------
        // Variant part: case discriminant is when ...
        //---------------------------------------------------------------------
        struct {
            AST_Node    *discriminant;  /// Discriminant name
            Node_Vector  variants;      /// Variant alternatives
            uint32_t     total_size;    /// Maximum variant size
        } variant_part;

        //---------------------------------------------------------------------
        // Parameter specification
        //---------------------------------------------------------------------
        struct {
            String_Slice param_name;    /// Parameter name
            AST_Node    *param_type;    /// Parameter type
            AST_Node    *default_value; /// Default value (if any)
            uint8_t      mode;          /// IN=1, OUT=2, IN OUT=3
        } param;

        //---------------------------------------------------------------------
        // Subprogram specification
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Subprogram name
            Node_Vector  params;        /// Parameter list
            AST_Node    *return_type;   /// Return type (functions only)
            String_Slice operator_name; /// Operator symbol (if operator)
        } subprog_spec;

        //---------------------------------------------------------------------
        // Subprogram body
        //---------------------------------------------------------------------
        struct {
            AST_Node    *spec;          /// Subprogram specification
            Node_Vector  decls;         /// Declarative part
            Node_Vector  stmts;         /// Statement sequence
            Node_Vector  handlers;      /// Exception handlers
            int          elaboration;   /// Elaboration order
            Symbol_Entry *parent_scope; /// Enclosing scope
            Node_Vector  local_labels;  /// Labels in this body
        } subprog_body;

        //---------------------------------------------------------------------
        // Package specification
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Package name
            Node_Vector  visible_decls; /// Visible declarations
            Node_Vector  private_decls; /// Private declarations
            int          elaboration;   /// Elaboration order
        } package_spec;

        //---------------------------------------------------------------------
        // Package body
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Package name
            Node_Vector  decls;         /// Declarative part
            Node_Vector  stmts;         /// Initialization statements
            Node_Vector  handlers;      /// Exception handlers
            int          elaboration;   /// Elaboration order
        } package_body;

        //---------------------------------------------------------------------
        // Object declaration
        //---------------------------------------------------------------------
        struct {
            Node_Vector  names;         /// Declared identifier(s)
            AST_Node    *object_type;   /// Object type
            AST_Node    *init_value;    /// Initial value (if any)
            bool         is_constant;   /// Constant declaration
        } object_decl;

        //---------------------------------------------------------------------
        // Type declaration
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Type name
            AST_Node    *definition;    /// Type definition
            AST_Node    *discriminants; /// Discriminant part (if any)
            bool         is_new;        /// Derived type (new)
            bool         is_derived;    /// Derived type marker
            AST_Node    *parent_type;   /// Parent for derived types
            Node_Vector  disc_list;     /// Discriminant specifications
        } type_decl;

        //---------------------------------------------------------------------
        // Subtype declaration
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Subtype name
            AST_Node    *indication;    /// Subtype indication
            AST_Node    *constraint;    /// Constraint
            AST_Node    *range_expr;    /// Range constraint value
        } subtype_decl;

        //---------------------------------------------------------------------
        // Exception declaration
        //---------------------------------------------------------------------
        struct {
            Node_Vector  names;         /// Exception identifier(s)
            AST_Node    *renaming;      /// Renamed exception (if any)
        } exception_decl;

        //---------------------------------------------------------------------
        // Renaming declaration
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// New name
            AST_Node    *renamed;       /// Renamed entity
        } renaming;

        //---------------------------------------------------------------------
        // Assignment statement
        //---------------------------------------------------------------------
        struct {
            AST_Node    *target;        /// Left-hand side
            AST_Node    *value;         /// Right-hand side
        } assignment;

        //---------------------------------------------------------------------
        // If statement
        //---------------------------------------------------------------------
        struct {
            AST_Node    *condition;     /// Condition expression
            Node_Vector  then_stmts;    /// Then clause statements
            Node_Vector  elsif_parts;   /// Elsif clauses
            Node_Vector  else_stmts;    /// Else clause statements
        } if_stmt;

        //---------------------------------------------------------------------
        // Case statement
        //---------------------------------------------------------------------
        struct {
            AST_Node    *selector;      /// Case selector expression
            Node_Vector  alternatives;  /// Case alternatives
        } case_stmt;

        //---------------------------------------------------------------------
        // Loop statement
        //---------------------------------------------------------------------
        struct {
            String_Slice label;         /// Loop label (if any)
            AST_Node    *iteration;     /// Iteration scheme
            bool         is_reverse;    /// Reverse iteration
            Node_Vector  stmts;         /// Loop body statements
            Node_Vector  local_labels;  /// Labels in loop
        } loop_stmt;

        //---------------------------------------------------------------------
        // Block statement
        //---------------------------------------------------------------------
        struct {
            String_Slice label;         /// Block label (if any)
            Node_Vector  decls;         /// Declarative part
            Node_Vector  stmts;         /// Statement sequence
            Node_Vector  handlers;      /// Exception handlers
        } block_stmt;

        //---------------------------------------------------------------------
        // Exit statement
        //---------------------------------------------------------------------
        struct {
            String_Slice label;         /// Target loop label
            AST_Node    *condition;     /// When condition (if any)
        } exit_stmt;

        //---------------------------------------------------------------------
        // Return statement
        //---------------------------------------------------------------------
        struct {
            AST_Node    *value;         /// Return value (functions)
        } return_stmt;

        //---------------------------------------------------------------------
        // Goto statement
        //---------------------------------------------------------------------
        struct {
            String_Slice label;         /// Target label
        } goto_stmt;

        //---------------------------------------------------------------------
        // Raise statement
        //---------------------------------------------------------------------
        struct {
            AST_Node    *exception;     /// Exception name (or NULL for reraise)
        } raise_stmt;

        //---------------------------------------------------------------------
        // Call statement
        //---------------------------------------------------------------------
        struct {
            AST_Node    *name;          /// Procedure name
            Node_Vector  args;          /// Actual parameters
        } call_stmt;

        //---------------------------------------------------------------------
        // Delay statement
        //---------------------------------------------------------------------
        struct {
            AST_Node    *duration;      /// Delay expression
        } delay_stmt;

        //---------------------------------------------------------------------
        // Accept statement
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Entry name
            Node_Vector  indices;       /// Entry family indices
            Node_Vector  params;        /// Entry parameters
            Node_Vector  stmts;         /// Accept body statements
            Node_Vector  handlers;      /// Exception handlers
            AST_Node    *guard;         /// Guard condition
        } accept_stmt;

        //---------------------------------------------------------------------
        // Select statement
        //---------------------------------------------------------------------
        struct {
            uint8_t      select_kind;   /// 0=select, 1=timed, 2=conditional
            AST_Node    *guard;         /// Guard expression
            Node_Vector  alternatives;  /// Select alternatives
        } select_stmt;

        //---------------------------------------------------------------------
        // Select alternative
        //---------------------------------------------------------------------
        struct {
            Node_Vector  choices;       /// Triggering entries/delays
            Node_Vector  stmts;         /// Alternative statements
        } select_alt;

        //---------------------------------------------------------------------
        // Exception handler
        //---------------------------------------------------------------------
        struct {
            Node_Vector  exceptions;    /// Exception names (or OTHERS)
            Node_Vector  stmts;         /// Handler statements
        } handler;

        //---------------------------------------------------------------------
        // When clause (case alternative)
        //---------------------------------------------------------------------
        struct {
            Node_Vector  choices;       /// Choice expressions
            Node_Vector  stmts;         /// Alternative statements
        } when_clause;

        //---------------------------------------------------------------------
        // Task specification
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Task name
            Node_Vector  entries;       /// Entry declarations
            bool         is_type;       /// Task type declaration
        } task_spec;

        //---------------------------------------------------------------------
        // Task body
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Task name
            Node_Vector  decls;         /// Declarative part
            Node_Vector  stmts;         /// Task body statements
            Node_Vector  handlers;      /// Exception handlers
        } task_body;

        //---------------------------------------------------------------------
        // Entry declaration
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Entry name
            Node_Vector  family_index;  /// Entry family index
            Node_Vector  params;        /// Entry parameters
            AST_Node    *guard;         /// Guard condition
        } entry_decl;

        //---------------------------------------------------------------------
        // Dereference: ptr.all
        //---------------------------------------------------------------------
        struct {
            AST_Node    *operand;       /// Access value to dereference
        } deref;

        //---------------------------------------------------------------------
        // Type conversion
        //---------------------------------------------------------------------
        struct {
            AST_Node    *target_type;   /// Target type
            AST_Node    *operand;       /// Expression to convert
        } conversion;

        //---------------------------------------------------------------------
        // Runtime check
        //---------------------------------------------------------------------
        struct {
            AST_Node    *operand;       /// Value being checked
            String_Slice check_name;    /// Check name (Constraint_Error, etc.)
        } check;

        //---------------------------------------------------------------------
        // Context clause
        //---------------------------------------------------------------------
        struct {
            Node_Vector  with_clauses;  /// With items
            Node_Vector  use_clauses;   /// Use items
        } context;

        //---------------------------------------------------------------------
        // With clause
        //---------------------------------------------------------------------
        struct {
            String_Slice unit_name;     /// Library unit name
        } with_clause;

        //---------------------------------------------------------------------
        // Use clause
        //---------------------------------------------------------------------
        struct {
            AST_Node    *package_name;  /// Package to use
        } use_clause;

        //---------------------------------------------------------------------
        // Pragma
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Pragma name
            Node_Vector  args;          /// Pragma arguments
        } pragma_node;

        //---------------------------------------------------------------------
        // Compilation unit
        //---------------------------------------------------------------------
        struct {
            AST_Node    *context;       /// Context clause
            Node_Vector  units;         /// Library unit(s)
        } comp_unit;

        //---------------------------------------------------------------------
        // Generic declaration
        //---------------------------------------------------------------------
        struct {
            Node_Vector  formal_params; /// Generic formal parameters
            Node_Vector  decls;         /// Declarations in generic
            AST_Node    *unit;          /// Generic unit (package/subprogram)
        } generic_decl;

        //---------------------------------------------------------------------
        // Generic instantiation
        //---------------------------------------------------------------------
        struct {
            String_Slice name;          /// Instance name
            String_Slice generic_name;  /// Generic unit name
            Node_Vector  actual_params; /// Actual parameters
        } generic_inst;

        //---------------------------------------------------------------------
        // Generic list (general-purpose)
        //---------------------------------------------------------------------
        struct {
            Node_Vector  items;         /// List items
        } list;

    }; // union

}; // struct AST_Node


///-----------------------------------------------------------------------------
///                   N O D E   C O N S T R U C T O R S
///-----------------------------------------------------------------------------

/// @brief Create a new AST node with specified kind and location
/// @param kind     Node type
/// @param location Source position
/// @return Newly allocated node (zero-initialized)
///
AST_Node *AST_New(AST_Node_Kind kind, Source_Location location);


/// @brief Create a node using shortened macro syntax
/// @note  Expands kind 'XXX' to 'N_XXX'
///
#define NODE(kind, loc) AST_New(N_##kind, loc)


///-----------------------------------------------------------------------------
///                   V E C T O R   O P E R A T I O N S
///-----------------------------------------------------------------------------

/// @brief Initialize a node vector to empty state
/// @param vec  The vector to initialize
///
/// Sets count and capacity to 0, data to NULL.
///
static inline void Node_Vector_Init(Node_Vector *vec)
{
    vec->data = NULL;
    vec->count = 0;
    vec->capacity = 0;
}


/// @brief Push a node onto a node vector
/// @param vec  The vector to modify
/// @param node The node to add
///
void Node_Vector_Push(Node_Vector *vec, AST_Node *node);


#endif // ADA83_AST_H
