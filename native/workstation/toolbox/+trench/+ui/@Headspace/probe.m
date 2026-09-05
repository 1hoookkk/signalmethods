function p = probe(app)
n=numel(app.frames); a=floor(app.position)+1; b=min(n,a+1); t=app.position-floor(app.position);
names=repmat({''},1,4);
for k=1:4, if ~isempty(app.corners{k}), names{k}=app.corners{k}.name; end, end
p=struct('position',app.position,'anchors',[a b],'morph',100*t,'words',app.words,'chord',app.chord, ...
    'current',app.current,'corners',{names},'status',app.status,'playing',app.playing,'source',app.source,'count',n);
end
