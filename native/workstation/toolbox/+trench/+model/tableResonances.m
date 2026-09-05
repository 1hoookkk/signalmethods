function [r,shelf] = tableResonances(hz,magnitude)
db=20*log10(max(magnitude,1e-6)); grid=logspace(log10(40),log10(16000),1200)';
smooth=interp1(hz,movmean(db,51),grid,'linear','extrap');
[peak,location,~,prominence]=findpeaks(smooth,grid,'MinPeakProminence',3);
bw=zeros(size(location));
for j=1:numel(location)
    i=find(grid==location(j),1); a=i; b=i;
    while a>1 && smooth(a)>peak(j)-3, a=a-1; end
    while b<numel(grid) && smooth(b)>peak(j)-3, b=b+1; end
    bw(j)=grid(b)-grid(a);
end
keep=bw<=location; r=sortrows([location(keep) bw(keep) prominence(keep)],1);
r=r(1:min(5,size(r,1)),:);
gain=max(-18,min(18,interp1(grid,smooth,100)-median(smooth)));
corner=200; if ~isempty(r), corner=r(1,1)/2; end
shelf=[gain corner];
end
