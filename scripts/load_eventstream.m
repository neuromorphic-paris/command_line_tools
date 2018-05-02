function event_data = load_eventstream(filename)
% td_data = load_eventstream(filename)
%
% loads ES files that have been recorded with version 1 of the ES format.
% See https://github.com/neuromorphic-paris/event_stream/blob/7ffcca590faa002b48e57f8e75810ceb200888a4/README.md

f=fopen(filename);

header = fgets(f, 12);
versions = fread(f, 2);
type = fread(f, 1);

if type ~= 1
    disp 'unsupported version'
    fclose(f);
    return;
end

data = fread(f, 'uint8');

arraySize = round(size(data)/3) + 1; %if all bytes were event bytes and no overflow or reset bytes
events = NaN(1,arraySize(1));
index = 1;
tsmask = bin2dec('00011111');
xmask = bin2dec('00100000');
pmask = bin2dec('10000000');

overflow = 0;
skiploop = 0;
x = 0;
y = 0;
t = 0;

for i = 1:length(data)
    b = bitshift(data(i), 3, 'uint8');
    
    if skiploop > 0
        skiploop = skiploop - 1;
        continue;
    end
    
    if b == 248
        helperbits = bitshift(data(i), -5);
        if helperbits == 0 %reset byte
            continue;
        else
            overflow = overflow + helperbits;
        end
    else 

        t = t + bitand(data(i), tsmask) + overflow * 31;
        event_data.ts(index) = t;
        overflow = 0;
                
        x =  bitor(bitshift(data(i), -5), bitshift(data(i+1), 3, 'uint8'));
        if bitand(data(i+1), xmask) == xmask %account for x[8]
            x = x + 256;
        end
        event_data.x(index) = x;
        
        y = bitor(bitshift(data(i+1), -6), bitshift(data(i+2), 2, 'uint8'));
        event_data.y(index) = y;
          
        if bitand(data(i+2), pmask) == pmask
            p = 1;
        else
            p = 0;
        end
        event_data.p(index) = p;
        
        index = index + 1;
        skiploop = 2;
    end
end

fclose(f);

end