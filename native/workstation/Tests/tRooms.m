classdef tRooms < matlab.unittest.TestCase
properties
    app
end
methods(TestMethodSetup)
    function make(t)
        t.app=trench.ui.Workstation(trench.setup,'off');
        t.addTeardown(@() delete(t.app));
    end
end
methods(Test)
    function r01Row(t)
        t.app.rooms.frames.onRow(1,'press'); p=t.app.probe;
        t.verifyEqual(p.live.kind,'frame'); t.verifyEqual(p.live.index,1);
    end
    function r02Status(t)
        t.app.rooms.frames.onRow(1,'press'); p=t.app.probe;
        t.verifyTrue(contains(p.status,t.app.bank.frames(1).name) && contains(p.status,'ROOT'));
        t.verifyTrue(contains(p.status,trench.bridge.intervalsOf(t.app.bank.frames(1).chord)));
    end
    function r03Different(t)
        t.app.rooms.frames.onRow(1,'press'); a=t.app.probe;
        t.app.rooms.frames.onRow(2,'press'); b=t.app.probe;
        t.verifyNotEqual(a.live.index,b.live.index); t.verifyNotEqual(a.words,b.words);
    end
    function r04UseKey(t)
        t.app.selectFrame(1); p=t.app.probe;
        t.verifyEqual(p.chosen,1); t.verifyEqual(t.app.body.useKeys(1).String,'USE AS M0 Q0');
        t.verifyEqual(t.app.body.useKeys(1).Enable,'on');
    end
    function r05Fill(t)
        t.app.selectFrame(1); t.app.useAs(1); p=t.app.probe;
        t.verifyEqual(p.corners,ones(1,4)); t.verifyTrue(startsWith(p.cornerNames{2},'copy of'));
    end
    function r06Opposite(t)
        pair(t.app); p=t.app.probe; t.verifyEqual(p.corners,[1 2 1 2]);
        t.verifyEqual(p.cornerNames{4},'copy of M1 Q0');
    end
    function r07Direction(t)
        pair(t.app); notes=wheelNotes(t.app); c=t.app.bodyCorners;
        bytes=trench.bridge.bodyBytes(t.app.rawCorners,t.app.rowOn,t.app.unity);
        for k=0:20
            t.verifyEqual(trench.bridge.wheelMorph(c,k/20,0),trench.bridge.bodyLerp(bytes,k/20,0));
        end
        d=diff(notes); t.verifyFalse(any(any(d>1,1)&any(d<-1,1)));
    end
    function r08Distance(t)
        pair(t.app); notes=wheelNotes(t.app);
        t.verifyLessThanOrEqual(max(abs(diff(notes)),[],'all'),6);
    end
    function r09Wheel(t)
        pair(t.app); t.app.setRoom('morph'); p=t.app.probe;
        t.verifyEqual(p.room,'morph'); t.verifyEqual(p.live.kind,'wheel');
    end
    function r10Drag(t)
        pair(t.app); t.app.setRoom('morph'); r=t.app.rooms.morph;
        r.onPad([0 0],'press'); r.onPad([25 0],'drag'); r.onPad([25 0],'release');
        p=t.app.probe; t.verifyEqual(p.morph,25,'AbsTol',2);
    end
    function r11Fine(t)
        pair(t.app); t.app.setRoom('morph'); r=t.app.rooms.morph; r.fine=true;
        r.onPad([0 0],'press'); r.onPad([50 0],'drag'); p=t.app.probe;
        t.verifyEqual(p.morph,5,'AbsTol',2);
    end
    function r12Hear(t)
        pair(t.app); t.app.setRoom('morph'); t.app.rooms.morph.onHear([0 0],'press');
        p=t.app.probe; t.verifyEqual(p.live.kind,'cornerAlone');
    end
    function r13Release(t)
        pair(t.app); t.app.setRoom('morph'); r=t.app.rooms.morph;
        r.onHear([0 0],'press'); r.onHear([0 0],'release'); p=t.app.probe;
        t.verifyEqual(p.live.kind,'wheel');
    end
    function r14Keep(t)
        pair(t.app); t.app.setRoom('morph'); t.app.rooms.morph.setPosition(35,0);
        t.app.rooms.morph.keepAsFrame; p=t.app.probe;
        t.verifyEqual(p.chosen,134); t.verifyTrue(t.app.bank.frames(end).capture);
        t.verifyTrue(startsWith(p.status,t.app.bank.frames(end).name));
    end
    function r15Play(t)
        t.app.setPlaying(true); p=t.app.probe; s=trench.audio.state;
        t.verifyTrue(contains(p.status,s.device) && ~isempty(p.status));
        t.verifyTrue(p.playing || contains(p.status,'no audio device')); t.app.setPlaying(false);
    end
    function r16CornerName(t)
        pair(t.app); t.app.setRoom('morph'); t.app.showCornerFrame(2); p=t.app.probe;
        t.verifyEqual(p.room,'frames'); t.verifyEqual(p.chosen,2);
    end
    function r17Sound(t)
        sound(t.app); p=t.app.probe; r=t.app.rooms.sound;
        expected=1+floor((numel(r.sound.mono)-r.fftSize)/r.stride);
        t.verifyEqual(p.live.kind,'slice'); t.verifyTrue(isgraphics(r.image,'image'));
        t.verifyEqual(size(r.image.CData,2),expected);
    end
    function r18Read(t)
        sound(t.app); t.app.rooms.sound.readFrameAtCursor; p=t.app.probe;
        t.verifyEqual(p.chosen,134); f=t.app.bank.frames(end);
        t.verifyTrue(startsWith(f.name,'test-vowel-ah @')); c=f.chord; on=c(:,1)~=0;
        t.verifyEqual(c(on,5),c(on,2)); t.verifyEqual(c(on,3),repmat(.25,sum(on),1),'AbsTol',.01);
        t.verifyEqual(c(on,6),repmat(4,sum(on),1),'AbsTol',.05);
    end
    function r19Position(t)
        a=t.app; a.setUnity(false); r=a.rooms.frames; r.setPosition(2); p=a.probe;
        t.verifyEqual(p.live.kind,'frame'); t.verifyEqual(p.live.index,3);
        r.setPosition(2.4); p=a.probe; t.verifyEqual(p.live.kind,'line');
        [ca,cb]=trench.bridge.leadTo(a.bank.frames(3).chord,a.bank.frames(4).chord);
        w=trench.bridge.pairMorph(trench.bridge.compile(ca),trench.bridge.compile(cb),.4);
        t.verifyEqual(p.words,w); verifyBetween(t,w,ca,cb);
    end
    function r20EqualHops(t)
        r=t.app.rooms.frames; r.sweep=true; r.hopSeconds=4;
        for start=[0 50]
            r.setPosition(start); r.direction=1;
            for k=1:120, r.tick(1/30); end
            p=t.app.probe; t.verifyEqual(r.position,start+1,'AbsTol',1e-9);
            t.verifyEqual(p.live.kind,'frame');
        end
    end
    function r21Past(t)
        pair(t.app); t.app.setRoom('morph'); r=t.app.rooms.morph;
        r.setPosition(150,0); p=t.app.probe; t.verifyEqual(p.morph,100);
        r.past=true; r.setPosition(150,0); p=t.app.probe; t.verifyEqual(p.morph,150);
        [~,g]=trench.bridge.wheelMorph(t.app.bodyCorners,1.5,0);
        if any(g), t.verifyTrue(contains(p.status,strtrim(sprintf('%d ',find(g))))); end
    end
    function r22Donor(t)
        pair(t.app); t.app.setRoom('morph'); r=t.app.rooms.morph; r.setPosition(17,34);
        before=t.app.probe; r.nextFrame(1); after=t.app.probe;
        t.verifyNotEqual(after.corners(2),before.corners(2)); t.verifyEqual(after.corners(2),after.corners(4));
        t.verifyEqual([after.morph after.q],[17 34]);
    end
    function r23Pitch(t)
        pair(t.app); t.app.setRoom('corner'); r=t.app.rooms.corner; r.setPitch(1,'C3+0');
        p=t.app.probe; c=trench.bridge.decompile(p.words); t.verifyEqual(c(1,2),48,'AbsTol',.05);
        r.setLock(1,true); r.activeRow=1; r.onPole([130.8 0],'press'); r.onPole([220 0],'drag');
        p=t.app.probe; c=trench.bridge.decompile(p.words); t.verifyEqual(c(1,2),c(1,5),'AbsTol',.05);
    end
    function r24Ceiling(t)
        pair(t.app); t.app.setRoom('corner'); t.app.rooms.corner.setCeiling;
        p=t.app.probe; g=trench.bridge.geometry(p.words);
        t.verifyEqual(g(6,5),20000,'RelTol',.01); t.verifyEqual(g(6,6),1,'AbsTol',1e-9);
    end
    function r25Write(t)
        pair(t.app); path=t.app.writeBodyFile; t.addTeardown(@() delete(path));
        t.verifyTrue(contains(path,fullfile('plugin','presets','user')));
        d=dir(path); t.verifyEqual(d.bytes,240); fid=fopen(path,'r'); bytes=fread(fid,Inf,'*uint8'); fclose(fid);
        c=t.app.bodyCorners; t.verifyEqual(trench.bridge.bodyLerp(bytes,0,0),c(:,:,1));
    end
    function r26Keys(t)
        pair(t.app); checks.verifyKeys(t,t.app);
    end
    function r27FirstBank(t)
        p=t.app.probe; t.verifyEqual(p.bank.name,'P2K'); t.verifyEqual(p.bank.count,133);
        t.verifyNumElements(t.app.rooms.frames.marks.XData,133);
    end
    function r28PutRemove(t)
        a=t.app; a.selectLibrary(200); a.rooms.frames.putLibrary; p=a.probe;
        t.verifyEqual(p.bank.count,134); t.verifyEqual(p.chosen,134);
        a.removeFromBank; p=a.probe; t.verifyEqual(p.bank.count,133);
    end
    function r29NearestWrap(t)
        a=t.app; pair(a); a.rooms.frames.setSort('NEAREST TO M0 Q0');
        order=a.bank.indices(a.rooms.frames.order); a.corners(2)=order(end); a.corners(4)=order(end);
        a.rooms.morph.nextFrame(1); p=a.probe; t.verifyEqual(p.corners(2),order(1));
    end
    function r30BankFile(t)
        a=t.app; before=a.probe; path=fullfile(tempname,'same.bank.json'); a.saveBank(path); a.newBank;
        p=a.probe; t.verifyEqual(p.bank.count,0); a.openBank(path); p=a.probe;
        t.verifyEqual(p.bank.count,before.bank.count); t.verifyEqual(p.bank.names,before.bank.names);
    end
    function r31FullRead(t)
        a=t.app; sound(a);
        for k=134:256, [a.bank,~,~]=trench.model.putInBank(a.bank,a.library(k)); end
        count=numel(a.library); a.rooms.sound.readFrameAtCursor; p=a.probe;
        t.verifyEqual(p.status,'bank full'); t.verifyEqual(p.bank.count,256);
        t.verifyEqual(numel(a.library),count+1); t.verifyEqual(a.library(end).group,'INSTRUMENTS');
    end
    function r32VowelLine(t)
        a=t.app; a.openBank(fullfile(a.root,'native','workstation','banks','Hillenbrand 1995.bank.json'));
        a.setUnity(false); a.rooms.frames.setSort('LOW > HIGH ROOT'); order=a.rooms.frames.order; previous=[];
        for hop=1:numel(order)-1
            notes=[];
            for step=0:9
                a.rooms.frames.setPosition(hop-1+step/10); p=a.probe;
                if ~isempty(previous), t.verifyNotEqual(p.words,previous); end
                previous=p.words;
                c=trench.bridge.decompile(p.words); notes(end+1,:)=c(1:3,2)';
            end
            d=diff(notes); t.verifyTrue(all(all(d>=-.05,1)|all(d<=.05,1)));
        end
        a.rooms.frames.setPosition(numel(order)-1); p=a.probe; t.verifyNotEqual(p.words,previous);
    end
    function r33Map(t)
        a=t.app; f=a.library(200); p=[f.root f.voicing];
        a.rooms.frames.onMap(p,'press'); first=a.probe;
        t.verifyEqual(first.bank.count,134);
        a.rooms.frames.onMap(p,'press'); second=a.probe;
        t.verifyEqual(second.chosen,first.chosen); t.verifyEqual(second.bank.count,134);
        a.removeFromBank; after=a.probe; t.verifyEqual(after.bank.count,133);
    end
end
end
function pair(a)
a.selectFrame(1); a.useAs(1); a.selectFrame(2); a.useAs(2);
end
function sound(a)
a.setRoom('sound'); a.rooms.sound.openSound(fullfile(a.root,'recipes','recordings','test-vowel-ah.wav'));
a.rooms.sound.setCursor(.3);
end
function notes=wheelNotes(a)
c=a.bodyCorners; notes=zeros(21,6);
for k=0:20
    w=trench.bridge.wheelMorph(c,k/20,0); ch=trench.bridge.decompile(w); notes(k+1,:)=ch(:,2)';
end
end
function verifyBetween(t,w,a,b)
c=trench.bridge.decompile(w); on=a(:,1)~=0 & b(:,1)~=0;
t.verifyGreaterThanOrEqual(c(on,2),min(a(on,2),b(on,2))-.05);
t.verifyLessThanOrEqual(c(on,2),max(a(on,2),b(on,2))+.05);
end
