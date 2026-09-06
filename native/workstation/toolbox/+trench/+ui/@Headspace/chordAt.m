function [chord,vertices,weights]=chordAt(app,point)
point=app.limitPosition(point);
triangle=pointLocation(app.tri,point);
if isnan(triangle)
    triangles=app.tri.ConnectivityList;
    for k=1:size(triangles,1)
        w=cartesianToBarycentric(app.tri,k,point);
        if min(w)>=-1e-12, triangle=k; break; end
    end
end
assert(~isnan(triangle),'Point outside lattice.');
vertices=app.tri.ConnectivityList(triangle,:);
weights=cartesianToBarycentric(app.tri,triangle,point);
weights=max(0,weights); weights=weights/sum(weights);
chord=trench.headspace.blend(cat(3,app.frames(vertices).chord),weights);
end
