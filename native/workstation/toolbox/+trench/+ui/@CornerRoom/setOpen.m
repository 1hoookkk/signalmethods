function setOpen(room,value)
if isempty(room.chord)||~isfinite(value), return; end
value=max(250,min(900,value)); c=room.chord;
ids=find(c(1:5,1) & c(1:5,3)<=6);
if isempty(ids), ids=find(c(1:5,1)); end
if isempty(ids), return; end
[~,j]=min(c(ids,2)); row=ids(j); old=c(row,2); c(row,2)=trench.bridge.noteOf(value);
c(row,3)=12*log2(1+(60+60*log(value/250)/log(900/250))/value);
if room.locks(row), c(row,5)=c(row,2); else, c(row,5)=c(row,5)+c(row,2)-old; end
room.landChord(c);
end
