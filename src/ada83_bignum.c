///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///          U N B O U N D E D   I N T E G E R   A R I T H M E T I C          --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  Implementation of multiprecision integer arithmetic for universal_integer --
///  computation. See ada83_bignum.h for interface documentation.             --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_bignum.h"


///-----------------------------------------------------------------------------
///                   K A R A T S U B A   T H R E S H O L D
///-----------------------------------------------------------------------------
///
///  The crossover point where Karatsuba multiplication becomes more efficient
///  than schoolbook multiplication. Below this threshold, the overhead of
///  Karatsuba's recursive calls and temporary allocations exceeds its
///  asymptotic advantage.
///
///  Value determined empirically for 64-bit limbs on modern processors.
///  GNAT's multiprecision library uses a similar threshold.
///
///-----------------------------------------------------------------------------

#define KARATSUBA_THRESHOLD 20


///-----------------------------------------------------------------------------
///                   C O N S T R U C T O R S
///-----------------------------------------------------------------------------

/// @brief Unbounded_New
///
Unbounded_Integer *Unbounded_New(uint32_t initial_capacity)
{
    // Allocate the descriptor structure
    Unbounded_Integer *result = malloc(sizeof(Unbounded_Integer));

    // Allocate and zero-initialize the limb array
    result->limbs    = calloc(initial_capacity, sizeof(uint64_t));
    result->count    = 0;
    result->capacity = initial_capacity;
    result->is_negative = false;

    return result;

} // Unbounded_New


/// @brief Unbounded_Free
///
void Unbounded_Free(Unbounded_Integer *value)
{
    if (value != NULL) {
        free(value->limbs);
        free(value);
    }

} // Unbounded_Free


/// @brief Unbounded_Grow
///
void Unbounded_Grow(Unbounded_Integer *value, uint32_t required)
{
    if (required > value->capacity) {
        // Reallocate with new capacity
        value->limbs = realloc(value->limbs, required * sizeof(uint64_t));

        // Zero-initialize the new portion
        memset(value->limbs + value->capacity, 0,
               (required - value->capacity) * sizeof(uint64_t));

        value->capacity = required;
    }

} // Unbounded_Grow


///-----------------------------------------------------------------------------
///                   C O M P A R I S O N   O P E R A T I O N S
///-----------------------------------------------------------------------------

/// @brief Unbounded_Compare_Abs
///
int Unbounded_Compare_Abs(const Unbounded_Integer *left,
                          const Unbounded_Integer *right)
{
    // Different number of limbs => different magnitudes
    if (left->count != right->count) {
        return (left->count > right->count) ? 1 : -1;
    }

    // Same number of limbs - compare from most significant
    for (int i = (int)left->count - 1; i >= 0; i--) {
        if (left->limbs[i] != right->limbs[i]) {
            return (left->limbs[i] > right->limbs[i]) ? 1 : -1;
        }
    }

    // All limbs equal
    return 0;

} // Unbounded_Compare_Abs


///-----------------------------------------------------------------------------
///                   I N T E R N A L   A D D / S U B
///-----------------------------------------------------------------------------
///
///  These internal functions operate on unsigned magnitudes and implement
///  the core addition/subtraction logic. The public functions handle signs.
///
///-----------------------------------------------------------------------------

/// @brief Add unsigned magnitudes: result := |left| + |right|
/// @param result Where to store the sum (pre-allocated with sufficient space)
/// @param left   First operand
/// @param right  Second operand
/// @param is_add True for addition, false for subtraction
///
/// This unified function handles both add and subtract of magnitudes.
/// For subtraction, assumes |left| >= |right|.
///
static void Unsigned_Binary_Op(Unbounded_Integer       *result,
                               const Unbounded_Integer *left,
                               const Unbounded_Integer *right,
                               bool                     is_add)
{
    if (is_add) {
        // Addition: result may need one extra limb for carry
        uint32_t max_count = (left->count > right->count ? left->count : right->count) + 1;
        Unbounded_Grow(result, max_count);

        uint64_t carry = 0;
        uint32_t i;

        // Add corresponding limbs plus carry
        for (i = 0; i < left->count || i < right->count || carry; i++) {
            uint64_t a = (i < left->count)  ? left->limbs[i]  : 0;
            uint64_t b = (i < right->count) ? right->limbs[i] : 0;
            carry = Add_With_Carry(a, b, carry, &result->limbs[i]);
        }

        result->count = i;
    }
    else {
        // Subtraction: |left| >= |right|, so result fits in left->count limbs
        Unbounded_Grow(result, left->count);

        uint64_t borrow = 0;

        for (uint32_t i = 0; i < left->count; i++) {
            uint64_t a = left->limbs[i];
            uint64_t b = (i < right->count) ? right->limbs[i] : 0;
            borrow = Sub_With_Borrow(a, b, borrow, &result->limbs[i]);
        }

        result->count = left->count;
    }

    // Remove leading zeros
    UNBOUNDED_NORMALIZE(result);

} // Unsigned_Binary_Op


/// @brief Internal addition of unsigned magnitudes
///
static void Unsigned_Add(Unbounded_Integer       *result,
                         const Unbounded_Integer *left,
                         const Unbounded_Integer *right)
{
    Unsigned_Binary_Op(result, left, right, true);

} // Unsigned_Add


/// @brief Internal subtraction of unsigned magnitudes (|left| >= |right|)
///
static void Unsigned_Sub(Unbounded_Integer       *result,
                         const Unbounded_Integer *left,
                         const Unbounded_Integer *right)
{
    Unsigned_Binary_Op(result, left, right, false);

} // Unsigned_Sub


///-----------------------------------------------------------------------------
///                   S I G N E D   A D D I T I O N
///-----------------------------------------------------------------------------

/// @brief Unbounded_Add
///
void Unbounded_Add(Unbounded_Integer       *result,
                   const Unbounded_Integer *left,
                   const Unbounded_Integer *right)
{
    // Same signs: add magnitudes, keep the sign
    if (left->is_negative == right->is_negative) {
        Unsigned_Add(result, left, right);
        result->is_negative = left->is_negative;
    }
    else {
        // Different signs: subtract smaller magnitude from larger
        int cmp = Unbounded_Compare_Abs(left, right);

        if (cmp >= 0) {
            // |left| >= |right|: result = |left| - |right|, sign = left's sign
            Unsigned_Sub(result, left, right);
            result->is_negative = left->is_negative;
        }
        else {
            // |left| < |right|: result = |right| - |left|, sign = right's sign
            Unsigned_Sub(result, right, left);
            result->is_negative = right->is_negative;
        }
    }

} // Unbounded_Add


///-----------------------------------------------------------------------------
///                   S I G N E D   S U B T R A C T I O N
///-----------------------------------------------------------------------------

/// @brief Unbounded_Sub
///
void Unbounded_Sub(Unbounded_Integer       *result,
                   const Unbounded_Integer *left,
                   const Unbounded_Integer *right)
{
    // Subtraction is addition with negated right operand
    // Create a temporary with flipped sign
    Unbounded_Integer negated_right = *right;
    negated_right.is_negative = !right->is_negative;

    Unbounded_Add(result, left, &negated_right);

} // Unbounded_Sub


///-----------------------------------------------------------------------------
///                   S C H O O L B O O K   M U L T I P L I C A T I O N
///-----------------------------------------------------------------------------
///
///  Classical O(n*m) multiplication algorithm. Each limb of the multiplier
///  is multiplied by the entire multiplicand, with partial products
///  accumulated into the result.
///
///  This is efficient for small operands but becomes slow for large numbers.
///
///-----------------------------------------------------------------------------

/// @brief Multiply using schoolbook algorithm
/// @param result Where to store the product
/// @param left   First factor
/// @param right  Second factor
///
static void Multiply_Schoolbook(Unbounded_Integer       *result,
                                const Unbounded_Integer *left,
                                const Unbounded_Integer *right)
{
    // Result has at most left->count + right->count limbs
    uint32_t result_capacity = left->count + right->count;
    Unbounded_Grow(result, result_capacity);

    // Zero-initialize result
    memset(result->limbs, 0, result_capacity * sizeof(uint64_t));

    // Multiply each limb of left by all of right
    for (uint32_t i = 0; i < left->count; i++) {
        uint64_t carry = 0;

        for (uint32_t j = 0; j < right->count; j++) {
            // 128-bit product of two 64-bit limbs
            __uint128_t product = (__uint128_t)left->limbs[i] * right->limbs[j]
                                + result->limbs[i + j]
                                + carry;

            result->limbs[i + j] = (uint64_t)product;
            carry = (uint64_t)(product >> 64);
        }

        // Store final carry
        result->limbs[i + right->count] = carry;
    }

    result->count = result_capacity;

    // Determine sign: negative iff exactly one operand is negative
    result->is_negative = (left->is_negative != right->is_negative);

    UNBOUNDED_NORMALIZE(result);

} // Multiply_Schoolbook


///-----------------------------------------------------------------------------
///                   K A R A T S U B A   M U L T I P L I C A T I O N
///-----------------------------------------------------------------------------
///
///  Karatsuba's divide-and-conquer algorithm reduces multiplication from
///  O(n^2) to O(n^log2(3)) = O(n^1.585) by trading multiplications for
///  additions.
///
///  For numbers split as: A = A1*B^m + A0,  B = B1*B^m + B0
///
///  Classical:     A*B = A1*B1*B^2m + (A1*B0 + A0*B1)*B^m + A0*B0
///                 (4 multiplications of half-size)
///
///  Karatsuba:     Z0 = A0*B0
///                 Z2 = A1*B1
///                 Z1 = (A0+A1)*(B0+B1) - Z0 - Z2 = A1*B0 + A0*B1
///                 A*B = Z2*B^2m + Z1*B^m + Z0
///                 (3 multiplications of half-size)
///
///  Reference: See GNAT's implementation in System.Generic_Bignums
///
///-----------------------------------------------------------------------------

/// @brief Multiply using Karatsuba algorithm
/// @param result Where to store the product
/// @param left   First factor
/// @param right  Second factor
///
static void Multiply_Karatsuba(Unbounded_Integer       *result,
                               const Unbounded_Integer *left,
                               const Unbounded_Integer *right)
{
    uint32_t n = left->count > right->count ? left->count : right->count;

    // Base case: fall back to schoolbook for small operands
    if (n < KARATSUBA_THRESHOLD) {
        Multiply_Schoolbook(result, left, right);
        return;
    }

    // Split point: half of the larger operand
    uint32_t m = n / 2;

    // Split left into low (A0) and high (A1) parts
    // A = A1 * 2^(64*m) + A0
    Unbounded_Integer left_low = {
        .limbs       = left->limbs,
        .count       = (left->count > m) ? m : left->count,
        .capacity    = left->capacity,
        .is_negative = false
    };

    Unbounded_Integer left_high = {
        .limbs       = (left->count > m) ? left->limbs + m : NULL,
        .count       = (left->count > m) ? left->count - m : 0,
        .capacity    = 0,
        .is_negative = false
    };

    // Split right into low (B0) and high (B1) parts
    Unbounded_Integer right_low = {
        .limbs       = right->limbs,
        .count       = (right->count > m) ? m : right->count,
        .capacity    = right->capacity,
        .is_negative = false
    };

    Unbounded_Integer right_high = {
        .limbs       = (right->count > m) ? right->limbs + m : NULL,
        .count       = (right->count > m) ? right->count - m : 0,
        .capacity    = 0,
        .is_negative = false
    };

    // Allocate temporaries for the three recursive products
    Unbounded_Integer *z0 = Unbounded_New(left_low.count + right_low.count);
    Unbounded_Integer *z2 = Unbounded_New(left_high.count + right_high.count);
    Unbounded_Integer *z1 = Unbounded_New(n * 2);

    // Z0 = A0 * B0
    Multiply_Karatsuba(z0, &left_low, &right_low);

    // Z2 = A1 * B1
    Multiply_Karatsuba(z2, &left_high, &right_high);

    // Compute (A0 + A1) and (B0 + B1)
    Unbounded_Integer *left_sum  = Unbounded_New(m + 1);
    Unbounded_Integer *right_sum = Unbounded_New(m + 1);

    Unbounded_Add(left_sum, &left_low, &left_high);
    Unbounded_Add(right_sum, &right_low, &right_high);

    // Z1 = (A0 + A1) * (B0 + B1)
    Multiply_Karatsuba(z1, left_sum, right_sum);

    // Z1 = Z1 - Z0 - Z2 = A0*B1 + A1*B0
    Unbounded_Sub(z1, z1, z0);
    Unbounded_Sub(z1, z1, z2);

    // Combine: result = Z2 * B^(2m) + Z1 * B^m + Z0
    Unbounded_Grow(result, 2 * n);
    memset(result->limbs, 0, 2 * n * sizeof(uint64_t));

    // Add Z0 at position 0
    for (uint32_t i = 0; i < z0->count; i++) {
        result->limbs[i] = z0->limbs[i];
    }

    // Add Z1 at position m (with carry propagation)
    uint64_t carry = 0;
    for (uint32_t i = 0; i < z1->count || carry; i++) {
        uint64_t val = result->limbs[m + i]
                     + (i < z1->count ? z1->limbs[i] : 0)
                     + carry;
        result->limbs[m + i] = val;
        carry = (val < result->limbs[m + i]) ? 1 : 0;
    }

    // Add Z2 at position 2*m (with carry propagation)
    carry = 0;
    for (uint32_t i = 0; i < z2->count || carry; i++) {
        uint64_t val = result->limbs[2 * m + i]
                     + (i < z2->count ? z2->limbs[i] : 0)
                     + carry;
        result->limbs[2 * m + i] = val;
        carry = (val < result->limbs[2 * m + i]) ? 1 : 0;
    }

    result->count = 2 * n;

    // Sign of product
    result->is_negative = (left->is_negative != right->is_negative);

    UNBOUNDED_NORMALIZE(result);

    // Clean up temporaries
    Unbounded_Free(z0);
    Unbounded_Free(z1);
    Unbounded_Free(z2);
    Unbounded_Free(left_sum);
    Unbounded_Free(right_sum);

} // Multiply_Karatsuba


///-----------------------------------------------------------------------------
///                   P U B L I C   M U L T I P L I C A T I O N
///-----------------------------------------------------------------------------

/// @brief Unbounded_Mul
///
void Unbounded_Mul(Unbounded_Integer       *result,
                   const Unbounded_Integer *left,
                   const Unbounded_Integer *right)
{
    // Delegate to Karatsuba which handles the threshold internally
    Multiply_Karatsuba(result, left, right);

} // Unbounded_Mul


///-----------------------------------------------------------------------------
///                   S T R I N G   C O N V E R S I O N
///-----------------------------------------------------------------------------

/// @brief Unbounded_From_Decimal
///
Unbounded_Integer *Unbounded_From_Decimal(const char *decimal_string)
{
    Unbounded_Integer *result = Unbounded_New(4);

    // Constant 10 for decimal conversion
    Unbounded_Integer *ten = Unbounded_New(1);
    ten->limbs[0] = 10;
    ten->count    = 1;

    // Handle sign
    bool is_negative = (*decimal_string == '-');
    if (is_negative || *decimal_string == '+') {
        decimal_string++;
    }

    // Process each decimal digit
    while (*decimal_string) {
        if (*decimal_string >= '0' && *decimal_string <= '9') {
            // Create single-digit value
            Unbounded_Integer *digit = Unbounded_New(1);
            digit->limbs[0] = *decimal_string - '0';
            digit->count    = 1;

            // result = result * 10
            Unbounded_Integer *temp = Unbounded_New(result->count * 2);
            Unbounded_Mul(temp, result, ten);
            Unbounded_Free(result);
            result = temp;

            // result = result + digit
            temp = Unbounded_New(result->count + 1);
            Unbounded_Add(temp, result, digit);
            Unbounded_Free(result);
            Unbounded_Free(digit);
            result = temp;
        }
        // Skip underscores (Ada numeric literal separators)

        decimal_string++;
    }

    result->is_negative = is_negative;
    Unbounded_Free(ten);

    return result;

} // Unbounded_From_Decimal
