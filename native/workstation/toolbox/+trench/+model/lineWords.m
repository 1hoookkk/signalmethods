function words = lineWords(bank, order, position)
assert(~isempty(order),'trench:line:empty','Bank is empty.');
position = max(0,min(numel(order)-1,position));
anchor = floor(position)+1; morph = position-floor(position);
a = bank.frames(order(anchor));
if morph == 0 || anchor == numel(order), words = a.words; return; end
b = bank.frames(order(anchor+1));
[ca,cb] = trench.bridge.leadTo(a.chord,b.chord);
words = trench.bridge.pairMorph(trench.bridge.compile(ca),trench.bridge.compile(cb),morph);
end
