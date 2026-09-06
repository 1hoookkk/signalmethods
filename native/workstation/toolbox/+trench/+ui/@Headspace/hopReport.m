function report=hopReport(app)
report='';
if nnz(unique(app.corners(app.corners>0)))<2, return; end
c=trench.bridge.bodyCorners(app.rawCorners,true(1,6),true);
edges={[0 0; 1 0],'M0 Q0 > M1 Q0';[0 1; 1 1],'M0 Q1 > M1 Q1';[0 0; 0 1],'M0 Q0 > M0 Q1';[1 0; 1 1],'M1 Q0 > M1 Q1'};
worst=0; where=''; guarded=false;
for e=1:4
    notes=zeros(21,6);
    for k=0:20
        t=k/20; m=edges{e,1}(1,1)*(1-t)+edges{e,1}(2,1)*t; qq=edges{e,1}(1,2)*(1-t)+edges{e,1}(2,2)*t;
        [w,g]=trench.bridge.wheelMorph(c,m,qq); guarded=guarded||any(g); ch=trench.bridge.decompile(w); notes(k+1,:)=ch(:,2)';
    end
    step=max(abs(diff(notes)),[],'all');
    if step>worst, worst=step; where=edges{e,2}; end
end
report=sprintf('hops: worst step %.1f st on %s',worst,where);
if worst>6, report=sprintf('hop breaks: pole jumps %.1f st in one twentieth of %s',worst,where); end
if guarded, report=[report '   guard hit']; end
end
