function delete(app)
if app.closing, return; end
app.closing=true;
if ~isempty(app.timer) && isvalid(app.timer), stop(app.timer); delete(app.timer); end
trench.audio.stop;
if ~isempty(app.figure) && isgraphics(app.figure), delete(app.figure); end
end
