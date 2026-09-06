function [chord,vertices,weights]=chordAt(app,point)
xy=(point-app.low)./app.span;
if isnan(pointLocation(app.tri,xy))
    edges=freeBoundary(app.tri); best=Inf; projected=xy;
    for k=1:size(edges,1)
        a=app.tri.Points(edges(k,1),:); b=app.tri.Points(edges(k,2),:); d=b-a;
        t=max(0,min(1,dot(xy-a,d)/dot(d,d))); candidate=a+t*d;
        distance=sum((xy-candidate).^2);
        if distance<best, best=distance; projected=candidate; end
    end
    xy=projected;
end
[~,chord,vertices,weights]=trench.model.blendAt(app.frames,app.tri,xy);
notes=point([2 1])';
chord(1:2,5)=chord(1:2,5)+notes-chord(1:2,2);
chord(1:2,2)=notes;
end
