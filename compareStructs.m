function compareStructs(S1, S2, parentField)
    if nargin < 3
        parentField = '';
    end

    % Get all unique field names from both structures
    fields = unique([fieldnames(S1); fieldnames(S2)]);

    for k = 1:length(fields)
        f = fields{k};
        fullField = f;
        if ~isempty(parentField)
            fullField = [parentField '.' f];
        end

        % Check if field exists in both
        hasS1 = isfield(S1, f);
        hasS2 = isfield(S2, f);

        if ~hasS1
            fprintf('Field %s missing in first structure\n', fullField);
            continue;
        end
        if ~hasS2
            fprintf('Field %s missing in second structure\n', fullField);
            continue;
        end

        val1 = S1.(f);
        val2 = S2.(f);

        % If both fields are structures, recurse
        if isstruct(val1) && isstruct(val2)
            compareStructs(val1, val2, fullField);
        else
            % Compare values
            if isequal(val1, val2)
                % Uncomment to print matches
                % fprintf('Field %s matches\n', fullField);
            else
                fprintf('Field %s differs\n', fullField);
                fprintf('  S1: %s\n', value2str(val1));
                fprintf('  S2: %s\n', value2str(val2));
            end
        end
    end
end

function str = value2str(val)
    % Converts almost any value to a string for display
    if isnumeric(val) || islogical(val)
        str = mat2str(val);
    elseif ischar(val) || isstring(val)
        str = ['''' char(val) ''''];
    elseif iscell(val)
        str = ['{' strjoin(cellfun(@value2str, val, 'UniformOutput', false), ', ') '}'];
    elseif isempty(val)
        str = '[]';
    else
        str = ['<' class(val) '>'];
    end
end
