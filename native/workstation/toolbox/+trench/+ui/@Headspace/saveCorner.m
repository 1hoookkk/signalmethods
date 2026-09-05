function saveCorner(app)
if isempty(app.chord), return; end
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'}; on=find(app.weights>0);
if numel(on)==1, name=app.frames(app.vertices(on)).name;
else
    parts=arrayfun(@(k) sprintf('%s %d',app.frames(app.vertices(k)).name,round(100*app.weights(k))),on,'UniformOutput',false);
    name=strjoin(parts,[' ' char(183) ' ']);
end
app.corners{app.current}=trench.model.makeFrame(app.chord,name,'HEADSPACE','headspace');
bank=trench.model.newBank('HEADSPACE');
for k=1:4
    if ~isempty(app.corners{k}), bank=trench.model.putInBank(bank,app.corners{k},k); end
end
trench.io.saveBank(bank,app.bankPath);
app.status=sprintf('saved to %s   %s',labels{app.current},name); app.refresh;
end
