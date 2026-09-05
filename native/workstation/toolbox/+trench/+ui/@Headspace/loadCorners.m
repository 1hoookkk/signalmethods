function loadCorners(app)
if ~isfile(app.bankPath), return; end
bank=trench.io.openBank(app.bankPath);
for k=1:4
    i=find(bank.indices==k,1); if ~isempty(i), app.corners{k}=bank.frames(i); end
end
end
