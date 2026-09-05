function saveCorner(app)
if isempty(app.chord), return; end
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
n=numel(app.frames); a=floor(app.position)+1; t=app.position-floor(app.position);
if t==0 || a==n, name=app.frames(a).name;
else, name=sprintf('%s > %s %d',app.frames(a).name,app.frames(a+1).name,round(100*t)); end
app.corners{app.current}=trench.model.makeFrame(app.chord,name,'HEADSPACE','headspace');
bank=trench.model.newBank('HEADSPACE');
for k=1:4
    if ~isempty(app.corners{k}), bank=trench.model.putInBank(bank,app.corners{k},k); end
end
trench.io.saveBank(bank,app.bankPath);
app.status=sprintf('saved to %s   %s',labels{app.current},name); app.refresh;
end
