function setCorner(app,k)
assert(ismember(k,1:4));
app.current=k; app.status=''; app.refresh;
end
