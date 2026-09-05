function sound = openSound(path)
[x,fs]=audioread(path); x=mean(x,2);
[~,name]=fileparts(path);
sound=struct('path',char(path),'name',name,'mono',x,'fs',fs,'duration',numel(x)/fs);
end
