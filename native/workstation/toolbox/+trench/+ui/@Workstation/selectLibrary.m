function selectLibrary(app,index)
if index<1 || index>numel(app.library), return; end
app.chosenLibrary=index; app.chosen=0;
app.live=struct('kind','frame','index',index,'library',true);
f=app.library(index);
app.status=sprintf('%s   ROOT %s   %s',f.name,trench.bridge.noteName(f.root),trench.bridge.intervalsOf(f.chord));
app.refresh;
end
