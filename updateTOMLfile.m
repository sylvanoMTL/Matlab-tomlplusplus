function updateTOMLfile(filename, modifications)
    % UPDATETOMLFILE Update values in TOML file while preserving comments
    %
    % Syntax:
    %   updateTOMLfile(filename, modifications)
    %
    % Description:
    %   Updates specific values in a TOML file while preserving all comments,
    %   formatting, and structure. Supports nested tables and complex structures.
    %
    % Inputs:
    %   filename      - Path to TOML file to modify
    %   modifications - Struct with fields to update using dot notation
    %                   For nested values: modifications.database.server = "new"
    %                   Or use setfield: setfield(mods, 'database', 'server', "new")
    %
    % Example:
    %   % Modify top-level and nested values
    %   mods = struct();
    %   mods.title = "Modified Example";
    %   mods.database.server = "192.168.1.100";
    %   mods.database.ports = [8001, 8002, 8003];
    %   
    %   updateTOMLfile('example.toml', mods);
    
    % Read original file preserving all comments and formatting
    fid = fopen(filename, 'r');
    if fid == -1
        error('updateTOMLfile:fileNotFound', 'Cannot open file: %s', filename);
    end
    
    lines = {};
    while ~feof(fid)
        line = fgetl(fid);
        if ischar(line)
            lines{end+1} = line;
        end
    end
    fclose(fid);
    
    % Parse to validate file and get structure
    try
        current = toml_parse_file(filename);
    catch ME
        error('updateTOMLfile:parseError', 'Failed to parse TOML file: %s', ME.message);
    end
    
    % Flatten modifications struct to dot-notation map
    mod_map = flattenStruct(modifications, '');
    
    % Track current section path
    section_stack = {};
    
    % Process each line
    i = 1;
    while i <= length(lines)
        line = lines{i};
        trimmed = strtrim(line);
        
        % Skip empty lines and comment-only lines
        if isempty(trimmed) || startsWith(trimmed, '#')
            i = i + 1;
            continue;
        end
        
        % Check for section headers [section] or [[array]]
        if startsWith(trimmed, '[[') && endsWith(trimmed, ']]')
            % Array of tables - not handled yet, skip
            section_name = extractBetween(trimmed, '[[', ']]');
            if ~isempty(section_name)
                section_stack = strsplit(char(section_name{1}), '.');
            end
            i = i + 1;
            continue;
        elseif startsWith(trimmed, '[') && endsWith(trimmed, ']')
            % Section header
            section_name = extractBetween(trimmed, '[', ']');
            if ~isempty(section_name)
                section_stack = strsplit(char(section_name{1}), '.');
            else
                section_stack = {};
            end
            i = i + 1;
            continue;
        end
        
        % Check if line contains a key = value
        eq_pos = strfind(line, '=');
        if ~isempty(eq_pos)
            % Extract key (before first =)
            key_part = line(1:eq_pos(1)-1);
            key = strtrim(key_part);
            
            % Build full key path
            if isempty(section_stack)
                full_key = key;
            else
                full_key = strjoin([section_stack, {key}], '.');
            end
            
            % Check if this key should be modified
            if isKey(mod_map, full_key)
                new_val = mod_map(full_key);
                
                % Check if it's a multi-line string (starts with """)
                value_part = strtrim(line(eq_pos(1)+1:end));
                is_multiline = startsWith(value_part, '"""');
                
                if is_multiline
                    % Handle multi-line string
                    [lines, i] = replaceMultilineString(lines, i, key, new_val);
                else
                    % Handle single-line value
                    lines{i} = replaceSingleLineValue(line, key, new_val, eq_pos(1));
                end
            end
        end
        
        i = i + 1;
    end
    
    % Write back to file
    fid = fopen(filename, 'w');
    if fid == -1
        error('updateTOMLfile:fileWriteError', 'Cannot write to file: %s', filename);
    end
    
    for i = 1:length(lines)
        fprintf(fid, '%s\n', lines{i});
    end
    fclose(fid);
end

function flat_map = flattenStruct(s, prefix)
    % Flatten nested struct to dot-notation map
    flat_map = containers.Map('KeyType', 'char', 'ValueType', 'any');
    
    if ~isstruct(s)
        return;
    end
    
    fields = fieldnames(s);
    for i = 1:length(fields)
        field = fields{i};
        value = s.(field);
        
        % Build key path
        if isempty(prefix)
            key_path = field;
        else
            key_path = [prefix '.' field];
        end
        
        % Recursively flatten nested structs (unless it's a special struct)
        if isstruct(value) && ~isSpecialStruct(value)
            sub_map = flattenStruct(value, key_path);
            keys_list = keys(sub_map);
            for j = 1:length(keys_list)
                flat_map(keys_list{j}) = sub_map(keys_list{j});
            end
        else
            flat_map(key_path) = value;
        end
    end
end

function is_special = isSpecialStruct(s)
    % Check if struct is a special type (formatted int, offset datetime)
    is_special = false;
    
    if ~isstruct(s)
        return;
    end
    
    fnames = fieldnames(s);
    
    % Check for formatted integer: {value, format}
    if length(fnames) == 2
        if (strcmp(fnames{1}, 'value') && strcmp(fnames{2}, 'format')) || ...
           (strcmp(fnames{1}, 'format') && strcmp(fnames{2}, 'value'))
            is_special = true;
            return;
        end
        
        % Check for offset datetime: {datetime, offset_minutes}
        if (strcmp(fnames{1}, 'datetime') && strcmp(fnames{2}, 'offset_minutes')) || ...
           (strcmp(fnames{1}, 'offset_minutes') && strcmp(fnames{2}, 'datetime'))
            is_special = true;
            return;
        end
    end
end

function new_line = replaceSingleLineValue(line, key, new_val, eq_pos)
    % Replace value in a single line while preserving comments and formatting
    
    % Find comment position (if any)
    comment_pos = strfind(line, '#');
    has_comment = false;
    comment = '';
    
    if ~isempty(comment_pos)
        % Check if # is after the =
        for cp = comment_pos
            if cp > eq_pos
                has_comment = true;
                comment = line(cp:end);
                break;
            end
        end
    end
    
    % Get leading whitespace
    leading_ws = '';
    if ~isempty(line)
        first_nonws = find(~isspace(line), 1);
        if ~isempty(first_nonws) && first_nonws > 1
            leading_ws = line(1:first_nonws-1);
        end
    end
    
    % Format new value
    new_value_str = formatTOMLvalue(new_val);
    
    % Reconstruct line
    new_line = [leading_ws key ' = ' new_value_str];
    if has_comment
        new_line = [new_line '  ' comment];
    end
end

function [lines, new_idx] = replaceMultilineString(lines, start_idx, key, new_val)
    % Replace multi-line string value
    
    % Find the end of the multi-line string (closing """)
    end_idx = start_idx;
    for j = start_idx+1:length(lines)
        if contains(lines{j}, '"""')
            end_idx = j;
            break;
        end
    end
    
    % Get leading whitespace from the key line
    line = lines{start_idx};
    leading_ws = '';
    if ~isempty(line)
        first_nonws = find(~isspace(line), 1);
        if ~isempty(first_nonws) && first_nonws > 1
            leading_ws = line(1:first_nonws-1);
        end
    end
    
    % Format new multi-line value
    if ischar(new_val) || isstring(new_val)
        val_str = char(new_val);
        
        % Check if it contains newlines
        if contains(val_str, newline) || contains(val_str, sprintf('\n'))
            % Create new multi-line string
            new_lines = {[leading_ws key ' = """']};
            
            % Split value by newlines
            val_lines = strsplit(val_str, '\n');
            for k = 1:length(val_lines)
                new_lines{end+1} = val_lines{k};
            end
            new_lines{end+1} = '"""';
            
            % Replace old lines with new lines
            lines = [lines(1:start_idx-1), new_lines, lines(end_idx+1:end)];
            new_idx = start_idx + length(new_lines) - 1;
        else
            % Convert to single-line
            lines{start_idx} = [leading_ws key ' = "' val_str '"'];
            % Remove old multi-line content
            lines = [lines(1:start_idx), lines(end_idx+1:end)];
            new_idx = start_idx;
        end
    else
        % Non-string value - convert to single line
        new_value_str = formatTOMLvalue(new_val);
        lines{start_idx} = [leading_ws key ' = ' new_value_str];
        % Remove old multi-line content
        lines = [lines(1:start_idx), lines(end_idx+1:end)];
        new_idx = start_idx;
    end
end

function str = formatTOMLvalue(val)
    % Format MATLAB value as TOML string
    
    % Check for datetime
    if isa(val, 'datetime')
        str = formatDatetime(val);
        return;
    end
    
    % Check for special structs
    if isstruct(val)
        % Formatted integer (hex, oct, bin)
        if isfield(val, 'value') && isfield(val, 'format')
            switch val.format
                case 'hex'
                    str = ['0x' upper(dec2hex(val.value))];
                case 'oct'
                    str = ['0o' dec2base(val.value, 8)];
                case 'bin'
                    str = ['0b' dec2bin(val.value)];
                otherwise
                    error('updateTOMLfile:unknownFormat', 'Unknown format: %s', val.format);
            end
            return;
        end
        
        % Offset datetime
        if isfield(val, 'datetime') && isfield(val, 'offset_minutes')
            str = formatOffsetDatetime(val.datetime, val.offset_minutes);
            return;
        end
        
        error('updateTOMLfile:unsupportedStruct', 'Unsupported struct type');
    end
    
    % String
    if ischar(val) || isstring(val)
        val_str = char(val);
        % Check if contains newlines - caller handles multi-line format
        str = ['"' escapeString(val_str) '"'];
        return;
    end
    
    % Boolean
    if islogical(val)
        if val
            str = 'true';
        else
            str = 'false';
        end
        return;
    end
    
    % Array
    if isnumeric(val) && numel(val) > 1
        str = formatArray(val);
        return;
    end
    
    % Single numeric value
    if isnumeric(val)
        if isinteger(val) || (val == floor(val) && abs(val) < 1e15)
            str = sprintf('%d', val);
        else
            % Format with appropriate precision
            str = sprintf('%.15g', val);
            % Clean up unnecessary precision
            if contains(str, 'e')
                % Scientific notation
                parts = strsplit(str, 'e');
                mantissa = str2double(parts{1});
                mantissa_str = sprintf('%.11g', mantissa);
                str = [mantissa_str 'e' parts{2}];
            end
        end
        return;
    end
    
    error('updateTOMLfile:unsupportedType', 'Unsupported value type: %s', class(val));
end

function str = formatArray(arr)
    % Format numeric array as TOML array
    str = '[ ';
    for i = 1:length(arr)
        if i > 1
            str = [str ', '];
        end
        
        val = arr(i);
        if isinteger(val) || (val == floor(val) && abs(val) < 1e15)
            str = [str sprintf('%d', val)];
        else
            str = [str sprintf('%.15g', val)];
        end
    end
    str = [str ' ]'];
end

function str = escapeString(s)
    % Escape special characters for TOML double-quoted strings
    % Note: Does NOT handle newlines (caller should use multi-line format)
    str = s;
    str = strrep(str, '\', '\\');  % Backslash
    str = strrep(str, '"', '\"');  % Quote
    str = strrep(str, sprintf('\t'), '\t');  % Tab
end

function str = formatDatetime(dt)
    % Format datetime as TOML datetime string
    y = year(dt);
    m = month(dt);
    d = day(dt);
    h = hour(dt);
    min = minute(dt);
    s = second(dt);
    
    % Check if it's date-only (time is midnight)
    if h == 0 && min == 0 && s == 0
        str = sprintf('%04d-%02d-%02d', y, m, d);
        return;
    end
    
    % Check if it's time-only (date is 1970-01-01)
    if y == 1970 && m == 1 && d == 1
        if s == floor(s)
            str = sprintf('%02d:%02d:%02d', h, min, floor(s));
        else
            str = sprintf('%02d:%02d:%09.6f', h, min, s);
        end
        return;
    end
    
    % Full datetime
    if s == floor(s)
        str = sprintf('%04d-%02d-%02dT%02d:%02d:%02d', y, m, d, h, min, floor(s));
    else
        str = sprintf('%04d-%02d-%02dT%02d:%02d:%09.6f', y, m, d, h, min, s);
    end
end

function str = formatOffsetDatetime(dt, offset_minutes)
    % Format offset datetime
    y = year(dt);
    m = month(dt);
    d = day(dt);
    h = hour(dt);
    min = minute(dt);
    s = second(dt);
    
    % Format datetime part
    if s == floor(s)
        dt_str = sprintf('%04d-%02d-%02dT%02d:%02d:%02d', y, m, d, h, min, floor(s));
    else
        dt_str = sprintf('%04d-%02d-%02dT%02d:%02d:%09.6f', y, m, d, h, min, s);
    end
    
    % Format offset
    if offset_minutes == 0
        offset_str = 'Z';
    else
        offset_hours = floor(offset_minutes / 60);
        offset_mins = abs(mod(offset_minutes, 60));
        offset_str = sprintf('%+03d:%02d', offset_hours, offset_mins);
    end
    
    str = [dt_str offset_str];
end