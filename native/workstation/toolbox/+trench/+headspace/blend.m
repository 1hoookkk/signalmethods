function chord=blend(chords,weights)
weights=weights(:)'/sum(weights);
one=find(weights==1,1);
if ~isempty(one), chord=chords(:,:,one); return; end
chord=ones(6,7); w=reshape(weights,1,1,[]);
chord(:,[2 5 7])=sum(chords(:,[2 5 7],:).*w,3);
chord(:,[3 6])=exp(sum(log(chords(:,[3 6],:)).*w,3));
chord(6,[3 6])=12;
end
