function selectAnchor(app,k)
if k<1 || k>numel(app.frames), return; end
app.selected=k; app.live='anchor'; f=app.frames(k);
app.words=trench.bridge.unityDc(f.words); trench.audio.words(app.words);
app.status=sprintf('%s   %s',f.name,f.group); app.refresh;
end
