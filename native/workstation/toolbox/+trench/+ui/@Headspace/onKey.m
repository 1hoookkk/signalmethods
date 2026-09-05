function onKey(app,event)
key=char(event.Key); mods=cellstr(event.Modifier);
control=any(strcmp(mods,'control')); shift=any(strcmp(mods,'shift'));
nudge=0; if shift, nudge=1; elseif control, nudge=.2; end
[~,here]=max(app.weights); here=app.vertices(here);
switch key
    case {'rightarrow','leftarrow'}
        sign=1; if strcmp(key,'leftarrow'), sign=-1; end
        if nudge, app.setPosition(app.position+[sign*nudge 0]); else, app.setPosition(app.points(step(app.points(:,1),here,sign),:)); end
    case {'uparrow','downarrow'}
        sign=1; if strcmp(key,'downarrow'), sign=-1; end
        if nudge, app.setPosition(app.position+[0 sign*nudge]); else, app.setPosition(app.points(step(app.points(:,2),here,sign),:)); end
    case 'space', app.setPlaying(~app.playing);
    case {'1','2','3','4'}, app.setCorner(str2double(key));
    case 's', if control, app.saveCorner; end
end
end
function next=step(values,here,sign)
[~,order]=sort(values); at=find(order==here,1); next=order(max(1,min(numel(order),at+sign)));
end
