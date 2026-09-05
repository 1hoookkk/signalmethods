function paths = shot(folder)
root=trench.setup; if nargin<1, folder=fullfile(root,'native','workstation','artifacts','shots'); end
if ~isfolder(folder), mkdir(folder); end
app=trench.ui.Headspace(root,'off'); cleanup=onCleanup(@() delete(app));
app.bankPath=fullfile(tempname,'HEADSPACE.bank.json');
app.setPosition(12.4); app.saveCorner; app.setCorner(2); app.setPosition(30); app.saveCorner;
app.setCorner(3); app.setPosition(12.4); app.refresh; drawnow;
paths={fullfile(folder,'headspace.png')};
trench.ui.draw.writeFigurePng(app.figure,paths{1});
end
