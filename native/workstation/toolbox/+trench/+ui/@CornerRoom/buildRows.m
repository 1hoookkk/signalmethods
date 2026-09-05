function buildRows(room)
room.rowAxes=gobjects(1,6); room.rowCurves=gobjects(1,6); room.wholeCurves=gobjects(1,6);
room.poles=gobjects(1,6); room.zeros=gobjects(1,6); room.pitchBoxes=gobjects(1,6);
room.widthBoxes=gobjects(1,6); room.amountBoxes=gobjects(1,6); room.zeroLabels=gobjects(1,6);
room.intervalLabels=gobjects(1,6); room.hzLabels=gobjects(1,6);
for k=1:6
    x=50+mod(k-1,3)*342; y=411-floor((k-1)/3)*318;
    ax=trench.ui.draw.fixedGrid(room.display,[x y 252 189],4/3); room.rowAxes(k)=ax;
    room.wholeCurves(k)=trench.ui.draw.responseCurve(ax,[.75 .75 .75]);
    room.rowCurves(k)=trench.ui.draw.responseCurve(ax);
    room.poles(k)=line(ax,NaN,NaN,'Marker','s','MarkerSize',7,'MarkerFaceColor',[0 .447 .741], ...
        'Color',[0 .447 .741],'ButtonDownFcn',@(~,~) room.startVoice(k,'pole'));
    room.zeros(k)=line(ax,NaN,NaN,'Marker','.','MarkerSize',18,'Color',[.851 .325 .098], ...
        'ButtonDownFcn',@(~,~) room.startVoice(k,'zero'));
    set(ax,'ButtonDownFcn',@(~,~) room.startVoice(k,'pole'));
    names={'PITCH','WIDTH','AMOUNT','ZERO'};
    for j=1:4, label(room,names{j},[x-20+(j-1)*81 y-41 78 16]); end
    room.pitchBoxes(k)=box(room,[x-20 y-63 78 23],@(s,~) room.setPitch(k,s.String),'PITCH');
    room.widthBoxes(k)=box(room,[x+61 y-63 78 23],@(s,~) room.setWidth(k,s.String),'WIDTH');
    room.amountBoxes(k)=box(room,[x+142 y-63 78 23],@(s,~) room.setAmount(k,str2double(s.String)),'AMOUNT');
    room.zeroLabels(k)=label(room,'',[x+223 y-63 78 23]);
    h=uicontrol(room.display,'Style','togglebutton','String','LOCK ZERO TO POLE','Value',1, ...
        'FontName','Arial','FontSize',8,'Position',[x-20 y+202 170 23],'Tag','LOCK ZERO TO POLE', ...
        'Callback',@(s,~) room.setLock(k,logical(s.Value)),'BackgroundColor',[.886 .886 .886]);
    room.intervalLabels(k)=label(room,'',[x+155 y+203 140 21]);
    room.hzLabels(k)=label(room,'',[x-20 y-87 321 20]); room.hzLabels(k).Visible='off';
    if k==6, h.String='CEILING'; h.Tag='CEILING'; h.Callback=@(~,~) room.setCeiling; end
end
end
function h=label(room,text,position)
h=uicontrol(room.display,'Style','text','String',text,'Position',position,'FontName','Arial', ...
    'FontSize',9,'BackgroundColor','w','HorizontalAlignment','left');
end
function h=box(room,position,callback,tag)
h=uicontrol(room.display,'Style','edit','String','','Position',position,'FontName','Arial', ...
    'FontSize',9,'BackgroundColor','w','Callback',callback,'Tag',tag);
end
