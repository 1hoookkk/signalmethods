function routeMouse(app,phase)
if isempty(app.drag), return; end
d=app.drag; p=get(d.axes,'CurrentPoint');
d.room.(d.method)(p(1,1:2),phase);
if strcmp(phase,'release'), app.drag=[]; end
end
