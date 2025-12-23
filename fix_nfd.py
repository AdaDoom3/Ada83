#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# The old pattern in N_FD case (near the end where it declares parameters)
old_pattern = 'fprintf(o,"declare %s @\\"%s\\"(",vt(rk),nb);for(uint32_t i=0;i<sp->sp.pmm.n;i++){if(i)fprintf(o,",");No*p=sp->sp.pmm.d[i];Vk k=p->pm.ty?tk2v(rst(g->sm,p->pm.ty)):VK_I;if(p->pm.md&2)fprintf(o,"ptr");else fprintf(o,"%s",vt(k));}fprintf(o,")\\n");}break;case N_BL:'

# The new pattern with array type check for PRAGMA IMPORT
new_pattern = 'fprintf(o,"declare %s @\\"%s\\"(",vt(rk),nb);for(uint32_t i=0;i<sp->sp.pmm.n;i++){if(i)fprintf(o,",");No*p=sp->sp.pmm.d[i];Ty*pt=p->pm.ty?rst(g->sm,p->pm.ty):0;Vk k=VK_I;if(pt){Ty*ptc=tcc(pt);if(n->sy&&n->sy->ext&&ptc&&ptc->k==TY_A)k=VK_I;else k=tk2v(pt);}if(p->pm.md&2)fprintf(o,"ptr");else fprintf(o,"%s",vt(k));}fprintf(o,")\\n");}break;case N_BL:'

# Check if pattern exists
if old_pattern in content:
    print("Found N_FD pattern, replacing...")
    content = content.replace(old_pattern, new_pattern)
    # Write back
    with open('/home/user/Ada83/ada83.c', 'w') as f:
        f.write(content)
    print("Done!")
else:
    print("Pattern not found in N_FD case")
