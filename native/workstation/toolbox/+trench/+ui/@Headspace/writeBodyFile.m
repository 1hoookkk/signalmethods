function path = writeBodyFile(app,path)
if any(app.corners==0), path=''; app.status='four corners first'; app.refresh; return; end
if nargin<2, path=trench.io.exportPath(app.root,'headspace_'); end
folder=fileparts(path); if ~isempty(folder) && ~isfolder(folder), mkdir(folder); end
trench.bridge.writeBody(path,app.rawCorners,true(1,6),true);
app.status=path; app.refresh;
end
