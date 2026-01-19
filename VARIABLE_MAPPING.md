# Ada83 Compiler - Comprehensive Variable and Function Name Mapping

## Overview
This document provides a complete mapping of abbreviated variable and function names in ada83.c to their fully expanded forms. The mappings are organized by section (types, lexer, parser, semantic, codegen) to facilitate systematic refactoring.

---

## 1. TYPE SYSTEM & DATA STRUCTURES

### Type_Info Structure Fields
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| k | kind | Type_Info member | Type category (enum TYPE_*) |
| ty | type_info | Syntax_Node member, function params | Resolved type after analysis |
| nm | name | Type_Info member | Type name identifier |
| sz | size | Type_Info member | Size in bytes |
| lo | low_bound | Type_Info member | Range lower bound |
| hi | high_bound | Type_Info member | Range upper bound |
| et | element_type | Type_Info member | Array element type |
| ix | index_type | Type_Info member | Array index type |
| dt | designated_type | Type_Info member | Access type target |
| pt | parent_type | Type_Info member | Derived type parent |
| bt | base_type | Type_Info member | Base type for subtypes |
| rt | return_type | Type_Info/Syntax_Node | Function return type |
| at | array_type | Local variables | Array type reference |
| tc | type_canonical | Local variables | Canonical type |

### Syntax_Node Structure Fields
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| k | kind | Syntax_Node member | Node_Kind enum value |
| ty | type_info | Syntax_Node member | Semantic type annotation |
| nm | name | Various node structs | Identifier name |
| sp | subprogram_spec | body, package_body members | Subprogram specification |
| pk | package_spec | Syntax_Node member | Package specification |
| op | operator | binary_node, unary_node | Token_Kind operator |
| ag | aggregate | qualified member | Aggregate expression |
| in | initializer | component_decl, object_decl | Initial value expression |
| po | position_offset | er member in Representation_Clause | Enumeration rep position |
| ad | address_clause | Representation_Clause variant | Address specification |
| rr | record_rep | Representation_Clause variant | Record representation |
| im | import_clause | Representation_Clause variant | Import/Interface spec |

### Symbol Structure Fields
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| k | kind | Symbol member | Entity kind (variable, type, etc.) |
| nm | name | Symbol member | Symbol identifier |
| ty | type_info | Symbol member | Symbol's type |
| df | definition | Symbol member | AST node of declaration |
| sc | scope | Symbol member | Scope nesting level |
| ss | storage_size | Symbol member | Size in bytes |
| eo | elaboration_order | Symbol member | Elaboration sequence number |
| lv | level | Symbol member | Nesting level |
| pn | parent | Symbol member | Enclosing package/procedure |

---

## 2. LEXER SECTION

### Lexer Structure Fields
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| src | source | Lexer field | Source code pointer (start) |
| inf | input_file | Main function | Input filename |
| src | start | Lexer member | Start of source buffer |
| ch | character | Lexer functions | Current character |

### Lexer Function Parameters
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| l | lexer | Function params | Lexer pointer |
| s | string/slice | Function params | String_Slice or source |
| c | character/count | Local variables | Character or count value |
| i | index | Loop variables | Array/string index |
| j | index_secondary | Loop variables | Secondary index |
| n | length/count | Local variables | Length or count value |
| p | pointer | Local variables | Pointer to data |
| h | hash | Local variables | Hash value |
| b | buffer | Local variables | Buffer pointer |

### Token Kind Abbreviations (T_*)
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| T_LP | T_LEFT_PAREN | Token_Kind | ( |
| T_RP | T_RIGHT_PAREN | Token_Kind | ) |
| T_LB | T_LEFT_BRACKET | Token_Kind | [ |
| T_RB | T_RIGHT_BRACKET | Token_Kind | ] |
| T_CM | T_COMMA | Token_Kind | , |
| T_DT | T_DOT | Token_Kind | . |
| T_SC | T_SEMICOLON | Token_Kind | ; |
| T_CL | T_COLON | Token_Kind | : |
| T_TK | T_TICK | Token_Kind | ' (attribute) |
| T_AS | T_ASSIGN | Token_Kind | := |
| T_AR | T_ARROW | Token_Kind | => |
| T_DD | T_DOT_DOT | Token_Kind | .. |
| T_LL | T_LEFT_LABEL | Token_Kind | << |
| T_GG | T_RIGHT_LABEL | Token_Kind | >> |
| T_BX | T_BOX | Token_Kind | <> |
| T_BR | T_BAR | Token_Kind | \| |
| T_PL | T_PLUS | Token_Kind | + |
| T_MN | T_MINUS | Token_Kind | - |
| T_ST | T_STAR | Token_Kind | * |
| T_SL | T_SLASH | Token_Kind | / |
| T_AM | T_AMPERSAND | Token_Kind | & |
| T_EX | T_EXPONENT | Token_Kind | ** |

---

## 3. PARSER SECTION

### Parser Structure Fields
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| p | parser | Function params | Parser pointer |
| pp | parser_pointer | Local variables | Parser double pointer |

### Parser Function Parameters
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| sp | subprogram_spec | Parse functions | Subprogram specification node |
| ch | choice | Parse functions | Case/aggregate choice |
| tn | type_name | Parse functions | Type name identifier |
| dt | discriminant_type | Parse functions | Discriminant type spec |
| fp | formal_parameters | Parse functions | Formal parameter list |
| ap | actual_parameters | Parse functions | Actual parameter list |
| dc | declarations | Parse functions | Declaration list |
| st | statements | Parse functions | Statement list |

### Node Kind Abbreviations (N_*)
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| N_AG | N_AGGREGATE | Node_Kind | Aggregate expression |
| N_BIN | N_BINARY | Node_Kind | Binary operation |
| N_UN | N_UNARY | Node_Kind | Unary operation |
| N_AT | N_ATTRIBUTE | Node_Kind | Attribute reference |
| N_QL | N_QUALIFIED | Node_Kind | Qualified expression |
| N_CL | N_CALL | Node_Kind | Function/procedure call |
| N_IX | N_INDEX | Node_Kind | Array indexing |
| N_SL | N_SLICE | Node_Kind | Array slice |
| N_SEL | N_SELECTED | Node_Kind | Selected component |
| N_ALC | N_ALLOCATOR | Node_Kind | new expression |
| N_AS | N_ASSIGNMENT | Node_Kind | Assignment statement |
| N_IF | N_IF_STATEMENT | Node_Kind | If statement |
| N_CS | N_CASE_STATEMENT | Node_Kind | Case statement |
| N_LP | N_LOOP | Node_Kind | Loop statement |
| N_BL | N_BLOCK | Node_Kind | Block statement |
| N_EX | N_EXIT | Node_Kind | Exit statement |
| N_RT | N_RETURN | Node_Kind | Return statement |
| N_GT | N_GOTO | Node_Kind | Goto statement |
| N_RS | N_RAISE | Node_Kind | Raise statement |
| N_OD | N_OBJECT_DECL | Node_Kind | Object declaration |
| N_TD | N_TYPE_DECL | Node_Kind | Type declaration |
| N_SD | N_SUBTYPE_DECL | Node_Kind | Subtype declaration |
| N_ED | N_EXCEPTION_DECL | Node_Kind | Exception declaration |
| N_PM | N_PARAM | Node_Kind | Parameter specification |
| N_PS | N_PROC_SPEC | Node_Kind | Procedure specification |
| N_FS | N_FUNC_SPEC | Node_Kind | Function specification |
| N_PB | N_PROC_BODY | Node_Kind | Procedure body |
| N_FB | N_FUNC_BODY | Node_Kind | Function body |
| N_PKS | N_PACKAGE_SPEC | Node_Kind | Package specification |
| N_PKB | N_PACKAGE_BODY | Node_Kind | Package body |
| N_TKS | N_TASK_SPEC | Node_Kind | Task specification |
| N_TKB | N_TASK_BODY | Node_Kind | Task body |
| N_ENT | N_ENTRY | Node_Kind | Entry declaration |
| N_ACC | N_ACCEPT | Node_Kind | Accept statement |
| N_DRF | N_DEREFERENCE | Node_Kind | .all dereference |
| N_CVT | N_CONVERSION | Node_Kind | Type conversion |
| N_CHK | N_CHECK | Node_Kind | Runtime check node |
| N_DRV | N_DERIVED | Node_Kind | Derived type |
| N_LBL | N_LABEL | Node_Kind | Statement label |

---

## 4. SEMANTIC ANALYSIS SECTION

### Symbol_Manager Structure Fields
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| sy | symbol_table | Symbol_Manager member | Hash table of symbols [4096] |
| sc | scope | Symbol_Manager member | Current scope level |
| ss | storage_size | Symbol_Manager member | Current storage size |
| ds | declaration_spec | Symbol_Manager member | Current declaration node |
| pk | package | Symbol_Manager member | Current package node |
| uv | use_vector | Symbol_Manager member | Use clause symbols |
| eo | elaboration_order | Symbol_Manager member | Elaboration counter |
| lu | library_units | Symbol_Manager member | Library unit vector |
| gt | generic_templates | Symbol_Manager member | Generic template vector |
| eb | exception_buffers | Symbol_Manager member | Exception buffer stack [16] |
| ed | exception_depth | Symbol_Manager member | Exception stack depth |
| ce | current_exception | Symbol_Manager member | Current exception names [16] |
| io | input_output | Symbol_Manager member | File vector for I/O |
| fn | file_number | Symbol_Manager member | File counter |
| lb | label_buffer | Symbol_Manager member | Label name vector |
| lv | level | Symbol_Manager member | Nesting level |
| ib | init_blocks | Symbol_Manager member | Initialization blocks |
| sst | symbol_stack_table | Symbol_Manager member | Symbol stack [256] |
| ssd | symbol_stack_depth | Symbol_Manager member | Symbol stack depth |
| dps | deferred_packages | Symbol_Manager member | Deferred package symbols [256] |
| dpn | deferred_package_count | Symbol_Manager member | Deferred package counter |
| ex | exceptions | Symbol_Manager member | Exception symbol vector |
| uv_vis | use_visibility | Symbol_Manager member | Use clause visibility bitmap [64] |
| eh | exception_handlers | Symbol_Manager member | Exception handler names |
| ap | actual_params | Symbol_Manager member | Actual parameter names |
| ps | procedure_stack | Symbol_Manager member | Enclosing procedure stack [256] |
| pn | procedure_nesting | Symbol_Manager member | Procedure nesting count |

### Semantic Function Parameters
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| sm | symbol_manager | Function params | Symbol_Manager pointer |
| s | symbol | Function params | Symbol pointer |
| t | type_info | Function params | Type_Info pointer |
| n | node | Function params | Syntax_Node pointer |
| tx | expected_type | Function params | Expected/target type |
| a | type_a | Type comparison | First type to compare |
| b | type_b | Type comparison | Second type to compare |
| f | formal | Parameter matching | Formal parameter |
| c | candidate | Overload resolution | Candidate symbol |
| cv | candidates_vector | Local variables | Candidate symbol vector |
| msc | max_scope | Local variables | Maximum scope level |
| br | best_result | Local variables | Best match symbol |
| bs | best_score | Local variables | Best match score |

---

## 5. CODE GENERATION SECTION

### Code_Generator Structure Fields
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| o | output | Code_Generator member | Output FILE pointer |
| tm | temporary | Code_Generator member | Temporary register counter |
| lb | label_block | Code_Generator member | Label block counter |
| md | metadata | Code_Generator member | Metadata counter |
| sm | symbol_manager | Code_Generator member | Symbol_Manager pointer |
| ll | label_list | Code_Generator member | Label list [64] |
| ls | label_stack | Code_Generator member | Label stack depth |
| el | exception_list | Code_Generator member | Exception symbol vector |
| eh | exception_handler | Code_Generator member | Exception_Handler pointer |
| tk | tasks | Code_Generator member | Task array [64] |
| tn | task_number | Code_Generator member | Task counter |
| pt | protected_types | Code_Generator member | Protected_Type array [64] |
| pn | protected_number | Code_Generator member | Protected type counter |
| lbs | labels | Code_Generator member | Label name vector |
| exs | exceptions | Code_Generator member | Exception name vector |
| dcl | declarations | Code_Generator member | Declaration name vector |
| ltb | label_table | Code_Generator member | Label_Entry vector |
| lopt | loop_optimizations | Code_Generator member | Loop optimization flags [64] |

### Code Generation Function Parameters
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| g | generator | Function params | Code_Generator pointer |
| o | output | Local variables | FILE pointer |
| v | value | Local variables | Value/register ID |
| r | result | Local variables | Result register/value |
| l | label | Local variables | Label block ID |
| fp | fat_pointer | Local variables | Fat pointer register |
| d | data_pointer | Local variables | Data pointer register |
| lo | low_bound | Local variables | Lower bound register |
| hi | high_bound | Local variables | Upper bound register |
| sz | size | Local variables | Size value |
| ix | index | Local variables | Index register |
| n | node | Function params | Syntax_Node pointer |

### Exception_Handler Structure
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| ec | exception_choice | Exception_Handler member | Exception name |
| jb | jump_buffer | Exception_Handler member | setjmp/longjmp buffer |
| nx | next | Exception_Handler member | Next handler in chain |

### Task Structure
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| th | thread | Task member | pthread_t thread handle |
| id | identifier | Task member | Task ID number |
| nm | name | Task member | Task name |
| en | entries | Task member | Entry declarations |
| pt | protected_type | Task member | Associated protected type |
| ac | active | Task member | Task active flag |
| tm | terminated | Task member | Task terminated flag |

### Protected_Type Structure
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| mx | mutex | Protected_Type member | pthread_mutex_t lock |
| nm | name | Protected_Type member | Protected type name |
| en | entries | Protected_Type member | Entry declarations |
| lk | locked | Protected_Type member | Lock state flag |

---

## 6. MAIN FUNCTION & COMPILATION UNIT

### Main Function Variables
| Original | Expanded | Context | Notes |
|----------|----------|---------|-------|
| ac | argc | main params | Argument count |
| av | argv | main params | Argument vector |
| inf | input_filename | Local variables | Input file path |
| cu | compilation_unit | Local variables | Root AST node |
| sm | symbol_manager | Local variables | Symbol manager instance |
| g | generator | Local variables | Code generator instance |
| of | output_filename | Local variables | Output file path |
| o | output | Local variables | Output FILE pointer |
| pth | path | Local variables | File path buffer |
| pb | path_buffer | Local variables | Path construction buffer |
| ln | library_name | Local variables | Library name (lowercase) |
| sd | source_directory | Local variables | Source directory path |
| dl | directory_length | Local variables | Directory path length |
| ld | loaded | Local variables | Library loaded flag |

---

## 7. COMMON LOCAL VARIABLE PATTERNS

### Single-Letter Variables by Context
| Letter | Common Expansions | Typical Contexts |
|--------|-------------------|------------------|
| a | operand_a, type_a, argument_a | Binary operations, comparisons |
| b | operand_b, type_b, argument_b, buffer | Binary operations, buffers |
| c | count, character, candidate, carry | Loops, lexer, overload resolution |
| d | data, discriminant, denominator | Pointers, type info |
| e | element, entry, exception | Arrays, tasks, error handling |
| f | file, formal, field, found | File I/O, parameters, flags |
| g | generator, generic | Code generation |
| h | hash, handler, high | Hash tables, exceptions |
| i | index, integer | Loop counters, primary index |
| j | index_secondary | Nested loops, secondary index |
| k | kind | Node/type/symbol kind enums |
| l | lexer, label, length, low | Lexer context, labels, bounds |
| m | max, mantissa, middle | Maximums, numeric parsing |
| n | node, count, name | AST nodes, counts |
| o | output | File output |
| p | parser, pointer, parameter | Parsing, pointers |
| r | result, register, range | Return values, registers |
| s | symbol, string, source, start | Symbols, strings |
| t | type_info, token, temp | Type system, tokens |
| u | unsigned, unit | Numbers, compilation units |
| v | value, vector | Values, vectors |
| w | with_clause | With clauses |
| x | expected, exception | Expected types |
| y | yielded | Results |
| z | zone | Allocations |

### Two-Letter Common Patterns
| Original | Expanded | Common Context |
|----------|----------|----------------|
| nm | name | Identifiers throughout |
| ty | type_info | Type references |
| sz | size | Size calculations |
| lo | low_bound | Range lower bounds |
| hi | high_bound | Range upper bounds |
| fp | formal_parameters / fat_pointer | Parameters / arrays |
| ap | actual_parameters | Call arguments |
| dc | declarations | Declaration lists |
| st | statements | Statement lists |
| sp | subprogram_spec | Subprogram specifications |
| pk | package | Package nodes |
| cu | compilation_unit | Top-level AST |
| sm | symbol_manager | Symbol table manager |
| df | definition | Symbol definitions |
| sc | scope | Scope levels |
| eo | elaboration_order | Elaboration sequence |
| lv | level | Nesting levels |
| ix | index | Array indices |
| cv | candidates_vector | Overload candidates |
| br | best_result | Best match |
| bs | best_score | Match score |
| nb | name_buffer | Name construction |
| pb | path_buffer | Path construction |

---

## 8. FUNCTION NAME PATTERNS

### Parse Functions
| Original Pattern | Expanded Pattern | Example |
|-----------------|------------------|---------|
| parse_* | parse_* | parse_expression, parse_statement |
| parser_* | parser_* | parser_expect, parser_match |

### Resolution Functions
| Original Pattern | Expanded Pattern | Example |
|-----------------|------------------|---------|
| resolve_* | resolve_* | resolve_expression, resolve_declaration |
| find_* | find_* | find_symbol, find_type |
| symbol_find* | symbol_find* | symbol_find_use, symbol_find_with_arity |
| type_* | type_* | type_scope, type_covers |

### Generation Functions
| Original Pattern | Expanded Pattern | Example |
|-----------------|------------------|---------|
| generate_* | generate_* | generate_expression, generate_statement |
| emit_* | emit_* | emit_label, emit_exception |
| encode_* | encode_* | encode_symbol_name |

### Utility Functions
| Original Pattern | Expanded Pattern | Example |
|-----------------|------------------|---------|
| new_* | new_* | new_temporary_register, new_label_block |
| add_* | add_* | add_declaration |
| get_* | get_* | get_or_create_label_basic_block |
| has_* | has_* | has_nested_function, has_return_statement |
| is_* | is_* | is_check_suppressed, is_compile_valid |

---

## 9. REFACTORING RECOMMENDATIONS

### Phase 1: Type System & Core Structures (Low Risk)
**Order**: Refactor these first as they're referenced throughout
- Type_Info fields: k→kind, nm→name, sz→size, lo→low_bound, hi→high_bound
- Syntax_Node fields: k→kind, ty→type_info
- Symbol fields: k→kind, nm→name, ty→type_info, df→definition
- **Rationale**: These are structural changes with clear search/replace patterns

### Phase 2: Symbol_Manager Fields (Medium Risk)
**Order**: Refactor after Phase 1 types are stable
- Symbol_Manager: sy→symbol_table, sc→scope, ss→storage_size, eo→elaboration_order
- Continue with: lv→level, lb→label_buffer, ib→init_blocks, ex→exceptions
- **Rationale**: Used heavily in semantic analysis, but mostly in isolated functions

### Phase 3: Code_Generator Fields (Medium Risk)
**Order**: Refactor after Symbol_Manager is done
- Code_Generator: o→output, tm→temporary, lb→label_block, md→metadata
- Continue with: sm→symbol_manager, tk→tasks, tn→task_number, pt→protected_types
- **Rationale**: Code generation is relatively isolated from other phases

### Phase 4: Token and Node Kind Enums (Low Risk)
**Order**: Can be done anytime, preferably after Phase 1-3
- Token_Kind: T_LP→T_LEFT_PAREN, T_RP→T_RIGHT_PAREN, etc.
- Node_Kind: N_AG→N_AGGREGATE, N_BIN→N_BINARY, etc.
- **Rationale**: These are enum values with clear scoping

### Phase 5: Local Variables (High Risk - Optional)
**Order**: Last, if at all - consider leaving as-is
- Function-local variables: i, j, k, n, p, s, t, etc.
- **Rationale**: High churn, low benefit - many single-letter variables are idiomatic in C

### Refactoring Strategy by Risk Level

#### Low Risk (Safe to refactor anytime)
- Structure field names with unique prefixes
- Enum value names
- Global/static variable names
- Function names (with careful testing)

#### Medium Risk (Refactor with caution)
- Structure fields with common names (nm, ty, k)
- Parameter names in complex functions
- Variables used across multiple scopes

#### High Risk (Consider leaving as-is)
- Single-letter loop counters (i, j, k)
- Common short names in small scopes (n, p, s, t)
- Variables in densely packed algorithms (bigint operations)

### Recommended Refactoring Tools
1. **Search & Replace with Context**: Use editor features that show context
2. **Semantic-Aware Refactoring**: Use tools that understand C scoping rules
3. **Git Branches**: Create a branch per phase
4. **Comprehensive Testing**: Run full test suite after each phase
5. **Incremental Commits**: Commit after each logical group of changes

### Anti-Patterns to Avoid
- Don't refactor everything at once
- Don't change names that are idiomatic (i for index, n for count in small scopes)
- Don't make names excessively long (balance clarity with brevity)
- Don't break consistency within a module (if one function uses short names, don't mix)

---

## 10. NAMING CONVENTIONS TO FOLLOW

### Structure Members
- Use full words: `kind` not `k`, `name` not `nm`
- Exception: Well-known abbreviations in context (e.g., `type_info` → `ty` is acceptable in very local scope)

### Function Parameters
- Use descriptive names for public APIs: `symbol_manager`, `node`, `type_info`
- Short names acceptable in private functions with small scopes
- Be consistent within a function family (all parse_* functions use similar parameter names)

### Local Variables
- Loop indices: `i`, `j`, `k` are fine
- Single-use temporaries: short names acceptable if scope is <10 lines
- Long-lived locals: use descriptive names

### Enumerations
- Use full words separated by underscores
- Type prefixes: `TYPE_*`, `NODE_*`, `TOKEN_*`
- Consistency: All related enums follow same pattern

---

## Document Maintenance
This mapping should be updated whenever:
- New abbreviated names are introduced
- Refactoring phases are completed (mark as DONE)
- Naming conventions change
- New patterns are discovered

**Last Updated**: 2026-01-19
**Status**: Initial mapping complete, refactoring not yet started
