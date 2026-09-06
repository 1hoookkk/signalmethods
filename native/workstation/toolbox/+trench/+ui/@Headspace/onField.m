function onField(app,phase,point)
if nargin<3, p=app.fieldAxes.CurrentPoint; point=p(1,1:2); end
switch phase
    case 'press', app.dragging=true;
    case 'drag', if ~app.dragging, return; end
    case 'release', if ~app.dragging, return; end, app.dragging=false;
end
pixels=app.fieldAxes.Position(3:4)./[diff(app.fieldAxes.XLim) diff(app.fieldAxes.YLim)];
[distance,index]=min(sum(((app.points-point).*pixels).^2,2));
if distance<=6^2, point=app.points(index,:); end
app.setPosition(point);
end
