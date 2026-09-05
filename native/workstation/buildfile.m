function plan = buildfile
plan=buildplan(localfunctions);
plan.DefaultTasks="test";
end
function mexTask(~)
[status,output]=system('build_mex.cmd'); disp(output); assert(status==0,'MEX build failed.');
end
function banksTask(~)
addpath toolbox
trench.io.makeFactoryBanks(trench.setup);
end
function testTask(~)
addpath toolbox
root=trench.setup;
[status,output]=system(sprintf('ctest --test-dir "%s" -R "^(trench_workstation|trench_core|trench_core_from_audio)$" --output-on-failure',fullfile(root,'out','build','vst3')));
disp(output); assert(status==0,'C++ tests failed.');
results=runtests('tests'); assertSuccess(results);
end
function checkTask(~)
addpath toolbox
trench.setup;
trench.check;
end
function shotTask(~)
addpath toolbox
trench.setup;
trench.shot(fullfile(pwd,'artifacts','shots'));
end
