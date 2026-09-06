function onSurface(app,phase,point)
if ~strcmp(phase,'press'), return; end
if nargin<3, p=app.surfaceAxes.CurrentPoint; point=p(1,1:2); end
span=[diff(app.surfaceAxes.XLim) diff(app.surfaceAxes.YLim)];
d=sqrt(sum(((app.points(:,1:2)-point(:)')./span).^2,2)); [nearest,k]=min(d);
if nearest<=.03, app.selectAnchor(k); end
end
