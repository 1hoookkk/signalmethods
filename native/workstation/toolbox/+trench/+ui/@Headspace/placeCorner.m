function placeCorner(app,slot)
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
app.current=slot;
if app.selected==0, app.status=labels{slot}; app.refresh; return; end
app.corners(slot)=app.selected; app.saveCorners;
app.status=sprintf('%s = %s   %s',labels{slot},app.frames(app.selected).name,app.hopReport); app.refresh;
end
