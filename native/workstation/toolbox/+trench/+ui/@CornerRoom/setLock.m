function setLock(room,row,on)
room.locks(row)=on;
if on && ~isempty(room.chord)
    c=room.chord; c(row,4:5)=[1 c(row,2)]; room.landChord(c);
end
end
