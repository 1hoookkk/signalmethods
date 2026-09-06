function path=bakeAnchors(root)
groups={'Klatt 1980','DVTD'}; records=struct([]);
for g=1:2
    bank=trench.io.openBank(fullfile(root,'native','workstation','banks',[groups{g} '.bank.json']));
    for k=1:numel(bank.frames)
        f=trench.model.conformShelf(bank.frames(k)); c=f.chord;
        frequency=440*2.^((c(1:5,2)-69)/12);
        bandwidth=frequency.*(2.^(c(1:5,3)/12)-1);
        if g==1
            frequency(4:5)=[3300;3750]; bandwidth(4:5)=[250;200];
            kind=repmat({'published_table_II'},1,5); kind(4:5)={'published_table_I'};
            upperSource='Klatt 1980 Table I typical values, F4 3300 Hz B4 250 Hz, F5 3750 Hz B5 200 Hz; F1 to F3 and B1 to B3 per vowel from Table II; evidence/mouths/klatt/Klatt-1980.pdf';
            for row=4:5
                note=69+12*log2(frequency(row)/440); width=12*log2(1+bandwidth(row)/frequency(row));
                c(row,:)=[1 note width 1 note min(120,16*width) 0];
            end
        else
            kind=repmat({'measured_response_peak'},1,5); upperSource=f.source;
        end
        assert(all(diff(frequency)>0) && all(bandwidth>0));
        provenance=struct('frequencyHz',frequency,'bandwidthHz',bandwidth,'frequencyKind',{kind}, ...
            'bandwidthKind',{kind},'upperSource',upperSource,'source',f.source, ...
            'conversion','note = 69 + 12 log2(F / 440); width = 12 log2(1 + B / F) semitones; zero on the pole note sixteen times wider, capped at 120', ...
            'shelfKind',pick(g==2,'measured_response_level','unity_no_low_band_data'));
        record=struct('name',f.name,'group',groups{g},'chord',c,'provenance',provenance);
        if isempty(records), records=record; else, records(end+1)=record; end
    end
end
assert(numel(records)==44);
path=fullfile(root,'native','workstation','data','headspace-anchors.json');
fid=fopen(path,'w','n','UTF-8'); assert(fid>=0); cleanup=onCleanup(@() fclose(fid));
fwrite(fid,jsonencode(struct('schema','headspace-anchors-v1','anchors',records),PrettyPrint=true),'char');
end
function value=pick(test,a,b)
value=b; if test, value=a; end
end
