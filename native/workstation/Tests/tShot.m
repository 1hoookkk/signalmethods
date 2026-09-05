classdef tShot < matlab.unittest.TestCase
properties
    paths
end
properties(TestParameter)
    room={1,2,3,4}
end
methods(TestClassSetup)
    function make(t)
        root=trench.setup; t.paths=trench.shot(fullfile(root,'native','workstation','artifacts','shots'));
    end
end
methods(Test)
    function png(t,room)
        path=t.paths{room}; d=dir(path); t.verifyGreaterThan(d.bytes,50000);
        rgb=imread(path); t.verifyEqual(reshape(rgb(1,1,:),1,3),uint8([204 204 204]));
        blue=rgb(:,:,1)==0 & rgb(:,:,2)==114 & rgb(:,:,3)==189; t.verifyTrue(any(blue,'all'));
        t.verifyGreaterThanOrEqual(size(rgb,2),2800);
    end
end
end
