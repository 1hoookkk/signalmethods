function setCursor(room,value)
if isempty(room.sound) || ~isfinite(value), return; end
room.cursor=max(0,min(room.sound.duration,value));
set(room.cursorSlider,'Value',room.cursor); set(room.cursorBox,'String',sprintf('%.3f',room.cursor));
set(room.cursorLine,'XData',[room.cursor room.cursor]);
room.readSlice;
end
