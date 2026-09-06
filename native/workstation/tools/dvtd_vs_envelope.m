function path = dvtd_vs_envelope(root,folder)
if nargin<1, root=trench.setup; end
if nargin<2, folder=fullfile(root,'native','workstation','artifacts','shots'); end
if ~isfolder(folder), mkdir(folder); end
anchors=jsondecode(fileread(fullfile(root,'native','workstation','data','headspace-anchors.json'))).anchors;
leaves={'s1-01-bahn-tense-a','s1-03-tiere-tense-i','s1-05-bude-tense-u','s1-19-butter-lax-u'};
names={'tense-a bahn s1','tense-i tiere s1','tense-u bude s1','lax-u butter s1'};
fs=11025; hz=trench.bridge.curveHz;
fig=figure('Visible','off','Color','w','Position',[1 1 1400 900],'Renderer','opengl','Theme','light','InvertHardcopy','off');
for k=1:4
    table=readmatrix(fullfile(root,'recipes','vocal','dvtd','subject-1',leaves{k},[leaves{k} '-vvtf-measured.txt']),'NumHeaderLines',1);
    measured=20*log10(max(table(:,2),1e-6)); measured=interp1(table(:,1),measured,hz,'linear','extrap');
    a=anchors(strcmp({anchors.name},names{k})); current=trench.bridge.responseDb(trench.bridge.unityDc(trench.bridge.compile(a.chord)),hz);
    grid=(0:fs/65536:fs/2-fs/65536)'; power=interp1(table(:,1),table(:,2),grid,'linear','extrap').^2;
    ac=real(ifft([power; flipud(power(2:end-1))])); ac=ac(1:13); ac(1)=ac(1)*(1+1e-9);
    [lpcA,e]=levinson(ac,12); lpcA=lpcA(:); g=sqrt(max(e,1e-12));
    envelope=20*log10(g./abs(exp(-1j*2*pi*hz(:)*(0:12)/fs)*lpcA)); envelope(hz>fs/2)=NaN;
    tilt=1./(1+(grid/100).^2); tilted_power=power.*tilt.^2;
    tac=real(ifft([tilted_power; flipud(tilted_power(2:end-1))])); tac=tac(1:13); tac(1)=tac(1)*(1+1e-9);
    [tiltA,te]=levinson(tac,12); tiltA=tiltA(:);
    tilted=20*log10(sqrt(max(te,1e-12))./abs(exp(-1j*2*pi*hz(:)*(0:12)/fs)*tiltA))-20*log10(1./(1+(hz(:)/100).^2)); tilted(hz>fs/2)=NaN;
    zt=roots(tiltA); zt=zt(imag(zt)>0); ft=sort(angle(zt)*fs/(2*pi));
    z=roots(lpcA); z=z(imag(z)>0); f=angle(z)*fs/(2*pi); r=abs(z); [f,order]=sort(f); r=r(order);
    chord=repmat([0 60 12 0 60 12 0],6,1);
    for row=1:min(6,numel(f))
        note=trench.bridge.noteOf(f(row)); width=trench.bridge.widthOf(f(row),r(row));
        chord(row,:)=[1 note width 1 note min(120,16*width) 0];
    end
    six=trench.bridge.responseDb(trench.bridge.unityDc(trench.bridge.compile(trench.bridge.fitVoices(chord))),hz);
    ax=subplot(2,2,k,'Parent',fig); hold(ax,'on');
    plot(ax,hz,measured-measured(1),'k','LineWidth',1.4);
    plot(ax,hz,current-current(1),'Color',[0 .447 .741],'LineWidth',1.2);
    plot(ax,hz,envelope-envelope(1),'Color',[.851 .325 .098],'LineWidth',1.2);
    plot(ax,hz,six-six(1),'--','Color',[.769 .561 0],'LineWidth',1.2);
    plot(ax,hz,tilted-tilted(1),'Color',[.494 .184 .556],'LineWidth',1.2);
    for row=1:numel(ft), line(ax,[ft(row) ft(row)],[-34 -28],'Color',[.494 .184 .556],'LineWidth',2); end
    for row=1:numel(f), line(ax,[f(row) f(row)],[-40 -34],'Color',[.851 .325 .098],'LineWidth',2); end
    set(ax,'XScale','log','XLim',[20 20000],'YLim',[-40 40],'XTick',[100 1000 10000],'XTickLabel',{'100','1k','10k'},'FontName','Arial','Box','on');
    line(ax,[fs/2 fs/2],[-40 40],'Color',[.7 .7 .7],'LineStyle',':');
    title(ax,sprintf('%s   LPC-12 poles %s   with source %s Hz',names{k},mat2str(round(f')),mat2str(round(ft'))),'FontName','Arial','FontWeight','normal');
    if k==1, legend(ax,{'measured transfer function','current anchor (peaks, 5 bells + shelf)','Spectrogram envelope, LPC-12 at 11,025 Hz','six bells from the LPC poles','LPC-12 with a -12 dB/oct source applied first'},'Location','southwest','FontName','Arial'); end
end
path=fullfile(folder,'dvtd_vs_envelope.png'); print(fig,path,'-dpng','-r110'); delete(fig);
end
