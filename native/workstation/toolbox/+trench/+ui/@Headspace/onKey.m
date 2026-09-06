function onKey(app,event)
key=char(event.Key); mods=cellstr(event.Modifier); control=any(strcmp(mods,'control'));
nudge=1; if control, nudge=.2; end
switch key
    case {'1','2','3','4'}, app.placeCorner(str2double(key));
    case 's', if control, app.placeCorner(app.current); end
    case 'space', app.setPlaying(~app.playing);
    case 'rightarrow', app.setWheel(app.morph+nudge,app.q);
    case 'leftarrow', app.setWheel(app.morph-nudge,app.q);
    case 'uparrow', app.setWheel(app.morph,app.q+nudge);
    case 'downarrow', app.setWheel(app.morph,app.q-nudge);
end
end
