classdef Workstation < handle
properties
    root
    figure
    room = 'frames'
    live = struct('kind','none')
    library
    bank
    chosen = 0
    chosenLibrary = 0
    corners = zeros(1,4)
    copies = zeros(1,4)
    selectedCorner = 1
    morph = 0
    q = 0
    rowOn = true(1,6)
    unity = true
    playing = false
    filterOn = true
    source = 'saw'
    status = ''
    audioRate = 44100
    timer
    rooms
    response
    body
    statusText
    nav
    previousWords = uint16([])
    prepared = uint16([])
    drag = []
    closing = false
end
methods
    function app=Workstation(root,visible)
        if nargin<2, visible='on'; end
        app.root=root; app.library=trench.bridge.loadFrames(root);
        bankPath=fullfile(root,'native','workstation','banks','P2K.bank.json');
        if ~isfile(bankPath), trench.io.makeFactoryBanks(root); end
        app.bank=trench.io.openBank(bankPath);
        app.buildFigure(visible);
        app.rooms.frames.refresh;
        app.setRoom('frames');
        if strcmp(visible,'on')
            app.timer=timer('ExecutionMode','fixedSpacing','Period',1/30,'BusyMode','drop', ...
                'TimerFcn',@(~,~) app.tick(1/30));
            start(app.timer);
        end
    end
    buildFigure(app,visible)
    setRoom(app,name)
    words=wordsOf(app)
    p=probe(app)
    refresh(app)
    tick(app,dt)
    selectFrame(app,slot)
    selectLibrary(app,index)
    [slot,status]=putFrame(app,frame)
    useAs(app,corner)
    names=cornerNames(app)
    chooseCorner(app,index)
    showCornerFrame(app,index)
    c=rawCorners(app)
    c=bodyCorners(app)
    setPlaying(app,on)
    setSource(app,name)
    setFilter(app,on)
    setRow(app,row,on)
    setUnity(app,on)
    path=writeBodyFile(app,path)
    setStatus(app,status)
    routeMouse(app,phase)
    beginDrag(app,room,method,axes,point)
    saveBank(app,path)
    openBank(app,path)
    newBank(app)
    removeFromBank(app)
    delete(app)
end
end
