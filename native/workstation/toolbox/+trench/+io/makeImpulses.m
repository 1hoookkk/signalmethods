function frames = makeImpulses(root)
folder=fullfile(root,'evidence','measured-bodies','ir_library');
files=dir(fullfile(folder,'**','*.wav')); files=files(~contains({files.name},'Sweep'));
source=['itsmusician IR-Library (MIT), instrument body impulse responses, ' ...
    'evidence/measured-bodies/ir_library; read by the bells reader'];
frames=struct([]); records=struct('name',{},'source',{},'stages',{});
for k=1:numel(files)
    [x,fs]=audioread(fullfile(files(k).folder,files(k).name)); x=mean(double(x),2);
    x=x(1:min(numel(x),round(fs)));
    w=trench.bridge.readFrame(x,fs,'bells'); if isempty(w), continue; end
    [~,name]=fileparts(files(k).name);
    f=trench.model.makeFrame(trench.model.bellChord(w),name,'INSTRUMENTS',[files(k).folder(numel(folder)+2:end) ' ' files(k).name],false);
    if isempty(frames), frames=f; else, frames(end+1)=f; end
    records(end+1)=struct('name',f.name,'source',f.source,'stages',trench.io.chordToStages(f.chord));
end
out=struct('schema','trench-chords-v1','floor','INSTRUMENTS','source',source,'chords',records);
path=fullfile(root,'native','python','workstation','chords','impulses.json');
fid=fopen(path,'w','n','UTF-8'); assert(fid>=0,'Cannot write impulse chords.');
cleanup=onCleanup(@() fclose(fid)); fwrite(fid,jsonencode(out,PrettyPrint=true),'char');
end
