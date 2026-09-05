function setSource(app,name)
if app.response.measureOn && ~strcmp(name,'white'), return; end
app.source=char(name); trench.audio.source(name);
names={'SAW','PINK NOISE','SAMPLE','LIVE IN'}; values={'saw','noise','sample','input'};
for k=1:4
    h=app.response.controls.keys.(matlab.lang.makeValidName(names{k}));
    on=strcmp(name,values{k}); h.Value=on;
    h.BackgroundColor=[.886 .886 .886]; h.ForegroundColor='k';
    if on, h.BackgroundColor=[.169 .169 .169]; h.ForegroundColor='w'; end
end
app.refresh;
end
