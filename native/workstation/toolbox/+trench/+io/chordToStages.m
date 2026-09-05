function stages = chordToStages(chord)
stages = repmat(struct('pole',NaN,'zero',NaN,'gain_db',0),6,1);
for k=1:6
    if chord(k,1), stages(k).pole=struct('note',chord(k,2),'width',chord(k,3)); end
    if chord(k,4), stages(k).zero=struct('note',chord(k,5),'width',chord(k,6)); end
    stages(k).gain_db=chord(k,7);
end
end
