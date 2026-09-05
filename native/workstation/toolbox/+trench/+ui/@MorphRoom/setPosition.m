function setPosition(room,morph,q)
if ~all(isfinite([morph q])), return; end
range=[0 100]; if room.past, range=[-100 200]; end
room.app.morph=max(range(1),min(range(2),morph));
room.app.q=max(range(1),min(range(2),q));
room.app.live=struct('kind','wheel');
room.app.status=sprintf('MORPH %.2f   Q %.2f',room.app.morph,room.app.q);
room.refresh; room.app.refresh;
end
