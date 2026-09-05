function openSound(room,path)
if nargin<2
    [file,folder]=uigetfile({'*.wav;*.aif;*.aiff;*.flac','sound'},'OPEN SOUND FILE');
    if isequal(file,0), return; end
    path=fullfile(folder,file);
end
room.sound=trench.io.openSound(path); room.liveInput=false;
room.cursor=0; room.region=[0 room.sound.duration]; room.envelopeDb=[];
trench.audio.clip(room.sound.mono,room.sound.fs);
trench.audio.region(room.region(1),room.region(2)); room.app.setSource('sample');
set(room.cursorSlider,'Max',max(.001,room.sound.duration),'Value',0);
set(room.outBox,'String',num2str(room.region(2)));
if ~ismember(path,room.paths)
    room.paths{end+1}=path; labels=cellfun(@fileName,room.paths,'UniformOutput',false);
    set(room.soundList,'String',[{'LIVE IN'};labels(:)]);
end
room.drawSound; room.readSlice;
end
function name=fileName(path), [~,name]=fileparts(path); end
