function setPosition(app,xy)
if numel(xy)~=2 || any(~isfinite(xy)), return; end
xy=app.limitPosition(xy);
index=find(all(app.points==xy,2),1);
if ~isempty(index)
    app.chord=app.frames(index).chord; app.vertices=[index index index]; app.weights=[1 0 0];
else
    [app.chord,app.vertices,app.weights]=app.chordAt(xy);
end
app.position=xy; app.words=trench.bridge.unityDc(trench.bridge.compile(app.chord));
trench.audio.words(app.words); app.refresh;
end
