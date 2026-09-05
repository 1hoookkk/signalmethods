function chord = blendChords(chords,weights)
[weights,order]=sort(weights(:)','descend'); chords=chords(:,:,order);
keep=weights>1e-9; weights=weights(keep)/sum(weights(keep)); chords=chords(:,:,keep); n=numel(weights);
if n==1, chord=chords(:,:,1); return; end
a=chords(:,:,1); led=zeros(6,7,n);
for k=2:n, [a,led(:,:,k)]=trench.bridge.leadTo(a,chords(:,:,k)); end
led(:,:,1)=a;
for k=2:n
    for r=1:5
        if a(r,1) && ~led(r,1,k), led(r,1:6,k)=[1 a(r,2) a(r,3) 1 a(r,2) a(r,3)]; end
        if a(r,4) && ~led(r,4,k), led(r,4:6,k)=[1 a(r,5) 120]; end
    end
end
chord=led(:,:,1);
chord(:,[1 4])=double(any(led(:,[1 4],:)~=0,3));
chord(:,[2 5 7])=sum(led(:,[2 5 7],:).*reshape(weights,1,1,n),3);
w=exp(sum(log(max(led(:,[3 6],:),1e-3)).*reshape(weights,1,1,n),3));
w(all(led(:,[3 6],:)==0,3))=0; chord(:,[3 6])=w;
end
