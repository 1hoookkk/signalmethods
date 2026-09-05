function paths = shot(folder)
root=trench.setup; if nargin<1, folder=fullfile(root,'native','workstation','artifacts','shots'); end
if ~isfolder(folder), mkdir(folder); end
app=trench.ui.Headspace(root,'off'); cleanup=onCleanup(@() delete(app));
app.bankPath=fullfile(tempname,'HEADSPACE.bank.json');
inside=mean(app.points(app.tri.ConnectivityList(1,:),:),1);
app.setPosition(inside); app.saveCorner; app.setCorner(2); app.setPosition(app.points(end,:)); app.saveCorner;
app.setCorner(3); app.setPosition(inside); app.refresh; drawnow;
paths={fullfile(folder,'headspace.png')};
trench.ui.draw.writeFigurePng(app.figure,paths{1});
end
