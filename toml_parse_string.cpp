/*
 * toml_parse_string.cpp
 * Minimal MEX wrapper for tomlplusplus to parse TOML strings in MATLAB
 * 
 * Compile with:
 *   mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_parse_string.cpp
 *
 * Usage in MATLAB:
 *   toml_str = '[server]
 *               host = "localhost"
 *               port = 8080';
 *   data = toml_parse_string(toml_str);
 */

#include "mex.h"
#include <toml++/toml.h>
#include <string>
#include <vector>
#include <sstream>

// Forward declaration
mxArray* convert_node(const toml::node& node);

// Convert TOML table to MATLAB struct
mxArray* convert_table(const toml::table& tbl) {
    // Collect field names
    std::vector<std::string> keys;
    for (auto& [k, v] : tbl) {
        keys.push_back(std::string(k));
    }
    
    // Create field names array for MATLAB
    std::vector<const char*> field_names;
    for (const auto& key : keys) {
        field_names.push_back(key.c_str());
    }
    
    // Create MATLAB struct
    mxArray* matlab_struct = mxCreateStructMatrix(
        1, 1,  // 1x1 struct
        static_cast<int>(field_names.size()),
        field_names.data()
    );
    
    // Populate struct fields
    int field_idx = 0;
    for (auto& [k, v] : tbl) {
        mxSetFieldByNumber(matlab_struct, 0, field_idx++, convert_node(v));
    }
    
    return matlab_struct;
}

// // Convert TOML array to MATLAB cell array
// mxArray* convert_array(const toml::array& arr) {
//     mxArray* cell = mxCreateCellMatrix(1, arr.size());
// 
//     for (size_t i = 0; i < arr.size(); ++i) {
//         mxSetCell(cell, i, convert_node(arr[i]));
//     }
// 
//     return cell;
// }


mxArray* convert_array(const toml::array& arr) {
    // Check if all elements are numeric
    bool all_numeric = true;
    for (const auto& el : arr) {
        if (!el.is_integer() && !el.is_floating_point()) {
            all_numeric = false;
            break;
        }
    }

    if (all_numeric) {
        // Create a 1×N double array
        mxArray* numeric_array = mxCreateDoubleMatrix(1, arr.size(), mxREAL);
        double* data = mxGetPr(numeric_array);
        for (size_t i = 0; i < arr.size(); ++i) {
            if (arr[i].is_integer()) {
                data[i] = static_cast<double>(arr[i].as_integer()->get());
            } else {
                data[i] = arr[i].as_floating_point()->get();
            }
        }
        return numeric_array;
    }

    // Otherwise, fallback to cell array
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
        mexErrMsgIdAndTxt("toml_parse_string:invalidArgs", 
                          "Usage: data = toml_parse_string(toml_string)");
    }
    
    if (!mxIsChar(prhs[0])) {
        mexErrMsgIdAndTxt("toml_parse_string:invalidInput", 
                          "Input must be a TOML string");
    }
    
    // Get TOML string
    char* toml_cstr = mxArrayToString(prhs[0]);
    if (!toml_cstr) {
        mexErrMsgIdAndTxt("toml_parse_string:memoryError", 
                          "Could not allocate memory for TOML string");
    }
    
    std::string toml_string(toml_cstr);
    mxFree(toml_cstr);
    
    // Parse TOML string
    try {
        toml::table tbl = toml::parse(toml_string);
        plhs[0] = convert_table(tbl);
    }
    catch (const toml::parse_error& err) {
        std::string error_msg = "TOML parse error: ";
        error_msg += err.what();
        mexErrMsgIdAndTxt("toml_parse_string:parseError", error_msg.c_str());
    }
    catch (const std::exception& e) {
        std::string error_msg = "Error: ";
        error_msg += e.what();
        mexErrMsgIdAndTxt("toml_parse_string:error", error_msg.c_str());
    }
}