///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                      L E X I C A L   A N A L Y S I S                      --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  Implementation of the Ada83 lexical analyzer.                            --
///  See ada83_lexer.h for interface documentation.                           --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_lexer.h"
#include "ada83_string.h"
#include "ada83_arena.h"


///-----------------------------------------------------------------------------
///                   T O K E N   N A M E   T A B L E
///-----------------------------------------------------------------------------
///
///  Human-readable names for each token kind, used in error messages
///  and debugging output. Indexed by Token_Kind enumeration.
///
///-----------------------------------------------------------------------------

const char *Token_Names[TK_COUNT] = {
    [TK_EOF]            = "end of file",
    [TK_ERROR]          = "error",
    [TK_IDENTIFIER]     = "identifier",
    [TK_INTEGER]        = "integer literal",
    [TK_REAL]           = "real literal",
    [TK_CHARACTER]      = "character literal",
    [TK_STRING]         = "string literal",
    [TK_LEFT_PAREN]     = "(",
    [TK_RIGHT_PAREN]    = ")",
    [TK_LEFT_BRACKET]   = "[",
    [TK_RIGHT_BRACKET]  = "]",
    [TK_COMMA]          = ",",
    [TK_DOT]            = ".",
    [TK_SEMICOLON]      = ";",
    [TK_COLON]          = ":",
    [TK_TICK]           = "'",
    [TK_ASSIGN]         = ":=",
    [TK_ARROW]          = "=>",
    [TK_DOUBLE_DOT]     = "..",
    [TK_DOUBLE_LESS]    = "<<",
    [TK_DOUBLE_GREATER] = ">>",
    [TK_BOX]            = "<>",
    [TK_BAR]            = "|",
    [TK_EQUAL]          = "=",
    [TK_NOT_EQUAL]      = "/=",
    [TK_LESS_THAN]      = "<",
    [TK_LESS_EQUAL]     = "<=",
    [TK_GREATER_THAN]   = ">",
    [TK_GREATER_EQUAL]  = ">=",
    [TK_PLUS]           = "+",
    [TK_MINUS]          = "-",
    [TK_STAR]           = "*",
    [TK_SLASH]          = "/",
    [TK_AMPERSAND]      = "&",
    [TK_DOUBLE_STAR]    = "**",
    [TK_ABORT]          = "ABORT",
    [TK_ABS]            = "ABS",
    [TK_ACCEPT]         = "ACCEPT",
    [TK_ACCESS]         = "ACCESS",
    [TK_ALIASED]        = "ALIASED",
    [TK_ALL]            = "ALL",
    [TK_AND]            = "AND",
    [TK_AND_THEN]       = "AND THEN",
    [TK_ARRAY]          = "ARRAY",
    [TK_AT]             = "AT",
    [TK_BEGIN]          = "BEGIN",
    [TK_BODY]           = "BODY",
    [TK_CASE]           = "CASE",
    [TK_CONSTANT]       = "CONSTANT",
    [TK_DECLARE]        = "DECLARE",
    [TK_DELAY]          = "DELAY",
    [TK_DELTA]          = "DELTA",
    [TK_DIGITS]         = "DIGITS",
    [TK_DO]             = "DO",
    [TK_ELSE]           = "ELSE",
    [TK_ELSIF]          = "ELSIF",
    [TK_END]            = "END",
    [TK_ENTRY]          = "ENTRY",
    [TK_EXCEPTION]      = "EXCEPTION",
    [TK_EXIT]           = "EXIT",
    [TK_FOR]            = "FOR",
    [TK_FUNCTION]       = "FUNCTION",
    [TK_GENERIC]        = "GENERIC",
    [TK_GOTO]           = "GOTO",
    [TK_IF]             = "IF",
    [TK_IN]             = "IN",
    [TK_IS]             = "IS",
    [TK_LIMITED]        = "LIMITED",
    [TK_LOOP]           = "LOOP",
    [TK_MOD]            = "MOD",
    [TK_NEW]            = "NEW",
    [TK_NOT]            = "NOT",
    [TK_NULL]           = "NULL",
    [TK_OF]             = "OF",
    [TK_OR]             = "OR",
    [TK_OR_ELSE]        = "OR ELSE",
    [TK_OTHERS]         = "OTHERS",
    [TK_OUT]            = "OUT",
    [TK_PACKAGE]        = "PACKAGE",
    [TK_PRAGMA]         = "PRAGMA",
    [TK_PRIVATE]        = "PRIVATE",
    [TK_PROCEDURE]      = "PROCEDURE",
    [TK_RAISE]          = "RAISE",
    [TK_RANGE]          = "RANGE",
    [TK_RECORD]         = "RECORD",
    [TK_REM]            = "REM",
    [TK_RENAMES]        = "RENAMES",
    [TK_RETURN]         = "RETURN",
    [TK_REVERSE]        = "REVERSE",
    [TK_SELECT]         = "SELECT",
    [TK_SEPARATE]       = "SEPARATE",
    [TK_SUBTYPE]        = "SUBTYPE",
    [TK_TASK]           = "TASK",
    [TK_TERMINATE]      = "TERMINATE",
    [TK_THEN]           = "THEN",
    [TK_TYPE]           = "TYPE",
    [TK_USE]            = "USE",
    [TK_WHEN]           = "WHEN",
    [TK_WHILE]          = "WHILE",
    [TK_WITH]           = "WITH",
    [TK_XOR]            = "XOR",
};


///-----------------------------------------------------------------------------
///                   K E Y W O R D   T A B L E
///-----------------------------------------------------------------------------
///
///  Table mapping keyword strings to token kinds.
///  Used by Lookup_Keyword for reserved word recognition.
///
///  Ada83 has 63 reserved words. The table is ordered for easy maintenance
///  (alphabetically), though lookup is linear.
///
///-----------------------------------------------------------------------------

static struct {
    String_Slice keyword;
    Token_Kind   kind;
} Keywords[] = {
    { STR("abort"),     TK_ABORT     },
    { STR("abs"),       TK_ABS       },
    { STR("accept"),    TK_ACCEPT    },
    { STR("access"),    TK_ACCESS    },
    { STR("all"),       TK_ALL       },
    { STR("and"),       TK_AND       },
    { STR("array"),     TK_ARRAY     },
    { STR("at"),        TK_AT        },
    { STR("begin"),     TK_BEGIN     },
    { STR("body"),      TK_BODY      },
    { STR("case"),      TK_CASE      },
    { STR("constant"),  TK_CONSTANT  },
    { STR("declare"),   TK_DECLARE   },
    { STR("delay"),     TK_DELAY     },
    { STR("delta"),     TK_DELTA     },
    { STR("digits"),    TK_DIGITS    },
    { STR("do"),        TK_DO        },
    { STR("else"),      TK_ELSE      },
    { STR("elsif"),     TK_ELSIF     },
    { STR("end"),       TK_END       },
    { STR("entry"),     TK_ENTRY     },
    { STR("exception"), TK_EXCEPTION },
    { STR("exit"),      TK_EXIT      },
    { STR("for"),       TK_FOR       },
    { STR("function"),  TK_FUNCTION  },
    { STR("generic"),   TK_GENERIC   },
    { STR("goto"),      TK_GOTO      },
    { STR("if"),        TK_IF        },
    { STR("in"),        TK_IN        },
    { STR("is"),        TK_IS        },
    { STR("limited"),   TK_LIMITED   },
    { STR("loop"),      TK_LOOP      },
    { STR("mod"),       TK_MOD       },
    { STR("new"),       TK_NEW       },
    { STR("not"),       TK_NOT       },
    { STR("null"),      TK_NULL      },
    { STR("of"),        TK_OF        },
    { STR("or"),        TK_OR        },
    { STR("others"),    TK_OTHERS    },
    { STR("out"),       TK_OUT       },
    { STR("package"),   TK_PACKAGE   },
    { STR("pragma"),    TK_PRAGMA    },
    { STR("private"),   TK_PRIVATE   },
    { STR("procedure"), TK_PROCEDURE },
    { STR("raise"),     TK_RAISE     },
    { STR("range"),     TK_RANGE     },
    { STR("record"),    TK_RECORD    },
    { STR("rem"),       TK_REM       },
    { STR("renames"),   TK_RENAMES   },
    { STR("return"),    TK_RETURN    },
    { STR("reverse"),   TK_REVERSE   },
    { STR("select"),    TK_SELECT    },
    { STR("separate"),  TK_SEPARATE  },
    { STR("subtype"),   TK_SUBTYPE   },
    { STR("task"),      TK_TASK      },
    { STR("terminate"), TK_TERMINATE },
    { STR("then"),      TK_THEN      },
    { STR("type"),      TK_TYPE      },
    { STR("use"),       TK_USE       },
    { STR("when"),      TK_WHEN      },
    { STR("while"),     TK_WHILE     },
    { STR("with"),      TK_WITH      },
    { STR("xor"),       TK_XOR       },
    { NULL_STRING,      TK_EOF       }  // Sentinel
};


///-----------------------------------------------------------------------------
///                   K E Y W O R D   L O O K U P
///-----------------------------------------------------------------------------

/// @brief Lookup_Keyword
///
Token_Kind Lookup_Keyword(String_Slice identifier)
{
    // Linear search through keyword table
    for (int i = 0; Keywords[i].keyword.data != NULL; i++) {
        if (String_Equal_CI(identifier, Keywords[i].keyword)) {
            return Keywords[i].kind;
        }
    }

    // Not a keyword
    return TK_IDENTIFIER;

} // Lookup_Keyword


///-----------------------------------------------------------------------------
///                   L E X E R   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

/// @brief Lexer_Init
///
Lexer_State Lexer_Init(const char *source, size_t length, const char *filename)
{
    return (Lexer_State){
        .source_start   = source,
        .current        = source,
        .source_end     = source + length,
        .line           = 1,
        .column         = 1,
        .filename       = filename,
        .previous_token = TK_EOF
    };

} // Lexer_Init


///-----------------------------------------------------------------------------
///                   C H A R A C T E R   A C C E S S
///-----------------------------------------------------------------------------

/// @brief Peek at a character at given offset from current position
/// @param lexer The lexer state
/// @param offset Offset from current position (0 = current char)
/// @return Character at offset, or '\0' if past end
///
static char Peek_Char(Lexer_State *lexer, size_t offset)
{
    if (lexer->current + offset >= lexer->source_end) {
        return '\0';
    }
    return lexer->current[offset];

} // Peek_Char


/// @brief Advance the lexer by one character
/// @param lexer The lexer state
/// @return The character that was consumed
///
/// Updates line and column tracking for newlines.
///
static char Advance_Char(Lexer_State *lexer)
{
    if (lexer->current >= lexer->source_end) {
        return '\0';
    }

    char c = *lexer->current++;

    // Track newlines for source location reporting
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    }
    else {
        lexer->column++;
    }

    return c;

} // Advance_Char


///-----------------------------------------------------------------------------
///                   W H I T E S P A C E   A N D   C O M M E N T S
///-----------------------------------------------------------------------------
///
///  Ada83 comments begin with double-hyphen and extend to end of line.
///  Per LRM 2.7: "A comment starts with two adjacent hyphens and extends
///  up to the end of the line."
///
///-----------------------------------------------------------------------------

/// @brief Skip whitespace and comments
/// @param lexer The lexer state
///
static void Skip_Whitespace(Lexer_State *lexer)
{
    for (;;) {
        // Skip whitespace characters
        while (lexer->current < lexer->source_end) {
            char c = *lexer->current;
            if (c == ' ' || c == '\t' || c == '\n' ||
                c == '\r' || c == '\v' || c == '\f') {
                Advance_Char(lexer);
            }
            else {
                break;
            }
        }

        // Check for comment (double-hyphen)
        if (lexer->current + 1 < lexer->source_end &&
            lexer->current[0] == '-' && lexer->current[1] == '-') {
            // Skip to end of line
            while (lexer->current < lexer->source_end && *lexer->current != '\n') {
                Advance_Char(lexer);
            }
            // Continue to skip more whitespace/comments
        }
        else {
            // Not whitespace or comment - done
            break;
        }
    }

} // Skip_Whitespace


///-----------------------------------------------------------------------------
///                   T O K E N   C O N S T R U C T I O N
///-----------------------------------------------------------------------------

/// @brief Create a token with given kind and location
/// @param kind     Token classification
/// @param location Source position of token
/// @param literal  Original token text (can be NULL_STRING)
/// @return Initialized token structure
///
static Token Make_Token(Token_Kind kind, Source_Location location, String_Slice literal)
{
    return (Token){
        .kind          = kind,
        .location      = location,
        .literal       = literal,
        .integer_value = 0,
        .real_value    = 0.0,
        .unbounded_int = NULL,
        .unbounded_real = NULL
    };

} // Make_Token


///-----------------------------------------------------------------------------
///                   I D E N T I F I E R   S C A N N I N G
///-----------------------------------------------------------------------------
///
///  Ada83 identifier syntax (LRM 2.3):
///    identifier ::= letter {[underscore] letter_or_digit}
///
///  - Must start with a letter
///  - May contain letters, digits, and single underscores
///  - Adjacent underscores or trailing underscore are illegal
///  - Case-insensitive
///
///-----------------------------------------------------------------------------

/// @brief Scan an identifier or reserved word
/// @param lexer The lexer state
/// @return Token for identifier or keyword
///
static Token Scan_Identifier(Lexer_State *lexer)
{
    Source_Location start_loc = {
        .line   = lexer->line,
        .column = lexer->column,
        .file   = lexer->filename
    };

    const char *start = lexer->current;

    // Consume identifier characters: letters, digits, underscores
    while (isalnum(Peek_Char(lexer, 0)) || Peek_Char(lexer, 0) == '_') {
        Advance_Char(lexer);
    }

    String_Slice text = { .data = start, .length = lexer->current - start };

    // Check if identifier is a reserved word
    Token_Kind kind = Lookup_Keyword(text);

    // Check for improper keyword extension (e.g., "beginx")
    if (kind != TK_IDENTIFIER) {
        char next = Peek_Char(lexer, 0);
        if (isalnum(next) || next == '_') {
            // This is actually an identifier, not a keyword
            return Make_Token(TK_ERROR, start_loc, STR("keyword followed by identifier char"));
        }
    }

    return Make_Token(kind, start_loc, text);

} // Scan_Identifier


///-----------------------------------------------------------------------------
///                   N U M E R I C   L I T E R A L   S C A N N I N G
///-----------------------------------------------------------------------------
///
///  Ada83 numeric literal syntax (LRM 2.4):
///
///  Decimal literals:
///    numeric_literal ::= decimal_literal | based_literal
///    decimal_literal ::= integer [.integer] [exponent]
///    integer ::= digit {[_] digit}
///    exponent ::= E [+|-] integer
///
///  Based literals:
///    based_literal ::= base # based_integer [.based_integer] # [exponent]
///    base ::= integer
///    based_integer ::= extended_digit {[_] extended_digit}
///    extended_digit ::= digit | A | B | C | D | E | F
///
///  Examples:
///    12, 0, 1E6, 123_456                   -- decimal integers
///    12.0, 0.0, 0.456, 3.14159_26          -- decimal reals
///    2#1111_1111#, 16#FF#, 016#0FF#        -- based integers
///    16#F.FF#E+2                           -- based real with exponent
///
///-----------------------------------------------------------------------------

/// @brief Scan a numeric literal (integer or real)
/// @param lexer The lexer state
/// @return Token for numeric literal
///
static Token Scan_Number(Lexer_State *lexer)
{
    Source_Location start_loc = {
        .line   = lexer->line,
        .column = lexer->column,
        .file   = lexer->filename
    };

    const char *start = lexer->current;
    const char *mantissa_start = NULL;
    const char *mantissa_end   = NULL;
    const char *exponent_start = NULL;

    int base = 10;
    bool is_real = false;
    bool is_based = false;
    char base_delim = 0;

    // Phase 1: Scan initial digit sequence
    while (isdigit(Peek_Char(lexer, 0)) || Peek_Char(lexer, 0) == '_') {
        Advance_Char(lexer);
    }

    // Check for based literal (# or : delimiter)
    if (Peek_Char(lexer, 0) == '#' ||
        (Peek_Char(lexer, 0) == ':' && isxdigit(Peek_Char(lexer, 1)))) {

        base_delim = Peek_Char(lexer, 0);
        const char *base_end = lexer->current;
        Advance_Char(lexer);  // Skip delimiter

        // Parse base value
        char *base_str = Arena_Alloc(32);
        int base_idx = 0;
        for (const char *p = start; p < base_end; p++) {
            if (*p != '_') {
                base_str[base_idx++] = *p;
            }
        }
        base_str[base_idx] = '\0';
        base = atoi(base_str);

        is_based = true;
        mantissa_start = lexer->current;

        // Scan based mantissa (extended digits)
        while (isxdigit(Peek_Char(lexer, 0)) || Peek_Char(lexer, 0) == '_') {
            Advance_Char(lexer);
        }

        // Check for fractional part
        if (Peek_Char(lexer, 0) == '.') {
            is_real = true;
            Advance_Char(lexer);
            while (isxdigit(Peek_Char(lexer, 0)) || Peek_Char(lexer, 0) == '_') {
                Advance_Char(lexer);
            }
        }

        // Expect closing delimiter
        if (Peek_Char(lexer, 0) == base_delim) {
            mantissa_end = lexer->current;
            Advance_Char(lexer);
        }

        // Check for exponent
        if (tolower(Peek_Char(lexer, 0)) == 'e') {
            is_based = true;  // Exponent present
            Advance_Char(lexer);
            if (Peek_Char(lexer, 0) == '+' || Peek_Char(lexer, 0) == '-') {
                Advance_Char(lexer);
            }
            exponent_start = lexer->current;
            while (isdigit(Peek_Char(lexer, 0)) || Peek_Char(lexer, 0) == '_') {
                Advance_Char(lexer);
            }
        }
    }
    else {
        // Decimal literal

        // Check for fractional part
        if (Peek_Char(lexer, 0) == '.') {
            // Distinguish ".." from decimal point
            if (Peek_Char(lexer, 1) != '.' && !isalpha(Peek_Char(lexer, 1))) {
                is_real = true;
                Advance_Char(lexer);  // Skip '.'
                while (isdigit(Peek_Char(lexer, 0)) || Peek_Char(lexer, 0) == '_') {
                    Advance_Char(lexer);
                }
            }
        }

        // Check for exponent
        if (tolower(Peek_Char(lexer, 0)) == 'e') {
            Advance_Char(lexer);
            if (Peek_Char(lexer, 0) == '+' || Peek_Char(lexer, 0) == '-') {
                Advance_Char(lexer);
            }
            while (isdigit(Peek_Char(lexer, 0)) || Peek_Char(lexer, 0) == '_') {
                Advance_Char(lexer);
            }
        }
    }

    // Check for illegal trailing letter/digit
    if (isalpha(Peek_Char(lexer, 0))) {
        return Make_Token(TK_ERROR, start_loc, STR("invalid character after number"));
    }

    // Build token
    String_Slice text = { .data = start, .length = lexer->current - start };
    Token token = Make_Token(is_real ? TK_REAL : TK_INTEGER, start_loc, text);

    // Parse the numeric value
    // Remove underscores and convert
    char *clean = Arena_Alloc(512);
    int clean_idx = 0;

    const char *parse_start = is_based && mantissa_start ? mantissa_start : start;
    const char *parse_end   = is_based && mantissa_end   ? mantissa_end   : lexer->current;

    for (const char *p = parse_start; p < parse_end; p++) {
        if (*p != '_' && *p != '#' && *p != ':') {
            clean[clean_idx++] = *p;
        }
    }
    clean[clean_idx] = '\0';

    // Convert based on type
    if (is_based && !is_real) {
        // Based integer conversion
        int64_t value = 0;
        for (int i = 0; i < clean_idx; i++) {
            int digit_val;
            if (clean[i] >= 'A' && clean[i] <= 'F') {
                digit_val = clean[i] - 'A' + 10;
            }
            else if (clean[i] >= 'a' && clean[i] <= 'f') {
                digit_val = clean[i] - 'a' + 10;
            }
            else {
                digit_val = clean[i] - '0';
            }
            value = value * base + digit_val;
        }
        token.integer_value = value;
    }
    else if (is_based && is_real) {
        // Based real - compute mantissa then apply exponent
        double mantissa = 0.0;
        int decimal_pos = -1;

        for (int i = 0; i < clean_idx; i++) {
            if (clean[i] == '.') {
                decimal_pos = i;
                break;
            }
        }

        int frac_digits = 0;
        for (int i = 0; i < clean_idx; i++) {
            if (clean[i] == '.') continue;

            int digit_val;
            if (clean[i] >= 'A' && clean[i] <= 'F') {
                digit_val = clean[i] - 'A' + 10;
            }
            else if (clean[i] >= 'a' && clean[i] <= 'f') {
                digit_val = clean[i] - 'a' + 10;
            }
            else {
                digit_val = clean[i] - '0';
            }

            if (decimal_pos < 0 || i < decimal_pos) {
                mantissa = mantissa * base + digit_val;
            }
            else {
                frac_digits++;
                mantissa += digit_val / pow(base, frac_digits);
            }
        }

        // Apply exponent if present
        if (exponent_start) {
            char exp_str[32];
            int exp_idx = 0;
            for (const char *p = exponent_start; p < lexer->current; p++) {
                if (*p != '_') {
                    exp_str[exp_idx++] = *p;
                }
            }
            exp_str[exp_idx] = '\0';
            int exponent = atoi(exp_str);
            token.real_value = mantissa * pow(base, exponent);
        }
        else {
            token.real_value = mantissa;
        }
    }
    else {
        // Decimal literal
        errno = 0;
        token.real_value = strtod(clean, NULL);

        if (!is_real) {
            // Also compute integer value
            token.unbounded_int = Unbounded_From_Decimal(clean);
            token.integer_value = (token.unbounded_int->count == 1)
                                ? token.unbounded_int->limbs[0]
                                : 0;
        }
    }

    return token;

} // Scan_Number


///-----------------------------------------------------------------------------
///                   C H A R A C T E R   L I T E R A L   S C A N N I N G
///-----------------------------------------------------------------------------
///
///  Ada83 character literal syntax (LRM 2.5):
///    character_literal ::= 'graphic_character'
///
///  A character literal is a single graphic character enclosed in single
///  quotes. The tick (apostrophe) is also used for attributes, so context
///  is needed to disambiguate.
///
///-----------------------------------------------------------------------------

/// @brief Scan a character literal
/// @param lexer The lexer state
/// @return Token for character literal
///
static Token Scan_Character(Lexer_State *lexer)
{
    Source_Location start_loc = {
        .line   = lexer->line,
        .column = lexer->column,
        .file   = lexer->filename
    };

    Advance_Char(lexer);  // Skip opening quote

    if (Peek_Char(lexer, 0) == '\0') {
        return Make_Token(TK_ERROR, start_loc, STR("unterminated character"));
    }

    char c = Peek_Char(lexer, 0);
    Advance_Char(lexer);  // Consume the character

    if (Peek_Char(lexer, 0) != '\'') {
        return Make_Token(TK_ERROR, start_loc, STR("unterminated character"));
    }
    Advance_Char(lexer);  // Skip closing quote

    Token token = Make_Token(TK_CHARACTER, start_loc, (String_Slice){&c, 1});
    token.integer_value = (unsigned char)c;

    return token;

} // Scan_Character


///-----------------------------------------------------------------------------
///                   S T R I N G   L I T E R A L   S C A N N I N G
///-----------------------------------------------------------------------------
///
///  Ada83 string literal syntax (LRM 2.6):
///    string_literal ::= "{graphic_character}"
///
///  String literals are enclosed in double quotes (or percent signs as
///  an alternative delimiter). A doubled delimiter within the string
///  represents a single occurrence.
///
///  Example: "He said ""Hello""" represents: He said "Hello"
///
///-----------------------------------------------------------------------------

/// @brief Scan a string literal
/// @param lexer The lexer state
/// @return Token for string literal
///
static Token Scan_String(Lexer_State *lexer)
{
    Source_Location start_loc = {
        .line   = lexer->line,
        .column = lexer->column,
        .file   = lexer->filename
    };

    char delimiter = Peek_Char(lexer, 0);  // " or %
    Advance_Char(lexer);  // Skip opening delimiter

    // Allocate buffer for string content
    char *buffer = Arena_Alloc(256);
    char *p = buffer;
    int length = 0;

    while (Peek_Char(lexer, 0) != '\0') {
        char c = Peek_Char(lexer, 0);

        if (c == delimiter) {
            // Check for doubled delimiter (escaped quote)
            if (Peek_Char(lexer, 1) == delimiter) {
                Advance_Char(lexer);
                Advance_Char(lexer);
                if (length < 255) {
                    *p++ = delimiter;
                }
                length++;
            }
            else {
                // End of string
                break;
            }
        }
        else {
            if (length < 255) {
                *p++ = c;
            }
            length++;
            Advance_Char(lexer);
        }
    }

    // Expect closing delimiter
    if (Peek_Char(lexer, 0) == delimiter) {
        Advance_Char(lexer);
    }
    else {
        return Make_Token(TK_ERROR, start_loc, STR("unterminated string"));
    }

    *p = '\0';
    String_Slice text = { .data = buffer, .length = length };

    return Make_Token(TK_STRING, start_loc, text);

} // Scan_String


///-----------------------------------------------------------------------------
///                   M A I N   T O K E N   S C A N N I N G
///-----------------------------------------------------------------------------

/// @brief Lexer_Next
///
Token Lexer_Next(Lexer_State *lexer)
{
    const char *before_whitespace = lexer->current;

    // Skip whitespace and comments
    Skip_Whitespace(lexer);

    // Track whether whitespace was present (for tick disambiguation)
    bool had_whitespace = (lexer->current != before_whitespace);

    Source_Location loc = {
        .line   = lexer->line,
        .column = lexer->column,
        .file   = lexer->filename
    };

    char c = Peek_Char(lexer, 0);

    // End of input
    if (c == '\0') {
        lexer->previous_token = TK_EOF;
        return Make_Token(TK_EOF, loc, NULL_STRING);
    }

    // Identifier or keyword
    if (isalpha(c)) {
        Token token = Scan_Identifier(lexer);
        lexer->previous_token = token.kind;
        return token;
    }

    // Numeric literal
    if (isdigit(c)) {
        Token token = Scan_Number(lexer);
        lexer->previous_token = token.kind;
        return token;
    }

    // Character literal vs attribute tick
    // A tick is a character literal if:
    //   - Next char exists and char after that is also a tick
    //   - Not immediately following an identifier without whitespace
    if (c == '\'') {
        char c2 = Peek_Char(lexer, 1);
        char pc = lexer->current > lexer->source_start ? lexer->current[-1] : 0;

        bool is_attribute = (lexer->previous_token == TK_IDENTIFIER && !had_whitespace && isalnum(pc));

        if (c2 != '\0' && Peek_Char(lexer, 2) == '\'' &&
            (lexer->current + 3 >= lexer->source_end || lexer->current[3] != '\'') &&
            !is_attribute) {
            lexer->previous_token = TK_CHARACTER;
            return Scan_Character(lexer);
        }

        Advance_Char(lexer);
        lexer->previous_token = TK_TICK;
        return Make_Token(TK_TICK, loc, STR("'"));
    }

    // String literal (double quote or percent)
    if (c == '"' || c == '%') {
        Token token = Scan_String(lexer);
        lexer->previous_token = token.kind;
        return token;
    }

    // Single-character and compound delimiters/operators
    Advance_Char(lexer);

    Token_Kind kind;
    switch (c) {
        case '(':
            kind = TK_LEFT_PAREN;
            break;

        case ')':
            kind = TK_RIGHT_PAREN;
            break;

        case '[':
            kind = TK_LEFT_BRACKET;
            break;

        case ']':
            kind = TK_RIGHT_BRACKET;
            break;

        case ',':
            kind = TK_COMMA;
            break;

        case ';':
            kind = TK_SEMICOLON;
            break;

        case '&':
            kind = TK_AMPERSAND;
            break;

        case '|':
        case '!':  // Alternative bar symbol
            kind = TK_BAR;
            break;

        case '+':
            kind = TK_PLUS;
            break;

        case '-':
            kind = TK_MINUS;
            break;

        case '/':
            // /=  (not equal)
            if (Peek_Char(lexer, 0) == '=') {
                Advance_Char(lexer);
                kind = TK_NOT_EQUAL;
            }
            else {
                kind = TK_SLASH;
            }
            break;

        case '*':
            // **  (exponentiation)
            if (Peek_Char(lexer, 0) == '*') {
                Advance_Char(lexer);
                kind = TK_DOUBLE_STAR;
            }
            else {
                kind = TK_STAR;
            }
            break;

        case '=':
            // =>  (arrow)
            if (Peek_Char(lexer, 0) == '>') {
                Advance_Char(lexer);
                kind = TK_ARROW;
            }
            else {
                kind = TK_EQUAL;
            }
            break;

        case ':':
            // :=  (assignment)
            if (Peek_Char(lexer, 0) == '=') {
                Advance_Char(lexer);
                kind = TK_ASSIGN;
            }
            else {
                kind = TK_COLON;
            }
            break;

        case '.':
            // ..  (range)
            if (Peek_Char(lexer, 0) == '.') {
                Advance_Char(lexer);
                kind = TK_DOUBLE_DOT;
            }
            else {
                kind = TK_DOT;
            }
            break;

        case '<':
            // <=  (less than or equal)
            // <<  (label bracket)
            // <>  (box)
            if (Peek_Char(lexer, 0) == '=') {
                Advance_Char(lexer);
                kind = TK_LESS_EQUAL;
            }
            else if (Peek_Char(lexer, 0) == '<') {
                Advance_Char(lexer);
                kind = TK_DOUBLE_LESS;
            }
            else if (Peek_Char(lexer, 0) == '>') {
                Advance_Char(lexer);
                kind = TK_BOX;
            }
            else {
                kind = TK_LESS_THAN;
            }
            break;

        case '>':
            // >=  (greater than or equal)
            // >>  (label bracket)
            if (Peek_Char(lexer, 0) == '=') {
                Advance_Char(lexer);
                kind = TK_GREATER_EQUAL;
            }
            else if (Peek_Char(lexer, 0) == '>') {
                Advance_Char(lexer);
                kind = TK_DOUBLE_GREATER;
            }
            else {
                kind = TK_GREATER_THAN;
            }
            break;

        default:
            kind = TK_ERROR;
            break;
    }

    lexer->previous_token = kind;
    return Make_Token(kind, loc, (kind == TK_ERROR) ? STR("unexpected character") : NULL_STRING);

} // Lexer_Next
