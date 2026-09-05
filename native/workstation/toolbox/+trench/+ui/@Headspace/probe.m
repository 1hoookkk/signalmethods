function p = probe(app)
names=repmat({''},1,4);
for k=1:4, if ~isempty(app.corners{k}), names{k}=app.corners{k}.name; end, end
p=struct('position',app.position,'vertices',app.vertices,'weights',app.weights,'words',app.words,'chord',app.chord, ...
    'current',app.current,'corners',{names},'status',app.status,'playing',app.playing,'source',app.source,'count',numel(app.frames));
end
