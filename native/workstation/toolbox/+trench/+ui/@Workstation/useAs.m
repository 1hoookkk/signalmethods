function useAs(app,corner)
slot=app.chosen;
if slot==0 && strcmp(app.live.kind,'line')
    frame=trench.model.makeFrame(trench.bridge.decompile(app.wordsOf),app.rooms.frames.positionName,'INSTRUMENTS','bank line');
    [slot,~]=app.putFrame(frame);
end
if slot==0, return; end
if ~any(app.corners)
    app.corners(:)=slot; app.copies(:)=corner; app.copies(corner)=0;
else
    app.corners(corner)=slot; app.copies(corner)=0;
    for k=1:4
        if app.copies(k)==corner, app.corners(k)=slot; end
    end
    if corner==2 && app.copies(4)>0, app.corners(4)=slot; app.copies(4)=2; end
end
app.selectedCorner=corner; app.prepared=uint16([]);
app.rooms.frames.refresh; app.refresh;
end
