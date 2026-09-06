function paths = shot(folder)
root=trench.setup; if nargin<1, folder=fullfile(root,'native','workstation','artifacts','shots'); end
if ~isfolder(folder), mkdir(folder); end
app=trench.ui.Headspace(root,'off'); cleanup=onCleanup(@() delete(app));
app.bankPath=fullfile(tempname,'HEADSPACE.bank.json'); app.corners=zeros(1,4);
picks=[3 9 20 40];
for k=1:4, app.selectAnchor(picks(k)); app.placeCorner(k); end
app.setWheel(40,30); app.refresh; drawnow;
paths={fullfile(folder,'headspace.png')};
trench.ui.draw.writeFigurePng(app.figure,paths{1});
end
