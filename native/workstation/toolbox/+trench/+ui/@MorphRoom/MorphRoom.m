classdef MorphRoom < handle
properties
    app
    controls
    display
    pad
    positionMark
    projection
    cornerKeys
    morphBox
    qBox
    fine = false
    past = false
    sweepMorph = false
    sweepQ = false
    direction = [1 1]
    dragStart = [0 0]
    valueStart = [0 0]
end
methods
    function room=MorphRoom(app)
        room.app=app; room.controls=trench.ui.ControlPanel(app.figure,[12 500 382 376]);
        room.display=uipanel(app.figure,'Units','pixels','Position',[414 31 1054 895],'BackgroundColor','w','BorderType','line');
        room.build;
    end
    build(room)
    refresh(room)
    onPad(room,point,phase)
    onHear(room,point,phase)
    setPosition(room,morph,q)
    keepAsFrame(room)
    nextFrame(room,direction)
    tick(room,dt)
end
end
