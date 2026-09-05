function frame = conformShelf(frame)
c=frame.chord;
if c(6,1)~=0 && c(6,4)~=0 && c(6,3)>=8 && c(6,6)>=8, return; end
bells=c(c(:,1)~=0 & c(:,3)<8,:); bells=sortrows(bells,2); bells=bells(1:min(5,size(bells,1)),:);
gain=c(6,7); c=repmat([0 60 12 0 60 12 gain],6,1); c(1:size(bells,1),:)=bells;
corner=100; if ~isempty(bells), corner=trench.bridge.hzOf(bells(1,2))/2; end
c(6,:)=trench.model.shelfRow(0,corner,gain);
frame=trench.model.makeFrame(c,frame.name,frame.group,frame.source,frame.capture);
end
