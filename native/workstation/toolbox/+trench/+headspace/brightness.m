function notes=brightness(frames)
hz=trench.bridge.curveHz; pitch=trench.bridge.noteOf(hz); notes=zeros(numel(frames),1);
for k=1:numel(frames)
    power=10.^(trench.bridge.responseDb(trench.bridge.unityDc(frames(k).words),hz)/10);
    notes(k)=sum(power(:).*pitch(:))/sum(power);
end
end
