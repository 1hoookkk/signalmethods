function setPosition(room,position)
if isempty(room.order)||~isfinite(position), return; end
position=max(0,min(numel(room.order)-1,position)); room.position=position;
if abs(position-round(position))<1e-10
    room.position=round(position);
    room.app.selectFrame(room.app.bank.indices(room.order(room.position+1)));
else
    room.app.chosen=0; room.app.live=struct('kind','line','position',position,'anchor',floor(position)+1,'morph',100*mod(position,1));
    room.app.status=room.positionName;
end
set(room.positionSlider,'Value',position); room.drawMap; room.app.refresh;
end
