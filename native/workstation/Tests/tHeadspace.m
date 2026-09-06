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
        t.verifyEqual(numel(f),30);
        t.verifyEqual([nnz(strcmp(groups,'Klatt 1980')) nnz(strcmp(groups,'DVTD'))],[12 18]);
        for k=1:30
            c=f(k).chord; trench.headspace.validate(c);
            t.verifyEqual(c(:,1),ones(6,1)); t.verifyEqual(c(:,4),zeros(6,1)); t.verifyGreaterThan(diff(c(:,2)),zeros(5,1));
            g=trench.bridge.geometry(f(k).words);
            t.verifyGreaterThan(g(:,3),zeros(6,1)); t.verifyLessThan(g(:,3),ones(6,1)); t.verifyEqual(g(:,4),zeros(6,1));
            t.verifyEqual(trench.bridge.noteOf(g(:,2)),c(:,2),'AbsTol',.05);
            hz=trench.bridge.curveHz;
            for row=1:6
                db=trench.bridge.sectionDb(f(k).words,row,hz); [~,peak]=max(db);
                if c(row,3)<12, t.verifyLessThan(abs(log2(hz(peak)/g(row,2))),.6); end
            end
        end
    end
    function h02Provenance(t)
        a=t.app;
        for k=1:30
            f=a.frames(k); p=f.provenance;
            t.verifyEqual(trench.bridge.hzOf(f.chord(:,2)),p.frequencyHz,'RelTol',3e-3);
            t.verifyEqual(f.chord(:,3),12*log2(1+p.bandwidthHz./p.frequencyHz),'RelTol',3e-3);
            t.verifyFalse(any(contains(string(p.kind),"estimat")));
            if k<=12
                t.verifyEqual(p.frequencyHz(4:6),[3300;3750;4900]); t.verifyEqual(p.bandwidthHz(4:6),[250;200;1000]);
                t.verifyEqual(string(p.kind),[repmat("published_table_II",3,1); repmat("published_table_I",3,1)]);
            else
                t.verifyEqual(string(p.kind),repmat("measured_model_sound_lpc12",6,1));
                t.verifyTrue(endsWith(p.source,'-model-sound.wav'));
                t.verifyLessThan(p.frequencyHz(6),5512.5);
            end
        end
        klatt=trench.io.openBank(fullfile(a.root,'native','workstation','banks','Klatt 1980.bank.json'));
        for k=1:12, t.verifyEqual(a.frames(k).chord(1:3,2),klatt.frames(k).chord(1:3,2),'AbsTol',1e-6); end
        wav=fullfile(a.root,'recipes','vocal','dvtd','subject-1','s1-01-bahn-tense-a','s1-01-bahn-tense-a-model-sound.wav');
        [chord,poles]=trench.headspace.readSound(wav); i=find(strcmp({a.frames.name},'tense-a bahn s1'),1);
        t.verifyEqual(size(poles,1),6); fitted=trench.bridge.fitVoices(chord); t.verifyEqual(a.frames(i).chord(:,2),fitted(:,2),'AbsTol',1e-9);
    end
    function h03Lattice(t)
        a=t.app; edges=a.tri.edges; lengths=vecnorm(a.points(edges(:,1),:)-a.points(edges(:,2),:),2,2);
        t.verifyEqual(lengths,ones(size(lengths)),'AbsTol',1e-9);
        t.verifyEqual(numel(unique(conncomp(graph(edges(:,1),edges(:,2))))),1);
        t.verifyEqual(30-size(edges,1)+size(a.tri.ConnectivityList,1),1);
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
        for k=1:30
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
        a=t.app; h=findall(a.figure,'Type','uicontrol'); tags=get(h,'Tag'); tags=tags(~cellfun(@isempty,tags));
        t.verifyEqual(sort(tags(:)'),sort({'PLAY','SAW','PINK NOISE','WRITE BODY FILE','1','2','3','4'}));
        t.verifyEqual(numel(findall(a.figure,'Type','figure')),1); t.verifyNumElements(findall(a.figure,'Type','axes'),2);
        t.verifyEqual(char(a.figure.Visible),'off'); t.verifyEqual(a.figure.Name,'HEADSPACE');
        t.verifyNumElements(a.fieldAxes.Children,8); t.verifyEqual(a.fieldAxes.Children(1),a.positionMark); t.verifyEqual(a.fieldAxes.Children(end),a.shade);
        t.verifyNumElements(a.contours,5); t.verifyTrue(all(isgraphics(a.contours,'contour'))); t.verifyTrue(isgraphics(a.edge,'line'));
        for k=1:5, notes=arrayfun(@(f) f.chord(k,2),a.frames); t.verifyEqual(a.contours(k).LevelList,floor(min(notes)):ceil(max(notes))); end
        t.verifyEqual(a.shade.EdgeColor,'none'); t.verifyEqual(a.shade.FaceVertexCData,a.brightness); t.verifyTrue(all(isfinite(a.brightness)) && numel(a.brightness)==30); t.verifyEqual(char(a.fieldAxes.Visible),'off');
        t.verifyEmpty(findall(a.fieldAxes,'Type','text')); t.verifyEmpty(findall(a.fieldAxes,'Type','scatter'));
        a.setPosition(mean(a.points(a.tri.ConnectivityList(1,:),:),1)); p=a.probe;
        t.verifyEqual([a.positionMark.XData a.positionMark.YData],p.position,'AbsTol',1e-12);
        for v=p.vertices(p.weights>0), t.verifyTrue(contains(a.fieldLabel.String,a.frames(v).name)); end
        t.verifyEmpty(a.fieldAxes.XTick); t.verifyEmpty(a.fieldAxes.YTick); t.verifyEmpty(a.fieldAxes.XLabel.String); t.verifyEmpty(a.fieldAxes.YLabel.String);
        t.verifyEqual(a.fieldAxes.DataAspectRatio,[1 1 1]);
        t.verifyEqual(a.liveAxes.XLim,[20 20000]); t.verifyEqual(a.liveAxes.YLim,[-30 30]);
        a.saveCorner; t.verifyEqual(get(a.cornerLabels,'String'),{'1';'2';'3';'4'});
        t.verifyEmpty(findall(a.figure,'Style','edit'));
    end
end
end
function chord=expectedBlend(chords,weights)
chord=repmat([1 60 12 0 60 12 0],6,1);
for row=1:6
    for col=[2 7], chord(row,col)=dot(squeeze(chords(row,col,:)),weights); end
    chord(row,3)=prod(squeeze(chords(row,3,:)).^weights(:));
end
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
