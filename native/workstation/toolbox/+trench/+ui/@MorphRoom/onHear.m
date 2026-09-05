function onHear(room,~,phase)
if strcmp(phase,'press'), room.app.live=struct('kind','cornerAlone','index',1);
elseif strcmp(phase,'release'), room.app.live=struct('kind','wheel'); end
room.app.refresh;
end
