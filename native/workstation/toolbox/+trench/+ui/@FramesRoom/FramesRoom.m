classdef FramesRoom < handle
properties
    app
    controls
    display
    bankList
    libraryList
    bankTitle
    map
    marks
    dimMarks
    pathLine
    hopLine
    anchorLabels
    cornerLabels
    positionMark
    projections
    order = []
    libraryOrder = []
    sortName = 'SLOT'
    librarySort = 'LOW > HIGH ROOT'
    groups = logical([1 0 1 1 1 1 1])
    position = 0
    sweep = false
    hopSeconds = 4
    direction = 1
    fine = false
    positionSlider
    positionAxes
    positionThumb
    positionBox
    anchorBox
    morphBox
    pairLabel
    dragStart = [0 0]
end
methods
    function room=FramesRoom(app)
        room.app=app; room.controls=trench.ui.ControlPanel(app.figure,[12 500 382 376]);
        room.display=uipanel(app.figure,'Units','pixels','Position',[414 31 1054 895],'BackgroundColor','w','BorderType','line');
        room.build;
    end
    build(room)
    refresh(room)
    refreshLibrary(room)
    drawMap(room)
    onRow(room,point,phase)
    onLibraryRow(room,point,phase)
    onMap(room,point,phase)
    onPosition(room,point,phase)
    setPosition(room,position)
    name=positionName(room)
    setSort(room,name)
    setGroup(room,k,on)
    putLibrary(room)
    tick(room,dt)
end
end
