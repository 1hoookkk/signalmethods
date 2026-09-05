function c = bodyCorners(app)
if isempty(app.prepared)
    app.prepared=trench.bridge.bodyCorners(app.rawCorners,app.rowOn,app.unity);
end
c=app.prepared;
end
