function build(app,visible)
blue=[0 114 189]/255; ground=[.8 .8 .8]; key=[226 226 226]/255;
app.figure=figure('Name','HEADSPACE','NumberTitle','off','MenuBar','none','ToolBar','none', ...
    'Visible',visible,'Color',ground,'Renderer','opengl','Theme','light','Position',[60 50 1480 860], ...
    'Resize','off','InvertHardcopy','off','CloseRequestFcn',@(~,~) delete(app), ...
    'WindowKeyPressFcn',@(~,e) app.onKey(e),'WindowButtonMotionFcn',@(~,~) app.onField('drag'), ...
    'WindowButtonUpFcn',@(~,~) app.onField('release'));
names={'PLAY','SAW','PINK NOISE'}; positions=[1010 790 90 32;1110 790 80 32;1200 790 170 32];
callbacks={@(s,~) app.setPlaying(logical(s.Value)),@(~,~) app.setSource('saw'),@(~,~) app.setSource('noise')};
app.keys=gobjects(1,3);
for k=1:3
    app.keys(k)=uicontrol(app.figure,'Style','togglebutton','String',names{k},'Tag',names{k},'Position',positions(k,:), ...
        'FontName','Arial','FontSize',11,'BackgroundColor',key,'Callback',callbacks{k});
end
panel=uipanel(app.figure,'Units','pixels','Position',[18 118 920 720],'BackgroundColor','w','BorderType','line');
app.fieldAxes=axes(panel,'Units','pixels','Position',[20 44 880 656],'Color','w', ...
    'XLim',[min(app.points(:,1)) max(app.points(:,1))],'YLim',[min(app.points(:,2)) max(app.points(:,2))], ...
    'YDir','reverse','DataAspectRatio',[1 1 1],'Visible','off','XTick',[],'YTick',[], ...
    'NextPlot','add','PositionConstraint','innerposition','ButtonDownFcn',@(~,~) app.onField('press'));
set(panel,'ButtonDownFcn',@(~,~) app.onField('press'));
app.shade=patch(app.fieldAxes,'Faces',app.tri.ConnectivityList,'Vertices',app.points,'FaceVertexCData',app.brightness, ...
    'FaceColor','interp','EdgeColor','none','HitTest','off');
colormap(app.fieldAxes,parula(256)); clim(app.fieldAxes,[min(app.brightness) max(app.brightness)]);
set(app.shade,'FaceAlpha',.3);
colors=[0 114 189; 217 83 25; 196 143 0; 110 110 110; 176 176 176]/255;
[x,y]=meshgrid(linspace(min(app.points(:,1)),max(app.points(:,1)),320),linspace(min(app.points(:,2)),max(app.points(:,2)),280));
app.contours=gobjects(1,5);
for k=1:5
    notes=arrayfun(@(f) f.chord(k,2),app.frames);
    field=scatteredInterpolant(app.points(:,1),app.points(:,2),notes(:),'linear','none'); z=field(x,y);
    [~,app.contours(k)]=contour(app.fieldAxes,x,y,z,floor(min(notes)):ceil(max(notes)),'LineColor',colors(k,:),'LineWidth',.6,'HitTest','off');
end
rim=freeBoundary(app.tri); loop=rim(1,:); rim(1,:)=[];
while ~isempty(rim)
    next=find(rim(:,1)==loop(end),1);
    if isempty(next), next=find(rim(:,2)==loop(end),1); rim(next,:)=fliplr(rim(next,:)); end
    loop(end+1)=rim(next,2); rim(next,:)=[];
end
rim=app.points(loop,:);
app.edge=line(app.fieldAxes,rim(:,1),rim(:,2),'Color','k','LineWidth',.75,'HitTest','off');
gold=[196 143 0]/255;
app.positionMark=line(app.fieldAxes,NaN,NaN,'Marker','s','MarkerSize',12,'MarkerFaceColor',gold,'Color',gold,'LineStyle','none','HitTest','off');
app.fieldLabel=uicontrol(panel,'Style','text','String','','FontName','Arial','FontSize',11,'BackgroundColor','w', ...
    'HorizontalAlignment','left','Position',[20 8 880 28]);
panel=uipanel(app.figure,'Units','pixels','Position',[958 346 504 374],'BackgroundColor','w','BorderType','line');
app.liveAxes=trench.ui.draw.fixedGrid(panel,[49 40 432 288]);
app.liveCurve=trench.ui.draw.responseCurve(app.liveAxes,blue);
app.cornerLabels=gobjects(1,4);
for k=1:4
    app.cornerLabels(k)=uicontrol(app.figure,'Style','togglebutton','String',num2str(k),'Tag',num2str(k), ...
        'Position',[18+(k-1)*235 30 217 62],'FontName','Arial','FontSize',11, ...
        'BackgroundColor',key,'Callback',@(~,~) app.setCorner(k));
end
app.writeKey=uicontrol(app.figure,'Style','pushbutton','String','WRITE BODY FILE','Tag','WRITE BODY FILE', ...
    'Position',[1010 45 360 42],'FontName','Arial','FontSize',11,'BackgroundColor',key, ...
    'Callback',@(~,~) app.writeBodyFile);
end
