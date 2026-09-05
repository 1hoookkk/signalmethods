function tick(room)
if room.liveInput
    n=trench.audio.inputWritten;
    if n-room.lastInput<room.app.audioRate/5, return; end
    room.lastInput=n; room.sound.mono=trench.audio.inputRing;
    room.envelopeDb=[]; room.drawSound; room.readSlice;
elseif room.app.playing && strcmp(room.app.source,'sample')
    t=trench.audio.playhead;
    set(room.cursorLine,'XData',[t t]);
end
end
