function frames = headspaceFrames(root)
library=trench.bridge.loadFrames(root);
t=trench.bridge.klattVowels;
klatt=trench.io.tableToChords(vertcat(t.F),vertcat(t.B),string({t.symbol}), ...
    'Klatt 1980 Table II; native/core formants.hpp; evidence/mouths/klatt');
for k=1:numel(klatt), klatt(k).group='Klatt 1980'; end
dvtd=library(strcmp({library.group},'VOWELS') & contains({library.source},'dvtd') & endsWith({library.name},' s1'));
for k=1:numel(dvtd), dvtd(k).group='DVTD'; end
heads=library(strcmp({library.group},'HEADS'));
frames=[klatt(:)' dvtd(:)' heads(:)'];
for k=1:numel(frames), frames(k)=trench.model.conformShelf(frames(k)); end
[~,order]=sort([frames.root]); frames=frames(order);
end
