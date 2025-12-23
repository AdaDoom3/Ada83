#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# The old pattern in the else if(s->k==4||s->k==5) branch
# This doesn't check for OUT parameters
old_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;int arid[64];Vk ark[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;V av=gex(g,n->ct.arr.d[i]);Vk ek=VK_I;if(pm&&pm->pm.ty){Ty*pt=rst(g->sm,pm->pm.ty);ek=tk2v(pt);}V cv=vcast(g,av,ek);arid[i]=cv.id;ark[i]=ek;}'

# The new pattern checks for OUT parameters and doesn't convert them
new_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;int arid[64];Vk ark[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;V av=gex(g,n->ct.arr.d[i]);Vk ek=VK_I;if(pm&&pm->pm.ty){Ty*pt=rst(g->sm,pm->pm.ty);if(pm->pm.md&2)ek=VK_P;else ek=tk2v(pt);}V cv=vcast(g,av,ek);arid[i]=cv.id;ark[i]=ek;}'

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
