function [slot,status] = putFrame(app,frame)
[app.bank,slot,status]=trench.model.putInBank(app.bank,frame);
if slot>0
    app.chosen=slot; app.chosenLibrary=0; app.rooms.frames.refresh;
else
    frame.group='INSTRUMENTS'; app.library(end+1)=frame;
end
app.setStatus(status);
end
