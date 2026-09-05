function chord = stagesToChord(stages)
chord=repmat([0 60 12 0 60 12 0],6,1);
for k=1:6
    if iscell(stages), s=stages{k}; else, s=stages(k); end
    if isstruct(s.pole), chord(k,1:3)=[1 s.pole.note s.pole.width]; end
    if isstruct(s.zero), chord(k,4:6)=[1 s.zero.note s.zero.width]; end
    chord(k,7)=s.gain_db;
end
end
