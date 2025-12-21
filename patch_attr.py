#!/usr/bin/env python3
import re

with open('/home/user/Ada83/ada83.c.bak', 'r') as f:
    code = f.read()

attr_sem = open('/home/user/Ada83/attr_patch.txt').read().strip()
attr_gen = open('/home/user/Ada83/attr_codegen.txt').read().strip()

pat1 = r'case N_AT:rex\(SM,n->at\.p,0\);for\(uint32_t i=0;i<n->at\.ar\.n;i\+\+\)rex\(SM,n->at\.ar\.d\[i\],0\);if\(si\(n->at\.at,Z\("LENGTH"\)\)\|\|si\(n->at\.at,Z\("SIZE"\)\)\)n->ty=TY_INT;else if\(si\(n->at\.at,Z\("FIRST"\)\)\|\|si\(n->at\.at,Z\("LAST"\)\)\)\{Ty\*pt=n->at\.p->ty\?tcc\(n->at\.p->ty\):0;n->ty=pt&&pt->el\?pt->el:TY_INT;\}else if\(si\(n->at\.at,Z\("ADDRESS"\)\)\)n->ty=TY_INT;else if\(si\(n->at\.at,Z\("COUNT"\)\)\)n->ty=TY_INT;else if\(si\(n->at\.at,Z\("CALLABLE"\)\)\)n->ty=TY_BOOL;else if\(si\(n->at\.at,Z\("TERMINATED"\)\)\)n->ty=TY_BOOL;else if\(si\(n->at\.at,Z\("ACCESS"\)\)\)n->ty=tyn\(TY_AC,N\);else n->ty=TY_INT;break;'

pat2 = r'case N_AT:\{if\(si\(n->at\.at,Z\("ADDRESS"\)\)\)\{V p=gex\(g,n->at\.p\);p=vcast\(g,p,VK_P\);r\.k=VK_I;fprintf\(g->o,"  %%t%d = ptrtoint ptr %%t%d to i64\\n",r\.id,p\.id\);\}else if\(si\(n->at\.at,Z\("SIZE"\)\)\)\{Ty\*t=n->at\.p\?tcc\(n->at\.p->ty\):0;int64_t sz=8;if\(t\)sz=t->sz\*8;r\.k=VK_I;fprintf\(g->o,"  %%t%d = add i64 0, %lld\\n",r\.id,\(long long\)sz\);\}else if\(si\(n->at\.at,Z\("FIRST"\)\)\|\|si\(n->at\.at,Z\("LAST"\)\)\|\|si\(n->at\.at,Z\("LENGTH"\)\)\)\{Ty\*t=n->at\.p\?tcc\(n->at\.p->ty\):0;int64_t lo=0,hi=-1;if\(t&&t->k==TY_A\)\{lo=t->lo;hi=t->hi;\}else if\(t&&\(t->k==TY_I\|\|t->k==TY_UI\|\|t->k==TY_E\)\)\{lo=t->lo;hi=t->hi;\}int64_t v=0;if\(si\(n->at\.at,Z\("FIRST"\)\)\)v=lo;else if\(si\(n->at\.at,Z\("LAST"\)\)\)v=hi;else v=hi>=lo\?\(hi-lo\+1\):0;r\.k=VK_I;fprintf\(g->o,"  %%t%d = add i64 0, %lld\\n",r\.id,\(long long\)v\);\}else if\(si\(n->at\.at,Z\("COUNT"\)\)\|\|si\(n->at\.at,Z\("CALLABLE"\)\)\|\|si\(n->at\.at,Z\("TERMINATED"\)\)\)\{r\.k=VK_I;fprintf\(g->o,"  %%t%d = add i64 0, 0\\n",r\.id\);\}else if\(si\(n->at\.at,Z\("ACCESS"\)\)\)\{V p=gex\(g,n->at\.p\);r\.k=VK_P;fprintf\(g->o,"  %%t%d = bitcast ptr %%t%d to ptr\\n",r\.id,p\.id\);\}else\{r\.k=VK_I;fprintf\(g->o,"  %%t%d = add i64 0, 8\\n",r\.id\);\}break;\}'

code = re.sub(pat1, attr_sem, code, count=1)
code = re.sub(pat2, attr_gen, code, count=1)

with open('/home/user/Ada83/ada83.c', 'w') as f:
    f.write(code)

print("Patched successfully!")
