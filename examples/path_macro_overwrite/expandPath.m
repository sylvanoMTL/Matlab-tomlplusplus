function expanded_path = expandPath(path_with_macros, macros)
    % Determine the correct file separator for the current OS
    fs = filesep;  % '\' on Windows, '/' on macOS/Linux

    % Ensure path_with_macros is a scalar string
    path_with_macros = string(path_with_macros);

    % Expand macros
    expanded_path = path_with_macros;
    keys = fieldnames(macros);

    for i = 1:numel(keys)
        key = keys{i};
        macro = ['${' key '}'];
        value = string(macros.(key)); % Ensure it's a string

        % Normalize slashes in macro value to the system's file separator
        value = strrep(value, '/', fs);
        value = strrep(value, '\', fs);

        % Remove trailing file separator to avoid double separators
        if strlength(value) > 0 && endsWith(value, fs)
            value = extractBefore(value, strlength(value)); % remove last char
        end

        expanded_path = strrep(expanded_path, macro, value);
    end

    % Normalize all slashes in the final path to the system's separator
    expanded_path = strrep(expanded_path, '/', fs);
    expanded_path = strrep(expanded_path, '\', fs);

    % Handle multiple consecutive separators
    expanded_path_char = char(expanded_path); % convert to char for regexprep
    if ispc && startsWith(expanded_path_char, '\\')
        % UNC path: keep leading \\ and normalize the rest
        rest = expanded_path_char(3:end);
        rest = regexprep(rest, [repmat('\\',1,2) '+'], fs); % match 2+ backslashes
        expanded_path_char = ['\\' rest];
    else
        % Normal path
        expanded_path_char = regexprep(expanded_path_char, [repmat('\\',1,2) '+'], fs);
    end

    % Convert back to string
    expanded_path = string(expanded_path_char);
end
