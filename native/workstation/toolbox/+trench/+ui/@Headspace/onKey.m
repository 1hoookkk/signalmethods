function onKey(app,event)
key=char(event.Key); mods=cellstr(event.Modifier); control=any(strcmp(mods,'control'));
focus=app.figure.CurrentObject;
if ~isempty(focus) && isgraphics(focus,'uicontrol') && strcmp(focus.Style,'edit') && ~(control && strcmp(key,'s')), return; end
nudge=1; if control, nudge=.2; end
switch key
    case 'rightarrow', app.setPosition(app.position+[-nudge 0]);
    case 'leftarrow', app.setPosition(app.position+[nudge 0]);
    case 'uparrow', app.setPosition(app.position+[0 -nudge]);
    case 'downarrow', app.setPosition(app.position+[0 nudge]);
    case 'space', app.setPlaying(~app.playing);
    case {'1','2','3','4'}, app.setCorner(str2double(key));
    case 's', if control, app.saveCorner; end
end
end
