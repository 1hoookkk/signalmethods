import json, math
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
SR=39062.5; OFF=0.45
GRID=np.logspace(math.log10(40),math.log10(18000),600); W=2*np.pi*GRID/SR
def part(hz,r):
    if not r or r<=0: return np.zeros_like(W)
    th=2*np.pi*hz/SR
    return 10*np.log10(np.maximum((1-2*r*np.cos(th)*np.cos(W)+r*r*np.cos(2*W))**2+(2*r*np.cos(th)*np.sin(W)-r*r*np.sin(2*W))**2,1e-30))
def sec_db(p,z): return part(z['hz'],z['r'])-part(p['hz'],p['r'])
cubes=json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
stages=[[] for _ in range(7)]; sums=[]
for cu in cubes:
    for c in cu['corners']:
        secs=c['sections'][:7]
        if not any(s['pole']['r']>OFF or s['zero']['r']>OFF for s in secs): continue
        tot=np.zeros_like(GRID)
        for i,s in enumerate(secs):
            cv=sec_db(s['pole'],s['zero'])
            stages[i].append(cv); tot=tot+cv
        sums.append(tot)
fig,axes=plt.subplots(2,4,figsize=(20,9),facecolor='white')
for i in range(7):
    a=axes[i//4][i%4]
    arr=np.array(stages[i])
    for cv in arr[::4]: a.semilogx(GRID,cv,color='#1f77b4',lw=.4,alpha=.06)
    a.semilogx(GRID,np.median(arr,axis=0),color='#d62728',lw=2.4)
    a.semilogx(GRID,np.percentile(arr,10,axis=0),color='#d62728',lw=1,ls=':')
    a.semilogx(GRID,np.percentile(arr,90,axis=0),color='#d62728',lw=1,ls=':')
    a.set_title(f'S{i+1}  ({len(arr)} corners)'); a.set_ylim(-70,50); a.grid(alpha=.25); a.set_xlabel('Hz'); a.set_ylabel('dB')
a=axes[1][3]; arr=np.array(sums)
for cv in arr[::4]: a.semilogx(GRID,cv,color='#2ca02c',lw=.4,alpha=.07)
a.semilogx(GRID,np.median(arr,axis=0),color='#000',lw=2.4)
a.set_title(f'whole cascade sum ({len(arr)} corners)'); a.set_ylim(-70,50); a.grid(alpha=.25); a.set_xlabel('Hz')
plt.suptitle('Morpheus 289 cubes — every non-empty corner, per stage. red = median, dotted = 10/90th percentile',fontsize=13)
plt.tight_layout(); plt.savefig('plots/cube_stage_curves.png',dpi=105)
print('corners:',len(sums))
