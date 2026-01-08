///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                  R E C U R S I V E   D E S C E N T   P A R S E R          --
///                                                                           --
///                                  S p e c                                  --
///                                                                           --
///  This module implements a recursive descent parser for Ada83 syntax.      --
///  The parser follows the grammar specified in Ada83 LRM Annex E.           --
///                                                                           --
///  The parser produces an Abstract Syntax Tree (AST) from the token stream. --
///  Each parsing function corresponds to a grammar production from the LRM.  --
///                                                                           --
///  Key grammar productions (simplified):                                    --
///    compilation_unit ::= context_clause library_unit                       --
///    library_unit ::= package_decl | subprogram_decl | generic_decl         --
///    declarative_part ::= {declarative_item}                                --
///    statement_sequence ::= statement {statement}                           --
///                                                                           --
///  Reference: GNAT's Par package provides similar functionality             --
///                                                                           --
///-----------------------------------------------------------------------------

#ifndef ADA83_PARSER_H
#define ADA83_PARSER_H

#include "ada83_common.h"
#include "ada83_lexer.h"
#include "ada83_ast.h"


///-----------------------------------------------------------------------------
///                   P A R S E R   S T A T E
///-----------------------------------------------------------------------------
///
///  Parser state maintains the lexer, current/lookahead tokens, and
///  error tracking information.
///
///-----------------------------------------------------------------------------

typedef struct {
    Lexer_State     lexer;          /// Lexer providing token stream
    Token           current;        /// Current token being processed
    Token           peek;           /// One-token lookahead
    int             error_count;    /// Number of parse errors
    String_Slice   *labels;         /// Declared labels in current scope
    uint32_t        label_count;    /// Number of labels
    uint32_t        label_capacity; /// Allocated capacity
} Parser_State;


///-----------------------------------------------------------------------------
///                   P A R S E R   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

/// @brief Initialize parser from source text
/// @param source   Source text to parse
/// @param length   Length of source text
/// @param filename Source file name for error messages
/// @return Initialized parser state
///
Parser_State Parser_Init(const char *source, size_t length, const char *filename);


///-----------------------------------------------------------------------------
///                   T O K E N   A C C E S S
///-----------------------------------------------------------------------------

/// @brief Advance to the next token
/// @param parser Parser state to advance
///
/// Updates current token to peek, fetches new peek token.
/// Handles compound tokens (AND THEN, OR ELSE).
///
void Parser_Advance(Parser_State *parser);


/// @brief Check if current token matches expected kind
/// @param parser Parser state
/// @param kind   Expected token kind
/// @return true if current token matches
///
bool Parser_Check(Parser_State *parser, Token_Kind kind);


/// @brief Consume current token if it matches expected kind
/// @param parser Parser state
/// @param kind   Expected token kind
/// @return true if token was consumed
///
bool Parser_Match(Parser_State *parser, Token_Kind kind);


/// @brief Require current token to match expected kind
/// @param parser Parser state
/// @param kind   Expected token kind
///
/// Calls Fatal_Error if token doesn't match.
///
void Parser_Expect(Parser_State *parser, Token_Kind kind);


/// @brief Get current source location
/// @param parser Parser state
/// @return Source location of current token
///
Source_Location Parser_Location(Parser_State *parser);


/// @brief Parse an identifier and return its name
/// @param parser Parser state
/// @return Identifier string
///
String_Slice Parser_Identifier(Parser_State *parser);


///-----------------------------------------------------------------------------
///                   E X P R E S S I O N   P A R S I N G
///-----------------------------------------------------------------------------
///
///  Expression parsing follows Ada's operator precedence (LRM 4.5):
///    Level 1 (lowest):  and, or, xor, and then, or else
///    Level 2:           =, /=, <, <=, >, >=, in, not in
///    Level 3:           +, -, & (binary)
///    Level 4:           +, - (unary)
///    Level 5:           *, /, mod, rem
///    Level 6 (highest): **, abs, not
///
///-----------------------------------------------------------------------------

/// @brief Parse an expression (entry point)
/// @param parser Parser state
/// @return AST node for expression
///
AST_Node *Parse_Expression(Parser_State *parser);


/// @brief Parse a name (identifier with possible selectors)
/// @param parser Parser state
/// @return AST node for name
///
AST_Node *Parse_Name(Parser_State *parser);


/// @brief Parse a range constraint (lo .. hi)
/// @param parser Parser state
/// @return AST node for range
///
AST_Node *Parse_Range(Parser_State *parser);


/// @brief Parse a subtype indication (type name with constraint)
/// @param parser Parser state
/// @return AST node for subtype indication
///
AST_Node *Parse_Subtype_Indication(Parser_State *parser);


///-----------------------------------------------------------------------------
///                   D E C L A R A T I O N   P A R S I N G
///-----------------------------------------------------------------------------

/// @brief Parse a single declaration
/// @param parser Parser state
/// @return AST node for declaration
///
AST_Node *Parse_Declaration(Parser_State *parser);


/// @brief Parse a declarative part (sequence of declarations)
/// @param parser Parser state
/// @return Vector of declaration nodes
///
Node_Vector Parse_Declarative_Part(Parser_State *parser);


/// @brief Parse a type definition
/// @param parser Parser state
/// @return AST node for type definition
///
AST_Node *Parse_Type_Definition(Parser_State *parser);


/// @brief Parse parameter list
/// @param parser Parser state
/// @return Vector of parameter nodes
///
Node_Vector Parse_Parameter_List(Parser_State *parser);


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   P A R S I N G
///-----------------------------------------------------------------------------

/// @brief Parse a single statement
/// @param parser Parser state
/// @return AST node for statement
///
AST_Node *Parse_Statement(Parser_State *parser);


/// @brief Parse a sequence of statements
/// @param parser Parser state
/// @return Vector of statement nodes
///
Node_Vector Parse_Statement_Sequence(Parser_State *parser);


/// @brief Parse exception handlers
/// @param parser Parser state
/// @return Vector of handler nodes
///
Node_Vector Parse_Exception_Handlers(Parser_State *parser);


///-----------------------------------------------------------------------------
///                   U N I T   P A R S I N G
///-----------------------------------------------------------------------------

/// @brief Parse a compilation unit
/// @param parser Parser state
/// @return AST node for compilation unit
///
AST_Node *Parse_Compilation_Unit(Parser_State *parser);


/// @brief Parse context clause (with/use)
/// @param parser Parser state
/// @return AST node for context clause
///
AST_Node *Parse_Context_Clause(Parser_State *parser);


/// @brief Parse a generic declaration
/// @param parser Parser state
/// @return AST node for generic
///
AST_Node *Parse_Generic(Parser_State *parser);


#endif // ADA83_PARSER_H
