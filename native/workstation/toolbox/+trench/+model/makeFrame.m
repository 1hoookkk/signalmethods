function frame = makeFrame(chord, name, group, source, made)
if nargin < 5, made = true; end
shape = trench.bridge.shapeOf(chord);
frame = struct('name',char(name),'group',char(group),'chord',chord, ...
    'words',trench.bridge.compile(chord),'capture',logical(made), ...
    'source',char(source),'root',shape(1),'voicing',shape(2),'resonance',shape(3));
end
