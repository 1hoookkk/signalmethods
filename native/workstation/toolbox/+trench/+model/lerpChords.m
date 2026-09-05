function chord = lerpChords(a,b,t)
chord=a;
chord(:,[1 4])=double(a(:,[1 4])~=0 | b(:,[1 4])~=0);
chord(:,[2 5])=(1-t)*a(:,[2 5])+t*b(:,[2 5]);
w=exp((1-t)*log(max(a(:,[3 6]),1e-3))+t*log(max(b(:,[3 6]),1e-3)));
w(a(:,[3 6])==0 & b(:,[3 6])==0)=0; chord(:,[3 6])=w;
chord(:,7)=(1-t)*a(:,7)+t*b(:,7);
end
