function block = windowAt(room,time)
if isempty(room.sound), block=[]; return; end
n=round(.06*room.sound.fs); a=max(1,min(numel(room.sound.mono)-n+1,round(time*room.sound.fs)-floor(n/2)+1));
b=min(numel(room.sound.mono),a+n-1); block=room.sound.mono(a:b);
end
