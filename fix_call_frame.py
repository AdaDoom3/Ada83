#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# The old pattern: adds frame parameter unconditionally when s->lv>0
old_pattern = 'if(s->lv>0){if(n->ct.arr.n>0)fprintf(o,", ");if(s->lv>=g->sm->lv)fprintf(o,"ptr %%__frame");else fprintf(o,"ptr %%__slnk");}'

# The new pattern: only add frame parameter for non-PRAGMA IMPORT procedures
new_pattern = 'if(s->lv>0&&!s->ext){if(n->ct.arr.n>0)fprintf(o,", ");if(s->lv>=g->sm->lv)fprintf(o,"ptr %%__frame");else fprintf(o,"ptr %%__slnk");}'

# Check if pattern exists
if old_pattern in content:
    print("Found pattern, replacing...")
    content = content.replace(old_pattern, new_pattern)
    # Write back
    with open('/home/user/Ada83/ada83.c', 'w') as f:
        f.write(content)
    print("Done!")
else:
    print("Pattern not found")
