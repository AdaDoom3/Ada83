///-----------------------------------------------------------------------------
///                                                                           --
///                        A D A 8 3   I N T E R P R E T E R                  --
///                                                                           --
///                    S Y M B O L   T A B L E                                --
///                                                                           --
///                                  B o d y                                  --
///                                                                           --
///  Implementation of the symbol table.                                      --
///  See ada83_symbols.h for interface documentation.                         --
///                                                                           --
///-----------------------------------------------------------------------------

#include "ada83_symbols.h"
#include "ada83_arena.h"
#include "ada83_string.h"


///-----------------------------------------------------------------------------
///                   G L O B A L   S T A T E
///-----------------------------------------------------------------------------

const char *Include_Paths[MAX_INCLUDE_PATHS];
int         Include_Path_Count = 0;

String_Slice Separate_Parent_Package = { NULL, 0 };


///-----------------------------------------------------------------------------
///                   H A S H   F U N C T I O N
///-----------------------------------------------------------------------------
///
///  Uses FNV-1a hash with case folding for Ada's case-insensitive identifiers.
///  Masks to SYMBOL_HASH_SIZE for bucket index.
///
///-----------------------------------------------------------------------------

/// @brief Symbol_Hash
///
uint32_t Symbol_Hash(String_Slice name)
{
    return (uint32_t)(String_Hash(name) & (SYMBOL_HASH_SIZE - 1));

} // Symbol_Hash


///-----------------------------------------------------------------------------
///                   S Y M B O L   C O N S T R U C T I O N
///-----------------------------------------------------------------------------

/// @brief Symbol_New
///
Symbol_Entry *Symbol_New(String_Slice     name,
                         Symbol_Kind      kind,
                         Type_Descriptor *type,
                         AST_Node        *def)
{
    Symbol_Entry *sym = Arena_Alloc(sizeof(Symbol_Entry));

    sym->name       = String_Dup(name);
    sym->kind       = kind;
    sym->type       = type;
    sym->definition = def;

    // Initialize other fields to defaults
    sym->elaboration   = -1;
    sym->nesting_level = -1;

    return sym;

} // Symbol_New


///-----------------------------------------------------------------------------
///                   S Y M B O L   A D D I T I O N
///-----------------------------------------------------------------------------
///
///  Adding a symbol links it into the hash chain for its name.
///  The symbol is tagged with the current scope level for lookup ordering.
///
///  Hash chains are maintained in a simple linked list per bucket.
///  More recently added symbols appear earlier in the chain.
///
///-----------------------------------------------------------------------------

/// @brief Symbol_Add
///
Symbol_Entry *Symbol_Add(Semantic_Context *ctx, Symbol_Entry *symbol)
{
    uint32_t hash = Symbol_Hash(symbol->name);

    // Link into hash chain (prepend for faster lookup of recent symbols)
    symbol->homonym = ctx->hash_table[hash];
    symbol->next    = ctx->hash_table[hash];

    // Set scope information
    symbol->scope        = ctx->current_scope;
    symbol->scope_serial = ctx->scope_serial++;
    symbol->elaboration  = ctx->elaboration_order++;
    symbol->nesting_level = ctx->current_scope;

    // Set visibility (directly visible by default)
    symbol->visibility = 1;

    // Generate unique ID
    uint64_t uid_hash = String_Hash(symbol->name);
    if (symbol->parent) {
        uid_hash = uid_hash * 31 + String_Hash(symbol->parent->name);
        if (symbol->nesting_level > 0) {
            uid_hash = uid_hash * 31 + symbol->scope;
            uid_hash = uid_hash * 31 + symbol->elaboration;
        }
    }
    symbol->uid = (uint32_t)(uid_hash & 0xFFFFFFFF);

    // Insert into hash table
    ctx->hash_table[hash] = symbol;

    // Track in scope stack
    if (ctx->scope_stack_depth < 256) {
        ctx->scope_stack[ctx->scope_stack_depth++] = symbol;
    }

    return symbol;

} // Symbol_Add


///-----------------------------------------------------------------------------
///                   S Y M B O L   L O O K U P
///-----------------------------------------------------------------------------
///
///  Lookup follows Ada's visibility rules:
///  1. Search for directly visible symbols (innermost scope first)
///  2. If not found directly, search use-visible symbols
///  3. Return the most recently declared matching symbol
///
///-----------------------------------------------------------------------------

/// @brief Symbol_Find
///
Symbol_Entry *Symbol_Find(Semantic_Context *ctx, String_Slice name)
{
    Symbol_Entry *immediate = NULL;
    Symbol_Entry *potential = NULL;

    uint32_t hash = Symbol_Hash(name);

    // Search hash chain
    for (Symbol_Entry *s = ctx->hash_table[hash]; s != NULL; s = s->next) {
        if (String_Equal_CI(s->name, name)) {
            // Check visibility
            if ((s->visibility & 1) && (!immediate || s->scope > immediate->scope)) {
                // Directly visible and better scope
                immediate = s;
            }
            if ((s->visibility & 2) && !potential) {
                // Use-visible
                potential = s;
            }
        }
    }

    // Prefer direct visibility
    if (immediate) {
        return immediate;
    }
    if (potential) {
        return potential;
    }

    // Search all entries (fallback for older symbols)
    for (Symbol_Entry *s = ctx->hash_table[hash]; s != NULL; s = s->next) {
        if (String_Equal_CI(s->name, name)) {
            if (!immediate || s->scope > immediate->scope) {
                immediate = s;
            }
        }
    }

    return immediate;

} // Symbol_Find


///-----------------------------------------------------------------------------
///                   O V E R L O A D   R E S O L U T I O N
///-----------------------------------------------------------------------------
///
///  Overload resolution finds the best matching subprogram when multiple
///  declarations share the same name. Per Ada83 LRM 8.7:
///    - Declarations are overloadable if they are subprograms or literals
///    - Resolution uses number and types of parameters
///    - Ambiguous calls are errors
///
///-----------------------------------------------------------------------------

/// @brief Symbol_Find_Overload
///
Symbol_Entry *Symbol_Find_Overload(Semantic_Context  *ctx,
                                   String_Slice       name,
                                   int                argument_count,
                                   Type_Descriptor   *expected_type)
{
    Symbol_Vector candidates = { NULL, 0, 0 };
    int max_scope = -1;

    uint32_t hash = Symbol_Hash(name);

    // Collect all matching symbols at the best scope
    for (Symbol_Entry *s = ctx->hash_table[hash]; s != NULL; s = s->next) {
        if (String_Equal_CI(s->name, name) && (s->visibility & 3)) {
            if (s->scope > max_scope) {
                candidates.count = 0;
                max_scope = s->scope;
            }
            if (s->scope == max_scope) {
                // Add to candidates (simple vector push)
                if (candidates.count >= candidates.capacity) {
                    candidates.capacity = candidates.capacity ? candidates.capacity * 2 : 8;
                    candidates.data = realloc(candidates.data,
                                              candidates.capacity * sizeof(Symbol_Entry *));
                }
                candidates.data[candidates.count++] = s;
            }
        }
    }

    if (candidates.count == 0) {
        return NULL;
    }
    if (candidates.count == 1) {
        return candidates.data[0];
    }

    // Score each candidate
    Symbol_Entry *best = NULL;
    int best_score = -1;

    for (uint32_t i = 0; i < candidates.count; i++) {
        Symbol_Entry *c = candidates.data[i];
        int score = 0;

        // Check if it's a callable (procedure/function)
        if (c->kind == SK_PROCEDURE || c->kind == SK_FUNCTION) {
            // Score based on parameter count and type matching
            if (c->overloads.count > 0) {
                for (uint32_t j = 0; j < c->overloads.count; j++) {
                    AST_Node *body = c->overloads.data[j];
                    if (body->kind == N_PB || body->kind == N_FB) {
                        int num_params = body->subprog_body.spec->subprog_spec.params.count;
                        if (num_params == argument_count) {
                            score += 1000;

                            // Type matching bonus
                            if (expected_type && c->type && c->type->element_type) {
                                int ts = Type_Score_Compatibility(c->type->element_type,
                                                                   expected_type, NULL);
                                score += ts;
                            }

                            if (score > best_score) {
                                best_score = score;
                                best = c;
                            }
                        }
                    }
                }
            }
            else {
                // No body yet - check spec
                if (argument_count == 1) {
                    score = 500;
                    if (expected_type) {
                        int ts = Type_Score_Compatibility(c->type, expected_type, NULL);
                        score += ts;
                    }
                    if (score > best_score) {
                        best_score = score;
                        best = c;
                    }
                }
            }
        }
        else if (c->kind == SK_TYPE) {
            // Type conversion
            if (argument_count < 0) {  // No call, just type reference
                score = 100;
                if (score > best_score) {
                    best_score = score;
                    best = c;
                }
            }
        }
    }

    return best ? best : candidates.data[0];

} // Symbol_Find_Overload


///-----------------------------------------------------------------------------
///                   U S E   V I S I B I L I T Y
///-----------------------------------------------------------------------------

/// @brief Symbol_Apply_Use
///
void Symbol_Apply_Use(Semantic_Context *ctx, Symbol_Entry *package, String_Slice name)
{
    uint32_t hash = Symbol_Hash(name) & 63;
    uint64_t bit = 1ULL << (Symbol_Hash(name) & 63);

    // Quick check for already processed
    if (ctx->use_visibility_bits[hash] & bit) {
        return;
    }
    ctx->use_visibility_bits[hash] |= bit;

    // Find all symbols in the package and make them use-visible
    for (Symbol_Entry *p = package; p != NULL; p = p->next) {
        if (String_Equal_CI(p->name, name) &&
            p->kind == SK_PACKAGE &&
            p->definition &&
            p->definition->kind == N_PKS) {

            AST_Node *pkg = p->definition;

            // Process visible declarations
            for (uint32_t i = 0; i < pkg->package_spec.visible_decls.count; i++) {
                AST_Node *decl = pkg->package_spec.visible_decls.data[i];

                if (decl->symbol) {
                    // Add to use-visible list
                    if (p->use_visible.count >= p->use_visible.capacity) {
                        p->use_visible.capacity = p->use_visible.capacity ? p->use_visible.capacity * 2 : 8;
                        p->use_visible.data = realloc(p->use_visible.data,
                                                       p->use_visible.capacity * sizeof(Symbol_Entry *));
                    }
                    p->use_visible.data[p->use_visible.count++] = decl->symbol;

                    // Mark as use-visible
                    decl->symbol->visibility |= 2;

                    // Also add to context's exception list if applicable
                    if (decl->kind == N_ED) {
                        for (uint32_t j = 0; j < decl->exception_decl.names.count; j++) {
                            AST_Node *e = decl->exception_decl.names.data[j];
                            if (e->symbol) {
                                if (ctx->exceptions.count >= ctx->exceptions.capacity) {
                                    ctx->exceptions.capacity = ctx->exceptions.capacity ? ctx->exceptions.capacity * 2 : 8;
                                    ctx->exceptions.data = realloc(ctx->exceptions.data,
                                                                    ctx->exceptions.capacity * sizeof(Symbol_Entry *));
                                }
                                ctx->exceptions.data[ctx->exceptions.count++] = e->symbol;
                            }
                        }
                    }
                }
            }

            // Track dependency
            if (ctx->dependency_depth < 256) {
                bool found = false;
                for (int i = 0; i < ctx->dependency_depth; i++) {
                    if (ctx->dependencies[i].count > 0 &&
                        String_Equal_CI(ctx->dependencies[i].data[0]->name, p->name)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (ctx->dependencies[ctx->dependency_depth].count >= ctx->dependencies[ctx->dependency_depth].capacity) {
                        ctx->dependencies[ctx->dependency_depth].capacity = 8;
                        ctx->dependencies[ctx->dependency_depth].data = malloc(8 * sizeof(Symbol_Entry *));
                    }
                    ctx->dependencies[ctx->dependency_depth].data[ctx->dependencies[ctx->dependency_depth].count++] = p;
                    ctx->dependency_depth++;
                }
            }
        }
    }

    ctx->use_visibility_bits[hash] &= ~bit;

} // Symbol_Apply_Use


///-----------------------------------------------------------------------------
///                   S C O P E   M A N A G E M E N T
///-----------------------------------------------------------------------------

/// @brief Scope_Enter
///
void Scope_Enter(Semantic_Context *ctx)
{
    ctx->current_scope++;
    ctx->scope_serial = 0;

} // Scope_Enter


/// @brief Scope_Exit
///
void Scope_Exit(Semantic_Context *ctx)
{
    if (ctx->current_scope > 0) {
        ctx->current_scope--;
    }

} // Scope_Exit


///-----------------------------------------------------------------------------
///                   G E N E R I C   T E M P L A T E   L O O K U P
///-----------------------------------------------------------------------------

/// @brief Generic_Find
///
Generic_Template *Generic_Find(Semantic_Context *ctx, String_Slice name)
{
    // Search through symbols for generic units
    Symbol_Entry *sym = Symbol_Find(ctx, name);

    if (sym && sym->kind == SK_GENERIC && sym->generic) {
        return sym->generic;
    }

    return NULL;

} // Generic_Find


///-----------------------------------------------------------------------------
///                   I N I T I A L I Z A T I O N
///-----------------------------------------------------------------------------

/// @brief Semantic_Init
///
void Semantic_Init(Semantic_Context *ctx)
{
    // Clear all state
    memset(ctx, 0, sizeof(Semantic_Context));

    // Initialize predefined types
    Type_Integer       = Type_New(TY_INTEGER, STR("INTEGER"));
    Type_Integer->low_bound  = -2147483648LL;
    Type_Integer->high_bound = 2147483647LL;

    Type_Natural       = Type_New(TY_INTEGER, STR("NATURAL"));
    Type_Natural->low_bound  = 0;
    Type_Natural->high_bound = 2147483647LL;

    Type_Positive      = Type_New(TY_INTEGER, STR("POSITIVE"));
    Type_Positive->low_bound  = 1;
    Type_Positive->high_bound = 2147483647LL;

    Type_Boolean       = Type_New(TY_BOOLEAN, STR("BOOLEAN"));
    Type_Character     = Type_New(TY_CHARACTER, STR("CHARACTER"));
    Type_Character->size = 1;

    Type_String        = Type_New(TY_ARRAY, STR("STRING"));
    Type_String->element_type = Type_Character;
    Type_String->index_type   = Type_Positive;
    Type_String->low_bound    = 0;
    Type_String->high_bound   = -1;  // Unconstrained

    Type_Float         = Type_New(TY_FLOAT, STR("FLOAT"));
    Type_Universal_Int = Type_New(TY_UNIVERSAL_INT, STR("universal_integer"));
    Type_Universal_Real= Type_New(TY_UNIVERSAL_REAL, STR("universal_real"));
    Type_File          = Type_New(TY_FILE, STR("FILE_TYPE"));

    // Add predefined symbols to context
    Symbol_Add(ctx, Symbol_New(STR("INTEGER"),   SK_TYPE, Type_Integer, NULL));
    Symbol_Add(ctx, Symbol_New(STR("NATURAL"),   SK_TYPE, Type_Natural, NULL));
    Symbol_Add(ctx, Symbol_New(STR("POSITIVE"),  SK_TYPE, Type_Positive, NULL));
    Symbol_Add(ctx, Symbol_New(STR("BOOLEAN"),   SK_TYPE, Type_Boolean, NULL));
    Symbol_Add(ctx, Symbol_New(STR("CHARACTER"), SK_TYPE, Type_Character, NULL));
    Symbol_Add(ctx, Symbol_New(STR("STRING"),    SK_TYPE, Type_String, NULL));
    Symbol_Add(ctx, Symbol_New(STR("FLOAT"),     SK_TYPE, Type_Float, NULL));
    Symbol_Add(ctx, Symbol_New(STR("FILE_TYPE"), SK_TYPE, Type_File, NULL));

    // Add boolean literals
    Symbol_Entry *sym_true  = Symbol_New(STR("TRUE"),  SK_ENUMERATION_LITERAL, Type_Boolean, NULL);
    sym_true->value = 1;
    Symbol_Add(ctx, sym_true);

    if (Type_Boolean->enum_literals.count >= Type_Boolean->enum_literals.capacity) {
        Type_Boolean->enum_literals.capacity = 8;
        Type_Boolean->enum_literals.data = malloc(8 * sizeof(Symbol_Entry *));
    }
    Type_Boolean->enum_literals.data[Type_Boolean->enum_literals.count++] = sym_true;

    Symbol_Entry *sym_false = Symbol_New(STR("FALSE"), SK_ENUMERATION_LITERAL, Type_Boolean, NULL);
    sym_false->value = 0;
    Symbol_Add(ctx, sym_false);
    Type_Boolean->enum_literals.data[Type_Boolean->enum_literals.count++] = sym_false;

    // Add predefined exceptions
    Symbol_Add(ctx, Symbol_New(STR("CONSTRAINT_ERROR"), SK_EXCEPTION, NULL, NULL));
    Symbol_Add(ctx, Symbol_New(STR("PROGRAM_ERROR"),    SK_EXCEPTION, NULL, NULL));
    Symbol_Add(ctx, Symbol_New(STR("STORAGE_ERROR"),    SK_EXCEPTION, NULL, NULL));
    Symbol_Add(ctx, Symbol_New(STR("TASKING_ERROR"),    SK_EXCEPTION, NULL, NULL));

    // Initialize I/O
    ctx->io_file_capacity = 8;
    ctx->io_files = malloc(8 * sizeof(FILE *));
    ctx->io_files[0] = stdin;
    ctx->io_files[1] = stdout;
    ctx->io_files[2] = stderr;
    ctx->io_file_count = 3;

} // Semantic_Init
