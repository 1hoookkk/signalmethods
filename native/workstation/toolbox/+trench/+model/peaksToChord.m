function chord=peaksToChord(peaks)
chord=repmat([0 60 12 0 60 12 0],6,1);
if size(peaks,1)>5
    [~,order]=sort(peaks(:,3),'descend'); peaks=sortrows(peaks(order(1:5),:),1);
end
for k=1:size(peaks,1)
    note=trench.bridge.noteOf(peaks(k,1));
    chord(k,:)=[1 note .25 1 note 4 0];
end
chord(6,:)=[0 60 12 1 trench.bridge.noteOf(20000) 0 0];
chord=trench.bridge.fitVoices(chord);
end
