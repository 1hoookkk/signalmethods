function readSlice(room)
room.app.live=struct('kind','slice','cursor',room.cursor);
if isempty(room.sound), room.app.refresh; return; end
block=room.windowAt(room.cursor);
room.sliceEnvelope=[]; room.slicePeaks=[];
if strcmp(room.mode,'speech') || room.envelope
    result=trench.model.lpcEnvelope(block,room.sound.fs,room.fftSize,room.window);
    room.sliceEnvelope=result; room.slicePeaks=result.peaks;
    room.sliceChord=trench.model.peaksToChord(result.peaks);
    if isempty(result.peaks), room.sliceChord=[]; end
else
    w=trench.bridge.readFrame(block,room.sound.fs,room.mode);
    if isempty(w), room.sliceChord=[]; else, room.sliceChord=trench.model.bellChord(w); end
end
if isempty(room.sliceChord)
    set(room.voiceMarks,'XData',NaN,'YData',NaN,'ZData',NaN);
    room.app.setStatus(room.sound.name); room.app.refresh; return
end
on=room.sliceChord(:,1)~=0; hz=trench.bridge.hzOf(room.sliceChord(on,2));
if room.threeD
    height=zeros(size(hz));
    if room.envelope, height=max(room.floor,interp1(result.hz,result.db,hz)+room.gain); end
    set(room.voiceMarks,'XData',hz,'YData',repmat(room.cursor,size(hz)),'ZData',height);
else
    if room.logF, hz=log10(hz); end
    set(room.voiceMarks,'XData',repmat(room.cursor,size(hz)),'YData',hz,'ZData',[]);
end
[power,f]=pwelch(block,hann(min(1024,numel(block))),[],2048,room.sound.fs);
set(room.app.response.spectrum,'Visible','on','YData',interp1(f,10*log10(max(power,realmin)),trench.bridge.curveHz,'linear','extrap'));
room.app.setStatus(sprintf('%s @%.2f',room.sound.name,room.cursor));
room.app.refresh;
end
