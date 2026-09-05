function startVoice(room,row,voice)
room.activeRow=row; room.voice=voice; ax=room.rowAxes(row); p=ax.CurrentPoint;
method='onPole'; if strcmp(voice,'zero'), method='onZero'; end
room.app.beginDrag(room,method,ax,p(1,1:2));
end
