% test_toml
clear all;clc
%% parse a TOML file
toml_file = fullfile(pwd,'tomlplusplus/examples/example.toml');

if isfile(toml_file)
    parsed_file_as_struct = toml_parse_file(toml_file);
end

%% parse a TOML string
string_to_parse =  sprintf( strcat( '[library]\n',...
                                'name = "toml++"\n',...
                                'authors = ["Mark Gillard <mark.gillard@outlook.com.au>"]\n',...
                                'cpp = 17\n'));

parsed_toml_string = toml_parse_string(string_to_parse);



%% write a TOML string
% Create a MATLAB struct
clear all,clc
config.name = 'MyApp';
config.version = 1.5;
config.debug = true;
config.ports = [8080, 8081, 8082];

config.database.host = 'localhost';
config.database.port = 5432;
config.database.credentials.user = 'admin';
config.database.credentials.password = 'secret';

% Convert to TOML string
toml_str = toml_write_string(config);
disp(toml_str);


% parse the toml string back to a structure
mystruct = toml_parse_string(toml_str)

% convert back to toml
toml_str2 = toml_write_string(mystruct)

% if the 2 structures are the same
if strcmp(toml_str,toml_str2)
    disp("the 2 structures are the same")
else
    error("different structures")
end



%% write toml file
% Define your structure
data.name = 'Toto';
data.age = 25;
data.server.host = 'localhost';
data.server.ports = [8080, 8081, 8082];

% Generate TOML string
toml_str = toml_write_string(data);

% Write it to a file
fid = fopen('config.toml', 'w');
fprintf(fid, '%s', toml_str);
fclose(fid);

%% second test
% retrieve a file ad parse it
clear all; clc
toml_file = fullfile(pwd,'tomlplusplus/examples/example.toml');
if isfile(toml_file)
    pfas = toml_parse_file(toml_file); % parsed file as structure

    % reserialise
    serialised_pfas = toml_write_string(pfas);
    % write a new file
    fid = fopen('newfile.toml', 'w');
    fprintf(fid, '%s', serialised_pfas);
    fclose(fid);


    pfas2 = toml_parse_file('newfile.toml');

    isequaln(pfas,pfas2)

end
%
compareStructs(pfas,pfas2)


