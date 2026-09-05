function onCursor(room,point,~)
if room.threeD, room.setCursor(point(2)); else, room.setCursor(point(1)); end
end
