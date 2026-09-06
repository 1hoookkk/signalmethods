function onField(app,phase,point)
if nargin<3, p=app.fieldAxes.CurrentPoint; point=p(1,1:2); end
switch phase
    case 'press', app.dragging=true;
    case 'drag', if ~app.dragging, return; end
    case 'release', if ~app.dragging, return; end, app.dragging=false;
end
app.setPosition(point);
end
