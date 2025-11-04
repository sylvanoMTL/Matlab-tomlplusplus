%% example expand path

clear, clc
% load toml file
preferenceFile = fullfile(pwd,'examples/path_macro_overwrite/preferences.toml')
preferences = parseTOMLfile(preferenceFile)

%% example static hardcoded
macros = struct('HOME', 'C:\Users\Tommy', 'PROJECT', 'MyProject');
path = '${HOME}\Documents\${PROJECT}\data';
expandedPath = expandPath(path, macros)

%% example pulling information from toml file
macros = struct('DATA_ROOT', preferences.macros.DATA_ROOT);     % , 'PROJECT', 'MyProject');
path = '${DATA_ROOT}\Documents\data';
expandedPath = expandPath(path, macros)


%% example changing the toml file
try 
    folderPath = uigetdir(pwd, 'Select a folder');
    if folderPath == 0
        disp('Folder selection cancelled.');
        
    else
        fprintf('Selected folder: %s\n', folderPath);
        mods.macros.DATA_ROOT= folderPath;
        updateTOMLfile(preferenceFile,mods)
    end
catch 

end

preferenceFile = fullfile(pwd,'examples/path_macro_overwrite/preferences.toml')
preferences = parseTOMLfile(preferenceFile)
macros = struct('DATA_ROOT', preferences.macros.DATA_ROOT); 
path = '${DATA_ROOT}/Documents/data';
expandedPath = expandPath(path, macros)