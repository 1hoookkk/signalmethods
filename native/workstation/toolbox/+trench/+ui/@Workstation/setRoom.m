function setRoom(app,name)
name=char(lower(string(name)));
if strcmp(name,'corner') && strcmp(app.room,'corner') && ~isempty(app.rooms.corner.chord), return; end
if ismember(name,{'morph','corner'}) && ~all(app.corners>0), return; end
if strcmp(app.room,'corner') && ~strcmp(name,'corner'), app.rooms.corner.finishEdit; end
app.room=name;
set(app.response.spectrum,'Visible',trench.ui.draw.onOff(strcmp(name,'sound')));
fields=fieldnames(app.rooms);
for k=1:numel(fields)
    room=app.rooms.(fields{k}); on=strcmp(fields{k},name);
    set(room.controls.panel,'Visible',trench.ui.draw.onOff(on));
    set(room.display,'Visible',trench.ui.draw.onOff(on));
    color=[.886 .886 .886]; textColor=[0 0 0];
    if on, color=[.169 .169 .169]; textColor=[1 1 1]; end
    set(app.nav(k),'Value',on,'BackgroundColor',color,'ForegroundColor',textColor);
end
switch name
    case 'morph', app.live=struct('kind','wheel'); app.rooms.morph.refresh;
    case 'corner', app.rooms.corner.beginEdit;
    case 'sound', app.rooms.sound.readSlice;
    case 'frames'
        if app.chosen>0, app.selectFrame(app.chosen); end
        app.rooms.frames.refresh;
end
app.refresh;
end
