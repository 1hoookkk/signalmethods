classdef tHeadspace < matlab.unittest.TestCase
properties
    app
end
methods(TestMethodSetup)
    function make(t)
        t.app=trench.ui.Headspace(trench.setup,'off');
        t.app.bankPath=fullfile(tempname,'HEADSPACE.bank.json');
        t.app.corners=cell(1,4); t.app.current=1; t.app.setPosition(0);
        t.addTeardown(@() delete(t.app));
    end
end
methods(Test)
    function h01Space(t)
        p=t.app.probe; f=t.app.frames; groups={f.group};
        t.verifyGreaterThanOrEqual(p.count,70);
        t.verifyGreaterThanOrEqual(nnz(strcmp(groups,'Klatt 1980')),12);
        t.verifyGreaterThanOrEqual(nnz(strcmp(groups,'DVTD')),14);
        t.verifyGreaterThanOrEqual(nnz(strcmp(groups,'HEADS')),40);
        t.verifyTrue(all(diff([f.root])>=0));
        for k=1:numel(f), c=f(k).chord; t.verifyEqual(c(6,[1 4 6]),[0 1 0]); t.verifyGreaterThanOrEqual(c(6,5),trench.bridge.noteOf(18000)); t.verifyEqual(f(k).words,trench.bridge.compile(c)); end
        t.verifyEqual(p.words,trench.bridge.unityDc(f(1).words));
        t.verifyEqual(char(t.app.figure.Visible),'off');
    end
    function h02Position(t)
        t.app.setPosition(12.4); p=t.app.probe;
        t.verifyEqual(p.anchors,[13 14]); t.verifyEqual(p.morph,40,'AbsTol',1e-9);
        [w,c]=trench.model.spaceWords(t.app.frames,12.4);
        t.verifyEqual(p.words,trench.bridge.unityDc(w)); t.verifyEqual(p.chord,c);
        t.verifyTrue(contains(p.status,'MORPH 40'));
        t.verifyTrue(contains(t.app.stripLabel.String,t.app.frames(13).name) && contains(t.app.stripLabel.String,t.app.frames(14).name));
    end
    function h03LogLerp(t)
        f=t.app.frames; [a,b]=trench.bridge.leadTo(f(1).chord,f(2).chord);
        previous=[]; notes=zeros(11,6); widths=zeros(11,6);
        for k=0:10
            t.app.setPosition(k/10); p=t.app.probe;
            if ~isempty(previous), t.verifyNotEqual(p.words,previous); end
            previous=p.words; notes(k+1,:)=p.chord(:,2)'; widths(k+1,:)=p.chord(:,3)';
        end
        on=a(:,1)~=0 & b(:,1)~=0;
        expected=(1-(0:10)'/10)*a(on,2)'+((0:10)'/10)*b(on,2)';
        t.verifyEqual(notes(:,on),expected,'AbsTol',1e-9);
        t.verifyEqual(log(widths(:,on)),(1-(0:10)'/10)*log(max(a(on,3),1e-3))'+((0:10)'/10)*log(max(b(on,3),1e-3))','AbsTol',1e-9);
        d=diff(notes); t.verifyFalse(any(any(d>1e-9,1)&any(d<-1e-9,1)));
    end
    function h04Keys(t)
        t.app.onKey(key('rightarrow')); t.verifyEqual(t.app.probe.position,1);
        t.app.onKey(key('rightarrow','shift')); t.verifyEqual(t.app.probe.position,1.05,'AbsTol',1e-9);
        t.app.onKey(key('rightarrow','control')); t.verifyEqual(t.app.probe.position,1.06,'AbsTol',1e-9);
        t.app.onKey(key('leftarrow')); t.verifyEqual(t.app.probe.position,1);
        t.app.onKey(key('3')); t.verifyEqual(t.app.probe.current,3);
        t.app.setPosition(5.3); live=t.app.probe;
        t.app.onKey(key('s','control')); p=t.app.probe;
        t.verifyEqual(t.app.corners{3}.chord,live.chord); t.verifyEqual(t.app.corners{3}.words,trench.bridge.compile(live.chord));
        t.verifyTrue(startsWith(p.status,'saved to M0 Q1')); t.verifyEmpty(p.corners{1});
        bank=trench.io.openBank(t.app.bankPath); t.verifyEqual(bank.indices,3); t.verifyEqual(bank.frames(1).chord,live.chord,'AbsTol',1e-9);
        t.verifyEqual(t.app.writeKey.Enable,'off');
    end
    function h05Body(t)
        for k=1:4, t.app.setCorner(k); t.app.setPosition(3*k+.5); t.app.saveCorner; end
        t.verifyEqual(t.app.writeKey.Enable,'on');
        path=fullfile(tempname,'headspace.body240'); mkdir(fileparts(path)); t.app.writeBodyFile(path);
        d=dir(path); t.verifyEqual(d.bytes,240);
        raw=zeros(6,5,4,'uint16'); for k=1:4, raw(:,:,k)=t.app.corners{k}.words; end
        corners=trench.bridge.bodyCorners(raw,true(1,6),true);
        t.verifyEqual(trench.bridge.bodyLerp(trench.bridge.bodyBytes(raw,true(1,6),true),0,0),corners(:,:,1));
        t.verifyEqual(t.app.probe.status,path);
        again=trench.ui.Headspace(t.app.root,'off'); t.addTeardown(@() delete(again));
        again.bankPath=t.app.bankPath; again.corners=cell(1,4); again.loadCorners;
        for k=1:4, t.verifyEqual(again.corners{k}.words,t.app.corners{k}.words); end
    end
    function h06Play(t)
        t.app.setPlaying(true); p=t.app.probe;
        t.verifyTrue(p.playing || startsWith(p.status,'no audio device'));
        t.app.setSource('saw'); t.verifyEqual(t.app.probe.source,'saw');
        t.app.setPlaying(false); t.verifyFalse(t.app.probe.playing);
    end
    function h07OneScreen(t)
        h=findall(t.app.figure,'Type','uicontrol'); tags=get(h,'Tag'); tags=tags(~cellfun(@isempty,tags));
        t.verifyEqual(sort(tags(:)'),sort({'PLAY','SAW','PINK NOISE','WRITE BODY FILE'}));
        t.verifyEqual(numel(findall(t.app.figure,'Type','figure')),1);
        t.verifyEqual(t.app.figure.Name,'HEADSPACE');
    end
end
end
function e=key(name,modifier)
if nargin<2, e=struct('Key',name,'Modifier',{{}}); else, e=struct('Key',name,'Modifier',{{modifier}}); end
end
