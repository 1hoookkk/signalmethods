function setFormant(app,row,hz)
if ~isscalar(hz) || ~isfinite(hz) || hz<=0, app.refresh; return; end
point=app.position; point(3-row)=trench.bridge.noteOf(hz); app.setPosition(point);
end
