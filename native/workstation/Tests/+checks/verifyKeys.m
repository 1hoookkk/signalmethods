function verifyKeys(t,app)
expected=struct;
expected.sound={'OPEN SOUND FILE','LIVE IN','FFT SIZE','WINDOW','STRIDE','3D','LOG F','AXES','ENVELOPE','GAIN','FLOOR','CURSOR','IN','OUT','SPEECH','BELLS','READ FRAME AT CURSOR'};
expected.frames={'NEW BANK','OPEN BANK','SAVE BANK','SAVE BANK AS','REMOVE FROM BANK','POSITION','ANCHOR','MORPH','FINE DRAG','SWEEP POSITION','PUT IN BANK'};
expected.morph={'MORPH','Q','FINE DRAG','PAST THE ENDS','SWEEP MORPH','SWEEP Q','HEAR M0 Q0','B: PREVIOUS FRAME','B: NEXT FRAME','KEEP AS FRAME','EDIT CORNER'};
expected.corner={'OPEN','SHARPEN','CEILING','SHOW HZ','PITCH','WIDTH','AMOUNT','LOCK ZERO TO POLE'};
common={'ANALYSE','FRAMES','MORPH','CORNER','PLAY','STOP','FILTER ON','MEASURE','SAW','PINK NOISE','SAMPLE','LIVE IN', ...
    'USE AS M0 Q0','USE AS M1 Q0','USE AS M0 Q1','USE AS M1 Q1','UNITY DC','WRITE BODY FILE','1','2','3','4','5','6'};
names=fieldnames(expected);
allowed=[common {'BANK','NEAREST TO','M0 Q0','M1 Q0','M0 Q1','M1 Q1'} trench.bridge.groupNames];
for k=1:4, allowed=[allowed expected.(names{k})]; end
for k=1:4
    name=names{k}; app.setRoom(name); room=app.rooms.(name);
    tags=[getTags(room.controls.panel) getTags(room.display)];
    t.verifyTrue(all(ismember(expected.(name),tags)),strjoin(setdiff(expected.(name),tags),', '));
    t.verifyTrue(all(ismember(tags,allowed)),strjoin(setdiff(tags,allowed),', '));
    for j=1:4
        t.verifyEqual(char(app.rooms.(names{j}).display.Visible),trench.ui.draw.onOff(k==j));
        t.verifyEqual(char(app.rooms.(names{j}).controls.panel.Visible),trench.ui.draw.onOff(k==j));
    end
end
t.verifyEqual(char(app.figure.Visible),'off');
end
function tags=getTags(parent)
h=findall(parent,'Type','uicontrol'); tags=get(h,'Tag');
if ischar(tags), tags={tags}; end
tags=tags(:)'; tags=tags(~cellfun(@isempty,tags));
end
