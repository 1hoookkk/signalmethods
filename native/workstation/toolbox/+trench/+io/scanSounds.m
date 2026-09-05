function paths = scanSounds(root)
folders = {'recipes/recordings','evidence/captures/inputs','evidence/measured-bodies/ir_library'};
paths = {};
for k=1:numel(folders)
    f=dir(fullfile(root,folders{k},'**','*.wav'));
    paths=[paths; arrayfun(@(x) fullfile(x.folder,x.name),f,'UniformOutput',false)];
end
paths=sort(paths);
end
