function setWheel(app,morph,q)
if ~any(app.corners) || any(~isfinite([morph q])), return; end
app.morph=max(0,min(100,morph)); app.q=max(0,min(100,q)); app.live='wheel';
[app.words,guarded]=trench.bridge.wheelMorph(trench.bridge.bodyCorners(app.rawCorners,true(1,6),true),app.morph/100,app.q/100);
trench.audio.words(app.words);
app.status=sprintf('MORPH %d   Q %d',round(app.morph),round(app.q));
if any(guarded), app.status=[app.status sprintf('   pole %s guarded',strtrim(sprintf('%d ',find(guarded))))]; end
app.refresh;
end
