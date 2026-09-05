function paths = makeFactoryBanks(root, folder)
if nargin<2, folder=fullfile(root,'native','workstation','banks'); end
hill=trench.io.makeHillenbrand(root); dvtd=trench.io.makeDvtd(root); impulses=trench.io.makeImpulses(root);
library=trench.bridge.loadFrames(root);
t=trench.bridge.klattVowels;
klatt=trench.io.tableToChords(vertcat(t.F),vertcat(t.B),string({t.symbol}), ...
    'Klatt 1980 Table II; native/core formants.hpp; evidence/mouths/klatt');
names={'P2K','Hillenbrand 1995','Klatt 1980','DVTD','X3','HEADS','XL-1','INSTRUMENTS'};
paths=cell(size(names));
for k=1:numel(names)
    name=names{k}; bank=trench.model.newBank(name);
    if k==2, frames=hill;
    elseif k==3, frames=klatt;
    elseif k==4, frames=dvtd;
    elseif k==8, frames=impulses;
    else
        frames=library(strcmp({library.group},name));
        if k==1
            schwa=library(strcmp({library.name},'vowel schwa'));
            frames=[frames schwa(1)];
        end
    end
    for j=1:min(256,numel(frames)), [bank,~,~]=trench.model.putInBank(bank,frames(j)); end
    paths{k}=fullfile(folder,[name '.bank.json']); trench.io.saveBank(bank,paths{k});
end
end
