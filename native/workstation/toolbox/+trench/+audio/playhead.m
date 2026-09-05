function varargout = playhead(varargin)
for k = 1:numel(varargin)
    if isstring(varargin{k}), varargin{k} = char(varargin{k}); end
end
[varargout{1:nargout}] = trench_audio('playhead', varargin{:});
end
