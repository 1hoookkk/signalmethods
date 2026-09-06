function saveCorners(app)
bank=trench.model.newBank('HEADSPACE');
for k=1:4
    if app.corners(k)>0, bank=trench.model.putInBank(bank,app.frames(app.corners(k)),k); end
end
trench.io.saveBank(bank,app.bankPath);
end
