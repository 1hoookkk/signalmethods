function build(room)
c=room.controls; app=room.app;
room.morphBox=c.number('MORPH',0,[12 302 169 32],@(s,~) room.setPosition(str2double(s.String),app.q));
room.qBox=c.number('Q',0,[196 302 174 32],@(s,~) room.setPosition(app.morph,str2double(s.String)));
c.key('FINE DRAG',[12 251 169 30],@(s,~) setFine(room,s.Value),true);
c.key('PAST THE ENDS',[196 251 174 30],@(s,~) setPast(room,s.Value),true);
c.key('SWEEP MORPH',[12 204 169 30],@(s,~) setSweep(room,1,s.Value),true);
c.key('SWEEP Q',[196 204 174 30],@(s,~) setSweep(room,2,s.Value),true);
h=c.key('HEAR M0 Q0',[12 151 358 33],@(~,~) []);
set(h,'Enable','inactive','ButtonDownFcn',@(~,~) hearPress(room));
c.key('B: PREVIOUS FRAME',[12 104 169 30],@(~,~) room.nextFrame(-1));
c.key('B: NEXT FRAME',[196 104 174 30],@(~,~) room.nextFrame(1));
c.key('KEEP AS FRAME',[12 51 169 33],@(~,~) room.keepAsFrame);
c.key('EDIT CORNER',[196 51 174 33],@(~,~) app.setRoom('corner'));
room.pad=axes(room.display,'Units','pixels','Position',[117 133 800 620], ...
    'XLim',[0 100],'YLim',[0 100],'XTick',[0 25 50 75 100],'YTick',[0 25 50 75 100], ...
    'FontName','Arial','Box','on','Color','w','NextPlot','add','DataAspectRatio',[1 1 1]);
xlabel(room.pad,'MORPH'); ylabel(room.pad,'Q');
rectangle(room.pad,'Position',[0 0 100 100],'EdgeColor',[.69 .69 .69],'HitTest','off');
room.projection=line(room.pad,NaN,NaN,'Color',[.69 .69 .69],'LineStyle',':','HitTest','off');
room.positionMark=line(room.pad,0,0,'Marker','s','MarkerSize',11,'MarkerFaceColor',[.769 .561 0], ...
    'Color',[.769 .561 0],'LineStyle','none','HitTest','off');
set(room.pad,'ButtonDownFcn',@(~,~) padPress(room));
room.cornerKeys=gobjects(1,4);
positions=[47 70 440  30; 566 70 440 30; 47 795 440 30; 566 795 440 30];
for k=1:4
    room.cornerKeys(k)=uicontrol(room.display,'Style','pushbutton','String','', ...
        'Position',positions(k,:),'FontName','Arial','FontSize',11,'BackgroundColor',[.886 .886 .886], ...
        'Callback',@(~,~) app.showCornerFrame(k));
end
end
function padPress(room)
p=room.pad.CurrentPoint; room.app.beginDrag(room,'onPad',room.pad,p(1,1:2));
end
function hearPress(room)
room.app.beginDrag(room,'onHear',room.pad,[0 0]);
end
function setFine(room,on), room.fine=logical(on); end
function setPast(room,on)
room.past=logical(on); room.setPosition(room.app.morph,room.app.q);
end
function setSweep(room,k,on)
if k==1, room.sweepMorph=logical(on); else, room.sweepQ=logical(on); end
end
