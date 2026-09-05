function beginDrag(app,room,method,axes,point)
app.drag=struct('room',room,'method',method,'axes',axes);
room.(method)(point,'press');
end
