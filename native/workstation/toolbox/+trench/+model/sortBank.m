function order = sortBank(bank, sortName, pinned)
order = 1:numel(bank.frames);
if isempty(order), return; end
switch string(sortName)
    case "LOW > HIGH ROOT"
        [~,order] = sortrows([[bank.frames.root]' bank.indices'],[1 2]);
    case "NEAREST TO M0 Q0"
        if isempty(pinned), return; end
        costs = arrayfun(@(f) trench.bridge.leadCost(pinned.chord,f.chord),bank.frames);
        [~,order] = sortrows([costs' bank.indices'],[1 2]);
end
order = order(:)';
end
