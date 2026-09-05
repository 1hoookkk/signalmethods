function putLibrary(room)
app=room.app;
if app.chosenLibrary>0
    [slot,~]=app.putFrame(app.library(app.chosenLibrary));
    if slot>0, app.selectFrame(slot); end
end
room.refresh;
end
