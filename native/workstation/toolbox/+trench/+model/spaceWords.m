function [words,chord] = spaceWords(frames,position)
n=numel(frames); position=max(0,min(n-1,position));
a=floor(position)+1; t=position-floor(position);
if t==0 || a==n, chord=frames(a).chord; words=frames(a).words; return; end
[ca,cb]=trench.bridge.leadTo(frames(a).chord,frames(a+1).chord);
chord=trench.model.lerpChords(ca,cb,t); words=trench.bridge.compile(chord);
end
