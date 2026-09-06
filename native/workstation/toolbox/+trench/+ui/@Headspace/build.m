function build(app,visible)
blue=[0 .447 .741]; orange=[.851 .325 .098]; gold=[.769 .561 0]; ground=[.8 .8 .8]; key=[.886 .886 .886];
app.figure=figure('Name','HEADSPACE','NumberTitle','off','MenuBar','none','ToolBar','none', ...
    'Visible',visible,'Color',ground,'Renderer','opengl','Theme','light','Position',[60 50 1480 860], ...
    'Resize','off','InvertHardcopy','off','CloseRequestFcn',@(~,~) delete(app), ...
    'WindowKeyPressFcn',@(~,e) app.onKey(e),'WindowButtonMotionFcn',@(~,~) app.onPad('drag'), ...
    'WindowButtonUpFcn',@(~,~) app.onPad('release'));
uicontrol(app.figure,'Style','text','String','HEADSPACE','FontName','Arial','FontSize',16,'FontWeight','bold', ...
    'BackgroundColor',ground,'HorizontalAlignment','left','Position',[18 816 300 30]);
names={'PLAY','SAW','PINK NOISE'}; positions=[880 816 90 32; 980 816 70 32; 1060 816 150 32];
callbacks={@(s,~) app.setPlaying(logical(s.Value)),@(~,~) app.setSource('saw'),@(~,~) app.setSource('noise')};
app.keys=gobjects(1,3);
for k=1:3
    app.keys(k)=uicontrol(app.figure,'Style','togglebutton','String',names{k},'Tag',names{k},'Position',positions(k,:), ...
        'FontName','Arial','FontSize',10,'BackgroundColor',key,'Callback',callbacks{k});
end
app.writeKey=uicontrol(app.figure,'Style','pushbutton','String','WRITE BODY FILE','Tag','WRITE BODY FILE','Position',[1240 816 222 32], ...
    'FontName','Arial','FontSize',10,'BackgroundColor',key,'Callback',@(~,~) app.writeBodyFile);
panel=uipanel(app.figure,'Units','pixels','Position',[18 300 900 500],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
low=min(app.points(:,1:2)); high=max(app.points(:,1:2)); x=[low(1)-2 high(1)+2]; y=[low(2)-2 high(2)+2];
xt=12*ceil(x(1)/12):12:12*floor(x(2)/12); yt=12*ceil(y(1)/12):12:12*floor(y(2)/12);
app.surfaceAxes=axes(panel,'Units','pixels','Position',[60 44 820 436],'Color','w','XLim',x,'YLim',y,'Box','on', ...
    'FontName','Arial','FontSize',9,'NextPlot','add','PositionConstraint','innerposition', ...
    'XTick',xt,'XTickLabel',arrayfun(@(n) trench.bridge.noteName(n),xt,'UniformOutput',false), ...
    'YTick',yt,'YTickLabel',arrayfun(@(n) trench.bridge.noteName(n),yt,'UniformOutput',false));
xlabel(app.surfaceAxes,'ROOT'); ylabel(app.surfaceAxes,'F2 AND ABOVE');
app.squares=scatter(app.surfaceAxes,app.points(:,1),app.points(:,2),24+5*max(0,app.points(:,3)),blue,'s','filled','HitTest','off');
app.selectedMark=scatter(app.surfaceAxes,NaN,NaN,180,orange,'s','LineWidth',2,'HitTest','off');
app.slotLabels=gobjects(1,4);
for k=1:4
    app.slotLabels(k)=text(app.surfaceAxes,NaN,NaN,num2str(k),'FontName','Arial','FontSize',11,'FontWeight','bold', ...
        'Color','w','BackgroundColor',orange,'Margin',2,'HitTest','off','HorizontalAlignment','center');
end
set(app.surfaceAxes,'ButtonDownFcn',@(~,~) app.onSurface('press'));
panel=uipanel(app.figure,'Units','pixels','Position',[936 300 526 500],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
app.padAxes=axes(panel,'Units','pixels','Position',[60 60 420 400],'Color','w','XLim',[0 100],'YLim',[0 100],'Box','on', ...
    'FontName','Arial','FontSize',9,'NextPlot','add','PositionConstraint','innerposition','XTick',0:25:100,'YTick',0:25:100, ...
    'DataAspectRatio',[1 1 1]);
xlabel(app.padAxes,'MORPH'); ylabel(app.padAxes,'Q');
app.padProjection=line(app.padAxes,NaN,NaN,'Color',gold,'LineStyle',':','HitTest','off');
app.padMark=line(app.padAxes,NaN,NaN,'Marker','s','MarkerSize',12,'MarkerFaceColor',gold,'Color',gold,'LineStyle','none','HitTest','off');
app.padNames=gobjects(1,4); spots=[2 3; 98 3; 2 97; 98 97]; align={'left','right','left','right'};
for k=1:4
    app.padNames(k)=text(app.padAxes,spots(k,1),spots(k,2),'','FontName','Arial','FontSize',9,'HorizontalAlignment',align{k},'HitTest','off');
end
set(app.padAxes,'ButtonDownFcn',@(~,~) app.onPad('press'));
panel=uipanel(app.figure,'Units','pixels','Position',[18 52 1444 236],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
app.liveAxes=trench.ui.draw.fixedGrid(panel,[40 18 300 200]); app.liveCurve=trench.ui.draw.responseCurve(app.liveAxes,blue);
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'}; app.cornerAxes=gobjects(1,4); app.cornerCurves=gobjects(1,4); app.cornerLabels=gobjects(1,4);
for k=1:4
    x0=380+(k-1)*265;
    app.cornerAxes(k)=trench.ui.draw.fixedGrid(panel,[x0 40 240 120]);
    app.cornerCurves(k)=trench.ui.draw.responseCurve(app.cornerAxes(k),blue);
    set(app.cornerAxes(k),'ButtonDownFcn',@(~,~) app.placeCorner(k));
    app.cornerLabels(k)=uicontrol(panel,'Style','text','String',labels{k},'FontName','Arial','FontSize',10,'BackgroundColor','w', ...
        'HorizontalAlignment','left','Position',[x0 190 258 22]);
end
app.statusText=uicontrol(app.figure,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor',ground, ...
    'HorizontalAlignment','left','Position',[18 18 1444 28]);
end
