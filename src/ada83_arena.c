///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                    M E M O R Y   A R E N A   A L L O C A T O R            --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  Implementation of the bump-pointer arena allocator.                      --
///  See ada83_arena.h for interface documentation.                           --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_arena.h"


///-----------------------------------------------------------------------------
///                   G L O B A L   A R E N A   I N S T A N C E
///-----------------------------------------------------------------------------

Memory_Arena Global_Arena = { NULL, NULL, NULL };


///-----------------------------------------------------------------------------
///                   A L I G N M E N T   C O N S T A N T S
///-----------------------------------------------------------------------------
///
///  All allocations are aligned to 8 bytes to ensure proper alignment for
///  any data type on 64-bit systems. This matches the natural alignment
///  of pointers and 64-bit integers.
///
///-----------------------------------------------------------------------------

#define ALIGNMENT 8
#define ALIGN_UP(size) (((size) + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1))


///-----------------------------------------------------------------------------
///                   A R E N A   A L L O C A T I O N
///-----------------------------------------------------------------------------

/// @brief Arena_Alloc
///
void *Arena_Alloc(size_t size)
{
    // Align the requested size to 8-byte boundary
    size = ALIGN_UP(size);

    // Check if we need to allocate a new block
    if (Global_Arena.base == NULL || Global_Arena.current + size > Global_Arena.end) {
        // Allocate new block (or first block)
        size_t block_size = ARENA_BLOCK_SIZE;

        // Ensure block is large enough for this allocation
        if (size > block_size) {
            block_size = ALIGN_UP(size);
        }

        Global_Arena.base    = malloc(block_size);
        Global_Arena.current = Global_Arena.base;
        Global_Arena.end     = Global_Arena.base + block_size;

        if (Global_Arena.base == NULL) {
            // Allocation failure is fatal for the interpreter
            fprintf(stderr, "Fatal: Arena allocation failed (requested %zu bytes)\n", block_size);
            exit(1);
        }
    }

    // Bump allocation: return current position, advance pointer
    void *result = Global_Arena.current;
    Global_Arena.current += size;

    // Zero-initialize the allocated memory
    // This matches Ada's default initialization for access types (null)
    // and provides predictable behavior for uninitialized fields
    memset(result, 0, size);

    return result;

} // Arena_Alloc


///-----------------------------------------------------------------------------
///                   A R E N A   R E S E T
///-----------------------------------------------------------------------------

/// @brief Arena_Reset
///
void Arena_Reset(void)
{
    // Free the current block (if any)
    // Note: In a more sophisticated implementation, we would maintain
    // a linked list of blocks and free all of them.
    if (Global_Arena.base != NULL) {
        free(Global_Arena.base);
        Global_Arena.base    = NULL;
        Global_Arena.current = NULL;
        Global_Arena.end     = NULL;
    }

} // Arena_Reset


///-----------------------------------------------------------------------------
///                   A R E N A   S T A T I S T I C S
///-----------------------------------------------------------------------------

/// @brief Arena_Stats
///
void Arena_Stats(size_t *used, size_t *available)
{
    if (Global_Arena.base != NULL) {
        *used      = Global_Arena.current - Global_Arena.base;
        *available = Global_Arena.end - Global_Arena.current;
    }
    else {
        *used      = 0;
        *available = 0;
    }

} // Arena_Stats
