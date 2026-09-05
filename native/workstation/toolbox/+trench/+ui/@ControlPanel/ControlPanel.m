classdef ControlPanel < handle
properties
    panel
    keys = struct
end
methods
    function obj=ControlPanel(parent,position)
        obj.panel=uipanel(parent,'Units','pixels','Position',position,'BackgroundColor','w','BorderType','line','HighlightColor',[.43 .43 .43]);
    end
    function h=key(obj,name,position,callback,toggle)
        if nargin<5, toggle=false; end
        style='pushbutton'; if toggle, style='togglebutton'; end
        h=uicontrol(obj.panel,'Style',style,'String',name,'Position',position, ...
            'FontName','Arial','FontSize',9,'BackgroundColor',[.886 .886 .886], ...
            'ForegroundColor','k','Callback',callback,'Tag',name);
        obj.keys.(matlab.lang.makeValidName(name))=h;
    end
    function h=label(obj,name,position)
        h=uicontrol(obj.panel,'Style','text','String',name,'Position',position, ...
            'FontName','Arial','FontSize',9,'BackgroundColor','w','HorizontalAlignment','left');
    end
    function h=number(obj,name,value,position,callback)
        if ~isempty(name), obj.label(name,[position(1) position(2)+position(4)+3 position(3) 16]); end
        h=uicontrol(obj.panel,'Style','edit','String',num2str(value),'Position',position, ...
            'FontName','Arial','FontSize',9,'BackgroundColor','w','Tag',name,'Callback',callback);
        obj.keys.(matlab.lang.makeValidName(name))=h;
    end
    function h=slider(obj,name,range,value,position,callback)
        obj.label(name,[position(1) position(2)+20 position(3) 16]);
        h=uicontrol(obj.panel,'Style','slider','Min',range(1),'Max',range(2),'Value',value, ...
            'Position',position,'BackgroundColor',[.886 .886 .886],'Callback',callback,'Tag',name);
        obj.keys.(matlab.lang.makeValidName(name))=h;
    end
    function h=popup(obj,name,items,position,callback)
        obj.label(name,[position(1) position(2)+23 position(3) 16]);
        h=uicontrol(obj.panel,'Style','popupmenu','String',items,'Position',position, ...
            'FontName','Arial','FontSize',9,'BackgroundColor','w','Callback',callback,'Tag',name);
        obj.keys.(matlab.lang.makeValidName(name))=h;
    end
end
end
