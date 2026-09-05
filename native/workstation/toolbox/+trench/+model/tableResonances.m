function r = tableResonances(hz,magnitude,fs,order)
grid=(0:fs/65536:fs/2-fs/65536)';
power=interp1(hz,magnitude,grid,'linear'); power(grid<hz(1))=magnitude(1); power(grid>hz(end))=magnitude(end);
power=max(power,1e-6).^2; full=[power; flipud(power(2:end-1))];
ac=real(ifft(full)); ac=ac(1:order+1); ac(1)=ac(1)*(1+1e-6);
[a,e]=levinson(ac,order); a=a(:); g=sqrt(max(e,1e-12));
envelope=@(f) 20*log10(g./abs(exp(-1j*2*pi*f(:)*(0:order)/fs)*a));
median_=median(envelope(logspace(log10(60),log10(16000),500)'));
z=roots(a); z=z(imag(z)>0 & abs(z)<1);
f=angle(z)*fs/(2*pi); bw=-log(abs(z))*fs/pi;
keep=f>=30 & f<=.45*fs & bw<=f; f=f(keep); bw=bw(keep);
p=envelope(f)-median_; keep=p>=3;
r=sortrows([f(keep) bw(keep) p(keep)],-3);
r=sortrows(r(1:min(5,size(r,1)),:),1);
end
