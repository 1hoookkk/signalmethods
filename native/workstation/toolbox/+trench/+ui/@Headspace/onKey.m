function onKey(app,event)
key=char(event.Key); mods=cellstr(event.Modifier);
control=any(strcmp(mods,'control')); shift=any(strcmp(mods,'shift'));
step=1; if shift, step=.05; elseif control, step=.01; end
switch key
    case 'rightarrow'
        if step==1, app.setPosition(floor(app.position+1e-9)+1); else, app.setPosition(app.position+step); end
    case 'leftarrow'
        if step==1, app.setPosition(ceil(app.position-1e-9)-1); else, app.setPosition(app.position-step); end
    case 'space', app.setPlaying(~app.playing);
    case {'1','2','3','4'}, app.setCorner(str2double(key));
    case 's', if control, app.saveCorner; end
end
end
