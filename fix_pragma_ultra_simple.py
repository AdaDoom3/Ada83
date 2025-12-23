#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# Get the current pattern (after all my previous fixes)
old_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;if(!sp&&s->ty&&s->ty->ops.n>0){sp=s->ty->ops.d[0]->bd.sp;}int arid[64];Vk ark[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;No*arg=n->ct.arr.d[i];V av={0,VK_I};Vk ek=VK_I;if(pm&&pm->pm.ty){Ty*pt=rst(g->sm,pm->pm.ty);ek=tk2v(pt);if(pm->pm.md&2)ek=VK_P;}if(s->ext&&ek==VK_P&&arg->k==N_ID){Sy*as=arg->sy?arg->sy:syf(g->sm,arg->s);av.id=nt(g);av.k=VK_P;if(as&&as->lv>=0&&as->lv<g->sm->lv)fprintf(o,"  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\\n",av.id,as->lv,LC(arg->s));else fprintf(o,"  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\\n",av.id,LC(arg->s),as?as->sc:0,as?as->el:0);}else{av=gex(g,arg);if(ek==VK_P){int ap=nt(g);fprintf(o,"  %%t%d = alloca %s\\n",ap,vt(av.k));fprintf(o,"  store %s %%t%d, ptr %%t%d\\n",vt(av.k),av.id,ap);av.id=ap;av.k=VK_P;}else{V cv=vcast(g,av,ek);av=cv;}}arid[i]=av.id;ark[i]=ek;}'

# Ultra-simple: just check declaration to see if parameter should be ptr
new_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;if(!sp&&s->ty&&s->ty->ops.n>0){sp=s->ty->ops.d[0]->bd.sp;}int arid[64];Vk ark[64];int arpt[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;arpt[i]=pm&&(pm->pm.md&2)?1:0;}for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*arg=n->ct.arr.d[i];V av={0,VK_I};Vk ek=VK_I;if(arpt[i]&&arg->k==N_ID){ek=VK_P;Sy*as=arg->sy?arg->sy:syf(g->sm,arg->s);av.id=nt(g);av.k=VK_P;if(as&&as->lv>=0&&as->lv<g->sm->lv)fprintf(o,"  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\\n",av.id,as->lv,LC(arg->s));else fprintf(o,"  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\\n",av.id,LC(arg->s),as?as->sc:0,as?as->el:0);}else{av=gex(g,arg);V cv=vcast(g,av,ek);av=cv;}arid[i]=av.id;ark[i]=ek;}'

if old_pattern in content:
    print("Applying ultra-simple fix...")
    content = content.replace(old_pattern, new_pattern)
    with open('/home/user/Ada83/ada83.c', 'w') as f:
        f.write(content)
    print("Done! Now using pre-scan to identify OUT parameters")
else:
    print("Pattern not found")
