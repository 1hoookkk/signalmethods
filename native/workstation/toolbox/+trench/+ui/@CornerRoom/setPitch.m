function setPitch(room,row,value)
if isempty(room.chord), return; end
shape=trench.bridge.shapeOf(room.chord);
note=trench.model.parseNote(value,shape(1)); note=max(trench.bridge.noteOf(20),min(trench.bridge.noteOf(20000),note));
c=room.chord;
if row==6
    c(row,4:6)=[1 note 0];
else
    c(row,1:2)=[1 note];
    if room.locks(row), c(row,4:5)=[1 note]; end
end
room.landChord(c);
end
