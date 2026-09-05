function note = parseNote(value, root)
if nargin < 2, root = 48; end
if isnumeric(value), note=value; return; end
value = lower(strtrim(char(value)));
if endsWith(value,'hz'), note=trench.bridge.noteOf(str2double(erase(value,'hz'))); return; end
if endsWith(value,'st'), note=root+str2double(erase(value,'st')); return; end
if strcmp(value,'12th'), note=root+19; return; end
t = regexp(value,'^([a-g])([#b]?)(-?\d+)([+-]\d+(?:\.\d+)?)?$','tokens','once');
if isempty(t), note=str2double(value); else
    semis=[9 11 0 2 4 5 7]; n=semis(double(t{1})-double('a')+1);
    if strcmp(t{2},'#'), n=n+1; elseif strcmp(t{2},'b'), n=n-1; end
    note=(str2double(t{3})+1)*12+n;
    if numel(t)>3 && ~isempty(t{4}), note=note+str2double(t{4})/100; end
end
assert(isfinite(note),'trench:note:invalid','Invalid note.');
end
