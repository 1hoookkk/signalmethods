function words = wordsOf(app)
words=repmat(uint16([57343 65535 57343 65535 57343]),6,1);
switch app.live.kind
    case 'frame'
        if isfield(app.live,'library') && app.live.library
            words=app.library(app.live.index).words;
        else
            i=find(app.bank.indices==app.live.index,1);
            if ~isempty(i), words=app.bank.frames(i).words; end
        end
    case 'line'
        words=trench.model.lineWords(app.bank,app.rooms.frames.order,app.live.position);
    case 'wheel'
        [words,guarded]=trench.bridge.wheelMorph(app.bodyCorners,app.morph/100,app.q/100);
        if any(guarded), app.status=['pole ' strtrim(sprintf('%d ',find(guarded)))]; end
        return
    case 'cornerAlone'
        c=app.bodyCorners; words=c(:,:,app.live.index); return
    case 'slice'
        if ~isempty(app.rooms.sound.sliceChord), words=trench.bridge.compile(app.rooms.sound.sliceChord); end
    case 'edit'
        if ~isempty(app.rooms.corner.chord), words=trench.bridge.compile(app.rooms.corner.chord); end
end
words(~app.rowOn,:)=repmat(uint16([57343 65535 57343 65535 57343]),sum(~app.rowOn),1);
if app.unity, words=trench.bridge.unityDc(words); end
end
