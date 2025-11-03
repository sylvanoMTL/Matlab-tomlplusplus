function success = writeTOMLfile(tomlfile, data)
    % WRITETOMLFILE Write MATLAB struct to TOML file with error handling
    %
    % Syntax:
    %   success = writeTOMLfile(tomlfile, data)
    %
    % Description:
    %   Wrapper for toml_write_string with robust error handling and validation
    %
    % Inputs:
    %   tomlfile - Output file path (string or char)
    %   data     - MATLAB struct to write as TOML
    %
    % Outputs:
    %   success  - True if write succeeded, false otherwise
    %
    % Example:
    %   config = struct();
    %   config.title = "My Config";
    %   config.database.server = "localhost";
    %   config.database.port = 5432;
    %   
    %   if writeTOMLfile('config.toml', config)
    %       fprintf('Successfully wrote config.toml\n');
    %   else
    %       fprintf('Failed to write config.toml\n');
    %   end
    
    % Initialize output
    success = false;
    
    % Validate inputs
    if nargin < 2
        error('writeTOMLfile:missingInput', ...
              'Two inputs required: writeTOMLfile(filename, data)');
    end
    
    % Convert filename to char if string
    if isstring(tomlfile)
        tomlfile = char(tomlfile);
    end
    
    % Check if filename is valid type
    if ~ischar(tomlfile)
        error('writeTOMLfile:invalidFilename', ...
              'Filename must be a string or char');
    end
    
    % Check if data is a struct
    if ~isstruct(data)
        error('writeTOMLfile:invalidData', ...
              'Data must be a MATLAB struct');
    end
    
    % Check if data is empty
    if isempty(fieldnames(data))
        warning('writeTOMLfile:emptyData', ...
                'Data struct is empty, writing empty file');
    end
    
    % Try to serialize and write
    try
        % Convert struct to TOML string
        toml_string = toml_write_string(data);
        
        % Write to file
        fid = fopen(tomlfile, 'w');
        if fid == -1
            error('writeTOMLfile:cannotOpenFile', ...
                  'Cannot open file for writing: %s', tomlfile);
        end
        
        fprintf(fid, '%s', toml_string);
        fclose(fid);
        
        success = true;
        
    catch ME
        % Handle different types of errors
        if contains(ME.identifier, 'cannotOpenFile')
            warning('writeTOMLfile:fileError', ...
                    'Cannot write to file: %s\nError: %s', ...
                    tomlfile, ME.message);
        elseif contains(ME.identifier, 'mexNotFound') || contains(ME.message, 'Undefined')
            warning('writeTOMLfile:mexNotCompiled', ...
                    'toml_write_string MEX function not found. Please compile it first:\n%s', ...
                    'mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_write_string.cpp');
        else
            warning('writeTOMLfile:unknownError', ...
                    'Unexpected error writing TOML file: %s\nError: %s', ...
                    tomlfile, ME.message);
        end
        
        success = false;
    end
end