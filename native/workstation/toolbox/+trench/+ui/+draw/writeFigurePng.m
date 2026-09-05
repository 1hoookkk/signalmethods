function writeFigurePng(fig,path)
assert(strcmp(fig.Visible,'off'),'trench:shot:visible','Shot figure must stay invisible.');
sizePixels=fig.Position(3:4);
page=figure('Visible','off','Color',fig.Color,'Renderer','opengl','Theme','light', ...
    'Position',[1 1 sizePixels],'MenuBar','none','ToolBar','none','InvertHardcopy','off');
cleanup=onCleanup(@() delete(page));
paper=axes(page,'Units','pixels','Position',[0 0 sizePixels],'XLim',[0 sizePixels(1)], ...
    'YLim',[0 sizePixels(2)],'Visible','off','NextPlot','add','PositionConstraint','innerposition');
panels=findall(fig,'Type','uipanel');
for k=numel(panels):-1:1
    if ~visible(panels(k)), continue; end
    rectangle(paper,'Position',getpixelposition(panels(k),true),'FaceColor',panels(k).BackgroundColor, ...
        'EdgeColor',[.43 .43 .43],'LineWidth',.5);
end
controls=findall(fig,'Type','uicontrol');
for k=numel(controls):-1:1
    if visible(controls(k)), trench.ui.draw.drawControl(paper,controls(k)); end
end
plots=findall(fig,'Type','axes');
for k=1:numel(plots)
    if ~visible(plots(k).Parent), continue; end
    ax=copyobj(plots(k),page); ax.Units='pixels'; ax.Position=getpixelposition(plots(k),true);
    ax.PositionConstraint='innerposition';
end
set(page,'PaperPositionMode','auto'); print(page,path,'-dpng','-r192');
end
function on=visible(object)
on=true;
while ~isgraphics(object,'figure')
    if isprop(object,'Visible') && strcmp(object.Visible,'off'), on=false; return; end
    object=object.Parent;
end
end
