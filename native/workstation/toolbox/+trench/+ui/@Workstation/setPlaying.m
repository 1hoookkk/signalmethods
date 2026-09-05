function setPlaying(app,on)
if on
    info=trench.audio.start;
    app.audioRate=info.rateHz;
    if isempty(info.device)
        app.status=['no audio device   ' info.error]; app.playing=false;
        set(app.response.device,'String','no audio device');
    else
        app.status=sprintf('%s %g Hz',info.device,info.rateHz); app.playing=true;
        set(app.response.device,'String',app.status);
    end
else
    app.playing=false;
end
trench.audio.playing(app.playing); app.refresh;
end
