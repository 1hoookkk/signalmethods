function selectFrame(app,slot)
i=find(app.bank.indices==slot,1); if isempty(i), return; end
app.chosen=slot; app.chosenLibrary=0;
app.live=struct('kind','frame','index',slot,'library',false);
frame=app.bank.frames(i);
tag=app.cornerOf(slot); if ~isempty(tag), tag=[tag ' ' char(183) ' ']; end
app.status=sprintf('%s%s   ROOT %s   %s',tag,frame.name,trench.bridge.noteName(frame.root),trench.bridge.intervalsOf(frame.chord));
if ~isempty(app.rooms)
    position=find(app.rooms.frames.order==i,1)-1;
    if ~isempty(position)
        app.rooms.frames.position=position;
        set(app.rooms.frames.bankList,'Value',position+1);
    end
end
app.refresh;
end
