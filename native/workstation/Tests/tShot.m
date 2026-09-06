classdef tShot < matlab.unittest.TestCase
methods(Test)
    function png(t)
        root=trench.setup; paths=trench.shot(fullfile(root,'native','workstation','artifacts','shots'));
        t.verifyNumElements(paths,1); path=paths{1}; d=dir(path); t.verifyGreaterThan(d.bytes,50000);
        rgb=imread(path); t.verifyEqual(reshape(rgb(1,1,:),1,3),uint8([204 204 204]));
        blue=rgb(:,:,1)==0 & rgb(:,:,2)==114 & rgb(:,:,3)==189; t.verifyTrue(any(blue,'all'));
        gold=rgb(:,:,1)==196 & rgb(:,:,2)==143 & rgb(:,:,3)==0; t.verifyTrue(any(gold,'all'));
        t.verifyGreaterThanOrEqual(size(rgb,2),2400);
    end
end
end
