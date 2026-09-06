function chord=blend(chords,weights)
weights=weights(:)'/sum(weights);
one=find(weights==1,1);
if ~isempty(one), chord=chords(:,:,one); return; end
chord=repmat([1 60 12 0 60 12 0],6,1); w=reshape(weights,1,1,[]);
chord(:,[2 7])=sum(chords(:,[2 7],:).*w,3);
chord(:,3)=exp(sum(log(chords(:,3,:)).*w,3));
end
