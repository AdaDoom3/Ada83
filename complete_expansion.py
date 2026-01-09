#!/usr/bin/env python3
"""
Complete identifier expansion - does ALL struct definitions and field accesses in one pass.
"""

import re

def expand_all(code):
    # Phase 1: Expand ALL struct definitions

    # Unsigned_Big_Integer
    code = re.sub(
        r'typedef struct\s*\{\s*uint64_t \*d;\s*uint32_t n, c;\s*bool s;\s*\} Unsigned_Big_Integer;',
        'typedef struct\n{\n  uint64_t *digits;\n  uint32_t count, capacity;\n  bool is_negative;\n} Unsigned_Big_Integer;',
        code, flags=re.DOTALL
    )

    # Rational_Number
    code = re.sub(
        r'typedef struct\s*\{\s*Unsigned_Big_Integer \*n, \*d;\s*\} Rational_Number;',
        'typedef struct\n{\n  Unsigned_Big_Integer *numerator, *denominator;\n} Rational_Number;',
        code, flags=re.DOTALL
    )

    # Fat_Pointer
    code = re.sub(
        r'typedef struct\s*\{\s*void \*d;\s*void \*b;\s*\} Fat_Pointer;',
        'typedef struct\n{\n  void *data;\n  void *bounds;\n} Fat_Pointer;',
        code, flags=re.DOTALL
    )

    # Array_Bounds
    code = re.sub(
        r'typedef struct\s*\{\s*int64_t lo, hi;\s*\} Array_Bounds;',
        'typedef struct\n{\n  int64_t low_bound, high_bound;\n} Array_Bounds;',
        code, flags=re.DOTALL
    )

    # Thread_Queue
    code = re.sub(
        r'typedef struct\s*\{\s*pthread_mutex_t m;\s*pthread_cond_t c;\s*int cnt, mx;\s*\} Thread_Queue;',
        'typedef struct\n{\n  pthread_mutex_t mutex;\n  pthread_cond_t condition;\n  int count, maximum;\n} Thread_Queue;',
        code, flags=re.DOTALL
    )

    # Thread_Args
    code = re.sub(
        r'typedef struct\s*\{\s*pthread_t t;\s*Thread_Queue \*q;\s*void \*d;\s*\} Thread_Args;',
        'typedef struct\n{\n  pthread_t thread;\n  Thread_Queue *queue;\n  void *data;\n} Thread_Args;',
        code, flags=re.DOTALL
    )

    # Arena (A -> Arena_Allocator)
    code = re.sub(
        r'typedef struct\s*\{\s*char \*b, \*p, \*e;\s*\} A;',
        'typedef struct\n{\n  char *base, *pointer, *end;\n} Arena_Allocator;',
        code, flags=re.DOTALL
    )
    code = re.sub(r'\bA\b(?!\w)', 'Arena_Allocator', code)  # Replace standalone A

    # String_Slice
    code = re.sub(
        r'typedef struct\s*\{\s*const char \*s;\s*uint32_t n;\s*\} String_Slice;',
        'typedef struct\n{\n  const char *string;\n  uint32_t length;\n} String_Slice;',
        code, flags=re.DOTALL
    )

    # Source_Location
    code = re.sub(
        r'typedef struct\s*\{\s*uint32_t l, c;\s*const char \*f;\s*\} Source_Location;',
        'typedef struct\n{\n  uint32_t line, column;\n  const char *filename;\n} Source_Location;',
        code, flags=re.DOTALL
    )

    # Token
    code = re.sub(
        r'(typedef struct\s*\{\s*Token_Kind )t;(\s*Source_Location )l;(\s*String_Slice lit;\s*int64_t )iv;(\s*double )fv;(\s*Unsigned_Big_Integer \*)ui;(\s*Rational_Number \*)ur;(\s*\} Token;)',
        r'\1kind;\2location;\3integer_value;\4float_value;\5unsigned_integer;\6unsigned_rational;\7',
        code, flags=re.DOTALL
    )

    # Lexer
    code = re.sub(
        r'(typedef struct\s*\{\s*const char \*)s, \*c, \*e;(\s*uint32_t )ln, cl;(\s*const char \*)f;(\s*Token_Kind )pt;(\s*\} Lexer;)',
        r'\1start, *current, *end;\2line_number, column;\3filename;\4previous_token;\5',
        code, flags=re.DOTALL
    )

    # KW struct
    code = re.sub(r'String_Slice k;(\s*)Token_Kind t;(\s*\} KW)', r'String_Slice keyword;\1Token_Kind token_kind;\2', code)

    # LE
    code = re.sub(r'(struct LE\s*\{\s*String_Slice )nm;(\s*int )bb;', r'\1name;\2basic_block;', code, flags=re.DOTALL)

    # Vector structs
    for vec_name in ['Node_Vector', 'Symbol_Vector', 'RV', 'LV', 'GV', 'FV', 'SLV', 'LEV']:
        code = re.sub(
            rf'(typedef struct\s*\{{[^}}]*)\*\*d;(\s*uint32_t )n, c;(\s*\}} {vec_name};)',
            r'\1**data;\2count, capacity;\3',
            code, flags=re.DOTALL
        )

    # VECPUSH macro
    code = re.sub(
        r'(#define VECPUSH.*?if \(v->)n( >= v->)c\)',
        r'\1count\2capacity)',
        code, flags=re.DOTALL
    )
    code = re.sub(r'(v->)c( = v->)c( \? v->)c( << 1 : 8;)', r'\1capacity\2capacity\3capacity\4', code)
    code = re.sub(r'(v->)d( = realloc\(v->)d(, v->)c', r'\1data\2data\3capacity', code)
    code = re.sub(r'(v->)d\[(v->)n\+\+\]', r'\1data[\2count++]', code)

    # Parser
    code = re.sub(
        r'(typedef struct\s*\{\s*Lexer )lx;(\s*Token )cr, pk;(\s*int )er;(\s*SLV )lb;(\s*\} Parser;)',
        r'\1lexer;\2current_token, peek_token;\3error_count;\4label_stack;\5',
        code, flags=re.DOTALL
    )

    # LU
    code = re.sub(r'(struct LU\s*\{\s*uint8_t )k;', r'\1kind;', code)
    code = re.sub(r'(String_Slice )nm;(\s*String_Slice )pth;', r'\1name;\2path;', code)
    code = re.sub(r'(Syntax_Node \*)sp;(\s*Syntax_Node \*)bd;(\s*LV )wth;(\s*LV )elb;',
                  r'\1spec;\2body;\3with_list;\4elaborates;', code)
    code = re.sub(r'(uint64_t )ts;(\s*bool )cmpl;', r'\1timestamp;\2complete;', code)

    # GT
    code = re.sub(r'(struct GT\s*\{[^}]*Node_Vector )fp;([^}]*Node_Vector )dc;([^}]*Syntax_Node \*)un;([^}]*Syntax_Node \*)bd;',
                  r'\1formal_parameters;\2declarations;\3unit;\4body;', code, flags=re.DOTALL)

    # RC
    code = re.sub(r'(struct RC\s*\{\s*uint8_t )k;(\s*Type_Info \*)ty;', r'\1kind;\2type_info;', code)

    # Phase 2: Fix ALL field accesses throughout the file

    # Very specific patterns first
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

    # Lexer
    code = re.sub(r'->ln\b', '->line_number', code)
    code = re.sub(r'\.ln\b', '.line_number', code)
    code = re.sub(r'->cl\b', '->column', code)
    code = re.sub(r'\.cl\b', '.column', code)
    code = re.sub(r'->pt\b', '->previous_token', code)
    code = re.sub(r'\.pt\b', '.previous_token', code)

    # Parser
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

    # Thread
    code = re.sub(r'->cnt\b', '->count', code)
    code = re.sub(r'\.cnt\b', '.count', code)
    code = re.sub(r'->mx\b', '->maximum', code)
    code = re.sub(r'\.mx\b', '.maximum', code)

    # Generic single letters - be aggressive now
    code = re.sub(r'->k\b', '->kind', code)
    code = re.sub(r'\.k\b', '.kind', code)
    code = re.sub(r'->nm\b', '->name', code)
    code = re.sub(r'\.nm\b', '.name', code)
    code = re.sub(r'->bb\b', '->basic_block', code)
    code = re.sub(r'\.bb\b', '.basic_block', code)
    code = re.sub(r'->ty\b', '->type_info', code)
    code = re.sub(r'\.ty\b', '.type_info', code)

    # String_Slice specifically
    code = re.sub(r'(\w+)\.s\b(?!t|p|i|y)', r'\1.string', code)
    code = re.sub(r'(\w+)->s\b(?!t|p|i|y)', r'\1->string', code)

    # Generic patterns for vectors/bigints/arena
    code = re.sub(r'->n\b', '->count', code)
    code = re.sub(r'\.n\b', '.count', code)
    code = re.sub(r'->c\b', '->capacity', code)
    code = re.sub(r'\.c\b', '.capacity', code)
    code = re.sub(r'->d\b', '->data', code)
    code = re.sub(r'\.d\b', '.data', code)
    code = re.sub(r'->b\b', '->base', code)
    code = re.sub(r'\.b\b', '.base', code)
    code = re.sub(r'->p\b', '->pointer', code)
    code = re.sub(r'\.p\b', '.pointer', code)
    code = re.sub(r'->e\b', '->end', code)
    code = re.sub(r'\.e\b', '.end', code)
    code = re.sub(r'->m\b', '->mutex', code)
    code = re.sub(r'\.m\b', '.mutex', code)
    code = re.sub(r'->q\b', '->queue', code)
    code = re.sub(r'\.q\b', '.queue', code)
    code = re.sub(r'->t\b', '->thread', code)
    code = re.sub(r'\.t\b', '.thread', code)
    code = re.sub(r'->l\b', '->location', code)
    code = re.sub(r'\.l\b', '.location', code)
    code = re.sub(r'->f\b', '->filename', code)
    code = re.sub(r'\.f\b', '.filename', code)

    # Fix Unsigned_Big_Integer fields specifically in UNSIGNED_BIGINT_NORMALIZE macro
    code = re.sub(r'\(u\)->count', '(u)->count', code)
    code = re.sub(r'\(u\)->digits', '(u)->digits', code)
    code = re.sub(r'\(u\)->is_negative', '(u)->is_negative', code)

    return code

# Read, process, write
with open('ada83.c', 'r') as f:
    code = f.read()

print("Applying complete expansion...")
code = expand_all(code)

with open('ada83.c', 'w') as f:
    f.write(code)

print("Done!")
