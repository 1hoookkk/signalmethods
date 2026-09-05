function refresh(app)
if app.closing || isempty(app.figure) || ~isgraphics(app.figure), return; end
words=app.wordsOf;
if ~isequal(words,app.previousWords)
    app.response.refresh(words); trench.audio.words(words); app.previousWords=words;
end
set(app.statusText,'String',app.status);
app.body.refresh;
set(app.nav(3:4),'Enable',trench.ui.draw.onOff(all(app.corners>0)));
if strcmp(app.figure.Visible,'on'), drawnow limitrate; end
end
