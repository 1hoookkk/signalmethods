function results = check
root=trench.setup;
results=runtests(fullfile(root,'native','workstation','tests','tRooms.m'));
assertSuccess(results);
end
