function root = setup
folder = fileparts(fileparts(mfilename('fullpath')));
addpath(folder, fullfile(folder, 'mex'));
root = fileparts(fileparts(fileparts(folder)));
assert(exist('trench_bridge', 'file') == 3, 'trench:setup:mex', 'Run buildtool mex.');
assert(exist('trench_audio', 'file') == 3, 'trench:setup:mex', 'Run buildtool mex.');
end
