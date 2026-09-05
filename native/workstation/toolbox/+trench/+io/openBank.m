function bank = openBank(path)
data=jsondecode(fileread(path));
assert(strcmp(data.schema,'trench-bank-v1') && data.slots==256,'trench:bank:schema','Invalid bank schema.');
[~,name]=fileparts(path); bank=trench.model.newBank(erase(name,'.bank')); bank.path=char(path);
for k=1:numel(data.frames)
    r=data.frames(k); assert(r.slot>=1 && r.slot<=256 && r.slot==fix(r.slot) && ~ismember(r.slot,bank.indices),'Invalid slot.');
    c=trench.io.stagesToChord(r.stages);
    if isfield(r,'rawWords')
        seed=trench.bridge.decompile(uint16(r.rawWords));
        if isfield(r,'inactive')
            c(c(:,1)==0,2:3)=r.inactive(c(:,1)==0,1:2);
            c(c(:,4)==0,5:6)=r.inactive(c(:,4)==0,3:4);
        else
            c(c(:,1)==0,2:3)=seed(c(:,1)==0,2:3);
            c(c(:,4)==0,5:6)=seed(c(:,4)==0,5:6);
        end
    end
    made=isfield(r,'capture') && r.capture;
    f=trench.model.makeFrame(c,r.name,r.group,r.source,made);
    [bank,~,~]=trench.model.putInBank(bank,f,r.slot);
end
end
