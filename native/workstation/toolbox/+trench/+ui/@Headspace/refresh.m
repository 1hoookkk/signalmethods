function refresh(app)
if app.closing || isempty(app.figure) || ~isgraphics(app.figure), return; end
hz=trench.bridge.curveHz; labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
if app.selected>0, set(app.selectedMark,'XData',app.points(app.selected,1),'YData',app.points(app.selected,2));
else, set(app.selectedMark,'XData',NaN,'YData',NaN); end
for k=1:4
    if app.corners(k)>0, set(app.slotLabels(k),'Position',[app.points(app.corners(k),1)+1.2 app.points(app.corners(k),2)+.8 0],'String',num2str(k));
    else, set(app.slotLabels(k),'Position',[NaN NaN 0]); end
end
set(app.padMark,'XData',app.morph,'YData',app.q);
set(app.padProjection,'XData',[0 app.morph app.morph],'YData',[app.q app.q 0]);
set(app.liveCurve,'YData',trench.bridge.responseDb(app.words,hz));
for k=1:4
    color='k'; width=.5; if k==app.current, color=[.851 .325 .098]; width=2; end
    set(app.cornerAxes(k),'XColor',color,'YColor',color,'LineWidth',width);
    if app.corners(k)==0
        set(app.cornerCurves(k),'Visible','off'); set(app.cornerLabels(k),'String',labels{k}); set(app.padNames(k),'String',labels{k});
    else
        f=app.frames(app.corners(k));
        set(app.cornerCurves(k),'Visible','on','YData',trench.bridge.responseDb(trench.bridge.unityDc(f.words),hz));
        name=[labels{k} '   ' f.name]; if numel(name)>40, name=[name(1:39) char(8230)]; end
        set(app.cornerLabels(k),'String',name); set(app.padNames(k),'String',f.name);
    end
end
set(app.writeKey,'Enable',trench.ui.draw.onOff(all(app.corners>0)));
states=[app.playing strcmp(app.source,'saw') strcmp(app.source,'noise')];
for k=1:3
    set(app.keys(k),'Value',states(k),'BackgroundColor',pick(states(k),[.169 .169 .169],[.886 .886 .886]),'ForegroundColor',pick(states(k),'w','k'));
end
set(app.statusText,'String',app.status);
if strcmp(app.figure.Visible,'on'), drawnow limitrate; end
end
function v=pick(tf,a,b), v=b; if tf, v=a; end, end
