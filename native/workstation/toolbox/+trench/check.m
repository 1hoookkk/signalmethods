function results = check
root=trench.setup;
results=runtests(fullfile(root,'native','workstation','tests','tHeadspace.m'));
assertSuccess(results);
end
