function landChord(room,chord)
chord=trench.bridge.fitVoices(chord);
w=trench.bridge.compile(chord);
room.chord=trench.bridge.decompile(w);
room.dirty=true; room.app.live=struct('kind','edit','corner',room.app.selectedCorner);
room.app.status=room.name; room.refresh; room.app.refresh;
end
