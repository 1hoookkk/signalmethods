function tick(room,dt)
position=[room.app.morph room.app.q]; enabled=[room.sweepMorph room.sweepQ];
for k=1:2
    if ~enabled(k), continue; end
    position(k)=position(k)+room.direction(k)*dt*25;
    if position(k)>100, position(k)=200-position(k); room.direction(k)=-1; end
    if position(k)<0, position(k)=-position(k); room.direction(k)=1; end
end
if any(enabled), room.setPosition(position(1),position(2)); end
end
