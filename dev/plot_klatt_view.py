import json, math, collections
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
cuts=json.load(open('ref/stage_vocabulary.json'))['cuts']; SR=39062.5; OFF=cuts['root_off_r']
def bwof(r): return -math.log(r)*SR/math.pi if 0<r<1 else float('nan')
cubes=json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
F=[];B=[];S=[]
for cu in cubes:
    for c in cu['corners']:
        for i,s in enumerate(c['sections'][:7]):
            p=s['pole']
            if p['r']>OFF and 40<p['hz']<18000:
                b=bwof(p['r'])
                if b==b and 1<b<4000: F.append(p['hz']); B.append(b); S.append(i)
F=np.array(F);B=np.array(B);S=np.array(S)
# P2K vocal corpus
import glob
pf=[];pb=[]
for f in glob.glob('recipes/architectures/*.json'):
    r=json.load(open(f))
    for sec in r['sections']:
        for cn,c in sec['corners'].items():
            p=c.get('pole') or {}
            hz=p.get('hz',0); rr=p.get('r',0)
            if rr and 40<hz<18000 and 0<rr<1:
                b=-math.log(rr)*SR/math.pi
                if 1<b<4000: pf.append(hz); pb.append(b)
pf=np.array(pf);pb=np.array(pb)
fig,ax=plt.subplots(1,2,figsize=(16,6),facecolor='white')
a=ax[0]
h=a.hexbin(F,B,xscale='log',yscale='log',gridsize=55,cmap='Blues',mincnt=1)
x=np.logspace(math.log10(40),math.log10(18000),100)
a.plot(x,15+20*(x/500)**0.5,'r--',lw=2,label='Klatt-style B(F): widens with F')
a.plot(x,0.06*x,'g:',lw=2,label='constant Q = 16.7')
a.set_xlabel('pole frequency (Hz)'); a.set_ylabel('bandwidth (Hz)')
a.set_title(f'Morpheus poles in Klatt coordinates  (n={len(F)})'); a.legend(fontsize=8); a.grid(alpha=.2)
plt.colorbar(h,ax=a)
a=ax[1]
h=a.hexbin(pf,pb,xscale='log',yscale='log',gridsize=45,cmap='Oranges',mincnt=1)
a.plot(x,15+20*(x/500)**0.5,'r--',lw=2)
a.plot(x,0.06*x,'g:',lw=2)
a.set_xlabel('pole frequency (Hz)'); a.set_ylabel('bandwidth (Hz)')
a.set_title(f'P2K vocal poles in Klatt coordinates  (n={len(pf)})'); a.grid(alpha=.2)
plt.colorbar(h,ax=a)
plt.tight_layout(); plt.savefig('plots/klatt_view.png',dpi=110)
for name,f,b in (('morpheus',F,B),('p2k',pf,pb)):
    q=f/b
    print(f"{name}: median B {np.median(b):.0f} Hz, median Q {np.median(q):.1f}, B<100Hz share {100*np.mean(b<100):.0f}%")
