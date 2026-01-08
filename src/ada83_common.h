///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                     C O M M O N   D E F I N I T I O N S                   --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module provides common type definitions, macros, and forward        --
///  declarations used throughout the Ada83 interpreter implementation.       --
///                                                                           --
///  The Ada83 interpreter implements a substantial subset of the Ada 83      --
///  programming language specification (ANSI/MIL-STD-1815A-1983).            --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_COMMON_H
#define ADA83_COMMON_H

///-----------------------------------------------------------------------------
///                         S T A N D A R D   I N C L U D E S
///-----------------------------------------------------------------------------
///
///  POSIX feature test macro enables extended POSIX APIs including:
///    - strndup, strnlen for string operations
///    - Additional pthread functionality for tasking support
///
///-----------------------------------------------------------------------------

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <setjmp.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <errno.h>


///-----------------------------------------------------------------------------
///                   I N C L U D E   P A T H   M A N A G E M E N T
///-----------------------------------------------------------------------------
///
///  Ada83 supports a search path mechanism for locating library units,
///  similar to GNAT's ADA_INCLUDE_PATH. Up to 32 include directories
///  can be specified via command-line options.
///
///  Reference: Ada83 LRM 10.1 - Compilation Units - Library Units
///
///-----------------------------------------------------------------------------

#define MAX_INCLUDE_PATHS 32

extern const char *Include_Paths[MAX_INCLUDE_PATHS];
extern int         Include_Path_Count;


///-----------------------------------------------------------------------------
///                         C O M P I L E R   F L A G S
///-----------------------------------------------------------------------------
///
///  Runtime check flags corresponding to Ada83 pragma SUPPRESS options.
///  These allow selective suppression of runtime checks as specified in
///  Ada83 LRM 11.7 - Suppressing Checks.
///
///  CHECK_OVERFLOW  : Suppress Overflow_Check     (numeric overflow)
///  CHECK_RANGE     : Suppress Range_Check        (subtype constraints)
///  CHECK_INDEX     : Suppress Index_Check        (array bounds)
///  CHECK_DISCRIM   : Suppress Discriminant_Check (variant records)
///  CHECK_LENGTH    : Suppress Length_Check       (array assignment)
///  CHECK_DIVISION  : Suppress Division_Check     (division by zero)
///  CHECK_ELAB      : Suppress Elaboration_Check  (package elaboration)
///  CHECK_ACCESS    : Suppress Access_Check       (null pointer)
///  CHECK_STORAGE   : Suppress Storage_Check      (heap allocation)
///
///-----------------------------------------------------------------------------

typedef enum {
    CHECK_OVERFLOW  = 1 << 0,   /// Numeric overflow checking
    CHECK_RANGE     = 1 << 1,   /// Subtype range constraint checking
    CHECK_INDEX     = 1 << 2,   /// Array index bounds checking
    CHECK_DISCRIM   = 1 << 3,   /// Discriminant constraint checking
    CHECK_LENGTH    = 1 << 4,   /// Array length matching checking
    CHECK_DIVISION  = 1 << 5,   /// Division by zero checking
    CHECK_ELAB      = 1 << 6,   /// Elaboration order checking
    CHECK_ACCESS    = 1 << 7,   /// Null access value checking
    CHECK_STORAGE   = 1 << 8    /// Storage allocation checking
} Runtime_Check_Flag;


///-----------------------------------------------------------------------------
///                   S O U R C E   L O C A T I O N
///-----------------------------------------------------------------------------
///
///  Source location tracking for error messages and debugging.
///  Corresponds to GNAT's Sloc (Source Location) concept.
///
///  Fields:
///    line   : 1-based line number in source file
///    column : 1-based column number in source line
///    file   : Pointer to source file name string
///
///-----------------------------------------------------------------------------

typedef struct {
    uint32_t    line;       /// Source line number (1-based)
    uint32_t    column;     /// Source column number (1-based)
    const char *file;       /// Source file name
} Source_Location;


///-----------------------------------------------------------------------------
///                   S T R I N G   S L I C E
///-----------------------------------------------------------------------------
///
///  An Ada-style string slice: pointer to characters with explicit length.
///  Unlike C strings, Ada strings are not null-terminated; they carry their
///  bounds explicitly. This structure represents an unconstrained string
///  value as described in Ada83 LRM 3.6.3.
///
///  The implementation uses a (pointer, length) pair which is more efficient
///  than the full Ada string descriptor (lower_bound, upper_bound, data).
///
///-----------------------------------------------------------------------------

typedef struct {
    const char *data;       /// Pointer to first character (not null-terminated)
    uint32_t    length;     /// Number of characters in string
} String_Slice;

/// @brief Create a string slice from a string literal
/// @note  The -1 accounts for the null terminator in the literal
#define STR(literal) ((String_Slice){(literal), sizeof(literal) - 1})

/// @brief Null/empty string slice constant
#define NULL_STRING ((String_Slice){NULL, 0})


///-----------------------------------------------------------------------------
///                   F O R W A R D   D E C L A R A T I O N S
///-----------------------------------------------------------------------------
///
///  Forward declarations for major interpreter components.
///  These correspond to Ada package specifications that would be
///  `with`ed by dependent units.
///
///-----------------------------------------------------------------------------

/// Multiprecision integer for unbounded integer arithmetic (universal_integer)
typedef struct Unbounded_Integer   Unbounded_Integer;

/// Rational number for universal_real computation
typedef struct Rational_Number     Rational_Number;

/// Abstract Syntax Tree node
typedef struct AST_Node            AST_Node;

/// Type descriptor for Ada type system
typedef struct Type_Descriptor     Type_Descriptor;

/// Symbol table entry
typedef struct Symbol_Entry        Symbol_Entry;

/// Representation clause record
typedef struct Rep_Clause          Rep_Clause;

/// Library unit descriptor
typedef struct Library_Unit        Library_Unit;

/// Generic template descriptor
typedef struct Generic_Template    Generic_Template;


///-----------------------------------------------------------------------------
///                   D Y N A M I C   A R R A Y   M A C R O
///-----------------------------------------------------------------------------
///
///  Generic dynamic array (vector) implementation macro.
///  Ada83 doesn't have built-in dynamic arrays, but the interpreter needs
///  them internally for managing lists of nodes, symbols, etc.
///
///  This macro generates a type-safe push function for each vector type.
///  The growth strategy doubles capacity when full, starting at 8 elements.
///
///  Parameters:
///    vec_type  : The vector structure type name
///    elem_type : The element type stored in the vector
///    func_name : Name for the generated push function
///
///-----------------------------------------------------------------------------

#define VECTOR_PUSH_IMPL(vec_type, elem_type, func_name)                       \
    static void func_name(vec_type *vec, elem_type elem) {                     \
        if (vec->count >= vec->capacity) {                                     \
            vec->capacity = vec->capacity ? vec->capacity << 1 : 8;            \
            vec->data = realloc(vec->data, vec->capacity * sizeof(elem_type)); \
        }                                                                      \
        vec->data[vec->count++] = elem;                                        \
    }


///-----------------------------------------------------------------------------
///                   E R R O R   H A N D L I N G
///-----------------------------------------------------------------------------
///
///  Global error counter for compilation/interpretation errors.
///  Non-zero value indicates errors were encountered.
///
///-----------------------------------------------------------------------------

extern int Global_Error_Count;


///-----------------------------------------------------------------------------
///                   S E P A R A T E   C O M P I L A T I O N
///-----------------------------------------------------------------------------
///
///  Support for Ada83 separate compilation (LRM Chapter 10).
///  When processing a subunit (separate body), this holds the parent
///  package name for proper name resolution.
///
///-----------------------------------------------------------------------------

extern String_Slice Separate_Parent_Package;


#endif // ADA83_COMMON_H
