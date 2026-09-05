function beginEdit(room)
app=room.app; i=find(app.bank.indices==app.corners(app.selectedCorner),1);
if isempty(i), return; end
f=app.bank.frames(i); room.original=f; room.chord=f.chord; room.name=['edit ' f.name]; room.dirty=false;
names={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
app.live=struct('kind','edit','corner',app.selectedCorner); app.status=[names{app.selectedCorner} ' ' char(183) ' ' room.name];
room.refresh; app.refresh;
end
