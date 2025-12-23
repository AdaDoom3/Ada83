#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# The current pattern (from v2) - calls vcast even for OUT parameters
old_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;int arid[64];Vk ark[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;No*arg=n->ct.arr.d[i];V av={0,VK_I};Vk ek=VK_I;if(pm&&pm->pm.ty){Ty*pt=rst(g->sm,pm->pm.ty);ek=tk2v(pt);if(pm->pm.md&2){ek=VK_P;if(arg->k==N_ID){Sy*as=arg->sy?arg->sy:syf(g->sm,arg->s);av.id=nt(g);av.k=VK_P;if(as&&as->lv>=0&&as->lv<g->sm->lv)fprintf(o,"  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\\n",av.id,as->lv,LC(arg->s));else fprintf(o,"  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\\n",av.id,LC(arg->s),as?as->sc:0,as?as->el:0);}else{av=gex(g,arg);int ap=nt(g);fprintf(o,"  %%t%d = alloca %s\\n",ap,vt(av.k));fprintf(o,"  store %s %%t%d, ptr %%t%d\\n",vt(av.k),av.id,ap);av.id=ap;av.k=VK_P;}}else{av=gex(g,arg);}}else{av=gex(g,arg);}V cv=vcast(g,av,ek);arid[i]=cv.id;ark[i]=ek;}'

# The new pattern - skip vcast for OUT parameters (use rf flag)
new_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;int arid[64];Vk ark[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;No*arg=n->ct.arr.d[i];V av={0,VK_I};Vk ek=VK_I;bool rf=false;if(pm&&pm->pm.ty){Ty*pt=rst(g->sm,pm->pm.ty);ek=tk2v(pt);if(pm->pm.md&2){ek=VK_P;rf=true;if(arg->k==N_ID){Sy*as=arg->sy?arg->sy:syf(g->sm,arg->s);av.id=nt(g);av.k=VK_P;if(as&&as->lv>=0&&as->lv<g->sm->lv)fprintf(o,"  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\\n",av.id,as->lv,LC(arg->s));else fprintf(o,"  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\\n",av.id,LC(arg->s),as?as->sc:0,as?as->el:0);}else{av=gex(g,arg);int ap=nt(g);fprintf(o,"  %%t%d = alloca %s\\n",ap,vt(av.k));fprintf(o,"  store %s %%t%d, ptr %%t%d\\n",vt(av.k),av.id,ap);av.id=ap;av.k=VK_P;}}else{av=gex(g,arg);}}else{av=gex(g,arg);}if(!rf){V cv=vcast(g,av,ek);av=cv;}arid[i]=av.id;ark[i]=ek;}'

# Check if pattern exists
if old_pattern in content:
    print("Found pattern, replacing...")
    content = content.replace(old_pattern, new_pattern)
    # Write back
    with open('/home/user/Ada83/ada83.c', 'w') as f:
        f.write(content)
    print("Done!")
else:
    print("Pattern not found - maybe pattern changed, let me check")
    # Check what's actually there
    idx = content.find('else if(s->k==4||s->k==5){')
    if idx >= 0:
        print("Current code:", content[idx:idx+800])
