function nextFrame(room,direction)
app=room.app; order=app.rooms.frames.order; if isempty(order), return; end
slots=app.bank.indices(order); i=find(slots==app.corners(2),1);
if isempty(i), i=1; end
next=slots(mod(i-1+direction,numel(slots))+1);
app.corners(2)=next;
if app.copies(4)==2, app.corners(4)=next; end
app.prepared=uint16([]); app.live=struct('kind','wheel');
room.refresh; app.refresh;
end
