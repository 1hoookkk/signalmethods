function names = cornerNames(app)
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'}; names=labels;
for k=1:4
    if app.copies(k)>0, names{k}=['copy of ' labels{app.copies(k)}];
    elseif app.corners(k)>0
        i=find(app.bank.indices==app.corners(k),1);
        if ~isempty(i), names{k}=app.bank.frames(i).name; end
    end
end
end
