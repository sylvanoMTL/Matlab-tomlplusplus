config.name = 'MyApp';
config.version = 1.5;
config.debug = true;
config.ports = [8080, 8081, 8082];

config.database.host = 'localhost';
config.database.port = 5432;
config.database.credentials.user = 'admin';
config.database.credentials.password = 'secret';

% Convert to TOML string
toml_str = writeTOMLstring(config);
disp(toml_str)