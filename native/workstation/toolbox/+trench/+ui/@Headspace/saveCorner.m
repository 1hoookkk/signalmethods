function saveCorner(app)
if isempty(app.chord), return; end
name=sprintf('Corner %d',app.current);
app.corners{app.current}=trench.model.makeFrame(app.chord,name,'HEADSPACE','headspace');
bank=trench.model.newBank('HEADSPACE');
for k=1:4
    if ~isempty(app.corners{k}), bank=trench.model.putInBank(bank,app.corners{k},k); end
end
trench.io.saveBank(bank,app.bankPath);
app.status=sprintf('Saved corner %d',app.current); app.refresh;
end
