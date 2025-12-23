#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# Fix N_PD case - array parameters should be i64 ONLY if NOT OUT parameters
old_npd = 'fprintf(o,"declare void @\\"%s\\"(",nb);for(uint32_t i=0;i<sp->sp.pmm.n;i++){if(i)fprintf(o,",");No*p=sp->sp.pmm.d[i];Ty*pt=p->pm.ty?rst(g->sm,p->pm.ty):0;Vk k=VK_I;if(pt){Ty*ptc=tcc(pt);if(n->sy&&n->sy->ext&&ptc&&ptc->k==TY_A)k=VK_I;else k=tk2v(pt);}if(p->pm.md&2)fprintf(o,"ptr");else fprintf(o,"%s",vt(k));}fprintf(o,")\\n");}'

new_npd = 'fprintf(o,"declare void @\\"%s\\"(",nb);for(uint32_t i=0;i<sp->sp.pmm.n;i++){if(i)fprintf(o,",");No*p=sp->sp.pmm.d[i];Ty*pt=p->pm.ty?rst(g->sm,p->pm.ty):0;Vk k=VK_I;if(pt){Ty*ptc=tcc(pt);if(n->sy&&n->sy->ext&&ptc&&ptc->k==TY_A&&!(p->pm.md&2))k=VK_I;else k=tk2v(pt);}if(p->pm.md&2)fprintf(o,"ptr");else fprintf(o,"%s",vt(k));}fprintf(o,")\\n");}'

# Fix N_FD case - same logic
old_nfd = 'fprintf(o,"declare %s @\\"%s\\"(",vt(rk),nb);for(uint32_t i=0;i<sp->sp.pmm.n;i++){if(i)fprintf(o,",");No*p=sp->sp.pmm.d[i];Ty*pt=p->pm.ty?rst(g->sm,p->pm.ty):0;Vk k=VK_I;if(pt){Ty*ptc=tcc(pt);if(n->sy&&n->sy->ext&&ptc&&ptc->k==TY_A)k=VK_I;else k=tk2v(pt);}if(p->pm.md&2)fprintf(o,"ptr");else fprintf(o,"%s",vt(k));}fprintf(o,")\\n");}'

new_nfd = 'fprintf(o,"declare %s @\\"%s\\"(",vt(rk),nb);for(uint32_t i=0;i<sp->sp.pmm.n;i++){if(i)fprintf(o,",");No*p=sp->sp.pmm.d[i];Ty*pt=p->pm.ty?rst(g->sm,p->pm.ty):0;Vk k=VK_I;if(pt){Ty*ptc=tcc(pt);if(n->sy&&n->sy->ext&&ptc&&ptc->k==TY_A&&!(p->pm.md&2))k=VK_I;else k=tk2v(pt);}if(p->pm.md&2)fprintf(o,"ptr");else fprintf(o,"%s",vt(k));}fprintf(o,")\\n");}'

count = 0
if old_npd in content:
    print("Fixing N_PD case...")
    content = content.replace(old_npd, new_npd)
    count += 1

if old_nfd in content:
    print("Fixing N_FD case...")
    content = content.replace(old_nfd, new_nfd)
    count += 1

if count > 0:
    with open('/home/user/Ada83/ada83.c', 'w') as f:
        f.write(content)
    print(f"Done! Fixed {count} cases.")
else:
    print("Patterns not found")
