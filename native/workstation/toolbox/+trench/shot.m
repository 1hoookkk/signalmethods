function paths = shot(folder)
root=trench.setup; if nargin<1, folder=fullfile(root,'native','workstation','artifacts','shots'); end
if ~isfolder(folder), mkdir(folder); end
app=trench.ui.Workstation(root,'off'); cleanup=onCleanup(@() delete(app));
app.selectFrame(1); app.useAs(1); app.selectFrame(2); app.useAs(2);
sound=fullfile(root,'recipes','recordings','test-vowel-ah.wav');
app.setRoom('sound'); app.rooms.sound.openSound(sound); app.rooms.sound.setCursor(.3);
names={'sound','frames','morph','corner'}; paths=cell(1,4);
for k=1:4
    app.setRoom(names{k});
    if k==2, app.selectFrame(3); app.rooms.frames.drawMap; end
    if k==3, app.rooms.morph.setPosition(35,20); end
    app.refresh; drawnow;
    paths{k}=fullfile(folder,[names{k} '.png']);
    trench.ui.draw.writeFigurePng(app.figure,paths{k});
end
end
