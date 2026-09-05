function name = positionName(room)
if isempty(room.order), name=''; return; end
p=min(numel(room.order)-1,room.position); a=floor(p)+1; b=min(a+1,numel(room.order));
f=room.app.bank.frames;
name=sprintf('%s > %s %.1f',f(room.order(a)).name,f(room.order(b)).name,100*mod(p,1));
end
