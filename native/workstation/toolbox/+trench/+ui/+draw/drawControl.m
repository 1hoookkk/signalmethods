function drawControl(ax,h)
p=getpixelposition(h,true); style=h.Style; bg=h.BackgroundColor; fg=h.ForegroundColor;
if ~strcmp(style,'text'), rectangle(ax,'Position',p,'FaceColor',bg,'EdgeColor',[110 110 110]/255,'LineWidth',.5); end
if strcmp(style,'slider')
    value=(h.Value-h.Min)/(h.Max-h.Min);
    rectangle(ax,'Position',[p(1)+value*(p(3)-12) p(2) 12 p(4)],'FaceColor',[.69 .69 .69],'EdgeColor',[.43 .43 .43]);
    return
end
items=h.String;
if strcmp(style,'listbox')
    if ischar(items), items=cellstr(items); end
    height=h.FontSize*96/72+3; count=floor(p(4)/height); first=h.ListboxTop;
    for k=first:min(numel(items),first+count-1)
        y=p(2)+p(4)-(k-first+1)*height;
        if ismember(k,h.Value), rectangle(ax,'Position',[p(1) y p(3) height],'FaceColor',[.89 .89 .89],'EdgeColor','none'); end
        placeText(ax,h,items{k},[p(1)+3 y p(3)-6 height],fg,'left');
    end
    return
end
if strcmp(style,'popupmenu')
    if iscell(items), items=items{h.Value}; else, items=items(h.Value,:); end
end
align=h.HorizontalAlignment;
if ismember(style,{'pushbutton','togglebutton'}), align='center'; end
placeText(ax,h,items,p,fg,align);
end
function placeText(ax,h,value,p,color,align)
x=p(1)+3; if strcmp(align,'center'), x=p(1)+p(3)/2; elseif strcmp(align,'right'), x=p(1)+p(3)-3; end
if iscell(value), value=strjoin(value,newline); end
text(ax,x,p(2)+p(4)/2,value,'FontName',h.FontName,'FontSize',h.FontSize, ...
    'Color',color,'HorizontalAlignment',align,'VerticalAlignment','middle','Interpreter','none','Clipping','on');
end
