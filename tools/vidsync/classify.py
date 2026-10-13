# classify.py: per-frame input-display scores from a strip array, using the slot templates
import numpy as np
ORDER='UDLRSsBAlr'
def load(path):
    t=np.load(path); T={}
    for k in ORDER:
        if k+'_w' not in t.files: continue
        w=t[k+'_w'].copy(); d=t[k+'_d'].copy()
        w[:,:3]=False; w[:,15:]=False; d[:,:3]=False; d[:,15:]=False
        T[k]=(int(t[k+'_x']), w, d)
    return T
def scores(strip, T):
    """strip: (N,36,150,3) uint8 (x 330.., y 84..). Returns (N, slots) score = interior - outline."""
    mn=strip.min(3).astype(np.float32); mx=strip.max(3).astype(np.float32)
    out=np.zeros((len(strip),len(ORDER)),np.float32)
    for j,k in enumerate(ORDER):
        if k not in T: out[:,j]=-999; continue
        x0,w,d=T[k]; x=x0-330-2
        win_mn=mn[:,91-84:111-84,x:x+17]; win_mx=mx[:,91-84:111-84,x:x+17]
        out[:,j]=win_mn[:,w].mean(1)-win_mx[:,d].mean(1)
    return out
if __name__=='__main__':
    import sys
    a=np.load(sys.argv[1]); T=load(sys.argv[2]); s=scores(a,T)
    for j,k in enumerate(ORDER):
        h,_=np.histogram(s[:,j],bins=np.arange(-260,270,20))
        print(k, ' '.join(f'{v:4d}' for v in h))
    print('bins', ' '.join(f'{b:4d}' for b in np.arange(-260,260,20)))
