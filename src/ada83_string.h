///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                     S T R I N G   H A N D L I N G                         --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module provides string handling utilities for the Ada83 interpreter. --
///  Ada83 identifiers and string literals are case-insensitive, which        --
///  requires special comparison and hashing functions.                       --
///                                                                           --
///  Per Ada83 LRM 2.3, identifiers are case-insensitive:                     --
///    "The same identifier may be written with different capitalization,     --
///     and all such forms are equivalent."                                   --
///                                                                           --
///  This module also provides string interning via the arena allocator       --
///  for efficient storage of frequently-used strings.                        --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_STRING_H
#define ADA83_STRING_H

#include "ada83_common.h"
#include "ada83_arena.h"


///-----------------------------------------------------------------------------
///                   S T R I N G   D U P L I C A T I O N
///-----------------------------------------------------------------------------

/// @brief Duplicate a string slice into arena-allocated storage
/// @param str The string slice to duplicate
/// @return A new string slice with arena-allocated storage
///
/// Creates a persistent copy of the input string in the global arena.
/// The returned string has its own storage and will not be invalidated
/// if the original string is deallocated.
///
/// The copy is NOT null-terminated (preserves Ada string semantics).
///
String_Slice String_Dup(String_Slice str);


///-----------------------------------------------------------------------------
///                   C A S E - I N S E N S I T I V E   C O M P A R I S O N
///-----------------------------------------------------------------------------

/// @brief Compare two strings for equality (case-insensitive)
/// @param a First string
/// @param b Second string
/// @return true if strings are equal (ignoring case), false otherwise
///
/// Implements Ada83's identifier comparison rules (LRM 2.3).
/// Uses the ASCII case-folding rules for Latin letters A-Z.
///
/// Examples:
///   String_Equal_CI("Hello", "HELLO") => true
///   String_Equal_CI("Ada83", "ADA83") => true
///   String_Equal_CI("foo", "bar")     => false
///
bool String_Equal_CI(String_Slice a, String_Slice b);


///-----------------------------------------------------------------------------
///                   C A S E   C O N V E R S I O N
///-----------------------------------------------------------------------------

/// @brief Convert a string to lowercase into a static buffer
/// @param str The string to convert
/// @return Null-terminated lowercase string in a static rotating buffer
///
/// Uses a rotating set of 8 static buffers, allowing up to 8 conversions
/// to be active simultaneously without overwriting each other.
///
/// Useful for keyword lookup and error message generation.
/// Maximum string length is 255 characters (truncated if longer).
///
/// Warning: Not thread-safe due to static buffers.
///
char *String_To_Lower(String_Slice str);


///-----------------------------------------------------------------------------
///                   H A S H I N G
///-----------------------------------------------------------------------------

/// @brief Compute a case-insensitive hash of a string
/// @param str The string to hash
/// @return 64-bit hash value
///
/// Uses FNV-1a (Fowler-Noll-Vo) hash algorithm with case-folding.
/// The hash is case-insensitive to support Ada's identifier rules.
///
/// FNV-1a was chosen for:
///   - Good distribution for short strings (typical identifier lengths)
///   - Simple implementation
///   - Proven avalanche properties
///
/// Reference: http://www.isthe.com/chongo/tech/comp/fnv/
///
uint64_t String_Hash(String_Slice str);


///-----------------------------------------------------------------------------
///                   F N V - 1 a   C O N S T A N T S
///-----------------------------------------------------------------------------
///
///  The FNV-1a hash constants for 64-bit hashing.
///  These are mathematically-derived primes with good distribution.
///
///-----------------------------------------------------------------------------

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME        1099511628211ULL


///-----------------------------------------------------------------------------
///                   E R R O R   R E P O R T I N G
///-----------------------------------------------------------------------------

/// @brief Report a fatal error at a source location and exit
/// @param loc    Source location for error message
/// @param format printf-style format string
/// @param ...    Format arguments
///
/// Prints error in standard compiler format:
///   file:line:column: message
///
/// Increments Global_Error_Count and terminates the interpreter.
/// This function does not return.
///
_Noreturn void Fatal_Error(Source_Location loc, const char *format, ...);


#endif // ADA83_STRING_H
