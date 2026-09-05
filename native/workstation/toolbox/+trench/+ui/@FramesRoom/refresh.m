function refresh(room)
app=room.app; pinned=[];
if app.corners(1)>0, pinned=app.bank.frames(app.bank.indices==app.corners(1)); end
room.order=trench.model.sortBank(app.bank,room.sortName,pinned);
items=cell(1,numel(room.order));
for k=1:numel(room.order)
    i=room.order(k); f=app.bank.frames(i);
    items{k}=sprintf('%3d  %-5s  %s   %s  %.1f  %.2f   %s   %s',app.bank.indices(i),app.cornerOf(app.bank.indices(i)),f.name, ...
        trench.bridge.noteName(f.root),f.voicing,f.resonance,trench.bridge.intervalsOf(f.chord),f.source);
end
if isempty(items), items={''}; end
set(room.bankList,'Value',1,'String',items);
set(room.bankTitle,'String',sprintf('%s   %d of 256',app.bank.name,numel(app.bank.frames)));
set(room.positionSlider,'Max',max(1,numel(room.order)-1),'Value',min(room.position,max(0,numel(room.order)-1)));
set(room.positionAxes,'XLim',[0 max(1,numel(room.order)-1)]);
room.refreshLibrary; room.drawMap;
end
