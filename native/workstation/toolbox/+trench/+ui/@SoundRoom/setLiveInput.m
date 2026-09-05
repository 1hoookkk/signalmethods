function setLiveInput(room)
room.liveInput=true; room.app.setSource('input'); info=trench.audio.start;
if isempty(info.device), room.app.setStatus(['no audio device   ' info.error]); return; end
room.app.audioRate=info.rateHz;
room.sound=struct('path','LIVE IN','name','LIVE IN','mono',zeros(round(info.rateHz*10),1),'fs',info.rateHz,'duration',10);
room.region=[0 10]; room.cursor=9.97; room.lastInput=0; room.app.live=struct('kind','slice','cursor',room.cursor);
set(room.cursorSlider,'Max',10,'Value',room.cursor);
end
