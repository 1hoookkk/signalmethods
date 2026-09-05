function frames = makeHillenbrand(root)
path=fullfile(root,'native','python','workstation','chords','hillenbrand_1995.json');
old=jsondecode(fileread(path)); names=string({old.chords.name});
csv=fullfile(root,'evidence','mouths','hillenbrand','hillenbrand-vowel-formatted.csv');
data=readtable(csv,TextType='string'); F=zeros(numel(names),3);
for k=1:numel(names)
    parts=split(names(k)); group=extractBetween(parts(3),1,1);
    rows=startsWith(data.ID,group) & endsWith(data.ID,parts(2));
    values=data{rows,{'F1','F2','F3'}}; values(values<=0)=NaN;
    F(k,:)=mean(values,1,'omitnan');
end
B=repmat([50 64 115],numel(names),1);
source=['Hillenbrand et al. 1995, JASA 97:3099-3111; mean F1-F3 by vowel and speaker group from ' ...
    'evidence/mouths/hillenbrand/hillenbrand-vowel-formatted.csv; bandwidth proxy: Dunn 1961, ' ...
    'English adults, 50/64/115 Hz, Kent and Vorperian 2018 Table 5 (p48), doi:10.1016/j.jcomdis.2018.05.004'];
frames=trench.io.tableToChords(F,B,names,source);
records=repmat(struct('name','','source','','stages',[]),1,numel(frames));
for k=1:numel(frames)
    records(k)=struct('name',frames(k).name,'source',source,'stages',trench.io.chordToStages(frames(k).chord));
end
out=struct('schema','trench-chords-v1','floor','VOWELS','source',source,'chords',records);
fid=fopen(path,'w','n','UTF-8'); assert(fid>=0,'Cannot write Hillenbrand chords.');
cleanup=onCleanup(@() fclose(fid)); fwrite(fid,jsonencode(out,PrettyPrint=true),'char');
end
