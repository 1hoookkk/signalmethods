function raw=rawCorners(app)
assert(any(app.corners),'No corner placed.');
filled=app.corners; fallback={[],1,1,[2 3 1]};
for k=1:4
    if filled(k)>0, continue; end
    for j=fallback{k}, if filled(j)>0, filled(k)=filled(j); break; end, end
    if filled(k)==0, filled(k)=app.corners(find(app.corners,1)); end
end
raw=zeros(6,5,4,'uint16');
for k=1:4, raw(:,:,k)=app.frames(filled(k)).words; end
end
