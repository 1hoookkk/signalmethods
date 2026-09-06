function path=bakeAnchors(root)
groups={'Klatt 1980','Hillenbrand 1995','DVTD'}; records=struct([]);
data=readtable(fullfile(root,'evidence','mouths','hillenbrand','hillenbrand-vowel-formatted.csv'),TextType='string');
for g=1:3
    bank=trench.io.openBank(fullfile(root,'native','workstation','banks',[groups{g} '.bank.json']));
    if g==3, bank.frames=bank.frames(endsWith({bank.frames.name},' s1')); end
    for k=1:numel(bank.frames)
        f=trench.model.conformShelf(bank.frames(k)); c=f.chord;
        frequency=440*2.^((c(1:5,2)-69)/12);
        bandwidth=frequency.*(2.^(c(1:5,3)/12)-1);
        kind=repmat({'measured'},1,5); widthKind=kind; counts=zeros(1,5);
        if g==1
            frequency(4:5)=[3300;3750]; bandwidth(4:5)=[250;200];
            kind=repmat({'published_target'},1,5); kind(4:5)={'published_default'}; widthKind=kind;
            upperSource='Klatt 1980 Table I typical values, F4 3300 Hz B4 250 Hz, F5 3750 Hz B5 200 Hz, not vowel-specific measurements; evidence/mouths/klatt/Klatt-1980.pdf';
        elseif g==2
            parts=split(string(f.name)); group=extractBetween(parts(3),1,1);
            rows=startsWith(data.ID,group) & endsWith(data.ID,parts(2));
            values=data{rows,{'F1','F2','F3','F4'}}; values(values<=0)=NaN;
            means=mean(values,1,'omitnan'); counts(1:4)=sum(isfinite(values),1);
            frequency(4)=means(4);
            spacing=median(diff(frequency(1:4)));
            frequency(5)=frequency(4)+spacing;
            bandwidth(4:5)=bandwidth(3)*frequency(4:5)/frequency(3);
            kind(5)={'estimated'}; widthKind(1:3)={'published_proxy'}; widthKind(4:5)={'estimated'};
            upperSource='Hillenbrand 1995 F4: positive-value group/vowel mean from evidence/mouths/hillenbrand/hillenbrand-vowel-formatted.csv; F5=F4+median(diff(F1:F4)); B4/B5=B3*F4/F3,B3*F5/F3';
        else
            upperSource=f.source;
            kind(:)={'measured_response_peak'}; widthKind=kind;
        end
        if g<3
            for row=4:5
                note=69+12*log2(frequency(row)/440); width=12*log2(1+bandwidth(row)/frequency(row));
                c(row,:)=[1 note width 1 note min(120,16*width) 0];
            end
        end
        assert(all(diff(frequency)>0) && all(bandwidth>0));
        provenance=struct('frequencyHz',frequency,'bandwidthHz',bandwidth,'frequencyKind',{kind}, ...
            'bandwidthKind',{widthKind},'measurementCounts',counts,'upperSource',upperSource,'source',f.source, ...
            'zeroWidthRule','Existing source zeros retained; new zeros sixteen times pole width, capped at 120 semitones', ...
            'shelfKind',pick(g==3,'measured_response_level','unity_no_low_band_data'));
        record=struct('name',f.name,'group',groups{g},'chord',c,'provenance',provenance);
        if isempty(records), records=record; else, records(end+1)=record; end
    end
end
assert(numel(records)==76);
path=fullfile(root,'native','workstation','data','headspace-anchors.json');
fid=fopen(path,'w','n','UTF-8'); assert(fid>=0); cleanup=onCleanup(@() fclose(fid));
fwrite(fid,jsonencode(struct('schema','headspace-anchors-v1','anchors',records),PrettyPrint=true),'char');
end
function value=pick(test,a,b)
value=b; if test, value=a; end
end
