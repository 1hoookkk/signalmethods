function refreshLibrary(room)
lib=room.app.library; names=trench.bridge.groupNames;
allowed=names(room.groups); ids=find(ismember({lib.group},allowed));
if strcmp(room.librarySort,'NEAREST TO') && room.app.chosen>0
    f=room.app.bank.frames(room.app.bank.indices==room.app.chosen);
    costs=arrayfun(@(i) trench.bridge.leadCost(f.chord,lib(i).chord),ids);
else
    costs=[lib(ids).root];
end
[~,order]=sort(costs); room.libraryOrder=ids(order);
items=arrayfun(@(i) sprintf('%s   %s   %.1f   %.2f',lib(i).name,trench.bridge.noteName(lib(i).root), ...
    lib(i).voicing,lib(i).resonance),room.libraryOrder,'UniformOutput',false);
if isempty(items), items={''}; end
set(room.libraryList,'Value',1,'String',items);
end
