function build_toml_mex()
    %% Cross-platform MEX compilation for toml_parse_string.cpp
    clear all;clc;
    % Base path (current folder)
    basePath = pwd;

    % Include path for tomlplusplus
    incPath = fullfile(basePath, 'tomlplusplus', 'include');

    % Determine platform-specific compiler flags
    switch computer
        case {'PCWIN', 'PCWIN64'}
            % Windows with MSVC
            cppFlag = '/std:c++17 /EHsc';  % /EHsc for exception handling
            mexFlagName = 'COMPFLAGS';
        case {'MACI64', 'GLNXA64', 'GLNX86'}
            % macOS or Linux with GCC/Clang
            cppFlag = '-std=c++17';
            mexFlagName = 'CXXFLAGS';
        otherwise
            error('Unsupported platform: %s', computer);
    end
    
    %% Compile MEX files
     %% parse string
    mex('toml_parse_string.cpp', ...
        ['-I"' incPath '"'], ...
        [mexFlagName '="$' mexFlagName ' ' cppFlag '"']);
    %% write string
    mex('toml_write_string.cpp', ...
        ['-I"' incPath '"'], ...
        [mexFlagName '="$' mexFlagName ' ' cppFlag '"']);

    %% parse file
    mex('toml_parse_file.cpp', ...
        ['-I"' incPath '"'], ...
        [mexFlagName '="$' mexFlagName ' ' cppFlag '"']);
    
    disp('Compilation finished successfully!');
end
