#!/usr/bin/env python3
"""
Comprehensive identifier expansion for Ada83 compiler - Third Iteration
Expands ALL remaining abbreviated names including struct fields, function parameters, and local variables.
This script must be applied very carefully to preserve code correctness.
"""

import re
import sys

# Phase 1: Expand struct field names
# These patterns match struct member access (-> or .) with specific contexts

def expand_struct_fields(code):
    """Expand abbreviated struct field names with context-aware matching."""

    # Unsigned_Big_Integer fields: d→digits, n→count, c→capacity, s→is_negative
    code = re.sub(r'(\w+)->d\b(?!\w)', r'\1->digits', code)
    code = re.sub(r'(\w+)->n\b(?!\w)', r'\1->count', code)
    code = re.sub(r'(\w+)->c\b(?!\w)', r'\1->capacity', code)
    code = re.sub(r'(\w+)->s\b(?!\w)', r'\1->is_negative', code)
    code = re.sub(r'(\w+)\.d\b(?!\w)', r'\1.digits', code)
    code = re.sub(r'(\w+)\.n\b(?!\w)', r'\1.count', code)
    code = re.sub(r'(\w+)\.c\b(?!\w)', r'\1.capacity', code)
    code = re.sub(r'(\w+)\.s\b(?!\w)', r'\1.is_negative', code)

    # Array_Bounds fields: lo→low_bound, hi→high_bound
    code = re.sub(r'->lo\b', '->low_bound', code)
    code = re.sub(r'->hi\b', '->high_bound', code)
    code = re.sub(r'\.lo\b', '.low_bound', code)
    code = re.sub(r'\.hi\b', '.high_bound', code)

    # Thread_Queue fields: m→mutex, c→condition, cnt→count, mx→maximum
    code = re.sub(r'->m\b(?=\s*[;,)])', '->mutex', code)
    code = re.sub(r'->cnt\b', '->count', code)
    code = re.sub(r'->mx\b', '->maximum', code)
    code = re.sub(r'\.m\b(?=\s*[;,)])', '.mutex', code)
    code = re.sub(r'\.cnt\b', '.count', code)
    code = re.sub(r'\.mx\b', '.maximum', code)

    # Thread_Args fields: t→thread, q→queue, d→data
    code = re.sub(r'->t\b(?=\s*[;,)])', '->thread', code)
    code = re.sub(r'->q\b(?=\s*[;,)])', '->queue', code)
    code = re.sub(r'\.t\b(?=\s*[;,)])', '.thread', code)
    code = re.sub(r'\.q\b(?=\s*[;,)])', '.queue', code)

    # Arena (A) fields: b→base, p→pointer, e→end
    code = re.sub(r'->b\b(?=\s*[;,)])', '->base', code)
    code = re.sub(r'->p\b(?=\s*[;,)])', '->pointer', code)
    code = re.sub(r'->e\b(?=\s*[;,)])', '->end', code)
    code = re.sub(r'\.b\b(?=\s*[;,)])', '.base', code)
    code = re.sub(r'\.p\b(?=\s*[;,)])', '.pointer', code)
    code = re.sub(r'\.e\b(?=\s*[;,)])', '.end', code)

    # String_Slice fields: s→string, n→length
    # (careful - conflicts with Unsigned_Big_Integer)
    code = re.sub(r'String_Slice\s+(\w+)\s*=\s*\{([^}]*?)\.s\b', r'String_Slice \1 = {\2.string', code)

    # Token fields: t→kind, l→location, lit→literal (already expanded),
    # iv→integer_value, fv→float_value, ui→unsigned_integer, ur→unsigned_rational
    code = re.sub(r'(\w+)->t\b(?=\s*[;,)=])', r'\1->kind', code)
    code = re.sub(r'(\w+)->l\b(?=\s*[;,)=.])', r'\1->location', code)
    code = re.sub(r'(\w+)->iv\b', r'\1->integer_value', code)
    code = re.sub(r'(\w+)->fv\b', r'\1->float_value', code)
    code = re.sub(r'(\w+)->ui\b', r'\1->unsigned_integer', code)
    code = re.sub(r'(\w+)->ur\b', r'\1->unsigned_rational', code)
    code = re.sub(r'(\w+)\.t\b(?=\s*[;,)=])', r'\1.kind', code)
    code = re.sub(r'(\w+)\.l\b(?=\s*[;,)=.])', r'\1.location', code)
    code = re.sub(r'(\w+)\.iv\b', r'\1.integer_value', code)
    code = re.sub(r'(\w+)\.fv\b', r'\1.float_value', code)

    # Lexer fields: s→start, c→current, e→end, ln→line_number, cl→column, f→filename, pt→previous_token
    code = re.sub(r'->ln\b', '->line_number', code)
    code = re.sub(r'->cl\b', '->column', code)
    code = re.sub(r'->pt\b', '->previous_token', code)
    code = re.sub(r'\.ln\b', '.line_number', code)
    code = re.sub(r'\.cl\b', '.column', code)
    code = re.sub(r'\.pt\b', '.previous_token', code)

    # Syntax_Node main fields: k→node_kind, l→source_location, ty→type_info, sy→symbol_ref
    code = re.sub(r'->k\b(?=\s*[;,)=])', '->node_kind', code)
    code = re.sub(r'->ty\b(?=\s*[;,)=.])', '->type_info', code)
    code = re.sub(r'->sy\b(?=\s*[;,)=.])', '->symbol_ref', code)
    code = re.sub(r'\.k\b(?=\s*[;,)=])', '.node_kind', code)
    code = re.sub(r'\.ty\b(?=\s*[;,)=.])', '.type_info', code)
    code = re.sub(r'\.sy\b(?=\s*[;,)=.])', '.symbol_ref', code)

    # LE (Label_Entry) fields: nm→name, bb→basic_block
    code = re.sub(r'->bb\b', '->basic_block', code)
    code = re.sub(r'\.bb\b', '.basic_block', code)

    # Parser fields: lx→lexer, cr→current_token, pk→peek_token, er→error_count, lb→label_stack
    code = re.sub(r'->lx\b', '->lexer', code)
    code = re.sub(r'->cr\b', '->current_token', code)
    code = re.sub(r'->pk\b', '->peek_token', code)
    code = re.sub(r'->er\b', '->error_count', code)
    code = re.sub(r'\.lx\b', '.lexer', code)
    code = re.sub(r'\.cr\b', '.current_token', code)
    code = re.sub(r'\.pk\b', '.peek_token', code)
    code = re.sub(r'\.er\b', '.error_count', code)

    # Type_Info fields - these are very common and need careful expansion
    # Tk_ k→type_kind, nm→name, bs→base_type, el→element_type, prt→parent_type,
    # ix→index_type, cm→components, dc→discriminants, sz→size, al→alignment,
    # ev→enum_values, rc→reference_constraints, ad→address, pk→is_packed,
    # ops→operations, sm→small, lg→large, sup→support, ctrl→controlled, frz→frozen, fzn→frozen_node
    code = re.sub(r'->bs\b', '->base_type', code)
    code = re.sub(r'->el\b(?=\s*[;,)=.])', '->element_type', code)
    code = re.sub(r'->prt\b', '->parent_type', code)
    code = re.sub(r'->ix\b(?=\s*[;,)=.])', '->index_type', code)
    code = re.sub(r'->cm\b', '->components', code)
    code = re.sub(r'->dc\b', '->discriminants', code)
    code = re.sub(r'->sz\b', '->size', code)
    code = re.sub(r'->al\b(?=\s*[;,)=])', '->alignment', code)
    code = re.sub(r'->ev\b', '->enum_values', code)
    code = re.sub(r'->rc\b', '->reference_constraints', code)
    code = re.sub(r'->ad\b', '->address', code)
    code = re.sub(r'->pk\b(?=\s*[;,)])', '->is_packed', code)
    code = re.sub(r'->ops\b', '->operations', code)
    code = re.sub(r'->sm\b', '->small', code)
    code = re.sub(r'->lg\b', '->large', code)
    code = re.sub(r'->sup\b', '->support', code)
    code = re.sub(r'->ctrl\b', '->controlled', code)
    code = re.sub(r'->frz\b', '->frozen', code)
    code = re.sub(r'->fzn\b', '->frozen_node', code)
    code = re.sub(r'\.bs\b', '.base_type', code)
    code = re.sub(r'\.el\b(?=\s*[;,)=.])', '.element_type', code)
    code = re.sub(r'\.prt\b', '.parent_type', code)
    code = re.sub(r'\.cm\b', '.components', code)
    code = re.sub(r'\.dc\b', '.discriminants', code)
    code = re.sub(r'\.sz\b', '.size', code)
    code = re.sub(r'\.al\b(?=\s*[;,)=])', '.alignment', code)

    # Symbol fields - another critical struct
    # k→kind, ty→type_info, df→definition, nx→next, pv→previous,
    # sc→scope, ss→scope_stack, vl→value, of→offset, ol→overloads, us→uses,
    # el→elaboration_level, gt→generic_type, pr→parent, lv→level, inl→is_inline,
    # shrd→is_shared, ext→is_external, ext_nm→external_name (already expanded),
    # ext_lang→external_language, mangled_nm→mangled_name, vis→visibility, hm→hash_map, uid→unique_id
    code = re.sub(r'->df\b', '->definition', code)
    code = re.sub(r'->nx\b', '->next', code)
    code = re.sub(r'->pv\b', '->previous', code)
    code = re.sub(r'->sc\b(?=\s*[;,)=])', '->scope', code)
    code = re.sub(r'->ss\b(?=\s*[;,)=])', '->scope_stack', code)
    code = re.sub(r'->vl\b', '->value', code)
    code = re.sub(r'->of\b(?=\s*[;,)=])', '->offset', code)
    code = re.sub(r'->ol\b', '->overloads', code)
    code = re.sub(r'->us\b', '->uses', code)
    code = re.sub(r'->gt\b(?=\s*[;,)=.])', '->generic_type', code)
    code = re.sub(r'->pr\b(?=\s*[;,)=.])', '->parent', code)
    code = re.sub(r'->lv\b(?=\s*[;,)=])', '->level', code)
    code = re.sub(r'->inl\b', '->is_inline', code)
    code = re.sub(r'->shrd\b', '->is_shared', code)
    code = re.sub(r'->ext\b(?=\s*[;,)=])', '->is_external', code)
    code = re.sub(r'->vis\b', '->visibility', code)
    code = re.sub(r'->hm\b', '->hash_map', code)
    code = re.sub(r'->uid\b', '->unique_id', code)
    code = re.sub(r'\.df\b', '.definition', code)
    code = re.sub(r'\.nx\b', '.next', code)
    code = re.sub(r'\.sc\b(?=\s*[;,)=])', '.scope', code)
    code = re.sub(r'\.ss\b(?=\s*[;,)=])', '.scope_stack', code)

    # Symbol_Manager fields
    # sy→symbols, uv→use_vector, eo→error_offset, lu→label_uses, eb→error_buffers,
    # ed→error_depth, ce→current_exception, fn→file_number, lb→label_stack,
    # ib→import_buffer, sst→subscope_stack, ssd→subscope_depth,
    # dps→deferred_package_specs, dpn→deferred_package_number, ex→exports,
    # uv_vis→use_vector_visibility, eh→error_handlers, ap→active_packages, uid_ctr→unique_id_counter
    code = re.sub(r'->eo\b', '->error_offset', code)
    code = re.sub(r'->lu\b(?=\s*[;,)=.])', '->label_uses', code)
    code = re.sub(r'->eb\b', '->error_buffers', code)
    code = re.sub(r'->ed\b', '->error_depth', code)
    code = re.sub(r'->ce\b', '->current_exception', code)
    code = re.sub(r'->fn\b(?=\s*[;,)=])', '->file_number', code)
    code = re.sub(r'->ib\b', '->import_buffer', code)
    code = re.sub(r'->sst\b', '->subscope_stack', code)
    code = re.sub(r'->ssd\b', '->subscope_depth', code)
    code = re.sub(r'->dps\b', '->deferred_package_specs', code)
    code = re.sub(r'->dpn\b', '->deferred_package_number', code)
    code = re.sub(r'->ex\b(?=\s*[;,)=.])', '->exports', code)
    code = re.sub(r'->uv_vis\b', '->use_vector_visibility', code)
    code = re.sub(r'->eh\b', '->error_handlers', code)
    code = re.sub(r'->ap\b', '->active_packages', code)
    code = re.sub(r'->uid_ctr\b', '->unique_id_counter', code)
    code = re.sub(r'\.eo\b', '.error_offset', code)
    code = re.sub(r'\.fn\b(?=\s*[;,)=])', '.file_number', code)

    return code

def expand_more_function_names(code):
    """Expand remaining abbreviated function names."""

    # Remaining short function names found in the code
    mappings = {
        r'\bpdc\(': 'parse_declarations(',
        r'\bphd\(': 'parse_handlers(',
        r'\bps\((?![a-z])': 'parse_subprogram_spec(',  # Be careful not to match 'psomething'
        r'\bpgf\(': 'parse_generic_formal(',
        r'\bprc\(': 'parse_record_component(',
        r'\bsyh\(': 'symbol_hash(',
        r'\bsyn\(': 'symbol_new(',
        r'\bmt\(': 'make_token(',
        r'\bav\(': 'advance_char(',
        r'\bsw\(': 'skip_whitespace(',
        r'\bkl\(': 'keyword_lookup(',
        r'\bln\((?!ot)': 'lexer_new(',  # Don't match 'lnot'
    }

    for pattern, replacement in mappings.items():
        code = re.sub(pattern, replacement, code)

    return code

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 expand_all_identifiers.py ada83.c", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r') as f:
        code = f.read()

    print("Phase 1: Expanding struct field names...", file=sys.stderr)
    code = expand_struct_fields(code)

    print("Phase 2: Expanding remaining function names...", file=sys.stderr)
    code = expand_more_function_names(code)

    print(code)

if __name__ == '__main__':
    main()
