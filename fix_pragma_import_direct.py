#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# Find the entire second branch and replace it with a simpler version
# that handles PRAGMA IMPORT specially

old_start = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;if(!sp&&s->ty&&s->ty->ops.n>0){sp=s->ty->ops.d[0]->bd.sp;}int arid[64];Vk ark[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;No*arg=n->ct.arr.d[i];V av={0,VK_I};Vk ek=VK_I;bool rf=false;if(pm&&pm->pm.ty){Ty*pt=rst(g->sm,pm->pm.ty);ek=tk2v(pt);if(pm->pm.md&2){ek=VK_P;rf=true;if(arg->k==N_ID){Sy*as=arg->sy?arg->sy:syf(g->sm,arg->s);av.id=nt(g);av.k=VK_P;if(as&&as->lv>=0&&as->lv<g->sm->lv)fprintf(o,"  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\\n",av.id,as->lv,LC(arg->s));else fprintf(o,"  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\\n",av.id,LC(arg->s),as?as->sc:0,as?as->el:0);}else{av=gex(g,arg);int ap=nt(g);fprintf(o,"  %%t%d = alloca %s\\n",ap,vt(av.k));fprintf(o,"  store %s %%t%d, ptr %%t%d\\n",vt(av.k),av.id,ap);av.id=ap;av.k=VK_P;}}else{av=gex(g,arg);}}else{av=gex(g,arg);}if(!rf){V cv=vcast(g,av,ek);av=cv;}arid[i]=av.id;ark[i]=ek;}'

# New simpler version that gets parameter info from declarations
new_start = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;if(!sp&&s->ty&&s->ty->ops.n>0){sp=s->ty->ops.d[0]->bd.sp;}int arid[64];Vk ark[64];for(uint32_t i=0;i<n->ct.arr.n&&i<64;i++){No*pm=sp&&i<sp->sp.pmm.n?sp->sp.pmm.d[i]:0;No*arg=n->ct.arr.d[i];V av={0,VK_I};Vk ek=VK_I;if(pm&&pm->pm.ty){Ty*pt=rst(g->sm,pm->pm.ty);ek=tk2v(pt);if(pm->pm.md&2)ek=VK_P;}if(s->ext&&ek==VK_P&&arg->k==N_ID){Sy*as=arg->sy?arg->sy:syf(g->sm,arg->s);av.id=nt(g);av.k=VK_P;if(as&&as->lv>=0&&as->lv<g->sm->lv)fprintf(o,"  %%t%d = bitcast ptr %%lnk.%d.%s to ptr\\n",av.id,as->lv,LC(arg->s));else fprintf(o,"  %%t%d = bitcast ptr %%v.%s.sc%u.%u to ptr\\n",av.id,LC(arg->s),as?as->sc:0,as?as->el:0);}else{av=gex(g,arg);if(ek==VK_P){int ap=nt(g);fprintf(o,"  %%t%d = alloca %s\\n",ap,vt(av.k));fprintf(o,"  store %s %%t%d, ptr %%t%d\\n",vt(av.k),av.id,ap);av.id=ap;av.k=VK_P;}else{V cv=vcast(g,av,ek);av=cv;}}arid[i]=av.id;ark[i]=ek;}'

if old_start in content:
    print("Replacing with simplified PRAGMA IMPORT handling...")
    content = content.replace(old_start, new_start)
    with open('/home/user/Ada83/ada83.c', 'w') as f:
        f.write(content)
    print("Done!")
else:
    print("Pattern not found - code may have changed")
