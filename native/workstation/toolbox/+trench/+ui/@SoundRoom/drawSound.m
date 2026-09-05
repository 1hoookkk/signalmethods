function drawSound(room)
if isempty(room.sound), return; end
x=room.sound.mono; fs=room.sound.fs; n=room.fftSize; stride=room.stride;
if numel(x)<n, x(end+1:n)=0; end
switch room.window
    case 'Hann', win=hann(n);
    case 'Blackman', win=blackman(n);
    case 'Blackman-Harris', win=blackmanharris(n);
    case 'Hamming', win=hamming(n);
    otherwise, win=ones(n,1);
end
[s,f,t]=spectrogram(x,win,n-stride,n,fs);
room.frequencies=f; room.times=t; db=20*log10(max(abs(s)/sum(win),1e-10));
if room.envelope
    if isempty(room.envelopeDb)
        env=zeros(floor(n/2)+1,numel(t));
        for k=1:numel(t)
            result=trench.model.lpcEnvelope(room.windowAt(t(k)),fs,n,room.window);
            env(:,k)=result.db;
            if mod(k,50)==0, room.app.setStatus(sprintf('ENVELOPE %d / %d',k,numel(t))); end
        end
        room.envelopeDb=env; room.envelopeHz=result.hz;
    end
    f=room.envelopeHz; db=room.envelopeDb;
end
room.db=db;
trench.ui.draw.spectrogramSurface(room,f,t,db);
room.app.setStatus(room.sound.name);
end
