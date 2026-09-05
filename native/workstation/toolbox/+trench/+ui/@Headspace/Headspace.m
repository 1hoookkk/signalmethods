classdef Headspace < handle
properties
    root
    figure
    frames
    points
    gains
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
    squares
    vertexMarks
    positionMark
    projections
    fieldLabel
    liveAxes
    liveCurve
    vertexAxes
    vertexCurves
    vertexLabels
    cornerAxes
    cornerCurves
    cornerLabels
    keys
    writeKey
    statusText
    dragging = false
    closing = false
end
methods
    function app=Headspace(root,visible)
        if nargin<2, visible='on'; end
        app.root=root; app.frames=trench.model.headspaceFrames(root);
        measures=trench.model.spacePoints(app.frames); app.points=measures(:,1:2); app.gains=measures(:,3);
        [~,~,group]=unique(app.points,'rows');
        for g=unique(group)', same=find(group==g); app.points(same,1)=app.points(same,1)+(0:numel(same)-1)'*.01; end
        app.low=min(app.points); app.span=max(app.points)-app.low; app.span(app.span==0)=1;
        scaled=(app.points-app.low)./app.span;
        app.tri=triangulation(delaunay(scaled(:,1),scaled(:,2)),scaled);
        app.bankPath=fullfile(root,'native','workstation','banks','HEADSPACE.bank.json');
        app.build(visible); app.loadCorners; app.setPosition(app.points(1,:));
    end
    build(app,visible)
    refresh(app)
    setPosition(app,xy)
    onField(app,phase)
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
