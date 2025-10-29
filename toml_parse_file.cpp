/*
 * toml_parse_file.cpp
 * Minimal MEX wrapper for tomlplusplus to parse TOML files in MATLAB
 * WITH FIELD ORDER PRESERVATION
 * 
 * Compile with:
 *   mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_parse_file.cpp
 *
 * Usage in MATLAB:
 *   data = toml_parse_file('config.toml');
 */

#include "mex.h"
#include <toml++/toml.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

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
        
        // Check if this integer has special formatting
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
            
            // Store the format string
            const char* format_str = is_hex ? "hex" : (is_oct ? "oct" : "bin");
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
    
    // Handle date/time types (convert to string)
    if (auto val = node.as_date()) {
        std::stringstream ss;
        ss << val->get();
        return mxCreateString(ss.str().c_str());
    }
    if (auto val = node.as_time()) {
        std::stringstream ss;
        ss << val->get();
        return mxCreateString(ss.str().c_str());
    }
    if (auto val = node.as_date_time()) {
        std::stringstream ss;
        ss << val->get();
        return mxCreateString(ss.str().c_str());
    }
    
    // Default: empty matrix
    return mxCreateDoubleMatrix(0, 0, mxREAL);
}

// MEX entry point
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    // Check arguments
    if (nrhs != 1) {
        mexErrMsgIdAndTxt("toml_parse_file:invalidArgs", 
                          "Usage: data = toml_parse_file('filename.toml')");
    }
    
    if (!mxIsChar(prhs[0])) {
        mexErrMsgIdAndTxt("toml_parse_file:invalidInput", 
                          "Input must be a filename string");
    }
    
    // Get filename
    char* filename = mxArrayToString(prhs[0]);
    if (!filename) {
        mexErrMsgIdAndTxt("toml_parse_file:memoryError", 
                          "Could not allocate memory for filename");
    }
    
    // Parse TOML file
    try {
        toml::table tbl = toml::parse_file(filename);
        plhs[0] = convert_table(tbl);
    }
    catch (const toml::parse_error& err) {
        mxFree(filename);
        std::string error_msg = "TOML parse error: ";
        error_msg += err.what();
        mexErrMsgIdAndTxt("toml_parse_file:parseError", error_msg.c_str());
    }
    catch (const std::exception& e) {
        mxFree(filename);
        std::string error_msg = "Error: ";
        error_msg += e.what();
        mexErrMsgIdAndTxt("toml_parse_file:error", error_msg.c_str());
    }
    
    mxFree(filename);
}