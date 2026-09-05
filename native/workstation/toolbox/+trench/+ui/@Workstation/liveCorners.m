function on = liveCorners(app)
on=false(1,4); live=app.live;
switch live.kind
    case 'frame'
        if ~(isfield(live,'library') && live.library), on=app.corners==live.index & app.corners>0; end
    case 'edit', on(live.corner)=true;
    case 'cornerAlone', on(live.index)=true;
    case 'wheel'
        at=[app.morph==0 & app.q==0, app.morph==100 & app.q==0, app.morph==0 & app.q==100, app.morph==100 & app.q==100];
        on=at & app.corners>0;
end
end
