function path = writeBodyFile(app,path)
if any(cellfun(@isempty,app.corners)), path=''; app.status='four corners first'; app.refresh; return; end
if nargin<2, path=trench.io.exportPath(app.root,'headspace_'); end
bytes=trench.headspace.bodyBytes(app.corners);
folder=fileparts(path); if ~isempty(folder) && ~isfolder(folder), mkdir(folder); end
fid=fopen(path,'w'); assert(fid>=0,'Cannot write body file.'); cleanup=onCleanup(@() fclose(fid));
assert(fwrite(fid,bytes,'uint8')==240,'Body write failed.');
app.status=path; app.refresh;
end
