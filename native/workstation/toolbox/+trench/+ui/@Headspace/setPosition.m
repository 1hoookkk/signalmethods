function setPosition(app,p)
if ~isfinite(p), return; end
n=numel(app.frames); app.position=max(0,min(n-1,p));
[app.words,app.chord]=trench.model.spaceWords(app.frames,app.position);
app.words=trench.bridge.unityDc(app.words);
trench.audio.words(app.words);
a=floor(app.position)+1; t=app.position-floor(app.position);
if t==0, app.status=sprintf('%d  %s',a,app.frames(a).name);
else, app.status=sprintf('%d > %d   MORPH %d',a,a+1,round(100*t)); end
app.refresh;
end
