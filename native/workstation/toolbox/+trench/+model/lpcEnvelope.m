function result=lpcEnvelope(mono,fs,nfft,window)
arguments
    mono (:,1) double {mustBeFinite}
    fs (1,1) double {mustBePositive,mustBeFinite}
    nfft (1,1) double {mustBeInteger,mustBePositive} = 2048
    window = 'Hann'
end
rate=min(fs,11025); [p,q]=rat(rate/fs,1e-12);
x=resample(mono,p,q); x=x-mean(x);
result=struct('rate',rate,'a',[1 zeros(1,12)],'gain',0, ...
    'hz',(0:floor(nfft/2))'*rate/nfft,'db',-200*ones(floor(nfft/2)+1,1), ...
    'impulse',zeros(nfft,1),'peaks',zeros(0,3));
if numel(x)<=12 || isempty(x) || max(abs(x))<1e-12, return; end
switch char(window)
    case 'Hann', win=hann(numel(x));
    case 'Blackman', win=blackman(numel(x));
    case 'Blackman-Harris', win=blackmanharris(numel(x));
    case 'Hamming', win=hamming(numel(x));
    otherwise, win=ones(size(x));
end
[a,g]=lpc(x.*win,12);
if any(~isfinite(a)) || ~isfinite(g) || g<=0 || any(abs(roots(a))>=1)
    correlation=xcorr(x.*win,12,'biased'); correlation=correlation(13:end);
    correlation(1)=correlation(1)*(1+1e-10);
    [a,g]=levinson(correlation,12);
end
if any(~isfinite(a)) || ~isfinite(g) || g<=0, return; end
impulse=filter(sqrt(g),a,[1;zeros(nfft-1,1)]);
magnitude=abs(fft(impulse,nfft));
result.a=a; result.gain=g; result.impulse=impulse;
result.db=20*log10(max(magnitude(1:numel(result.hz)),1e-10));
result.peaks=trench.model.lpcPeaks(result);
end
