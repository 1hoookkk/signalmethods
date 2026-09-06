function setPosition(app,xy)
if numel(xy)~=2 || any(~isfinite(xy)), return; end
xy=app.limitPosition(xy);
[distance,index]=min(sum((app.points-xy).^2,2));
if distance<1e-18
    app.chord=app.frames(index).chord; app.vertices=[index index index]; app.weights=[1 0 0];
    app.status=app.frames(index).name;
else
    [app.chord,app.vertices,app.weights]=app.chordAt(xy);
    app.status=sprintf('F1 %.2f Hz   F2 %.2f Hz',trench.bridge.hzOf(xy(2)),trench.bridge.hzOf(xy(1)));
end
app.position=xy; app.words=trench.bridge.unityDc(trench.bridge.compile(app.chord));
trench.audio.words(app.words); app.refresh;
end
