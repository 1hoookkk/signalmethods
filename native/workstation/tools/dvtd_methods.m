function path = dvtd_methods(root,folder)
if nargin<1, root=trench.setup; end
if nargin<2, folder=fullfile(root,'native','workstation','artifacts','shots'); end
if ~isfolder(folder), mkdir(folder); end
anchors=jsondecode(fileread(fullfile(root,'native','workstation','data','headspace-anchors.json'))).anchors;
leaves={'s1-01-bahn-tense-a','s1-03-tiere-tense-i','s1-05-bude-tense-u','s1-19-butter-lax-u'};
names={'tense-a bahn s1','tense-i tiere s1','tense-u bude s1','lax-u butter s1'};
methods={'peaks on the measured curve (current anchors)','LPC-12 at 11,025 Hz, the Spectrogram method', ...
    'LPC-12 after a -12 dB/oct source, divided back','wildcard: Steiglitz-McBride 12 poles 12 zeros on the impulse response'};
colors=[0 .447 .741; .851 .325 .098; .494 .184 .556; .769 .561 0];
fs=11025; hz=trench.bridge.curveHz; n=4096; grid=(0:n/2)'*fs/n;
fig=figure('Visible','off','Color','w','Position',[1 1 1700 1100],'Renderer','opengl','Theme','light','InvertHardcopy','off');
for k=1:4
    table=readmatrix(fullfile(root,'recipes','vocal','dvtd','subject-1',leaves{k},[leaves{k} '-vvtf-measured.txt']),'NumHeaderLines',1);
    measured=20*log10(max(table(:,2),1e-6)); measured=interp1(table(:,1),measured,hz,'linear','extrap'); measured=measured-measured(1);
    magnitude=interp1(table(:,1),table(:,2),grid,'linear','extrap'); phase=interp1(table(:,1),table(:,3),grid,'linear','extrap');
    power=magnitude.^2;
    a=anchors(strcmp({anchors.name},names{k}));
    curves=cell(1,4); poles=cell(1,4);
    curves{1}=trench.bridge.responseDb(trench.bridge.unityDc(trench.bridge.compile(a.chord)),hz); poles{1}=trench.bridge.hzOf(a.chord(1:5,2));
    [curves{2},poles{2}]=lpcEnvelope(power,fs,hz);
    tilt=1./(1+(grid/100).^2); [curves{3},poles{3}]=lpcEnvelope(power.*tilt.^2,fs,hz); curves{3}=curves{3}-20*log10(1./(1+(hz(:)/100).^2))';
    spectrum=magnitude.*exp(1j*phase); full=[spectrum; conj(flipud(spectrum(2:end-1)))]; impulse=real(ifft(full));
    [b,aa]=stmcb(impulse(1:1024),12,12); response=freqz(b,aa,hz,fs); curves{4}=20*log10(abs(response(:)))';
    z=roots(aa); z=z(imag(z)>0); poles{4}=sort(angle(z)*fs/(2*pi));
    for m=1:4
        ax=subplot(4,4,(m-1)*4+k,'Parent',fig); hold(ax,'on');
        plot(ax,hz,measured,'k','LineWidth',1.3);
        c=curves{m}(:); c(hz(:)>fs/2 & m>1)=NaN; plot(ax,hz,c-c(1),'Color',colors(m,:),'LineWidth',1.3);
        for f=poles{m}(:)', line(ax,[f f],[-40 -33],'Color',colors(m,:),'LineWidth',2); end
        set(ax,'XScale','log','XLim',[40 20000],'YLim',[-40 40],'XTick',[100 1000 10000],'XTickLabel',{'100','1k','10k'},'FontName','Arial','FontSize',8,'Box','on');
        line(ax,[fs/2 fs/2],[-40 40],'Color',[.75 .75 .75],'LineStyle',':');
        if m==1, title(ax,names{k},'FontName','Arial','FontWeight','normal','FontSize',10); end
        if k==1, ylabel(ax,methods{m},'FontName','Arial','FontSize',8); end
    end
end
path=fullfile(folder,'dvtd_methods.png'); print(fig,path,'-dpng','-r110'); delete(fig);
end
function [db,f]=lpcEnvelope(power,fs,hz)
ac=real(ifft([power; flipud(power(2:end-1))])); ac=ac(1:13); ac(1)=ac(1)*(1+1e-9);
[a,e]=levinson(ac,12); a=a(:); g=sqrt(max(e,1e-12));
db=(20*log10(g./abs(exp(-1j*2*pi*hz(:)*(0:12)/fs)*a)))';
z=roots(a); z=z(imag(z)>0); f=sort(angle(z)*fs/(2*pi));
end
