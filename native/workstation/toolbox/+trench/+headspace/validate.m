function validate(c)
assert(isequal(size(c),[6 7]) && all(isfinite(c),'all'),'Invalid chord.');
assert(all(c(:,1)==1) && all(c(:,4)==0),'Six poles and no zeros required.');
assert(all(diff(c(:,2))>0),'Poles must rise from row 1 to row 6.');
assert(all(c(:,3)>0) && all(c(:,3)<=120),'Invalid widths.');
w=trench.bridge.compile(c); geometry=trench.bridge.geometry(w);
assert(all(geometry(:,1)~=0) && all(geometry(:,3)<1) && all(geometry(:,3)>0) && all(geometry(:,4)==0),'Unstable, missing or extra sections.');
assert(all(isfinite(trench.bridge.responseDb(w,trench.bridge.curveHz))),'Invalid response.');
end
