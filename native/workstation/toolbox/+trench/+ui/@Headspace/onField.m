function onField(app,phase)
switch phase
    case 'press', app.dragging=true;
    case 'drag', if ~app.dragging, return; end
    case 'release', if ~app.dragging, return; end, app.dragging=false;
end
p=get(app.fieldAxes,'CurrentPoint'); app.setPosition(p(1,1:2));
end
