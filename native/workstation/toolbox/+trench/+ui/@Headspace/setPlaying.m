function setPlaying(app,on)
if on
    info=trench.audio.start;
    if isempty(info.device), app.status=['no audio device   ' info.error]; app.playing=false;
    else, app.status=sprintf('%s %g Hz',info.device,info.rateHz); app.playing=true; end
else
    app.playing=false;
end
trench.audio.playing(app.playing); app.refresh;
end
