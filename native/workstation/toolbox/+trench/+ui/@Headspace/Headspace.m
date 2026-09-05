classdef Headspace < handle
properties
    root
    figure
    frames
    bankPath
    position = 0
    words = uint16([])
    chord = []
    corners = cell(1,4)
    current = 1
    playing = false
    source = 'noise'
    status = ''
    strip
    stripAxes
    squares
    positionMark
    positionLine
    stripLabel
    liveAxes
    liveCurve
    beforeAxes
    beforeCurve
    beforeLabel
    afterAxes
    afterCurve
    afterLabel
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
        app.bankPath=fullfile(root,'native','workstation','banks','HEADSPACE.bank.json');
        app.build(visible); app.loadCorners; app.setPosition(0);
    end
    build(app,visible)
    refresh(app)
    setPosition(app,p)
    onStrip(app,phase)
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
