function toml_string = writeTOMLstring(data)
    % WRITETOMLSTRING Write MATLAB struct to TOML string with error handling
    %
    % Syntax:
    %   success = writeTOMLstring(data)
    %
    % Description:
    %   Wrapper for toml_write_string with robust error handling and validation
    %
    % Inputs:
    %   data     - MATLAB struct to write as TOML
    %
    % Outputs:
    %   success  - True if write succeeded, false otherwise
    %   toml_string - TOML string
    %
    % Example:
    %   config = struct();
    %   config.title = "My Config";
    %   config.database.server = "localhost";
    %   config.database.port = 5432;
    %   
    %   if writeTOMLstring(config)
    %       fprintf('Successfully wrote toml string\n');
    %   else
    %       fprintf('Failed to write toml string\n');
    %   end
    
    % Initialize output
    success = false;
    
    nargin
    % Validate inputs
    if nargin ~= 1
        error('writeTOMLstrong:missingInput', ...
              'One input required: writeTOMLstring(data)');
    end
    

    
    % Check if data is a struct
    if ~isstruct(data)
        error('writeTOMLstring:invalidData', ...
              'Data must be a MATLAB struct');
    end
    
    % Check if data is empty
    if isempty(fieldnames(data))
        warning('writeTOMLstring:emptyData', ...
                'Data struct is empty, writing empty string');
    end
    
    % Try to serialize and write
    try
        % Convert struct to TOML string
        toml_string = toml_write_string(data);

        success = true;
        
    catch ME
        % Handle different types of errors
        
        if contains(ME.identifier, 'mexNotFound') || contains(ME.message, 'Undefined')
            warning('writeTOMLstring:mexNotCompiled', ...
                    'toml_write_string MEX function not found. Please compile it first:\n%s', ...
                    'mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_write_string.cpp');
        else
            warning('writeTOMLstring:unknownError', ...
                    'Unexpected error writing TOML string: \nError: %s', ...
                     ME.message);
        end
        
        success = false;
    end
end