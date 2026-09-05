function openBank(app,path)
if nargin<2
    [file,folder]=uigetfile('*.bank.json','OPEN BANK',fullfile(app.root,'native','workstation','banks'));
    if isequal(file,0), return; end
    path=fullfile(folder,file);
end
bank=trench.io.openBank(path);
app.bank=bank; app.corners=zeros(1,4); app.copies=zeros(1,4); app.chosen=0;
app.prepared=uint16([]); app.live=struct('kind','none'); app.rooms.frames.position=0;
app.rooms.frames.refresh; app.setRoom('frames');
end
