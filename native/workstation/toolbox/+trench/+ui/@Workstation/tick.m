function tick(app,dt)
if app.closing, return; end
if strcmp(app.room,'frames'), app.rooms.frames.tick(dt); end
if strcmp(app.room,'morph'), app.rooms.morph.tick(dt); end
if strcmp(app.room,'sound'), app.rooms.sound.tick; end
app.refresh; app.response.tick;
end
