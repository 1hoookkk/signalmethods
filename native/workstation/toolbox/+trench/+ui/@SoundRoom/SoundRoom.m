classdef SoundRoom < handle
properties
    app
    controls
    display
    axes
    surface
    image
    cursorLine
    inLine
    outLine
    voiceMarks
    titleText
    soundList
    paths
    sound = []
    cursor = 0
    region = [0 1]
    fftSize = 2048
    stride = 512
    window = 'Hann'
    mode = 'speech'
    threeD = false
    logF = true
    axesOn = true
    envelope = false
    gain = 0
    floor = -80
    sliceChord = []
    frequencies = []
    times = []
    db = []
    envelopeDb = []
    envelopeHz = []
    sliceEnvelope = []
    slicePeaks = []
    cursorSlider
    cursorBox
    inBox
    outBox
    liveInput = false
    lastInput = 0
    dragStart
    viewStart
end
methods
    function room=SoundRoom(app)
        room.app=app; room.controls=trench.ui.ControlPanel(app.figure,[12 500 382 376]);
        room.display=uipanel(app.figure,'Units','pixels','Position',[414 31 1054 895],'BackgroundColor','w','BorderType','line');
        room.paths=trench.io.scanSounds(app.root); room.build;
    end
    build(room)
    openSound(room,path)
    drawSound(room)
    readSlice(room)
    block=windowAt(room,time)
    readFrameAtCursor(room)
    setSetting(room,name,value)
    setCursor(room,value)
    setRegion(room,a,b)
    setLiveInput(room)
    onCursor(room,point,phase)
    onIn(room,point,phase)
    onOut(room,point,phase)
    onOrbit(room,point,phase)
    tick(room)
end
end
