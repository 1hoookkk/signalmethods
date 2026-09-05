function build(app,visible)
blue=[0 .447 .741]; gold=[.769 .561 0]; orange=[.851 .325 .098]; grey=[.69 .69 .69]; ground=[.8 .8 .8]; key=[.886 .886 .886];
app.figure=figure('Name','HEADSPACE','NumberTitle','off','MenuBar','none','ToolBar','none', ...
    'Visible',visible,'Color',ground,'Renderer','opengl','Theme','light','Position',[80 60 1280 800], ...
    'Resize','off','InvertHardcopy','off','CloseRequestFcn',@(~,~) delete(app), ...
    'KeyPressFcn',@(~,e) app.onKey(e),'WindowButtonMotionFcn',@(~,~) app.onField('drag'), ...
    'WindowButtonUpFcn',@(~,~) app.onField('release'));
uicontrol(app.figure,'Style','text','String','HEADSPACE','FontName','Arial','FontSize',16,'FontWeight','bold', ...
    'BackgroundColor',ground,'HorizontalAlignment','left','Position',[20 758 300 30]);
names={'PLAY','SAW','PINK NOISE'}; positions=[900 758 90 30; 1000 758 70 30; 1080 758 180 30];
callbacks={@(s,~) app.setPlaying(logical(s.Value)),@(~,~) app.setSource('saw'),@(~,~) app.setSource('noise')};
app.keys=gobjects(1,3);
for k=1:3
    app.keys(k)=uicontrol(app.figure,'Style','togglebutton','String',names{k},'Tag',names{k},'Position',positions(k,:), ...
        'FontName','Arial','FontSize',10,'BackgroundColor',key,'Callback',callbacks{k},'KeyPressFcn',@(~,e) app.onKey(e));
end
panel=uipanel(app.figure,'Units','pixels','Position',[20 470 1240 278],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
x=[app.low(1)-1 app.low(1)+app.span(1)+1]; y=[app.low(2)-1 app.low(2)+app.span(2)+1];
ticks=12*ceil(x(1)/12):12:12*floor(x(2)/12); yticks=12*ceil(y(1)/12):12:12*floor(y(2)/12);
app.fieldAxes=axes(panel,'Units','pixels','Position',[70 44 1150 216],'Color','w','XLim',x,'YLim',y,'Box','on', ...
    'FontName','Arial','FontSize',9,'NextPlot','add','PositionConstraint','innerposition', ...
    'XTick',ticks,'XTickLabel',arrayfun(@(n) trench.bridge.noteName(n),ticks,'UniformOutput',false), ...
    'YTick',yticks,'YTickLabel',arrayfun(@(n) trench.bridge.noteName(n),yticks,'UniformOutput',false));
xlabel(app.fieldAxes,'ROOT'); ylabel(app.fieldAxes,'F2 AND ABOVE');
app.squares=scatter(app.fieldAxes,app.points(:,1),app.points(:,2),20+6*max(0,app.gains),blue,'s','filled','HitTest','off');
app.vertexMarks=scatter(app.fieldAxes,NaN(1,3),NaN(1,3),130,orange,'s','LineWidth',1.5,'HitTest','off');
app.projections=line(app.fieldAxes,NaN,NaN,'Color',gold,'LineStyle',':','HitTest','off');
app.positionMark=line(app.fieldAxes,NaN,NaN,'Marker','s','MarkerSize',13,'MarkerFaceColor',gold,'Color',gold,'LineStyle','none','HitTest','off');
set(app.fieldAxes,'ButtonDownFcn',@(~,~) app.onField('press'));
app.fieldLabel=uicontrol(panel,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor','w', ...
    'HorizontalAlignment','left','Position',[10 4 1220 22]);
panel=uipanel(app.figure,'Units','pixels','Position',[20 246 1240 216],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
app.liveAxes=trench.ui.draw.fixedGrid(panel,[40 10 300 200]); app.liveCurve=trench.ui.draw.responseCurve(app.liveAxes,blue);
app.vertexAxes=gobjects(1,3); app.vertexCurves=gobjects(1,3); app.vertexLabels=gobjects(1,3);
for k=1:3
    x0=400+(k-1)*275;
    app.vertexAxes(k)=trench.ui.draw.fixedGrid(panel,[x0 40 240 120]); app.vertexCurves(k)=trench.ui.draw.responseCurve(app.vertexAxes(k),grey);
    app.vertexLabels(k)=uicontrol(panel,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor','w', ...
        'HorizontalAlignment','left','Position',[x0 168 265 22]);
end
panel=uipanel(app.figure,'Units','pixels','Position',[20 64 1240 174],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'}; app.cornerAxes=gobjects(1,4); app.cornerCurves=gobjects(1,4); app.cornerLabels=gobjects(1,4);
for k=1:4
    x0=30+(k-1)*300;
    app.cornerAxes(k)=trench.ui.draw.fixedGrid(panel,[x0 14 240 120]);
    app.cornerCurves(k)=trench.ui.draw.responseCurve(app.cornerAxes(k),blue);
    set(app.cornerAxes(k),'ButtonDownFcn',@(~,~) app.setCorner(k));
    app.cornerLabels(k)=uicontrol(panel,'Style','text','String',labels{k},'FontName','Arial','FontSize',10,'BackgroundColor','w', ...
        'HorizontalAlignment','left','Position',[x0 142 280 22]);
end
app.writeKey=uicontrol(app.figure,'Style','pushbutton','String','WRITE BODY FILE','Tag','WRITE BODY FILE','Position',[1080 22 180 32], ...
    'FontName','Arial','FontSize',10,'BackgroundColor',key,'Callback',@(~,~) app.writeBodyFile,'KeyPressFcn',@(~,e) app.onKey(e));
app.statusText=uicontrol(app.figure,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor',ground, ...
    'HorizontalAlignment','left','Position',[20 22 1040 30]);
end
