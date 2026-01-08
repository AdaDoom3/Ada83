///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///               A B S T R A C T   S Y N T A X   T R E E                     --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  Implementation of AST node operations.                                   --
///  See ada83_ast.h for interface documentation.                             --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_ast.h"
#include "ada83_arena.h"


///-----------------------------------------------------------------------------
///                   N O D E   C O N S T R U C T I O N
///-----------------------------------------------------------------------------

/// @brief AST_New
///
AST_Node *AST_New(AST_Node_Kind kind, Source_Location location)
{
    // Allocate from arena (zero-initialized)
    AST_Node *node = Arena_Alloc(sizeof(AST_Node));

    // Set basic fields
    node->kind     = kind;
    node->location = location;
    node->type     = NULL;
    node->symbol   = NULL;

    // Union is already zero-initialized by Arena_Alloc

    return node;

} // AST_New


///-----------------------------------------------------------------------------
///                   V E C T O R   O P E R A T I O N S
///-----------------------------------------------------------------------------
///
///  Node vectors use a standard doubling growth strategy:
///    - Initial capacity: 8 elements
///    - Growth factor: 2x when full
///
///  This provides amortized O(1) insertion with reasonable memory usage.
///
///-----------------------------------------------------------------------------

/// @brief Node_Vector_Push
///
void Node_Vector_Push(Node_Vector *vec, AST_Node *node)
{
    // Check if growth is needed
    if (vec->count >= vec->capacity) {
        // Double capacity (or set initial capacity to 8)
        vec->capacity = vec->capacity ? vec->capacity << 1 : 8;

        // Reallocate array
        vec->data = realloc(vec->data, vec->capacity * sizeof(AST_Node *));
    }

    // Add element
    vec->data[vec->count++] = node;

} // Node_Vector_Push
