function points = spacePoints(frames)
hz=trench.bridge.curveHz; points=zeros(numel(frames),3);
for k=1:numel(frames)
    c=frames(k).chord; on=c(:,1)~=0 & c(:,2)>=12 & c(:,2)<=trench.bridge.noteOf(5512.5);
    narrow=on & c(:,3)<=6; if nnz(narrow)>=2, on=narrow; end
    notes=sort(c(on,2)); if isempty(notes), notes=60; end
    upper=notes(1); if numel(notes)>1, upper=mean(notes(2:end)); end
    points(k,:)=[notes(1) upper max(trench.bridge.responseDb(trench.bridge.unityDc(frames(k).words),hz))];
end
end
