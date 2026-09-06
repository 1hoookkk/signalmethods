function arrangeReferences(app)
horizontal={'center','left','right'}; vertical={'middle','top','bottom'};
rectangles=zeros(0,4);
for k=1:numel(app.referenceLabels)
    h=app.referenceLabels(k); score=Inf; chosen=[1 1];
    for x=1:3
        for y=1:3
            set(h,'HorizontalAlignment',horizontal{x},'VerticalAlignment',vertical{y});
            box=h.Extent; low=max(box(1:2),rectangles(:,1:2)); high=min(box(1:2)+box(3:4),rectangles(:,1:2)+rectangles(:,3:4));
            overlap=sum(prod(max(0,high-low),2));
            if overlap<score, score=overlap; chosen=[x y]; end
        end
    end
    set(h,'HorizontalAlignment',horizontal{chosen(1)},'VerticalAlignment',vertical{chosen(2)});
    rectangles(end+1,:)=h.Extent;
end
end
