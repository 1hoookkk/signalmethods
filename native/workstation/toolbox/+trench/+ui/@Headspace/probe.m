function p = probe(app)
names=repmat({''},1,4);
for k=1:4, if app.corners(k)>0, names{k}=app.frames(app.corners(k)).name; end, end
p=struct('selected',app.selected,'corners',app.corners,'cornerNames',{names},'current',app.current, ...
    'morph',app.morph,'q',app.q,'live',app.live,'words',app.words,'status',app.status, ...
    'playing',app.playing,'source',app.source,'count',numel(app.frames));
end
