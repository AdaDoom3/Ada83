///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                         T Y P E   S Y S T E M                             --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  Implementation of the Ada83 type system.                                 --
///  See ada83_types.h for interface documentation.                           --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_types.h"
#include "ada83_arena.h"
#include "ada83_string.h"
#include "ada83_symbols.h"


///-----------------------------------------------------------------------------
///                   P R E D E F I N E D   T Y P E   I N S T A N C E S
///-----------------------------------------------------------------------------

Type_Descriptor *Type_Integer       = NULL;
Type_Descriptor *Type_Natural       = NULL;
Type_Descriptor *Type_Positive      = NULL;
Type_Descriptor *Type_Boolean       = NULL;
Type_Descriptor *Type_Character     = NULL;
Type_Descriptor *Type_String        = NULL;
Type_Descriptor *Type_Float         = NULL;
Type_Descriptor *Type_Universal_Int = NULL;
Type_Descriptor *Type_Universal_Real= NULL;
Type_Descriptor *Type_File          = NULL;


///-----------------------------------------------------------------------------
///                   T Y P E   C O N S T R U C T O R
///-----------------------------------------------------------------------------

/// @brief Type_New
///
Type_Descriptor *Type_New(Type_Kind kind, String_Slice name)
{
    Type_Descriptor *type = Arena_Alloc(sizeof(Type_Descriptor));

    type->kind      = kind;
    type->name      = String_Dup(name);
    type->size      = 8;    // Default 64-bit size
    type->alignment = 8;    // Default 64-bit alignment

    return type;

} // Type_New


///-----------------------------------------------------------------------------
///                   P R E D E F I N E D   T Y P E   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

void Types_Initialize(void *sm)
{
    Semantic_Context *sem = (Semantic_Context *)sm;

    /// Initialize INTEGER type
    Type_Integer = Type_New(TY_INTEGER, STR("INTEGER"));
    Type_Integer->low_bound = -2147483648LL;
    Type_Integer->high_bound = 2147483647LL;

    /// Initialize NATURAL (subtype of INTEGER)
    Type_Natural = Type_New(TY_INTEGER, STR("NATURAL"));
    Type_Natural->low_bound = 0;
    Type_Natural->high_bound = 2147483647LL;
    Type_Natural->base_type = Type_Integer;

    /// Initialize POSITIVE (subtype of INTEGER)
    Type_Positive = Type_New(TY_INTEGER, STR("POSITIVE"));
    Type_Positive->low_bound = 1;
    Type_Positive->high_bound = 2147483647LL;
    Type_Positive->base_type = Type_Integer;

    /// Initialize BOOLEAN type
    Type_Boolean = Type_New(TY_BOOLEAN, STR("BOOLEAN"));
    Type_Boolean->low_bound = 0;
    Type_Boolean->high_bound = 1;

    /// Initialize CHARACTER type
    Type_Character = Type_New(TY_CHARACTER, STR("CHARACTER"));
    Type_Character->low_bound = 0;
    Type_Character->high_bound = 255;
    Type_Character->size = 1;

    /// Initialize STRING type (array of CHARACTER)
    Type_String = Type_New(TY_ARRAY, STR("STRING"));
    Type_String->element_type = Type_Character;
    Type_String->index_type = Type_Positive;
    Type_String->low_bound = 0;
    Type_String->high_bound = -1;  // Unconstrained

    /// Initialize FLOAT type
    Type_Float = Type_New(TY_FLOAT, STR("FLOAT"));

    /// Initialize universal types
    Type_Universal_Int = Type_New(TY_UNIVERSAL_INT, STR("universal_integer"));
    Type_Universal_Real = Type_New(TY_UNIVERSAL_REAL, STR("universal_real"));

    /// Initialize FILE type
    Type_File = Type_New(TY_FILE, STR("FILE_TYPE"));

    /// Add predefined types to symbol table
    if (sem) {
        Symbol_Add(sem, Symbol_New(STR("INTEGER"), SK_TYPE, Type_Integer, NULL));
        Symbol_Add(sem, Symbol_New(STR("NATURAL"), SK_TYPE, Type_Natural, NULL));
        Symbol_Add(sem, Symbol_New(STR("POSITIVE"), SK_TYPE, Type_Positive, NULL));
        Symbol_Add(sem, Symbol_New(STR("BOOLEAN"), SK_TYPE, Type_Boolean, NULL));
        Symbol_Add(sem, Symbol_New(STR("CHARACTER"), SK_TYPE, Type_Character, NULL));
        Symbol_Add(sem, Symbol_New(STR("STRING"), SK_TYPE, Type_String, NULL));
        Symbol_Add(sem, Symbol_New(STR("FLOAT"), SK_TYPE, Type_Float, NULL));

        /// Add Boolean literals
        Symbol_Entry *true_sym = Symbol_New(STR("TRUE"), SK_ENUMERATION_LITERAL, Type_Boolean, NULL);
        true_sym->value = 1;
        Symbol_Add(sem, true_sym);

        Symbol_Entry *false_sym = Symbol_New(STR("FALSE"), SK_ENUMERATION_LITERAL, Type_Boolean, NULL);
        false_sym->value = 0;
        Symbol_Add(sem, false_sym);

        /// Add predefined exceptions
        Symbol_Add(sem, Symbol_New(STR("CONSTRAINT_ERROR"), SK_EXCEPTION, NULL, NULL));
        Symbol_Add(sem, Symbol_New(STR("PROGRAM_ERROR"), SK_EXCEPTION, NULL, NULL));
        Symbol_Add(sem, Symbol_New(STR("STORAGE_ERROR"), SK_EXCEPTION, NULL, NULL));
        Symbol_Add(sem, Symbol_New(STR("TASKING_ERROR"), SK_EXCEPTION, NULL, NULL));
    }
}


///-----------------------------------------------------------------------------
///                   T Y P E   C O M P A R I S O N
///-----------------------------------------------------------------------------
///
///  Ada uses NAME equivalence, not structural equivalence.
///  Two types are the same only if they originate from the same declaration.
///
///  However, certain implicit conversions are allowed:
///    - Universal types convert to any numeric type
///    - Derived types inherit operations from parent
///
///-----------------------------------------------------------------------------

/// @brief Type_Same
///
bool Type_Same(Type_Descriptor *t1, Type_Descriptor *t2)
{
    // NULL types are never equal
    if (t1 == NULL || t2 == NULL) {
        return false;
    }

    // Pointer equality = same type declaration
    if (t1 == t2) {
        return true;
    }

    // Universal types are compatible with their category
    if (t1->kind == TY_UNIVERSAL_INT) {
        return t2->kind == TY_INTEGER || t2->kind == TY_UNIVERSAL_INT;
    }
    if (t2->kind == TY_UNIVERSAL_INT) {
        return t1->kind == TY_INTEGER || t1->kind == TY_UNIVERSAL_INT;
    }

    if (t1->kind == TY_UNIVERSAL_REAL) {
        return t2->kind == TY_FLOAT || t2->kind == TY_UNIVERSAL_REAL;
    }
    if (t2->kind == TY_UNIVERSAL_REAL) {
        return t1->kind == TY_FLOAT || t1->kind == TY_UNIVERSAL_REAL;
    }

    // Different types
    return false;

} // Type_Same


///-----------------------------------------------------------------------------
///                   T Y P E   C L A S S I F I C A T I O N
///-----------------------------------------------------------------------------

/// @brief Type_Is_Scalar
///
bool Type_Is_Scalar(Type_Descriptor *t)
{
    if (t == NULL) return false;

    switch (t->kind) {
        case TY_INTEGER:
        case TY_BOOLEAN:
        case TY_CHARACTER:
        case TY_FLOAT:
        case TY_FIXED:
        case TY_ENUMERATION:
        case TY_UNIVERSAL_INT:
        case TY_UNIVERSAL_REAL:
            return true;
        default:
            return false;
    }

} // Type_Is_Scalar


/// @brief Type_Is_Discrete
///
bool Type_Is_Discrete(Type_Descriptor *t)
{
    if (t == NULL) return false;

    switch (t->kind) {
        case TY_INTEGER:
        case TY_BOOLEAN:
        case TY_CHARACTER:
        case TY_ENUMERATION:
        case TY_UNIVERSAL_INT:
            return true;
        default:
            return false;
    }

} // Type_Is_Discrete


/// @brief Type_Is_Numeric
///
bool Type_Is_Numeric(Type_Descriptor *t)
{
    if (t == NULL) return false;

    switch (t->kind) {
        case TY_INTEGER:
        case TY_FLOAT:
        case TY_FIXED:
        case TY_UNIVERSAL_INT:
        case TY_UNIVERSAL_REAL:
            return true;
        default:
            return false;
    }

} // Type_Is_Numeric


///-----------------------------------------------------------------------------
///                   T Y P E   C O M P A T I B I L I T Y
///-----------------------------------------------------------------------------
///
///  Compatibility scoring is used for overload resolution.
///  Higher scores indicate better matches.
///
///  Score ranges:
///    0    = incompatible
///    1-99 = implicit conversion required
///    100+ = exact or preferred match
///
///-----------------------------------------------------------------------------

/// @brief Type_Score_Compatibility
///
int Type_Score_Compatibility(Type_Descriptor *t1,
                             Type_Descriptor *t2,
                             Type_Descriptor *result)
{
    (void)result;  // Not used in basic implementation

    if (t1 == NULL || t2 == NULL) {
        return 0;
    }

    // Exact match
    if (t1 == t2) {
        return 1000;
    }

    // Universal integer to integer
    if (t1->kind == TY_UNIVERSAL_INT && t2->kind == TY_INTEGER) {
        return 500;
    }
    if (t2->kind == TY_UNIVERSAL_INT && t1->kind == TY_INTEGER) {
        return 500;
    }

    // Universal real to float
    if (t1->kind == TY_UNIVERSAL_REAL && t2->kind == TY_FLOAT) {
        return 500;
    }
    if (t2->kind == TY_UNIVERSAL_REAL && t1->kind == TY_FLOAT) {
        return 500;
    }

    // Same kind (for derived types, etc.)
    if (t1->kind == t2->kind) {
        return 100;
    }

    return 0;

} // Type_Score_Compatibility


///-----------------------------------------------------------------------------
///                   T Y P E   F R E E Z I N G
///-----------------------------------------------------------------------------
///
///  Freezing computes the final representation of a type.
///  For composite types, this includes:
///    - Computing component offsets
///    - Determining total size
///    - Applying alignment requirements
///    - Generating implicit operations
///
///  The freeze point is where a type becomes fully elaborated and
///  its representation is committed.
///
///-----------------------------------------------------------------------------

/// @brief Type_Freeze
///
void Type_Freeze(void *sm, Type_Descriptor *type, Source_Location loc)
{
    (void)sm;  // Semantic context for symbol registration

    // Already frozen?
    if (type == NULL || type->freeze_state != 0) {
        return;
    }

    // For incomplete types (private, access to incomplete), defer freezing
    if (type->kind == TY_PRIVATE && type->parent_type != NULL) {
        if (type->parent_type->freeze_state == 0) {
            return;  // Wait for full type
        }
    }

    // Mark as frozen
    type->freeze_state = 1;
    type->freeze_node  = NODE(ERR, loc);

    // Freeze dependent types first
    if (type->base_type && type->base_type != type) {
        Type_Freeze(sm, type->base_type, loc);
    }
    if (type->parent_type) {
        Type_Freeze(sm, type->parent_type, loc);
    }
    if (type->element_type) {
        Type_Freeze(sm, type->element_type, loc);
    }

    // Compute representation based on type kind
    switch (type->kind) {
        case TY_RECORD: {
            // Layout record components
            uint32_t offset = 0;
            uint32_t max_align = 1;

            for (uint32_t i = 0; i < type->components.count; i++) {
                AST_Node *comp = type->components.data[i];
                if (comp->kind != N_CM) continue;

                // Get component type
                Type_Descriptor *comp_type = comp->type;
                if (comp_type) {
                    Type_Freeze(sm, comp_type, loc);
                }

                uint32_t comp_align = (comp_type && comp_type->alignment) ? comp_type->alignment : 8;
                uint32_t comp_size  = (comp_type && comp_type->size)      ? comp_type->size      : 8;

                // Track maximum alignment
                if (comp_align > max_align) {
                    max_align = comp_align;
                }

                // Align offset
                offset = (offset + comp_align - 1) & ~(comp_align - 1);

                // Store component offset
                comp->component.offset = offset;

                // Advance offset
                offset += comp_size;
            }

            // Final size (rounded up to alignment)
            type->size      = (offset + max_align - 1) & ~(max_align - 1);
            type->alignment = max_align;
            break;
        }

        case TY_ARRAY: {
            // Array size = element_count * element_size
            if (type->element_type) {
                Type_Freeze(sm, type->element_type, loc);

                uint32_t elem_align = type->element_type->alignment ? type->element_type->alignment : 8;
                uint32_t elem_size  = type->element_type->size      ? type->element_type->size      : 8;

                int64_t count = (type->high_bound - type->low_bound + 1);
                if (count > 0) {
                    type->size = (uint32_t)(count * elem_size);
                }
                else {
                    type->size = 0;  // Empty array
                }
                type->alignment = elem_align;
            }
            break;
        }

        default:
            // Other types use default size/alignment
            break;
    }

    // Generate implicit operations for composite types
    if ((type->kind == TY_RECORD || type->kind == TY_ARRAY) &&
        type->name.data && type->name.length > 0) {

        AST_Node *eq_op = Generate_Equality_Op(type, loc);
        if (eq_op) {
            Node_Vector_Push(&type->operations, eq_op);
        }

        AST_Node *assign_op = Generate_Assignment_Op(type, loc);
        if (assign_op) {
            Node_Vector_Push(&type->operations, assign_op);
        }

        AST_Node *init_op = Generate_Init_Op(type, loc);
        if (init_op) {
            Node_Vector_Push(&type->operations, init_op);
        }
    }

} // Type_Freeze


///-----------------------------------------------------------------------------
///                   I M P L I C I T   E Q U A L I T Y
///-----------------------------------------------------------------------------
///
///  For records: component-by-component comparison
///  For arrays: element-by-element comparison
///
///  function "=" (L, R : Type_Name) return Boolean is
///  begin
///    return L.comp1 = R.comp1 and L.comp2 = R.comp2 ...;  -- record
///    -- or --
///    for I in L'Range loop                                -- array
///      if L(I) /= R(I) then return False; end if;
///    end loop;
///    return True;
///  end "=";
///
///-----------------------------------------------------------------------------

/// @brief Generate_Equality_Op
///
AST_Node *Generate_Equality_Op(Type_Descriptor *type, Source_Location loc)
{
    // Create function body node
    AST_Node *func = NODE(FB, loc);

    // Create function specification
    func->subprog_body.spec = NODE(FS, loc);

    // Generate mangled name: Oeq<typename>
    char name_buf[256];
    snprintf(name_buf, sizeof(name_buf), "Oeq%.*s",
             (int)type->name.length, type->name.data);
    func->subprog_body.spec->subprog_spec.name = String_Dup(STR(name_buf));
    func->subprog_body.spec->subprog_spec.operator_name = STR("=");

    // Create parameters: (L, R : Type_Name)
    AST_Node *param_L = NODE(PM, loc);
    param_L->param.param_name = STR("L");
    param_L->param.param_type = NODE(ID, loc);
    param_L->param.param_type->string_val = type->name;
    param_L->param.mode = 0;  // IN

    AST_Node *param_R = NODE(PM, loc);
    param_R->param.param_name = STR("R");
    param_R->param.param_type = NODE(ID, loc);
    param_R->param.param_type->string_val = type->name;
    param_R->param.mode = 0;  // IN

    Node_Vector_Push(&func->subprog_body.spec->subprog_spec.params, param_L);
    Node_Vector_Push(&func->subprog_body.spec->subprog_spec.params, param_R);

    // Return type: BOOLEAN
    func->subprog_body.spec->subprog_spec.return_type = NODE(ID, loc);
    func->subprog_body.spec->subprog_spec.return_type->string_val = STR("BOOLEAN");

    // Create return statement with comparison
    AST_Node *ret_stmt = NODE(RT, loc);
    ret_stmt->return_stmt.value = NODE(BIN, loc);
    ret_stmt->return_stmt.value->binary.op = TK_EQUAL;

    if (type->kind == TY_RECORD) {
        // Build AND chain of component comparisons
        AST_Node *compare_expr = NULL;

        for (uint32_t i = 0; i < type->components.count; i++) {
            AST_Node *comp = type->components.data[i];
            if (comp->kind != N_CM) continue;

            // Create: L.comp = R.comp
            AST_Node *cmp = NODE(BIN, loc);
            cmp->binary.op = TK_EQUAL;

            AST_Node *left_sel = NODE(SEL, loc);
            left_sel->selected.prefix = NODE(ID, loc);
            left_sel->selected.prefix->string_val = STR("L");
            left_sel->selected.selector = comp->component.name;

            AST_Node *right_sel = NODE(SEL, loc);
            right_sel->selected.prefix = NODE(ID, loc);
            right_sel->selected.prefix->string_val = STR("R");
            right_sel->selected.selector = comp->component.name;

            cmp->binary.left = left_sel;
            cmp->binary.right = right_sel;

            if (compare_expr == NULL) {
                compare_expr = cmp;
            }
            else {
                // Chain with AND
                AST_Node *and_node = NODE(BIN, loc);
                and_node->binary.op = TK_AND;
                and_node->binary.left = compare_expr;
                and_node->binary.right = cmp;
                compare_expr = and_node;
            }
        }

        if (compare_expr) {
            ret_stmt->return_stmt.value = compare_expr;
        }
        else {
            // Empty record: always true
            ret_stmt->return_stmt.value = NODE(ID, loc);
            ret_stmt->return_stmt.value->string_val = STR("TRUE");
        }
    }
    else if (type->kind == TY_ARRAY) {
        // For arrays, generate a loop
        // Simplified: just return True placeholder
        ret_stmt->return_stmt.value = NODE(ID, loc);
        ret_stmt->return_stmt.value->string_val = STR("TRUE");
    }

    Node_Vector_Push(&func->subprog_body.stmts, ret_stmt);

    return func;

} // Generate_Equality_Op


///-----------------------------------------------------------------------------
///                   I M P L I C I T   A S S I G N M E N T
///-----------------------------------------------------------------------------
///
///  For records: component-by-component copy
///  For arrays: element-by-element copy
///
///-----------------------------------------------------------------------------

/// @brief Generate_Assignment_Op
///
AST_Node *Generate_Assignment_Op(Type_Descriptor *type, Source_Location loc)
{
    // Create procedure body node
    AST_Node *proc = NODE(PB, loc);

    // Create procedure specification
    proc->subprog_body.spec = NODE(PS, loc);

    // Generate mangled name: Oas<typename>
    char name_buf[256];
    snprintf(name_buf, sizeof(name_buf), "Oas%.*s",
             (int)type->name.length, type->name.data);
    proc->subprog_body.spec->subprog_spec.name = String_Dup(STR(name_buf));
    proc->subprog_body.spec->subprog_spec.operator_name = STR(":=");

    // Create parameters: (T : out Type_Name; S : Type_Name)
    AST_Node *param_T = NODE(PM, loc);
    param_T->param.param_name = STR("T");
    param_T->param.param_type = NODE(ID, loc);
    param_T->param.param_type->string_val = type->name;
    param_T->param.mode = 3;  // IN OUT

    AST_Node *param_S = NODE(PM, loc);
    param_S->param.param_name = STR("S");
    param_S->param.param_type = NODE(ID, loc);
    param_S->param.param_type->string_val = type->name;
    param_S->param.mode = 0;  // IN

    Node_Vector_Push(&proc->subprog_body.spec->subprog_spec.params, param_T);
    Node_Vector_Push(&proc->subprog_body.spec->subprog_spec.params, param_S);

    if (type->kind == TY_RECORD) {
        // Generate: T.comp := S.comp for each component
        for (uint32_t i = 0; i < type->components.count; i++) {
            AST_Node *comp = type->components.data[i];
            if (comp->kind != N_CM) continue;

            AST_Node *assign = NODE(AS, loc);

            AST_Node *target = NODE(SEL, loc);
            target->selected.prefix = NODE(ID, loc);
            target->selected.prefix->string_val = STR("T");
            target->selected.selector = comp->component.name;

            AST_Node *source = NODE(SEL, loc);
            source->selected.prefix = NODE(ID, loc);
            source->selected.prefix->string_val = STR("S");
            source->selected.selector = comp->component.name;

            assign->assignment.target = target;
            assign->assignment.value  = source;

            Node_Vector_Push(&proc->subprog_body.stmts, assign);
        }
    }

    return proc;

} // Generate_Assignment_Op


///-----------------------------------------------------------------------------
///                   I M P L I C I T   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------
///
///  Generates default initialization for record types with component defaults.
///
///-----------------------------------------------------------------------------

/// @brief Generate_Init_Op
///
AST_Node *Generate_Init_Op(Type_Descriptor *type, Source_Location loc)
{
    if (type->kind != TY_RECORD) {
        return NULL;
    }

    // Check if any components have default values
    bool has_defaults = false;
    for (uint32_t i = 0; i < type->components.count; i++) {
        AST_Node *comp = type->components.data[i];
        if (comp->kind == N_CM && comp->component.init_value != NULL) {
            has_defaults = true;
            break;
        }
    }

    if (!has_defaults) {
        return NULL;
    }

    // Create function that returns aggregate with defaults
    AST_Node *func = NODE(FB, loc);
    func->subprog_body.spec = NODE(FS, loc);

    char name_buf[256];
    snprintf(name_buf, sizeof(name_buf), "Oin%.*s",
             (int)type->name.length, type->name.data);
    func->subprog_body.spec->subprog_spec.name = String_Dup(STR(name_buf));

    func->subprog_body.spec->subprog_spec.return_type = NODE(ID, loc);
    func->subprog_body.spec->subprog_spec.return_type->string_val = type->name;

    // Build aggregate with defaults
    AST_Node *aggregate = NODE(AG, loc);

    for (uint32_t i = 0; i < type->components.count; i++) {
        AST_Node *comp = type->components.data[i];
        if (comp->kind != N_CM || comp->component.init_value == NULL) {
            continue;
        }

        AST_Node *assoc = NODE(ASC, loc);

        AST_Node *choice = NODE(ID, loc);
        choice->string_val = comp->component.name;
        Node_Vector_Push(&assoc->association.choices, choice);

        assoc->association.value = comp->component.init_value;
        Node_Vector_Push(&aggregate->aggregate.items, assoc);
    }

    AST_Node *ret_stmt = NODE(RT, loc);
    ret_stmt->return_stmt.value = aggregate;
    Node_Vector_Push(&func->subprog_body.stmts, ret_stmt);

    return (aggregate->aggregate.items.count > 0) ? func : NULL;

} // Generate_Init_Op
