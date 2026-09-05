function labels = cornerLabels(app)
names={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'}; frames=app.cornerNames; labels=names;
for k=1:4
    if ~strcmp(frames{k},names{k}), labels{k}=[names{k} ' ' char(183) ' ' frames{k}]; end
end
end
