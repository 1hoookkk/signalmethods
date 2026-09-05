function peaks=lpcPeaks(envelope)
r=roots(envelope.a); r=r(imag(r)>1e-8 & abs(r)<1);
frequency=angle(r)*envelope.rate/(2*pi);
width=-envelope.rate/pi*log(abs(r));
[height,location,~,prominence]=findpeaks(envelope.db,envelope.hz);
eligible=location>=20 & location<envelope.rate/2;
height=height(eligible); location=location(eligible); prominence=prominence(eligible);
matches=zeros(0,4);
for k=1:numel(r)
    candidates=find(abs(location-frequency(k))<=max(width(k),2*envelope.rate/numel(envelope.impulse)));
    if isempty(candidates), continue; end
    [~,best]=max(prominence(candidates)); j=candidates(best);
    matches(end+1,:)=[location(j) width(k) height(j) abs(location(j)-frequency(k))];
end
peaks=zeros(0,3); if isempty(matches), return; end
matches=sortrows(matches,4);
[~,uniqueRows]=unique(matches(:,1),'stable'); peaks=sortrows(matches(uniqueRows,1:3),1);
end
