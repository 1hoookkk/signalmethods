function onZero(room,point,phase)
if strcmp(phase,'press'), room.dragStart=point; room.chordStart=room.chord; return; end
row=room.activeRow; c=room.chordStart;
note=trench.bridge.noteOf(max(20,min(20000,point(1))));
width=max(0,min(120,c(row,6)*2^(-(point(2)-room.dragStart(2))/12)));
if row==6, width=0; end
c(row,4:6)=[1 note width]; room.landChord(c);
end
