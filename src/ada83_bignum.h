///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///          U N B O U N D E D   I N T E G E R   A R I T H M E T I C          --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module implements multiprecision integer arithmetic required for    --
///  Ada83's universal_integer type. Per Ada83 LRM 3.5.4, integer literals    --
///  are of type universal_integer, which must support arbitrarily large      --
///  values at compile time.                                                  --
///                                                                           --
///  Implementation Notes:                                                    --
///    - Uses 64-bit limbs for efficient computation on modern hardware       --
///    - Karatsuba multiplication for large operands (threshold: 20 limbs)    --
///    - Sign-magnitude representation (separate sign flag)                   --
///                                                                           --
///  Reference: GNAT's s-bignum.ads provides similar functionality            --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_BIGNUM_H
#define ADA83_BIGNUM_H

#include "ada83_common.h"


///-----------------------------------------------------------------------------
///                   U N B O U N D E D   I N T E G E R   T Y P E
///-----------------------------------------------------------------------------
///
///  Unbounded_Integer represents arbitrary precision signed integers.
///
///  The internal representation uses an array of 64-bit unsigned "limbs"
///  stored in little-endian order (least significant limb first).
///
///  Mathematical value = sign * SUM(limbs[i] * 2^(64*i)) for i in 0..count-1
///
///  Invariants:
///    - Leading zeros are normalized away (limbs[count-1] != 0 unless count=0)
///    - Zero is represented with count=0 and is_negative=false
///    - Capacity >= count (sufficient space for current value)
///
///-----------------------------------------------------------------------------

struct Unbounded_Integer {
    uint64_t *limbs;        /// Array of 64-bit limbs (little-endian order)
    uint32_t  count;        /// Number of significant limbs
    uint32_t  capacity;     /// Allocated capacity in limbs
    bool      is_negative;  /// Sign flag: true if value < 0
};


///-----------------------------------------------------------------------------
///                   R A T I O N A L   N U M B E R   T Y P E
///-----------------------------------------------------------------------------
///
///  Rational_Number represents exact rational arithmetic for universal_real.
///  Per Ada83 LRM 3.5.6, real literals are of type universal_real which
///  requires exact representation during static expression evaluation.
///
///  The rational is stored as numerator/denominator where both are
///  unbounded integers. The representation is kept in lowest terms
///  (GCD(numerator, denominator) = 1).
///
///-----------------------------------------------------------------------------

struct Rational_Number {
    Unbounded_Integer *numerator;    /// Numerator (sign carried here)
    Unbounded_Integer *denominator;  /// Denominator (always positive)
};


///-----------------------------------------------------------------------------
///                   C O N S T R U C T O R S
///-----------------------------------------------------------------------------

/// @brief Allocate a new unbounded integer with specified capacity
/// @param initial_capacity Number of 64-bit limbs to allocate
/// @return Newly allocated integer initialized to zero
///
/// Creates an unbounded integer with pre-allocated storage.
/// The value is initialized to zero (count=0, is_negative=false).
///
Unbounded_Integer *Unbounded_New(uint32_t initial_capacity);


/// @brief Free an unbounded integer and its storage
/// @param value The integer to deallocate (may be NULL)
///
/// Releases all memory associated with the unbounded integer.
/// Safe to call with NULL pointer (no-op).
///
void Unbounded_Free(Unbounded_Integer *value);


/// @brief Parse a decimal string into an unbounded integer
/// @param decimal_string Null-terminated decimal representation
/// @return Newly allocated integer with parsed value
///
/// Parses a string of the form [-]?[0-9_]+ into an unbounded integer.
/// Underscores are ignored (Ada83 numeric literal separator).
/// Leading '+' or '-' determines sign.
///
/// Example: "-123_456" => -123456
///
Unbounded_Integer *Unbounded_From_Decimal(const char *decimal_string);


///-----------------------------------------------------------------------------
///                   S T O R A G E   M A N A G E M E N T
///-----------------------------------------------------------------------------

/// @brief Ensure the integer has at least the specified capacity
/// @param value    The integer to potentially grow
/// @param required Minimum required capacity in limbs
///
/// If current capacity is insufficient, reallocates with the new capacity.
/// Preserves existing value. New limbs are zero-initialized.
///
void Unbounded_Grow(Unbounded_Integer *value, uint32_t required);


/// @brief Normalize the integer by removing leading zero limbs
/// @param value The integer to normalize (modified in place)
///
/// Reduces count to exclude leading zeros. Ensures the representation
/// invariant: limbs[count-1] != 0 (unless value is zero).
/// Also ensures zero values have is_negative = false.
///
/// This macro is used after arithmetic operations to maintain invariants.
///
#define UNBOUNDED_NORMALIZE(value) do {                                        \
    while ((value)->count > 0 && !(value)->limbs[(value)->count - 1])          \
        (value)->count--;                                                      \
    if (!(value)->count)                                                       \
        (value)->is_negative = false;                                          \
} while (0)


///-----------------------------------------------------------------------------
///                   C O M P A R I S O N   O P E R A T I O N S
///-----------------------------------------------------------------------------

/// @brief Compare absolute values of two unbounded integers
/// @param left  First operand
/// @param right Second operand
/// @return -1 if |left| < |right|, 0 if equal, +1 if |left| > |right|
///
/// Performs unsigned comparison, ignoring sign flags.
/// Used internally for implementing signed comparison and subtraction.
///
int Unbounded_Compare_Abs(const Unbounded_Integer *left,
                          const Unbounded_Integer *right);


///-----------------------------------------------------------------------------
///                   A R I T H M E T I C   O P E R A T I O N S
///-----------------------------------------------------------------------------
///
///  All arithmetic operations store their result in the first parameter.
///  The result may alias one of the operands.
///
///-----------------------------------------------------------------------------

/// @brief Add two unbounded integers: result := left + right
/// @param result Where to store the sum
/// @param left   First addend
/// @param right  Second addend
///
/// Implements signed addition using the standard algorithm:
///   - Same signs: add magnitudes, keep sign
///   - Different signs: subtract smaller from larger, use larger's sign
///
void Unbounded_Add(Unbounded_Integer       *result,
                   const Unbounded_Integer *left,
                   const Unbounded_Integer *right);


/// @brief Subtract two unbounded integers: result := left - right
/// @param result Where to store the difference
/// @param left   Minuend
/// @param right  Subtrahend
///
/// Implemented as: left + (-right)
///
void Unbounded_Sub(Unbounded_Integer       *result,
                   const Unbounded_Integer *left,
                   const Unbounded_Integer *right);


/// @brief Multiply two unbounded integers: result := left * right
/// @param result Where to store the product
/// @param left   First factor
/// @param right  Second factor
///
/// Uses Karatsuba algorithm for large operands (>= 20 limbs each),
/// falling back to schoolbook multiplication for smaller values.
///
/// Karatsuba reduces O(n^2) to O(n^1.585) for large multiplications.
///
void Unbounded_Mul(Unbounded_Integer       *result,
                   const Unbounded_Integer *left,
                   const Unbounded_Integer *right);


///-----------------------------------------------------------------------------
///                   I N T E R N A L   P R I M I T I V E S
///-----------------------------------------------------------------------------
///
///  Low-level operations used by the arithmetic functions.
///  These operate on unsigned magnitudes only.
///
///-----------------------------------------------------------------------------

/// @brief Add with carry: result = a + b + carry_in, return carry_out
/// @param a        First operand
/// @param b        Second operand
/// @param carry_in Input carry (0 or 1)
/// @param result   Output: low 64 bits of sum
/// @return Carry out (0 or 1)
///
/// Uses 128-bit arithmetic for exact computation.
/// This is the fundamental building block for multi-limb addition.
///
static inline uint64_t Add_With_Carry(uint64_t  a,
                                      uint64_t  b,
                                      uint64_t  carry_in,
                                      uint64_t *result)
{
    __uint128_t sum = (__uint128_t)a + b + carry_in;
    *result = (uint64_t)sum;
    return (uint64_t)(sum >> 64);

} // Add_With_Carry


/// @brief Subtract with borrow: result = a - b - borrow_in, return borrow_out
/// @param a         First operand (minuend)
/// @param b         Second operand (subtrahend)
/// @param borrow_in Input borrow (0 or 1)
/// @param result    Output: low 64 bits of difference
/// @return Borrow out (0 or 1)
///
/// Uses 128-bit arithmetic to detect underflow.
/// The borrow is the negation of the high 64 bits.
///
static inline uint64_t Sub_With_Borrow(uint64_t  a,
                                       uint64_t  b,
                                       uint64_t  borrow_in,
                                       uint64_t *result)
{
    __uint128_t diff = (__uint128_t)a - b - borrow_in;
    *result = (uint64_t)diff;
    return -(uint64_t)(diff >> 64);  // Negate to get borrow

} // Sub_With_Borrow


#endif // ADA83_BIGNUM_H
