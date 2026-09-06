function path = recording_methods(root,folder)
if nargin<1, root=trench.setup; end
if nargin<2, folder=fullfile(root,'native','workstation','artifacts','shots'); end
wav=fullfile(root,'recipes','recordings','test-vowel-ah.wav'); [x,fs]=audioread(wav); x=mean(double(x),2);
n=numel(x); block=x(round(n*.35):round(n*.65)); hz=trench.bridge.curveHz;
[power,f]=pwelch(block,hann(4096),2048,8192,fs); spectrum=interp1(f,10*log10(max(power,realmin)),hz,'linear','extrap'); spectrum=spectrum-max(spectrum);
smooth=interp1(f,movmean(10*log10(max(power,realmin)),25),hz,'linear','extrap'); smooth=smooth-max(smooth);
methods=cell(1,4); poles=cell(1,4);
methods{1}='peaks on the smoothed spectrum';
[~,location,~,prominence]=findpeaks(smooth(:),hz(:),'MinPeakProminence',6); [~,order]=sort(prominence,'descend'); poles{1}=sort(location(order(1:min(6,end))));
curves{1}=smooth;
e=trench.model.lpcEnvelope(block,fs); [curves{2},poles{2}]=envelopeOf(e.a,e.gain,e.rate,hz); methods{2}='LPC-12 at 11,025 Hz, the Spectrogram method';
pre=filter([1 -.97],1,block); e=trench.model.lpcEnvelope(pre,fs); [curves{3},poles{3}]=envelopeOf(e.a,e.gain,e.rate,hz);
curves{3}=curves{3}-20*log10(abs(1-.97*exp(-1j*2*pi*hz(:)/e.rate)))'; methods{3}='LPC-12 with 0.97 pre-emphasis, divided back';
rate=44100; y=resample(block,rate,fs); y=y-mean(y); [a,g]=lpc(y.*hann(numel(y)),24); [curves{4},poles{4}]=envelopeOf(a,g,rate,hz); methods{4}='wildcard: LPC-24 at 44,100 Hz';
colors=[0 .447 .741; .851 .325 .098; .494 .184 .556; .769 .561 0];
fig=figure('Visible','off','Color','w','Position',[1 1 1600 900],'Renderer','opengl','Theme','light','InvertHardcopy','off');
for m=1:4
    ax=subplot(2,2,m,'Parent',fig); hold(ax,'on');
    plot(ax,hz,spectrum,'Color',[.8 .8 .8],'LineWidth',.8); plot(ax,hz,smooth,'k','LineWidth',1.2);
    c=curves{m}(:); c=c-max(c(hz<5000)); plot(ax,hz,c,'Color',colors(m,:),'LineWidth',1.5);
    for f0=poles{m}(:)', line(ax,[f0 f0],[-70 -62],'Color',colors(m,:),'LineWidth',2); end
    set(ax,'XScale','log','XLim',[40 20000],'YLim',[-70 10],'XTick',[100 1000 10000],'XTickLabel',{'100','1k','10k'},'FontName','Arial','Box','on');
    line(ax,[5512.5 5512.5],[-70 10],'Color',[.75 .75 .75],'LineStyle',':');
    title(ax,sprintf('%s   %s Hz',methods{m},mat2str(round(poles{m}(:)'))),'FontName','Arial','FontWeight','normal','FontSize',9);
    if m==1, legend(ax,{'spectrum of the recording','smoothed spectrum','method'},'Location','southwest','FontName','Arial'); end
end
path=fullfile(folder,'recording_methods.png'); print(fig,path,'-dpng','-r110'); delete(fig);
end
function [db,f]=envelopeOf(a,g,rate,hz)
a=a(:); db=(20*log10(sqrt(max(g,1e-12))./abs(exp(-1j*2*pi*hz(:)*(0:numel(a)-1)/rate)*a)))'; db(hz>rate/2)=NaN;
z=roots(a); z=z(imag(z)>1e-9 & abs(z)<1); f=sort(angle(z)*rate/(2*pi)); f=f(f>=30);
end
