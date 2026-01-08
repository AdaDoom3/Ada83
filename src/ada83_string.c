///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                     S T R I N G   H A N D L I N G                         --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  Implementation of string handling utilities.                             --
///  See ada83_string.h for interface documentation.                          --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_string.h"


///-----------------------------------------------------------------------------
///                   G L O B A L   E R R O R   C O U N T E R
///-----------------------------------------------------------------------------

int Global_Error_Count = 0;


///-----------------------------------------------------------------------------
///                   S T R I N G   D U P L I C A T I O N
///-----------------------------------------------------------------------------

/// @brief String_Dup
///
String_Slice String_Dup(String_Slice str)
{
    // Allocate space for the string data in the arena
    // Add 1 byte for optional null-terminator (C compatibility)
    char *buffer = Arena_Alloc(str.length + 1);

    // Copy the string content
    memcpy(buffer, str.data, str.length);

    // Null-terminate for C compatibility (not counted in length)
    buffer[str.length] = '\0';

    return (String_Slice){ .data = buffer, .length = str.length };

} // String_Dup


///-----------------------------------------------------------------------------
///                   C A S E - I N S E N S I T I V E   C O M P A R I S O N
///-----------------------------------------------------------------------------

/// @brief String_Equal_CI
///
bool String_Equal_CI(String_Slice a, String_Slice b)
{
    // Different lengths => not equal
    if (a.length != b.length) {
        return false;
    }

    // Compare character by character, case-insensitively
    for (uint32_t i = 0; i < a.length; i++) {
        // ASCII case-folding: convert to lowercase before comparison
        char ca = tolower((unsigned char)a.data[i]);
        char cb = tolower((unsigned char)b.data[i]);

        if (ca != cb) {
            return false;
        }
    }

    return true;

} // String_Equal_CI


///-----------------------------------------------------------------------------
///                   C A S E   C O N V E R S I O N
///-----------------------------------------------------------------------------
///
///  Rotating buffer implementation allows multiple conversions to coexist:
///
///    buffer[0] <-- "first"
///    buffer[1] <-- "second"  (first still valid)
///    ...
///    buffer[7] <-- "eighth"  (all previous still valid)
///    buffer[0] <-- "ninth"   (overwrites "first")
///
///  This pattern is common in GNAT's internal utilities.
///
///-----------------------------------------------------------------------------

/// Number of rotating buffers
#define NUM_BUFFERS 8

/// Maximum string length (truncated if exceeded)
#define MAX_STRING_LENGTH 255

/// @brief String_To_Lower
///
char *String_To_Lower(String_Slice str)
{
    // Static rotating buffer array
    static char   buffers[NUM_BUFFERS][MAX_STRING_LENGTH + 1];
    static int    buffer_index = 0;

    // Select next buffer in rotation
    char *buffer = buffers[buffer_index++ & (NUM_BUFFERS - 1)];

    // Truncate if necessary
    uint32_t length = str.length < MAX_STRING_LENGTH ? str.length : MAX_STRING_LENGTH;

    // Convert to lowercase
    for (uint32_t i = 0; i < length; i++) {
        buffer[i] = tolower((unsigned char)str.data[i]);
    }

    // Null-terminate
    buffer[length] = '\0';

    return buffer;

} // String_To_Lower


///-----------------------------------------------------------------------------
///                   H A S H I N G
///-----------------------------------------------------------------------------
///
///  FNV-1a algorithm:
///    hash = FNV_OFFSET_BASIS
///    for each byte:
///        hash = hash XOR byte
///        hash = hash * FNV_PRIME
///
///  The XOR-then-multiply order (as opposed to FNV-1's multiply-then-XOR)
///  provides better avalanche behavior.
///
///-----------------------------------------------------------------------------

/// @brief String_Hash
///
uint64_t String_Hash(String_Slice str)
{
    uint64_t hash = FNV_OFFSET_BASIS;

    for (uint32_t i = 0; i < str.length; i++) {
        // Convert to lowercase for case-insensitive hashing
        uint8_t byte = (uint8_t)tolower((unsigned char)str.data[i]);

        // FNV-1a: XOR then multiply
        hash ^= byte;
        hash *= FNV_PRIME;
    }

    return hash;

} // String_Hash


///-----------------------------------------------------------------------------
///                   E R R O R   R E P O R T I N G
///-----------------------------------------------------------------------------
///
///  Error message format matches standard compiler conventions:
///    filename:line:column: error message
///
///  This format is understood by IDEs and editors (e.g., Emacs, VSCode)
///  for navigation to error locations.
///
///-----------------------------------------------------------------------------

/// @brief Fatal_Error
///
_Noreturn void Fatal_Error(Source_Location loc, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    // Print location prefix
    fprintf(stderr, "%s:%u:%u: ", loc.file, loc.line, loc.column);

    // Print formatted error message
    vfprintf(stderr, format, args);

    // Terminate with newline
    fputc('\n', stderr);

    va_end(args);

    // Increment global error counter
    Global_Error_Count++;

    // Terminate interpreter
    exit(1);

} // Fatal_Error
