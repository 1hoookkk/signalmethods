function onLibraryRow(room,point,phase)
if ~strcmp(phase,'press') || isempty(room.libraryOrder), return; end
row=max(1,min(numel(room.libraryOrder),round(point(1))));
room.app.selectLibrary(room.libraryOrder(row)); room.drawMap;
end
