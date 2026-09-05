function setSetting(room,name,value)
if isnumeric(value) && any(~isfinite(value)), return; end
if strcmp(name,'fftSize'), value=2^round(log2(max(256,min(32768,value)))); end
if strcmp(name,'stride'), value=max(1,min(room.fftSize,round(value))); end
room.(name)=value; room.stride=min(room.stride,room.fftSize);
if strcmp(name,'envelope') && value, room.mode='speech'; end
if strcmp(name,'mode') && strcmp(value,'bells'), room.envelope=false; end
keys={'SPEECH','BELLS','ENVELOPE'}; states=[strcmp(room.mode,'speech') strcmp(room.mode,'bells') room.envelope];
for k=1:3
    h=findall(room.controls.panel,'Tag',keys{k}); set(h,'Value',states(k));
end
if ismember(name,{'fftSize','stride','window','mode'}), room.envelopeDb=[]; end
room.drawSound; room.readSlice;
end
