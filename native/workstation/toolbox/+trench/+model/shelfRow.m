function row = shelfRow(gainDb,cornerHz,rowGainDb)
if nargin<3, rowGainDb=0; end
gainDb=max(-18,min(18,gainDb)); cornerHz=max(30,min(4000,cornerHz));
pole=trench.bridge.noteOf(cornerHz); zero=trench.bridge.noteOf(cornerHz*10^(gainDb/40));
row=[1 pole 12 1 zero 12 rowGainDb];
end
