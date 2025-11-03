function expanded_path = expandPath(path_with_macros, macros)
    expanded_path = path_with_macros;
    keys = fieldnames(macros);

    for i = 1:numel(keys)
        key = keys{i};
        macro = ['${' key '}'];
        value = string(macros.(key)); % Ensure it's a string
        expanded_path = strrep(expanded_path, macro, value);
    end

    % Replace double backslashes with single ones, but keep UNC paths (\\server\share)
    expanded_path = regexprep(expanded_path, '(?<!^)\\\\', '\');
end
