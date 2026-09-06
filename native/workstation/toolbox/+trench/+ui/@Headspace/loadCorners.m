function loadCorners(app)
if ~isfile(app.bankPath), return; end
bank=trench.io.openBank(app.bankPath);
for k=1:4
    i=find(bank.indices==k,1);
    if isempty(i), continue; end
    try
        trench.headspace.validate(bank.frames(i).chord);
        app.corners{k}=bank.frames(i);
    catch
        app.status='Saved bank contains an incompatible corner';
    end
end
end
