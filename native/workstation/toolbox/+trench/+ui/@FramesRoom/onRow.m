function onRow(room,point,phase)
if ~strcmp(phase,'press') || isempty(room.order), return; end
row=max(1,min(numel(room.order),round(point(1))));
slot=room.app.bank.indices(room.order(row));
room.app.selectFrame(slot); room.drawMap;
end
