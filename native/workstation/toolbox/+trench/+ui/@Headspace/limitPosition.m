function point=limitPosition(app,point)
point=point(:)';
if ~isnan(pointLocation(app.tri,point)), return; end
edges=freeBoundary(app.tri); best=Inf; projected=point;
for k=1:size(edges,1)
    a=app.points(edges(k,1),:); b=app.points(edges(k,2),:); d=b-a;
    t=max(0,min(1,dot(point-a,d)/dot(d,d))); candidate=a+t*d;
    distance=sum((point-candidate).^2);
    if distance<best, best=distance; projected=candidate; end
end
point=projected;
end
