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

// Structure to track field order by source position
struct FieldWithPosition {
    std::string key;
    const toml::node* node;
    uint32_t line;
    uint32_t column;
    
    // Sort by line first, then column (to preserve file order)
    bool operator<(const FieldWithPosition& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

// Convert TOML table to MATLAB struct (with order preservation)
mxArray* convert_table(const toml::table& tbl) {
    if (tbl.empty()) {
        return mxCreateStructMatrix(1, 1, 0, nullptr);
    }
    
    // Step 1: Collect all fields with their source positions
    std::vector<FieldWithPosition> fields;
    fields.reserve(tbl.size());
    
    for (const auto& [k, v] : tbl) {
        FieldWithPosition field;
        field.key = std::string(k);
        field.node = &v;
        
        // Get source location to determine original order
        auto src = v.source();
        if (src.begin) {
            field.line = src.begin.line;
            field.column = src.begin.column;
        } else {
            // If no source info (shouldn't happen for parsed files),
            // use maximum values so they sort to the end
            field.line = UINT32_MAX;
            field.column = UINT32_MAX;
        }
        
        fields.push_back(field);
    }
    
    // Step 2: Sort by source position to restore original file order
    std::sort(fields.begin(), fields.end());
    
    // Step 3: Create field names array in the correct order
    std::vector<const char*> field_names;
    field_names.reserve(fields.size());
    for (const auto& field : fields) {
        field_names.push_back(field.key.c_str());
    }
    
    // Step 4: Create MATLAB struct with ordered fields
    mxArray* matlab_struct = mxCreateStructMatrix(
        1, 1,  // 1x1 struct
        static_cast<int>(field_names.size()),
        field_names.data()
    );
    
    // Step 5: Populate struct fields in order
    for (size_t i = 0; i < fields.size(); ++i) {
        mxSetFieldByNumber(matlab_struct, 0, static_cast<int>(i), 
                          convert_node(*fields[i].node));
    }
    
    return matlab_struct;
}

// Convert TOML array to MATLAB cell array or numeric array
mxArray* convert_array(const toml::array& arr) {
    if (arr.empty()) {
        return mxCreateCellMatrix(1, 0);
    }
    
    // Check if array is homogeneous and numeric
    bool all_integers = true;
    bool all_floats = true;
    
    for (const auto& elem : arr) {
        if (!elem.is_integer()) all_integers = false;
        if (!elem.is_floating_point()) all_floats = false;
    }
    
    // Create numeric array for homogeneous numeric data
    if (all_integers || all_floats) {
        mxArray* numeric_array = mxCreateDoubleMatrix(1, arr.size(), mxREAL);
        double* data = mxGetPr(numeric_array);
        
        for (size_t i = 0; i < arr.size(); ++i) {
            if (all_integers) {
                data[i] = static_cast<double>(arr[i].value_or<int64_t>(0));
            } else {
                data[i] = arr[i].value_or<double>(0.0);
            }
        }
        
        return numeric_array;
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
    // Handle tables (nested structs)
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
    
    // Handle integer values
    if (auto val = node.as_integer()) {
        return mxCreateDoubleScalar(static_cast<double>(val->get()));
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