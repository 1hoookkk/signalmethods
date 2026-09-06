function frames=referenceFrames(app)
names={'Klatt 1980','Hillenbrand 1995','DVTD'}; frames=struct([]);
for group=1:3
    bank=trench.io.openBank(fullfile(app.root,'native','workstation','banks',[names{group} '.bank.json']));
    if group==3, bank.frames=bank.frames(endsWith({bank.frames.name},' s1')); end
    for k=1:numel(bank.frames)
        frame=trench.model.conformShelf(bank.frames(k)); frame.group=names{group};
        if isempty(frames), frames=frame; else, frames(end+1)=frame; end
    end
end
assert(numel(frames)==76,'HEADSPACE requires 12 Klatt, 48 Hillenbrand and 16 DVTD subject-1 references.');
end
