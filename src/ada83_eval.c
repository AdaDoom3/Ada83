///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///          S E M A N T I C   A N A L Y S I S   A N D   E V A L U A T I O N  --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  This module implements semantic analysis and evaluation (interpretation)  --
///  of Ada83 programs.                                                        --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_eval.h"
#include "ada83_arena.h"
#include "ada83_bignum.h"
#include "ada83_string.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <setjmp.h>

/// @brief Push a symbol onto a Symbol_Vector
static inline void Symbol_Vector_Push(Symbol_Vector *vec, Symbol_Entry *sym)
{
    if (vec->count >= vec->capacity) {
        uint32_t new_cap = vec->capacity ? vec->capacity * 2 : 8;
        vec->data = realloc(vec->data, new_cap * sizeof(Symbol_Entry *));
        vec->capacity = new_cap;
    }
    vec->data[vec->count++] = sym;
}

/// @brief Scope management stubs
static void Scope_Push(Semantic_Context *sem) { (void)sem; }
static void Scope_Pop(Semantic_Context *sem) { (void)sem; }
static void Symbol_Use_Package(Semantic_Context *sem, Symbol_Entry *pkg, String_Slice name)
{
    (void)sem; (void)pkg; (void)name;
}


///-----------------------------------------------------------------------------
///                   P R E D E F I N E D   T Y P E S
///-----------------------------------------------------------------------------
///
///  These are the predefined types from package STANDARD (LRM Annex C).
///  They are initialized by Eval_Init and used throughout evaluation.
///
///-----------------------------------------------------------------------------

/// Predefined types are declared extern in ada83_types.h:
///   Type_Integer, Type_Natural, Type_Positive, Type_Boolean,
///   Type_Character, Type_String, Type_Float, Type_Universal_Int,
///   Type_Universal_Real


///-----------------------------------------------------------------------------
///                   H E L P E R   F U N C T I O N S
///-----------------------------------------------------------------------------

/// @brief Create a new runtime value of given kind
static Runtime_Value Make_Value(Value_Kind kind, Type_Descriptor *type)
{
    Runtime_Value v = {0};
    v.kind = kind;
    v.type = type;
    return v;
}

/// @brief Create an integer value
static Runtime_Value Make_Integer(int64_t value, Type_Descriptor *type)
{
    Runtime_Value v = Make_Value(VK_INTEGER, type ? type : Type_Integer);
    v.integer = value;
    return v;
}

/// @brief Create a real value
static Runtime_Value Make_Real(double value, Type_Descriptor *type)
{
    Runtime_Value v = Make_Value(VK_REAL, type ? type : Type_Float);
    v.real = value;
    return v;
}

/// @brief Create a boolean value
static Runtime_Value Make_Boolean(bool value)
{
    Runtime_Value v = Make_Value(VK_INTEGER, Type_Boolean);
    v.integer = value ? 1 : 0;
    return v;
}

/// @brief Create a string value
static Runtime_Value Make_String(String_Slice value)
{
    Runtime_Value v = Make_Value(VK_STRING, Type_String);
    v.string = value;
    return v;
}

/// @brief Create a null access value
static Runtime_Value Make_Null(Type_Descriptor *type)
{
    Runtime_Value v = Make_Value(VK_ACCESS, type);
    v.access = NULL;
    return v;
}


///-----------------------------------------------------------------------------
///                   T Y P E   C O M P A T I B I L I T Y
///-----------------------------------------------------------------------------
///
///  Ada83 uses name equivalence for type checking (LRM 4.2). Two types are
///  the same only if they are declared by the same declaration. However,
///  certain implicit conversions are allowed for universal types.
///
///-----------------------------------------------------------------------------

/// @brief Get the base type (for derived types)
static Type_Descriptor *Base_Type(Type_Descriptor *type)
{
    if (!type) return Type_Integer;

    while (type->base_type && type->base_type != type) {
        type = type->base_type;
    }
    return type;
}

/// @brief Check if type is numeric (integer or real)
static bool Is_Numeric(Type_Descriptor *type)
{
    if (!type) return false;
    Type_Kind k = type->kind;
    return k == TY_INTEGER || k == TY_FLOAT || k == TY_FIXED ||
           k == TY_UNIVERSAL_INT || k == TY_UNIVERSAL_REAL;
}

/// @brief Check if type is discrete (integer, enum, or character)
static bool Is_Discrete(Type_Descriptor *type)
{
    if (!type) return false;
    Type_Kind k = Base_Type(type)->kind;
    return k == TY_INTEGER || k == TY_ENUMERATION || k == TY_CHARACTER ||
           k == TY_UNIVERSAL_INT;
}

/// @brief Check if type is an array type
static bool Is_Array(Type_Descriptor *type)
{
    if (!type) return false;
    return Base_Type(type)->kind == TY_ARRAY;
}

/// @brief Check if type is a record type
static bool Is_Record(Type_Descriptor *type)
{
    if (!type) return false;
    return Base_Type(type)->kind == TY_RECORD;
}

/// @brief Check if type is an access type
static bool Is_Access(Type_Descriptor *type)
{
    if (!type) return false;
    return Base_Type(type)->kind == TY_ACCESS;
}

/// @brief Check type compatibility for assignment/comparison
static bool Types_Compatible(Type_Descriptor *target, Type_Descriptor *source)
{
    if (!target || !source) return true;  // Incomplete types

    /// Same type
    if (target == source) return true;

    /// Universal types convert to any numeric
    if (source->kind == TY_UNIVERSAL_INT && Is_Numeric(target))
        return true;
    if (source->kind == TY_UNIVERSAL_REAL &&
        (target->kind == TY_FLOAT || target->kind == TY_FIXED))
        return true;

    /// String literal to any array of Character
    if (target->kind == TY_ARRAY && target->element_type &&
        target->element_type->kind == TY_CHARACTER &&
        source == Type_String)
        return true;

    /// Derived type and parent
    if (target->parent_type == source || source->parent_type == target)
        return true;

    /// Same base type
    if (Base_Type(target) == Base_Type(source))
        return true;

    return false;
}


///-----------------------------------------------------------------------------
///                   C O N T E X T   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

void Eval_Init(Eval_Context *ctx, Semantic_Context *sem)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->sem = sem;

    /// Initialize predefined types (package STANDARD)
    Types_Initialize(sem);

    /// Setup I/O
    ctx->current_input = stdin;
    ctx->current_output = stdout;

    /// Initialize call stack
    ctx->call_capacity = 256;
    ctx->call_stack = calloc(ctx->call_capacity, sizeof(Call_Frame));
    ctx->call_depth = 0;
}


void Eval_Cleanup(Eval_Context *ctx)
{
    if (ctx->call_stack) {
        free(ctx->call_stack);
        ctx->call_stack = NULL;
    }

    if (ctx->globals.bindings) {
        free(ctx->globals.bindings);
        ctx->globals.bindings = NULL;
    }
}


///-----------------------------------------------------------------------------
///                   S U B T Y P E   R E S O L U T I O N
///-----------------------------------------------------------------------------

Type_Descriptor *Resolve_Subtype(Semantic_Context *sem, AST_Node *node)
{
    if (!node) return Type_Integer;

    switch (node->kind) {
    case N_ID: {
        Symbol_Entry *sym = Symbol_Find(sem, node->string_val);
        if (sym && sym->type) return sym->type;
        return Type_Integer;
    }

    case N_SEL: {
        /// Package.Type_Name
        AST_Node *prefix = node->selected.prefix;
        if (prefix->kind == N_ID) {
            Symbol_Entry *pkg = Symbol_Find(sem, prefix->string_val);
            if (pkg && pkg->kind == SK_PACKAGE && pkg->definition) {
                /// Search package declarations for type
                AST_Node *pkg_node = pkg->definition;
                if (pkg_node->kind == N_PKS) {
                    for (uint32_t i = 0; i < pkg_node->package_spec.visible_decls.count; i++) {
                        AST_Node *d = pkg_node->package_spec.visible_decls.data[i];
                        if (d->kind == N_TD &&
                            String_Equal_CI(d->type_decl.name, node->selected.selector)) {
                            if (d->symbol && d->symbol->type)
                                return d->symbol->type;
                        }
                    }
                }
            }
        }
        return Type_Integer;
    }

    case N_ST: {
        Type_Descriptor *base = Resolve_Subtype(sem, node->subtype.type_mark);
        if (node->subtype.constraint) {
            /// Create constrained subtype
            Type_Descriptor *sub = Type_New(base->kind, (String_Slice){NULL, 0});
            sub->base_type = base;
            sub->element_type = base->element_type;
            sub->index_type = base->index_type;

            AST_Node *cn = node->subtype.constraint;
            if (cn && cn->kind == N_RN) {
                AST_Node *rng = cn;
                if (rng->range.low_bound && rng->range.low_bound->kind == N_INT)
                    sub->low_bound = rng->range.low_bound->integer_val;
                if (rng->range.high_bound && rng->range.high_bound->kind == N_INT)
                    sub->high_bound = rng->range.high_bound->integer_val;
            }
            return sub;
        }
        return base;
    }

    case N_TI: {
        Type_Descriptor *t = Type_New(TY_INTEGER, (String_Slice){NULL, 0});
        if (node->range.low_bound && node->range.low_bound->kind == N_INT)
            t->low_bound = node->range.low_bound->integer_val;
        if (node->range.high_bound && node->range.high_bound->kind == N_INT)
            t->high_bound = node->range.high_bound->integer_val;
        return t;
    }

    case N_TF:
        return Type_New(TY_FLOAT, (String_Slice){NULL, 0});

    case N_TX:
        return Type_New(TY_FIXED, (String_Slice){NULL, 0});

    case N_TA: {
        Type_Descriptor *t = Type_New(TY_ARRAY, (String_Slice){NULL, 0});
        t->element_type = Resolve_Subtype(sem, node->array_type.element_type);
        if (node->array_type.indices.count > 0) {
            AST_Node *idx = node->array_type.indices.data[0];
            if (idx->kind == N_RN) {
                if (idx->range.low_bound && idx->range.low_bound->kind == N_INT)
                    t->low_bound = idx->range.low_bound->integer_val;
                if (idx->range.high_bound && idx->range.high_bound->kind == N_INT)
                    t->high_bound = idx->range.high_bound->integer_val;
            }
        }
        return t;
    }

    case N_TR:
        return Type_New(TY_RECORD, (String_Slice){NULL, 0});

    case N_TAC: {
        Type_Descriptor *t = Type_New(TY_ACCESS, (String_Slice){NULL, 0});
        t->element_type = Resolve_Subtype(sem, node->unary.operand);
        return t;
    }

    case N_TE:
        return Type_New(TY_ENUMERATION, (String_Slice){NULL, 0});

    case N_TP:
        return Type_New(TY_PRIVATE, (String_Slice){NULL, 0});

    default:
        return Type_Integer;
    }
}


///-----------------------------------------------------------------------------
///                   E X P R E S S I O N   A N A L Y S I S
///-----------------------------------------------------------------------------

void Analyze_Expression(Semantic_Context *sem, AST_Node *expr,
                       Type_Descriptor *expected)
{
    if (!expr) return;

    switch (expr->kind) {
    case N_ID: {
        /// Special handling for enumeration context
        if (expected && expected->kind == TY_ENUMERATION) {
            for (uint32_t i = 0; i < expected->enum_literals.count; i++) {
                Symbol_Entry *lit = expected->enum_literals.data[i];
                if (String_Equal_CI(lit->name, expr->string_val)) {
                    expr->type = expected;
                    expr->symbol = lit;
                    return;
                }
            }
        }

        Symbol_Entry *sym = Symbol_Find(sem, expr->string_val);
        if (sym) {
            expr->type = sym->type;
            expr->symbol = sym;

            /// Constant folding
            if (sym->kind == SK_ENUMERATION_LITERAL && sym->definition) {
                AST_Node *def = sym->definition;
                if (def->kind == N_INT) {
                    expr->kind = N_INT;
                    expr->integer_val = def->integer_val;
                    expr->type = Type_Universal_Int;
                }
            }
        }
        else if (!String_Equal_CI(expr->string_val, STR("others"))) {
            Fatal_Error(expr->location, "undefined identifier '%.*s'",
                       (int)expr->string_val.length, expr->string_val.data);
        }
        break;
    }

    case N_INT:
        expr->type = Type_Universal_Int;
        break;

    case N_REAL:
        expr->type = Type_Universal_Real;
        break;

    case N_CHAR:
        expr->type = Type_Character;
        break;

    case N_STR:
        expr->type = expected && Is_Array(expected) ? expected : Type_String;
        break;

    case N_NULL:
        expr->type = expected && Is_Access(expected) ? expected : Type_Integer;
        break;

    case N_BIN: {
        Analyze_Expression(sem, expr->binary.left, expected);
        Analyze_Expression(sem, expr->binary.right, expected);

        Token_Kind op = expr->binary.op;

        /// Short-circuit operators
        if (op == TK_AND_THEN || op == TK_OR_ELSE) {
            expr->type = Type_Boolean;
            break;
        }

        /// Logical operators
        if (op == TK_AND || op == TK_OR || op == TK_XOR) {
            Type_Descriptor *lt = expr->binary.left->type;
            if (lt && lt->kind == TY_ARRAY)
                expr->type = lt;
            else
                expr->type = Type_Boolean;
            break;
        }

        /// Membership test
        if (op == TK_IN) {
            expr->type = Type_Boolean;
            break;
        }

        /// Constant folding for integer operations
        AST_Node *left = expr->binary.left;
        AST_Node *right = expr->binary.right;

        if (left->kind == N_INT && right->kind == N_INT) {
            int64_t a = left->integer_val;
            int64_t b = right->integer_val;
            int64_t result = 0;

            switch (op) {
            case TK_PLUS:  result = a + b; break;
            case TK_MINUS: result = a - b; break;
            case TK_STAR:  result = a * b; break;
            case TK_SLASH: if (b != 0) result = a / b; break;
            case TK_MOD:   if (b != 0) result = a % b; break;
            case TK_REM:   if (b != 0) result = a % b; break;
            default: goto no_fold;
            }

            expr->kind = N_INT;
            expr->integer_val = result;
            expr->type = Type_Universal_Int;
            break;
        }
        no_fold:

        /// Comparison operators return Boolean
        if (op >= TK_EQUAL && op <= TK_GREATER_EQUAL) {
            expr->type = Type_Boolean;
        }
        else {
            expr->type = Base_Type(left->type);
        }
        break;
    }

    case N_UN: {
        Analyze_Expression(sem, expr->unary.operand, expected);

        Token_Kind op = expr->unary.op;
        AST_Node *operand = expr->unary.operand;

        /// Constant folding
        if (op == TK_MINUS && operand->kind == N_INT) {
            expr->kind = N_INT;
            expr->integer_val = -operand->integer_val;
            expr->type = Type_Universal_Int;
            break;
        }
        if (op == TK_PLUS && operand->kind == N_INT) {
            expr->kind = N_INT;
            expr->integer_val = operand->integer_val;
            expr->type = Type_Universal_Int;
            break;
        }

        if (op == TK_NOT) {
            Type_Descriptor *ot = operand->type ? Base_Type(operand->type) : NULL;
            expr->type = (ot && ot->kind == TY_ARRAY) ? ot : Type_Boolean;
        }
        else {
            expr->type = Base_Type(operand->type);
        }
        break;
    }

    case N_IX: {
        Analyze_Expression(sem, expr->indexed.prefix, NULL);
        for (uint32_t i = 0; i < expr->indexed.indices.count; i++) {
            Analyze_Expression(sem, expr->indexed.indices.data[i], NULL);
        }
        Type_Descriptor *pt = expr->indexed.prefix->type;
        if (pt && pt->kind == TY_ARRAY) {
            expr->type = Base_Type(pt->element_type);
        }
        else {
            expr->type = Type_Integer;
        }
        break;
    }

    case N_SEL: {
        Analyze_Expression(sem, expr->selected.prefix, NULL);
        AST_Node *prefix = expr->selected.prefix;

        /// Check for package selection
        if (prefix->kind == N_ID) {
            Symbol_Entry *pkg_sym = Symbol_Find(sem, prefix->string_val);
            if (pkg_sym && pkg_sym->kind == SK_PACKAGE && pkg_sym->definition) {
                AST_Node *pkg = pkg_sym->definition;
                if (pkg->kind == N_PKS) {
                    /// Search package for the selected entity
                    for (uint32_t i = 0; i < pkg->package_spec.visible_decls.count; i++) {
                        AST_Node *d = pkg->package_spec.visible_decls.data[i];
                        if (d->symbol && String_Equal_CI(d->symbol->name,
                                                         expr->selected.selector)) {
                            expr->type = d->symbol->type;
                            expr->symbol = d->symbol;
                            return;
                        }
                    }
                }
            }
        }

        /// Record component selection
        Type_Descriptor *pt = prefix->type ? Base_Type(prefix->type) : NULL;
        if (pt && pt->kind == TY_RECORD) {
            for (uint32_t i = 0; i < pt->components.count; i++) {
                AST_Node *comp = pt->components.data[i];
                if (comp->kind == N_CM &&
                    String_Equal_CI(comp->component.name, expr->selected.selector)) {
                    expr->type = Resolve_Subtype(sem, comp->component.comp_type);
                    return;
                }
            }
        }
        expr->type = Type_Integer;
        break;
    }

    case N_AT: {
        Analyze_Expression(sem, expr->attr.prefix, NULL);
        for (uint32_t i = 0; i < expr->attr.args.count; i++) {
            Analyze_Expression(sem, expr->attr.args.data[i], NULL);
        }

        Type_Descriptor *pt = expr->attr.prefix->type;
        String_Slice attr = expr->attr.attribute;

        /// Determine attribute result type
        if (String_Equal_CI(attr, STR("FIRST")) || String_Equal_CI(attr, STR("LAST"))) {
            if (pt && pt->element_type)
                expr->type = pt->element_type;
            else if (pt && Is_Discrete(pt))
                expr->type = pt;
            else
                expr->type = Type_Integer;
        }
        else if (String_Equal_CI(attr, STR("LENGTH")) ||
                 String_Equal_CI(attr, STR("SIZE")) ||
                 String_Equal_CI(attr, STR("POS")) ||
                 String_Equal_CI(attr, STR("COUNT"))) {
            expr->type = Type_Integer;
        }
        else if (String_Equal_CI(attr, STR("IMAGE"))) {
            expr->type = Type_String;
        }
        else if (String_Equal_CI(attr, STR("VALUE")) ||
                 String_Equal_CI(attr, STR("SUCC")) ||
                 String_Equal_CI(attr, STR("PRED")) ||
                 String_Equal_CI(attr, STR("VAL"))) {
            expr->type = pt ? pt : Type_Integer;
        }
        else if (String_Equal_CI(attr, STR("RANGE"))) {
            expr->type = Type_Integer;
        }
        else if (String_Equal_CI(attr, STR("CALLABLE")) ||
                 String_Equal_CI(attr, STR("TERMINATED")) ||
                 String_Equal_CI(attr, STR("CONSTRAINED"))) {
            expr->type = Type_Boolean;
        }
        else {
            expr->type = Type_Integer;
        }
        break;
    }

    case N_QL: {
        Type_Descriptor *qt = Resolve_Subtype(sem, expr->qualified.type_name);
        Analyze_Expression(sem, expr->qualified.expression, qt);
        expr->type = qt;
        break;
    }

    case N_CL: {
        Analyze_Expression(sem, expr->call.callee, NULL);
        for (uint32_t i = 0; i < expr->call.args.count; i++) {
            Analyze_Expression(sem, expr->call.args.data[i], NULL);
        }

        /// Check if this is actually an indexed component
        Type_Descriptor *ft = expr->call.callee->type;
        if (ft && ft->kind == TY_ARRAY) {
            /// Convert to indexed component
            AST_Node *prefix = expr->call.callee;
            Node_Vector indices = expr->call.args;
            expr->kind = N_IX;
            expr->indexed.prefix = prefix;
            expr->indexed.indices = indices;
            Analyze_Expression(sem, expr, expected);
            break;
        }

        /// Function call
        if (expr->call.callee->symbol) {
            Symbol_Entry *fn = expr->call.callee->symbol;
            if (fn->type && fn->type->kind == TY_PRIVATE && fn->type->element_type) {
                expr->type = fn->type->element_type;
            }
            else if (fn->kind == SK_TYPE) {
                /// Type conversion
                expr->type = fn->type;
            }
            else {
                expr->type = Type_Integer;
            }
        }
        else {
            expr->type = Type_Integer;
        }
        break;
    }

    case N_AG: {
        for (uint32_t i = 0; i < expr->aggregate.items.count; i++) {
            Type_Descriptor *elem = expected && expected->element_type ?
                                   expected->element_type : expected;
            Analyze_Expression(sem, expr->aggregate.items.data[i], elem);
        }
        expr->type = expected ? expected : Type_Integer;
        break;
    }

    case N_ASC: {
        if (expr->association.value) {
            Type_Descriptor *vt = expected && expected->kind == TY_ARRAY ?
                                 expected->element_type : expected;
            Analyze_Expression(sem, expr->association.value, vt);
        }
        break;
    }

    case N_ALC: {
        Type_Descriptor *t = Type_New(TY_ACCESS, (String_Slice){NULL, 0});
        t->element_type = Resolve_Subtype(sem, expr->allocator.subtype);
        if (expr->allocator.init_value) {
            Analyze_Expression(sem, expr->allocator.init_value, t->element_type);
        }
        expr->type = t;
        break;
    }

    case N_RN: {
        Analyze_Expression(sem, expr->range.low_bound, expected);
        Analyze_Expression(sem, expr->range.high_bound, expected);
        expr->type = expr->range.low_bound ? Base_Type(expr->range.low_bound->type) : Type_Integer;
        break;
    }

    case N_DRF: {
        Analyze_Expression(sem, expr->unary.operand, NULL);
        Type_Descriptor *dt = expr->unary.operand->type ?
                             Base_Type(expr->unary.operand->type) : NULL;
        if (dt && dt->kind == TY_ACCESS) {
            expr->type = dt->element_type;
        }
        else {
            expr->type = Type_Integer;
        }
        break;
    }

    default:
        break;
    }
}


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   A N A L Y S I S
///-----------------------------------------------------------------------------

void Analyze_Statement(Semantic_Context *sem, AST_Node *stmt)
{
    if (!stmt) return;

    switch (stmt->kind) {
    case N_AS:
        Analyze_Expression(sem, stmt->assignment.target, NULL);
        Analyze_Expression(sem, stmt->assignment.value, stmt->assignment.target->type);
        break;

    case N_IF:
        Analyze_Expression(sem, stmt->if_stmt.condition, Type_Boolean);
        for (uint32_t i = 0; i < stmt->if_stmt.then_stmts.count; i++) {
            Analyze_Statement(sem, stmt->if_stmt.then_stmts.data[i]);
        }
        for (uint32_t i = 0; i < stmt->if_stmt.elsif_parts.count; i++) {
            AST_Node *elsif = stmt->if_stmt.elsif_parts.data[i];
            Analyze_Expression(sem, elsif->if_stmt.condition, Type_Boolean);
            for (uint32_t j = 0; j < elsif->if_stmt.then_stmts.count; j++) {
                Analyze_Statement(sem, elsif->if_stmt.then_stmts.data[j]);
            }
        }
        for (uint32_t i = 0; i < stmt->if_stmt.else_stmts.count; i++) {
            Analyze_Statement(sem, stmt->if_stmt.else_stmts.data[i]);
        }
        break;

    case N_CS:
        Analyze_Expression(sem, stmt->case_stmt.selector, NULL);
        for (uint32_t i = 0; i < stmt->case_stmt.alternatives.count; i++) {
            AST_Node *alt = stmt->case_stmt.alternatives.data[i];
            for (uint32_t j = 0; j < alt->when_clause.choices.count; j++) {
                Analyze_Expression(sem, alt->when_clause.choices.data[j],
                                 stmt->case_stmt.selector->type);
            }
            for (uint32_t j = 0; j < alt->when_clause.stmts.count; j++) {
                Analyze_Statement(sem, alt->when_clause.stmts.data[j]);
            }
        }
        break;

    case N_LP: {
        if (stmt->loop_stmt.label.data) {
            Symbol_Entry *lbl = Symbol_New(stmt->loop_stmt.label, SK_LABEL, NULL, stmt);
            Symbol_Add(sem, lbl);
        }

        if (stmt->loop_stmt.iteration) {
            /// FOR loop - declare iteration variable
            if (stmt->loop_stmt.iteration->kind == N_BIN &&
                stmt->loop_stmt.iteration->binary.op == TK_IN) {
                AST_Node *var = stmt->loop_stmt.iteration->binary.left;
                if (var->kind == N_ID) {
                    Type_Descriptor *rt = stmt->loop_stmt.iteration->binary.right->type;
                    Symbol_Entry *lv = Symbol_New(var->string_val, SK_VARIABLE,
                                                 rt ? rt : Type_Integer, NULL);
                    lv->kind = SK_LOOP_VARIABLE;
                    Symbol_Add(sem, lv);
                    var->symbol = lv;
                }
            }
            Analyze_Expression(sem, stmt->loop_stmt.iteration, Type_Boolean);
        }

        for (uint32_t i = 0; i < stmt->loop_stmt.stmts.count; i++) {
            Analyze_Statement(sem, stmt->loop_stmt.stmts.data[i]);
        }
        break;
    }

    case N_BL: {
        if (stmt->block_stmt.label.data) {
            Symbol_Entry *lbl = Symbol_New(stmt->block_stmt.label, SK_LABEL, NULL, stmt);
            Symbol_Add(sem, lbl);
        }

        Scope_Push(sem);
        for (uint32_t i = 0; i < stmt->block_stmt.decls.count; i++) {
            Analyze_Declaration(sem, stmt->block_stmt.decls.data[i]);
        }
        for (uint32_t i = 0; i < stmt->block_stmt.stmts.count; i++) {
            Analyze_Statement(sem, stmt->block_stmt.stmts.data[i]);
        }
        for (uint32_t i = 0; i < stmt->block_stmt.handlers.count; i++) {
            AST_Node *h = stmt->block_stmt.handlers.data[i];
            for (uint32_t j = 0; j < h->handler.stmts.count; j++) {
                Analyze_Statement(sem, h->handler.stmts.data[j]);
            }
        }
        Scope_Pop(sem);
        break;
    }

    case N_RT:
        if (stmt->return_stmt.value) {
            Analyze_Expression(sem, stmt->return_stmt.value, NULL);
        }
        break;

    case N_EX:
        if (stmt->exit_stmt.condition) {
            Analyze_Expression(sem, stmt->exit_stmt.condition, Type_Boolean);
        }
        break;

    case N_RS:
        if (stmt->raise_stmt.exception) {
            Analyze_Expression(sem, stmt->raise_stmt.exception, NULL);
        }
        break;

    case N_CLT:
        Analyze_Expression(sem, stmt->call.callee, NULL);
        for (uint32_t i = 0; i < stmt->call.args.count; i++) {
            Analyze_Expression(sem, stmt->call.args.data[i], NULL);
        }
        break;

    case N_ACC:
        Scope_Push(sem);
        for (uint32_t i = 0; i < stmt->accept_stmt.params.count; i++) {
            AST_Node *p = stmt->accept_stmt.params.data[i];
            Type_Descriptor *pt = Resolve_Subtype(sem, p->param.param_type);
            Symbol_Entry *ps = Symbol_New(p->param.param_name, SK_VARIABLE, pt, p);
            Symbol_Add(sem, ps);
            p->symbol = ps;
        }
        for (uint32_t i = 0; i < stmt->accept_stmt.stmts.count; i++) {
            Analyze_Statement(sem, stmt->accept_stmt.stmts.data[i]);
        }
        Scope_Pop(sem);
        break;

    case N_SLS:
        if (stmt->select_stmt.guard) {
            Analyze_Expression(sem, stmt->select_stmt.guard, NULL);
        }
        for (uint32_t i = 0; i < stmt->select_stmt.alternatives.count; i++) {
            Analyze_Statement(sem, stmt->select_stmt.alternatives.data[i]);
        }
        break;

    case N_DL:
        Analyze_Expression(sem, stmt->delay_stmt.duration, NULL);
        break;

    case N_AB:
        /// Abort statement - task name in call.callee
        if (stmt->call.callee) {
            Analyze_Expression(sem, stmt->call.callee, NULL);
        }
        break;

    case N_US:
        if (stmt->use_clause.package_name->kind == N_ID) {
            Symbol_Entry *pkg = Symbol_Find(sem, stmt->use_clause.package_name->string_val);
            if (pkg) {
                /// Make package declarations directly visible
                Symbol_Use_Package(sem, pkg, stmt->use_clause.package_name->string_val);
            }
        }
        break;

    default:
        break;
    }
}


///-----------------------------------------------------------------------------
///                   D E C L A R A T I O N   A N A L Y S I S
///-----------------------------------------------------------------------------

void Analyze_Declaration(Semantic_Context *sem, AST_Node *decl)
{
    if (!decl) return;

    switch (decl->kind) {
    case N_TD: {
        Type_Descriptor *type;

        if (decl->type_decl.is_derived && decl->type_decl.parent_type) {
            Type_Descriptor *parent = Resolve_Subtype(sem, decl->type_decl.parent_type);
            type = Type_New(parent->kind, decl->type_decl.name);
            type->parent_type = parent;
            type->base_type = parent;
            type->element_type = parent->element_type;
            type->low_bound = parent->low_bound;
            type->high_bound = parent->high_bound;
        }
        else if (decl->type_decl.definition) {
            type = Resolve_Subtype(sem, decl->type_decl.definition);
            type->name = decl->type_decl.name;
        }
        else {
            type = Type_New(TY_VOID, decl->type_decl.name);
        }

        Symbol_Entry *sym = Symbol_New(decl->type_decl.name, SK_TYPE, type, decl);
        Symbol_Add(sem, sym);
        decl->symbol = sym;

        /// Process record components
        if (type->kind == TY_RECORD && decl->type_decl.definition &&
            decl->type_decl.definition->kind == N_TR) {
            AST_Node *rec = decl->type_decl.definition;
            for (uint32_t i = 0; i < rec->record_type.components.count; i++) {
                AST_Node *comp = rec->record_type.components.data[i];
                if (comp->kind == N_CM) {
                    Type_Descriptor *ct = Resolve_Subtype(sem, comp->component.comp_type);
                    Symbol_Entry *cs = Symbol_New(comp->component.name, SK_COMPONENT, ct, comp);
                    cs->parent = sym;
                    comp->symbol = cs;
                    Node_Vector_Push(&type->components, comp);
                }
            }
        }

        /// Process enumeration literals
        if (type->kind == TY_ENUMERATION && decl->type_decl.definition &&
            decl->type_decl.definition->kind == N_TE) {
            AST_Node *en = decl->type_decl.definition;
            for (uint32_t i = 0; i < en->enumeration.literals.count; i++) {
                AST_Node *lit = en->enumeration.literals.data[i];
                String_Slice name;
                if (lit->kind == N_ID) {
                    name = lit->string_val;
                }
                else if (lit->kind == N_CHAR) {
                    char *buf = Arena_Alloc(2);
                    buf[0] = lit->integer_val;
                    buf[1] = '\0';
                    name = (String_Slice){buf, 1};
                }
                else continue;

                Symbol_Entry *ls = Symbol_New(name, SK_ENUMERATION_LITERAL, type, NULL);
                ls->value = i;
                Symbol_Add(sem, ls);
                Symbol_Vector_Push(&type->enum_literals, ls);
            }
            type->low_bound = 0;
            type->high_bound = en->enumeration.literals.count - 1;
        }

        Type_Freeze(sem, type, decl->location);
        break;
    }

    case N_SD: {
        Type_Descriptor *base = Resolve_Subtype(sem, decl->subtype_decl.indication);
        Symbol_Entry *sym = Symbol_New(decl->subtype_decl.name, SK_TYPE, base, decl);
        Symbol_Add(sem, sym);
        decl->symbol = sym;
        break;
    }

    case N_OD: {
        Type_Descriptor *type = decl->object_decl.object_type ?
                               Resolve_Subtype(sem, decl->object_decl.object_type) : NULL;

        if (decl->object_decl.init_value) {
            Analyze_Expression(sem, decl->object_decl.init_value, type);
            if (!type) type = decl->object_decl.init_value->type;
        }

        for (uint32_t i = 0; i < decl->object_decl.names.count; i++) {
            AST_Node *id = decl->object_decl.names.data[i];
            Symbol_Kind sk = decl->object_decl.is_constant ? SK_CONSTANT : SK_VARIABLE;
            Symbol_Entry *sym = Symbol_New(id->string_val, sk, type, decl);
            Symbol_Add(sem, sym);
            id->symbol = sym;
        }
        break;
    }

    case N_ED: {
        for (uint32_t i = 0; i < decl->exception_decl.names.count; i++) {
            AST_Node *id = decl->exception_decl.names.data[i];
            Symbol_Entry *sym = Symbol_New(id->string_val, SK_EXCEPTION, NULL, decl);
            Symbol_Add(sem, sym);
            id->symbol = sym;
        }
        break;
    }

    case N_PD:
    case N_PB: {
        AST_Node *spec = decl->kind == N_PB ?
                        decl->subprog_body.spec : decl->subprog_body.spec;

        Type_Descriptor *subp_type = Type_New(TY_PRIVATE, spec->subprog_spec.name);
        Symbol_Entry *sym = Symbol_New(spec->subprog_spec.name, SK_PROCEDURE,
                                      subp_type, decl);
        Symbol_Add(sem, sym);
        decl->symbol = sym;

        if (decl->kind == N_PB) {
            Scope_Push(sem);

            /// Add parameters
            for (uint32_t i = 0; i < spec->subprog_spec.params.count; i++) {
                AST_Node *p = spec->subprog_spec.params.data[i];
                Type_Descriptor *pt = Resolve_Subtype(sem, p->param.param_type);
                Symbol_Entry *ps = Symbol_New(p->param.param_name, SK_VARIABLE, pt, p);
                Symbol_Add(sem, ps);
                p->symbol = ps;
            }

            /// Analyze body
            for (uint32_t i = 0; i < decl->subprog_body.decls.count; i++) {
                Analyze_Declaration(sem, decl->subprog_body.decls.data[i]);
            }
            for (uint32_t i = 0; i < decl->subprog_body.stmts.count; i++) {
                Analyze_Statement(sem, decl->subprog_body.stmts.data[i]);
            }
            for (uint32_t i = 0; i < decl->subprog_body.handlers.count; i++) {
                AST_Node *h = decl->subprog_body.handlers.data[i];
                for (uint32_t j = 0; j < h->handler.stmts.count; j++) {
                    Analyze_Statement(sem, h->handler.stmts.data[j]);
                }
            }

            Scope_Pop(sem);
        }
        break;
    }

    case N_FD:
    case N_FB: {
        AST_Node *spec = decl->kind == N_FB ?
                        decl->subprog_body.spec : decl->subprog_body.spec;

        Type_Descriptor *ret_type = Resolve_Subtype(sem, spec->subprog_spec.return_type);
        Type_Descriptor *subp_type = Type_New(TY_PRIVATE, spec->subprog_spec.name);
        subp_type->element_type = ret_type;

        Symbol_Entry *sym = Symbol_New(spec->subprog_spec.name, SK_FUNCTION,
                                      subp_type, decl);
        Symbol_Add(sem, sym);
        decl->symbol = sym;

        if (decl->kind == N_FB) {
            Scope_Push(sem);

            for (uint32_t i = 0; i < spec->subprog_spec.params.count; i++) {
                AST_Node *p = spec->subprog_spec.params.data[i];
                Type_Descriptor *pt = Resolve_Subtype(sem, p->param.param_type);
                Symbol_Entry *ps = Symbol_New(p->param.param_name, SK_VARIABLE, pt, p);
                Symbol_Add(sem, ps);
                p->symbol = ps;
            }

            for (uint32_t i = 0; i < decl->subprog_body.decls.count; i++) {
                Analyze_Declaration(sem, decl->subprog_body.decls.data[i]);
            }
            for (uint32_t i = 0; i < decl->subprog_body.stmts.count; i++) {
                Analyze_Statement(sem, decl->subprog_body.stmts.data[i]);
            }
            for (uint32_t i = 0; i < decl->subprog_body.handlers.count; i++) {
                AST_Node *h = decl->subprog_body.handlers.data[i];
                for (uint32_t j = 0; j < h->handler.stmts.count; j++) {
                    Analyze_Statement(sem, h->handler.stmts.data[j]);
                }
            }

            Scope_Pop(sem);
        }
        break;
    }

    case N_PKS: {
        Type_Descriptor *pkg_type = Type_New(TY_VOID, decl->package_spec.name);
        Symbol_Entry *sym = Symbol_New(decl->package_spec.name, SK_PACKAGE, pkg_type, decl);
        Symbol_Add(sem, sym);
        decl->symbol = sym;

        Scope_Push(sem);
        for (uint32_t i = 0; i < decl->package_spec.visible_decls.count; i++) {
            Analyze_Declaration(sem, decl->package_spec.visible_decls.data[i]);
        }
        for (uint32_t i = 0; i < decl->package_spec.private_decls.count; i++) {
            Analyze_Declaration(sem, decl->package_spec.private_decls.data[i]);
        }
        Scope_Pop(sem);
        break;
    }

    case N_PKB: {
        Symbol_Entry *pkg = Symbol_Find(sem, decl->package_body.name);
        if (!pkg) {
            Type_Descriptor *pkg_type = Type_New(TY_VOID, decl->package_body.name);
            pkg = Symbol_New(decl->package_body.name, SK_PACKAGE, pkg_type, decl);
            Symbol_Add(sem, pkg);
        }
        decl->symbol = pkg;

        Scope_Push(sem);
        for (uint32_t i = 0; i < decl->package_body.decls.count; i++) {
            Analyze_Declaration(sem, decl->package_body.decls.data[i]);
        }
        for (uint32_t i = 0; i < decl->package_body.stmts.count; i++) {
            Analyze_Statement(sem, decl->package_body.stmts.data[i]);
        }
        Scope_Pop(sem);
        break;
    }

    case N_TKS: {
        Type_Descriptor *task_type = Type_New(TY_TASK, decl->task_spec.name);
        Symbol_Entry *sym = Symbol_New(decl->task_spec.name, SK_TASK_TYPE, task_type, decl);
        Symbol_Add(sem, sym);
        decl->symbol = sym;
        break;
    }

    case N_TKB: {
        Symbol_Entry *task = Symbol_Find(sem, decl->task_body.name);
        if (!task) {
            Type_Descriptor *task_type = Type_New(TY_TASK, decl->task_body.name);
            task = Symbol_New(decl->task_body.name, SK_TASK_TYPE, task_type, decl);
            Symbol_Add(sem, task);
        }
        decl->symbol = task;

        Scope_Push(sem);
        for (uint32_t i = 0; i < decl->task_body.decls.count; i++) {
            Analyze_Declaration(sem, decl->task_body.decls.data[i]);
        }
        for (uint32_t i = 0; i < decl->task_body.stmts.count; i++) {
            Analyze_Statement(sem, decl->task_body.stmts.data[i]);
        }
        Scope_Pop(sem);
        break;
    }

    case N_US:
        Analyze_Statement(sem, decl);
        break;

    case N_PG:
        for (uint32_t i = 0; i < decl->pragma_node.args.count; i++) {
            Analyze_Expression(sem, decl->pragma_node.args.data[i], NULL);
        }
        break;

    case N_GEN:
        /// Generic declarations are stored but not elaborated until instantiation
        if (decl->generic_decl.unit) {
            String_Slice name;
            if (decl->generic_decl.unit->kind == N_PKS) {
                name = decl->generic_decl.unit->package_spec.name;
            }
            else if (decl->generic_decl.unit->kind == N_PD ||
                     decl->generic_decl.unit->kind == N_FD) {
                name = decl->generic_decl.unit->subprog_body.spec->subprog_spec.name;
            }
            else {
                break;
            }
            Symbol_Entry *sym = Symbol_New(name, SK_GENERIC, NULL, decl);
            Symbol_Add(sem, sym);
            decl->symbol = sym;
        }
        break;

    case N_GINST: {
        Symbol_Entry *gen = Symbol_Find(sem, decl->generic_inst.generic_name);
        if (gen && gen->definition && gen->definition->kind == N_GEN) {
            /// TODO: Instantiate generic with actual parameters
            Symbol_Entry *sym = Symbol_New(decl->generic_inst.name,
                                          SK_PACKAGE, NULL, decl);
            Symbol_Add(sem, sym);
            decl->symbol = sym;
        }
        break;
    }

    case N_LST:
        for (uint32_t i = 0; i < decl->list.items.count; i++) {
            Analyze_Declaration(sem, decl->list.items.data[i]);
        }
        break;

    default:
        break;
    }
}


///-----------------------------------------------------------------------------
///                   C O M P I L A T I O N   U N I T   A N A L Y S I S
///-----------------------------------------------------------------------------

void Analyze_Compilation_Unit(Semantic_Context *sem, AST_Node *unit)
{
    if (!unit || unit->kind != N_CU) return;

    /// Process context clause (WITH and USE)
    if (unit->comp_unit.context) {
        AST_Node *ctx = unit->comp_unit.context;

        for (uint32_t i = 0; i < ctx->context.with_clauses.count; i++) {
            AST_Node *with = ctx->context.with_clauses.data[i];
            /// TODO: Load the withed library unit
            (void)with;
        }

        for (uint32_t i = 0; i < ctx->context.use_clauses.count; i++) {
            AST_Node *use = ctx->context.use_clauses.data[i];
            Analyze_Statement(sem, use);
        }
    }

    /// Process library units
    for (uint32_t i = 0; i < unit->comp_unit.units.count; i++) {
        Analyze_Declaration(sem, unit->comp_unit.units.data[i]);
    }
}


///-----------------------------------------------------------------------------
///                   E X P R E S S I O N   E V A L U A T I O N
///-----------------------------------------------------------------------------

Runtime_Value Eval_Expression(Eval_Context *ctx, AST_Node *expr)
{
    if (!expr) return Make_Integer(0, NULL);

    switch (expr->kind) {
    case N_INT:
        return Make_Integer(expr->integer_val, Type_Universal_Int);

    case N_REAL:
        return Make_Real(expr->real_val, Type_Universal_Real);

    case N_CHAR:
        return Make_Integer(expr->integer_val, Type_Character);

    case N_STR:
        return Make_String(expr->string_val);

    case N_NULL:
        return Make_Null(expr->type);

    case N_ID: {
        if (expr->symbol) {
            /// Look up value in bindings
            Symbol_Entry *sym = expr->symbol;

            /// Check current call frame
            if (ctx->call_depth > 0) {
                Call_Frame *frame = &ctx->call_stack[ctx->call_depth - 1];
                for (uint32_t i = 0; i < frame->locals.count; i++) {
                    if (frame->locals.bindings[i].symbol == sym) {
                        return frame->locals.bindings[i].value;
                    }
                }
            }

            /// Check globals
            for (uint32_t i = 0; i < ctx->globals.count; i++) {
                if (ctx->globals.bindings[i].symbol == sym) {
                    return ctx->globals.bindings[i].value;
                }
            }

            /// Enum literal
            if (sym->kind == SK_ENUMERATION_LITERAL) {
                return Make_Integer(sym->value, sym->type);
            }

            /// Constant with value
            if (sym->kind == SK_CONSTANT && sym->definition) {
                AST_Node *def = sym->definition;
                if (def->kind == N_OD && def->object_decl.init_value) {
                    return Eval_Expression(ctx, def->object_decl.init_value);
                }
            }
        }
        return Make_Integer(0, expr->type);
    }

    case N_BIN: {
        Token_Kind op = expr->binary.op;

        /// Short-circuit evaluation
        if (op == TK_AND_THEN) {
            Runtime_Value left = Eval_Expression(ctx, expr->binary.left);
            if (!left.integer) return Make_Boolean(false);
            return Eval_Expression(ctx, expr->binary.right);
        }
        if (op == TK_OR_ELSE) {
            Runtime_Value left = Eval_Expression(ctx, expr->binary.left);
            if (left.integer) return Make_Boolean(true);
            return Eval_Expression(ctx, expr->binary.right);
        }

        Runtime_Value left = Eval_Expression(ctx, expr->binary.left);
        Runtime_Value right = Eval_Expression(ctx, expr->binary.right);

        return Eval_Binary_Op(ctx, op, left, right, expr->type);
    }

    case N_UN: {
        Runtime_Value operand = Eval_Expression(ctx, expr->unary.operand);
        return Eval_Unary_Op(ctx, expr->unary.op, operand, expr->type);
    }

    case N_IX: {
        Runtime_Value arr = Eval_Expression(ctx, expr->indexed.prefix);
        if (arr.kind != VK_ARRAY && arr.kind != VK_STRING) {
            return Make_Integer(0, expr->type);
        }

        Runtime_Value idx = Eval_Expression(ctx, expr->indexed.indices.data[0]);
        int64_t index = idx.integer;

        if (arr.kind == VK_STRING) {
            int64_t offset = index - 1;  // String indices start at 1
            if (offset >= 0 && offset < (int64_t)arr.string.length) {
                return Make_Integer(arr.string.data[offset], Type_Character);
            }
            Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), expr->location);
        }

        Check_Index(ctx, index, arr.type, expr->location);
        int64_t offset = index - arr.array.low;
        return arr.array.elements[offset];
    }

    case N_SEL: {
        Runtime_Value rec = Eval_Expression(ctx, expr->selected.prefix);
        if (rec.kind != VK_RECORD) {
            return Make_Integer(0, expr->type);
        }

        for (uint32_t i = 0; i < rec.record.count; i++) {
            if (String_Equal_CI(rec.record.components[i].name, expr->selected.selector)) {
                return *rec.record.components[i].value;
            }
        }
        return Make_Integer(0, expr->type);
    }

    case N_AT:
        return Eval_Attribute(ctx, expr->attr.prefix,
                             expr->attr.attribute,
                             &expr->attr.args);

    case N_QL: {
        Runtime_Value val = Eval_Expression(ctx, expr->qualified.expression);
        val.type = expr->type;
        return val;
    }

    case N_CL: {
        if (expr->call.callee->symbol) {
            Symbol_Entry *fn = expr->call.callee->symbol;

            /// Type conversion
            if (fn->kind == SK_TYPE) {
                Runtime_Value arg = Eval_Expression(ctx, expr->call.args.data[0]);
                arg.type = fn->type;
                return arg;
            }

            /// Function call
            if (fn->definition) {
                return Exec_Call(ctx, fn->definition, &expr->call.args);
            }
        }
        return Make_Integer(0, expr->type);
    }

    case N_AG: {
        if (expr->type && Is_Array(expr->type)) {
            /// Array aggregate
            Runtime_Value val = Make_Value(VK_ARRAY, expr->type);
            val.array.low = expr->type->low_bound;
            val.array.high = expr->type->high_bound;
            val.array.count = val.array.high - val.array.low + 1;
            val.array.elements = calloc(val.array.count, sizeof(Runtime_Value));

            for (uint32_t i = 0; i < expr->aggregate.items.count; i++) {
                AST_Node *comp = expr->aggregate.items.data[i];
                Runtime_Value elem = Eval_Expression(ctx, comp);
                if (i < val.array.count) {
                    val.array.elements[i] = elem;
                }
            }
            return val;
        }
        else if (expr->type && Is_Record(expr->type)) {
            /// Record aggregate
            Runtime_Value val = Make_Value(VK_RECORD, expr->type);
            val.record.count = expr->aggregate.items.count;
            val.record.components = calloc(val.record.count, sizeof(Record_Component_Value));

            for (uint32_t i = 0; i < expr->aggregate.items.count; i++) {
                AST_Node *comp = expr->aggregate.items.data[i];
                if (comp->kind == N_ASC && comp->association.choices.count > 0) {
                    AST_Node *name = comp->association.choices.data[0];
                    val.record.components[i].name = name->string_val;
                    val.record.components[i].value = Arena_Alloc(sizeof(Runtime_Value));
                    *val.record.components[i].value = Eval_Expression(ctx, comp->association.value);
                }
                else {
                    val.record.components[i].value = Arena_Alloc(sizeof(Runtime_Value));
                    *val.record.components[i].value = Eval_Expression(ctx, comp);
                }
            }
            return val;
        }
        return Make_Integer(0, expr->type);
    }

    case N_ALC: {
        Runtime_Value val = Make_Value(VK_ACCESS, expr->type);
        val.access = Arena_Alloc(sizeof(Runtime_Value));
        if (expr->allocator.init_value) {
            *(Runtime_Value*)val.access = Eval_Expression(ctx, expr->allocator.init_value);
        }
        return val;
    }

    case N_DRF: {
        Runtime_Value ptr = Eval_Expression(ctx, expr->unary.operand);
        if (ptr.kind == VK_ACCESS && ptr.access) {
            return *(Runtime_Value*)ptr.access;
        }
        Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), expr->location);
    }

    case N_ASC:
        if (expr->association.value) {
            return Eval_Expression(ctx, expr->association.value);
        }
        return Make_Integer(0, NULL);

    default:
        return Make_Integer(0, expr->type);
    }
}


int64_t Eval_Integer(Eval_Context *ctx, AST_Node *expr)
{
    Runtime_Value val = Eval_Expression(ctx, expr);
    return val.integer;
}


double Eval_Real(Eval_Context *ctx, AST_Node *expr)
{
    Runtime_Value val = Eval_Expression(ctx, expr);
    if (val.kind == VK_REAL) return val.real;
    return (double)val.integer;
}


bool Eval_Boolean(Eval_Context *ctx, AST_Node *expr)
{
    Runtime_Value val = Eval_Expression(ctx, expr);
    return val.integer != 0;
}


///-----------------------------------------------------------------------------
///                   B I N A R Y   O P E R A T O R   E V A L U A T I O N
///-----------------------------------------------------------------------------

Runtime_Value Eval_Binary_Op(Eval_Context *ctx, Token_Kind op,
                            Runtime_Value left, Runtime_Value right,
                            Type_Descriptor *result_type)
{
    /// Integer operations
    if (left.kind == VK_INTEGER && right.kind == VK_INTEGER) {
        int64_t a = left.integer;
        int64_t b = right.integer;
        int64_t result = 0;

        switch (op) {
        case TK_PLUS:  result = a + b; break;
        case TK_MINUS: result = a - b; break;
        case TK_STAR:  result = a * b; break;
        case TK_SLASH:
            if (b == 0) Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), (Source_Location){0,0,""});
            result = a / b;
            break;
        case TK_MOD:
            if (b == 0) Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), (Source_Location){0,0,""});
            result = ((a % b) + b) % b;  // Ada MOD semantics
            break;
        case TK_REM:
            if (b == 0) Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), (Source_Location){0,0,""});
            result = a % b;
            break;
        case TK_DOUBLE_STAR:
            result = (int64_t)pow((double)a, (double)b);
            break;

        /// Comparison operators
        case TK_EQUAL:         return Make_Boolean(a == b);
        case TK_NOT_EQUAL:     return Make_Boolean(a != b);
        case TK_LESS_THAN:          return Make_Boolean(a < b);
        case TK_LESS_EQUAL:    return Make_Boolean(a <= b);
        case TK_GREATER_THAN:       return Make_Boolean(a > b);
        case TK_GREATER_EQUAL: return Make_Boolean(a >= b);

        /// Logical operators
        case TK_AND: result = a & b; break;
        case TK_OR:  result = a | b; break;
        case TK_XOR: result = a ^ b; break;

        default:
            return Make_Integer(0, result_type);
        }
        return Make_Integer(result, result_type);
    }

    /// Real operations
    if (left.kind == VK_REAL || right.kind == VK_REAL) {
        double a = left.kind == VK_REAL ? left.real : (double)left.integer;
        double b = right.kind == VK_REAL ? right.real : (double)right.integer;
        double result = 0;

        switch (op) {
        case TK_PLUS:  result = a + b; break;
        case TK_MINUS: result = a - b; break;
        case TK_STAR:  result = a * b; break;
        case TK_SLASH:
            if (b == 0.0) Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), (Source_Location){0,0,""});
            result = a / b;
            break;
        case TK_DOUBLE_STAR: result = pow(a, b); break;

        /// Comparison
        case TK_EQUAL:         return Make_Boolean(a == b);
        case TK_NOT_EQUAL:     return Make_Boolean(a != b);
        case TK_LESS_THAN:          return Make_Boolean(a < b);
        case TK_LESS_EQUAL:    return Make_Boolean(a <= b);
        case TK_GREATER_THAN:       return Make_Boolean(a > b);
        case TK_GREATER_EQUAL: return Make_Boolean(a >= b);

        default:
            return Make_Real(0, result_type);
        }
        return Make_Real(result, result_type);
    }

    /// String concatenation
    if ((left.kind == VK_STRING || left.kind == VK_ARRAY) && op == TK_AMPERSAND) {
        String_Slice ls = left.kind == VK_STRING ? left.string :
                         (String_Slice){(const char*)left.array.elements, left.array.count};
        String_Slice rs = right.kind == VK_STRING ? right.string :
                         (String_Slice){(const char*)right.array.elements,
                                       right.kind == VK_ARRAY ? right.array.count : 1};

        uint32_t len = ls.length + rs.length;
        char *buf = Arena_Alloc(len + 1);
        memcpy(buf, ls.data, ls.length);
        memcpy(buf + ls.length, rs.data, rs.length);
        buf[len] = '\0';
        return Make_String((String_Slice){buf, len});
    }

    return Make_Integer(0, result_type);
}


///-----------------------------------------------------------------------------
///                   U N A R Y   O P E R A T O R   E V A L U A T I O N
///-----------------------------------------------------------------------------

Runtime_Value Eval_Unary_Op(Eval_Context *ctx, Token_Kind op,
                           Runtime_Value operand,
                           Type_Descriptor *result_type)
{
    (void)ctx;

    if (operand.kind == VK_INTEGER) {
        switch (op) {
        case TK_PLUS:  return Make_Integer(operand.integer, result_type);
        case TK_MINUS: return Make_Integer(-operand.integer, result_type);
        case TK_ABS:   return Make_Integer(operand.integer >= 0 ? operand.integer : -operand.integer, result_type);
        case TK_NOT:   return Make_Boolean(!operand.integer);
        default: break;
        }
    }

    if (operand.kind == VK_REAL) {
        switch (op) {
        case TK_PLUS:  return Make_Real(operand.real, result_type);
        case TK_MINUS: return Make_Real(-operand.real, result_type);
        case TK_ABS:   return Make_Real(fabs(operand.real), result_type);
        default: break;
        }
    }

    return Make_Integer(0, result_type);
}


///-----------------------------------------------------------------------------
///                   A T T R I B U T E   E V A L U A T I O N
///-----------------------------------------------------------------------------

Runtime_Value Eval_Attribute(Eval_Context *ctx, AST_Node *prefix,
                            String_Slice attr, Node_Vector *args)
{
    (void)ctx;

    Type_Descriptor *pt = prefix ? prefix->type : NULL;

    if (String_Equal_CI(attr, STR("FIRST"))) {
        if (pt && pt->low_bound != 0) {
            return Make_Integer(pt->low_bound, pt);
        }
        if (pt && pt->kind == TY_ARRAY) {
            return Make_Integer(pt->low_bound, pt->index_type);
        }
        return Make_Integer(0, Type_Integer);
    }

    if (String_Equal_CI(attr, STR("LAST"))) {
        if (pt && pt->high_bound != 0) {
            return Make_Integer(pt->high_bound, pt);
        }
        if (pt && pt->kind == TY_ARRAY) {
            return Make_Integer(pt->high_bound, pt->index_type);
        }
        return Make_Integer(0, Type_Integer);
    }

    if (String_Equal_CI(attr, STR("LENGTH"))) {
        if (pt && pt->kind == TY_ARRAY) {
            int64_t len = pt->high_bound - pt->low_bound + 1;
            return Make_Integer(len > 0 ? len : 0, Type_Integer);
        }
        return Make_Integer(0, Type_Integer);
    }

    if (String_Equal_CI(attr, STR("SIZE"))) {
        if (pt) {
            return Make_Integer(pt->size > 0 ? pt->size * 8 : 32, Type_Integer);
        }
        return Make_Integer(32, Type_Integer);
    }

    if (String_Equal_CI(attr, STR("POS"))) {
        if (args && args->count > 0) {
            Runtime_Value arg = Eval_Expression(ctx, args->data[0]);
            return Make_Integer(arg.integer, Type_Integer);
        }
        return Make_Integer(0, Type_Integer);
    }

    if (String_Equal_CI(attr, STR("VAL"))) {
        if (args && args->count > 0) {
            Runtime_Value arg = Eval_Expression(ctx, args->data[0]);
            return Make_Integer(arg.integer, pt);
        }
        return Make_Integer(0, pt);
    }

    if (String_Equal_CI(attr, STR("SUCC"))) {
        if (args && args->count > 0) {
            Runtime_Value arg = Eval_Expression(ctx, args->data[0]);
            return Make_Integer(arg.integer + 1, pt);
        }
        return Make_Integer(0, pt);
    }

    if (String_Equal_CI(attr, STR("PRED"))) {
        if (args && args->count > 0) {
            Runtime_Value arg = Eval_Expression(ctx, args->data[0]);
            return Make_Integer(arg.integer - 1, pt);
        }
        return Make_Integer(0, pt);
    }

    if (String_Equal_CI(attr, STR("IMAGE"))) {
        if (args && args->count > 0) {
            Runtime_Value arg = Eval_Expression(ctx, args->data[0]);
            char *buf = Arena_Alloc(64);
            int len = snprintf(buf, 64, "%lld", (long long)arg.integer);
            return Make_String((String_Slice){buf, len});
        }
        return Make_String(STR(""));
    }

    return Make_Integer(0, Type_Integer);
}


///-----------------------------------------------------------------------------
///                   S T A T E M E N T   E X E C U T I O N
///-----------------------------------------------------------------------------

void Exec_Statement(Eval_Context *ctx, AST_Node *stmt)
{
    if (!stmt) return;
    if (ctx->exit_loop || ctx->goto_active || ctx->exception_raised) return;

    switch (stmt->kind) {
    case N_AS: {
        Runtime_Value val = Eval_Expression(ctx, stmt->assignment.value);
        AST_Node *target = stmt->assignment.target;

        if (target->symbol) {
            /// Simple variable assignment
            Symbol_Entry *sym = target->symbol;

            /// Check current call frame
            if (ctx->call_depth > 0) {
                Call_Frame *frame = &ctx->call_stack[ctx->call_depth - 1];
                for (uint32_t i = 0; i < frame->locals.count; i++) {
                    if (frame->locals.bindings[i].symbol == sym) {
                        frame->locals.bindings[i].value = val;
                        return;
                    }
                }
            }

            /// Check/add to globals
            for (uint32_t i = 0; i < ctx->globals.count; i++) {
                if (ctx->globals.bindings[i].symbol == sym) {
                    ctx->globals.bindings[i].value = val;
                    return;
                }
            }

            /// Add new global binding
            if (ctx->globals.count >= ctx->globals.capacity) {
                ctx->globals.capacity = ctx->globals.capacity ? ctx->globals.capacity * 2 : 64;
                ctx->globals.bindings = realloc(ctx->globals.bindings,
                                               ctx->globals.capacity * sizeof(Binding));
            }
            ctx->globals.bindings[ctx->globals.count].symbol = sym;
            ctx->globals.bindings[ctx->globals.count].value = val;
            ctx->globals.count++;
        }
        break;
    }

    case N_IF: {
        if (Eval_Boolean(ctx, stmt->if_stmt.condition)) {
            Exec_Statements(ctx, &stmt->if_stmt.then_stmts);
        }
        else {
            bool handled = false;
            for (uint32_t i = 0; i < stmt->if_stmt.elsif_parts.count && !handled; i++) {
                AST_Node *elsif = stmt->if_stmt.elsif_parts.data[i];
                if (Eval_Boolean(ctx, elsif->if_stmt.condition)) {
                    Exec_Statements(ctx, &elsif->if_stmt.then_stmts);
                    handled = true;
                }
            }
            if (!handled) {
                Exec_Statements(ctx, &stmt->if_stmt.else_stmts);
            }
        }
        break;
    }

    case N_CS: {
        Runtime_Value sel = Eval_Expression(ctx, stmt->case_stmt.selector);

        for (uint32_t i = 0; i < stmt->case_stmt.alternatives.count; i++) {
            AST_Node *alt = stmt->case_stmt.alternatives.data[i];
            bool matched = false;

            for (uint32_t j = 0; j < alt->when_clause.choices.count && !matched; j++) {
                AST_Node *choice = alt->when_clause.choices.data[j];

                if (choice->kind == N_ID &&
                    String_Equal_CI(choice->string_val, STR("others"))) {
                    matched = true;
                }
                else if (choice->kind == N_RN) {
                    Runtime_Value lo = Eval_Expression(ctx, choice->range.low_bound);
                    Runtime_Value hi = Eval_Expression(ctx, choice->range.high_bound);
                    matched = (sel.integer >= lo.integer && sel.integer <= hi.integer);
                }
                else {
                    Runtime_Value cv = Eval_Expression(ctx, choice);
                    matched = (sel.integer == cv.integer);
                }
            }

            if (matched) {
                Exec_Statements(ctx, &alt->when_clause.stmts);
                break;
            }
        }
        break;
    }

    case N_LP: {
        ctx->exit_loop = false;

        if (stmt->loop_stmt.iteration) {
            /// FOR or WHILE loop
            if (stmt->loop_stmt.iteration->kind == N_BIN &&
                stmt->loop_stmt.iteration->binary.op == TK_IN) {
                /// FOR loop
                AST_Node *var = stmt->loop_stmt.iteration->binary.left;
                AST_Node *range_node = stmt->loop_stmt.iteration->binary.right;

                int64_t lo, hi;
                if (range_node->kind == N_RN) {
                    lo = Eval_Integer(ctx, range_node->range.low_bound);
                    hi = Eval_Integer(ctx, range_node->range.high_bound);
                }
                else if (range_node->type) {
                    lo = range_node->type->low_bound;
                    hi = range_node->type->high_bound;
                }
                else {
                    lo = 0;
                    hi = -1;
                }

                /// Create loop variable binding
                if (ctx->call_depth > 0) {
                    Call_Frame *frame = &ctx->call_stack[ctx->call_depth - 1];
                    if (frame->locals.count >= frame->locals.capacity) {
                        frame->locals.capacity = frame->locals.capacity ? frame->locals.capacity * 2 : 16;
                        frame->locals.bindings = realloc(frame->locals.bindings,
                                                        frame->locals.capacity * sizeof(Binding));
                    }
                    frame->locals.bindings[frame->locals.count].symbol = var->symbol;
                    frame->locals.bindings[frame->locals.count].value = Make_Integer(lo, var->type);
                    uint32_t var_idx = frame->locals.count++;

                    if (stmt->loop_stmt.is_reverse) {
                        for (int64_t i = hi; i >= lo && !ctx->exit_loop; i--) {
                            frame->locals.bindings[var_idx].value.integer = i;
                            Exec_Statements(ctx, &stmt->loop_stmt.stmts);
                        }
                    }
                    else {
                        for (int64_t i = lo; i <= hi && !ctx->exit_loop; i++) {
                            frame->locals.bindings[var_idx].value.integer = i;
                            Exec_Statements(ctx, &stmt->loop_stmt.stmts);
                        }
                    }
                }
            }
            else {
                /// WHILE loop
                while (!ctx->exit_loop && Eval_Boolean(ctx, stmt->loop_stmt.iteration)) {
                    Exec_Statements(ctx, &stmt->loop_stmt.stmts);
                }
            }
        }
        else {
            /// Simple loop
            while (!ctx->exit_loop) {
                Exec_Statements(ctx, &stmt->loop_stmt.stmts);
            }
        }

        ctx->exit_loop = false;
        break;
    }

    case N_BL:
        for (uint32_t i = 0; i < stmt->block_stmt.decls.count; i++) {
            Elaborate_Declaration(ctx, stmt->block_stmt.decls.data[i]);
        }
        Exec_Statements(ctx, &stmt->block_stmt.stmts);
        break;

    case N_RT:
        if (ctx->call_depth > 0) {
            Call_Frame *frame = &ctx->call_stack[ctx->call_depth - 1];
            if (stmt->return_stmt.value) {
                frame->return_value = Eval_Expression(ctx, stmt->return_stmt.value);
            }
            frame->has_returned = true;
        }
        break;

    case N_EX:
        if (stmt->exit_stmt.condition) {
            if (!Eval_Boolean(ctx, stmt->exit_stmt.condition)) {
                break;
            }
        }
        ctx->exit_loop = true;
        ctx->exit_label = stmt->exit_stmt.label;
        break;

    case N_GT:
        ctx->goto_active = true;
        ctx->goto_label = stmt->goto_stmt.label;
        break;

    case N_RS:
        if (stmt->raise_stmt.exception) {
            if (stmt->raise_stmt.exception->kind == N_ID) {
                Raise_Exception(ctx, stmt->raise_stmt.exception->string_val,
                              stmt->location);
            }
        }
        else {
            Raise_Exception(ctx, STR("PROGRAM_ERROR"), stmt->location);
        }
        break;

    case N_NS:
        break;

    case N_CLT: {
        if (stmt->call.callee->symbol) {
            Symbol_Entry *proc = stmt->call.callee->symbol;
            if (proc->definition) {
                Exec_Call(ctx, proc->definition, &stmt->call.args);
            }
        }
        break;
    }

    case N_DL: {
        Runtime_Value dur = Eval_Expression(ctx, stmt->delay_stmt.duration);
        double secs = dur.kind == VK_REAL ? dur.real : (double)dur.integer;
        /// In a real implementation, this would delay execution
        (void)secs;
        break;
    }

    default:
        break;
    }
}


void Exec_Statements(Eval_Context *ctx, Node_Vector *stmts)
{
    for (uint32_t i = 0; i < stmts->count; i++) {
        if (ctx->exit_loop || ctx->goto_active || ctx->exception_raised) break;
        if (ctx->call_depth > 0 && ctx->call_stack[ctx->call_depth - 1].has_returned) break;
        Exec_Statement(ctx, stmts->data[i]);
    }
}


///-----------------------------------------------------------------------------
///                   S U B P R O G R A M   C A L L   E X E C U T I O N
///-----------------------------------------------------------------------------

Runtime_Value Exec_Call(Eval_Context *ctx, AST_Node *subprogram,
                       Node_Vector *arguments)
{
    if (!subprogram) return Make_Integer(0, NULL);

    /// Check stack overflow
    if (ctx->call_depth >= ctx->call_capacity) {
        Raise_Exception(ctx, STR("STORAGE_ERROR"), subprogram->location);
    }

    /// Push new call frame
    Call_Frame *frame = &ctx->call_stack[ctx->call_depth++];
    memset(frame, 0, sizeof(Call_Frame));
    frame->subprogram = subprogram;

    /// Get spec
    AST_Node *spec = NULL;
    Node_Vector *decls = NULL;
    Node_Vector *stmts = NULL;
    Node_Vector *handlers = NULL;

    if (subprogram->kind == N_PB || subprogram->kind == N_FB) {
        spec = subprogram->subprog_body.spec;
        decls = &subprogram->subprog_body.decls;
        stmts = &subprogram->subprog_body.stmts;
        handlers = &subprogram->subprog_body.handlers;
    }
    else {
        ctx->call_depth--;
        return Make_Integer(0, NULL);
    }

    /// Bind parameters
    frame->locals.capacity = 16;
    frame->locals.bindings = calloc(frame->locals.capacity, sizeof(Binding));

    for (uint32_t i = 0; i < spec->subprog_spec.params.count; i++) {
        AST_Node *param = spec->subprog_spec.params.data[i];
        Runtime_Value val = Make_Integer(0, NULL);

        if (arguments && i < arguments->count) {
            AST_Node *arg = arguments->data[i];
            if (arg->kind == N_ASC) {
                val = Eval_Expression(ctx, arg->association.value);
            }
            else {
                val = Eval_Expression(ctx, arg);
            }
        }
        else if (param->param.default_value) {
            val = Eval_Expression(ctx, param->param.default_value);
        }

        if (frame->locals.count >= frame->locals.capacity) {
            frame->locals.capacity *= 2;
            frame->locals.bindings = realloc(frame->locals.bindings,
                                            frame->locals.capacity * sizeof(Binding));
        }
        frame->locals.bindings[frame->locals.count].symbol = param->symbol;
        frame->locals.bindings[frame->locals.count].value = val;
        frame->locals.count++;
    }

    /// Elaborate local declarations
    for (uint32_t i = 0; i < decls->count; i++) {
        Elaborate_Declaration(ctx, decls->data[i]);
    }

    /// Execute statements
    Exec_Statements(ctx, stmts);

    /// Handle exceptions
    if (ctx->exception_raised && handlers && handlers->count > 0) {
        ctx->exception_raised = false;
        for (uint32_t i = 0; i < handlers->count; i++) {
            AST_Node *h = handlers->data[i];
            bool matches = false;

            for (uint32_t j = 0; j < h->handler.exceptions.count && !matches; j++) {
                AST_Node *exc = h->handler.exceptions.data[j];
                if (exc->kind == N_ID) {
                    if (String_Equal_CI(exc->string_val, STR("others")) ||
                        String_Equal_CI(exc->string_val, ctx->current_exception)) {
                        matches = true;
                    }
                }
            }

            if (matches) {
                Exec_Statements(ctx, &h->handler.stmts);
                break;
            }
        }
    }

    Runtime_Value result = frame->return_value;

    /// Clean up
    if (frame->locals.bindings) {
        free(frame->locals.bindings);
    }

    ctx->call_depth--;

    return result;
}


///-----------------------------------------------------------------------------
///                   D E C L A R A T I O N   E L A B O R A T I O N
///-----------------------------------------------------------------------------

void Elaborate_Declaration(Eval_Context *ctx, AST_Node *decl)
{
    if (!decl) return;

    switch (decl->kind) {
    case N_OD: {
        for (uint32_t i = 0; i < decl->object_decl.names.count; i++) {
            AST_Node *id = decl->object_decl.names.data[i];
            if (!id->symbol) continue;

            Runtime_Value val = Make_Integer(0, id->symbol->type);

            if (decl->object_decl.init_value) {
                val = Eval_Expression(ctx, decl->object_decl.init_value);
            }

            /// Add to appropriate frame
            if (ctx->call_depth > 0) {
                Call_Frame *frame = &ctx->call_stack[ctx->call_depth - 1];
                if (frame->locals.count >= frame->locals.capacity) {
                    frame->locals.capacity = frame->locals.capacity ? frame->locals.capacity * 2 : 16;
                    frame->locals.bindings = realloc(frame->locals.bindings,
                                                    frame->locals.capacity * sizeof(Binding));
                }
                frame->locals.bindings[frame->locals.count].symbol = id->symbol;
                frame->locals.bindings[frame->locals.count].value = val;
                frame->locals.count++;
            }
            else {
                if (ctx->globals.count >= ctx->globals.capacity) {
                    ctx->globals.capacity = ctx->globals.capacity ? ctx->globals.capacity * 2 : 64;
                    ctx->globals.bindings = realloc(ctx->globals.bindings,
                                                   ctx->globals.capacity * sizeof(Binding));
                }
                ctx->globals.bindings[ctx->globals.count].symbol = id->symbol;
                ctx->globals.bindings[ctx->globals.count].value = val;
                ctx->globals.count++;
            }
        }
        break;
    }

    case N_LST:
        for (uint32_t i = 0; i < decl->list.items.count; i++) {
            Elaborate_Declaration(ctx, decl->list.items.data[i]);
        }
        break;

    default:
        break;
    }
}


void Elaborate_Compilation_Unit(Eval_Context *ctx, AST_Node *unit)
{
    if (!unit || unit->kind != N_CU) return;

    for (uint32_t i = 0; i < unit->comp_unit.units.count; i++) {
        AST_Node *u = unit->comp_unit.units.data[i];

        if (u->kind == N_PKB) {
            for (uint32_t j = 0; j < u->package_body.decls.count; j++) {
                Elaborate_Declaration(ctx, u->package_body.decls.data[j]);
            }
            Exec_Statements(ctx, &u->package_body.stmts);
        }
        else if (u->kind == N_PB) {
            /// Main procedure
            Exec_Call(ctx, u, NULL);
        }
    }
}


///-----------------------------------------------------------------------------
///                   R U N T I M E   C H E C K S
///-----------------------------------------------------------------------------

void Check_Range(Eval_Context *ctx, Runtime_Value value,
                Type_Descriptor *type, Source_Location loc)
{
    if (!type || false /* suppress_checks not implemented */) return;

    if (Is_Discrete(type) || type->kind == TY_FLOAT || type->kind == TY_FIXED) {
        int64_t v = value.kind == VK_REAL ? (int64_t)value.real : value.integer;
        if (v < type->low_bound || v > type->high_bound) {
            Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), loc);
        }
    }
}


void Check_Index(Eval_Context *ctx, int64_t index,
                Type_Descriptor *array, Source_Location loc)
{
    if (!array || false /* suppress_checks not implemented */) return;

    if (index < array->low_bound || index > array->high_bound) {
        Raise_Exception(ctx, STR("CONSTRAINT_ERROR"), loc);
    }
}


void Check_Discriminant(Eval_Context *ctx, Runtime_Value record,
                       Type_Descriptor *type, Source_Location loc)
{
    (void)ctx;
    (void)record;
    (void)type;
    (void)loc;
    /// TODO: Implement discriminant checking
}


_Noreturn void Raise_Exception(Eval_Context *ctx, String_Slice exception_name,
                              Source_Location loc)
{
    ctx->exception_raised = true;
    ctx->current_exception = exception_name;

    if (ctx->exception_handler) {
        longjmp(*ctx->exception_handler, 1);
    }

    Fatal_Error(loc, "unhandled exception: %.*s",
               (int)exception_name.length, exception_name.data);
}


///-----------------------------------------------------------------------------
///                   T E X T _ I O   O P E R A T I O N S
///-----------------------------------------------------------------------------

void Text_IO_Put(Eval_Context *ctx, Runtime_Value item)
{
    switch (item.kind) {
    case VK_INTEGER:
        fprintf(ctx->current_output, "%lld", (long long)item.integer);
        break;
    case VK_REAL:
        fprintf(ctx->current_output, "%g", item.real);
        break;
    case VK_STRING:
        fprintf(ctx->current_output, "%.*s",
               (int)item.string.length, item.string.data);
        break;
    default:
        break;
    }
}


Runtime_Value Text_IO_Get(Eval_Context *ctx, Type_Descriptor *type)
{
    if (Is_Discrete(type)) {
        int64_t v;
        if (fscanf(ctx->current_input, "%lld", (long long*)&v) == 1) {
            return Make_Integer(v, type);
        }
    }
    else if (type->kind == TY_FLOAT) {
        double v;
        if (fscanf(ctx->current_input, "%lf", &v) == 1) {
            return Make_Real(v, type);
        }
    }
    return Make_Integer(0, type);
}


void Text_IO_New_Line(Eval_Context *ctx, int64_t count)
{
    for (int64_t i = 0; i < count; i++) {
        fputc('\n', ctx->current_output);
    }
}


///-----------------------------------------------------------------------------
///                                  E N D                                    --
///-----------------------------------------------------------------------------
