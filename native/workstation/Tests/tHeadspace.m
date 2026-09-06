classdef tHeadspace < matlab.unittest.TestCase
properties
    app
end
methods(TestMethodSetup)
    function make(t)
        t.app=trench.ui.Headspace(trench.setup,'off');
        t.app.bankPath=fullfile(tempname,'HEADSPACE.bank.json');
        t.app.corners=cell(1,4); t.app.current=1; t.app.setPosition(mean(t.app.points,1));
        t.addTeardown(@() delete(t.app));
    end
end
methods(Test)
    function h01Anatomy(t)
        f=t.app.frames; groups={f.group};
        t.verifyEqual(numel(f),76);
        t.verifyEqual([nnz(strcmp(groups,'Klatt 1980')) nnz(strcmp(groups,'Hillenbrand 1995')) nnz(strcmp(groups,'DVTD'))],[12 48 16]);
        for k=1:76
            c=f(k).chord; trench.headspace.validate(c);
            t.verifyEqual(c(:,[1 4]),ones(6,2)); t.verifyGreaterThan(diff(c(1:5,2)),zeros(4,1));
            t.verifyGreaterThan(c(1:5,6),c(1:5,3)); t.verifyEqual(c(6,[3 6]),[12 12]);
            g=trench.bridge.geometry(f(k).words);
            t.verifyGreaterThan(g(:,3),zeros(6,1)); t.verifyLessThan(g(:,3),ones(6,1));
            t.verifyEqual(trench.bridge.noteOf(g(1:5,2)),c(1:5,2),'AbsTol',.05);
            for row=1:5
                hz=g(row,2)*[.6 1 1.5]; db=trench.bridge.sectionDb(f(k).words,row,hz);
                t.verifyGreaterThan(db(2),max(db([1 3])));
            end
        end
    end
    function h02Provenance(t)
        a=t.app; data=readtable(fullfile(a.root,'evidence','mouths','hillenbrand','hillenbrand-vowel-formatted.csv'),TextType='string');
        for k=1:76
            f=a.frames(k); p=f.provenance;
            t.verifyEqual(trench.bridge.hzOf(f.chord(1:5,2)),p.frequencyHz,'AbsTol',1e-9);
            if k<=12
                t.verifyEqual(p.frequencyHz(4:5),[3300;3750]);
                t.verifyEqual(p.bandwidthHz(4:5),[250;200]);
                t.verifyEqual(string(p.frequencyKind(4:5)),["published_default";"published_default"]);
                t.verifyEqual(f.chord(6,2),f.chord(6,5));
            elseif k<=60
                parts=split(string(f.name)); rows=startsWith(data.ID,extractBetween(parts(3),1,1)) & endsWith(data.ID,parts(2));
                values=data.F4(rows); values=values(values>0);
                t.verifyEqual(p.frequencyHz(4),mean(values),'AbsTol',1e-9);
                t.verifyEqual(p.measurementCounts(4),numel(values));
                t.verifyEqual(string(p.frequencyKind(4:5)),["measured";"estimated"]);
                t.verifyEqual(p.frequencyHz(5),p.frequencyHz(4)+median(diff(p.frequencyHz(1:4))),'AbsTol',1e-9);
                t.verifyEqual(p.bandwidthHz(4:5),p.bandwidthHz(3)*p.frequencyHz(4:5)/p.frequencyHz(3),'AbsTol',1e-9);
                t.verifyEqual(f.chord(6,2),f.chord(6,5));
            else
                t.verifyEqual(string(p.frequencyKind),repmat("measured_response_peak",5,1));
            end
        end
        for group={'Klatt 1980','Hillenbrand 1995','DVTD'}
            bank=trench.io.openBank(fullfile(a.root,'native','workstation','banks',[group{1} '.bank.json']));
            if strcmp(group{1},'DVTD'), bank.frames=bank.frames(endsWith({bank.frames.name},' s1')); end
            completed=a.frames(strcmp({a.frames.group},group{1}));
            for k=1:numel(completed)
                original=trench.model.conformShelf(bank.frames(k));
                t.verifyEqual(completed(k).chord([1:3 6],:),original.chord([1:3 6],:));
                if strcmp(group{1},'DVTD'), t.verifyEqual(completed(k).chord,original.chord); end
            end
        end
    end
    function h03Lattice(t)
        a=t.app; edges=a.tri.edges; lengths=vecnorm(a.points(edges(:,1),:)-a.points(edges(:,2),:),2,2);
        t.verifyEqual(lengths,ones(size(lengths)),'AbsTol',1e-9);
        t.verifyEqual(numel(unique(conncomp(graph(edges(:,1),edges(:,2))))),1);
        t.verifyEqual(76-size(edges,1)+size(a.tri.ConnectivityList,1),1);
        boundary=freeBoundary(a.tri); components=conncomp(graph(boundary(:,1),boundary(:,2),'omitselfloops'));
        t.verifyEqual(numel(unique(components(unique(boundary)))),1);
        xy=a.points; area=sum(abs(detTriangles(xy,a.tri.ConnectivityList)))/2; hull=convhull(xy);
        t.verifyEqual(area,polyarea(xy(hull,1),xy(hull,2)),'AbsTol',1e-9);
        d=jsondecode(fileread(fullfile(a.root,'native','workstation','data','headspace-layout.json')));
        t.verifyEqual(a.points,d.points); t.verifyLessThan(d.edgeCost,d.randomMinEdgeCost);
        a.setPosition(mean(a.points,1)); t.verifyEqual(a.points,d.points);
    end
    function h04ExactAnchors(t)
        a=t.app;
        for k=1:76
            a.setPosition(a.points(k,:));
            t.verifyEqual(a.chord,a.frames(k).chord); t.verifyEqual(a.weights,[1 0 0]);
            t.verifyEqual(a.words,trench.bridge.unityDc(a.frames(k).words));
        end
    end
    function h05Edges(t)
        a=t.app; edges=a.tri.edges;
        for k=1:size(edges,1)
            v=edges(k,:); w=[.73 .27]; a.setPosition(w*a.points(v,:));
            expected=expectedBlend(cat(3,a.frames(v).chord),w);
            t.verifyEqual(a.chord,expected,'AbsTol',1e-9);
            t.verifyEqual(trench.bridge.responseDb(a.words,trench.bridge.curveHz),trench.bridge.responseDb(trench.bridge.unityDc(trench.bridge.compile(expected)),trench.bridge.curveHz),'AbsTol',.05);
        end
    end
    function h06Triangles(t)
        a=t.app;
        for v=a.tri.ConnectivityList'
            w=[.23 .31 .46]; a.setPosition(w*a.points(v,:));
            expected=expectedBlend(cat(3,a.frames(v).chord),w);
            t.verifyEqual(a.chord,expected,'AbsTol',1e-9); trench.headspace.validate(a.chord);
            t.verifyEqual(a.words,trench.bridge.unityDc(trench.bridge.compile(a.chord)));
        end
    end
    function h07Continuity(t)
        a=t.app; edges=a.tri.edges; attached=edgeAttachments(a.tri,edges); hz=trench.bridge.curveHz;
        for k=1:size(edges,1)
            if numel(attached{k})~=2, continue; end
            v=edges(k,:); p=mean(a.points(v,:),1); d=diff(a.points(v,:)); normal=[-d(2) d(1)];
            a.setPosition(p+1e-8*normal); c=a.chord; response=trench.bridge.responseDb(a.words,hz);
            a.setPosition(p-1e-8*normal);
            t.verifyEqual(a.chord,c,'AbsTol',1e-6);
            t.verifyEqual(trench.bridge.responseDb(a.words,hz),response,'AbsTol',.05);
        end
    end
    function h08Nudges(t)
        a=t.app; start=a.position;
        directions={'rightarrow','leftarrow','uparrow','downarrow'}; delta=[1 0;-1 0;0 -1;0 1];
        for k=1:4
            a.setPosition(start); a.onKey(key(directions{k})); t.verifyEqual(a.position,start+.05*delta(k,:),'AbsTol',1e-9);
            a.setPosition(start); a.onKey(key(directions{k},'control')); t.verifyEqual(a.position,start+.01*delta(k,:),'AbsTol',1e-9);
        end
    end
    function h09NoSnap(t)
        a=t.app; v=a.tri.ConnectivityList(1,:); p=a.points(v(1),:); d=a.points(v(2),:)-p;
        for distance=[1e-7 .001 .01 .03]
            a.onField('press',p+distance*d); a.onField('drag',p+distance*d); a.onField('release',p+distance*d);
            t.verifyEqual(a.position,p+distance*d,'AbsTol',1e-9); t.verifyNotEqual(a.chord,a.frames(v(1)).chord);
        end
        t.verifyFalse(a.dragging); before=a.position; a.onField('drag',p); t.verifyEqual(a.position,before);
    end
    function h10Boundary(t)
        a=t.app; edges=freeBoundary(a.tri); centre=mean(a.points,1);
        for k=1:size(edges,1)
            v=edges(k,:); p=mean(a.points(v,:),1); d=diff(a.points(v,:)); normal=[-d(2) d(1)];
            if dot(normal,p-centre)<0, normal=-normal; end
            a.setPosition(p+normal*100); t.verifyEqual(a.position,p,'AbsTol',1e-9);
            t.verifyEqual(a.chord,expectedBlend(cat(3,a.frames(v).chord),[.5 .5]),'AbsTol',1e-9);
            a.setPosition(p+normal*100+d*.001); t.verifyEqual(a.position,p+d*.001,'AbsTol',1e-9);
        end
        p=a.position; c=a.chord; a.setPosition([NaN 0]); t.verifyEqual(a.position,p); t.verifyEqual(a.chord,c);
    end
    function h11Save(t)
        a=t.app; live=a.probe; a.onKey(key('3')); t.verifyEqual(a.position,live.position);
        a.onKey(key('s','control')); t.verifyEqual(a.position,live.position);
        t.verifyEqual(a.corners{3}.chord,live.chord); t.verifyEqual(a.corners{3}.words,trench.bridge.compile(live.chord));
        bank=trench.io.openBank(a.bankPath); t.verifyEqual(bank.indices,3);
        t.verifyEqual(bank.frames(1).chord,live.chord,'AbsTol',1e-9); t.verifyEqual(bank.frames(1).words,a.corners{3}.words);
        t.verifyEqual(a.writeKey.Enable,'off'); t.verifyEmpty(a.writeBodyFile(fullfile(tempname,'not-ready.body240')));
    end
    function h12Body(t)
        a=t.app; v=a.tri.ConnectivityList(1,:); spots=[a.points(v,:);mean(a.points(v,:),1)]; liveWords=cell(1,4);
        for k=1:4
            a.setPosition(spots(k,:)); before=a.position; liveWords{k}=a.words;
            a.setCorner(k); a.saveCorner; t.verifyEqual(a.position,before);
        end
        t.verifyEqual(a.writeKey.Enable,'on'); path=fullfile(tempname,'headspace.body240'); a.writeBodyFile(path);
        d=dir(path); t.verifyEqual(d.bytes,240); fid=fopen(path,'r'); bytes=fread(fid,Inf,'*uint8'); fclose(fid);
        t.verifyTrue(trench.bridge.bodyRepresentable(bytes)); coordinates=[0 0;1 0;0 1;1 1];
        for k=1:4
            landed=trench.bridge.bodyLerp(bytes,coordinates(k,1),coordinates(k,2));
            t.verifyEqual(landed,liveWords{k});
            t.verifyEqual(landed(:,1:4),a.corners{k}.words(:,1:4));
        end
        saved=a.corners; a.corners=cell(1,4); a.loadCorners;
        for k=1:4, t.verifyEqual(a.corners{k}.chord,saved{k}.chord,'AbsTol',1e-9); end
    end
    function h13AudioBoundary(t)
        a=t.app; trench.audio.unload;
        folder=tempname; mkdir(folder); path=fullfile(folder,'trench_audio.m');
        fid=fopen(path,'w'); fprintf(fid,'function trench_audio(command,varargin)\nglobal headspaceAudioWords\nif strcmp(command,''words''), headspaceAudioWords=varargin{1}; end\nend\n'); fclose(fid);
        addpath(folder,'-begin'); cleanup=onCleanup(@() removeSpy(folder)); clear trench_audio
        global headspaceAudioWords
        for v=a.tri.ConnectivityList(1:9,:)'
            a.setPosition([.2 .3 .5]*a.points(v,:));
            t.verifyEqual(headspaceAudioWords,a.words);
            t.verifyEqual(a.liveCurve.YData(:),trench.bridge.responseDb(headspaceAudioWords,trench.bridge.curveHz));
        end
    end
    function h14Play(t)
        a=t.app; a.setPlaying(true); p=a.probe;
        t.verifyTrue(p.playing || startsWith(p.status,'no audio device'));
        if p.playing, pause(.2); t.verifyGreaterThan(max(abs(trench.audio.snapshot)),0); end
        a.setSource('saw'); t.verifyEqual(a.probe.source,'saw'); a.setSource('noise'); t.verifyEqual(a.probe.source,'noise');
        a.setPlaying(false); t.verifyFalse(a.probe.playing);
    end
    function h15OneScreen(t)
        a=t.app; h=findall(a.figure,'Type','uicontrol'); tags=get(h,'Tag');
        t.verifyEqual(sort(tags(:)'),sort({'PLAY','SAW','PINK NOISE','WRITE BODY FILE','1','2','3','4'}));
        t.verifyEqual(numel(findall(a.figure,'Type','figure')),1); t.verifyNumElements(findall(a.figure,'Type','axes'),2);
        t.verifyEqual(char(a.figure.Visible),'off'); t.verifyEqual(a.figure.Name,'HEADSPACE');
        t.verifyEmpty(a.fieldAxes.Children); t.verifyEqual(char(a.fieldAxes.Visible),'off');
        t.verifyEmpty(a.fieldAxes.XTick); t.verifyEmpty(a.fieldAxes.YTick); t.verifyEmpty(a.fieldAxes.XLabel.String); t.verifyEmpty(a.fieldAxes.YLabel.String);
        t.verifyEqual(a.fieldAxes.DataAspectRatio,[1 1 1]);
        t.verifyEqual(a.liveAxes.XLim,[20 20000]); t.verifyEqual(a.liveAxes.YLim,[-30 30]);
        a.saveCorner; t.verifyEqual(get(a.cornerLabels,'String'),{'1';'2';'3';'4'});
        t.verifyEmpty(findall(a.figure,'Style','edit'));
    end
end
end
function chord=expectedBlend(chords,weights)
chord=ones(6,7);
for row=1:6
    for col=[2 5 7], chord(row,col)=dot(squeeze(chords(row,col,:)),weights); end
    for col=[3 6], chord(row,col)=prod(squeeze(chords(row,col,:)).^weights(:)); end
end
chord(6,[3 6])=12;
end
function d=detTriangles(p,v)
a=p(v(:,2),:)-p(v(:,1),:); b=p(v(:,3),:)-p(v(:,1),:); d=a(:,1).*b(:,2)-a(:,2).*b(:,1);
end
function removeSpy(folder)
rmpath(folder); clear trench_audio; clear global headspaceAudioWords
end
function e=key(name,modifier)
if nargin<2, e=struct('Key',name,'Modifier',{{}}); else, e=struct('Key',name,'Modifier',{{modifier}}); end
end
