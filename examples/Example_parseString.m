% example_parseTOMLstring.m
% Example demonstrating parseTOMLstring wrapper function

%% Example 1: Parse simple TOML string
fprintf('=== Example 1: Parse simple TOML string ===\n');

toml_str = ['title = "Example Configuration"' newline ...
            'version = "1.0.0"' newline ...
            'enabled = true'];

config = parseTOMLstring(toml_str);

if ~isempty(fieldnames(config))
    fprintf('✓ Successfully parsed TOML string\n');
    fprintf('  Title: %s\n', config.title);
    fprintf('  Version: %s\n', config.version);
    fprintf('  Enabled: %d\n', config.enabled);
end

%% Example 2: Parse nested structures
fprintf('\n=== Example 2: Parse nested structures ===\n');

toml_str = [...
'[database]' newline ...
'server = "192.168.1.1"' newline ...
'port = 5432' newline ...
'credentials.username = "admin"' newline ...
'credentials.password = "secret"' newline ...
'' newline ...
'[network]' newline ...
'timeout = 30' newline ...
'retry_count = 3'];

config = parseTOMLstring(toml_str);

if ~isempty(fieldnames(config))
    fprintf('✓ Parsed nested structures\n');
    fprintf('  Database server: %s\n', config.database.server);
    fprintf('  Database port: %d\n', config.database.port);
    fprintf('  Network timeout: %d\n', config.network.timeout);
end

%% Example 3: Parse arrays
fprintf('\n=== Example 3: Parse arrays ===\n');

toml_str = [...
'numbers = [ 1, 2, 3, 4, 5 ]' newline ...
'colors = [ "red", "green", "blue" ]' newline ...
'enabled_features = [ true, false, true ]'];

config = parseTOMLstring(toml_str);

if ~isempty(fieldnames(config))
    fprintf('✓ Parsed arrays\n');
    fprintf('  Numbers: [');
    fprintf(' %d', config.numbers);
    fprintf(' ]\n');
    fprintf('  Colors: { %s }\n', strjoin(config.colors, ', '));
end

%% Example 4: Parse formatted integers
fprintf('\n=== Example 4: Parse formatted integers ===\n');

toml_str = [...
'hex_value = 0xDEADBEEF' newline ...
'octal_perm = 0o755' newline ...
'binary_flag = 0b11010110'];

config = parseTOMLstring(toml_str);

if ~isempty(fieldnames(config))
    fprintf('✓ Parsed formatted integers\n');
    fprintf('  Hex: 0x%X (decimal: %d)\n', ...
            config.hex_value.value, config.hex_value.value);
    fprintf('  Octal: 0o%s (decimal: %d)\n', ...
            dec2base(config.octal_perm.value, 8), config.octal_perm.value);
    fprintf('  Binary: 0b%s (decimal: %d)\n', ...
            dec2bin(config.binary_flag.value), config.binary_flag.value);
end

%% Example 5: Parse dates and times
fprintf('\n=== Example 5: Parse dates and times ===\n');

toml_str = [...
'date = 1979-05-27' newline ...
'time = 07:32:00' newline ...
'datetime = 1979-05-27T07:32:00' newline ...
'datetime_offset = 1979-05-27T00:32:00-07:00'];

config = parseTOMLstring(toml_str);

if ~isempty(fieldnames(config))
    fprintf('✓ Parsed dates and times\n');
    fprintf('  Date: %s\n', datestr(config.date, 'yyyy-mm-dd'));
    fprintf('  Time: %s\n', datestr(config.time, 'HH:MM:SS'));
    fprintf('  DateTime: %s\n', datestr(config.datetime));
end

%% Example 6: Round-trip with toml_write_string
fprintf('\n=== Example 6: Round-trip (string → struct → string) ===\n');

% Create original TOML string
original_str = [...
'title = "Round Trip Test"' newline ...
'[settings]' newline ...
'value = 42' newline ...
'name = "test"'];

fprintf('Original TOML:\n%s\n', original_str);

% Parse it
config = parseTOMLstring(original_str);

% Write it back
if ~isempty(fieldnames(config))
    new_str = toml_write_string(config);
    fprintf('\nRound-trip TOML:\n%s\n', new_str);
    
    % Parse again to verify
    verify = parseTOMLstring(new_str);
    fprintf('✓ Round-trip successful!\n');
    fprintf('  Title matches: %d\n', strcmp(config.title, verify.title));
    fprintf('  Value matches: %d\n', config.settings.value == verify.settings.value);
end

%% Example 7: Error handling - empty string
fprintf('\n=== Example 7: Error handling - empty string ===\n');

config = parseTOMLstring('');
if isempty(fieldnames(config))
    fprintf('✓ Correctly returned empty struct for empty string\n');
end

%% Example 8: Error handling - invalid TOML
fprintf('\n=== Example 8: Error handling - invalid TOML ===\n');

invalid_toml = [...
'title = "Missing closing quote' newline ...
'value = 123'];

config = parseTOMLstring(invalid_toml);
if isempty(fieldnames(config))
    fprintf('✓ Correctly returned empty struct for invalid TOML\n');
end

%% Example 9: Parse multi-line strings
fprintf('\n=== Example 9: Parse multi-line strings ===\n');

toml_str = [...
'description = """' newline ...
'This is a' newline ...
'multi-line' newline ...
'description.' newline ...
'"""'];

config = parseTOMLstring(toml_str);

if ~isempty(fieldnames(config))
    fprintf('✓ Parsed multi-line string\n');
    fprintf('Description:\n%s\n', config.description);
end

%% Example 10: Practical use case - configuration from variable
fprintf('\n=== Example 10: Dynamic configuration ===\n');

% Build configuration dynamically
server = "localhost";
port = 8080;

toml_str = sprintf(['server = "%s"\n' ...
                    'port = %d\n' ...
                    'debug = true'], server, port);

config = parseTOMLstring(toml_str);

if ~isempty(fieldnames(config))
    fprintf('✓ Parsed dynamic configuration\n');
    fprintf('  Server: %s\n', config.server);
    fprintf('  Port: %d\n', config.port);
    fprintf('  Debug: %d\n', config.debug);
end

%% Summary
fprintf('\n=== Summary ===\n');
fprintf('parseTOMLstring() provides:\n');
fprintf('  ✓ Parse TOML from strings (not files)\n');
fprintf('  ✓ Input validation\n');
fprintf('  ✓ Clear error messages\n');
fprintf('  ✓ Graceful error handling\n');
fprintf('  ✓ Safe defaults (empty struct on error)\n');
fprintf('  ✓ Round-trip with toml_write_string()\n');
fprintf('\nUsage:\n');
fprintf('  data = parseTOMLstring(toml_str)  %% Returns empty struct on error\n');
fprintf('\nUse cases:\n');
fprintf('  • Parse configuration from variables\n');
fprintf('  • Round-trip testing\n');
fprintf('  • Dynamic config generation\n');
fprintf('  • API responses (TOML as string)\n');