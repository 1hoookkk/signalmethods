classdef Headspace < handle
properties
    root
    figure
    frames
    points
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
    shade
    brightness
    contours
    edge
    positionMark
    fieldLabel
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
        layout=jsondecode(fileread(fullfile(root,'native','workstation','data','headspace-layout.json')));
        assert(isequal(string(layout.names(:)),string({app.frames.group})'+": "+string({app.frames.name})'));
        app.points=layout.points;
        app.tri=triangulation(layout.triangles,app.points);
        app.brightness=trench.headspace.brightness(app.frames);
        app.bankPath=fullfile(root,'native','workstation','banks','HEADSPACE.bank.json');
        app.build(visible); app.loadCorners; trench.audio.wet(true); app.setPosition(app.points(1,:)); app.setSource('noise');
    end
    build(app,visible)
    refresh(app)
    setPosition(app,xy)
    onField(app,phase,point)
    frames=referenceFrames(app)
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
