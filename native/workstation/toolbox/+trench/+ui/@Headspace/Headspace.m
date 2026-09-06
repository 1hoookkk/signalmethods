classdef Headspace < handle
properties
    root
    figure
    frames
    points
    low
    span
    tri
    bankPath
    position = [0 0]
    vertices = [1 1 1]
    weights = [1 0 0]
    words = uint16([])
    chord = []
    corners = cell(1,4)
    current = 1
    playing = false
    source = 'noise'
    status = ''
    fieldAxes
    positionMark
    referenceLabels
    quadrilateral
    formantBoxes
    liveAxes
    liveCurve
    cornerLabels
    keys
    writeKey
    dragging = false
    closing = false
end
methods
    function app=Headspace(root,visible)
        if nargin<2, visible='on'; end
        app.root=root; app.frames=app.referenceFrames;
        app.points=zeros(numel(app.frames),2);
        for k=1:numel(app.frames), app.points(k,:)=app.frames(k).chord([2 1],2)'; end
        app.low=min(app.points); app.span=max(app.points)-app.low; app.span(app.span==0)=1;
        scaled=(app.points-app.low)./app.span;
        app.tri=triangulation(delaunay(scaled(:,1),scaled(:,2)),scaled);
        app.quadrilateral=reshape(trench.bridge.noteOf([4400 120;400 120;1250 1200;2900 1200]),4,2);
        app.bankPath=fullfile(root,'native','workstation','banks','HEADSPACE.bank.json');
        app.build(visible); app.loadCorners; app.setPosition(app.points(1,:)); app.setSource('noise');
    end
    build(app,visible)
    arrangeReferences(app)
    refresh(app)
    setPosition(app,xy)
    onField(app,phase,point)
    frames=referenceFrames(app)
    setFormant(app,row,hz)
    point=limitPosition(app,point)
    [chord,vertices,weights]=chordAt(app,point)
    onKey(app,event)
    setCorner(app,k)
    saveCorner(app)
    path=writeBodyFile(app,path)
    setPlaying(app,on)
    setSource(app,name)
    loadCorners(app)
    p=probe(app)
    delete(app)
end
end
