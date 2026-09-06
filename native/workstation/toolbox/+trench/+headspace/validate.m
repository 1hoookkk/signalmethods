function validate(c)
assert(isequal(size(c),[6 7]) && all(isfinite(c),'all'),'Invalid chord.');
assert(all(c(:,[1 4])==1,'all') && all(diff(c(1:5,2))>0),'Five ordered bells required.');
assert(all(c(:,[3 6])>0,'all') && all(c(:,[3 6])<=120,'all'),'Invalid widths.');
assert(all(c(1:5,2)==c(1:5,5)) && all(c(1:5,6)>c(1:5,3)),'Resonant bells required.');
assert(all(c(6,[3 6])==12),'Row six must be a low shelf.');
w=trench.bridge.compile(c); geometry=trench.bridge.geometry(w);
assert(all(geometry(1:5,1)~=0) && all(geometry(:,3)<1) && all(geometry(:,3)>0),'Unstable or missing poles.');
assert(all(isfinite(trench.bridge.responseDb(w,trench.bridge.curveHz))),'Invalid response.');
end
