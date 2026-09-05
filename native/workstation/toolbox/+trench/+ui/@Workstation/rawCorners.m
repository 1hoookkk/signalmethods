function c = rawCorners(app)
c=zeros(6,5,4,'uint16');
for k=1:4
    i=find(app.bank.indices==app.corners(k),1);
    assert(~isempty(i),'trench:body:corner','Corner is empty.');
    c(:,:,k)=app.bank.frames(i).words;
end
end
