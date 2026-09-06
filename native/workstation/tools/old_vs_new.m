function path = old_vs_new(root,folder)
if nargin<1, root=trench.setup; end
if nargin<2, folder=fullfile(root,'native','workstation','artifacts','shots'); end
old=jsondecode(fileread(fullfile(root,'native','workstation','artifacts','old-anchors-v1.json'))).anchors;
new=jsondecode(fileread(fullfile(root,'native','workstation','data','headspace-anchors.json'))).anchors;
leaves={'s1-01-bahn-tense-a','s1-03-tiere-tense-i','s1-05-bude-tense-u','s1-19-butter-lax-u'};
names={'tense-a bahn s1','tense-i tiere s1','tense-u bude s1','lax-u butter s1'}; hz=trench.bridge.curveHz;
fig=figure('Visible','off','Color','w','Position',[1 1 1800 900],'Renderer','opengl','Theme','light','InvertHardcopy','off');
for k=1:4
    folderK=fullfile(root,'recipes','vocal','dvtd','subject-1',leaves{k});
    table=readmatrix(fullfile(folderK,[leaves{k} '-vvtf-measured.txt']),'NumHeaderLines',1);
    measured=interp1(table(:,1),20*log10(max(table(:,2),1e-6)),hz,'linear','extrap'); measured=measured-measured(1);
    [x,fs]=audioread(fullfile(folderK,[leaves{k} '-model-sound.wav'])); x=mean(double(x),2); x=x(round(end/4):round(3*end/4));
    [power,f]=pwelch(x,hann(2048),1024,4096,fs); sound=interp1(f,10*log10(max(power,realmin)),hz,'linear','extrap'); sound=sound-max(sound);
    a=old(strcmp({old.name},names{k})); before=trench.bridge.responseDb(trench.bridge.unityDc(trench.bridge.compile(a.chord)),hz);
    [chord,poles]=trench.headspace.readSound(fullfile(folderK,[leaves{k} '-model-sound.wav'])); chord=trench.bridge.fitVoices(chord);
    after=trench.bridge.responseDb(trench.bridge.unityDc(trench.bridge.compile(chord)),hz); admitted=any(strcmp({new.name},names{k}));
    traces={before-before(1),after-after(1)}; colors=[0 .447 .741; .851 .325 .098];
    labels={'before: peaks, five bells and shelf','after: LPC-12 of the model sound, poles only'};
    for m=1:2
        ax=subplot(2,4,(m-1)*4+k,'Parent',fig); hold(ax,'on');
        plot(ax,hz,measured,'k','LineWidth',1.2); plot(ax,hz,traces{m},'Color',colors(m,:),'LineWidth',1.4);
        if m==2, for f0=poles(:,1)', line(ax,[f0 f0],[-40 -34],'Color',colors(m,:),'LineWidth',2); end, end
        set(ax,'XScale','log','XLim',[40 20000],'YLim',[-40 40],'XTick',[100 1000 10000],'XTickLabel',{'100','1k','10k'},'FontName','Arial','FontSize',9,'Box','on');
        line(ax,[5512.5 5512.5],[-40 40],'Color',[.75 .75 .75],'LineStyle',':');
        state='admitted'; if ~admitted, state='not admitted'; end
        if m==1, title(ax,names{k},'FontName','Arial','FontWeight','normal'); else, title(ax,sprintf('%d pole pairs, %s',size(poles,1),state),'FontName','Arial','FontWeight','normal'); end
        if k==1, ylabel(ax,labels{m},'FontName','Arial','FontSize',9); end
    end
end
path=fullfile(folder,'old_vs_new.png'); print(fig,path,'-dpng','-r110'); delete(fig);
end
