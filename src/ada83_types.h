///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                         T Y P E   S Y S T E M                             --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module implements Ada83's strong static type system. Ada's type     --
///  system is one of its defining features, providing type safety through    --
///  distinct types, subtypes, and constraints.                               --
///                                                                           --
///  Key Ada83 type concepts (LRM Chapter 3):                                 --
///    - Scalar types: discrete (integer, enumeration), real (float, fixed)   --
///    - Composite types: arrays, records                                     --
///    - Access types: pointers to objects and subprograms                    --
///    - Private types: encapsulation for abstract data types                 --
///    - Derived types: type derivation with inheritance                      --
///    - Subtypes: constrained views of existing types                        --
///                                                                           --
///  The type system enforces:                                                --
///    - Name equivalence (not structural equivalence)                        --
///    - Range checking for scalar types                                      --
///    - Index and discriminant constraints                                   --
///    - Access value checking (null pointer detection)                       --
///                                                                           --
///  Reference: GNAT's Einfo package provides similar type descriptors        --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_TYPES_H
#define ADA83_TYPES_H

#include "ada83_common.h"
#include "ada83_ast.h"


///-----------------------------------------------------------------------------
///                   T Y P E   K I N D   E N U M E R A T I O N
///-----------------------------------------------------------------------------
///
///  Classification of Ada83 type kinds based on LRM Chapter 3.
///  The organization follows the type hierarchy defined in the LRM.
///
///-----------------------------------------------------------------------------

typedef enum {
    //-------------------------------------------------------------------------
    // Special Types
    //-------------------------------------------------------------------------
    TY_VOID = 0,            /// No type / error type

    //-------------------------------------------------------------------------
    // Scalar Types (LRM 3.5)
    //-------------------------------------------------------------------------
    TY_INTEGER,             /// Signed integer type
    TY_BOOLEAN,             /// Boolean type (predefined enumeration)
    TY_CHARACTER,           /// Character type (predefined enumeration)
    TY_FLOAT,               /// Floating-point type
    TY_ENUMERATION,         /// User-defined enumeration type
    TY_FIXED,               /// Fixed-point type (ordinary and decimal)

    //-------------------------------------------------------------------------
    // Composite Types (LRM 3.6, 3.7)
    //-------------------------------------------------------------------------
    TY_ARRAY,               /// Array type (constrained or unconstrained)
    TY_RECORD,              /// Record type (with or without discriminants)

    //-------------------------------------------------------------------------
    // Access Types (LRM 3.8)
    //-------------------------------------------------------------------------
    TY_ACCESS,              /// Access (pointer) type

    //-------------------------------------------------------------------------
    // Task Types (LRM 9.1)
    //-------------------------------------------------------------------------
    TY_TASK,                /// Task type

    //-------------------------------------------------------------------------
    // Private Types (LRM 7.4)
    //-------------------------------------------------------------------------
    TY_PRIVATE,             /// Private type (visible declaration)

    //-------------------------------------------------------------------------
    // Universal Types (LRM 3.5.4, 3.5.6)
    //-------------------------------------------------------------------------
    TY_UNIVERSAL_INT,       /// Universal_integer (compile-time only)
    TY_UNIVERSAL_REAL,      /// Universal_real (compile-time only)

    //-------------------------------------------------------------------------
    // Derived Types (LRM 3.4)
    //-------------------------------------------------------------------------
    TY_DERIVED,             /// Derived type (new parent_type ...)

    //-------------------------------------------------------------------------
    // Special/Internal Types
    //-------------------------------------------------------------------------
    TY_PRIVATE_FULL,        /// Full view of private type
    TY_FILE,                /// File type (Text_IO)

    TY_COUNT                /// Sentinel for array sizing
} Type_Kind;


///-----------------------------------------------------------------------------
///                   S Y M B O L   V E C T O R
///-----------------------------------------------------------------------------
///
///  Vector of symbol pointers, used for enumeration literals,
///  subprogram overloads, etc.
///
///-----------------------------------------------------------------------------

typedef struct {
    Symbol_Entry **data;
    uint32_t       count;
    uint32_t       capacity;
} Symbol_Vector;


///-----------------------------------------------------------------------------
///                   R E P R E S E N T A T I O N   C L A U S E   V E C T O R
///-----------------------------------------------------------------------------

typedef struct {
    Rep_Clause **data;
    uint32_t     count;
    uint32_t     capacity;
} RepClause_Vector;


///-----------------------------------------------------------------------------
///                   T Y P E   D E S C R I P T O R
///-----------------------------------------------------------------------------
///
///  Type_Descriptor contains all information about an Ada type.
///  This structure is similar to GNAT's entity information (Einfo).
///
///  The descriptor uses a discriminated record pattern where 'kind'
///  determines which fields are meaningful.
///
///-----------------------------------------------------------------------------

struct Type_Descriptor {
    //-------------------------------------------------------------------------
    // Common Fields (all type kinds)
    //-------------------------------------------------------------------------
    Type_Kind        kind;          /// Type classification
    String_Slice     name;          /// Type name (for error messages)

    //-------------------------------------------------------------------------
    // Type Relationships
    //-------------------------------------------------------------------------
    Type_Descriptor *base_type;     /// Base type (for subtypes/derived)
    Type_Descriptor *element_type;  /// Element type (arrays, access)
    Type_Descriptor *parent_type;   /// Parent type (for derived types)
    Type_Descriptor *index_type;    /// Index type (for arrays)

    //-------------------------------------------------------------------------
    // Scalar Type Bounds
    //-------------------------------------------------------------------------
    int64_t          low_bound;     /// Range lower bound
    int64_t          high_bound;    /// Range upper bound

    //-------------------------------------------------------------------------
    // Composite Type Information
    //-------------------------------------------------------------------------
    Node_Vector      components;    /// Record components
    Node_Vector      discriminants; /// Discriminant specifications

    //-------------------------------------------------------------------------
    // Size and Alignment (LRM 13.3)
    //-------------------------------------------------------------------------
    uint32_t         size;          /// Size in bytes
    uint32_t         alignment;     /// Alignment in bytes

    //-------------------------------------------------------------------------
    // Enumeration Information
    //-------------------------------------------------------------------------
    Symbol_Vector    enum_literals; /// Enumeration literal symbols

    //-------------------------------------------------------------------------
    // Representation Clauses
    //-------------------------------------------------------------------------
    RepClause_Vector rep_clauses;   /// Associated representation clauses
    uint64_t         address;       /// Address clause value (if any)
    bool             is_packed;     /// Pragma Pack applied

    //-------------------------------------------------------------------------
    // Derived Type Operations
    //-------------------------------------------------------------------------
    Node_Vector      operations;    /// Inherited/overriding operations

    //-------------------------------------------------------------------------
    // Fixed-Point Specific
    //-------------------------------------------------------------------------
    int64_t          small;         /// Fixed-point small value
    int64_t          delta_val;     /// Fixed-point delta

    //-------------------------------------------------------------------------
    // Suppression Flags (LRM 11.7)
    //-------------------------------------------------------------------------
    uint16_t         suppressed;    /// Bit mask of suppressed checks

    //-------------------------------------------------------------------------
    // Special Flags
    //-------------------------------------------------------------------------
    bool             is_controlled; /// Controlled type (finalization)
    uint8_t          freeze_state;  /// 0=not frozen, 1=frozen
    AST_Node        *freeze_node;   /// Point of freezing
};


///-----------------------------------------------------------------------------
///                   P R E D E F I N E D   T Y P E S
///-----------------------------------------------------------------------------
///
///  Ada83 defines several predefined types in package STANDARD (LRM Annex C):
///    - INTEGER, NATURAL, POSITIVE
///    - BOOLEAN (with literals FALSE, TRUE)
///    - CHARACTER
///    - STRING
///    - FLOAT
///    - DURATION
///
///  These are initialized during interpreter startup and available globally.
///
///-----------------------------------------------------------------------------

extern Type_Descriptor *Type_Integer;       /// INTEGER
extern Type_Descriptor *Type_Natural;       /// NATURAL (subtype of INTEGER)
extern Type_Descriptor *Type_Positive;      /// POSITIVE (subtype of INTEGER)
extern Type_Descriptor *Type_Boolean;       /// BOOLEAN
extern Type_Descriptor *Type_Character;     /// CHARACTER
extern Type_Descriptor *Type_String;        /// STRING
extern Type_Descriptor *Type_Float;         /// FLOAT
extern Type_Descriptor *Type_Universal_Int; /// universal_integer
extern Type_Descriptor *Type_Universal_Real;/// universal_real
extern Type_Descriptor *Type_File;          /// FILE_TYPE


///-----------------------------------------------------------------------------
///                   T Y P E   C O N S T R U C T O R S
///-----------------------------------------------------------------------------

/// @brief Create a new type descriptor
/// @param kind Type classification
/// @param name Type name
/// @return Newly allocated type descriptor
///
Type_Descriptor *Type_New(Type_Kind kind, String_Slice name);


/// @brief Initialize predefined types (package STANDARD)
/// @param sm Semantic context (for symbol registration)
///
/// Creates all predefined types and their associated symbols.
/// Must be called during interpreter initialization.
///
void Types_Initialize(void *sm);


///-----------------------------------------------------------------------------
///                   T Y P E   O P E R A T I O N S
///-----------------------------------------------------------------------------

/// @brief Check if two types are the same type
/// @param t1 First type
/// @param t2 Second type
/// @return true if types are identical (name equivalence)
///
/// Ada uses name equivalence: types are the same only if they have
/// the same declaration. Structurally identical types are still distinct.
///
bool Type_Same(Type_Descriptor *t1, Type_Descriptor *t2);


/// @brief Check if a type is a scalar type
/// @param t Type to check
/// @return true if t is a scalar type (discrete or real)
///
bool Type_Is_Scalar(Type_Descriptor *t);


/// @brief Check if a type is a discrete type
/// @param t Type to check
/// @return true if t is discrete (integer, enumeration, or character)
///
bool Type_Is_Discrete(Type_Descriptor *t);


/// @brief Check if a type is a numeric type
/// @param t Type to check
/// @return true if t is numeric (integer or real)
///
bool Type_Is_Numeric(Type_Descriptor *t);


/// @brief Check if types are compatible for binary operations
/// @param t1     First operand type
/// @param t2     Second operand type
/// @param result Output: resulting type of operation
/// @return Type compatibility score (0 = incompatible)
///
/// Used for overload resolution and implicit conversion checks.
///
int Type_Score_Compatibility(Type_Descriptor *t1,
                             Type_Descriptor *t2,
                             Type_Descriptor *result);


///-----------------------------------------------------------------------------
///                   T Y P E   F R E E Z I N G
///-----------------------------------------------------------------------------
///
///  Type freezing is the process of finalizing a type's representation.
///  After freezing, representation aspects cannot be changed.
///  See Ada83 LRM 13.1 for freezing rules.
///
///-----------------------------------------------------------------------------

/// @brief Freeze a type and compute its representation
/// @param sm   Semantic context
/// @param type Type to freeze
/// @param loc  Source location (for error messages)
///
/// Computes size, alignment, and component offsets for composite types.
/// Also generates implicit operations for records and arrays.
///
void Type_Freeze(void *sm, Type_Descriptor *type, Source_Location loc);


///-----------------------------------------------------------------------------
///                   I M P L I C I T   O P E R A T I O N S
///-----------------------------------------------------------------------------
///
///  Ada83 implicitly declares certain operations for each type:
///    - "=" and "/=" for equality testing
///    - ":=" for assignment (composite types)
///    - Type conversion operations
///
///  These functions generate AST nodes for these implicit operations.
///
///-----------------------------------------------------------------------------

/// @brief Generate implicit "=" operation for a type
/// @param type Type for which to generate equality
/// @param loc  Source location
/// @return Function body node for equality operation
///
AST_Node *Generate_Equality_Op(Type_Descriptor *type, Source_Location loc);


/// @brief Generate implicit ":=" operation for a type
/// @param type Type for which to generate assignment
/// @param loc  Source location
/// @return Procedure body node for assignment operation
///
AST_Node *Generate_Assignment_Op(Type_Descriptor *type, Source_Location loc);


/// @brief Generate implicit initialization for a type
/// @param type Type for which to generate initialization
/// @param loc  Source location
/// @return Function body for default initialization (or NULL)
///
AST_Node *Generate_Init_Op(Type_Descriptor *type, Source_Location loc);


#endif // ADA83_TYPES_H
