function setRegion(room,a,b)
if isempty(room.sound)||~all(isfinite([a b])), return; end
a=max(0,min(room.sound.duration-1/room.sound.fs,a)); b=min(room.sound.duration,max(a+1/room.sound.fs,b));
room.region=[a b]; trench.audio.region(a,b);
set(room.inBox,'String',sprintf('%.3f',a)); set(room.outBox,'String',sprintf('%.3f',b));
set(room.inLine,'XData',[a a]); set(room.outLine,'XData',[b b]);
end
