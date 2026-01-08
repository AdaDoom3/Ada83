///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                      L E X I C A L   A N A L Y S I S                      --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module implements the lexical analyzer (scanner/tokenizer) for the  --
///  Ada83 interpreter. It transforms source text into a stream of tokens     --
///  according to Ada83 LRM Chapter 2 (Lexical Elements).                     --
///                                                                           --
///  Key Ada83 lexical features supported:                                    --
///    - Identifiers (case-insensitive, LRM 2.3)                              --
///    - Numeric literals (decimal and based, LRM 2.4)                        --
///    - Character literals (LRM 2.5)                                         --
///    - String literals (LRM 2.6)                                            --
///    - Comments (double-hyphen to end of line, LRM 2.7)                     --
///    - Reserved words (63 keywords, LRM 2.9)                                --
///    - Delimiters and operators (LRM 2.2)                                   --
///                                                                           --
///  Reference: GNAT's Scn/Scan package provides similar functionality        --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_LEXER_H
#define ADA83_LEXER_H

#include "ada83_common.h"
#include "ada83_bignum.h"


///-----------------------------------------------------------------------------
///                   T O K E N   K I N D   E N U M E R A T I O N
///-----------------------------------------------------------------------------
///
///  Ada83 token types organized by category:
///    - Special tokens (EOF, error)
///    - Literals (identifier, integer, real, character, string)
///    - Delimiters (parentheses, brackets, punctuation)
///    - Operators (arithmetic, relational, logical)
///    - Reserved words (Ada83 keywords)
///
///  Token codes roughly follow the order in Ada83 LRM Chapter 2.
///
///-----------------------------------------------------------------------------

typedef enum {
    //-------------------------------------------------------------------------
    // Special tokens
    //-------------------------------------------------------------------------
    TK_EOF = 0,             /// End of file/input
    TK_ERROR,               /// Lexical error (malformed token)

    //-------------------------------------------------------------------------
    // Literals (LRM 2.4-2.6)
    //-------------------------------------------------------------------------
    TK_IDENTIFIER,          /// Identifier (e.g., "My_Variable")
    TK_INTEGER,             /// Integer literal (e.g., "42", "16#FF#")
    TK_REAL,                /// Real literal (e.g., "3.14", "2.0E-5")
    TK_CHARACTER,           /// Character literal (e.g., "'A'")
    TK_STRING,              /// String literal (e.g., "Hello")

    //-------------------------------------------------------------------------
    // Delimiters (LRM 2.2)
    //-------------------------------------------------------------------------
    TK_LEFT_PAREN,          /// (
    TK_RIGHT_PAREN,         /// )
    TK_LEFT_BRACKET,        /// [  (used in array indexing, Ada83 alternative)
    TK_RIGHT_BRACKET,       /// ]
    TK_COMMA,               /// ,
    TK_DOT,                 /// .
    TK_SEMICOLON,           /// ;
    TK_COLON,               /// :
    TK_TICK,                /// '  (attribute prefix, qualified expression)
    TK_ASSIGN,              /// :=
    TK_ARROW,               /// =>
    TK_DOUBLE_DOT,          /// ..  (range separator)
    TK_DOUBLE_LESS,         /// <<  (label bracket)
    TK_DOUBLE_GREATER,      /// >>  (label bracket)
    TK_BOX,                 /// <>  (unconstrained array, default param)
    TK_BAR,                 /// |   (alternative separator)

    //-------------------------------------------------------------------------
    // Operators (LRM 4.5)
    //-------------------------------------------------------------------------
    TK_EQUAL,               /// =
    TK_NOT_EQUAL,           /// /=
    TK_LESS_THAN,           /// <
    TK_LESS_EQUAL,          /// <=
    TK_GREATER_THAN,        /// >
    TK_GREATER_EQUAL,       /// >=
    TK_PLUS,                /// +   (addition, unary plus)
    TK_MINUS,               /// -   (subtraction, unary minus)
    TK_STAR,                /// *   (multiplication)
    TK_SLASH,               /// /   (division)
    TK_AMPERSAND,           /// &   (concatenation)
    TK_DOUBLE_STAR,         /// **  (exponentiation)

    //-------------------------------------------------------------------------
    // Reserved Words (LRM 2.9) - Ada83 Keywords
    //-------------------------------------------------------------------------
    TK_ABORT,               /// abort
    TK_ABS,                 /// abs       (absolute value operator)
    TK_ACCEPT,              /// accept    (task entry accept)
    TK_ACCESS,              /// access    (pointer type)
    TK_ALIASED,             /// aliased   (Ada95 - included for compatibility)
    TK_ALL,                 /// all       (access all components)
    TK_AND,                 /// and       (logical and)
    TK_AND_THEN,            /// and then  (short-circuit and)
    TK_ARRAY,               /// array
    TK_AT,                  /// at        (representation clause)
    TK_BEGIN,               /// begin
    TK_BODY,                /// body      (package/task body)
    TK_CASE,                /// case
    TK_CONSTANT,            /// constant
    TK_DECLARE,             /// declare   (block statement)
    TK_DELAY,               /// delay     (task delay)
    TK_DELTA,               /// delta     (fixed point)
    TK_DIGITS,              /// digits    (floating point)
    TK_DO,                  /// do        (accept statement)
    TK_ELSE,                /// else
    TK_ELSIF,               /// elsif
    TK_END,                 /// end
    TK_ENTRY,               /// entry     (task entry)
    TK_EXCEPTION,           /// exception
    TK_EXIT,                /// exit
    TK_FOR,                 /// for
    TK_FUNCTION,            /// function
    TK_GENERIC,             /// generic
    TK_GOTO,                /// goto
    TK_IF,                  /// if
    TK_IN,                  /// in        (parameter mode, membership test)
    TK_IS,                  /// is
    TK_LIMITED,             /// limited   (limited private type)
    TK_LOOP,                /// loop
    TK_MOD,                 /// mod       (modulus operator)
    TK_NEW,                 /// new       (allocator, derived type)
    TK_NOT,                 /// not       (logical not)
    TK_NULL,                /// null
    TK_OF,                  /// of
    TK_OR,                  /// or        (logical or)
    TK_OR_ELSE,             /// or else   (short-circuit or)
    TK_OTHERS,              /// others    (exception/case others)
    TK_OUT,                 /// out       (parameter mode)
    TK_PACKAGE,             /// package
    TK_PRAGMA,              /// pragma
    TK_PRIVATE,             /// private
    TK_PROCEDURE,           /// procedure
    TK_RAISE,               /// raise
    TK_RANGE,               /// range
    TK_RECORD,              /// record
    TK_REM,                 /// rem       (remainder operator)
    TK_RENAMES,             /// renames
    TK_RETURN,              /// return
    TK_REVERSE,             /// reverse   (reverse iteration)
    TK_SELECT,              /// select    (selective wait)
    TK_SEPARATE,            /// separate  (subunit)
    TK_SUBTYPE,             /// subtype
    TK_TASK,                /// task
    TK_TERMINATE,           /// terminate (selective accept)
    TK_THEN,                /// then
    TK_TYPE,                /// type
    TK_USE,                 /// use
    TK_WHEN,                /// when
    TK_WHILE,               /// while
    TK_WITH,                /// with      (context clause)
    TK_XOR,                 /// xor       (logical xor)

    TK_COUNT                /// Sentinel value for array sizing
} Token_Kind;


///-----------------------------------------------------------------------------
///                   T O K E N   S T R U C T U R E
///-----------------------------------------------------------------------------
///
///  A token represents a single lexical element from the source text.
///  It carries both syntactic information (kind) and semantic information
///  (literal value, numeric value, etc.).
///
///-----------------------------------------------------------------------------

typedef struct {
    Token_Kind          kind;           /// Token classification
    Source_Location     location;       /// Position in source file
    String_Slice        literal;        /// Original text of token

    //-------------------------------------------------------------------------
    // Semantic values (for literals)
    //-------------------------------------------------------------------------
    int64_t             integer_value;  /// Value for integer literals
    double              real_value;     /// Value for real literals

    //-------------------------------------------------------------------------
    // Extended precision values
    //-------------------------------------------------------------------------
    Unbounded_Integer  *unbounded_int;  /// For very large integers
    Rational_Number    *unbounded_real; /// For exact real computation

} Token;


///-----------------------------------------------------------------------------
///                   L E X E R   S T A T E
///-----------------------------------------------------------------------------
///
///  The lexer maintains state about its position in the source text.
///  This structure allows for efficient scanning with single-character
///  lookahead.
///
///-----------------------------------------------------------------------------

typedef struct {
    const char     *source_start;    /// Beginning of source text
    const char     *current;         /// Current scanning position
    const char     *source_end;      /// End of source text

    uint32_t        line;            /// Current line number (1-based)
    uint32_t        column;          /// Current column number (1-based)
    const char     *filename;        /// Source file name for error messages

    Token_Kind      previous_token;  /// Previous token kind (for tick parsing)
} Lexer_State;


///-----------------------------------------------------------------------------
///                   T O K E N   N A M E S
///-----------------------------------------------------------------------------
///
///  Array of human-readable token names for error messages and debugging.
///  Indexed by Token_Kind enumeration values.
///
///-----------------------------------------------------------------------------

extern const char *Token_Names[TK_COUNT];


///-----------------------------------------------------------------------------
///                   L E X E R   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

/// @brief Initialize a lexer state for scanning source text
/// @param source   Pointer to source text (not necessarily null-terminated)
/// @param length   Length of source text in bytes
/// @param filename Source file name for error messages
/// @return Initialized lexer state, positioned at start of source
///
Lexer_State Lexer_Init(const char *source, size_t length, const char *filename);


///-----------------------------------------------------------------------------
///                   T O K E N   S C A N N I N G
///-----------------------------------------------------------------------------

/// @brief Scan and return the next token from the source
/// @param lexer Lexer state (modified to advance position)
/// @return The next token from the input
///
/// Skips whitespace and comments before returning the next significant token.
/// Returns TK_EOF when the end of input is reached.
///
/// This is the primary lexer interface function.
///
Token Lexer_Next(Lexer_State *lexer);


///-----------------------------------------------------------------------------
///                   K E Y W O R D   L O O K U P
///-----------------------------------------------------------------------------

/// @brief Check if an identifier is a reserved word
/// @param identifier The identifier text to check
/// @return The keyword token kind, or TK_IDENTIFIER if not a keyword
///
/// Uses a simple linear search through the keyword table.
/// Ada83 has 63 reserved words, so this is efficient enough.
///
/// Note: Comparison is case-insensitive per Ada83 LRM 2.3.
///
Token_Kind Lookup_Keyword(String_Slice identifier);


#endif // ADA83_LEXER_H
