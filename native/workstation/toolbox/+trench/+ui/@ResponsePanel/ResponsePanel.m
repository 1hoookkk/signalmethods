classdef ResponsePanel < handle
properties
    app
    controls
    axes
    curve
    measured
    spectrum
    excess
    meter
    device
    measureOn = false
    oldSource = 'saw'
end
methods
    function obj=ResponsePanel(app)
        obj.app=app;
        obj.controls=trench.ui.ControlPanel(app.figure,[12 186 382 304]);
        obj.axes=trench.ui.draw.fixedGrid(obj.controls.panel,[43 103 291 194]);
        obj.spectrum=trench.ui.draw.responseCurve(obj.axes,[.75 .75 .75]);
        obj.measured=trench.ui.draw.responseCurve(obj.axes,[.6 .6 .6]);
        set([obj.spectrum obj.measured],'Visible','off');
        obj.curve=trench.ui.draw.responseCurve(obj.axes);
        obj.excess=line(obj.axes,NaN,NaN,'LineStyle','none','Marker','|','Color',[0 .447 .741]);
        obj.meter=uicontrol(obj.controls.panel,'Style','text','String','','Position',[355 104 9 1],'BackgroundColor',[.769 .561 0]);
        obj.controls.key('PLAY',[10 67 60 24],@(~,~) app.setPlaying(true));
        obj.controls.key('STOP',[74 67 60 24],@(~,~) app.setPlaying(false));
        h=obj.controls.key('FILTER ON',[140 67 100 24],@(s,~) app.setFilter(logical(s.Value)),true); h.Value=1;
        obj.controls.key('MEASURE',[246 67 124 24],@(s,~) obj.setMeasure(logical(s.Value)),true);
        names={'SAW','PINK NOISE','SAMPLE','LIVE IN'};
        sources={'saw','noise','sample','input'};
        for k=1:4
            obj.controls.key(names{k},[10+(k-1)*91 37 86 24],@(~,~) app.setSource(sources{k}),true);
        end
        obj.device=obj.controls.label('no audio device',[10 8 360 20]);
    end
    function setMeasure(obj,on)
        obj.measureOn=on;
        if on, obj.oldSource=obj.app.source; obj.app.setSource('white'); else, obj.app.setSource(obj.oldSource); end
        set(obj.measured,'Visible',trench.ui.draw.onOff(on));
        obj.app.refresh;
    end
    function refresh(obj,words)
        hz=trench.bridge.curveHz;
        set(obj.curve,'YData',trench.bridge.responseDb(words,hz));
        e=trench.bridge.excessOf(words);
        if isempty(e), set(obj.excess,'XData',NaN,'YData',NaN);
        else, set(obj.excess,'XData',e(:,1),'YData',60*e(:,2)-30); end
    end
    function tick(obj)
        p=trench.audio.peak;
        set(obj.meter,'Position',[355 104 9 max(1,194*min(1,p))]);
        if obj.measureOn
            x=trench.audio.snapshot;
            [power,f]=pwelch(x,hann(4096),2048,4096,obj.app.audioRate);
            db=10*log10(max(power,realmin));
            set(obj.measured,'YData',interp1(f,db,trench.bridge.curveHz,'linear','extrap'));
        end
    end
end
end
