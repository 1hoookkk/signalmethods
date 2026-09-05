function points = spacePoints(frames)
hz=trench.bridge.curveHz; points=zeros(numel(frames),2);
for k=1:numel(frames)
    points(k,:)=[frames(k).root max(trench.bridge.responseDb(trench.bridge.unityDc(frames(k).words),hz))];
end
end
