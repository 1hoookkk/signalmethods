function [bank, slot, status] = putInBank(bank, frame, preferred)
if nargin < 3, preferred = 0; end
free = setdiff(1:256, bank.indices);
slot = 0; status = 'bank full';
if isempty(free), return; end
slot = free(1);
if ismember(preferred, free), slot = preferred; end
bank.indices(end+1) = slot;
if isempty(bank.frames), bank.frames = frame; else, bank.frames(end+1) = frame; end
[bank.indices, order] = sort(bank.indices);
bank.frames = bank.frames(order);
status = frame.name;
end
