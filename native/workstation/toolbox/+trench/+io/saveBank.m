function bank = saveBank(bank, path)
[folder,name]=fileparts(path); name=erase(name,'.bank');
if ~isfolder(folder), mkdir(folder); end
records=repmat(struct('slot',0,'name','','group','','source','','stages',[], ...
    'inactive',[],'rawWords',[],'capture',false),1,numel(bank.frames));
for k=1:numel(bank.frames)
    f=bank.frames(k);
    records(k)=struct('slot',bank.indices(k),'name',f.name,'group',f.group, ...
        'source',f.source,'stages',trench.io.chordToStages(f.chord), ...
        'inactive',f.chord(:,[2 3 5 6]),'rawWords',double(f.words),'capture',f.capture);
end
data=struct('schema','trench-bank-v1','name',name,'slots',256,'frames',records);
temporary=[tempname(folder) '.json'];
fid=fopen(temporary,'w','n','UTF-8'); assert(fid>=0,'Cannot write bank.');
clean=onCleanup(@() fclose(fid));
fwrite(fid,jsonencode(data,PrettyPrint=true),'char'); clear clean
movefile(temporary,path,'f');
bank.name=name; bank.path=char(path);
end
