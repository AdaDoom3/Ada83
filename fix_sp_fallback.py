#!/usr/bin/env python3

# Read the file
with open('/home/user/Ada83/ada83.c', 'r') as f:
    content = f.read()

# The current pattern that only tries one way to get sp
old_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;'

# New pattern with fallback to get sp from type if first method fails
new_pattern = 'else if(s->k==4||s->k==5){No*sp=s->ol.n>0&&(s->ol.d[0]->k==N_PD||s->ol.d[0]->k==N_FD)?s->ol.d[0]->bd.sp:0;if(!sp&&s->ty&&s->ty->ops.n>0){sp=s->ty->ops.d[0]->bd.sp;}'

if old_pattern in content:
    print("Adding sp fallback...")
    content = content.replace(old_pattern, new_pattern)
    with open('/home/user/Ada83/ada83.c', 'w') as f:
        f.write(content)
    print("Done!")
else:
    print("Pattern not found")
