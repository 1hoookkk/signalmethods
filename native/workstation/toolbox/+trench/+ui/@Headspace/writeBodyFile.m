function path = writeBodyFile(app,path)
if any(cellfun(@isempty,app.corners)), path=''; app.status='four corners first'; app.refresh; return; end
if nargin<2, path=trench.io.exportPath(app.root,'headspace_'); end
raw=zeros(6,5,4,'uint16'); for k=1:4, raw(:,:,k)=app.corners{k}.words; end
trench.bridge.writeBody(path,raw,true(1,6),true);
app.status=path; app.refresh;
end
