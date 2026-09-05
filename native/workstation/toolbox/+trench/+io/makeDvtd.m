function frames = makeDvtd(root)
folder=fullfile(root,'recipes','vocal','dvtd');
files=dir(fullfile(folder,'subject-*','*','*-vvtf-measured.txt'));
source=['Birkholz et al., Dresden Vocal Tract Dataset (DVTD), measured vocal tract transfer functions of ' ...
    '3D-printed MRI vocal tracts, recipes/vocal/dvtd/*/*-vvtf-measured.txt; peaks read off the measured ' ...
    'magnitude smoothed 25 Hz either side, prominence 3 dB, 3 dB bandwidth under the frequency, the five ' ...
    'lowest as F1..F5; the low shelf is the level at 100 Hz over the median, cornered at half F1; 44,100 Hz datum'];
frames=struct([]); records=struct('name',{},'source',{},'stages',{});
for k=1:numel(files)
    [~,leaf]=fileparts(files(k).folder); parts=regexp(leaf,'^(s\d)-\d+-([a-z]+)-(.+)$','tokens','once');
    if isempty(parts) || ~(startsWith(parts{3},'tense-') || startsWith(parts{3},'lax-') || strcmp(parts{3},'schwa')), continue; end
    table=readmatrix(fullfile(files(k).folder,files(k).name),'NumHeaderLines',1);
    [r,shelf]=trench.model.tableResonances(table(:,1),table(:,2));
    if isempty(r), continue; end
    name=sprintf('%s %s %s',parts{3},parts{2},parts{1});
    f=trench.io.tableToChords(r(:,1)',r(:,2)',name,source,shelf);
    if isempty(frames), frames=f; else, frames(end+1)=f; end
    records(end+1)=struct('name',f.name,'source',source,'stages',trench.io.chordToStages(f.chord));
end
out=struct('schema','trench-chords-v1','floor','VOWELS','source',source,'chords',records);
path=fullfile(root,'native','python','workstation','chords','dvtd.json');
fid=fopen(path,'w','n','UTF-8'); assert(fid>=0,'Cannot write DVTD chords.');
cleanup=onCleanup(@() fclose(fid)); fwrite(fid,jsonencode(out,PrettyPrint=true),'char');
end
