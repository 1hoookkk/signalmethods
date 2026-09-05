function setCorner(app,k)
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
app.current=k; app.status=labels{k}; app.refresh;
end
