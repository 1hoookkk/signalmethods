function path = writeBodyFile(app,path)
if nargin<2, path=trench.io.exportPath(app.root); end
if strcmp(app.room,'corner'), app.rooms.corner.finishEdit; app.rooms.corner.beginEdit; end
trench.bridge.writeBody(path,app.rawCorners,app.rowOn,app.unity);
app.setStatus(path);
end
