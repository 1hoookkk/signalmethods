classdef BodyGroup < handle
properties
    app
    controls
    cornerKeys
    useKeys
    writeKey
end
methods
    function obj=BodyGroup(app)
        obj.app=app; obj.controls=trench.ui.ControlPanel(app.figure,[12 31 382 146]);
        names={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
        obj.cornerKeys=gobjects(1,4); obj.useKeys=gobjects(1,4);
        for k=1:4
            x=10+mod(k-1,2)*183; y=110-floor((k-1)/2)*28;
            obj.cornerKeys(k)=obj.controls.key(names{k},[x y 176 24],@(~,~) app.chooseCorner(k));
            obj.useKeys(k)=obj.controls.key(['USE AS ' names{k}],[10+(k-1)*91 50 86 24],@(~,~) app.useAs(k));
        end
        for k=1:6
            h=obj.controls.key(num2str(k),[10+(k-1)*25 16 23 25],@(s,~) app.setRow(k,logical(s.Value)),true); h.Value=1;
        end
        h=obj.controls.key('UNITY DC',[164 16 78 25],@(s,~) app.setUnity(logical(s.Value)),true); h.Value=1;
        obj.writeKey=obj.controls.key('WRITE BODY FILE',[246 16 127 25],@(~,~) app.writeBodyFile);
    end
    function refresh(obj)
        app=obj.app; names=app.cornerLabels; live=app.liveCorners;
        for k=1:4
            set(obj.cornerKeys(k),'String',names{k},'Enable',trench.ui.draw.onOff(app.corners(k)>0));
            color=[.886 .886 .886]; if app.selectedCorner==k && app.corners(k)>0, color=[.851 .325 .098]; end
            if live(k), color=[.769 .561 0]; end
            set(obj.cornerKeys(k),'BackgroundColor',color);
            set(obj.useKeys(k),'Enable',trench.ui.draw.onOff(app.chosen>0 || strcmp(app.live.kind,'line')));
        end
        set(obj.writeKey,'Enable',trench.ui.draw.onOff(all(app.corners>0)));
    end
end
end
