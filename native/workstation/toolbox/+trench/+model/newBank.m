function bank = newBank(name)
if nargin == 0, name = 'bank'; end
bank = struct('name',char(name),'slots',256,'frames',struct([]),'indices',[],'path','');
end
