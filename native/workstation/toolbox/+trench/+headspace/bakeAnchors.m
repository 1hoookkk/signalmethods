function path=bakeAnchors(root)
records=struct([]);
klatt=trench.io.openBank(fullfile(root,'native','workstation','banks','Klatt 1980.bank.json'));
upper=[3300 250; 3750 200; 4900 1000];
source='Klatt 1980, evidence/mouths/klatt/Klatt-1980.pdf: F1 to F3 with bandwidths per vowel from Table II; F4 3300/250, F5 3750/200, F6 4900/1000 Hz, the typical values of Table I; a six-resonator cascade, poles only';
for k=1:numel(klatt.frames)
    c=klatt.frames(k).chord; frequency=440*2.^((c(1:3,2)-69)/12); bandwidth=frequency.*(2.^(c(1:3,3)/12)-1);
    frequency=[frequency; upper(:,1)]; bandwidth=[bandwidth; upper(:,2)];
    chord=repmat([0 60 12 0 60 12 0],6,1);
    for row=1:6, chord(row,1:3)=[1 69+12*log2(frequency(row)/440) 12*log2(1+bandwidth(row)/frequency(row))]; end
    kind=[repmat({'published_table_II'},1,3) repmat({'published_table_I'},1,3)];
    records=add(records,klatt.frames(k).name,'Klatt 1980',chord,struct('frequencyHz',frequency,'bandwidthHz',bandwidth, ...
        'kind',{kind},'source',source,'method','table values converted: note = 69 + 12 log2(F / 440), width = 12 log2(1 + B / F)'));
end
files=dir(fullfile(root,'recipes','vocal','dvtd','subject-*','*','*-model-sound.wav'));
dvtdSource='Birkholz et al., Dresden Vocal Tract Dataset (DVTD), model sound of the 3D-printed MRI vocal tract, recipes/vocal/dvtd/*/*-model-sound.wav';
method='Spectrogram envelope, the E-mu P2K method: the middle half of the sound resampled to 11,025 Hz, Hann window, order-12 LPC, the six conjugate pole pairs as the six sections; note from the angle, width from the radius; no zeros';
for k=1:numel(files)
    [~,leaf]=fileparts(files(k).folder); parts=regexp(leaf,'^(s\d)-\d+-([a-z]+)-(.+)$','tokens','once');
    if isempty(parts) || ~(startsWith(parts{3},'tense-') || startsWith(parts{3},'lax-') || strcmp(parts{3},'schwa')), continue; end
    [chord,poles]=trench.headspace.readSound(fullfile(files(k).folder,files(k).name));
    if size(poles,1)<6, fprintf('%s: %d pole pairs, left out\n',leaf,size(poles,1)); continue; end
    records=add(records,sprintf('%s %s %s',parts{3},parts{2},parts{1}),'DVTD',chord,struct('frequencyHz',poles(1:6,1),'bandwidthHz',poles(1:6,2), ...
        'kind',{repmat({'measured_model_sound_lpc12'},1,6)},'source',[dvtdSource ' ' files(k).name],'method',method));
end
path=fullfile(root,'native','workstation','data','headspace-anchors.json');
fid=fopen(path,'w','n','UTF-8'); assert(fid>=0); cleanup=onCleanup(@() fclose(fid));
fwrite(fid,jsonencode(struct('schema','headspace-anchors-v2','anchors',records),PrettyPrint=true),'char');
end
function records=add(records,name,group,chord,provenance)
chord=trench.bridge.fitVoices(chord); trench.headspace.validate(chord);
record=struct('name',name,'group',group,'chord',chord,'provenance',provenance);
if isempty(records), records=record; else, records(end+1)=record; end
end
