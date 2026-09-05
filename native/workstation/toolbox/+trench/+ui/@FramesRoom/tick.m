function tick(room,dt)
if ~room.sweep || numel(room.order)<2, return; end
last=numel(room.order)-1; p=room.position+room.direction*dt/room.hopSeconds;
while p>last || p<0
    if p>last, p=2*last-p; room.direction=-1; end
    if p<0, p=-p; room.direction=1; end
end
room.setPosition(p);
end
