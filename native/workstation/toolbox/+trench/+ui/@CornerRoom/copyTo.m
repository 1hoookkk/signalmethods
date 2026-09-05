function copyTo(room,index)
app=room.app; f=trench.model.makeFrame(room.chord,room.name,room.original.group,room.original.source);
[slot,~]=app.putFrame(f);
if slot>0
    app.corners(index)=slot; app.copies(index)=app.selectedCorner;
    if index==app.selectedCorner, app.copies(index)=0; end
    app.prepared=uint16([]); app.refresh;
end
end
