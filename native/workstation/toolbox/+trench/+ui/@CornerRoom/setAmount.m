function setAmount(room,row,value)
if ~isfinite(value), return; end
c=room.chord; if ~c(row,1), return; end
c(row,4:5)=[1 c(row,2)]; hz=trench.bridge.hzOf(c(row,2));
width=fminbnd(@(x) errorAt(c,row,hz,x,value),0,120,optimset('TolX',1e-5,'Display','off'));
c(row,6)=width; room.landChord(c);
end
function e=errorAt(c,row,hz,width,target)
c(row,6)=width; w=trench.bridge.compile(c);
e=(trench.bridge.sectionDb(w,row,hz)-target)^2;
end
