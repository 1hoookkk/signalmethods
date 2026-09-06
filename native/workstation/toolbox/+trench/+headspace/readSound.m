function [chord,poles] = readSound(path)
[x,fs]=audioread(path); x=mean(double(x),2);
n=numel(x); block=x(max(1,round(n*.25)):round(n*.75));
e=trench.model.lpcEnvelope(block,fs);
z=roots(e.a); z=z(imag(z)>1e-9 & abs(z)<1);
hz=angle(z)*e.rate/(2*pi); radius=abs(z); [hz,order]=sort(hz); radius=radius(order);
keep=hz>=30 & hz<e.rate/2; hz=hz(keep); radius=radius(keep);
bandwidth=-log(radius)*e.rate/pi; width=12*log2(1+bandwidth./hz);
poles=[hz bandwidth]; chord=repmat([0 60 12 0 60 12 0],6,1);
for row=1:min(6,numel(hz))
    chord(row,1:3)=[1 trench.bridge.noteOf(hz(row)) min(120,width(row))];
end
end
