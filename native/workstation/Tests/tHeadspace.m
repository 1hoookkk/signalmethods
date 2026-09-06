classdef tHeadspace < matlab.unittest.TestCase
properties
    app
end
methods(TestMethodSetup)
    function make(t)
        t.app=trench.ui.Headspace(trench.setup,'off');
        t.app.bankPath=fullfile(tempname,'HEADSPACE.bank.json');
        t.app.corners=zeros(1,4); t.app.current=1; t.app.selectAnchor(1);
        t.addTeardown(@() delete(t.app));
    end
end
methods(Test)
    function h01Surface(t)
        a=t.app; f=a.frames; groups={f.group}; n=numel(f);
        t.verifyGreaterThanOrEqual(n,50);
        t.verifyEqual([nnz(strcmp(groups,'P2K')) nnz(strcmp(groups,'Klatt 1980')) nnz(strcmp(groups,'DVTD'))],[24 12 18]);
        t.verifyEqual(nnz(strcmp({f.name},'vowel schwa')),1);
        t.verifyTrue(all(startsWith({f(strcmp(groups,'P2K')).name},{'Ooh To Eee','Eeh To Aah','Multi Q Vox','Talking Hedz','Ubu Orator','Deep Bouche'})));
        t.verifyEqual(size(a.points),[n 3]); t.verifyTrue(all(isfinite(a.points),'all'));
        t.verifyEqual(numel(a.squares.XData),n); t.verifyEqual(a.squares.XData(:),a.points(:,1)); t.verifyEqual(a.squares.YData(:),a.points(:,2));
        for k=1:n, t.verifyEqual(size(f(k).words),[6 5]); t.verifyEqual(size(f(k).chord),[6 7]); end
    end
    function h02Select(t)
        a=t.app; a.selectAnchor(5); p=a.probe;
        t.verifyEqual(p.selected,5); t.verifyEqual(p.live,'anchor');
        t.verifyEqual(p.words,trench.bridge.unityDc(a.frames(5).words)); t.verifyTrue(contains(p.status,a.frames(5).name));
        t.verifyEqual([a.selectedMark.XData a.selectedMark.YData],a.points(5,1:2));
        a.onSurface('press',a.points(7,1:2)); t.verifyEqual(a.probe.selected,7);
        far=a.points(7,1:2)+[diff(a.surfaceAxes.XLim) 0]; a.onSurface('press',far); t.verifyEqual(a.probe.selected,7);
    end
    function h03Place(t)
        a=t.app; a.selectAnchor(3); a.onKey(key('1')); a.selectAnchor(9); a.onKey(key('2')); p=a.probe;
        t.verifyEqual(p.corners,[3 9 0 0]); t.verifyEqual(p.current,2); t.verifyEqual(p.selected,9); t.verifyEqual(p.live,'anchor');
        t.verifyTrue(startsWith(p.status,'M1 Q0 = ')); t.verifyTrue(contains(p.status,'hop'));
        raw=a.rawCorners; t.verifyEqual(raw(:,:,1),a.frames(3).words); t.verifyEqual(raw(:,:,2),a.frames(9).words);
        t.verifyEqual(raw(:,:,3),a.frames(3).words); t.verifyEqual(raw(:,:,4),a.frames(9).words);
        bank=trench.io.openBank(a.bankPath); t.verifyEqual(bank.indices,[1 2]); t.verifyEqual(bank.frames(1).name,a.frames(3).name);
        t.verifyEqual(a.slotLabels(1).Position(1:2),a.points(3,1:2)+[1.2 .8]); t.verifyTrue(isnan(a.slotLabels(3).Position(1)));
        t.verifyEqual(a.writeKey.Enable,'off');
        a.selectAnchor(11); a.onKey(key('s','control')); t.verifyEqual(a.probe.corners,[3 11 0 0]);
        again=trench.ui.Headspace(a.root,'off'); t.addTeardown(@() delete(again)); again.bankPath=a.bankPath; again.corners=zeros(1,4); again.loadCorners;
        t.verifyEqual(again.corners,[3 11 0 0]);
    end
    function h04Wheel(t)
        a=t.app; picks=[3 9 20 40];
        for k=1:4, a.selectAnchor(picks(k)); a.placeCorner(k); end
        a.setWheel(37,20); p=a.probe;
        t.verifyEqual(p.live,'wheel'); t.verifyEqual([p.morph p.q],[37 20]); t.verifyTrue(startsWith(p.status,'MORPH 37   Q 20'));
        raw=a.rawCorners; led=trench.bridge.bodyCorners(raw,true(1,6),true);
        t.verifyEqual(p.words,trench.bridge.wheelMorph(led,.37,.2));
        t.verifyEqual(p.words,trench.bridge.bodyLerp(trench.bridge.bodyBytes(raw,true(1,6),true),.37,.2));
        t.verifyEqual([a.padMark.XData a.padMark.YData],[37 20]);
        a.onPad('press',[60 80]); t.verifyEqual([a.probe.morph a.probe.q],[60 80]); a.onPad('release',[60 80]);
        a.onKey(key('rightarrow')); t.verifyEqual(a.probe.morph,61); a.onKey(key('uparrow','control')); t.verifyEqual(a.probe.q,80.2,'AbsTol',1e-9);
        a.setWheel(0,0); t.verifyEqual(a.probe.words,led(:,:,1)); a.setWheel(100,100); t.verifyEqual(a.probe.words,led(:,:,4));
        a.selectAnchor(picks(1)); t.verifyEqual(a.probe.live,'anchor');
    end
    function h05Hops(t)
        a=t.app; a.selectAnchor(3); a.placeCorner(1); t.verifyEmpty(a.hopReport);
        a.selectAnchor(9); a.placeCorner(2); r=a.hopReport; t.verifyTrue(startsWith(r,'hops: worst step') || startsWith(r,'hop breaks'));
        step=sscanf(r(regexp(r,'[0-9.]+ st'):end),'%f'); t.verifyGreaterThanOrEqual(step,0);
        a.selectAnchor(3); a.placeCorner(2); t.verifyEmpty(a.hopReport);
    end
    function h06Write(t)
        a=t.app; picks=[3 9 20 40];
        for k=1:4, a.selectAnchor(picks(k)); a.placeCorner(k); end
        t.verifyEqual(a.writeKey.Enable,'on');
        path=fullfile(tempname,'headspace.body240'); a.writeBodyFile(path); d=dir(path); t.verifyEqual(d.bytes,240);
        fid=fopen(path,'r'); bytes=fread(fid,'uint8=>uint8'); fclose(fid);
        led=trench.bridge.bodyCorners(a.rawCorners,true(1,6),true);
        t.verifyEqual(trench.bridge.bodyLerp(bytes,0,0),led(:,:,1)); t.verifyEqual(trench.bridge.bodyLerp(bytes,1,1),led(:,:,4));
        t.verifyEqual(a.probe.status,path);
        a.corners(4)=0; t.verifyEmpty(a.writeBodyFile(fullfile(tempname,'none.body240'))); t.verifyEqual(a.probe.status,'four corners first');
    end
    function h07Play(t)
        a=t.app; a.setPlaying(true); p=a.probe;
        t.verifyTrue(p.playing || startsWith(p.status,'no audio device'));
        a.setSource('saw'); t.verifyEqual(a.probe.source,'saw'); a.setPlaying(false); t.verifyFalse(a.probe.playing);
    end
    function h08OneScreen(t)
        a=t.app; h=findall(a.figure,'Type','uicontrol'); tags=get(h,'Tag'); tags=tags(~cellfun(@isempty,tags));
        t.verifyEqual(sort(tags(:)'),sort({'PLAY','SAW','PINK NOISE','WRITE BODY FILE'}));
        t.verifyEmpty(findall(a.figure,'Style','edit')); t.verifyEqual(numel(findall(a.figure,'Type','figure')),1);
        t.verifyEqual(char(a.figure.Visible),'off'); t.verifyEqual(a.figure.Name,'HEADSPACE');
        t.verifyEqual(a.liveAxes.XLim,[20 20000]); t.verifyEqual(a.liveAxes.YLim,[-30 30]);
        t.verifyEqual(a.padAxes.XLim,[0 100]); t.verifyEqual(a.padAxes.YLim,[0 100]);
    end
end
end
function e=key(name,modifier)
if nargin<2, e=struct('Key',name,'Modifier',{{}}); else, e=struct('Key',name,'Modifier',{{modifier}}); end
end
