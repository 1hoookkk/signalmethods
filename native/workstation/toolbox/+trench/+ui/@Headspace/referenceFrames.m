function frames=referenceFrames(app)
data=jsondecode(fileread(fullfile(app.root,'native','workstation','data','headspace-anchors.json')));
frames=struct([]);
for k=1:numel(data.anchors)
    a=data.anchors(k); trench.headspace.validate(a.chord);
    f=trench.model.makeFrame(a.chord,a.name,a.group,a.provenance.source,false);
    f.provenance=a.provenance;
    if isempty(frames), frames=f; else, frames(end+1)=f; end
end
end
