function onOrbit(room,point,phase)
if strcmp(phase,'press'), room.dragStart=point; room.viewStart=view(room.axes); return; end
delta=point-room.dragStart; view(room.axes,room.viewStart+[delta(1)/100 delta(2)*10]);
end
