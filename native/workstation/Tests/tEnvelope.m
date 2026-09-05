classdef tEnvelope < matlab.unittest.TestCase
methods(TestClassSetup)
    function setup(~), trench.setup; end
end
methods(Test)
    function impulseFft(t)
        x=vowel(44100); e=trench.model.lpcEnvelope(x,44100,2048,'Hann');
        y=resample(x,1,4); y=y-mean(y); [a,g]=lpc(y.*hann(numel(y)),12);
        h=filter(sqrt(g),a,[1;zeros(2047,1)]); spectrum=fft(h);
        t.verifyNumElements(e.a,13); t.verifyEqual(e.a,a,'AbsTol',1e-12);
        t.verifyEqual(e.db,20*log10(max(abs(spectrum(1:1025)),1e-10)),'AbsTol',1e-10);
        t.verifyEqual(e.hz(end),5512.5); t.verifyEqual(e.rate,11025);
    end
    function formantsAcrossRates(t)
        for fs=[11025 44100 48000]
            e=trench.model.lpcEnvelope(vowel(fs),fs);
            for hz=[500 1500 2500], t.verifyLessThan(min(abs(e.peaks(:,1)-hz))/hz,.04); end
        end
    end
    function antiAlias(t)
        fs=44100; time=(0:fs-1)'/fs;
        low=trench.model.lpcEnvelope(sin(2*pi*1000*time),fs);
        high=trench.model.lpcEnvelope(sin(2*pi*10000*time),fs);
        t.verifyLessThan(max(high.db),max(low.db)-40);
    end
    function amplitudeIsPreserved(t)
        x=vowel(44100); a=trench.model.lpcEnvelope(x,44100); b=trench.model.lpcEnvelope(x/2,44100);
        t.verifyEqual(a.db-b.db,repmat(20*log10(2),size(a.db)),'AbsTol',1e-7);
        t.verifyEqual(a.peaks(:,1),b.peaks(:,1));
    end
    function silence(t)
        e=trench.model.lpcEnvelope(zeros(2646,1),44100);
        t.verifyEmpty(e.peaks); t.verifyTrue(all(isfinite(e.db))); t.verifyEqual(e.gain,0);
    end
    function surfaceAndSosUseSamePeaks(t)
        [a,r]=app(t); r.setSetting('envelope',true); r.setCursor(r.times(20));
        e=trench.model.lpcEnvelope(r.windowAt(r.cursor),r.sound.fs,r.fftSize,r.window);
        t.verifyEqual(r.db(:,20),e.db); t.verifyEqual(r.envelopeHz,e.hz);
        t.verifyEqual(r.slicePeaks,e.peaks); t.verifyEqual(r.sliceChord,trench.model.peaksToChord(e.peaks));
        t.verifyEqual(r.axes.YLim(2),log10(5512.5),'AbsTol',1e-12);
        a.setUnity(false); t.verifyEqual(a.wordsOf,trench.bridge.compile(r.sliceChord));
        chord=r.sliceChord; on=chord(:,1)==1;
        for note=chord(on,2)'
            t.verifyLessThan(min(abs(trench.bridge.noteOf(e.peaks(:,1))-note)),.05);
        end
    end
    function markersAndSavedCorner(t)
        [a,r]=app(t); r.setSetting('envelope',true); r.setSetting('threeD',true); r.setCursor(.3);
        c=r.sliceChord; hz=trench.bridge.hzOf(c(c(:,1)~=0,2)); e=r.sliceEnvelope;
        t.verifyEqual(r.voiceMarks.XData(:),hz);
        expected=max(r.floor,interp1(e.hz,e.db,hz)+r.gain);
        t.verifyEqual(r.voiceMarks.ZData(:),expected,'AbsTol',1e-9);
        r.readFrameAtCursor; frame=a.bank.frames(end);
        t.verifyEqual(frame.chord,c); t.verifyEqual(frame.words,trench.bridge.compile(c));
        t.verifyEqual(frame.chord(6,1),0); t.verifyEqual(frame.chord(6,6),0);
    end
    function modeSwitchAndEmptyInput(t)
        [a,r]=app(t); r.setSetting('mode','bells'); r.setSetting('envelope',true);
        t.verifyEqual(r.mode,'speech'); r.setSetting('mode','bells'); t.verifyFalse(r.envelope);
        r.setSetting('envelope',true); r.sound.mono(:)=0; r.envelopeDb=[]; r.drawSound; r.readSlice;
        t.verifyEmpty(r.sliceChord); t.verifyEmpty(r.slicePeaks); t.verifyTrue(all(isnan(r.voiceMarks.XData)));
        before=numel(a.bank.frames); r.readFrameAtCursor; t.verifyNumElements(a.bank.frames,before);
        t.verifyEqual(char(a.figure.Visible),'off');
    end
end
end
function x=vowel(fs)
rng(44); x=randn(fs*2,1); F=[500 1500 2500]; B=[60 90 120];
for k=1:3
    radius=exp(-pi*B(k)/fs); x=filter(1,[1 -2*radius*cos(2*pi*F(k)/fs) radius^2],x);
end
x=x(fs+1:end); x=x/max(abs(x))*.4;
end
function [a,r]=app(t)
a=trench.ui.Workstation(trench.setup,'off'); t.addTeardown(@() delete(a));
a.setRoom('sound'); r=a.rooms.sound;
r.openSound(fullfile(a.root,'recipes','recordings','test-vowel-ah.wav'));
end
