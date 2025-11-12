# Matlab-tomlplusplus

MATLAB wrapper for [tomlplusplus](https://github.com/marzer/tomlplusplus) - a header-only TOML config file parser for C++17.

## Building

First, compile the MEX files:

```matlab
build_toml_mex
```

If it fails, ensure you have MEX compiler capability installed on your machine.

## Examples

### Parse a TOML string

```matlab
toml_str = 'title = "Example"' + newline + 'count = 42';
data = toml_parse_string(toml_str);
```

### Parse a TOML file

```matlab
data = toml_parse('config.toml');
title = data.title;
port = data.server.port;
```

### Write a TOML string

```matlab
data = struct('title', 'My App', 'version', '1.0');
toml_str = toml_write_string(data);
```

### Write a TOML file

```matlab
data = struct('database', struct('host', 'localhost', 'port', 5432));
toml_write('config.toml', data);
```

### Update a TOML file (preserve formatting)

```matlab
data = toml_parse('config.toml');
data.server.port = 8080;
toml_update('config.toml', data);  % Comments and formatting preserved
```

See the [examples](examples/) folder for more usage examples.

## Requirements

- MATLAB with MEX compiler
- C++17 compatible compiler

## Reference

- [TOML v1.0.0 Specification](https://toml.io/en/v1.0.0)
- [tomlplusplus Documentation](https://marzer.github.io/tomlplusplus/)
