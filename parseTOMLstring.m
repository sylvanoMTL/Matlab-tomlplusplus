function parsedStructure = parseTOMLstring(tomlstring)
    % PARSETOMLSTRING Parse TOML string with error handling
    %
    % Syntax:
    %   parsedStructure = parseTOMLstring(tomlstring)
    %
    % Description:
    %   Wrapper for toml_parse_string with robust error handling and validation
    %
    % Inputs:
    %   tomlstring - TOML content as string or char
    %
    % Outputs:
    %   parsedStructure - Parsed TOML data as MATLAB struct
    %                     Empty struct if parse error
    %
    % Example:
    %   toml_str = ['title = "Example"' newline ...
    %               'version = "1.0"' newline ...
    %               '[database]' newline ...
    %               'server = "localhost"'];
    %   
    %   config = parseTOMLstring(toml_str);
    %   if isempty(fieldnames(config))
    %       warning('Failed to parse TOML string');
    %   else
    %       disp(config.title);
    %   end
    
    % Initialize output
    parsedStructure = struct();
    
    % Validate input
    if nargin < 1
        error('parseTOMLstring:missingInput', 'Input TOML string is required');
    end
    
    % Convert to char if string
    if isstring(tomlstring)
        tomlstring = char(tomlstring);
    end
    
    % Check if input is valid type
    if ~ischar(tomlstring)
        error('parseTOMLstring:invalidInput', ...
              'Input must be a TOML string (string or char)');
    end
    
    % Check if string is empty
    if isempty(strtrim(tomlstring))
        warning('parseTOMLstring:emptyString', ...
                'Input TOML string is empty');
        return;
    end
    
    % Try to parse the string
    try
        parsedStructure = toml_parse_string(tomlstring);
        
    catch ME
        % Handle different types of errors
        if contains(ME.identifier, 'parseError')
            warning('parseTOMLstring:parseError', ...
                    'Failed to parse TOML string.\nError: %s', ME.message);
        elseif contains(ME.identifier, 'mexNotFound') || contains(ME.message, 'Undefined')
            error('parseTOMLstring:mexNotCompiled', ...
                    'toml_parse_string MEX function not found. Please compile it first:\n%s', ...
                    'mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_parse_string.cpp');
        else
            warning('parseTOMLstring:unknownError', ...
                    'Unexpected error parsing TOML string.\nError: %s', ME.message);
        end
        
        % Return empty struct on error
        parsedStructure = struct();

        % rethrow(ME)
    end
end