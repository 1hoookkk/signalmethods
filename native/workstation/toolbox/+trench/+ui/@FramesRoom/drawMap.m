function drawMap(room)
app=room.app; lib=app.library;
points=unique([[lib.root]' [lib.voicing]'],'rows','stable');
set(room.dimMarks,'XData',points(:,1),'YData',points(:,2));
if isempty(room.order)
    set(room.marks,'XData',NaN,'YData',NaN);
    set(room.pathLine,'XData',NaN,'YData',NaN); set(room.positionMark,'XData',NaN,'YData',NaN);
    set(room.hopLine,'XData',NaN,'YData',NaN); set([room.anchorLabels room.cornerLabels],'String',''); return
end
f=app.bank.frames; x=[f.root]; y=[f.voicing];
colors=repmat([0 .447 .741],numel(f),1);
colors(app.bank.indices==app.chosen,:)=repmat([.851 .325 .098],sum(app.bank.indices==app.chosen),1);
set(room.marks,'XData',x,'YData',y,'SizeData',max(12,min(100,25./max(.25,[f.resonance]))),'CData',colors);
set(room.pathLine,'XData',x(room.order),'YData',y(room.order));
names={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
for k=1:4
    i=[]; if app.corners(k)>0 && app.copies(k)==0, i=find(app.bank.indices==app.corners(k),1); end
    if isempty(i), set(room.cornerLabels(k),'String','');
    else, set(room.cornerLabels(k),'Position',[max(24.5,min(105,x(i)+.5)) max(.15,min(5.8,y(i)-.22)) 0],'String',names{k}); end
end
p=max(0,min(numel(room.order)-1,room.position)); a=floor(p)+1; b=min(a+1,numel(room.order)); t=p-floor(p);
px=x(room.order(a))*(1-t)+x(room.order(b))*t; py=y(room.order(a))*(1-t)+y(room.order(b))*t;
set(room.positionMark,'XData',px,'YData',py);
set(room.projections,'XData',[24 px px],'YData',[py py 0]);
ids=room.order([a b]);
set(room.hopLine,'XData',x(ids),'YData',y(ids));
for k=1:2
    set(room.anchorLabels(k),'Position',[max(24.5,min(105,x(ids(k))+.5)) max(.15,min(5.8,y(ids(k))+.13)) 0], ...
        'String',num2str(app.bank.indices(ids(k))));
end
set(room.positionThumb,'XData',p);
set(room.positionBox,'String',sprintf('%.3f',p)); set(room.anchorBox,'String',num2str(a));
set(room.morphBox,'String',sprintf('%.1f',100*t));
set(room.pairLabel,'String',{sprintf('%d  %s',app.bank.indices(ids(1)),f(ids(1)).name), ...
    sprintf('> %d  %s',app.bank.indices(ids(2)),f(ids(2)).name)});
end
