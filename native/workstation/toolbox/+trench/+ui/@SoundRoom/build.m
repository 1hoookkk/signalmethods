function build(room)
c=room.controls;
c.key('OPEN SOUND FILE',[10 339 231 26],@(~,~) room.openSound);
c.key('LIVE IN',[249 339 122 26],@(~,~) room.setLiveInput,true);
c.number('FFT SIZE',2048,[10 284 109 25],@(s,~) room.setSetting('fftSize',str2double(s.String)));
c.popup('WINDOW',{'Hann','Blackman','Blackman-Harris','Hamming','rectangular'},[129 284 132 25],@(s,~) room.setSetting('window',s.String{s.Value}));
c.number('STRIDE',512,[271 284 100 25],@(s,~) room.setSetting('stride',str2double(s.String)));
names={'3D','LOG F','AXES','ENVELOPE'}; fields={'threeD','logF','axesOn','envelope'};
for k=1:4
    h=c.key(names{k},[10+(k-1)*91 244 86 25],@(s,~) room.setSetting(fields{k},logical(s.Value)),true);
    h.Value=room.(fields{k});
end
c.slider('GAIN',[-30 60],0,[10 198 170 18],@(s,~) room.setSetting('gain',s.Value));
c.slider('FLOOR',[-120 -10],-80,[201 198 170 18],@(s,~) room.setSetting('floor',s.Value));
room.cursorSlider=c.slider('CURSOR',[0 1],0,[10 151 259 18],@(s,~) room.setCursor(s.Value));
room.cursorBox=c.number('',0,[279 148 92 25],@(s,~) room.setCursor(str2double(s.String)));
room.inBox=c.number('IN',0,[10 96  90 25],@(s,~) room.setRegion(str2double(s.String),room.region(2)));
room.outBox=c.number('OUT',1,[108 96  90 25],@(s,~) room.setRegion(room.region(1),str2double(s.String)));
c.key('SPEECH',[210 96 76 25],@(~,~) room.setSetting('mode','speech'),true);
c.key('BELLS',[294 96 77 25],@(~,~) room.setSetting('mode','bells'),true);
c.key('READ FRAME AT CURSOR',[10 27 361 40],@(~,~) room.readFrameAtCursor);
labels=cellfun(@(p) fileName(p),room.paths,'UniformOutput',false);
room.soundList=uicontrol(room.display,'Style','listbox','String',[{'LIVE IN'}; labels(:)], ...
    'FontName','Arial','FontSize',10,'BackgroundColor','w','Position',[16 761 1022 115], ...
    'Callback',@(s,~) selectSound(room,s.Value));
room.axes=axes(room.display,'Units','pixels','Position',[76 68 913 647], ...
    'Color','w','FontName','Arial','FontSize',10,'Box','on','NextPlot','add');
room.surface=surf(room.axes,[0 1],[20 20000],zeros(2),'EdgeColor','none','Visible','off','HitTest','off');
room.image=image(room.axes,'XData',[0 1],'YData',[log10(20) log10(20000)],'CData',zeros(2),'CDataMapping','scaled','HitTest','off');
room.cursorLine=line(room.axes,[0 0],[log10(20) log10(20000)],'Color',[.769 .561 0],'LineWidth',1.5);
room.inLine=line(room.axes,[0 0],[log10(20) log10(20000)],'Color',[.851 .325 .098],'LineStyle','--');
room.outLine=line(room.axes,[1 1],[log10(20) log10(20000)],'Color',[.851 .325 .098],'LineStyle','--');
room.voiceMarks=line(room.axes,NaN,NaN,'Marker','s','Color',[0 .447 .741],'LineStyle','none','MarkerFaceColor',[0 .447 .741]);
set(room.axes,'ButtonDownFcn',@(~,~) startMouse(room,'onCursor'));
set(room.cursorLine,'ButtonDownFcn',@(~,~) startMouse(room,'onCursor'));
set(room.inLine,'ButtonDownFcn',@(~,~) startMouse(room,'onIn'));
set(room.outLine,'ButtonDownFcn',@(~,~) startMouse(room,'onOut'));
colormap(room.axes,parula(256)); xlabel(room.axes,'sound'); ylabel(room.axes,'kHz');
end
function name=fileName(path), [~,name]=fileparts(path); end
function selectSound(room,index)
if index==1, room.setLiveInput; else, room.openSound(room.paths{index-1}); end
end
function startMouse(room,method)
p=room.axes.CurrentPoint;
if strcmp(room.app.figure.SelectionType,'alt') && room.threeD, method='onOrbit'; end
room.app.beginDrag(room,method,room.axes,p(1,1:2));
end
