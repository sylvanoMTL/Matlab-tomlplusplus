%% parse a TOML file
filename = fullfile(pwd,'examples\example.toml');

if isfile(filename)
    try
        parsed = parseTOMLfile(filename);
        disp(parsed)
    catch ME
        error('updateTOMLfile:parseError', 'Failed to parse TOML file: %s', ME.message);
    end
end

