#!/usr/bin/env python3
"""
Fix all field accesses after struct field name expansion.
This script updates all references to struct fields that were renamed.
"""

import re
import sys

def fix_field_accesses(code):
    """Fix all struct field accesses with expanded names."""

    # Unsigned_Big_Integer: d→digits, n→count, c→capacity, s→is_negative
    code = re.sub(r'->d\b', '->digits', code)
    code = re.sub(r'\.d\b', '.digits', code)
    code = re.sub(r'->n\b', '->count', code)
    code = re.sub(r'\.n\b', '.count', code)
    code = re.sub(r'->c\b', '->capacity', code)
    code = re.sub(r'\.c\b', '.capacity', code)
    code = re.sub(r'->s\b', '->is_negative', code)
    code = re.sub(r'\.s\b', '.is_negative', code)

    # Rational_Number: n→numerator, d→denominator
    # (conflicts with above, but will get caught)

    # Fat_Pointer: d→data, b→bounds
    # (conflicts with above, will get caught)

    # Array_Bounds: lo→low_bound, hi→high_bound
    code = re.sub(r'->lo\b', '->low_bound', code)
    code = re.sub(r'\.lo\b', '.low_bound', code)
    code = re.sub(r'->hi\b', '->high_bound', code)
    code = re.sub(r'\.hi\b', '.high_bound', code)

    # Thread_Queue: m→mutex, c→condition, cnt→count, mx→maximum
    code = re.sub(r'->m\b', '->mutex', code)
    code = re.sub(r'\.m\b', '.mutex', code)
    code = re.sub(r'->cnt\b', '->count', code)
    code = re.sub(r'\.cnt\b', '.count', code)
    code = re.sub(r'->mx\b', '->maximum', code)
    code = re.sub(r'\.mx\b', '.maximum', code)

    # Thread_Args: t→thread, q→queue, d→data
    code = re.sub(r'->t\b', '->thread', code)
    code = re.sub(r'\.t\b', '.thread', code)
    code = re.sub(r'->q\b', '->queue', code)
    code = re.sub(r'\.q\b', '.queue', code)

    # Arena_Allocator: b→base, p→pointer, e→end
    code = re.sub(r'->b\b', '->base', code)
    code = re.sub(r'\.b\b', '.base', code)
    code = re.sub(r'->p\b', '->pointer', code)
    code = re.sub(r'\.p\b', '.pointer', code)
    code = re.sub(r'->e\b', '->end', code)
    code = re.sub(r'\.e\b', '.end', code)

    # String_Slice: s→string, n→length
    code = re.sub(r'->s\b', '->string', code)
    code = re.sub(r'->n\b', '->length', code)

    # Source_Location: l→line, c→column, f→filename
    code = re.sub(r'->l\b', '->line', code)
    code = re.sub(r'\.l\b', '.line', code)
    code = re.sub(r'->f\b', '->filename', code)
    code = re.sub(r'\.f\b', '.filename', code)

    # Token: t→kind, l→location, iv→integer_value, fv→float_value, ui→unsigned_integer, ur→unsigned_rational
    code = re.sub(r'->iv\b', '->integer_value', code)
    code = re.sub(r'\.iv\b', '.integer_value', code)
    code = re.sub(r'->fv\b', '->float_value', code)
    code = re.sub(r'\.fv\b', '.float_value', code)
    code = re.sub(r'->ui\b', '->unsigned_integer', code)
    code = re.sub(r'\.ui\b', '.unsigned_integer', code)
    code = re.sub(r'->ur\b', '->unsigned_rational', code)
    code = re.sub(r'\.ur\b', '.unsigned_rational', code)
    code = re.sub(r'->lit\b', '->literal', code)
    code = re.sub(r'\.lit\b', '.literal', code)

    # Lexer: s→start, c→current, e→end, ln→line_number, cl→column, f→filename, pt→previous_token
    code = re.sub(r'->ln\b', '->line_number', code)
    code = re.sub(r'\.ln\b', '.line_number', code)
    code = re.sub(r'->cl\b', '->column', code)
    code = re.sub(r'\.cl\b', '.column', code)
    code = re.sub(r'->pt\b', '->previous_token', code)
    code = re.sub(r'\.pt\b', '.previous_token', code)

    # KW: k→keyword, t→token_kind

    # LE: nm→name, bb→basic_block
    code = re.sub(r'->nm\b', '->name', code)
    code = re.sub(r'\.nm\b', '.name', code)
    code = re.sub(r'->bb\b', '->basic_block', code)
    code = re.sub(r'\.bb\b', '.basic_block', code)

    # Vector types: d→data, n→count, c→capacity (already handled above)

    # LU: k→kind, nm→name, pth→path, sp→spec, bd→body, wth→with_list, elb→elaborates, ts→timestamp, cmpl→complete
    code = re.sub(r'->pth\b', '->path', code)
    code = re.sub(r'\.pth\b', '.path', code)
    code = re.sub(r'->sp\b', '->spec', code)
    code = re.sub(r'\.sp\b', '.spec', code)
    code = re.sub(r'->bd\b', '->body', code)
    code = re.sub(r'\.bd\b', '.body', code)
    code = re.sub(r'->wth\b', '->with_list', code)
    code = re.sub(r'\.wth\b', '.with_list', code)
    code = re.sub(r'->elb\b', '->elaborates', code)
    code = re.sub(r'\.elb\b', '.elaborates', code)
    code = re.sub(r'->ts\b', '->timestamp', code)
    code = re.sub(r'\.ts\b', '.timestamp', code)
    code = re.sub(r'->cmpl\b', '->complete', code)
    code = re.sub(r'\.cmpl\b', '.complete', code)

    # GT: nm→name, fp→formal_parameters, dc→declarations, un→unit, bd→body
    code = re.sub(r'->fp\b', '->formal_parameters', code)
    code = re.sub(r'\.fp\b', '.formal_parameters', code)
    code = re.sub(r'->dc\b', '->declarations', code)
    code = re.sub(r'\.dc\b', '.declarations', code)
    code = re.sub(r'->un\b', '->unit', code)
    code = re.sub(r'\.un\b', '.unit', code)

    # RC: k→kind, ty→type_info, and nested unions
    code = re.sub(r'->ty\b', '->type_info', code)
    code = re.sub(r'\.ty\b', '.type_info', code)
    code = re.sub(r'\.er\b', '.enum_rep', code)
    code = re.sub(r'\.ad\b', '.address_clause', code)
    code = re.sub(r'\.rr\b', '.record_rep', code)
    code = re.sub(r'\.im\b', '.import_clause', code)
    code = re.sub(r'->po\b', '->position', code)
    code = re.sub(r'\.po\b', '.position', code)
    code = re.sub(r'->cp\b', '->components', code)
    code = re.sub(r'\.cp\b', '.components', code)
    code = re.sub(r'->lang\b', '->language', code)
    code = re.sub(r'\.lang\b', '.language', code)
    code = re.sub(r'->ext\b', '->external_name', code)
    code = re.sub(r'\.ext\b', '.external_name', code)

    # Parser: lx→lexer, cr→current_token, pk→peek_token, er→error_count, lb→label_stack
    code = re.sub(r'->lx\b', '->lexer', code)
    code = re.sub(r'\.lx\b', '.lexer', code)
    code = re.sub(r'->cr\b', '->current_token', code)
    code = re.sub(r'\.cr\b', '.current_token', code)
    code = re.sub(r'->pk\b', '->peek_token', code)
    code = re.sub(r'\.pk\b', '.peek_token', code)
    code = re.sub(r'->er\b', '->error_count', code)
    code = re.sub(r'\.er\b', '.error_count', code)
    code = re.sub(r'->lb\b', '->label_stack', code)
    code = re.sub(r'\.lb\b', '.label_stack', code)

    # Common pattern: k→kind for many structs
    code = re.sub(r'->k\b', '->kind', code)
    code = re.sub(r'\.k\b', '.kind', code)

    return code

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 fix_field_accesses.py ada83.c", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r') as f:
        code = f.read()

    print("Fixing field accesses...", file=sys.stderr)
    code = fix_field_accesses(code)

    print(code)

if __name__ == '__main__':
    main()
