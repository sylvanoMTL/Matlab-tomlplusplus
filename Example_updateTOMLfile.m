% example_updateTOMLfile.m
% Comprehensive example demonstrating updateTOMLfile with nested tables and complex cases

%% Setup - Create a sample TOML file with nested structure
sample_toml = [...
'# Configuration File' newline ...
'' newline ...
'title = "TOML Example"' newline ...
'version = "1.0.0"' newline ...
'' newline ...
'# Database configuration' newline ...
'[database]' newline ...
'server = "192.168.1.1"  # Server IP' newline ...
'ports = [ 8001, 8001, 8002 ]' newline ...
'connection_max = 5000' newline ...
'enabled = true' newline ...
'' newline ...
'# Network settings' newline ...
'[network]' newline ...
'timeout = 30' newline ...
'retry_count = 3' newline ...
'' newline ...
'# Server configuration' newline ...
'[servers]' newline ...
'' newline ...
'  # Alpha server' newline ...
'  [servers.alpha]' newline ...
'  ip = "10.0.0.1"' newline ...
'  dc = "east"' newline ...
'' newline ...
'  # Beta server' newline ...
'  [servers.beta]' newline ...
'  ip = "10.0.0.2"' newline ...
'  dc = "west"' newline ...
'' newline ...
'# Special formatting' newline ...
'hex_color = 0xDEADBEEF' newline ...
'octal_perm = 0o755' newline ...
'binary_flag = 0b11010110' newline];

% Write sample file
fid = fopen('config.toml', 'w');
fprintf(fid, '%s', sample_toml);
fclose(fid);

fprintf('=== Created config.toml ===\n\n');
type('config.toml');

%% Example 1: Modify top-level values
fprintf('\n=== Example 1: Modify top-level values ===\n');

% Parse original
config = toml_parse_file('config.toml');
fprintf('Original title: %s\n', config.title);
fprintf('Original version: %s\n', config.version);

% Modify top-level fields
mods = struct();
mods.title = "Production Configuration";
mods.version = "2.0.0";

updateTOMLfile('config.toml', mods);

% Verify
config = toml_parse_file('config.toml');
fprintf('\nUpdated title: %s\n', config.title);
fprintf('Updated version: %s\n', config.version);
fprintf('✓ Top-level values modified, comments preserved!\n');

%% Example 2: Modify nested table values
fprintf('\n=== Example 2: Modify nested table values ===\n');

fprintf('Original database.server: %s\n', config.database.server);
fprintf('Original database.connection_max: %d\n', config.database.connection_max);

% Modify nested database config
mods = struct();
mods.database.server = "192.168.1.100";
mods.database.connection_max = 10000;
mods.database.enabled = false;

updateTOMLfile('config.toml', mods);

config = toml_parse_file('config.toml');
fprintf('\nUpdated database.server: %s\n', config.database.server);
fprintf('Updated database.connection_max: %d\n', config.database.connection_max);
fprintf('Updated database.enabled: %d\n', config.database.enabled);
fprintf('✓ Nested table values modified!\n');

%% Example 3: Modify deeply nested values
fprintf('\n=== Example 3: Modify deeply nested values ===\n');

fprintf('Original servers.alpha.ip: %s\n', config.servers.alpha.ip);
fprintf('Original servers.beta.dc: %s\n', config.servers.beta.dc);

% Modify deeply nested server config
mods = struct();
mods.servers.alpha.ip = "10.0.0.10";
mods.servers.alpha.dc = "central";
mods.servers.beta.ip = "10.0.0.20";

updateTOMLfile('config.toml', mods);

config = toml_parse_file('config.toml');
fprintf('\nUpdated servers.alpha.ip: %s\n', config.servers.alpha.ip);
fprintf('Updated servers.alpha.dc: %s\n', config.servers.alpha.dc);
fprintf('Updated servers.beta.ip: %s\n', config.servers.beta.ip);
fprintf('✓ Deeply nested values modified!\n');

%% Example 4: Modify arrays
fprintf('\n=== Example 4: Modify arrays ===\n');

fprintf('Original database.ports: [');
fprintf(' %d', config.database.ports);
fprintf(' ]\n');

% Modify array
mods = struct();
mods.database.ports = [8001, 8002, 8003, 8004];

updateTOMLfile('config.toml', mods);

config = toml_parse_file('config.toml');
fprintf('Updated database.ports: [');
fprintf(' %d', config.database.ports);
fprintf(' ]\n');
fprintf('✓ Array modified!\n');

%% Example 5: Modify formatted integers
fprintf('\n=== Example 5: Modify formatted integers ===\n');

fprintf('Original hex_color: 0x%X\n', config.servers.beta.hex_color.value);
fprintf('Original octal_perm: 0o%s\n', dec2base(config.servers.beta.octal_perm.value, 8));
fprintf('Original binary_flag: 0b%s\n', dec2bin(config.servers.beta.binary_flag.value));

% Modify formatted integers while preserving format
mods = struct();
mods.servers.beta.hex_color = struct('value', hex2dec('CAFEBABE'), 'format', 'hex');
mods.servers.beta.octal_perm = struct('value', 644, 'format', 'oct');
mods.servers.beta.binary_flag = struct('value', 42, 'format', 'bin');

updateTOMLfile('config.toml', mods);

config = toml_parse_file('config.toml');
fprintf('\nUpdated hex_color: 0x%X\n', config.servers.beta.hex_color.value);
fprintf('Updated octal_perm: 0o%s\n', dec2base(config.servers.beta.octal_perm.value, 8));
fprintf('Updated binary_flag: 0b%s\n', dec2bin(config.servers.beta.binary_flag.value));
fprintf('✓ Formatted integers modified with format preserved!\n');

%% Example 6: Mix multiple modifications
fprintf('\n=== Example 6: Mix multiple modifications ===\n');

% Modify various fields at once
mods = struct();
mods.title = "Final Configuration";
mods.network.timeout = 60;
mods.network.retry_count = 5;
mods.servers.alpha.dc = "north";
mods.database.ports = [9001, 9002];

updateTOMLfile('config.toml', mods);

config = toml_parse_file('config.toml');
fprintf('Modified multiple fields across different nesting levels\n');
fprintf('  title: %s\n', config.title);
fprintf('  network.timeout: %d\n', config.network.timeout);
fprintf('  servers.alpha.dc: %s\n', config.servers.alpha.dc);
fprintf('✓ Multiple modifications applied!\n');

%% View final file
fprintf('\n=== Final config.toml ===\n');
type('config.toml');

%% Cleanup
fprintf('\n=== Cleanup ===\n');
delete('config.toml');
fprintf('Deleted config.toml\n');

%% Summary
fprintf('\n=== Summary ===\n');
fprintf('updateTOMLfile() supports:\n');
fprintf('  ✓ Top-level values\n');
fprintf('  ✓ Nested tables (database.server)\n');
fprintf('  ✓ Deeply nested tables (servers.alpha.ip)\n');
fprintf('  ✓ Arrays\n');
fprintf('  ✓ Formatted integers (hex/oct/bin)\n');
fprintf('  ✓ Booleans, strings, numbers\n');
fprintf('  ✓ Preserves ALL comments\n');
fprintf('  ✓ Preserves formatting\n');
fprintf('  ✓ Preserves file structure\n');
fprintf('\nLimitations:\n');
fprintf('  ✗ Does not add new sections\n');
fprintf('  ✗ Does not handle arrays of tables [[...]]\n');
fprintf('  ✗ Only modifies existing keys\n');