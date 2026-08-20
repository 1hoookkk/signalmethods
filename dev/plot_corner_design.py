import json, math, collections
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
cuts=json.load(open('ref/stage_vocabulary.json'))['cuts']; SR=39062.5; OFF=cuts['root_off_r']
GRID=np.logspace(math.log10(40),math.log10(18000),512); W=2*np.pi*GRID/SR
def part(hz,r):
    if r<=0: return np.zeros_like(W)
    th=2*np.pi*hz/SR
    return 10*np.log10(np.maximum((1-2*r*np.cos(th)*np.cos(W)+r*r*np.cos(2*W))**2+(2*r*np.cos(th)*np.sin(W)-r*r*np.sin(2*W))**2,1e-30))
def sec(p,z): return part(z['hz'],z['r'])-part(p['hz'],p['r'])
cubes=json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
peak_by_stage=[[] for _ in range(7)]; span=[]; contrib=[]
for cu in cubes:
    for c in cu['corners']:
        secs=c['sections'][:7]
        act=[i for i,s in enumerate(secs) if s['pole']['r']>OFF or s['zero']['r']>OFF]
        if len(act)<4: continue
        curves=[sec(secs[i]['pole'],secs[i]['zero']) for i in act]
        tot=np.sum(curves,axis=0)
        span.append(tot.max()-tot.min())
        for i,cv in zip(act,curves):
            peak_by_stage[i].append(cv.max()-cv.min())
            contrib.append((cv.max()-cv.min())/(tot.max()-tot.min()+1e-9))
fig,ax=plt.subplots(1,3,figsize=(17,5),facecolor='white')
ax[0].boxplot([p for p in peak_by_stage],labels=[f'S{i+1}' for i in range(7)],showfliers=False)
ax[0].set_title('per-section dB span by stage position\n(how much each stage shapes, alone)')
ax[0].set_ylabel('dB span'); ax[0].grid(alpha=.25)
ax[1].hist(span,bins=60,color='#1f77b4'); ax[1].set_title(f'total corner dB span\nmedian {np.median(span):.0f} dB')
ax[1].set_xlabel('dB'); ax[1].grid(alpha=.25)
ax[2].hist(np.clip(contrib,0,1.2),bins=60,color='#2ca02c')
ax[2].set_title('one section\'s span ÷ whole corner span\n(>1 means it fights the others)')
ax[2].grid(alpha=.25)
plt.tight_layout(); plt.savefig('plots/corner_design.png',dpi=110)
print('median section span per stage:',[round(float(np.median(p)),1) for p in peak_by_stage])
print('median corner span',round(float(np.median(span)),1))
