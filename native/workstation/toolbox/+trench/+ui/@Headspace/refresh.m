function refresh(app)
if app.closing || isempty(app.figure) || ~isgraphics(app.figure), return; end
hz=trench.bridge.curveHz; p=app.position; x=app.fieldAxes.XLim; y=app.fieldAxes.YLim;
set(app.positionMark,'XData',p(1),'YData',p(2));
set(app.projections,'XData',[x(1) p(1) p(1)],'YData',[p(2) p(2) y(1)]);
on=app.weights>0; v=app.vertices;
set(app.vertexMarks,'XData',app.points(v(on),1),'YData',app.points(v(on),2));
set(app.liveCurve,'YData',trench.bridge.responseDb(app.words,hz));
parts={};
for k=1:3
    f=app.frames(v(k));
    if on(k)
        set(app.vertexCurves(k),'Visible','on','YData',trench.bridge.responseDb(trench.bridge.unityDc(f.words),hz));
        set(app.vertexLabels(k),'String',sprintf('%d  %s   %s   %d',v(k),f.name,f.group,round(100*app.weights(k))));
        parts{end+1}=sprintf('%d  %s  %d',v(k),f.name,round(100*app.weights(k)));
    else
        set(app.vertexCurves(k),'Visible','off'); set(app.vertexLabels(k),'String','');
    end
end
set(app.fieldLabel,'String',strjoin(parts,['   ' char(183) '   ']));
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
for k=1:4
    color='k'; width=.5; if k==app.current, color=[.851 .325 .098]; width=2; end
    set(app.cornerAxes(k),'XColor',color,'YColor',color,'LineWidth',width);
    if isempty(app.corners{k})
        set(app.cornerCurves(k),'Visible','off'); set(app.cornerLabels(k),'String',labels{k});
    else
        set(app.cornerCurves(k),'Visible','on','YData',trench.bridge.responseDb(trench.bridge.unityDc(app.corners{k}.words),hz));
        label=[labels{k} '   ' app.corners{k}.name]; if numel(label)>44, label=[label(1:43) char(8230)]; end
        set(app.cornerLabels(k),'String',label);
    end
end
set(app.writeKey,'Enable',trench.ui.draw.onOff(all(~cellfun(@isempty,app.corners))));
states=[app.playing strcmp(app.source,'saw') strcmp(app.source,'noise')];
for k=1:3
    set(app.keys(k),'Value',states(k),'BackgroundColor',pick(states(k),[.169 .169 .169],[.886 .886 .886]),'ForegroundColor',pick(states(k),'w','k'));
end
set(app.statusText,'String',app.status);
if strcmp(app.figure.Visible,'on'), drawnow limitrate; end
end
function v=pick(tf,a,b), v=b; if tf, v=a; end, end
