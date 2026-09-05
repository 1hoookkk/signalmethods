function spectrogramSurface(room,f,t,db)
ax=room.axes; high=min(20000,room.sound.fs/2);
if room.envelope, high=min(high,room.envelopeHz(end)); end
grid=linspace(20,high,256)';
if room.logF, grid=logspace(log10(20),log10(high),256)'; end
z=interp1(f,db,grid,'linear','extrap')+room.gain;
if room.threeD
    set(room.image,'Visible','off'); set(room.surface,'Visible','on','XData',grid', ...
        'YData',t(:),'ZData',max(room.floor,z'),'CData',z');
    set(ax,'XScale',choose(room.logF,'log','linear'),'YScale','linear', ...
        'XLim',[20 high],'YLim',[0 max(.01,room.sound.duration)],'ZLim',[room.floor 30]);
    xlabel(ax,'kHz'); ylabel(ax,'sound'); zlabel(ax,'dB'); view(ax,-38,28);
    set(ax,'XTick',[100 1000 10000],'XTickLabel',{'0.1','1','10'});
    set([room.cursorLine room.inLine room.outLine],'Visible','off');
else
    y=grid; if room.logF, y=log10(y); end
    set(room.surface,'Visible','off'); set(room.image,'Visible','on','XData',[t(1) t(end)],'YData',[y(1) y(end)],'CData',z);
    set(ax,'XScale','linear','YScale','linear','XLim',[0 max(.01,room.sound.duration)], ...
        'YLim',[y(1) y(end)],'YDir','normal'); view(ax,2);
    ticks=[100 1000 10000]; if room.logF, ticks=log10(ticks); end
    set(ax,'YTick',ticks,'YTickLabel',{'0.1','1','10'},'XTickMode','auto','XTickLabelMode','auto');
    xlabel(ax,'sound'); ylabel(ax,'kHz');
    set([room.cursorLine room.inLine room.outLine],'Visible','on');
    set(room.cursorLine,'YData',[y(1) y(end)],'XData',[room.cursor room.cursor]);
    set(room.inLine,'YData',[y(1) y(end)],'XData',[room.region(1) room.region(1)]);
    set(room.outLine,'YData',[y(1) y(end)],'XData',[room.region(2) room.region(2)]);
end
clim(ax,[room.floor 30]); set(ax,'Visible',onOff(room.axesOn));
end
function value=choose(tf,a,b), value=b; if tf, value=a; end, end
function value=onOff(tf), value='off'; if tf, value='on'; end, end
