function point=limitPosition(app,point)
q=app.quadrilateral; point=point(:)';
point(2)=max(q(1,2),min(q(3,2),point(2)));
t=(point(2)-q(1,2))/(q(3,2)-q(1,2));
left=(1-t)*q(1,1)+t*q(4,1); right=(1-t)*q(2,1)+t*q(3,1);
point(1)=max(right,min(left,point(1)));
end
