function parsedStructure = parseTOMLfile(tomlfile)
    % PARSETOMLFILE Parse TOML file with error handling
    %
    % Syntax:
    %   parsedStructure = parseTOMLfile(tomlfile)
    %
    % Description:
    %   Wrapper for toml_parse_file with robust error handling and validation
    %
    % Inputs:
    %   tomlfile - Path to TOML file (string or char)
    %
    % Outputs:
    %   parsedStructure - Parsed TOML data as MATLAB struct
    %                     Empty struct if file not found or parse error
    %
    % Example:
    %   config = parseTOMLfile('config.toml');
    %   if isempty(fieldnames(config))
    %       warning('Failed to parse TOML file');
    %   else
    %       % Use config data
    %       disp(config.title);
    %   end
    
    % Initialize output
    parsedStructure = struct();
    
    % Validate input
    if nargin < 1
        error('parseTOMLfile:missingInput', 'Input file path is required');
    end
    
    % Convert to char if string
    if isstring(tomlfile)
        tomlfile = char(tomlfile);
    end
    
    % Check if input is valid type
    if ~ischar(tomlfile)
        error('parseTOMLfile:invalidInput', ...
              'Input must be a file path (string or char)');
    end
    
    % Check if file exists
    if ~isfile(tomlfile)
        warning('parseTOMLfile:fileNotFound', ...
                'TOML file not found: %s', tomlfile);
        return;
    end
    
    % Check file extension (optional warning)
    [~, ~, ext] = fileparts(tomlfile);
    if ~strcmpi(ext, '.toml')
        warning('parseTOMLfile:unexpectedExtension', ...
                'File does not have .toml extension: %s', tomlfile);
    end
    
    % Try to parse the file
    try
        parsedStructure = toml_parse_file(tomlfile);
        
    catch ME
        % Handle different types of errors
        if contains(ME.identifier, 'parseError')
            warning('parseTOMLfile:parseError', ...
                    'Failed to parse TOML file: %s\nError: %s', ...
                    tomlfile, ME.message);
        elseif contains(ME.identifier, 'mexNotFound') || contains(ME.message, 'Undefined')
            warning('parseTOMLfile:mexNotCompiled', ...
                    'toml_parse_file MEX function not found. Please compile it first:\n%s', ...
                    'mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_parse_file.cpp');
        else
            warning('parseTOMLfile:unknownError', ...
                    'Unexpected error parsing TOML file: %s\nError: %s', ...
                    tomlfile, ME.message);
        end
        
        % Return empty struct on error
        parsedStructure = struct();
    end
end