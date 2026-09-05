function build(room)
c=room.controls;
c.slider('OPEN',[log(250) log(900)],log(500),[12 310 358 22],@(s,~) room.setOpen(exp(s.Value)));
c.number('',500,[270 339 100 24],@(s,~) room.setOpen(str2double(s.String)));
c.key('SHARPEN',[12 256 169 33],@(~,~) room.sharpen);
c.key('CEILING',[196 256 174 33],@(~,~) room.setCeiling);
c.key('SHOW HZ',[12 205 358 30],@(s,~) room.setShowHz(logical(s.Value)),true);
c.label('COPY TO',[12 171 358 18]);
labels={'M0 Q0','M1 Q0','M0 Q1','M1 Q1'};
for k=1:4
    c.key(labels{k},[12+mod(k-1,2)*184 131-floor((k-1)/2)*43 174 33],@(~,~) room.copyTo(k));
end
room.wholeAxes=trench.ui.draw.fixedGrid(room.display,[360 644 336 224]);
room.wholeCurve=trench.ui.draw.responseCurve(room.wholeAxes,[.769 .561 0]);
room.buildRows;
end
