function setPosition(app,xy)
if numel(xy)~=2 || any(~isfinite(xy)), return; end
xy=max(app.low,min(app.low+app.span,xy(:)'));
[app.words,app.chord,app.vertices,app.weights]=trench.model.blendAt(app.frames,app.tri,(xy-app.low)./app.span);
app.position=xy; app.words=trench.bridge.unityDc(app.words);
trench.audio.words(app.words);
on=app.weights>0;
if nnz(on)==1
    app.status=sprintf('%d  %s',app.vertices(on),app.frames(app.vertices(on)).name);
else
    parts=arrayfun(@(k) sprintf('%s %d',app.frames(app.vertices(k)).name,round(100*app.weights(k))),find(on),'UniformOutput',false);
    app.status=strjoin(parts,['   ' char(183) '   ']);
end
app.refresh;
end
