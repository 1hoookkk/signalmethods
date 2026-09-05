classdef tBridge < matlab.unittest.TestCase
properties
    root
    frames
end
methods(TestClassSetup)
    function load(t)
        t.root=trench.setup; t.frames=trench.bridge.loadFrames(t.root);
    end
end
methods(Test)
    function b01Library(t)
        f=t.frames;
        t.verifyGreaterThanOrEqual(numel(f),304+1567);
        t.verifyEqual(sum(strcmp({f.group},'P2K')),132);
        t.verifyEqual(f(1).name,['Ace Of Bass ' char(183) ' M0 Q0']);
        for k=1:numel(f)
            t.assertSize(f(k).words,[6 5]); t.assertClass(f(k).words,'uint16'); t.assertSize(f(k).chord,[6 7]);
        end
    end
    function b02RoundTrip(t)
        hz=trench.bridge.curveHz;
        for f=t.frames(strcmp({t.frames.group},'P2K'))
            w=trench.bridge.compile(trench.bridge.decompile(f.words));
            t.verifyLessThanOrEqual(max(abs(trench.bridge.responseDb(w,hz)-trench.bridge.responseDb(f.words,hz))),.25);
        end
    end
    function b03LedBody(t)
        for k=1:10
            i=4*(k-1)+1; a=t.frames(i).words; b=t.frames(i+1).words; raw=cat(3,a,b,a,b);
            c=trench.bridge.bodyCorners(raw,true(6,1),false);
            bytes=trench.bridge.bodyBytes(raw,true(6,1),false);
            t.verifyEqual(trench.bridge.bodyLerp(bytes,.5,0),trench.bridge.pairMorph(c(:,:,1),c(:,:,2),.5));
        end
    end
    function b04Write(t)
        folder=tempname; path=fullfile(folder,'body.body240');
        raw=repmat(t.frames(1).words,1,1,4);
        trench.bridge.writeBody(path,raw,true(1,6),false);
        d=dir(path); t.verifyEqual(d.bytes,240);
        fid=fopen(path,'r'); bytes=fread(fid,Inf,'*uint8'); fclose(fid);
        t.verifyTrue(trench.bridge.bodyRepresentable(bytes));
    end
    function b05Radius(t)
        a=t.frames(1).words; b=t.frames(2).words;
        pushed=trench.bridge.pairMorph(a,b,1.5);
        c=cat(3,pushed,a,pushed,a);
        for m=linspace(-1,2,11)
            for q=linspace(-1,2,11)
                w=trench.bridge.wheelMorph(c,m,q); g=trench.bridge.geometry(w);
                t.verifyLessThanOrEqual(max(g(:,3)),.9995+1e-9);
            end
        end
    end
    function b06Reader(t)
        fs=44100; rng(44); x=randn(fs,1);
        F=[500 1500 2500]; B=[60 90 120];
        for k=1:3
            r=exp(-pi*B(k)/fs); x=filter(1,[1 -2*r*cos(2*pi*F(k)/fs) r*r],x);
        end
        x=x/max(abs(x))*.4;
        res=trench.bridge.readResonances(x,fs,'speech');
        for hz=F, t.verifyLessThan(min(abs(res(:,1)-hz))/hz,.04); end
        c=trench.model.bellChord(trench.bridge.readFrame(x,fs,'speech')); on=c(:,1)~=0; on(6)=false;
        t.verifyTrue(c(6,1)~=0 && c(6,4)~=0 && c(6,3)>=8 && c(6,6)>=8 && abs(c(6,2)-c(6,5))<1e-6);
        t.verifyEqual(c(on,5),c(on,2),'AbsTol',1e-12);
        t.verifyEqual(c(on,3),repmat(.25,sum(on),1),'AbsTol',.01);
        t.verifyEqual(c(on,6),repmat(4,sum(on),1),'AbsTol',.05);
    end
    function b07Line(t)
        bank=trench.io.openBank(fullfile(t.root,'native','workstation','banks','P2K.bank.json'));
        for k=1:10
            a=bank.frames(k); b=bank.frames(k+1); [ca,cb]=trench.bridge.leadTo(a.chord,b.chord);
            wa=trench.bridge.compile(ca); wb=trench.bridge.compile(cb);
            t.verifyEqual(trench.model.lineWords(bank,1:133,k-1),a.words);
            t.verifyEqual(trench.bridge.pairMorph(wa,wb,1),wb);
            t.verifyEqual(trench.model.lineWords(bank,1:133,k-.5),trench.bridge.pairMorph(wa,wb,.5));
            t.verifyEqual(trench.model.lineWords(bank,1:133,k),b.words);
        end
    end
    function b08Typed(t)
        c=trench.bridge.chordFrom(48,[0 7 12 16 19 24],1,0);
        c=trench.bridge.decompile(trench.bridge.compile(c));
        t.verifyEqual(c([1 4],2),[48;64],'AbsTol',.05);
    end
    function b09Audio(t)
        cleanup=onCleanup(@() trench.audio.stop);
        info=trench.audio.start;
        if isempty(info.device), t.verifyNotEmpty(info.error); return; end
        identity=repmat(uint16([57343 65535 57343 65535 57343]),6,1);
        trench.audio.words(identity); trench.audio.source('noise'); trench.audio.playing(true);
        pause(.2); x=trench.audio.snapshot;
        t.verifyGreaterThan(max(abs(x)),0); t.verifyLessThan(trench.audio.peak,1);
        t.verifyEqual(numel(x),16384);
    end
    function b10FactoryBanks(t)
        folder=tempname; paths=trench.io.makeFactoryBanks(t.root,folder);
        t.verifyNumElements(paths,8);
        b=trench.io.openBank(paths{1}); t.verifyNumElements(b.frames,133);
        t.verifyEqual(b.frames(1).name,['Ace Of Bass ' char(183) ' M0 Q0']);
        t.verifyEqual(b.frames(133).name,'vowel schwa');
        for k=1:132, t.verifyEqual(b.frames(k).words,t.frames(k).words); end
        for k=1:8
            bank=trench.io.openBank(paths{k}); t.verifyLessThanOrEqual(numel(bank.frames),256);
            if k==2, t.verifyNumElements(bank.frames,48); end
            if k==3, t.verifyNumElements(bank.frames,numel(trench.bridge.klattVowels)); end
        end
    end
    function b11BankRoundTrip(t)
        b=trench.model.newBank('ten');
        for k=1:10, [b,~,~]=trench.model.putInBank(b,t.frames(k),2*k); end
        path=fullfile(tempname,'ten.bank.json'); trench.io.saveBank(b,path); back=trench.io.openBank(path);
        t.verifyEqual(back.indices,b.indices); t.verifyEqual({back.frames.name},{b.frames.name});
        for k=1:10
            t.verifyEqual(back.frames(k).chord,b.frames(k).chord,'AbsTol',1e-9);
            t.verifyEqual(back.frames(k).words,b.frames(k).words);
        end
    end
    function b12Full(t)
        b=trench.model.newBank;
        for k=1:256, [b,~,~]=trench.model.putInBank(b,t.frames(k)); end
        [after,slot,status]=trench.model.putInBank(b,t.frames(257));
        t.verifyEqual(after,b); t.verifyEqual(slot,0); t.verifyEqual(status,'bank full');
    end
    function b13Table(t)
        f=trench.io.tableToChords([500 1500 2500],[60 90 120],'ah','test'); c=f.chord;
        t.verifyEqual(c(1:3,2),trench.bridge.noteOf([500 1500 2500]),'AbsTol',.05);
        widths=12*log2(1+[60 90 120]./[500 1500 2500]);
        t.verifyEqual(c(1:3,3),widths','AbsTol',.05);
        t.verifyEqual(c(1:3,5),c(1:3,2),'AbsTol',.05);
        t.verifyEqual(c(1:3,6),c(1:3,3)*16,'AbsTol',.05);
        t.verifyEqual(c(4:5,[1 4]),zeros(2));
        t.verifyEqual(c(6,[1 4]),[1 1]); t.verifyEqual(c(6,2),c(6,5),'AbsTol',1e-6); t.verifyEqual(c(6,2),trench.bridge.noteOf(250),'AbsTol',.05); t.verifyGreaterThanOrEqual(min(c(6,[3 6])),8);
        shelved=trench.io.tableToChords([500 1500 2500],[60 90 120],'ah','test',[12 250]); s=shelved.chord;
        t.verifyEqual(s(6,5)-s(6,2),12*log2(10^(12/40)),'AbsTol',.1);
        hz=trench.bridge.curveHz; d=trench.bridge.responseDb(shelved.words,hz)-trench.bridge.responseDb(f.words,hz);
        t.verifyGreaterThan(interp1(hz,d,60)-interp1(hz,d,8000),8);
    end
end
end
