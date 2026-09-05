function path = exportPath(root)
folder=fullfile(root,'plugin','presets','user');
if ~isfolder(folder), mkdir(folder); end
base=['ws_' char(datetime('now','Format','yyyyMMdd_HHmmss'))];
path=fullfile(folder,[base '.body240']); n=1;
while isfile(path)
    path=fullfile(folder,sprintf('%s_%02d.body240',base,n)); n=n+1;
end
end
