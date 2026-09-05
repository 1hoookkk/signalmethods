function setSort(room,name)
if strcmp(name,'NEAREST TO M0 Q0') && room.app.corners(1)==0, return; end
room.sortName=name; room.refresh; room.setPosition(0);
end
