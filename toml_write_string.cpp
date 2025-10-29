/*
 * toml_write_string.cpp
 * Serialize MATLAB struct to TOML string preserving MATLAB field order
 * including nested structs, and forcing double quotes for string scalars.
 *
 * Compile with:
 *   mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_write_string.cpp
 */

#include "mex.h"
#include <toml++/toml.h>
#include <string>
#include <sstream>
#include <memory>
#include <cmath>
#include <climits>
#include <vector>

// Forward declarations
std::unique_ptr<toml::node> convert_mx_to_node(const mxArray* mx);
void serialize_value(std::ostringstream &ss, const mxArray* mx);
void serialize_struct_recursive(std::ostringstream &ss, const mxArray* mx_struct, 
                                const std::string& prefix);

// Helper: escape a string for double quotes
static std::string escape_for_double_quotes(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default: out += c; break;
        }
    }
    return out;
}

// Convert MATLAB cell array to TOML array
toml::array convert_cell_to_array(const mxArray* mx_cell) {
    toml::array arr;
    mwSize num_elements = mxGetNumberOfElements(mx_cell);
    
    for (mwSize i = 0; i < num_elements; ++i) {
        mxArray* element = mxGetCell(mx_cell, i);
        if (!element || mxIsEmpty(element)) continue;
        
        auto node_ptr = convert_mx_to_node(element);
        if (node_ptr) {
            if (auto t = node_ptr->as_table()) {
                toml::table* raw = static_cast<toml::table*>(node_ptr.release());
                arr.push_back(std::move(*raw));
                delete raw;
            }
            else if (auto a = node_ptr->as_array()) {
                toml::array* raw = static_cast<toml::array*>(node_ptr.release());
                arr.push_back(std::move(*raw));
                delete raw;
            }
            else if (auto s = node_ptr->as_string()) {
                auto *raw = static_cast<toml::value<std::string>*>(node_ptr.release());
                arr.push_back(std::move(*raw));
                delete raw;
            }
            else if (auto i = node_ptr->as_integer()) {
                auto *raw = static_cast<toml::value<int64_t>*>(node_ptr.release());
                arr.push_back(std::move(*raw));
                delete raw;
            }
            else if (auto f = node_ptr->as_floating_point()) {
                auto *raw = static_cast<toml::value<double>*>(node_ptr.release());
                arr.push_back(std::move(*raw));
                delete raw;
            }
            else if (auto b = node_ptr->as_boolean()) {
                auto *raw = static_cast<toml::value<bool>*>(node_ptr.release());
                arr.push_back(std::move(*raw));
                delete raw;
            }
            else {
                node_ptr.reset();
            }
        }
    }
    return arr;
}

// Convert numeric MATLAB arrays to TOML array
toml::array convert_numeric_array_to_toml(const mxArray* mx) {
    toml::array arr;
    double* data = mxGetPr(mx);
    mwSize num_elements = mxGetNumberOfElements(mx);
    
    for (mwSize i = 0; i < num_elements; ++i) {
        double val = data[i];
        if (val == std::floor(val) && val >= INT64_MIN && val <= INT64_MAX)
            arr.push_back(static_cast<int64_t>(val));
        else
            arr.push_back(val);
    }
    return arr;
}

// Convert MATLAB types to toml::node (but NOT structs - those are handled separately)
std::unique_ptr<toml::node> convert_mx_to_node(const mxArray* mx) {
    if (!mx || mxIsEmpty(mx))
        return nullptr;

    // Structs are NOT converted here - they're serialized directly
    if (mxIsStruct(mx))
        return nullptr;

    if (mxIsCell(mx))
        return std::make_unique<toml::array>(convert_cell_to_array(mx));

    if (mxIsChar(mx)) {
        char* str = mxArrayToString(mx);
        std::string result(str ? str : "");
        if (str) mxFree(str);
        return std::make_unique<toml::value<std::string>>(result);
    }

    if (mxIsLogical(mx)) {
        mwSize num = mxGetNumberOfElements(mx);
        if (num == 1) {
            bool v = mxGetLogicals(mx)[0] != 0;
            return std::make_unique<toml::value<bool>>(v);
        } else {
            toml::array arr;
            mxLogical* data = mxGetLogicals(mx);
            for (mwSize i = 0; i < num; ++i)
                arr.push_back(data[i] != 0);
            return std::make_unique<toml::array>(std::move(arr));
        }
    }

    if (mxIsDouble(mx) || mxIsSingle(mx)) {
        mwSize num_elements = mxGetNumberOfElements(mx);
        if (num_elements == 1) {
            double val = mxGetScalar(mx);
            if (val == std::floor(val) && val >= INT64_MIN && val <= INT64_MAX)
                return std::make_unique<toml::value<int64_t>>(static_cast<int64_t>(val));
            else
                return std::make_unique<toml::value<double>>(val);
        } else {
            return std::make_unique<toml::array>(convert_numeric_array_to_toml(mx));
        }
    }

    return nullptr;
}

// Serialize a single value (non-struct) to the output stream
void serialize_value(std::ostringstream &ss, const mxArray* mx) {
    auto node_ptr = convert_mx_to_node(mx);
    if (!node_ptr) return;
    
    // String scalar - force double quotes
    if (auto s = node_ptr->as_string()) {
        auto *raw = static_cast<toml::value<std::string>*>(node_ptr.release());
        std::string v = raw->get();
        delete raw;
        ss << "\"" << escape_for_double_quotes(v) << "\"";
        return;
    }
    
    // For other types, create temporary table and stream it, then extract the value part
    toml::table tmp;
    const char* dummy_key = "__tmp__";
    
    if (auto a = node_ptr->as_array()) {
        toml::array* raw = static_cast<toml::array*>(node_ptr.release());
        tmp.insert_or_assign(dummy_key, std::move(*raw));
        delete raw;
    }
    else if (auto i = node_ptr->as_integer()) {
        auto *raw = static_cast<toml::value<int64_t>*>(node_ptr.release());
        tmp.insert_or_assign(dummy_key, std::move(*raw));
        delete raw;
    }
    else if (auto f = node_ptr->as_floating_point()) {
        auto *raw = static_cast<toml::value<double>*>(node_ptr.release());
        tmp.insert_or_assign(dummy_key, std::move(*raw));
        delete raw;
    }
    else if (auto b = node_ptr->as_boolean()) {
        auto *raw = static_cast<toml::value<bool>*>(node_ptr.release());
        tmp.insert_or_assign(dummy_key, std::move(*raw));
        delete raw;
    }
    else {
        node_ptr.reset();
        return;
    }
    
    // Stream the temporary table and extract just the value part
    std::ostringstream tmp_ss;
    tmp_ss << tmp;
    std::string result = tmp_ss.str();
    
    // Find "= " and extract everything after it
    size_t pos = result.find("= ");
    if (pos != std::string::npos) {
        ss << result.substr(pos + 2);
    }
}

// Recursively serialize a struct, preserving MATLAB field order
void serialize_struct_recursive(std::ostringstream &ss, const mxArray* mx_struct, 
                                const std::string& prefix) {
    int num_fields = mxGetNumberOfFields(mx_struct);
    
    // First pass: write all non-struct fields
    for (int i = 0; i < num_fields; ++i) {
        const char* fname = mxGetFieldNameByNumber(mx_struct, i);
        mxArray* fv = mxGetField(mx_struct, 0, fname);
        if (!fv || mxIsEmpty(fv)) continue;
        
        // Skip structs in first pass
        if (mxIsStruct(fv)) continue;
        
        ss << fname << " = ";
        serialize_value(ss, fv);
        ss << "\n";
    }
    
    // Second pass: write all struct fields (nested tables)
    for (int i = 0; i < num_fields; ++i) {
        const char* fname = mxGetFieldNameByNumber(mx_struct, i);
        mxArray* fv = mxGetField(mx_struct, 0, fname);
        if (!fv || mxIsEmpty(fv) || !mxIsStruct(fv)) continue;
        
        // Build full table path
        std::string full_path = prefix.empty() ? fname : (prefix + "." + fname);
        
        ss << "\n[" << full_path << "]\n";
        serialize_struct_recursive(ss, fv, full_path);
    }
}

// Main MEX entry
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    if (nrhs != 1) {
        mexErrMsgIdAndTxt("toml_write_string:invalidArgs", 
                         "Usage: toml_str = toml_write_string(struct)");
    }
    if (nlhs > 1) {
        mexErrMsgIdAndTxt("toml_write_string:tooManyOutputs", 
                         "Too many output arguments");
    }
    if (!mxIsStruct(prhs[0])) {
        mexErrMsgIdAndTxt("toml_write_string:invalidInput", 
                         "Input must be a MATLAB struct");
    }

    try {
        std::ostringstream ss;
        serialize_struct_recursive(ss, prhs[0], "");
        
        std::string toml_string = ss.str();
        plhs[0] = mxCreateString(toml_string.c_str());
    }
    catch (const std::exception &e) {
        std::string error_msg = "Error creating TOML: ";
        error_msg += e.what();
        mexErrMsgIdAndTxt("toml_write_string:error", error_msg.c_str());
    }
}