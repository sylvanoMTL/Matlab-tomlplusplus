/*
 * toml_parse_string.cpp
 * Minimal MEX wrapper for tomlplusplus to parse TOML strings in MATLAB
 * WITH FIELD ORDER PRESERVATION
 * 
 * Compile with:
 *   mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_parse_string.cpp
 *
 * Usage in MATLAB:
 *   toml_str = 'name = "value"' + newline + 'number = 42';
 *   data = toml_parse_string(toml_str);
 */

#include "mex.h"
#include <toml++/toml.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstring>

// Forward declaration
mxArray* convert_node(const toml::node& node);

// Helper structure to track field order by source position
struct FieldInfo {
    std::string key;
    const toml::node* node;
    uint32_t line;
    uint32_t column;
    
    // Sort by line first, then column to preserve original order
    bool operator<(const FieldInfo& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

// Convert TOML table to MATLAB struct (with order preservation)
mxArray* convert_table(const toml::table& tbl) {
    if (tbl.empty()) {
        return mxCreateStructMatrix(1, 1, 0, nullptr);
    }
    
    // Collect all fields with their source positions
    std::vector<FieldInfo> fields;
    fields.reserve(tbl.size());
    
    for (auto& [k, v] : tbl) {
        FieldInfo info;
        info.key = std::string(k);
        info.node = &v;
        
        // Get source location to preserve original order
        auto src = v.source();
        if (src.begin) {
            info.line = src.begin.line;
            info.column = src.begin.column;
        } else {
            // If no source info (e.g., programmatically created), 
            // use max values (will sort to end)
            info.line = UINT32_MAX;
            info.column = UINT32_MAX;
        }
        
        fields.push_back(info);
    }
    
    // Sort by source position to restore original order
    std::sort(fields.begin(), fields.end());
    
    // Create field names array in correct order
    std::vector<const char*> field_names;
    field_names.reserve(fields.size());
    for (const auto& field : fields) {
        field_names.push_back(field.key.c_str());
    }
    
    // Create MATLAB struct with ordered fields
    mxArray* matlab_struct = mxCreateStructMatrix(
        1, 1,  // 1x1 struct
        static_cast<int>(field_names.size()),
        field_names.data()
    );
    
    // Populate struct fields in the correct order
    for (size_t i = 0; i < fields.size(); ++i) {
        mxSetFieldByNumber(matlab_struct, 0, i, convert_node(*fields[i].node));
    }
    
    return matlab_struct;
}

// Convert TOML array to MATLAB array (typed for homogeneous data)
mxArray* convert_array(const toml::array& arr) {
    if (arr.empty()) {
        return mxCreateCellMatrix(1, 0);
    }
    
    // Check if array is homogeneous
    bool all_integers = true;
    bool all_floats = true;
    bool all_bools = true;
    
    for (const auto& elem : arr) {
        if (!elem.is_integer()) all_integers = false;
        if (!elem.is_floating_point()) all_floats = false;
        if (!elem.is_boolean()) all_bools = false;
    }
    
    // Create int64 array for integer arrays
    if (all_integers) {
        mxArray* int_array = mxCreateNumericMatrix(1, arr.size(), mxINT64_CLASS, mxREAL);
        int64_t* data = (int64_t*)mxGetData(int_array);
        
        for (size_t i = 0; i < arr.size(); ++i) {
            data[i] = arr[i].value_or<int64_t>(0);
        }
        
        return int_array;
    }
    
    // Create double array for float arrays
    if (all_floats) {
        mxArray* float_array = mxCreateDoubleMatrix(1, arr.size(), mxREAL);
        double* data = mxGetPr(float_array);
        
        for (size_t i = 0; i < arr.size(); ++i) {
            data[i] = arr[i].value_or<double>(0.0);
        }
        
        return float_array;
    }
    
    // Create logical array for boolean arrays
    if (all_bools) {
        mxArray* bool_array = mxCreateLogicalMatrix(1, arr.size());
        mxLogical* data = mxGetLogicals(bool_array);
        
        for (size_t i = 0; i < arr.size(); ++i) {
            data[i] = arr[i].value_or<bool>(false);
        }
        
        return bool_array;
    }
    
    // Otherwise use cell array for heterogeneous data
    mxArray* cell = mxCreateCellMatrix(1, arr.size());
    
    for (size_t i = 0; i < arr.size(); ++i) {
        mxSetCell(cell, i, convert_node(arr[i]));
    }
    
    return cell;
}

// Convert any TOML node to MATLAB type
mxArray* convert_node(const toml::node& node) {
    // Handle tables
    if (auto tbl = node.as_table()) {
        return convert_table(*tbl);
    }
    
    // Handle arrays
    if (auto arr = node.as_array()) {
        return convert_array(*arr);
    }
    
    // Handle string values
    if (auto val = node.as_string()) {
        return mxCreateString(val->get().c_str());
    }
    
    // Handle integer values - check for special formatting (hex, octal, binary)
    if (auto val = node.as_integer()) {
        int64_t int_val = val->get();
        
        // Access value flags from the value object itself
        auto flags = val->flags();
        
        // Check if this integer has special formatting (check in specific order)
        bool is_hex = (flags & toml::value_flags::format_as_hexadecimal) != toml::value_flags::none;
        bool is_oct = (flags & toml::value_flags::format_as_octal) != toml::value_flags::none;
        bool is_bin = (flags & toml::value_flags::format_as_binary) != toml::value_flags::none;
        
        // If it has special formatting, store as struct with format info
        if (is_hex || is_oct || is_bin) {
            const char* field_names[] = {"value", "format"};
            mxArray* result = mxCreateStructMatrix(1, 1, 2, field_names);
            
            // Store the numeric value
            mxArray* value_field = mxCreateNumericMatrix(1, 1, mxINT64_CLASS, mxREAL);
            *((int64_t*)mxGetData(value_field)) = int_val;
            mxSetField(result, 0, "value", value_field);
            
            // Store the format string - check each format explicitly
            const char* format_str;
            if (is_bin) {
                format_str = "bin";
            } else if (is_oct) {
                format_str = "oct";
            } else if (is_hex) {
                format_str = "hex";
            } else {
                // Shouldn't reach here, but default to hex
                format_str = "hex";
            }
            mxSetField(result, 0, "format", mxCreateString(format_str));
            
            return result;
        }
        
        // Regular integer without special formatting
        mxArray* result = mxCreateNumericMatrix(1, 1, mxINT64_CLASS, mxREAL);
        *((int64_t*)mxGetData(result)) = int_val;
        return result;
    }
    
    // Handle floating point values
    if (auto val = node.as_floating_point()) {
        return mxCreateDoubleScalar(val->get());
    }
    
    // Handle boolean values
    if (auto val = node.as_boolean()) {
        return mxCreateLogicalScalar(val->get());
    }
    
    // Handle date/time types
    if (auto val = node.as_date()) {
        // Local date only - create datetime with just the date
        auto d = val->get();
        
        // Create MATLAB datetime: datetime(year, month, day)
        mxArray* dateArgs[3];
        dateArgs[0] = mxCreateDoubleScalar(d.year);
        dateArgs[1] = mxCreateDoubleScalar(d.month);
        dateArgs[2] = mxCreateDoubleScalar(d.day);
        
        mxArray* lhs[1];
        mexCallMATLAB(1, lhs, 3, dateArgs, "datetime");
        
        mxDestroyArray(dateArgs[0]);
        mxDestroyArray(dateArgs[1]);
        mxDestroyArray(dateArgs[2]);
        
        return lhs[0];
    }
    
    if (auto val = node.as_time()) {
        // Local time only - create datetime with default date (1970-01-01) and the time
        auto t = val->get();
        
        // Create MATLAB datetime: datetime(1970, 1, 1, hour, minute, second)
        mxArray* dateArgs[6];
        dateArgs[0] = mxCreateDoubleScalar(1970);  // Default year
        dateArgs[1] = mxCreateDoubleScalar(1);     // Default month
        dateArgs[2] = mxCreateDoubleScalar(1);     // Default day
        dateArgs[3] = mxCreateDoubleScalar(t.hour);
        dateArgs[4] = mxCreateDoubleScalar(t.minute);
        dateArgs[5] = mxCreateDoubleScalar(t.second + t.nanosecond / 1e9);
        
        mxArray* lhs[1];
        mexCallMATLAB(1, lhs, 6, dateArgs, "datetime");
        
        for (int i = 0; i < 6; i++) {
            mxDestroyArray(dateArgs[i]);
        }
        
        return lhs[0];
    }
    
    if (auto val = node.as_date_time()) {
        // Date-time (with or without offset)
        auto dt = val->get();
        
        // Create datetime(year, month, day, hour, minute, second)
        mxArray* dateArgs[6];
        dateArgs[0] = mxCreateDoubleScalar(dt.date.year);
        dateArgs[1] = mxCreateDoubleScalar(dt.date.month);
        dateArgs[2] = mxCreateDoubleScalar(dt.date.day);
        dateArgs[3] = mxCreateDoubleScalar(dt.time.hour);
        dateArgs[4] = mxCreateDoubleScalar(dt.time.minute);
        dateArgs[5] = mxCreateDoubleScalar(dt.time.second + dt.time.nanosecond / 1e9);
        
        mxArray* lhs[1];
        mexCallMATLAB(1, lhs, 6, dateArgs, "datetime");
        
        for (int i = 0; i < 6; i++) {
            mxDestroyArray(dateArgs[i]);
        }
        
        // If there's a timezone offset, store as struct with datetime and offset
        if (dt.offset.has_value()) {
            auto offset = dt.offset.value();
            int offset_minutes = offset.minutes;
            
            // Create the datetime without timezone first (local time)
            const char* field_names[] = {"datetime", "offset_minutes"};
            mxArray* result = mxCreateStructMatrix(1, 1, 2, field_names);
            
            // Store the datetime
            mxSetField(result, 0, "datetime", lhs[0]);
            
            // Store the offset in minutes
            mxArray* offset_field = mxCreateDoubleScalar(offset_minutes);
            mxSetField(result, 0, "offset_minutes", offset_field);
            
            return result;
        }
        
        return lhs[0];
    }
    
    // Default: empty matrix
    return mxCreateDoubleMatrix(0, 0, mxREAL);
}

// MEX entry point
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    // Check arguments
    if (nrhs != 1) {
        mexErrMsgIdAndTxt("toml_parse_string:invalidArgs", 
                          "Usage: data = toml_parse_string(toml_string)");
    }
    
    if (!mxIsChar(prhs[0])) {
        mexErrMsgIdAndTxt("toml_parse_string:invalidInput", 
                          "Input must be a TOML string");
    }
    
    // Get TOML string
    char* toml_string = mxArrayToString(prhs[0]);
    if (!toml_string) {
        mexErrMsgIdAndTxt("toml_parse_string:memoryError", 
                          "Could not allocate memory for TOML string");
    }
    
    // Parse TOML string
    try {
        toml::table tbl = toml::parse(toml_string);
        mxFree(toml_string);
        plhs[0] = convert_table(tbl);
    }
    catch (const toml::parse_error& err) {
        mxFree(toml_string);
        std::string error_msg = "TOML parse error: ";
        error_msg += err.what();
        mexErrMsgIdAndTxt("toml_parse_string:parseError", error_msg.c_str());
    }
    catch (const std::exception& e) {
        mxFree(toml_string);
        std::string error_msg = "Error: ";
        error_msg += e.what();
        mexErrMsgIdAndTxt("toml_parse_string:error", error_msg.c_str());
    }
}