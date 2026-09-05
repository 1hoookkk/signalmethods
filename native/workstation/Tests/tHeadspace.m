classdef tHeadspace < matlab.unittest.TestCase
properties
    app
end
methods(TestMethodSetup)
    function make(t)
        t.app=trench.ui.Headspace(trench.setup,'off');
        t.app.bankPath=fullfile(tempname,'HEADSPACE.bank.json');
        t.app.corners=cell(1,4); t.app.current=1; t.app.setPosition(t.app.points(1,:));
        t.addTeardown(@() delete(t.app));
    end
end
methods(Test)
    function h01Space(t)
        p=t.app.probe; f=t.app.frames; groups={f.group}; n=numel(f);
        t.verifyGreaterThanOrEqual(p.count,70);
        t.verifyGreaterThanOrEqual(nnz(strcmp(groups,'Klatt 1980')),12);
        t.verifyGreaterThanOrEqual(nnz(strcmp(groups,'DVTD')),14);
        t.verifyGreaterThanOrEqual(nnz(strcmp(groups,'HEADS')),40);
        t.verifyEqual(size(t.app.points),[n 2]); t.verifyGreaterThan(size(t.app.tri.ConnectivityList,1),n);
        for k=1:n, c=f(k).chord; t.verifyEqual(c(6,[1 4]),[1 1]); t.verifyGreaterThanOrEqual(min(c(6,[3 6])),8); t.verifyLessThanOrEqual(nnz(c(1:5,1)),5); end
        for k=[1 20 n]
            t.app.setPosition(t.app.points(k,:)); q=t.app.probe;
            t.verifyEqual(max(q.weights),1); t.verifyEqual(q.vertices(q.weights==1),k);
            t.verifyEqual(q.words,trench.bridge.unityDc(f(k).words)); t.verifyTrue(contains(q.status,f(k).name));
        end
        t.verifyEqual(char(t.app.figure.Visible),'off');
    end
    function h02Edge(t)
        f=t.app.frames; v=double(t.app.tri.ConnectivityList(1,:)); i=v(1); j=v(2);
        t.app.setPosition(mean(t.app.points([i j],:),1)); p=t.app.probe;
        w=zeros(1,3); w(p.vertices==i)=.5; w(p.vertices==j)=.5;
        t.verifyEqual(p.weights,w,'AbsTol',1e-6);
        [a,b]=trench.bridge.leadTo(f(i).chord,f(j).chord); expected=trench.model.lerpChords(a,b,.5); hz=trench.bridge.curveHz;
        t.verifyEqual(trench.bridge.responseDb(p.words,hz),trench.bridge.responseDb(trench.bridge.unityDc(trench.bridge.compile(expected)),hz),'AbsTol',.05);
        t.verifyEqual(sort(p.chord(p.chord(:,1)~=0,2)),sort(expected(expected(:,1)~=0,2)),'AbsTol',1e-6);
        t.verifyEqual(p.position(1),sum(p.weights.*t.app.points(p.vertices,1)'),'AbsTol',1e-6);
        t.verifyEqual(p.words,trench.bridge.unityDc(trench.bridge.compile(p.chord)));
    end
    function h03Inside(t)
        f=t.app.frames; v=double(t.app.tri.ConnectivityList(1,:));
        t.app.setPosition(mean(t.app.points(v,:),1)); p=t.app.probe;
        t.verifyEqual(sort(p.vertices),sort(v)); t.verifyEqual(p.weights,[1 1 1]/3,'AbsTol',1e-6);
        expected=trench.model.blendChords(cat(3,f(p.vertices).chord),p.weights);
        t.verifyEqual(p.chord,expected,'AbsTol',1e-9);
        for k=v, t.verifyNotEqual(p.words,trench.bridge.unityDc(f(k).words)); t.verifyTrue(contains(p.status,f(k).name)); end
        on=p.chord(:,1)~=0; t.verifyTrue(all(p.chord(on,3)>0));
    end
    function h04Keys(t)
        [~,order]=sort(t.app.points(:,1)); t.app.setPosition(t.app.points(order(1),:));
        t.app.onKey(key('rightarrow')); t.verifyEqual(t.app.probe.position,t.app.points(order(2),:));
        before=t.app.probe.position;
        t.app.onKey(key('rightarrow','shift')); t.verifyEqual(t.app.probe.position,before+[1 0],'AbsTol',1e-9);
        t.app.onKey(key('uparrow','control')); t.verifyEqual(t.app.probe.position,before+[1 .2],'AbsTol',1e-9);
        t.app.onKey(key('3')); t.verifyEqual(t.app.probe.current,3);
        live=t.app.probe; t.app.onKey(key('s','control')); p=t.app.probe;
        t.verifyEqual(t.app.corners{3}.chord,live.chord); t.verifyEqual(t.app.corners{3}.words,trench.bridge.compile(live.chord));
        t.verifyTrue(startsWith(p.status,'saved to M0 Q1')); t.verifyEmpty(p.corners{1});
        bank=trench.io.openBank(t.app.bankPath); t.verifyEqual(bank.indices,3); t.verifyEqual(bank.frames(1).chord,live.chord,'AbsTol',1e-9);
        t.verifyEqual(t.app.writeKey.Enable,'off');
    end
    function h05Body(t)
        v=double(t.app.tri.ConnectivityList(1,:)); spots=[t.app.points(v,:); mean(t.app.points(v,:),1)];
        for k=1:4, t.app.setCorner(k); t.app.setPosition(spots(k,:)); t.app.saveCorner; end
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
