///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                    M E M O R Y   A R E N A   A L L O C A T O R            --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module implements a simple bump-pointer arena allocator optimized   --
///  for the allocation patterns of a compiler/interpreter: many small        --
///  allocations that share a common lifetime (the compilation session).      --
///                                                                           --
///  Benefits over malloc:                                                    --
///    - O(1) allocation (just bump a pointer)                                --
///    - No per-object overhead                                               --
///    - Excellent cache locality                                             --
///    - Bulk deallocation (free entire arena at once)                        --
///                                                                           --
///  This is similar to GNAT's storage pool mechanism described in the        --
///  Ada83 LRM 13.3, but implemented directly in C for the interpreter.       --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_ARENA_H
#define ADA83_ARENA_H

#include "ada83_common.h"


///-----------------------------------------------------------------------------
///                   A R E N A   D E S C R I P T O R
///-----------------------------------------------------------------------------
///
///  The arena uses a simple linear allocation strategy:
///    - Allocate a large block of memory (16MB by default)
///    - Bump the 'current' pointer for each allocation
///    - Align all allocations to 8-byte boundaries
///    - When block is exhausted, allocate a new block (linked list)
///
///  Memory layout:
///    [allocated objects...][free space...][end]
///     ^                     ^              ^
///     base                  current        end
///
///-----------------------------------------------------------------------------

typedef struct {
    char *base;        /// Start of current memory block
    char *current;     /// Next allocation position (bump pointer)
    char *end;         /// End of current memory block
} Memory_Arena;


///-----------------------------------------------------------------------------
///                   D E F A U L T   B L O C K   S I Z E
///-----------------------------------------------------------------------------
///
///  16 MB block size is chosen to:
///    - Fit comfortably in memory for typical compilation units
///    - Minimize number of block allocations
///    - Be large enough for complex packages
///
///-----------------------------------------------------------------------------

#define ARENA_BLOCK_SIZE (1 << 24)  // 16 MB


///-----------------------------------------------------------------------------
///                   G L O B A L   A R E N A
///-----------------------------------------------------------------------------
///
///  The interpreter uses a single global arena for all AST nodes, types,
///  symbols, and other compilation-time data structures. This simplifies
///  memory management and ensures all data has the same lifetime.
///
///-----------------------------------------------------------------------------

extern Memory_Arena Global_Arena;


///-----------------------------------------------------------------------------
///                   A L L O C A T I O N   F U N C T I O N S
///-----------------------------------------------------------------------------

/// @brief Allocate memory from the global arena
/// @param size Number of bytes to allocate
/// @return Pointer to zero-initialized memory, aligned to 8 bytes
///
/// This is the primary allocation function for interpreter data structures.
/// The returned memory is zero-initialized, which matches Ada's default
/// initialization semantics for access types.
///
/// If the current block is exhausted, a new block is allocated.
/// This function never returns NULL (allocation failure is fatal).
///
/// Example usage:
///   AST_Node *node = Arena_Alloc(sizeof(AST_Node));
///
void *Arena_Alloc(size_t size);


/// @brief Reset the arena, deallocating all memory
///
/// Frees all memory allocated through Arena_Alloc. After this call,
/// all previously allocated pointers become invalid.
///
/// Typically called at the end of processing a compilation unit.
///
void Arena_Reset(void);


/// @brief Get current arena usage statistics
/// @param used      Output: bytes currently allocated
/// @param available Output: bytes still available in current block
///
void Arena_Stats(size_t *used, size_t *available);


#endif // ADA83_ARENA_H
