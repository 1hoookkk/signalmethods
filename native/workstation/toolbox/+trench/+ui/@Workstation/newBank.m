function newBank(app)
app.bank=trench.model.newBank; app.corners=zeros(1,4); app.copies=zeros(1,4);
app.chosen=0; app.chosenLibrary=0; app.live=struct('kind','none'); app.prepared=uint16([]);
app.rooms.frames.position=0; app.rooms.frames.refresh; app.setRoom('frames');
end
