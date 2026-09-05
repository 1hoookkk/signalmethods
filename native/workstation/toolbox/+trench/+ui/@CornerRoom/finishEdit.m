function finishEdit(room)
if ~room.dirty || isempty(room.chord), return; end
app=room.app; frame=trench.model.makeFrame(room.chord,room.name,room.original.group,room.original.source);
[slot,status]=app.putFrame(frame);
if slot>0
    app.corners(app.selectedCorner)=slot; app.copies(app.selectedCorner)=0; app.prepared=uint16([]);
end
room.dirty=false; app.setStatus(status);
end
