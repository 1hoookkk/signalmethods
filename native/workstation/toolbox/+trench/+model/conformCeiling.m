function frame = conformCeiling(frame)
c=frame.chord; ceiling=[0 60 12 1 trench.bridge.noteOf(20000) 0 c(6,7)];
if c(6,1)==0 && c(6,4)==1 && c(6,6)==0 && c(6,5)>=trench.bridge.noteOf(18000), return; end
on=find(c(:,1)~=0); [~,order]=sort(c(on,2)); rows=c(on(order),:);
if size(rows,1)>5, rows=rows(1:5,:); end
c=repmat([0 60 12 0 60 12 c(6,7)],6,1); c(1:size(rows,1),:)=rows; c(6,:)=ceiling;
frame=trench.model.makeFrame(c,frame.name,frame.group,frame.source,frame.capture);
end
