function keepAsFrame(room)
app=room.app; names=app.cornerNames;
name=sprintf('%s %.2f/%.2f',names{1},app.morph,app.q);
frame=trench.model.makeFrame(trench.bridge.decompile(app.wordsOf),name,'INSTRUMENTS','MORPH');
[slot,status]=app.putFrame(frame);
if slot>0, app.live=struct('kind','frame','index',slot,'library',false); end
app.setStatus(status);
end
