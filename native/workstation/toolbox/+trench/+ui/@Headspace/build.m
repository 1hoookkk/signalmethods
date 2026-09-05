function build(app,visible)
blue=[0 .447 .741]; gold=[.769 .561 0]; grey=[.69 .69 .69]; ground=[.8 .8 .8]; key=[.886 .886 .886];
app.figure=figure('Name','HEADSPACE','NumberTitle','off','MenuBar','none','ToolBar','none', ...
    'Visible',visible,'Color',ground,'Renderer','opengl','Theme','light','Position',[80 60 1280 800], ...
    'Resize','off','InvertHardcopy','off','CloseRequestFcn',@(~,~) delete(app), ...
    'KeyPressFcn',@(~,e) app.onKey(e),'WindowButtonMotionFcn',@(~,~) app.onStrip('drag'), ...
    'WindowButtonUpFcn',@(~,~) app.onStrip('release'));
uicontrol(app.figure,'Style','text','String','HEADSPACE','FontName','Arial','FontSize',16,'FontWeight','bold', ...
    'BackgroundColor',ground,'HorizontalAlignment','left','Position',[20 758 300 30]);
names={'PLAY','SAW','PINK NOISE'}; positions=[900 758 90 30; 1000 758 70 30; 1080 758 180 30];
callbacks={@(s,~) app.setPlaying(logical(s.Value)),@(~,~) app.setSource('saw'),@(~,~) app.setSource('noise')};
app.keys=gobjects(1,3);
for k=1:3
    app.keys(k)=uicontrol(app.figure,'Style','togglebutton','String',names{k},'Tag',names{k},'Position',positions(k,:), ...
        'FontName','Arial','FontSize',10,'BackgroundColor',key,'Callback',callbacks{k},'KeyPressFcn',@(~,e) app.onKey(e));
end
app.strip=uipanel(app.figure,'Units','pixels','Position',[20 640 1240 108],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
n=numel(app.frames);
app.stripAxes=axes(app.strip,'Units','pixels','Position',[10 34 1220 64],'XLim',[-.5 n-.5],'YLim',[0 1], ...
    'Visible','off','NextPlot','add','PositionConstraint','innerposition');
app.squares=gobjects(1,n);
for k=1:n
    app.squares(k)=patch(app.stripAxes,[k-1.4 k-.6 k-.6 k-1.4],[.2 .2 .8 .8],blue,'EdgeColor','w','HitTest','off');
end
app.positionLine=line(app.stripAxes,[0 0],[0 1],'Color',gold,'LineWidth',2,'HitTest','off');
app.positionMark=line(app.stripAxes,0,.5,'Marker','s','MarkerSize',12,'MarkerFaceColor',gold,'Color',gold,'LineStyle','none','HitTest','off');
set(app.stripAxes,'ButtonDownFcn',@(~,~) app.onStrip('press'));
app.stripLabel=uicontrol(app.strip,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor','w', ...
    'HorizontalAlignment','left','Position',[10 6 1220 22]);
panel=uipanel(app.figure,'Units','pixels','Position',[20 246 1240 386],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
app.beforeAxes=trench.ui.draw.fixedGrid(panel,[40 110 300 200]); app.beforeCurve=trench.ui.draw.responseCurve(app.beforeAxes,grey);
app.liveAxes=trench.ui.draw.fixedGrid(panel,[380 30 480 320]); app.liveCurve=trench.ui.draw.responseCurve(app.liveAxes,blue);
app.afterAxes=trench.ui.draw.fixedGrid(panel,[900 110 300 200]); app.afterCurve=trench.ui.draw.responseCurve(app.afterAxes,grey);
app.beforeLabel=uicontrol(panel,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor','w','HorizontalAlignment','left','Position',[40 318 320 22]);
app.afterLabel=uicontrol(panel,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor','w','HorizontalAlignment','left','Position',[900 318 320 22]);
panel=uipanel(app.figure,'Units','pixels','Position',[20 64 1240 174],'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'}; app.cornerAxes=gobjects(1,4); app.cornerCurves=gobjects(1,4); app.cornerLabels=gobjects(1,4);
for k=1:4
    x=30+(k-1)*300;
    app.cornerAxes(k)=trench.ui.draw.fixedGrid(panel,[x 14 240 120]);
    app.cornerCurves(k)=trench.ui.draw.responseCurve(app.cornerAxes(k),blue);
    set(app.cornerAxes(k),'ButtonDownFcn',@(~,~) app.setCorner(k));
    app.cornerLabels(k)=uicontrol(panel,'Style','text','String',labels{k},'FontName','Arial','FontSize',10,'BackgroundColor','w', ...
        'HorizontalAlignment','left','Position',[x 142 280 22]);
end
app.writeKey=uicontrol(app.figure,'Style','pushbutton','String','WRITE BODY FILE','Tag','WRITE BODY FILE','Position',[1080 22 180 32], ...
    'FontName','Arial','FontSize',10,'BackgroundColor',key,'Callback',@(~,~) app.writeBodyFile,'KeyPressFcn',@(~,e) app.onKey(e));
app.statusText=uicontrol(app.figure,'Style','text','String','','FontName','Arial','FontSize',10,'BackgroundColor',ground, ...
    'HorizontalAlignment','left','Position',[20 22 1040 30]);
end
