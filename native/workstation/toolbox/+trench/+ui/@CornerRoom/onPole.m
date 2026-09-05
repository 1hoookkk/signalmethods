function onPole(room,point,phase)
if strcmp(phase,'press'), room.dragStart=point; room.chordStart=room.chord; return; end
row=room.activeRow; c=room.chordStart;
note=trench.bridge.noteOf(max(20,min(20000,point(1))));
if row==6, c(row,4:6)=[1 note 0];
else
    c(row,1:3)=[1 note max(0,min(120,c(row,3)*2^(-(point(2)-room.dragStart(2))/12)))];
    if room.locks(row), c(row,4:5)=[1 note]; end
end
room.landChord(c);
end
