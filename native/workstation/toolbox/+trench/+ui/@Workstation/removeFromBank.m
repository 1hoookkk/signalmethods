function removeFromBank(app)
slot=app.chosen; i=find(app.bank.indices==slot,1); if isempty(i), return; end
app.bank.frames(i)=[]; app.bank.indices(i)=[];
app.corners(app.corners==slot)=0; app.copies(app.corners==0)=0;
app.chosen=0; app.live=struct('kind','none'); app.prepared=uint16([]);
app.rooms.frames.refresh; app.setStatus(app.bank.name);
end
