function label = cornerOf(app,slot)
names={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
label=strjoin(names(app.corners==slot & app.copies==0 & slot>0),' ');
end
