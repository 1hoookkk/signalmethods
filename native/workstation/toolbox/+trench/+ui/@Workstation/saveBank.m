function saveBank(app,path)
if nargin<2, path=app.bank.path; end
if isempty(path)
    [file,folder]=uiputfile('*.bank.json','SAVE BANK AS');
    if isequal(file,0), return; end
    path=fullfile(folder,file);
end
app.bank=trench.io.saveBank(app.bank,path); app.rooms.frames.refresh; app.setStatus(path);
end
