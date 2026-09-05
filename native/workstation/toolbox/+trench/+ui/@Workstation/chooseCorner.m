function chooseCorner(app,index)
if app.corners(index)==0, return; end
if strcmp(app.room,'corner')
    app.rooms.corner.finishEdit; app.selectedCorner=index; app.rooms.corner.beginEdit;
else
    app.selectedCorner=index; app.setRoom('corner');
end
end
