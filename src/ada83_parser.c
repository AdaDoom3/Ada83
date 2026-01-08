///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                  R E C U R S I V E   D E S C E N T   P A R S E R          --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  This module implements a recursive descent parser for Ada83 syntax.      --
///  The parser follows the grammar specified in Ada83 LRM Annex E.           --
///                                                                           --
///  Implementation follows GNAT's Par package structure where applicable.    --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_parser.h"
#include "ada83_arena.h"
#include "ada83_string.h"
#include <string.h>
#include <ctype.h>


///-----------------------------------------------------------------------------
///                   F O R W A R D   D E C L A R A T I O N S
///-----------------------------------------------------------------------------

static AST_Node *Parse_Primary(Parser_State *parser);
static AST_Node *Parse_Factor(Parser_State *parser);
static AST_Node *Parse_Term(Parser_State *parser);
static AST_Node *Parse_Simple_Expression(Parser_State *parser);
static AST_Node *Parse_Relation(Parser_State *parser);
static AST_Node *Parse_And_Expression(Parser_State *parser);
static AST_Node *Parse_Or_Expression(Parser_State *parser);
AST_Node *Parse_Type_Definition(Parser_State *parser);
static AST_Node *Parse_Record_Definition(Parser_State *parser);
static AST_Node *Parse_Aggregate(Parser_State *parser, Source_Location loc);
static AST_Node *Parse_Allocator(Parser_State *parser, Source_Location loc);
static AST_Node *Parse_If_Statement(Parser_State *parser);
static AST_Node *Parse_Case_Statement(Parser_State *parser);
static AST_Node *Parse_Loop_Statement(Parser_State *parser, String_Slice label);
static AST_Node *Parse_Block_Statement(Parser_State *parser, String_Slice label);
static AST_Node *Parse_Select_Statement(Parser_State *parser);
static AST_Node *Parse_Procedure(Parser_State *parser);
static AST_Node *Parse_Function(Parser_State *parser);
static AST_Node *Parse_Package(Parser_State *parser);
static AST_Node *Parse_Task(Parser_State *parser);
static void Parse_Representation_Clause(Parser_State *parser);


///-----------------------------------------------------------------------------
///                   P A R S E R   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------
///
///  Initialize parser with source text. Sets up lexer and fetches
///  first two tokens (current and peek) to enable lookahead.
///
///-----------------------------------------------------------------------------

Parser_State Parser_Init(const char *source, size_t length, const char *filename)
{
    Parser_State parser = {0};

    parser.lexer = Lexer_Init(source, length, filename);
    parser.error_count = 0;
    parser.labels = NULL;
    parser.label_count = 0;
    parser.label_capacity = 0;

    /// Fetch first two tokens for lookahead
    parser.current = Lexer_Next(&parser.lexer);
    parser.peek = Lexer_Next(&parser.lexer);

    /// Handle compound tokens (AND THEN, OR ELSE)
    if (parser.current.kind == TK_AND && parser.peek.kind == TK_THEN) {
        parser.current.kind = TK_AND_THEN;
        parser.peek = Lexer_Next(&parser.lexer);
    }
    if (parser.current.kind == TK_OR && parser.peek.kind == TK_ELSE) {
        parser.current.kind = TK_OR_ELSE;
        parser.peek = Lexer_Next(&parser.lexer);
    }

    return parser;
}


///-----------------------------------------------------------------------------
///                   T O K E N   A C C E S S   F U N C T I O N S
///-----------------------------------------------------------------------------
///
///  These functions provide access to the token stream with one-token
///  lookahead. The compound token handling for "AND THEN" and "OR ELSE"
///  is performed here as these are lexically two tokens but syntactically
///  single operators (LRM 4.4).
///
///-----------------------------------------------------------------------------

void Parser_Advance(Parser_State *parser)
{
    parser->current = parser->peek;
    parser->peek = Lexer_Next(&parser->lexer);

    /// Handle compound tokens
    if (parser->current.kind == TK_AND && parser->peek.kind == TK_THEN) {
        parser->current.kind = TK_AND_THEN;
        parser->peek = Lexer_Next(&parser->lexer);
    }
    if (parser->current.kind == TK_OR && parser->peek.kind == TK_ELSE) {
        parser->current.kind = TK_OR_ELSE;
        parser->peek = Lexer_Next(&parser->lexer);
    }
}


bool Parser_Check(Parser_State *parser, Token_Kind kind)
{
    return parser->current.kind == kind;
}


bool Parser_Match(Parser_State *parser, Token_Kind kind)
{
    if (Parser_Check(parser, kind)) {
        Parser_Advance(parser);
        return true;
    }
    return false;
}


void Parser_Expect(Parser_State *parser, Token_Kind kind)
{
    if (!Parser_Match(parser, kind)) {
        Fatal_Error(parser->current.location,
                   "expected '%s', got '%s'",
                   Token_Names[kind],
                   Token_Names[parser->current.kind]);
    }
}


Source_Location Parser_Location(Parser_State *parser)
{
    return parser->current.location;
}


String_Slice Parser_Identifier(Parser_State *parser)
{
    String_Slice name = String_Dup(parser->current.literal);
    Parser_Expect(parser, TK_IDENTIFIER);
    return name;
}


///-----------------------------------------------------------------------------
///                   A T T R I B U T E   N A M E   P A R S I N G
///-----------------------------------------------------------------------------
///
///  Per LRM 4.1.4, attribute designators can be identifiers or certain
///  reserved words (RANGE, ACCESS, DIGITS, DELTA, MOD, etc.).
///
///-----------------------------------------------------------------------------

static String_Slice Parse_Attribute_Designator(Parser_State *parser)
{
    String_Slice result;

    if (Parser_Check(parser, TK_IDENTIFIER)) {
        result = Parser_Identifier(parser);
    }
    else if (Parser_Match(parser, TK_RANGE)) {
        result = STR("RANGE");
    }
    else if (Parser_Match(parser, TK_ACCESS)) {
        result = STR("ACCESS");
    }
    else if (Parser_Match(parser, TK_DIGITS)) {
        result = STR("DIGITS");
    }
    else if (Parser_Match(parser, TK_DELTA)) {
        result = STR("DELTA");
    }
    else if (Parser_Match(parser, TK_MOD)) {
        result = STR("MOD");
    }
    else if (Parser_Match(parser, TK_REM)) {
        result = STR("REM");
    }
    else if (Parser_Match(parser, TK_ABS)) {
        result = STR("ABS");
    }
    else if (Parser_Match(parser, TK_NOT)) {
        result = STR("NOT");
    }
    else if (Parser_Match(parser, TK_AND)) {
        result = STR("AND");
    }
    else if (Parser_Match(parser, TK_OR)) {
        result = STR("OR");
    }
    else if (Parser_Match(parser, TK_XOR)) {
        result = STR("XOR");
    }
    else if (Parser_Match(parser, TK_PLUS)) {
        result = STR("+");
    }
    else if (Parser_Match(parser, TK_MINUS)) {
        result = STR("-");
    }
    else if (Parser_Match(parser, TK_STAR)) {
        result = STR("*");
    }
    else if (Parser_Match(parser, TK_SLASH)) {
        result = STR("/");
    }
    else if (Parser_Match(parser, TK_EQUAL)) {
        result = STR("=");
    }
    else if (Parser_Match(parser, TK_NOT_EQUAL)) {
        result = STR("/=");
    }
    else if (Parser_Match(parser, TK_LESS_THAN)) {
        result = STR("<");
    }
    else if (Parser_Match(parser, TK_LESS_EQUAL)) {
        result = STR("<=");
    }
    else if (Parser_Match(parser, TK_GREATER_THAN)) {
        result = STR(">");
    }
    else if (Parser_Match(parser, TK_GREATER_EQUAL)) {
        result = STR(">=");
    }
    else if (Parser_Match(parser, TK_AMPERSAND)) {
        result = STR("&");
    }
    else if (Parser_Match(parser, TK_DOUBLE_STAR)) {
        result = STR("**");
    }
    else {
        Fatal_Error(Parser_Location(parser), "expected attribute designator");
    }

    return result;
}


///-----------------------------------------------------------------------------
///                   N A M E   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 4.1: name ::= simple_name | indexed_component | slice |
///                    selected_component | attribute
///
///  Names are parsed left-to-right, building up from a simple identifier
///  by adding suffixes (dot selection, indexing, attributes, etc.).
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Name(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    /// Start with identifier
    AST_Node *node = AST_New(N_ID, loc);
    node->string_val = Parser_Identifier(parser);

    /// Parse suffixes
    for (;;) {
        if (Parser_Match(parser, TK_DOT)) {
            /// Selected component or .ALL dereference
            if (Parser_Match(parser, TK_ALL)) {
                AST_Node *deref = AST_New(N_DRF, loc);
                deref->unary.operand = node;
                node = deref;
            }
            else {
                AST_Node *sel = AST_New(N_SEL, loc);
                sel->selected.prefix = node;

                /// Selector can be identifier, string literal, or character
                if (Parser_Check(parser, TK_STRING)) {
                    sel->selected.selector = String_Dup(parser->current.literal);
                    Parser_Advance(parser);
                }
                else if (Parser_Check(parser, TK_CHARACTER)) {
                    char *buf = Arena_Alloc(2);
                    buf[0] = (char)parser->current.integer_value;
                    buf[1] = '\0';
                    sel->selected.selector = (String_Slice){buf, 1};
                    Parser_Advance(parser);
                }
                else {
                    sel->selected.selector = Parser_Identifier(parser);
                }
                node = sel;
            }
        }
        else if (Parser_Match(parser, TK_TICK)) {
            /// Attribute or qualified expression
            if (Parser_Check(parser, TK_LEFT_PAREN)) {
                /// Qualified expression: T'(expr)
                Parser_Advance(parser);
                AST_Node *qual = AST_New(N_QL, loc);
                qual->qualified.type_name = node;
                qual->qualified.expression = Parse_Expression(parser);
                Parser_Expect(parser, TK_RIGHT_PAREN);
                node = qual;
            }
            else {
                /// Attribute
                String_Slice attr = Parse_Attribute_Designator(parser);
                AST_Node *attr_node = AST_New(N_AT, loc);
                attr_node->attr.prefix = node;
                attr_node->attr.attribute = attr;
                Node_Vector_Init(&attr_node->attr.args);

                /// Optional arguments
                if (Parser_Match(parser, TK_LEFT_PAREN)) {
                    do {
                        Node_Vector_Push(&attr_node->attr.args,
                                        Parse_Expression(parser));
                    } while (Parser_Match(parser, TK_COMMA));
                    Parser_Expect(parser, TK_RIGHT_PAREN);
                }
                node = attr_node;
            }
        }
        else if (Parser_Check(parser, TK_LEFT_PAREN)) {
            /// Function call, indexed component, or type conversion
            Parser_Advance(parser);

            if (Parser_Check(parser, TK_RIGHT_PAREN)) {
                /// Empty argument list: function call
                Parser_Expect(parser, TK_RIGHT_PAREN);
                AST_Node *call = AST_New(N_CL, loc);
                call->call.callee = node;
                Node_Vector_Init(&call->call.args);
                node = call;
            }
            else {
                /// Parse argument list
                Node_Vector args;
                Node_Vector_Init(&args);

                do {
                    Node_Vector choices;
                    Node_Vector_Init(&choices);

                    AST_Node *expr = Parse_Expression(parser);
                    Node_Vector_Push(&choices, expr);

                    /// Check for choice list (a | b | c)
                    while (Parser_Match(parser, TK_BAR)) {
                        Node_Vector_Push(&choices, Parse_Expression(parser));
                    }

                    if (Parser_Match(parser, TK_ARROW)) {
                        /// Named association
                        AST_Node *val = Parse_Expression(parser);
                        for (uint32_t i = 0; i < choices.count; i++) {
                            AST_Node *assoc = AST_New(N_ASC, loc);
                            Node_Vector_Init(&assoc->association.choices);
                            Node_Vector_Push(&assoc->association.choices, choices.data[i]);
                            assoc->association.value = val;
                            Node_Vector_Push(&args, assoc);
                        }
                    }
                    else {
                        /// Positional argument
                        if (choices.count == 1) {
                            Node_Vector_Push(&args, choices.data[0]);
                        }
                        else {
                            Fatal_Error(loc, "expected '=>'");
                        }
                    }
                } while (Parser_Match(parser, TK_COMMA));

                Parser_Expect(parser, TK_RIGHT_PAREN);

                /// Could be call or indexed component - create as call
                AST_Node *call = AST_New(N_CL, loc);
                call->call.callee = node;
                call->call.args = args;
                node = call;
            }
        }
        else {
            break;
        }
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   P R I M A R Y   E X P R E S S I O N S
///-----------------------------------------------------------------------------
///
///  LRM 4.4: primary ::= numeric_literal | null | aggregate | string_literal |
///                       name | allocator | qualified_expression |
///                       '(' expression ')'
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Primary(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    /// Parenthesized expression or aggregate
    if (Parser_Match(parser, TK_LEFT_PAREN)) {
        return Parse_Aggregate(parser, loc);
    }

    /// Allocator: NEW subtype_indication [aggregate]
    if (Parser_Match(parser, TK_NEW)) {
        return Parse_Allocator(parser, loc);
    }

    /// NULL literal
    if (Parser_Match(parser, TK_NULL)) {
        return AST_New(N_NULL, loc);
    }

    /// OTHERS (in aggregates)
    if (Parser_Match(parser, TK_OTHERS)) {
        AST_Node *node = AST_New(N_ID, loc);
        node->string_val = STR("others");
        return node;
    }

    /// Integer literal
    if (Parser_Check(parser, TK_INTEGER)) {
        AST_Node *node = AST_New(N_INT, loc);
        node->integer_val = parser->current.integer_value;
        Parser_Advance(parser);
        return node;
    }

    /// Real literal
    if (Parser_Check(parser, TK_REAL)) {
        AST_Node *node = AST_New(N_REAL, loc);
        node->real_val = parser->current.real_value;
        Parser_Advance(parser);
        return node;
    }

    /// Character literal
    if (Parser_Check(parser, TK_CHARACTER)) {
        AST_Node *node = AST_New(N_CHAR, loc);
        node->integer_val = (char)parser->current.integer_value;
        Parser_Advance(parser);
        return node;
    }

    /// String literal
    if (Parser_Check(parser, TK_STRING)) {
        AST_Node *node = AST_New(N_STR, loc);
        node->string_val = String_Dup(parser->current.literal);
        Parser_Advance(parser);

        /// String can be followed by function call syntax for operator symbols
        while (Parser_Check(parser, TK_LEFT_PAREN)) {
            Parser_Advance(parser);
            Node_Vector args;
            Node_Vector_Init(&args);

            do {
                Node_Vector_Push(&args, Parse_Expression(parser));
            } while (Parser_Match(parser, TK_COMMA));

            Parser_Expect(parser, TK_RIGHT_PAREN);

            AST_Node *call = AST_New(N_CL, loc);
            call->call.callee = node;
            call->call.args = args;
            node = call;
        }
        return node;
    }

    /// Identifier/name
    if (Parser_Check(parser, TK_IDENTIFIER)) {
        return Parse_Name(parser);
    }

    /// Unary NOT
    if (Parser_Match(parser, TK_NOT)) {
        AST_Node *node = AST_New(N_UN, loc);
        node->unary.op = TK_NOT;
        node->unary.operand = Parse_Primary(parser);
        return node;
    }

    /// Unary ABS
    if (Parser_Match(parser, TK_ABS)) {
        AST_Node *node = AST_New(N_UN, loc);
        node->unary.op = TK_ABS;
        node->unary.operand = Parse_Primary(parser);
        return node;
    }

    /// .ALL (dereference without prefix - error)
    if (Parser_Match(parser, TK_ALL)) {
        AST_Node *node = AST_New(N_DRF, loc);
        node->unary.operand = Parse_Primary(parser);
        return node;
    }

    Fatal_Error(loc, "expected expression");
}


///-----------------------------------------------------------------------------
///                   A G G R E G A T E   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 4.3: aggregate ::= '(' component_association {, component_association} ')'
///
///  Aggregates and parenthesized expressions share the same syntax start.
///  We distinguish them based on context (presence of =>, comma, etc.).
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Aggregate(Parser_State *parser, Source_Location loc)
{
    Node_Vector items;
    Node_Vector_Init(&items);

    do {
        Node_Vector choices;
        Node_Vector_Init(&choices);

        AST_Node *expr = Parse_Expression(parser);
        Node_Vector_Push(&choices, expr);

        /// Choice list: a | b | c
        while (Parser_Match(parser, TK_BAR)) {
            Node_Vector_Push(&choices, Parse_Expression(parser));
        }

        if (Parser_Match(parser, TK_ARROW)) {
            /// Named association
            AST_Node *val = Parse_Expression(parser);
            for (uint32_t i = 0; i < choices.count; i++) {
                AST_Node *assoc = AST_New(N_ASC, loc);
                Node_Vector_Init(&assoc->association.choices);
                Node_Vector_Push(&assoc->association.choices, choices.data[i]);
                assoc->association.value = val;
                Node_Vector_Push(&items, assoc);
            }
        }
        else if (choices.count == 1 && choices.data[0]->kind == N_ID
                && Parser_Match(parser, TK_RANGE)) {
            /// Subtype indication with range in aggregate
            AST_Node *rng = Parse_Range(parser);
            Parser_Expect(parser, TK_ARROW);
            AST_Node *val = Parse_Expression(parser);

            AST_Node *subtype = AST_New(N_ST, loc);
            subtype->subtype.type_mark = choices.data[0];
            AST_Node *constraint = AST_New(N_CN, loc);
            constraint->constraint.range_constraint = rng;
            subtype->subtype.constraint = constraint;

            AST_Node *assoc = AST_New(N_ASC, loc);
            Node_Vector_Init(&assoc->association.choices);
            Node_Vector_Push(&assoc->association.choices, subtype);
            assoc->association.value = val;
            Node_Vector_Push(&items, assoc);
        }
        else {
            /// Positional
            if (choices.count == 1) {
                Node_Vector_Push(&items, choices.data[0]);
            }
            else {
                Fatal_Error(loc, "expected '=>'");
            }
        }
    } while (Parser_Match(parser, TK_COMMA));

    Parser_Expect(parser, TK_RIGHT_PAREN);

    /// Single non-association item is parenthesized expression
    if (items.count == 1 && items.data[0]->kind != N_ASC) {
        return items.data[0];
    }

    /// Otherwise it's an aggregate
    AST_Node *node = AST_New(N_AG, loc);
    node->aggregate.items = items;
    return node;
}


///-----------------------------------------------------------------------------
///                   A L L O C A T O R   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 4.8: allocator ::= NEW subtype_indication | NEW qualified_expression
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Allocator(Parser_State *parser, Source_Location loc)
{
    AST_Node *node = AST_New(N_ALC, loc);
    node->allocator.subtype = Parse_Name(parser);

    /// Optional initial value via 'T'(value) qualified expression
    if (Parser_Match(parser, TK_TICK)) {
        Parser_Expect(parser, TK_LEFT_PAREN);
        node->allocator.init_value = Parse_Expression(parser);
        Parser_Expect(parser, TK_RIGHT_PAREN);
    }
    else {
        node->allocator.init_value = NULL;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   F A C T O R   ( E X P O N E N T I A T I O N )
///-----------------------------------------------------------------------------
///
///  LRM 4.4 Level 6: factor ::= primary [** primary] | ABS primary | NOT primary
///
///  Note: ** is right-associative
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Factor(Parser_State *parser)
{
    AST_Node *node = Parse_Primary(parser);

    if (Parser_Match(parser, TK_DOUBLE_STAR)) {
        Source_Location loc = Parser_Location(parser);
        AST_Node *bin = AST_New(N_BIN, loc);
        bin->binary.op = TK_DOUBLE_STAR;
        bin->binary.left = node;
        bin->binary.right = Parse_Factor(parser);  // Right-associative
        return bin;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   T E R M   ( M U L T I P L Y I N G )
///-----------------------------------------------------------------------------
///
///  LRM 4.4 Level 5: term ::= factor {multiplying_operator factor}
///  multiplying_operator ::= * | / | MOD | REM
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Term(Parser_State *parser)
{
    AST_Node *node = Parse_Factor(parser);

    while (Parser_Check(parser, TK_STAR) ||
           Parser_Check(parser, TK_SLASH) ||
           Parser_Check(parser, TK_MOD) ||
           Parser_Check(parser, TK_REM)) {

        Token_Kind op = parser->current.kind;
        Parser_Advance(parser);

        Source_Location loc = Parser_Location(parser);
        AST_Node *bin = AST_New(N_BIN, loc);
        bin->binary.op = op;
        bin->binary.left = node;
        bin->binary.right = Parse_Factor(parser);
        node = bin;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   S I M P L E   E X P R E S S I O N
///-----------------------------------------------------------------------------
///
///  LRM 4.4 Levels 3-4: simple_expression ::= [unary_adding_operator] term
///                                            {binary_adding_operator term}
///  adding_operator ::= + | - | &
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Simple_Expression(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Token_Kind unary_op = TK_EOF;

    /// Optional unary + or -
    if (Parser_Match(parser, TK_MINUS)) {
        unary_op = TK_MINUS;
    }
    else if (Parser_Match(parser, TK_PLUS)) {
        unary_op = TK_PLUS;
    }

    AST_Node *node = Parse_Term(parser);

    /// Apply unary operator
    if (unary_op != TK_EOF) {
        AST_Node *unary = AST_New(N_UN, loc);
        unary->unary.op = unary_op;
        unary->unary.operand = node;
        node = unary;
    }

    /// Binary adding operators
    while (Parser_Check(parser, TK_PLUS) ||
           Parser_Check(parser, TK_MINUS) ||
           Parser_Check(parser, TK_AMPERSAND)) {

        Token_Kind op = parser->current.kind;
        Parser_Advance(parser);

        loc = Parser_Location(parser);
        AST_Node *bin = AST_New(N_BIN, loc);
        bin->binary.op = op;
        bin->binary.left = node;
        bin->binary.right = Parse_Term(parser);
        node = bin;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   R A N G E   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 3.5: range ::= simple_expression .. simple_expression |
///                     range_attribute
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Range(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    /// Box <> for unconstrained
    if (Parser_Match(parser, TK_BOX)) {
        AST_Node *node = AST_New(N_RN, loc);
        node->range.low_bound = NULL;
        node->range.high_bound = NULL;
        return node;
    }

    AST_Node *low = Parse_Simple_Expression(parser);

    if (Parser_Match(parser, TK_DOUBLE_DOT)) {
        AST_Node *node = AST_New(N_RN, loc);
        node->range.low_bound = low;
        node->range.high_bound = Parse_Simple_Expression(parser);
        return node;
    }

    return low;
}


///-----------------------------------------------------------------------------
///                   R E L A T I O N   E X P R E S S I O N
///-----------------------------------------------------------------------------
///
///  LRM 4.4 Level 2: relation ::= simple_expression [relational_operator
///                                simple_expression]
///                              | simple_expression [NOT] IN range
///                              | simple_expression [NOT] IN type_mark
///
///  relational_operator ::= = | /= | < | <= | > | >=
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Relation(Parser_State *parser)
{
    AST_Node *node = Parse_Simple_Expression(parser);

    /// Range expression: lo .. hi
    if (Parser_Match(parser, TK_DOUBLE_DOT)) {
        Source_Location loc = Parser_Location(parser);
        AST_Node *rng = AST_New(N_RN, loc);
        rng->range.low_bound = node;
        rng->range.high_bound = Parse_Simple_Expression(parser);
        return rng;
    }

    /// Relational operators
    if (Parser_Check(parser, TK_EQUAL) ||
        Parser_Check(parser, TK_NOT_EQUAL) ||
        Parser_Check(parser, TK_LESS_THAN) ||
        Parser_Check(parser, TK_LESS_EQUAL) ||
        Parser_Check(parser, TK_GREATER_THAN) ||
        Parser_Check(parser, TK_GREATER_EQUAL) ||
        Parser_Check(parser, TK_IN) ||
        Parser_Check(parser, TK_NOT)) {

        Token_Kind op = parser->current.kind;
        Parser_Advance(parser);

        /// NOT IN membership test
        if (op == TK_NOT) {
            Parser_Expect(parser, TK_IN);
        }

        Source_Location loc = Parser_Location(parser);
        AST_Node *bin = AST_New(N_BIN, loc);
        bin->binary.op = op;
        bin->binary.left = node;

        if (op == TK_IN || op == TK_NOT) {
            bin->binary.right = Parse_Range(parser);
        }
        else {
            bin->binary.right = Parse_Simple_Expression(parser);
        }

        return bin;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   A N D   E X P R E S S I O N
///-----------------------------------------------------------------------------
///
///  LRM 4.4 Level 1 (partial): and_expression ::= relation {AND relation}
///                                              | relation {AND THEN relation}
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_And_Expression(Parser_State *parser)
{
    AST_Node *node = Parse_Relation(parser);

    while (Parser_Check(parser, TK_AND) || Parser_Check(parser, TK_AND_THEN)) {
        Token_Kind op = parser->current.kind;
        Parser_Advance(parser);

        Source_Location loc = Parser_Location(parser);
        AST_Node *bin = AST_New(N_BIN, loc);
        bin->binary.op = op;
        bin->binary.left = node;
        bin->binary.right = Parse_Relation(parser);
        node = bin;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   O R   E X P R E S S I O N
///-----------------------------------------------------------------------------
///
///  LRM 4.4 Level 1: expression ::= and_expression {OR and_expression}
///                                | and_expression {OR ELSE and_expression}
///                                | and_expression {XOR and_expression}
///
///  Note: OR, OR ELSE, and XOR cannot be mixed in the same expression.
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Or_Expression(Parser_State *parser)
{
    AST_Node *node = Parse_And_Expression(parser);

    while (Parser_Check(parser, TK_OR) ||
           Parser_Check(parser, TK_OR_ELSE) ||
           Parser_Check(parser, TK_XOR)) {

        Token_Kind op = parser->current.kind;
        Parser_Advance(parser);

        Source_Location loc = Parser_Location(parser);
        AST_Node *bin = AST_New(N_BIN, loc);
        bin->binary.op = op;
        bin->binary.left = node;
        bin->binary.right = Parse_And_Expression(parser);
        node = bin;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   E X P R E S S I O N   ( E N T R Y   P O I N T )
///-----------------------------------------------------------------------------

AST_Node *Parse_Expression(Parser_State *parser)
{
    return Parse_Or_Expression(parser);
}


///-----------------------------------------------------------------------------
///                   S U B T Y P E   I N D I C A T I O N
///-----------------------------------------------------------------------------
///
///  LRM 3.3.2: subtype_indication ::= type_mark [constraint]
///  constraint ::= range_constraint | index_constraint | discriminant_constraint
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Subtype_Indication(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    /// Parse type mark (name)
    AST_Node *type_mark = Parse_Name(parser);

    /// Optional DELTA constraint
    if (Parser_Match(parser, TK_DELTA)) {
        Parse_Simple_Expression(parser);
    }

    /// Optional DIGITS constraint
    if (Parser_Match(parser, TK_DIGITS)) {
        Parse_Expression(parser);
    }

    /// RANGE constraint
    if (Parser_Match(parser, TK_RANGE)) {
        Source_Location rloc = Parser_Location(parser);
        AST_Node *constraint = AST_New(N_CN, rloc);
        constraint->constraint.range_constraint = Parse_Range(parser);

        AST_Node *node = AST_New(N_ST, loc);
        node->subtype.type_mark = type_mark;
        node->subtype.constraint = constraint;
        return node;
    }

    /// Index/discriminant constraint (parenthesized)
    if (Parser_Check(parser, TK_LEFT_PAREN)) {
        Parser_Advance(parser);
        Source_Location cloc = Parser_Location(parser);
        AST_Node *constraint = AST_New(N_CN, cloc);
        Node_Vector_Init(&constraint->index_constraint.ranges);

        do {
            AST_Node *r = Parse_Range(parser);
            Node_Vector_Push(&constraint->index_constraint.ranges, r);
        } while (Parser_Match(parser, TK_COMMA));

        Parser_Expect(parser, TK_RIGHT_PAREN);

        AST_Node *node = AST_New(N_ST, loc);
        node->subtype.type_mark = type_mark;
        node->subtype.constraint = constraint;
        return node;
    }

    return type_mark;
}


///-----------------------------------------------------------------------------
///                   P A R A M E T E R   L I S T   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 6.1: parameter_specification ::=
///      identifier_list : mode subtype_indication [:= expression]
///
///  mode ::= [IN] | IN OUT | OUT
///
///-----------------------------------------------------------------------------

Node_Vector Parse_Parameter_List(Parser_State *parser)
{
    Node_Vector params;
    Node_Vector_Init(&params);

    if (!Parser_Match(parser, TK_LEFT_PAREN)) {
        return params;
    }

    do {
        Source_Location loc = Parser_Location(parser);

        /// Parse identifier list
        Node_Vector ids;
        Node_Vector_Init(&ids);
        do {
            String_Slice name = Parser_Identifier(parser);
            AST_Node *id = AST_New(N_ID, loc);
            id->string_val = name;
            Node_Vector_Push(&ids, id);
        } while (Parser_Match(parser, TK_COMMA));

        Parser_Expect(parser, TK_COLON);

        /// Parse mode: IN, OUT, IN OUT
        uint8_t mode = 0;
        if (Parser_Match(parser, TK_IN)) {
            mode |= 1;
        }
        if (Parser_Match(parser, TK_OUT)) {
            mode |= 2;
        }
        if (mode == 0) {
            mode = 1;  // Default is IN
        }

        /// Parse type
        AST_Node *type = Parse_Name(parser);

        /// Optional default
        AST_Node *default_val = NULL;
        if (Parser_Match(parser, TK_ASSIGN)) {
            default_val = Parse_Expression(parser);
        }

        /// Create parameter node for each identifier
        for (uint32_t i = 0; i < ids.count; i++) {
            AST_Node *param = AST_New(N_PM, loc);
            param->param.param_name = ids.data[i]->string_val;
            param->param.param_type = type;
            param->param.default_value = default_val;
            param->param.mode = mode;
            Node_Vector_Push(&params, param);
        }

    } while (Parser_Match(parser, TK_SEMICOLON));

    Parser_Expect(parser, TK_RIGHT_PAREN);
    return params;
}


///-----------------------------------------------------------------------------
///                   T Y P E   D E F I N I T I O N   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 3.3: type_definition ::= enumeration_type_definition |
///                               integer_type_definition | real_type_definition |
///                               array_type_definition | record_type_definition |
///                               access_type_definition | derived_type_definition |
///                               private_type_definition
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Type_Definition(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    /// Enumeration type: (lit1, lit2, ...)
    if (Parser_Match(parser, TK_LEFT_PAREN)) {
        AST_Node *node = AST_New(N_TE, loc);
        Node_Vector_Init(&node->enumeration.literals);

        do {
            if (Parser_Check(parser, TK_CHARACTER)) {
                AST_Node *lit = AST_New(N_CHAR, loc);
                lit->integer_val = (char)parser->current.integer_value;
                Parser_Advance(parser);
                Node_Vector_Push(&node->enumeration.literals, lit);
            }
            else {
                String_Slice name = Parser_Identifier(parser);
                AST_Node *id = AST_New(N_ID, loc);
                id->string_val = name;
                Node_Vector_Push(&node->enumeration.literals, id);
            }
        } while (Parser_Match(parser, TK_COMMA));

        Parser_Expect(parser, TK_RIGHT_PAREN);
        return node;
    }

    /// Integer type: RANGE lo .. hi
    if (Parser_Match(parser, TK_RANGE)) {
        AST_Node *node = AST_New(N_TI, loc);
        if (Parser_Match(parser, TK_BOX)) {
            node->range.low_bound = NULL;
            node->range.high_bound = NULL;
        }
        else {
            node->range.low_bound = Parse_Simple_Expression(parser);
            Parser_Expect(parser, TK_DOUBLE_DOT);
            node->range.high_bound = Parse_Simple_Expression(parser);
        }
        return node;
    }

    /// Modular type: MOD expression
    if (Parser_Match(parser, TK_MOD)) {
        AST_Node *node = AST_New(N_TI, loc);
        node->unary.op = TK_MOD;
        node->unary.operand = Parse_Expression(parser);
        return node;
    }

    /// Floating point: DIGITS expression [RANGE ...]
    if (Parser_Match(parser, TK_DIGITS)) {
        AST_Node *node = AST_New(N_TF, loc);
        if (Parser_Match(parser, TK_BOX)) {
            node->unary.operand = NULL;
        }
        else {
            node->unary.operand = Parse_Expression(parser);
        }
        if (Parser_Match(parser, TK_RANGE)) {
            node->range.low_bound = Parse_Simple_Expression(parser);
            Parser_Expect(parser, TK_DOUBLE_DOT);
            node->range.high_bound = Parse_Simple_Expression(parser);
        }
        return node;
    }

    /// Fixed point: DELTA expression RANGE ...
    if (Parser_Match(parser, TK_DELTA)) {
        AST_Node *node = AST_New(N_TX, loc);
        if (Parser_Match(parser, TK_BOX)) {
            node->range.low_bound = NULL;
            node->range.high_bound = NULL;
            node->binary.right = NULL;
        }
        else {
            node->range.low_bound = Parse_Expression(parser);
            Parser_Expect(parser, TK_RANGE);
            node->range.high_bound = Parse_Simple_Expression(parser);
            Parser_Expect(parser, TK_DOUBLE_DOT);
            node->binary.right = Parse_Simple_Expression(parser);
        }
        return node;
    }

    /// Array type: ARRAY (index_spec) OF element_type
    if (Parser_Match(parser, TK_ARRAY)) {
        Parser_Expect(parser, TK_LEFT_PAREN);

        AST_Node *node = AST_New(N_TA, loc);
        Node_Vector_Init(&node->array_type.indices);

        do {
            AST_Node *idx = Parse_Range(parser);

            /// Check for discrete_subtype_indication with RANGE
            if (idx->kind == N_ID && Parser_Match(parser, TK_RANGE)) {
                AST_Node *subtype = AST_New(N_ST, loc);
                subtype->subtype.type_mark = idx;
                AST_Node *constraint = AST_New(N_CN, loc);
                constraint->constraint.range_constraint = Parse_Range(parser);
                subtype->subtype.constraint = constraint;
                Node_Vector_Push(&node->array_type.indices, subtype);
            }
            else {
                Node_Vector_Push(&node->array_type.indices, idx);
            }
        } while (Parser_Match(parser, TK_COMMA));

        Parser_Expect(parser, TK_RIGHT_PAREN);
        Parser_Expect(parser, TK_OF);

        node->array_type.element_type = Parse_Subtype_Indication(parser);
        return node;
    }

    /// Record type
    if (Parser_Match(parser, TK_RECORD)) {
        return Parse_Record_Definition(parser);
    }

    /// Access type: ACCESS subtype_indication
    if (Parser_Match(parser, TK_ACCESS)) {
        AST_Node *node = AST_New(N_TAC, loc);
        node->unary.operand = Parse_Subtype_Indication(parser);
        return node;
    }

    /// Private type
    if (Parser_Match(parser, TK_PRIVATE)) {
        return AST_New(N_TP, loc);
    }

    /// Limited private
    if (Parser_Match(parser, TK_LIMITED)) {
        Parser_Match(parser, TK_PRIVATE);
        return AST_New(N_TP, loc);
    }

    /// Otherwise it's a subtype indication (derived type base)
    return Parse_Subtype_Indication(parser);
}


///-----------------------------------------------------------------------------
///                   R E C O R D   D E F I N I T I O N
///-----------------------------------------------------------------------------
///
///  LRM 3.7: record_type_definition ::= RECORD
///      component_list
///  END RECORD
///
///-----------------------------------------------------------------------------

static AST_Node *Parse_Record_Definition(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    AST_Node *node = AST_New(N_TR, loc);
    Node_Vector_Init(&node->record_type.components);

    uint32_t offset = 0;

    /// Optional discriminant part before RECORD
    /// (handled by caller - we're already past RECORD keyword)

    /// Parse components
    while (!Parser_Check(parser, TK_END) &&
           !Parser_Check(parser, TK_CASE) &&
           !Parser_Check(parser, TK_NULL)) {

        /// Parse identifier list
        Node_Vector ids;
        Node_Vector_Init(&ids);
        do {
            String_Slice name = Parser_Identifier(parser);
            AST_Node *id = AST_New(N_ID, loc);
            id->string_val = name;
            Node_Vector_Push(&ids, id);
        } while (Parser_Match(parser, TK_COMMA));

        Parser_Expect(parser, TK_COLON);

        /// Parse type
        AST_Node *type = Parse_Subtype_Indication(parser);

        /// Optional default
        AST_Node *init = NULL;
        if (Parser_Match(parser, TK_ASSIGN)) {
            init = Parse_Expression(parser);
        }

        Parser_Expect(parser, TK_SEMICOLON);

        /// Create component for each identifier
        for (uint32_t i = 0; i < ids.count; i++) {
            AST_Node *comp = AST_New(N_CM, loc);
            comp->component.name = ids.data[i]->string_val;
            comp->component.comp_type = type;
            comp->component.init_value = init;
            comp->component.offset = offset++;
            Node_Vector_Push(&node->record_type.components, comp);
        }
    }

    /// NULL;
    if (Parser_Match(parser, TK_NULL)) {
        Parser_Expect(parser, TK_SEMICOLON);
    }

    /// Variant part
    if (Parser_Match(parser, TK_CASE)) {
        AST_Node *variant = AST_New(N_VP, loc);
        variant->variant_part.discriminant = Parse_Name(parser);
        Node_Vector_Init(&variant->variant_part.variants);

        Parser_Expect(parser, TK_IS);

        while (Parser_Match(parser, TK_WHEN)) {
            AST_Node *var = AST_New(N_VR, loc);
            Node_Vector_Init(&var->variant.choices);
            Node_Vector_Init(&var->variant.components);

            /// Parse choices
            do {
                AST_Node *choice = Parse_Expression(parser);
                if (Parser_Match(parser, TK_DOUBLE_DOT)) {
                    AST_Node *rng = AST_New(N_RN, loc);
                    rng->range.low_bound = choice;
                    rng->range.high_bound = Parse_Expression(parser);
                    choice = rng;
                }
                Node_Vector_Push(&var->variant.choices, choice);
            } while (Parser_Match(parser, TK_BAR));

            Parser_Expect(parser, TK_ARROW);

            /// Parse variant components
            while (!Parser_Check(parser, TK_WHEN) &&
                   !Parser_Check(parser, TK_END) &&
                   !Parser_Check(parser, TK_NULL)) {

                Node_Vector ids;
                Node_Vector_Init(&ids);
                do {
                    String_Slice name = Parser_Identifier(parser);
                    AST_Node *id = AST_New(N_ID, loc);
                    id->string_val = name;
                    Node_Vector_Push(&ids, id);
                } while (Parser_Match(parser, TK_COMMA));

                Parser_Expect(parser, TK_COLON);
                AST_Node *type = Parse_Subtype_Indication(parser);

                AST_Node *init = NULL;
                if (Parser_Match(parser, TK_ASSIGN)) {
                    init = Parse_Expression(parser);
                }

                Parser_Expect(parser, TK_SEMICOLON);

                for (uint32_t i = 0; i < ids.count; i++) {
                    AST_Node *comp = AST_New(N_CM, loc);
                    comp->component.name = ids.data[i]->string_val;
                    comp->component.comp_type = type;
                    comp->component.init_value = init;
                    comp->component.offset = offset++;
                    Node_Vector_Push(&var->variant.components, comp);
                }
            }

            if (Parser_Match(parser, TK_NULL)) {
                Parser_Expect(parser, TK_SEMICOLON);
            }

            Node_Vector_Push(&variant->variant_part.variants, var);
        }

        if (Parser_Match(parser, TK_NULL)) {
            Parser_Expect(parser, TK_RECORD);
        }

        Parser_Expect(parser, TK_END);
        Parser_Expect(parser, TK_CASE);
        Parser_Expect(parser, TK_SEMICOLON);

        Node_Vector_Push(&node->record_type.components, variant);
    }

    Parser_Expect(parser, TK_END);
    Parser_Expect(parser, TK_RECORD);

    return node;
}


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM Chapter 5: statement ::= {label} simple_statement | {label} compound_statement
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Statement(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    String_Slice label = {NULL, 0};

    /// Parse label: <<label_name>>
    while (Parser_Match(parser, TK_DOUBLE_LESS)) {
        label = Parser_Identifier(parser);
        Parser_Expect(parser, TK_DOUBLE_GREATER);
        /// Add to label list
        if (parser->label_count >= parser->label_capacity) {
            parser->label_capacity = parser->label_capacity ? parser->label_capacity * 2 : 16;
            parser->labels = realloc(parser->labels,
                                    parser->label_capacity * sizeof(String_Slice));
        }
        parser->labels[parser->label_count++] = label;
    }

    /// Alternative label syntax: identifier :
    if (label.data == NULL && Parser_Check(parser, TK_IDENTIFIER) &&
        parser->peek.kind == TK_COLON) {
        label = Parser_Identifier(parser);
        Parser_Expect(parser, TK_COLON);
        if (parser->label_count >= parser->label_capacity) {
            parser->label_capacity = parser->label_capacity ? parser->label_capacity * 2 : 16;
            parser->labels = realloc(parser->labels,
                                    parser->label_capacity * sizeof(String_Slice));
        }
        parser->labels[parser->label_count++] = label;
    }

    /// IF statement
    if (Parser_Check(parser, TK_IF)) {
        return Parse_If_Statement(parser);
    }

    /// CASE statement
    if (Parser_Check(parser, TK_CASE)) {
        return Parse_Case_Statement(parser);
    }

    /// SELECT statement
    if (Parser_Check(parser, TK_SELECT)) {
        return Parse_Select_Statement(parser);
    }

    /// LOOP, WHILE, FOR statements
    if (Parser_Check(parser, TK_LOOP) ||
        Parser_Check(parser, TK_WHILE) ||
        Parser_Check(parser, TK_FOR)) {
        return Parse_Loop_Statement(parser, label);
    }

    /// DECLARE or BEGIN block
    if (Parser_Check(parser, TK_DECLARE) || Parser_Check(parser, TK_BEGIN)) {
        return Parse_Block_Statement(parser, label);
    }

    /// If we have a label, wrap following statement
    if (label.data != NULL) {
        AST_Node *block = AST_New(N_BL, loc);
        block->block_stmt.label = label;
        Node_Vector_Init(&block->block_stmt.decls);
        Node_Vector_Init(&block->block_stmt.stmts);
        Node_Vector_Init(&block->block_stmt.handlers);
        Node_Vector_Push(&block->block_stmt.stmts, Parse_Statement(parser));
        return block;
    }

    /// ACCEPT statement
    if (Parser_Match(parser, TK_ACCEPT)) {
        AST_Node *node = AST_New(N_ACC, loc);
        node->accept_stmt.name = Parser_Identifier(parser);
        Node_Vector_Init(&node->accept_stmt.indices);
        node->accept_stmt.params = Parse_Parameter_List(parser);
        Node_Vector_Init(&node->accept_stmt.stmts);

        if (Parser_Match(parser, TK_DO)) {
            while (!Parser_Check(parser, TK_END)) {
                Node_Vector_Push(&node->accept_stmt.stmts, Parse_Statement(parser));
            }
            Parser_Expect(parser, TK_END);
            if (Parser_Check(parser, TK_IDENTIFIER)) {
                Parser_Advance(parser);
            }
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// DELAY statement
    if (Parser_Match(parser, TK_DELAY)) {
        AST_Node *node = AST_New(N_DL, loc);
        node->delay_stmt.duration = Parse_Expression(parser);
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// ABORT statement
    if (Parser_Match(parser, TK_ABORT)) {
        AST_Node *node = AST_New(N_AB, loc);
        if (!Parser_Check(parser, TK_SEMICOLON)) {
            node->call_stmt.name = Parse_Name(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// RETURN statement
    if (Parser_Match(parser, TK_RETURN)) {
        AST_Node *node = AST_New(N_RT, loc);
        if (!Parser_Check(parser, TK_SEMICOLON)) {
            node->return_stmt.value = Parse_Expression(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// EXIT statement
    if (Parser_Match(parser, TK_EXIT)) {
        AST_Node *node = AST_New(N_EX, loc);
        if (Parser_Check(parser, TK_IDENTIFIER)) {
            node->exit_stmt.label = Parser_Identifier(parser);
        }
        if (Parser_Match(parser, TK_WHEN)) {
            node->exit_stmt.condition = Parse_Expression(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// GOTO statement
    if (Parser_Match(parser, TK_GOTO)) {
        AST_Node *node = AST_New(N_GT, loc);
        node->goto_stmt.label = Parser_Identifier(parser);
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// RAISE statement
    if (Parser_Match(parser, TK_RAISE)) {
        AST_Node *node = AST_New(N_RS, loc);
        if (!Parser_Check(parser, TK_SEMICOLON)) {
            node->raise_stmt.exception = Parse_Name(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// NULL statement
    if (Parser_Match(parser, TK_NULL)) {
        Parser_Expect(parser, TK_SEMICOLON);
        return AST_New(N_NS, loc);
    }

    /// PRAGMA
    if (Parser_Match(parser, TK_PRAGMA)) {
        AST_Node *node = AST_New(N_PG, loc);
        node->pragma_node.name = Parser_Identifier(parser);
        Node_Vector_Init(&node->pragma_node.args);
        if (Parser_Match(parser, TK_LEFT_PAREN)) {
            do {
                Node_Vector_Push(&node->pragma_node.args, Parse_Expression(parser));
            } while (Parser_Match(parser, TK_COMMA));
            Parser_Expect(parser, TK_RIGHT_PAREN);
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// Assignment or procedure call
    AST_Node *expr = Parse_Name(parser);

    if (Parser_Match(parser, TK_ASSIGN)) {
        /// Assignment statement
        AST_Node *node = AST_New(N_AS, loc);

        /// Convert call to indexed component if needed
        if (expr->kind == N_CL) {
            AST_Node *idx = AST_New(N_IX, loc);
            idx->indexed.prefix = expr->call.callee;
            idx->indexed.indices = expr->call.args;
            expr = idx;
        }

        node->assignment.target = expr;
        node->assignment.value = Parse_Expression(parser);
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// Procedure call
    AST_Node *node = AST_New(N_CLT, loc);
    if (expr->kind == N_IX) {
        node->call.callee = expr->indexed.prefix;
        node->call.args = expr->indexed.indices;
    }
    else if (expr->kind == N_CL) {
        node->call.callee = expr->call.callee;
        node->call.args = expr->call.args;
    }
    else {
        node->call.callee = expr;
        Node_Vector_Init(&node->call.args);
    }
    Parser_Expect(parser, TK_SEMICOLON);
    return node;
}


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   S E Q U E N C E
///-----------------------------------------------------------------------------

Node_Vector Parse_Statement_Sequence(Parser_State *parser)
{
    Node_Vector stmts;
    Node_Vector_Init(&stmts);

    while (!Parser_Check(parser, TK_END) &&
           !Parser_Check(parser, TK_EXCEPTION) &&
           !Parser_Check(parser, TK_ELSIF) &&
           !Parser_Check(parser, TK_ELSE) &&
           !Parser_Check(parser, TK_WHEN) &&
           !Parser_Check(parser, TK_OR)) {
        Node_Vector_Push(&stmts, Parse_Statement(parser));
    }

    return stmts;
}


///-----------------------------------------------------------------------------
///                   I F   S T A T E M E N T
///-----------------------------------------------------------------------------

static AST_Node *Parse_If_Statement(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_IF);

    AST_Node *node = AST_New(N_IF, loc);
    node->if_stmt.condition = Parse_Expression(parser);
    Parser_Expect(parser, TK_THEN);

    Node_Vector_Init(&node->if_stmt.then_stmts);
    while (!Parser_Check(parser, TK_ELSIF) &&
           !Parser_Check(parser, TK_ELSE) &&
           !Parser_Check(parser, TK_END)) {
        Node_Vector_Push(&node->if_stmt.then_stmts, Parse_Statement(parser));
    }

    Node_Vector_Init(&node->if_stmt.elsif_parts);
    while (Parser_Match(parser, TK_ELSIF)) {
        AST_Node *elsif = AST_New(N_EL, loc);
        elsif->if_stmt.condition = Parse_Expression(parser);
        Parser_Expect(parser, TK_THEN);

        Node_Vector_Init(&elsif->if_stmt.then_stmts);
        while (!Parser_Check(parser, TK_ELSIF) &&
               !Parser_Check(parser, TK_ELSE) &&
               !Parser_Check(parser, TK_END)) {
            Node_Vector_Push(&elsif->if_stmt.then_stmts, Parse_Statement(parser));
        }
        Node_Vector_Push(&node->if_stmt.elsif_parts, elsif);
    }

    Node_Vector_Init(&node->if_stmt.else_stmts);
    if (Parser_Match(parser, TK_ELSE)) {
        while (!Parser_Check(parser, TK_END)) {
            Node_Vector_Push(&node->if_stmt.else_stmts, Parse_Statement(parser));
        }
    }

    Parser_Expect(parser, TK_END);
    Parser_Expect(parser, TK_IF);
    Parser_Expect(parser, TK_SEMICOLON);

    return node;
}


///-----------------------------------------------------------------------------
///                   C A S E   S T A T E M E N T
///-----------------------------------------------------------------------------

static AST_Node *Parse_Case_Statement(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_CASE);

    AST_Node *node = AST_New(N_CS, loc);
    node->case_stmt.selector = Parse_Expression(parser);
    Parser_Expect(parser, TK_IS);

    /// Skip pragmas
    while (Parser_Check(parser, TK_PRAGMA)) {
        Parse_Statement(parser);
    }

    Node_Vector_Init(&node->case_stmt.alternatives);

    while (Parser_Match(parser, TK_WHEN)) {
        AST_Node *alt = AST_New(N_WH, loc);
        Node_Vector_Init(&alt->when_clause.choices);

        do {
            AST_Node *choice = Parse_Expression(parser);
            if (choice->kind == N_ID && Parser_Match(parser, TK_RANGE)) {
                AST_Node *rng = Parse_Range(parser);
                Node_Vector_Push(&alt->when_clause.choices, rng);
            }
            else if (Parser_Match(parser, TK_DOUBLE_DOT)) {
                AST_Node *rng = AST_New(N_RN, loc);
                rng->range.low_bound = choice;
                rng->range.high_bound = Parse_Expression(parser);
                Node_Vector_Push(&alt->when_clause.choices, rng);
            }
            else {
                Node_Vector_Push(&alt->when_clause.choices, choice);
            }
        } while (Parser_Match(parser, TK_BAR));

        Parser_Expect(parser, TK_ARROW);

        Node_Vector_Init(&alt->when_clause.stmts);
        while (!Parser_Check(parser, TK_WHEN) && !Parser_Check(parser, TK_END)) {
            Node_Vector_Push(&alt->when_clause.stmts, Parse_Statement(parser));
        }

        Node_Vector_Push(&node->case_stmt.alternatives, alt);
    }

    Parser_Expect(parser, TK_END);
    Parser_Expect(parser, TK_CASE);
    Parser_Expect(parser, TK_SEMICOLON);

    return node;
}


///-----------------------------------------------------------------------------
///                   L O O P   S T A T E M E N T
///-----------------------------------------------------------------------------

static AST_Node *Parse_Loop_Statement(Parser_State *parser, String_Slice label)
{
    Source_Location loc = Parser_Location(parser);

    AST_Node *node = AST_New(N_LP, loc);
    node->loop_stmt.label = label;
    node->loop_stmt.iteration = NULL;
    node->loop_stmt.is_reverse = false;

    /// WHILE condition
    if (Parser_Match(parser, TK_WHILE)) {
        node->loop_stmt.iteration = Parse_Expression(parser);
    }
    /// FOR loop
    else if (Parser_Match(parser, TK_FOR)) {
        String_Slice var = Parser_Identifier(parser);
        Parser_Expect(parser, TK_IN);

        node->loop_stmt.is_reverse = Parser_Match(parser, TK_REVERSE);

        AST_Node *range = Parse_Range(parser);

        /// Check for explicit RANGE attribute
        if (Parser_Match(parser, TK_RANGE)) {
            AST_Node *rng = AST_New(N_RN, loc);
            rng->range.low_bound = Parse_Simple_Expression(parser);
            Parser_Expect(parser, TK_DOUBLE_DOT);
            rng->range.high_bound = Parse_Simple_Expression(parser);
            range = rng;
        }

        /// Build iteration scheme: var IN range
        AST_Node *iter = AST_New(N_BIN, loc);
        iter->binary.op = TK_IN;
        iter->binary.left = AST_New(N_ID, loc);
        iter->binary.left->string_val = var;
        iter->binary.right = range;
        node->loop_stmt.iteration = iter;
    }

    Parser_Expect(parser, TK_LOOP);

    Node_Vector_Init(&node->loop_stmt.stmts);
    while (!Parser_Check(parser, TK_END)) {
        Node_Vector_Push(&node->loop_stmt.stmts, Parse_Statement(parser));
    }

    Parser_Expect(parser, TK_END);
    Parser_Expect(parser, TK_LOOP);
    if (Parser_Check(parser, TK_IDENTIFIER)) {
        Parser_Advance(parser);
    }
    Parser_Expect(parser, TK_SEMICOLON);

    return node;
}


///-----------------------------------------------------------------------------
///                   B L O C K   S T A T E M E N T
///-----------------------------------------------------------------------------

static AST_Node *Parse_Block_Statement(Parser_State *parser, String_Slice label)
{
    Source_Location loc = Parser_Location(parser);

    AST_Node *node = AST_New(N_BL, loc);
    node->block_stmt.label = label;
    Node_Vector_Init(&node->block_stmt.decls);

    if (Parser_Match(parser, TK_DECLARE)) {
        node->block_stmt.decls = Parse_Declarative_Part(parser);
    }

    Parser_Expect(parser, TK_BEGIN);

    Node_Vector_Init(&node->block_stmt.stmts);
    while (!Parser_Check(parser, TK_EXCEPTION) && !Parser_Check(parser, TK_END)) {
        Node_Vector_Push(&node->block_stmt.stmts, Parse_Statement(parser));
    }

    Node_Vector_Init(&node->block_stmt.handlers);
    if (Parser_Match(parser, TK_EXCEPTION)) {
        node->block_stmt.handlers = Parse_Exception_Handlers(parser);
    }

    Parser_Expect(parser, TK_END);
    if (Parser_Check(parser, TK_IDENTIFIER)) {
        Parser_Advance(parser);
    }
    Parser_Expect(parser, TK_SEMICOLON);

    return node;
}


///-----------------------------------------------------------------------------
///                   S E L E C T   S T A T E M E N T
///-----------------------------------------------------------------------------

static AST_Node *Parse_Select_Statement(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_SELECT);

    AST_Node *node = AST_New(N_SLS, loc);
    node->select_stmt.select_kind = 0;
    node->select_stmt.guard = NULL;
    Node_Vector_Init(&node->select_stmt.alternatives);

    /// Timed entry call: DELAY ... THEN
    if (Parser_Match(parser, TK_DELAY)) {
        node->select_stmt.select_kind = 1;
        node->select_stmt.guard = Parse_Expression(parser);
        Parser_Expect(parser, TK_THEN);

        if (Parser_Match(parser, TK_ABORT)) {
            node->select_stmt.select_kind = 3;
        }

        while (!Parser_Check(parser, TK_OR) &&
               !Parser_Check(parser, TK_ELSE) &&
               !Parser_Check(parser, TK_END)) {
            Node_Vector_Push(&node->select_stmt.alternatives, Parse_Statement(parser));
        }
    }
    /// Selective accept
    else {
        while (Parser_Check(parser, TK_WHEN) || Parser_Check(parser, TK_ACCEPT) ||
               Parser_Check(parser, TK_DELAY) || Parser_Check(parser, TK_TERMINATE)) {

            AST_Node *alt = AST_New(N_SA, loc);
            Node_Vector_Init(&alt->select_alt.choices);
            Node_Vector_Init(&alt->select_alt.stmts);

            if (Parser_Match(parser, TK_WHEN)) {
                do {
                    Node_Vector_Push(&alt->select_alt.choices, Parse_Expression(parser));
                } while (Parser_Match(parser, TK_BAR));
                Parser_Expect(parser, TK_ARROW);
            }

            if (Parser_Match(parser, TK_ACCEPT)) {
                alt->kind = N_ACC;
                alt->accept_stmt.name = Parser_Identifier(parser);
                Node_Vector_Init(&alt->accept_stmt.indices);
                alt->accept_stmt.params = Parse_Parameter_List(parser);
                Node_Vector_Init(&alt->accept_stmt.stmts);

                if (Parser_Match(parser, TK_DO)) {
                    while (!Parser_Check(parser, TK_END) &&
                           !Parser_Check(parser, TK_OR) &&
                           !Parser_Check(parser, TK_ELSE)) {
                        Node_Vector_Push(&alt->accept_stmt.stmts, Parse_Statement(parser));
                    }
                    Parser_Expect(parser, TK_END);
                    if (Parser_Check(parser, TK_IDENTIFIER)) {
                        Parser_Advance(parser);
                    }
                }

                while (!Parser_Check(parser, TK_OR) &&
                       !Parser_Check(parser, TK_ELSE) &&
                       !Parser_Check(parser, TK_END) &&
                       !Parser_Check(parser, TK_WHEN)) {
                    Node_Vector_Push(&alt->select_alt.stmts, Parse_Statement(parser));
                }
            }
            else if (Parser_Match(parser, TK_TERMINATE)) {
                alt->kind = N_TRM;
                Parser_Expect(parser, TK_SEMICOLON);
            }
            else if (Parser_Match(parser, TK_DELAY)) {
                alt->kind = N_DL;
                alt->delay_stmt.duration = Parse_Expression(parser);
                Parser_Expect(parser, TK_SEMICOLON);

                while (!Parser_Check(parser, TK_OR) &&
                       !Parser_Check(parser, TK_ELSE) &&
                       !Parser_Check(parser, TK_END)) {
                    Node_Vector_Push(&alt->select_alt.stmts, Parse_Statement(parser));
                }
            }

            Node_Vector_Push(&node->select_stmt.alternatives, alt);

            if (!Parser_Match(parser, TK_OR)) {
                break;
            }
        }
    }

    Node_Vector_Init(&node->select_stmt.alternatives);
    if (Parser_Match(parser, TK_ELSE)) {
        while (!Parser_Check(parser, TK_END)) {
            Node_Vector_Push(&node->select_stmt.alternatives, Parse_Statement(parser));
        }
    }

    Parser_Expect(parser, TK_END);
    Parser_Expect(parser, TK_SELECT);
    Parser_Expect(parser, TK_SEMICOLON);

    return node;
}


///-----------------------------------------------------------------------------
///                   E X C E P T I O N   H A N D L E R S
///-----------------------------------------------------------------------------

Node_Vector Parse_Exception_Handlers(Parser_State *parser)
{
    Node_Vector handlers;
    Node_Vector_Init(&handlers);

    while (Parser_Match(parser, TK_WHEN)) {
        Source_Location loc = Parser_Location(parser);
        AST_Node *handler = AST_New(N_HD, loc);
        Node_Vector_Init(&handler->handler.exceptions);

        do {
            if (Parser_Match(parser, TK_OTHERS)) {
                AST_Node *id = AST_New(N_ID, loc);
                id->string_val = STR("others");
                Node_Vector_Push(&handler->handler.exceptions, id);
            }
            else {
                Node_Vector_Push(&handler->handler.exceptions, Parse_Name(parser));
            }
        } while (Parser_Match(parser, TK_BAR));

        Parser_Expect(parser, TK_ARROW);

        Node_Vector_Init(&handler->handler.stmts);
        while (!Parser_Check(parser, TK_WHEN) && !Parser_Check(parser, TK_END)) {
            Node_Vector_Push(&handler->handler.stmts, Parse_Statement(parser));
        }

        Node_Vector_Push(&handlers, handler);
    }

    return handlers;
}


///-----------------------------------------------------------------------------
///                   D E C L A R A T I O N   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM Chapter 3: basic_declaration ::= type_declaration | subtype_declaration |
///      object_declaration | number_declaration | subprog_bodyaration |
///      package_declaration | task_declaration | generic_declaration | ...
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Declaration(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    /// Generic declaration
    if (Parser_Check(parser, TK_GENERIC)) {
        return Parse_Generic(parser);
    }

    /// Type declaration
    if (Parser_Match(parser, TK_TYPE)) {
        String_Slice name = Parser_Identifier(parser);

        AST_Node *node = AST_New(N_TD, loc);
        node->type_decl.name = name;
        node->type_decl.definition = NULL;
        Node_Vector_Init(&node->type_decl.disc_list);

        /// Optional discriminant part
        if (Parser_Match(parser, TK_LEFT_PAREN)) {
            do {
                Node_Vector ids;
                Node_Vector_Init(&ids);
                do {
                    String_Slice dname = Parser_Identifier(parser);
                    AST_Node *id = AST_New(N_ID, loc);
                    id->string_val = dname;
                    Node_Vector_Push(&ids, id);
                } while (Parser_Match(parser, TK_COMMA));

                Parser_Expect(parser, TK_COLON);
                AST_Node *dtype = Parse_Name(parser);

                AST_Node *ddefault = NULL;
                if (Parser_Match(parser, TK_ASSIGN)) {
                    ddefault = Parse_Expression(parser);
                }

                for (uint32_t i = 0; i < ids.count; i++) {
                    AST_Node *disc = AST_New(N_DS, loc);
                    disc->param.param_name = ids.data[i]->string_val;
                    disc->param.param_type = dtype;
                    disc->param.default_value = ddefault;
                    Node_Vector_Push(&node->type_decl.disc_list, disc);
                }
            } while (Parser_Match(parser, TK_SEMICOLON));
            Parser_Expect(parser, TK_RIGHT_PAREN);
        }

        /// Type completion
        if (Parser_Match(parser, TK_IS)) {
            node->type_decl.is_new = Parser_Match(parser, TK_NEW);
            node->type_decl.is_derived = node->type_decl.is_new;

            if (node->type_decl.is_derived) {
                node->type_decl.parent_type = Parse_Name(parser);
                node->type_decl.definition = node->type_decl.parent_type;

                /// Optional constraint on derived type
                if (Parser_Match(parser, TK_DIGITS)) {
                    Parse_Expression(parser);
                    if (Parser_Match(parser, TK_RANGE)) {
                        Parse_Simple_Expression(parser);
                        Parser_Expect(parser, TK_DOUBLE_DOT);
                        Parse_Simple_Expression(parser);
                    }
                }
                else if (Parser_Match(parser, TK_DELTA)) {
                    Parse_Expression(parser);
                    Parser_Expect(parser, TK_RANGE);
                    Parse_Simple_Expression(parser);
                    Parser_Expect(parser, TK_DOUBLE_DOT);
                    Parse_Simple_Expression(parser);
                }
                else if (Parser_Match(parser, TK_RANGE)) {
                    AST_Node *rng = AST_New(N_RN, loc);
                    rng->range.low_bound = Parse_Simple_Expression(parser);
                    Parser_Expect(parser, TK_DOUBLE_DOT);
                    rng->range.high_bound = Parse_Simple_Expression(parser);
                    node->type_decl.definition = rng;
                }
            }
            else {
                node->type_decl.definition = Parse_Type_Definition(parser);
            }
        }

        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// Subtype declaration
    if (Parser_Match(parser, TK_SUBTYPE)) {
        String_Slice name = Parser_Identifier(parser);
        Parser_Expect(parser, TK_IS);

        AST_Node *node = AST_New(N_SD, loc);
        node->subtype_decl.name = name;
        node->subtype_decl.indication = Parse_Subtype_Indication(parser);

        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// Procedure declaration/body
    if (Parser_Check(parser, TK_PROCEDURE)) {
        return Parse_Procedure(parser);
    }

    /// Function declaration/body
    if (Parser_Check(parser, TK_FUNCTION)) {
        return Parse_Function(parser);
    }

    /// Package declaration/body
    if (Parser_Check(parser, TK_PACKAGE)) {
        return Parse_Package(parser);
    }

    /// Task declaration/body
    if (Parser_Check(parser, TK_TASK)) {
        return Parse_Task(parser);
    }

    /// USE clause
    if (Parser_Match(parser, TK_USE)) {
        Node_Vector names;
        Node_Vector_Init(&names);
        do {
            Node_Vector_Push(&names, Parse_Name(parser));
        } while (Parser_Match(parser, TK_COMMA));
        Parser_Expect(parser, TK_SEMICOLON);

        if (names.count == 1) {
            AST_Node *node = AST_New(N_US, loc);
            node->use_clause.package_name = names.data[0];
            return node;
        }

        /// Multiple USE - create list
        AST_Node *list = AST_New(N_LST, loc);
        Node_Vector_Init(&list->list.items);
        for (uint32_t i = 0; i < names.count; i++) {
            AST_Node *use = AST_New(N_US, loc);
            use->use_clause.package_name = names.data[i];
            Node_Vector_Push(&list->list.items, use);
        }
        return list;
    }

    /// PRAGMA
    if (Parser_Match(parser, TK_PRAGMA)) {
        AST_Node *node = AST_New(N_PG, loc);
        node->pragma_node.name = Parser_Identifier(parser);
        Node_Vector_Init(&node->pragma_node.args);
        if (Parser_Match(parser, TK_LEFT_PAREN)) {
            do {
                Node_Vector_Push(&node->pragma_node.args, Parse_Expression(parser));
            } while (Parser_Match(parser, TK_COMMA));
            Parser_Expect(parser, TK_RIGHT_PAREN);
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// Object/exception declaration: identifier_list : [CONSTANT] ...
    Node_Vector ids;
    Node_Vector_Init(&ids);
    do {
        String_Slice name = Parser_Identifier(parser);
        AST_Node *id = AST_New(N_ID, loc);
        id->string_val = name;
        Node_Vector_Push(&ids, id);
    } while (Parser_Match(parser, TK_COMMA));

    Parser_Expect(parser, TK_COLON);

    bool is_constant = Parser_Match(parser, TK_CONSTANT);

    /// Exception declaration
    if (Parser_Match(parser, TK_EXCEPTION)) {
        AST_Node *node = AST_New(N_ED, loc);
        node->exception_decl.names = ids;
        node->exception_decl.renaming = NULL;
        if (Parser_Match(parser, TK_RENAMES)) {
            node->exception_decl.renaming = Parse_Expression(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);
        return node;
    }

    /// Object type
    AST_Node *type = NULL;
    if (!Parser_Check(parser, TK_ASSIGN)) {
        if (Parser_Check(parser, TK_ARRAY) || Parser_Check(parser, TK_ACCESS)) {
            type = Parse_Type_Definition(parser);
        }
        else {
            type = Parse_Subtype_Indication(parser);
        }
    }

    /// Initial value or rename
    AST_Node *init = NULL;
    if (Parser_Match(parser, TK_RENAMES)) {
        init = Parse_Expression(parser);
    }
    else if (Parser_Match(parser, TK_ASSIGN)) {
        init = Parse_Expression(parser);
    }

    Parser_Expect(parser, TK_SEMICOLON);

    AST_Node *node = AST_New(N_OD, loc);
    node->object_decl.names = ids;
    node->object_decl.object_type = type;
    node->object_decl.init_value = init;
    node->object_decl.is_constant = is_constant;

    return node;
}


///-----------------------------------------------------------------------------
///                   D E C L A R A T I V E   P A R T
///-----------------------------------------------------------------------------

Node_Vector Parse_Declarative_Part(Parser_State *parser)
{
    Node_Vector decls;
    Node_Vector_Init(&decls);

    while (!Parser_Check(parser, TK_BEGIN) &&
           !Parser_Check(parser, TK_END) &&
           !Parser_Check(parser, TK_PRIVATE) &&
           !Parser_Check(parser, TK_EOF) &&
           !Parser_Check(parser, TK_ENTRY)) {

        /// Representation clause: FOR ... USE
        if (Parser_Check(parser, TK_FOR)) {
            Parse_Representation_Clause(parser);
            continue;
        }

        /// PRAGMA
        if (Parser_Check(parser, TK_PRAGMA)) {
            AST_Node *pragma = Parse_Declaration(parser);
            Node_Vector_Push(&decls, pragma);
            continue;
        }

        Node_Vector_Push(&decls, Parse_Declaration(parser));
    }

    return decls;
}


///-----------------------------------------------------------------------------
///                   P R O C E D U R E   P A R S I N G
///-----------------------------------------------------------------------------

static AST_Node *Parse_Procedure(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_PROCEDURE);

    AST_Node *spec = AST_New(N_PS, loc);

    /// Name can be identifier or operator string
    if (Parser_Check(parser, TK_STRING)) {
        spec->subprog_spec.name = String_Dup(parser->current.literal);
        Parser_Advance(parser);
    }
    else {
        spec->subprog_spec.name = Parser_Identifier(parser);
    }

    spec->subprog_spec.params = Parse_Parameter_List(parser);
    spec->subprog_spec.return_type = NULL;

    /// RENAMES
    if (Parser_Match(parser, TK_RENAMES)) {
        Parse_Expression(parser);
        Parser_Expect(parser, TK_SEMICOLON);
        AST_Node *node = AST_New(N_PD, loc);
        node->subprog_body.spec = spec;
        return node;
    }

    /// IS ...
    if (Parser_Match(parser, TK_IS)) {
        /// SEPARATE
        if (Parser_Match(parser, TK_SEPARATE)) {
            Parser_Expect(parser, TK_SEMICOLON);
            AST_Node *node = AST_New(N_PD, loc);
            node->subprog_body.spec = spec;
            return node;
        }

        /// Generic instantiation: IS NEW generic_name
        if (Parser_Match(parser, TK_NEW)) {
            String_Slice generic_name = Parser_Identifier(parser);
            Node_Vector actuals;
            Node_Vector_Init(&actuals);

            if (Parser_Match(parser, TK_LEFT_PAREN)) {
                do {
                    AST_Node *arg = Parse_Expression(parser);
                    if (arg->kind == N_ID && Parser_Match(parser, TK_ARROW)) {
                        AST_Node *assoc = AST_New(N_ASC, loc);
                        Node_Vector_Init(&assoc->association.choices);
                        Node_Vector_Push(&assoc->association.choices, arg);
                        assoc->association.value = Parse_Expression(parser);
                        Node_Vector_Push(&actuals, assoc);
                    }
                    else {
                        Node_Vector_Push(&actuals, arg);
                    }
                } while (Parser_Match(parser, TK_COMMA));
                Parser_Expect(parser, TK_RIGHT_PAREN);
            }
            Parser_Expect(parser, TK_SEMICOLON);

            AST_Node *node = AST_New(N_GINST, loc);
            node->generic_inst.name = spec->subprog_spec.name;
            node->generic_inst.generic_name = generic_name;
            node->generic_inst.actual_params = actuals;
            return node;
        }

        /// Procedure body
        AST_Node *node = AST_New(N_PB, loc);
        node->subprog_body.spec = spec;
        node->subprog_body.decls = Parse_Declarative_Part(parser);

        Parser_Expect(parser, TK_BEGIN);
        node->subprog_body.stmts = Parse_Statement_Sequence(parser);

        Node_Vector_Init(&node->subprog_body.handlers);
        if (Parser_Match(parser, TK_EXCEPTION)) {
            node->subprog_body.handlers = Parse_Exception_Handlers(parser);
        }

        Parser_Expect(parser, TK_END);
        if (Parser_Check(parser, TK_IDENTIFIER) || Parser_Check(parser, TK_STRING)) {
            Parser_Advance(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);

        return node;
    }

    /// Declaration only
    Parser_Expect(parser, TK_SEMICOLON);
    AST_Node *node = AST_New(N_PD, loc);
    node->subprog_body.spec = spec;
    return node;
}


///-----------------------------------------------------------------------------
///                   F U N C T I O N   P A R S I N G
///-----------------------------------------------------------------------------

static AST_Node *Parse_Function(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_FUNCTION);

    String_Slice name;
    if (Parser_Check(parser, TK_STRING)) {
        name = String_Dup(parser->current.literal);
        Parser_Advance(parser);
    }
    else {
        name = Parser_Identifier(parser);
    }

    /// Generic instantiation before parameters
    if (Parser_Match(parser, TK_IS) && Parser_Match(parser, TK_NEW)) {
        String_Slice generic_name = Parser_Identifier(parser);
        Node_Vector actuals;
        Node_Vector_Init(&actuals);

        if (Parser_Match(parser, TK_LEFT_PAREN)) {
            do {
                AST_Node *arg = Parse_Expression(parser);
                Node_Vector_Push(&actuals, arg);
            } while (Parser_Match(parser, TK_COMMA));
            Parser_Expect(parser, TK_RIGHT_PAREN);
        }
        Parser_Expect(parser, TK_SEMICOLON);

        AST_Node *node = AST_New(N_GINST, loc);
        node->generic_inst.name = name;
        node->generic_inst.generic_name = generic_name;
        node->generic_inst.actual_params = actuals;
        return node;
    }

    AST_Node *spec = AST_New(N_FS, loc);
    spec->subprog_spec.name = name;
    spec->subprog_spec.params = Parse_Parameter_List(parser);

    Parser_Expect(parser, TK_RETURN);
    spec->subprog_spec.return_type = Parse_Name(parser);

    /// RENAMES
    if (Parser_Match(parser, TK_RENAMES)) {
        Parse_Expression(parser);
        Parser_Expect(parser, TK_SEMICOLON);
        AST_Node *node = AST_New(N_FD, loc);
        node->subprog_body.spec = spec;
        return node;
    }

    /// IS body
    if (Parser_Match(parser, TK_IS)) {
        if (Parser_Match(parser, TK_SEPARATE)) {
            Parser_Expect(parser, TK_SEMICOLON);
            AST_Node *node = AST_New(N_FD, loc);
            node->subprog_body.spec = spec;
            return node;
        }

        AST_Node *node = AST_New(N_FB, loc);
        node->subprog_body.spec = spec;
        node->subprog_body.decls = Parse_Declarative_Part(parser);

        Parser_Expect(parser, TK_BEGIN);
        node->subprog_body.stmts = Parse_Statement_Sequence(parser);

        Node_Vector_Init(&node->subprog_body.handlers);
        if (Parser_Match(parser, TK_EXCEPTION)) {
            node->subprog_body.handlers = Parse_Exception_Handlers(parser);
        }

        Parser_Expect(parser, TK_END);
        if (Parser_Check(parser, TK_IDENTIFIER) || Parser_Check(parser, TK_STRING)) {
            Parser_Advance(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);

        return node;
    }

    /// Declaration only
    Parser_Expect(parser, TK_SEMICOLON);
    AST_Node *node = AST_New(N_FD, loc);
    node->subprog_body.spec = spec;
    return node;
}


///-----------------------------------------------------------------------------
///                   P A C K A G E   P A R S I N G
///-----------------------------------------------------------------------------

static AST_Node *Parse_Package(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_PACKAGE);

    /// Package body
    if (Parser_Match(parser, TK_BODY)) {
        String_Slice name = Parser_Identifier(parser);
        Parser_Expect(parser, TK_IS);

        if (Parser_Match(parser, TK_SEPARATE)) {
            Parser_Expect(parser, TK_SEMICOLON);
            AST_Node *node = AST_New(N_PKB, loc);
            node->package_body.name = name;
            Node_Vector_Init(&node->package_body.decls);
            Node_Vector_Init(&node->package_body.stmts);
            Node_Vector_Init(&node->package_body.handlers);
            return node;
        }

        AST_Node *node = AST_New(N_PKB, loc);
        node->package_body.name = name;
        node->package_body.decls = Parse_Declarative_Part(parser);

        Node_Vector_Init(&node->package_body.stmts);
        if (Parser_Match(parser, TK_BEGIN)) {
            node->package_body.stmts = Parse_Statement_Sequence(parser);

            Node_Vector_Init(&node->package_body.handlers);
            if (Parser_Match(parser, TK_EXCEPTION)) {
                node->package_body.handlers = Parse_Exception_Handlers(parser);
            }
        }

        Parser_Expect(parser, TK_END);
        if (Parser_Check(parser, TK_IDENTIFIER)) {
            Parser_Advance(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);

        return node;
    }

    /// Package spec
    String_Slice name = Parser_Identifier(parser);

    /// Renames
    if (Parser_Match(parser, TK_RENAMES)) {
        AST_Node *rename = Parse_Expression(parser);
        Parser_Expect(parser, TK_SEMICOLON);
        AST_Node *node = AST_New(N_RE, loc);
        node->renaming.name = name;
        node->renaming.renamed = rename;
        return node;
    }

    Parser_Expect(parser, TK_IS);

    /// Generic instantiation
    if (Parser_Match(parser, TK_NEW)) {
        String_Slice generic_name = Parser_Identifier(parser);
        Node_Vector actuals;
        Node_Vector_Init(&actuals);

        if (Parser_Match(parser, TK_LEFT_PAREN)) {
            do {
                AST_Node *arg = Parse_Expression(parser);
                if (arg->kind == N_ID && Parser_Match(parser, TK_ARROW)) {
                    AST_Node *assoc = AST_New(N_ASC, loc);
                    Node_Vector_Init(&assoc->association.choices);
                    Node_Vector_Push(&assoc->association.choices, arg);
                    assoc->association.value = Parse_Expression(parser);
                    Node_Vector_Push(&actuals, assoc);
                }
                else {
                    Node_Vector_Push(&actuals, arg);
                }
            } while (Parser_Match(parser, TK_COMMA));
            Parser_Expect(parser, TK_RIGHT_PAREN);
        }
        Parser_Expect(parser, TK_SEMICOLON);

        AST_Node *node = AST_New(N_GINST, loc);
        node->generic_inst.name = name;
        node->generic_inst.generic_name = generic_name;
        node->generic_inst.actual_params = actuals;
        return node;
    }

    /// Package spec
    AST_Node *node = AST_New(N_PKS, loc);
    node->package_spec.name = name;
    node->package_spec.visible_decls = Parse_Declarative_Part(parser);

    Node_Vector_Init(&node->package_spec.private_decls);
    if (Parser_Match(parser, TK_PRIVATE)) {
        node->package_spec.private_decls = Parse_Declarative_Part(parser);
    }

    Parser_Expect(parser, TK_END);
    if (Parser_Check(parser, TK_IDENTIFIER)) {
        Parser_Advance(parser);
    }
    Parser_Expect(parser, TK_SEMICOLON);

    return node;
}


///-----------------------------------------------------------------------------
///                   T A S K   P A R S I N G
///-----------------------------------------------------------------------------

static AST_Node *Parse_Task(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_TASK);

    /// Task body
    if (Parser_Match(parser, TK_BODY)) {
        String_Slice name = Parser_Identifier(parser);
        Parser_Expect(parser, TK_IS);

        if (Parser_Match(parser, TK_SEPARATE)) {
            Parser_Expect(parser, TK_SEMICOLON);
            AST_Node *node = AST_New(N_TKB, loc);
            node->task_body.name = name;
            return node;
        }

        AST_Node *node = AST_New(N_TKB, loc);
        node->task_body.name = name;
        node->task_body.decls = Parse_Declarative_Part(parser);

        Parser_Expect(parser, TK_BEGIN);
        node->task_body.stmts = Parse_Statement_Sequence(parser);

        Node_Vector_Init(&node->task_body.handlers);
        if (Parser_Match(parser, TK_EXCEPTION)) {
            node->task_body.handlers = Parse_Exception_Handlers(parser);
        }

        Parser_Expect(parser, TK_END);
        if (Parser_Check(parser, TK_IDENTIFIER)) {
            Parser_Advance(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);

        return node;
    }

    /// Task type or single task
    bool is_type = Parser_Match(parser, TK_TYPE);
    String_Slice name = Parser_Identifier(parser);

    AST_Node *node = AST_New(N_TKS, loc);
    node->task_spec.name = name;
    node->task_spec.is_type = is_type;
    Node_Vector_Init(&node->task_spec.entries);

    if (Parser_Match(parser, TK_IS)) {
        while (!Parser_Check(parser, TK_END)) {
            if (Parser_Match(parser, TK_ENTRY)) {
                AST_Node *entry = AST_New(N_ENT, loc);
                entry->entry_decl.name = Parser_Identifier(parser);
                Node_Vector_Init(&entry->entry_decl.family_index);
                entry->entry_decl.params = Parse_Parameter_List(parser);
                Parser_Expect(parser, TK_SEMICOLON);
                Node_Vector_Push(&node->task_spec.entries, entry);
            }
            else if (Parser_Match(parser, TK_PRAGMA)) {
                Parser_Identifier(parser);
                if (Parser_Match(parser, TK_LEFT_PAREN)) {
                    do {
                        Parse_Expression(parser);
                    } while (Parser_Match(parser, TK_COMMA));
                    Parser_Expect(parser, TK_RIGHT_PAREN);
                }
                Parser_Expect(parser, TK_SEMICOLON);
            }
            else {
                break;
            }
        }
        Parser_Expect(parser, TK_END);
        if (Parser_Check(parser, TK_IDENTIFIER)) {
            Parser_Advance(parser);
        }
    }

    Parser_Expect(parser, TK_SEMICOLON);
    return node;
}


///-----------------------------------------------------------------------------
///                   G E N E R I C   P A R S I N G
///-----------------------------------------------------------------------------

AST_Node *Parse_Generic(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);
    Parser_Expect(parser, TK_GENERIC);

    AST_Node *node = AST_New(N_GEN, loc);
    Node_Vector_Init(&node->generic_decl.formal_params);
    Node_Vector_Init(&node->generic_decl.decls);

    /// Parse formal parameters
    while (!Parser_Check(parser, TK_PROCEDURE) &&
           !Parser_Check(parser, TK_FUNCTION) &&
           !Parser_Check(parser, TK_PACKAGE)) {

        if (Parser_Match(parser, TK_TYPE)) {
            /// Generic type parameter
            String_Slice name = Parser_Identifier(parser);

            /// Optional discriminant constraint
            if (Parser_Match(parser, TK_LEFT_PAREN)) {
                while (!Parser_Check(parser, TK_RIGHT_PAREN)) {
                    Parser_Advance(parser);
                }
                Parser_Expect(parser, TK_RIGHT_PAREN);
            }

            if (Parser_Match(parser, TK_IS)) {
                if (Parser_Match(parser, TK_DIGITS) ||
                    Parser_Match(parser, TK_DELTA) ||
                    Parser_Match(parser, TK_RANGE)) {
                    Parser_Expect(parser, TK_BOX);
                }
                else if (Parser_Match(parser, TK_LEFT_PAREN)) {
                    Parser_Expect(parser, TK_BOX);
                    Parser_Expect(parser, TK_RIGHT_PAREN);
                }
                else if (Parser_Check(parser, TK_LIMITED) ||
                         Parser_Check(parser, TK_ARRAY) ||
                         Parser_Check(parser, TK_RECORD) ||
                         Parser_Check(parser, TK_ACCESS) ||
                         Parser_Check(parser, TK_PRIVATE)) {
                    Parse_Type_Definition(parser);
                }
                else {
                    Parse_Expression(parser);
                }
            }

            AST_Node *formal = AST_New(N_GTP, loc);
            formal->type_decl.name = name;
            Node_Vector_Push(&node->generic_decl.formal_params, formal);
            Parser_Expect(parser, TK_SEMICOLON);
        }
        else if (Parser_Match(parser, TK_WITH)) {
            /// Generic subprogram or package formal
            if (Parser_Check(parser, TK_PROCEDURE)) {
                AST_Node *sp = Parse_Procedure(parser);
                sp->kind = N_GSP;
                if (Parser_Match(parser, TK_IS)) {
                    if (!Parser_Match(parser, TK_BOX)) {
                        while (!Parser_Check(parser, TK_SEMICOLON)) {
                            Parser_Advance(parser);
                        }
                    }
                }
                Node_Vector_Push(&node->generic_decl.formal_params, sp);
            }
            else if (Parser_Check(parser, TK_FUNCTION)) {
                AST_Node *sp = Parse_Function(parser);
                sp->kind = N_GSP;
                if (Parser_Match(parser, TK_IS)) {
                    if (!Parser_Match(parser, TK_BOX)) {
                        while (!Parser_Check(parser, TK_SEMICOLON)) {
                            Parser_Advance(parser);
                        }
                    }
                }
                Node_Vector_Push(&node->generic_decl.formal_params, sp);
            }
            else {
                /// Generic formal object
                Node_Vector ids;
                Node_Vector_Init(&ids);
                do {
                    String_Slice name = Parser_Identifier(parser);
                    AST_Node *id = AST_New(N_ID, loc);
                    id->string_val = name;
                    Node_Vector_Push(&ids, id);
                } while (Parser_Match(parser, TK_COMMA));

                Parser_Expect(parser, TK_COLON);

                uint8_t mode = 0;
                if (Parser_Match(parser, TK_IN)) mode |= 1;
                if (Parser_Match(parser, TK_OUT)) mode |= 2;
                if (!mode) mode = 1;

                AST_Node *type = Parse_Name(parser);
                Parser_Match(parser, TK_ASSIGN);
                if (!Parser_Check(parser, TK_SEMICOLON)) {
                    Parse_Expression(parser);
                }

                AST_Node *formal = AST_New(N_GVL, loc);
                formal->object_decl.names = ids;
                formal->object_decl.object_type = type;
                Node_Vector_Push(&node->generic_decl.formal_params, formal);
                Parser_Expect(parser, TK_SEMICOLON);
            }
        }
        else {
            /// Generic formal object (no WITH)
            Node_Vector ids;
            Node_Vector_Init(&ids);
            do {
                String_Slice name = Parser_Identifier(parser);
                AST_Node *id = AST_New(N_ID, loc);
                id->string_val = name;
                Node_Vector_Push(&ids, id);
            } while (Parser_Match(parser, TK_COMMA));

            Parser_Expect(parser, TK_COLON);

            uint8_t mode = 0;
            if (Parser_Match(parser, TK_IN)) mode |= 1;
            if (Parser_Match(parser, TK_OUT)) mode |= 2;
            if (!mode) mode = 1;

            AST_Node *type = Parse_Name(parser);
            Parser_Match(parser, TK_ASSIGN);
            if (!Parser_Check(parser, TK_SEMICOLON)) {
                Parse_Expression(parser);
            }

            AST_Node *formal = AST_New(N_GVL, loc);
            formal->object_decl.names = ids;
            formal->object_decl.object_type = type;
            Node_Vector_Push(&node->generic_decl.formal_params, formal);
            Parser_Expect(parser, TK_SEMICOLON);
        }
    }

    /// Parse the generic unit
    if (Parser_Check(parser, TK_PROCEDURE)) {
        AST_Node *sp = Parse_Procedure(parser);
        node->generic_decl.unit = AST_New(N_PD, loc);
        node->generic_decl.unit->subprog_body.spec = sp->subprog_body.spec;
        return node;
    }

    if (Parser_Check(parser, TK_FUNCTION)) {
        AST_Node *sp = Parse_Function(parser);
        node->generic_decl.unit = AST_New(N_FD, loc);
        node->generic_decl.unit->subprog_body.spec = sp->subprog_body.spec;
        return node;
    }

    if (Parser_Match(parser, TK_PACKAGE)) {
        String_Slice name = Parser_Identifier(parser);
        Parser_Expect(parser, TK_IS);

        Node_Vector decls = Parse_Declarative_Part(parser);
        if (Parser_Match(parser, TK_PRIVATE)) {
            Node_Vector priv = Parse_Declarative_Part(parser);
            for (uint32_t i = 0; i < priv.count; i++) {
                Node_Vector_Push(&decls, priv.data[i]);
            }
        }

        node->generic_decl.decls = decls;

        Parser_Expect(parser, TK_END);
        if (Parser_Check(parser, TK_IDENTIFIER)) {
            Parser_Advance(parser);
        }
        Parser_Expect(parser, TK_SEMICOLON);

        AST_Node *pkg = AST_New(N_PKS, loc);
        pkg->package_spec.name = name;
        pkg->package_spec.visible_decls = node->generic_decl.decls;
        node->generic_decl.unit = pkg;
        return node;
    }

    return node;
}


///-----------------------------------------------------------------------------
///                   R E P R E S E N T A T I O N   C L A U S E
///-----------------------------------------------------------------------------
///
///  LRM Chapter 13: representation_clause ::= type_representation_clause |
///                                            address_clause
///
///  These are pragmatically handled as they affect code generation.
///
///-----------------------------------------------------------------------------

static void Parse_Representation_Clause(Parser_State *parser)
{
    Parser_Expect(parser, TK_FOR);
    Parse_Name(parser);
    Parser_Expect(parser, TK_USE);

    if (Parser_Match(parser, TK_AT)) {
        /// Address clause
        Parse_Expression(parser);
        Parser_Expect(parser, TK_SEMICOLON);
        return;
    }

    if (Parser_Match(parser, TK_RECORD)) {
        /// Record representation clause
        while (!Parser_Check(parser, TK_END)) {
            Parser_Identifier(parser);
            Parser_Expect(parser, TK_AT);
            Parse_Expression(parser);
            Parser_Expect(parser, TK_RANGE);
            Parse_Range(parser);
            Parser_Expect(parser, TK_SEMICOLON);
        }
        Parser_Expect(parser, TK_END);
        Parser_Expect(parser, TK_RECORD);
        Parser_Expect(parser, TK_SEMICOLON);
        return;
    }

    /// Enumeration representation or size clause
    Parse_Expression(parser);
    Parser_Expect(parser, TK_SEMICOLON);
}


///-----------------------------------------------------------------------------
///                   C O N T E X T   C L A U S E   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 10.1.1: context_clause ::= {with_clause | use_clause | pragma}
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Context_Clause(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    AST_Node *ctx = AST_New(N_CX, loc);
    Node_Vector_Init(&ctx->context.with_clauses);
    Node_Vector_Init(&ctx->context.use_clauses);

    while (Parser_Check(parser, TK_WITH) ||
           Parser_Check(parser, TK_USE) ||
           Parser_Check(parser, TK_PRAGMA)) {

        if (Parser_Match(parser, TK_WITH)) {
            do {
                AST_Node *with = AST_New(N_WI, loc);
                with->with_clause.unit_name = Parser_Identifier(parser);
                Node_Vector_Push(&ctx->context.with_clauses, with);
            } while (Parser_Match(parser, TK_COMMA));
            Parser_Expect(parser, TK_SEMICOLON);
        }
        else if (Parser_Match(parser, TK_USE)) {
            do {
                AST_Node *use = AST_New(N_US, loc);
                use->use_clause.package_name = Parse_Name(parser);
                Node_Vector_Push(&ctx->context.use_clauses, use);
            } while (Parser_Match(parser, TK_COMMA));
            Parser_Expect(parser, TK_SEMICOLON);
        }
        else {
            /// PRAGMA
            AST_Node *pragma = Parse_Declaration(parser);
            if (pragma) {
                Node_Vector_Push(&ctx->context.use_clauses, pragma);
            }
        }
    }

    return ctx;
}


///-----------------------------------------------------------------------------
///                   C O M P I L A T I O N   U N I T   P A R S I N G
///-----------------------------------------------------------------------------
///
///  LRM 10.1: compilation_unit ::= context_clause library_unit |
///                                 context_clause secondary_unit
///
///  This is the entry point for parsing a complete Ada source file.
///
///-----------------------------------------------------------------------------

AST_Node *Parse_Compilation_Unit(Parser_State *parser)
{
    Source_Location loc = Parser_Location(parser);

    AST_Node *unit = AST_New(N_CU, loc);
    unit->comp_unit.context = Parse_Context_Clause(parser);
    Node_Vector_Init(&unit->comp_unit.units);

    while (Parser_Check(parser, TK_WITH) ||
           Parser_Check(parser, TK_USE) ||
           Parser_Check(parser, TK_PROCEDURE) ||
           Parser_Check(parser, TK_FUNCTION) ||
           Parser_Check(parser, TK_PACKAGE) ||
           Parser_Check(parser, TK_GENERIC) ||
           Parser_Check(parser, TK_PRAGMA) ||
           Parser_Check(parser, TK_SEPARATE)) {

        /// Additional context clauses
        if (Parser_Check(parser, TK_WITH) ||
            Parser_Check(parser, TK_USE) ||
            Parser_Check(parser, TK_PRAGMA)) {
            AST_Node *ctx = Parse_Context_Clause(parser);
            for (uint32_t i = 0; i < ctx->context.with_clauses.count; i++) {
                Node_Vector_Push(&unit->comp_unit.context->context.with_clauses,
                               ctx->context.with_clauses.data[i]);
            }
            for (uint32_t i = 0; i < ctx->context.use_clauses.count; i++) {
                Node_Vector_Push(&unit->comp_unit.context->context.use_clauses,
                               ctx->context.use_clauses.data[i]);
            }
        }
        /// SEPARATE subunit
        else if (Parser_Match(parser, TK_SEPARATE)) {
            Parser_Expect(parser, TK_LEFT_PAREN);
            AST_Node *parent_name = Parse_Name(parser);
            Parser_Expect(parser, TK_RIGHT_PAREN);

            /// Store parent name for subunit resolution
            (void)parent_name;

            Node_Vector_Push(&unit->comp_unit.units, Parse_Declaration(parser));
        }
        else {
            Node_Vector_Push(&unit->comp_unit.units, Parse_Declaration(parser));
        }
    }

    return unit;
}


///-----------------------------------------------------------------------------
///                                  E N D                                    --
///-----------------------------------------------------------------------------
