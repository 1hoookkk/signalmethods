classdef CornerRoom < handle
properties
    app
    controls
    display
    chord = []
    original = []
    name = ''
    dirty = false
    locks = true(6,1)
    showHz = false
    rowAxes
    rowCurves
    wholeCurves
    poles
    zeros
    pitchBoxes
    widthBoxes
    amountBoxes
    zeroLabels
    intervalLabels
    hzLabels
    wholeAxes
    wholeCurve
    activeRow = 1
    voice = 'pole'
    dragStart
    chordStart
end
methods
    function room=CornerRoom(app)
        room.app=app; room.controls=trench.ui.ControlPanel(app.figure,[12 500 382 376]);
        room.display=uipanel(app.figure,'Units','pixels','Position',[414 31 1054 895],'BackgroundColor','w','BorderType','line');
        room.build;
    end
    build(room)
    buildRows(room)
    beginEdit(room)
    finishEdit(room)
    refresh(room)
    landChord(room,chord)
    setPitch(room,row,value)
    setWidth(room,row,value)
    setAmount(room,row,value)
    setCeiling(room)
    setOpen(room,value)
    sharpen(room)
    copyTo(room,index)
    setLock(room,row,on)
    startVoice(room,row,voice)
    onPole(room,point,phase)
    onZero(room,point,phase)
    setShowHz(room,on)
end
end
