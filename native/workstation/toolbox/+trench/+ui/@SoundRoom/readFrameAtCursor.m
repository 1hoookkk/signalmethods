function readFrameAtCursor(room)
if isempty(room.sliceChord), return; end
name=sprintf('%s @%.2f',room.sound.name,room.cursor);
frame=trench.model.makeFrame(room.sliceChord,name,'INSTRUMENTS',room.sound.path);
[~,status]=room.app.putFrame(frame);
room.app.live=struct('kind','slice','cursor',room.cursor); room.app.setStatus(status);
end
