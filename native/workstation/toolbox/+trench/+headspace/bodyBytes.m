function bytes=bodyBytes(corners)
assert(numel(corners)==4 && all(~cellfun(@isempty,corners)),'Four corners required.');
raw=zeros(5,6,4,'uint16');
for k=1:4
    c=corners{k}.chord;
    trench.headspace.validate(c);
    raw(:,:,k)=trench.bridge.unityDc(trench.bridge.compile(c))';
end
words=raw(:); bytes=zeros(240,1,'uint8');
bytes(1:2:end)=uint8(bitand(words,255)); bytes(2:2:end)=uint8(bitshift(words,-8));
end
