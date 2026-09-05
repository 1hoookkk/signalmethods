function refresh(room)
if isempty(room.chord), return; end
w=trench.bridge.compile(room.chord); hz=trench.bridge.curveHz;
whole=trench.bridge.responseDb(room.app.wordsOf,hz); set(room.wholeCurve,'YData',whole);
g=trench.bridge.geometry(w); shape=trench.bridge.shapeOf(room.chord);
for k=1:6
    row=trench.bridge.sectionDb(w,k,hz);
    set(room.rowCurves(k),'YData',row); set(room.wholeCurves(k),'YData',whole);
    if g(k,1)
        y=trench.bridge.sectionDb(w,k,g(k,2));
        set(room.poles(k),'XData',g(k,2),'YData',max(-29,min(29,y)));
    else, set(room.poles(k),'XData',NaN,'YData',NaN); end
    if g(k,4)
        y=trench.bridge.sectionDb(w,k,g(k,5));
        set(room.zeros(k),'XData',g(k,5),'YData',max(-29,min(29,y)));
    else, set(room.zeros(k),'XData',NaN,'YData',NaN); end
    pitch=room.chord(k,2); if k==6 && room.chord(k,4), pitch=room.chord(k,5); end
    set(room.pitchBoxes(k),'String',trench.bridge.noteName(pitch));
    set(room.widthBoxes(k),'String',sprintf('%.3f',room.chord(k,3)));
    amount=trench.bridge.sectionDb(w,k,trench.bridge.hzOf(pitch));
    set(room.amountBoxes(k),'String',sprintf('%.2f',amount));
    set(room.zeroLabels(k),'String',zeroName(room.chord(k,:)));
    text=sprintf('%d   +%.2f',k,pitch-shape(1)); if k==1, text='1   root'; elseif k==6, text='6   CEILING'; end
    set(room.intervalLabels(k),'String',text);
    set(room.hzLabels(k),'String',sprintf('%.1f Hz  r %.5f | %.1f Hz  r %.5f',g(k,2),g(k,3),g(k,5),g(k,6)), ...
        'Visible',trench.ui.draw.onOff(room.showHz));
end
end
function name=zeroName(c)
if ~c(4), name='off';
elseif c(6)==0, name='ceiling';
elseif abs(c(5)-c(2))>.05, name='below';
elseif c(6)>c(3)*1.01, name='wider';
elseif c(6)<c(3)*.99, name='tighter';
else, name='on pole'; end
end
