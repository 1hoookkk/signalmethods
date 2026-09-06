function frames = headspaceFrames(root)
library=trench.bridge.loadFrames(root);
bodies={'Ooh To Eee','Eeh To Aah','Multi Q Vox','Talking Hedz','Ubu Orator','Deep Bouche'};
p2k=library(strcmp({library.group},'P2K'));
keep=false(size(p2k));
for k=1:numel(p2k), keep(k)=any(startsWith(p2k(k).name,bodies)); end
frames=[p2k(keep) library(strcmp({library.name},'vowel schwa'))];
data=jsondecode(fileread(fullfile(root,'native','workstation','data','headspace-anchors.json')));
for k=1:numel(data.anchors)
    a=data.anchors(k); f=trench.model.makeFrame(a.chord,a.name,a.group,a.provenance.source,false);
    frames(end+1)=f;
end
end
