function refresh(room)
app=room.app; range=[0 100]; if room.past, range=[-100 200]; end
set(room.pad,'XLim',range,'YLim',range);
set(room.positionMark,'XData',app.morph,'YData',app.q);
set(room.projection,'XData',[range(1) app.morph app.morph],'YData',[app.q app.q range(1)]);
set(room.morphBox,'String',sprintf('%.2f',app.morph));
set(room.qBox,'String',sprintf('%.2f',app.q));
names=app.cornerLabels;
for k=1:4, set(room.cornerKeys(k),'String',names{k}); end
end
