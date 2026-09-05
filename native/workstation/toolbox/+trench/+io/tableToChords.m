function frames = tableToChords(F, B, names, citation)
assert(isequal(size(F),size(B)) && size(F,2)<=5 && all(F>0,'all') && all(B>0,'all'),'Invalid formant table.');
names=string(names); assert(numel(names)==size(F,1),'Expected one name per row.');
frames=struct([]);
for i=1:size(F,1)
    c=repmat([0 60 12 0 60 12 0],6,1);
    for j=1:size(F,2)
        note=trench.bridge.noteOf(F(i,j)); width=12*log2(1+B(i,j)/F(i,j));
        c(j,:)=[1 note width 1 note min(120,width*16) 0];
    end
    c(6,4:6)=[1 trench.bridge.noteOf(20000) 0];
    c=trench.bridge.fitVoices(c);
    w=trench.bridge.unityDc(trench.bridge.compile(c));
    landed=trench.bridge.decompile(w); c(:,7)=landed(:,7);
    f=trench.model.makeFrame(c,names(i),'VOWELS',citation,false);
    if isempty(frames), frames=f; else, frames(end+1)=f; end
end
end
