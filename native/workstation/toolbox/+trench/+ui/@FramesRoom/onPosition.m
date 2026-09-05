function onPosition(room,point,phase)
if strcmp(phase,'press'), room.dragStart=[point(1) room.position]; return; end
scale=1; if room.fine, scale=.1; end
room.setPosition(room.dragStart(2)+(point(1)-room.dragStart(1))*scale);
end
