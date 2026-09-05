function delete(app)
if app.closing, return; end
app.closing=true; trench.audio.stop;
if ~isempty(app.figure) && isgraphics(app.figure), delete(app.figure); end
end
