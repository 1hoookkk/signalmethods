function [words,chord,idx,w] = blendAt(frames,tri,xy)
[t,bary]=pointLocation(tri,xy);
if isnan(t)
    [~,i]=min(sum((tri.Points-xy).^2,2)); idx=[i i i]; w=[1 0 0];
    chord=frames(i).chord; words=frames(i).words; return
end
idx=double(tri.ConnectivityList(t,:)); w=bary(:)'; w(w<1e-9)=0; w=w/sum(w);
if nnz(w)==1, i=idx(w>0); chord=frames(i).chord; words=frames(i).words; return; end
chord=trench.model.blendChords(cat(3,frames(idx).chord),w); words=trench.bridge.compile(chord);
end
