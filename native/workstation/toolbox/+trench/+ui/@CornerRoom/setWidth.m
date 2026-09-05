function setWidth(room,row,value)
if ischar(value)||isstring(value)
    text=lower(char(value));
    if endsWith(text,'hz')
        hz=trench.bridge.hzOf(room.chord(row,2)); value=12*log2(1+str2double(erase(text,'hz'))/hz);
    else, value=str2double(erase(text,'st')); end
end
if ~isfinite(value), return; end
c=room.chord; c(row,3)=max(0,min(120,value)); room.landChord(c);
end
