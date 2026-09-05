function onPad(room,point,phase)
if strcmp(phase,'press')
    room.dragStart=point; room.valueStart=[room.app.morph room.app.q];
    room.app.live=struct('kind','wheel'); return
end
scale=1; if room.fine, scale=.1; end
position=room.valueStart+(point-room.dragStart)*scale;
room.setPosition(position(1),position(2));
end
