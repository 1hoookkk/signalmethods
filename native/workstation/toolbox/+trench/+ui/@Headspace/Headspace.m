classdef Headspace < handle
properties
    root
    figure
    frames
    points
    bankPath
    selected = 0
    corners = zeros(1,4)
    current = 1
    morph = 0
    q = 0
    live = 'anchor'
    words = uint16([])
    playing = false
    source = 'noise'
    status = ''
    surfaceAxes
    squares
    selectedMark
    slotLabels
    padAxes
    padMark
    padProjection
    padNames
    liveAxes
    liveCurve
    cornerAxes
    cornerCurves
    cornerLabels
    keys
    writeKey
    statusText
    dragging = ''
    closing = false
end
methods
    function app=Headspace(root,visible)
        if nargin<2, visible='on'; end
        app.root=root; app.frames=trench.model.headspaceFrames(root);
        app.points=trench.model.spacePoints(app.frames);
        app.bankPath=fullfile(root,'native','workstation','banks','HEADSPACE.bank.json');
        app.build(visible); app.loadCorners;
        first=1; if any(app.corners), first=app.corners(find(app.corners,1)); end
        app.selectAnchor(first); app.setSource('noise');
    end
    build(app,visible)
    refresh(app)
    selectAnchor(app,k)
    placeCorner(app,slot)
    setWheel(app,morph,q)
    raw=rawCorners(app)
    report=hopReport(app)
    onSurface(app,phase,point)
    onPad(app,phase,point)
    onKey(app,event)
    path=writeBodyFile(app,path)
    setPlaying(app,on)
    setSource(app,name)
    loadCorners(app)
    saveCorners(app)
    p=probe(app)
    delete(app)
end
end
