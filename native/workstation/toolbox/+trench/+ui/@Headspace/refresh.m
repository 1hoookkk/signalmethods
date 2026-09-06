function refresh(app)
if app.closing || isempty(app.figure) || ~isgraphics(app.figure), return; end
set(app.liveCurve,'YData',trench.bridge.responseDb(app.words,trench.bridge.curveHz));
for k=1:4
    label=num2str(k);
    set(app.cornerLabels(k),'FontWeight',pick(isempty(app.corners{k}),'normal','bold'));
    set(app.cornerLabels(k),'String',label,'Value',double(k==app.current), ...
        'ForegroundColor',pick(k==app.current,'w','k'), ...
        'BackgroundColor',pick(k==app.current,[217 83 25]/255,[226 226 226]/255));
end
set(app.writeKey,'Enable',trench.ui.draw.onOff(all(~cellfun(@isempty,app.corners))),'TooltipString',app.status);
states=[app.playing strcmp(app.source,'saw') strcmp(app.source,'noise')];
for k=1:3
    set(app.keys(k),'Value',states(k),'BackgroundColor',pick(states(k),[43 43 43]/255,[226 226 226]/255), ...
        'ForegroundColor',pick(states(k),'w','k'));
end
set(app.keys(1),'TooltipString',app.status);
if strcmp(app.figure.Visible,'on'), drawnow limitrate; end
end
function v=pick(tf,a,b), v=b; if tf, v=a; end, end
