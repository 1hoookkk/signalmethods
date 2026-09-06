function loadCorners(app)
if ~isfile(app.bankPath), return; end
bank=trench.io.openBank(app.bankPath); names={app.frames.name};
for k=1:4
    i=find(bank.indices==k,1); if isempty(i), continue; end
    j=find(strcmp(names,bank.frames(i).name),1);
    if ~isempty(j) && isequal(app.frames(j).words,bank.frames(i).words), app.corners(k)=j; end
end
end
