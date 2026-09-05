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
end
end
function x=vowel(fs)
rng(44); x=randn(fs*2,1); F=[500 1500 2500]; B=[60 90 120];
for k=1:3
    radius=exp(-pi*B(k)/fs); x=filter(1,[1 -2*radius*cos(2*pi*F(k)/fs) radius^2],x);
end
x=x(fs+1:end); x=x/max(abs(x))*.4;
end
