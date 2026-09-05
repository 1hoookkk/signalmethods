function curve = responseCurve(ax, color)
if nargin<2, color=[0 .447 .741]; end
hz=trench.bridge.curveHz;
curve=line(ax,hz,zeros(size(hz)),'Color',color,'LineWidth',1.25,'HitTest','off');
end
