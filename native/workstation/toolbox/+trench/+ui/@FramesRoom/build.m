function build(room)
c=room.controls; app=room.app;
c.key('NEW BANK',[10 339 112 26],@(~,~) app.newBank);
c.key('OPEN BANK',[130 339 112 26],@(~,~) app.openBank);
c.key('SAVE BANK',[250 339 121 26],@(~,~) app.saveBank);
c.key('SAVE BANK AS',[10 306 172 26],@(~,~) app.saveBank(''));
c.key('REMOVE FROM BANK',[190 306 181 26],@(~,~) app.removeFromBank);
c.popup('BANK',{'SLOT','LOW > HIGH ROOT','NEAREST TO M0 Q0'},[10 250 361 25],@(s,~) room.setSort(s.String{s.Value}));
room.positionSlider=c.slider('POSITION',[0 132],0,[10 201 275 19],@(s,~) room.setPosition(s.Value));
set(room.positionSlider,'Visible','off');
room.positionAxes=axes(c.panel,'Units','pixels','Position',[12 200 271 21], ...
    'XLim',[0 132],'YLim',[0 1],'Visible','off','NextPlot','add','PositionConstraint','innerposition');
line(room.positionAxes,[0 132],[.5 .5],'Color',[.69 .69 .69],'LineWidth',2,'HitTest','off');
room.positionThumb=line(room.positionAxes,0,.5,'Marker','s','MarkerSize',9,'Color',[.769 .561 0], ...
    'MarkerFaceColor',[.769 .561 0],'ButtonDownFcn',@(~,~) positionPress(room));
set(room.positionAxes,'ButtonDownFcn',@(~,~) positionPress(room));
room.positionBox=c.number('',0,[296 197 75 25],@(s,~) room.setPosition(str2double(s.String)));
room.anchorBox=c.number('ANCHOR',1,[10 143  72 25],@(s,~) room.setPosition(str2double(s.String)-1));
room.morphBox=c.number('MORPH',0,[92 143 72 25],@(s,~) room.setPosition(floor(room.position)+str2double(s.String)/100));
c.key('FINE DRAG',[177 143  90 25],@(s,~) setFine(room,s.Value),true);
c.key('SWEEP POSITION',[10 106 151 25],@(s,~) setSweep(room,s.Value),true);
c.number('',4,[169 106  60 25],@(s,~) setSeconds(room,str2double(s.String)));
room.pairLabel=c.label('',[10  73 361  30]);
names=trench.bridge.groupNames;
for k=1:7
    h=c.key(names{k},[10+mod(k-1,4)*91 42-floor((k-1)/4)*31 86 25],@(s,~) room.setGroup(k,logical(s.Value)),true);
    h.Value=room.groups(k);
end
room.bankTitle=uicontrol(room.display,'Style','text','String','','FontName','Arial','FontSize',11, ...
    'BackgroundColor','w','HorizontalAlignment','left','Position',[16 864 495 22]);
uicontrol(room.display,'Style','text','String','ROOT    VOICING    RESONANCE','FontName','Arial', ...
    'BackgroundColor','w','HorizontalAlignment','left','Position',[16 841 495 20]);
room.bankList=uicontrol(room.display,'Style','listbox','String',{''},'BackgroundColor','w', ...
    'FontName','Lucida Console','FontSize',9,'Position',[16 620 495 221], ...
    'Callback',@(s,~) room.onRow(s.Value,'press'));
uicontrol(room.display,'Style','popupmenu','String',{'LOW > HIGH ROOT','NEAREST TO'}, ...
    'FontName','Arial','BackgroundColor','w','Position',[531 861 326 26], ...
    'Callback',@(s,~) setLibrarySort(room,s.String{s.Value}),'Tag','NEAREST TO');
uicontrol(room.display,'Style','pushbutton','String','PUT IN BANK','FontName','Arial', ...
    'Position',[864 861 174 26],'Callback',@(~,~) room.putLibrary,'Tag','PUT IN BANK');
room.libraryList=uicontrol(room.display,'Style','listbox','String',{''},'BackgroundColor','w', ...
    'FontName','Arial','FontSize',9,'Position',[531 620 507 231], ...
    'Callback',@(s,~) room.onLibraryRow(s.Value,'press'));
room.map=axes(room.display,'Units','pixels','Position',[70 60 924 505],'Color','w', ...
    'XLim',[24 108],'YLim',[0 6],'XTick',24:12:108,'XTickLabel',{'C1','C2','C3','C4','C5','C6','C7','C8'}, ...
    'FontName','Arial','Box','on','NextPlot','add');
xlabel(room.map,'ROOT'); ylabel(room.map,'VOICING');
room.dimMarks=scatter(room.map,NaN,NaN,15,[.82 .82 .82],'filled','HitTest','off');
room.pathLine=line(room.map,NaN,NaN,'Color',[.84 .84 .84],'HitTest','off');
room.hopLine=line(room.map,NaN,NaN,'Color',[.769 .561 0],'LineWidth',2,'HitTest','off');
room.anchorLabels=gobjects(1,2);
for k=1:2
    room.anchorLabels(k)=text(room.map,NaN,NaN,'','FontName','Arial','FontSize',10, ...
        'Color','k','BackgroundColor','w','Margin',2,'Clipping','on','HitTest','off');
end
room.cornerLabels=gobjects(1,4);
for k=1:4
    room.cornerLabels(k)=text(room.map,NaN,NaN,'','FontName','Arial','FontSize',10,'FontWeight','bold', ...
        'Color',[.851 .325 .098],'BackgroundColor','w','Margin',2,'Clipping','on','HitTest','off');
end
room.marks=scatter(room.map,NaN,NaN,30,[0 .447 .741],'filled','HitTest','off');
room.positionMark=scatter(room.map,NaN,NaN,85,[.769 .561 0],'s','filled','HitTest','off');
room.projections=line(room.map,NaN,NaN,'Color',[.769 .561 0],'LineStyle',':','HitTest','off');
set(room.map,'ButtonDownFcn',@(~,~) mapPress(room));
end
function mapPress(room)
p=room.map.CurrentPoint; room.onMap(p(1,1:2),'press');
end
function positionPress(room)
p=room.positionAxes.CurrentPoint; room.app.beginDrag(room,'onPosition',room.positionAxes,p(1,1:2));
end
function setFine(room,on), room.fine=logical(on); end
function setSweep(room,on), room.sweep=logical(on); end
function setSeconds(room,n), if isfinite(n)&&n>0, room.hopSeconds=n; end, end
function setLibrarySort(room,name), room.librarySort=name; room.refreshLibrary; end
