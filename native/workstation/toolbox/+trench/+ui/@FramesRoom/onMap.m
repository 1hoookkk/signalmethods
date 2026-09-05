function onMap(room,point,phase)
if ~strcmp(phase,'press'), return; end
app=room.app; lib=app.library; bank=app.bank.frames;
lp=[[lib.root]' [lib.voicing]']; dist=sum(((lp-point)./[84 6]).^2,2);
[best,index]=min(dist);
if ~isempty(bank)
    bp=[[bank.root]' [bank.voicing]']; bd=sum(((bp-point)./[84 6]).^2,2);
    [value,b]=min(bd);
    if value<=best+1e-12
        tied=find(abs(bd-value)<1e-12); chosen=find(app.bank.indices(tied)==app.chosen,1);
        if ~isempty(chosen), b=tied(chosen); end
        app.selectFrame(app.bank.indices(b)); room.drawMap; return
    end
end
same=find(abs(dist-best)<1e-12);
[slot,status]=app.putFrame(lib(index));
if slot>0
    app.selectFrame(slot);
    if numel(same)>1, app.setStatus(strjoin({lib(same).name},' | ')); end
else, app.setStatus(status); end
room.drawMap;
end
