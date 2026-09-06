function path = ir_spectrogram(root,folder)
if nargin<1, root=trench.setup; end
if nargin<2, folder=fullfile(root,'native','workstation','artifacts','shots'); end
downloads='C:\Users\hooki\Downloads';
packs={'Origin Effects - IR-Cab Library V3','JST Anvil IR Pack','Eminence - The Karnivore IR Pack by Kristian Kohle','ML Sound Lab''s BEST IR IN THE WORLD','shift-line bass HD IR pack'};
files={};
for k=1:numel(packs)
    list=dir(fullfile(downloads,packs{k},'**','*.wav')); list=list(~[list.isdir]);
    if isempty(list), continue; end
    [~,order]=sort({list.name}); list=list(order);
    pick=[1 round(numel(list)/2)]; if k>2, pick=1; end
    for p=unique(pick), files{end+1}=fullfile(list(p).folder,list(p).name); end
end
files=files(1:min(8,numel(files))); hz=trench.bridge.curveHz;
fig=figure('Visible','off','Color','w','Position',[1 1 1800 900],'Renderer','opengl','Theme','light','InvertHardcopy','off');
for k=1:numel(files)
    [x,fs]=audioread(files{k}); x=mean(double(x),2); x=x(1:min(numel(x),round(fs*.5)));
    spectrum=20*log10(max(abs(fft(x,65536)),1e-9)); f=(0:65535)'*fs/65536; spectrum=interp1(f(1:32768),spectrum(1:32768),hz,'linear','extrap'); spectrum=spectrum-max(spectrum);
    e=trench.model.lpcEnvelope(x,fs); a=e.a(:); rate=e.rate;
    y=resample(x,rate,fs); [bb,aa]=stmcb(y(1:min(numel(y),2048)),12,12); fit=freqz(bb,aa,hz,rate); fit=20*log10(abs(fit(:)))'; fit(hz>rate/2)=NaN; fit=fit-max(fit);
    zz=roots(aa); zz=zz(imag(zz)>1e-9 & abs(zz)<1); fitPoles=sort(angle(zz)*rate/(2*pi));
    envelope=(20*log10(sqrt(max(e.gain,1e-12))./abs(exp(-1j*2*pi*hz(:)*(0:12)/rate)*a)))'; envelope(hz>rate/2)=NaN; envelope=envelope-max(envelope);
    z=roots(a); z=z(imag(z)>1e-9 & abs(z)<1); poles=sort(angle(z)*rate/(2*pi));
    ax=subplot(2,4,k,'Parent',fig); hold(ax,'on');
    plot(ax,hz,spectrum,'k','LineWidth',1); plot(ax,hz,envelope,'Color',[.851 .325 .098],'LineWidth',1.2); plot(ax,hz,fit,'Color',[.769 .561 0],'LineWidth',1.5);
    for f0=poles(:)', line(ax,[f0 f0],[-60 -55],'Color',[.851 .325 .098],'LineWidth',2); end
    for f0=fitPoles(:)', line(ax,[f0 f0],[-54 -49],'Color',[.769 .561 0],'LineWidth',2); end
    set(ax,'XScale','log','XLim',[40 20000],'YLim',[-60 10],'XTick',[100 1000 10000],'XTickLabel',{'100','1k','10k'},'FontName','Arial','FontSize',8,'Box','on');
    line(ax,[rate/2 rate/2],[-60 10],'Color',[.75 .75 .75],'LineStyle',':');
    [~,name]=fileparts(files{k}); [~,pack]=fileparts(fileparts(files{k}));
    title(ax,sprintf('%s   %s   %d Hz\npoles %s Hz',pack,name,fs,mat2str(round(poles'))),'FontName','Arial','FontWeight','normal','FontSize',7,'Interpreter','none');
end
path=fullfile(folder,'ir_methods.png'); print(fig,path,'-dpng','-r110'); delete(fig);
end
