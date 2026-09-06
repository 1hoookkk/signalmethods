function onPad(app,phase,point)
switch phase
    case 'press', app.dragging='pad';
    case 'drag', if ~strcmp(app.dragging,'pad'), return; end
    case 'release', if ~strcmp(app.dragging,'pad'), return; end, app.dragging='';
end
if nargin<3, p=app.padAxes.CurrentPoint; point=p(1,1:2); end
app.setWheel(point(1),point(2));
end
