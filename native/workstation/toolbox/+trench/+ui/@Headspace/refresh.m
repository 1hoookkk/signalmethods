function refresh(app)
if app.closing || isempty(app.figure) || ~isgraphics(app.figure), return; end
n=numel(app.frames); p=app.position; a=floor(p)+1; b=min(n,a+1); t=p-floor(p); hz=trench.bridge.curveHz;
set(app.positionLine,'XData',[p p]); set(app.positionMark,'XData',p);
set(app.liveCurve,'YData',trench.bridge.responseDb(app.words,hz));
fa=app.frames(a); fb=app.frames(b);
set(app.beforeCurve,'YData',trench.bridge.responseDb(trench.bridge.unityDc(fa.words),hz));
set(app.afterCurve,'YData',trench.bridge.responseDb(trench.bridge.unityDc(fb.words),hz));
set(app.beforeLabel,'String',sprintf('%d  %s   %s',a,fa.name,fa.group));
set(app.afterLabel,'String',sprintf('%d  %s   %s',b,fb.name,fb.group));
set(app.stripLabel,'String',sprintf('%d  %s      MORPH %d      >  %d  %s',a,fa.name,round(100*t),b,fb.name));
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
for k=1:4
    color='k'; width=.5; if k==app.current, color=[.851 .325 .098]; width=2; end
    set(app.cornerAxes(k),'XColor',color,'YColor',color,'LineWidth',width);
    if isempty(app.corners{k})
        set(app.cornerCurves(k),'Visible','off'); set(app.cornerLabels(k),'String',labels{k});
    else
        set(app.cornerCurves(k),'Visible','on','YData',trench.bridge.responseDb(trench.bridge.unityDc(app.corners{k}.words),hz));
        set(app.cornerLabels(k),'String',[labels{k} '   ' app.corners{k}.name]);
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
