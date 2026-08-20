import json, math, glob
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
SR=39062.5
GRID=np.logspace(math.log10(40),math.log10(18000),600); W=2*np.pi*GRID/SR
def part(hz,r):
    if not r or r<=0: return np.zeros_like(W)
    th=2*np.pi*hz/SR
    return 10*np.log10(np.maximum((1-2*r*np.cos(th)*np.cos(W)+r*r*np.cos(2*W))**2+(2*r*np.cos(th)*np.sin(W)-r*r*np.sin(2*W))**2,1e-30))
def sec_db(p,z): return part(z.get('hz',0),z.get('r',0))-part(p.get('hz',0),p.get('r',0))

stages=[[] for _ in range(7)]; sums=[]
for f in sorted(glob.glob('recipes/architectures/*.json')):
    r=json.load(open(f))
    for cn in sorted(r['sections'][0]['corners'].keys()):
        tot=np.zeros_like(GRID); any_=False
        for i,sec in enumerate(r['sections'][:7]):
            c=sec['corners'].get(cn)
            if not c: continue
            p=c.get('pole') or {}; z=c.get('zero') or {}
            if 'pair' in p or 'pair' in z: continue
            cv=sec_db(p,z)
            stages[i].append(cv); tot=tot+cv; any_=True
        if any_: sums.append(tot)

fig,axes=plt.subplots(2,4,figsize=(20,9),facecolor='white')
for i in range(7):
    a=axes[i//4][i%4]
    for cv in stages[i]: a.semilogx(GRID,cv,color='#1f77b4',lw=.5,alpha=.14)
    if stages[i]:
        a.semilogx(GRID,np.median(np.array(stages[i]),axis=0),color='#d62728',lw=2.4)
    a.set_title(f'S{i+1}  ({len(stages[i])} corners)'); a.set_ylim(-70,50); a.grid(alpha=.25)
    a.set_xlabel('Hz'); a.set_ylabel('dB')
a=axes[1][3]
for cv in sums: a.semilogx(GRID,cv,color='#2ca02c',lw=.5,alpha=.18)
a.semilogx(GRID,np.median(np.array(sums),axis=0),color='#000',lw=2.4)
a.set_title(f'whole cascade sum ({len(sums)} corners)'); a.set_ylim(-70,50); a.grid(alpha=.25); a.set_xlabel('Hz')
plt.suptitle('P2K vocal corpus — every corner, per stage. thin = one corner, red = median',fontsize=13)
plt.tight_layout(); plt.savefig('plots/p2k_stage_curves.png',dpi=105)
print('corners:',len(sums),'per-stage counts:',[len(s) for s in stages])
