#!/usr/bin/env python3
"""
Comprehensive field access fixer - applies ALL struct field expansions
throughout the entire ada83.c file.
"""

import re
import sys

def apply_all_fixes(code):
    """Apply all field access fixes in careful order to avoid conflicts."""

    # First pass: Fix very specific patterns that might conflict
    # Token fields - before generic single-letter fixes
    code = re.sub(r'\.ui\b', '.unsigned_integer', code)
    code = re.sub(r'->ui\b', '->unsigned_integer', code)
    code = re.sub(r'\.ur\b', '.unsigned_rational', code)
    code = re.sub(r'->ur\b', '->unsigned_rational', code)
    code = re.sub(r'\.iv\b', '.integer_value', code)
    code = re.sub(r'->iv\b', '->integer_value', code)
    code = re.sub(r'\.fv\b', '.float_value', code)
    code = re.sub(r'->fv\b', '->float_value', code)
    code = re.sub(r'\.lit\b', '.literal', code)
    code = re.sub(r'->lit\b', '->literal', code)

    # Lexer-specific before generic
    code = re.sub(r'->ln\b', '->line_number', code)
    code = re.sub(r'\.ln\b', '.line_number', code)
    code = re.sub(r'->cl\b', '->column', code)
    code = re.sub(r'\.cl\b', '.column', code)
    code = re.sub(r'->pt\b', '->previous_token', code)
    code = re.sub(r'\.pt\b', '.previous_token', code)

    # Parser-specific
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

    # Array_Bounds
    code = re.sub(r'->lo\b', '->low_bound', code)
    code = re.sub(r'\.lo\b', '.low_bound', code)
    code = re.sub(r'->hi\b', '->high_bound', code)
    code = re.sub(r'\.hi\b', '.high_bound', code)

    # Thread types
    code = re.sub(r'->cnt\b', '->count', code)
    code = re.sub(r'\.cnt\b', '.count', code)
    code = re.sub(r'->mx\b', '->maximum', code)
    code = re.sub(r'\.mx\b', '.maximum', code)

    # LU struct
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

    # GT struct
    code = re.sub(r'->fp\b', '->formal_parameters', code)
    code = re.sub(r'\.fp\b', '.formal_parameters', code)
    code = re.sub(r'->dc\b', '->declarations', code)
    code = re.sub(r'\.dc\b', '.declarations', code)
    code = re.sub(r'->un\b', '->unit', code)
    code = re.sub(r'\.un\b', '.unit', code)

    # RC nested struct fields
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

    # Generic single-letter fields (after specific ones)
    # These are very common and need careful handling

    # k -> kind (very common across many structs)
    code = re.sub(r'->k\b', '->kind', code)
    code = re.sub(r'\.k\b', '.kind', code)

    # nm -> name (very common)
    code = re.sub(r'->nm\b', '->name', code)
    code = re.sub(r'\.nm\b', '.name', code)

    # bb -> basic_block
    code = re.sub(r'->bb\b', '->basic_block', code)
    code = re.sub(r'\.bb\b', '.basic_block', code)

    # ty -> type_info
    code = re.sub(r'->ty\b', '->type_info', code)
    code = re.sub(r'\.ty\b', '.type_info', code)

    # l -> location or line (context dependent, but Source_Location.l is always line)
    # For Token.l it's location, for Source_Location it's line
    # Let's be careful and only do Source_Location context
    code = re.sub(r'(\w+)\.l\.l\b', r'\1.location.line', code)  # Token.location.line
    code = re.sub(r'(\w+)\.l\.c\b', r'\1.location.column', code)  # Token.location.column
    code = re.sub(r'(\w+)\.l\.f\b', r'\1.location.filename', code)  # Token.location.filename

    # Now fix standalone patterns
    code = re.sub(r'->l\b(?!ocation|ine|ength|abel|anguage)', '->location', code)
    code = re.sub(r'\.l\b(?!ocation|ine|ength|abel|anguage)', '.location', code)

    # f -> filename
    code = re.sub(r'->f\b(?!ile|loat|ormal)', '->filename', code)
    code = re.sub(r'\.f\b(?!ile|loat|ormal)', '.filename', code)

    # t -> kind (for Token.t) or thread (for Thread_Args.t) - already handled above mostly
    code = re.sub(r'->t\b(?!ype|ime|hread|oken)', '->kind', code)
    code = re.sub(r'\.t\b(?!ype|ime|hread|oken)', '.kind', code)

    # Generic arena/slice/vector patterns
    # s -> string (String_Slice), start (Lexer), is_negative (already done for bigint)
    code = re.sub(r'->s\b(?!t|pec|tring|ize|cope)', '->string', code)

    # Generic n, c, d for vectors and Unsigned_Big_Integer (already mostly done)
    # But we need to catchremaining ones outside the bigint section
    code = re.sub(r'->n\b(?!ame|ext|ot)', '->count', code)
    code = re.sub(r'\.n\b(?!ame|ext|ot)', '.count', code)
    code = re.sub(r'->c\b(?!o|u|a)', '->capacity', code)
    code = re.sub(r'\.c\b(?!o|u|a)', '.capacity', code)
    code = re.sub(r'->d\b(?!a|e|i)', '->data', code)
    code = re.sub(r'\.d\b(?!a|e|i)', '.data', code)

    # Arena fields
    code = re.sub(r'->b\b(?!a|o|y)', '->base', code)
    code = re.sub(r'\.b\b(?!a|o|y)', '.base', code)
    code = re.sub(r'->p\b(?!a|o|r)', '->pointer', code)
    code = re.sub(r'\.p\b(?!a|o|r)', '.pointer', code)
    code = re.sub(r'->e\b(?!n|l|r|x)', '->end', code)
    code = re.sub(r'\.e\b(?!n|l|r|x)', '.end', code)

    # Thread fields
    code = re.sub(r'->m\b(?!a|u|o)', '->mutex', code)
    code = re.sub(r'\.m\b(?!a|u|o)', '.mutex', code)
    code = re.sub(r'->q\b(?!u)', '->queue', code)
    code = re.sub(r'\.q\b(?!u)', '.queue', code)

    return code

def main():
    with open('ada83.c', 'r') as f:
        code = f.read()

    print("Applying comprehensive field access fixes...", file=sys.stderr)
    code = apply_all_fixes(code)

    with open('ada83.c', 'w') as f:
        f.write(code)

    print("Done! Now testing compilation...", file=sys.stderr)

if __name__ == '__main__':
    main()
