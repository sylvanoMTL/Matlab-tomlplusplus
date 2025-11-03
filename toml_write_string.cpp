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
#include <iomanip>
#include <cstring>

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
    // EXCEPT for special structs: formatted integers and offset datetimes
    if (mxIsStruct(mx)) {
        // Check for offset datetime struct
        if (mxGetNumberOfFields(mx) == 2) {
            mxArray* datetime_field = mxGetField(mx, 0, "datetime");
            mxArray* offset_field = mxGetField(mx, 0, "offset_minutes");
            
            if (datetime_field && offset_field && 
                strcmp(mxGetClassName(datetime_field), "datetime") == 0 &&
                mxIsDouble(offset_field)) {
                
                // This is an offset datetime - extract components
                mxArray* yearLhs[1], *monthLhs[1], *dayLhs[1];
                mxArray* hourLhs[1], *minuteLhs[1], *secondLhs[1];
                
                mxArray* rhs[1] = {datetime_field};
                
                mexCallMATLAB(1, yearLhs, 1, rhs, "year");
                mexCallMATLAB(1, monthLhs, 1, rhs, "month");
                mexCallMATLAB(1, dayLhs, 1, rhs, "day");
                mexCallMATLAB(1, hourLhs, 1, rhs, "hour");
                mexCallMATLAB(1, minuteLhs, 1, rhs, "minute");
                mexCallMATLAB(1, secondLhs, 1, rhs, "second");
                
                int y = (int)mxGetScalar(yearLhs[0]);
                int m = (int)mxGetScalar(monthLhs[0]);
                int d = (int)mxGetScalar(dayLhs[0]);
                int h = (int)mxGetScalar(hourLhs[0]);
                int min = (int)mxGetScalar(minuteLhs[0]);
                double s = mxGetScalar(secondLhs[0]);
                
                mxDestroyArray(yearLhs[0]);
                mxDestroyArray(monthLhs[0]);
                mxDestroyArray(dayLhs[0]);
                mxDestroyArray(hourLhs[0]);
                mxDestroyArray(minuteLhs[0]);
                mxDestroyArray(secondLhs[0]);
                
                int offset_minutes = (int)mxGetScalar(offset_field);
                
                // Create date_time with offset
                int sec = (int)s;
                int nanosec = (int)((s - sec) * 1e9);
                
                toml::date date{y, (unsigned)m, (unsigned)d};
                toml::time time{(unsigned)h, (unsigned)min, (unsigned)sec, (unsigned)nanosec};
                
                int tz_hours = offset_minutes / 60;
                int tz_minutes = offset_minutes % 60;
                toml::time_offset offset{tz_hours, tz_minutes};
                toml::date_time dt{date, time, offset};
                
                return std::make_unique<toml::value<toml::date_time>>(dt);
            }
        }
        
        // Not a special struct - return nullptr so it's handled by serialization
        return nullptr;
    }

    if (mxIsCell(mx))
        return std::make_unique<toml::array>(convert_cell_to_array(mx));

    if (mxIsChar(mx)) {
        char* str = mxArrayToString(mx);
        std::string result(str ? str : "");
        if (str) mxFree(str);
        return std::make_unique<toml::value<std::string>>(result);
    }
    
    // Handle MATLAB datetime objects
    if (strcmp(mxGetClassName(mx), "datetime") == 0) {
        // Extract datetime components using MATLAB functions
        mxArray* yearLhs[1], *monthLhs[1], *dayLhs[1];
        mxArray* hourLhs[1], *minuteLhs[1], *secondLhs[1];
        mxArray* tzLhs[1];
        
        mxArray* rhs[1] = {const_cast<mxArray*>(mx)};
        
        mexCallMATLAB(1, yearLhs, 1, rhs, "year");
        mexCallMATLAB(1, monthLhs, 1, rhs, "month");
        mexCallMATLAB(1, dayLhs, 1, rhs, "day");
        mexCallMATLAB(1, hourLhs, 1, rhs, "hour");
        mexCallMATLAB(1, minuteLhs, 1, rhs, "minute");
        mexCallMATLAB(1, secondLhs, 1, rhs, "second");
        
        int y = (int)mxGetScalar(yearLhs[0]);
        int m = (int)mxGetScalar(monthLhs[0]);
        int d = (int)mxGetScalar(dayLhs[0]);
        int h = (int)mxGetScalar(hourLhs[0]);
        int min = (int)mxGetScalar(minuteLhs[0]);
        double s = mxGetScalar(secondLhs[0]);
        
        mxDestroyArray(yearLhs[0]);
        mxDestroyArray(monthLhs[0]);
        mxDestroyArray(dayLhs[0]);
        mxDestroyArray(hourLhs[0]);
        mxDestroyArray(minuteLhs[0]);
        mxDestroyArray(secondLhs[0]);
        
        // Check if it's date-only (all time components are 0)
        if (h == 0 && min == 0 && s == 0.0) {
            // Date only
            toml::date date{y, (unsigned)m, (unsigned)d};
            return std::make_unique<toml::value<toml::date>>(date);
        }
        
        // Check if it's time-only (date is default 1970-01-01)
        if (y == 1970 && m == 1 && d == 1) {
            // Time only - write as TOML local time
            int sec = (int)s;
            int nanosec = (int)((s - sec) * 1e9);
            toml::time time{(unsigned)h, (unsigned)min, (unsigned)sec, (unsigned)nanosec};
            return std::make_unique<toml::value<toml::time>>(time);
        }
        
        // Regular datetime (local, no timezone)
        int sec = (int)s;
        int nanosec = (int)((s - sec) * 1e9);
        
        toml::date date{y, (unsigned)m, (unsigned)d};
        toml::time time{(unsigned)h, (unsigned)min, (unsigned)sec, (unsigned)nanosec};
        toml::date_time dt{date, time};
        return std::make_unique<toml::value<toml::date_time>>(dt);
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

    // Handle int64 arrays (from toml_parse_file)
    if (mxIsInt64(mx)) {
        mwSize num_elements = mxGetNumberOfElements(mx);
        if (num_elements == 1) {
            int64_t val = *((int64_t*)mxGetData(mx));
            return std::make_unique<toml::value<int64_t>>(val);
        } else {
            toml::array arr;
            int64_t* data = (int64_t*)mxGetData(mx);
            for (mwSize i = 0; i < num_elements; ++i)
                arr.push_back(data[i]);
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
    // Special case: formatted integer struct (from parser)
    if (mxIsStruct(mx) && mxGetNumberOfFields(mx) == 2) {
        mxArray* value_field = mxGetField(mx, 0, "value");
        mxArray* format_field = mxGetField(mx, 0, "format");
        
        if (value_field && format_field && mxIsInt64(value_field) && mxIsChar(format_field)) {
            int64_t val = *((int64_t*)mxGetData(value_field));
            char* format_str = mxArrayToString(format_field);
            
            if (format_str) {
                std::string fmt(format_str);
                mxFree(format_str);
                
                // Write in the specified format
                if (fmt == "hex") {
                    ss << "0x" << std::hex << std::uppercase << val << std::dec;
                    return;
                } else if (fmt == "oct") {
                    ss << "0o";
                    // Convert to octal string manually
                    if (val == 0) {
                        ss << "0";
                    } else {
                        std::string octal;
                        int64_t v = val;
                        // Handle negative numbers
                        if (v < 0) {
                            ss << "-";
                            v = -v;
                        }
                        uint64_t uval = static_cast<uint64_t>(v);
                        while (uval > 0) {
                            octal = (char)('0' + (uval & 7)) + octal;
                            uval >>= 3;
                        }
                        ss << octal;
                    }
                    return;
                } else if (fmt == "bin") {
                    ss << "0b";
                    // Convert to binary string
                    if (val == 0) {
                        ss << "0";
                    } else {
                        std::string binary;
                        int64_t v = val;
                        // Handle negative numbers
                        if (v < 0) {
                            ss << "-";
                            v = -v;
                        }
                        uint64_t uval = static_cast<uint64_t>(v);
                        while (uval > 0) {
                            binary = (char)('0' + (uval & 1)) + binary;
                            uval >>= 1;
                        }
                        ss << binary;
                    }
                    return;
                }
            }
        }
    }
    
    auto node_ptr = convert_mx_to_node(mx);
    if (!node_ptr) return;
    
    // String scalar - check for newlines and use appropriate format
    if (auto s = node_ptr->as_string()) {
        auto *raw = static_cast<toml::value<std::string>*>(node_ptr.release());
        std::string v = raw->get();
        delete raw;
        
        // Check if string contains newlines
        if (v.find('\n') != std::string::npos) {
            // Multi-line string - use triple quotes
            // Output content exactly as-is (don't add trailing newline)
            ss << "\"\"\"\n" << v << "\"\"\"";
        } else {
            // Single-line string - use double quotes with escaping
            ss << "\"" << escape_for_double_quotes(v) << "\"";
        }
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
        double val = raw->get();
        delete raw;
        
        // Format float with reasonable precision
        // Check for special values
        if (std::isinf(val)) {
            ss << (val > 0 ? "inf" : "-inf");
        } else if (std::isnan(val)) {
            ss << "nan";
        } else {
            std::ostringstream float_ss;
            double abs_val = std::abs(val);
            
            // Use scientific notation for very small/large numbers
            if (abs_val > 0 && (abs_val < 1e-4 || abs_val >= 1e10)) {
                // Round to 12 significant figures to avoid precision artifacts
                // This turns 9.9999999999999994e-12 into 1e-11
                float_ss << std::scientific << std::setprecision(11) << val;
                std::string str = float_ss.str();
                
                // Clean up: remove unnecessary trailing zeros in mantissa
                size_t e_pos = str.find('e');
                if (e_pos != std::string::npos) {
                    size_t decimal_pos = str.find('.');
                    if (decimal_pos != std::string::npos && decimal_pos < e_pos) {
                        size_t last_nonzero = e_pos - 1;
                        while (last_nonzero > decimal_pos && str[last_nonzero] == '0') {
                            last_nonzero--;
                        }
                        if (last_nonzero == decimal_pos) {
                            str = str.substr(0, decimal_pos) + str.substr(e_pos);
                        } else {
                            str = str.substr(0, last_nonzero + 1) + str.substr(e_pos);
                        }
                    }
                }
                ss << str;
            } else if (val == std::floor(val) && abs_val < 1e15) {
                // Integer-like value
                float_ss << std::fixed << std::setprecision(1) << val;
                ss << float_ss.str();
            } else {
                // Regular float - use reasonable precision
                float_ss << std::setprecision(12) << val;
                std::string str = float_ss.str();
                
                // Clean up trailing zeros
                if (str.find('.') != std::string::npos) {
                    size_t last_nonzero = str.length() - 1;
                    while (last_nonzero > 0 && str[last_nonzero] == '0') {
                        last_nonzero--;
                    }
                    if (str[last_nonzero] == '.') {
                        str = str.substr(0, last_nonzero + 2);
                    } else {
                        str = str.substr(0, last_nonzero + 1);
                    }
                }
                ss << str;
            }
        }
        return;
    }
    else if (auto b = node_ptr->as_boolean()) {
        auto *raw = static_cast<toml::value<bool>*>(node_ptr.release());
        tmp.insert_or_assign(dummy_key, std::move(*raw));
        delete raw;
    }
    else if (auto d = node_ptr->as_date()) {
        auto *raw = static_cast<toml::value<toml::date>*>(node_ptr.release());
        tmp.insert_or_assign(dummy_key, std::move(*raw));
        delete raw;
    }
    else if (auto t = node_ptr->as_time()) {
        auto *raw = static_cast<toml::value<toml::time>*>(node_ptr.release());
        tmp.insert_or_assign(dummy_key, std::move(*raw));
        delete raw;
    }
    else if (auto dt = node_ptr->as_date_time()) {
        auto *raw = static_cast<toml::value<toml::date_time>*>(node_ptr.release());
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
    
    // First pass: write all non-struct, non-cell-of-structs fields
    for (int i = 0; i < num_fields; ++i) {
        const char* fname = mxGetFieldNameByNumber(mx_struct, i);
        mxArray* fv = mxGetField(mx_struct, 0, fname);
        if (!fv || mxIsEmpty(fv)) continue;
        
        // Special case: formatted integer struct (value + format) should be treated as a value
        bool is_formatted_int = false;
        if (mxIsStruct(fv) && mxGetNumberOfFields(fv) == 2) {
            mxArray* value_field = mxGetField(fv, 0, "value");
            mxArray* format_field = mxGetField(fv, 0, "format");
            if (value_field && format_field && mxIsInt64(value_field) && mxIsChar(format_field)) {
                is_formatted_int = true;
            }
        }
        
        // Special case: offset datetime struct should be treated as a value
        bool is_offset_datetime = false;
        if (mxIsStruct(fv) && mxGetNumberOfFields(fv) == 2) {
            mxArray* datetime_field = mxGetField(fv, 0, "datetime");
            mxArray* offset_field = mxGetField(fv, 0, "offset_minutes");
            if (datetime_field && offset_field && 
                strcmp(mxGetClassName(datetime_field), "datetime") == 0 &&
                mxIsDouble(offset_field)) {
                is_offset_datetime = true;
            }
        }
        
        // Skip structs in first pass (unless it's a special struct)
        if (mxIsStruct(fv) && !is_formatted_int && !is_offset_datetime) continue;
        
        // Skip cell arrays of structs (array of tables) in first pass
        if (mxIsCell(fv)) {
            mwSize num_elements = mxGetNumberOfElements(fv);
            bool is_array_of_structs = true;
            for (mwSize j = 0; j < num_elements; ++j) {
                mxArray* elem = mxGetCell(fv, j);
                if (!elem || !mxIsStruct(elem)) {
                    is_array_of_structs = false;
                    break;
                }
            }
            if (is_array_of_structs && num_elements > 0) continue;
        }
        
        ss << fname << " = ";
        serialize_value(ss, fv);
        ss << "\n";
    }
    
    // Second pass: write all struct fields (nested tables)
    for (int i = 0; i < num_fields; ++i) {
        const char* fname = mxGetFieldNameByNumber(mx_struct, i);
        mxArray* fv = mxGetField(mx_struct, 0, fname);
        if (!fv || mxIsEmpty(fv) || !mxIsStruct(fv)) continue;
        
        // Skip formatted integer structs (they were handled in first pass)
        if (mxGetNumberOfFields(fv) == 2) {
            mxArray* value_field = mxGetField(fv, 0, "value");
            mxArray* format_field = mxGetField(fv, 0, "format");
            if (value_field && format_field && mxIsInt64(value_field) && mxIsChar(format_field)) {
                continue;
            }
            
            // Skip offset datetime structs (they were handled in first pass)
            mxArray* datetime_field = mxGetField(fv, 0, "datetime");
            mxArray* offset_field = mxGetField(fv, 0, "offset_minutes");
            if (datetime_field && offset_field && 
                strcmp(mxGetClassName(datetime_field), "datetime") == 0 &&
                mxIsDouble(offset_field)) {
                continue;
            }
        }
        
        // Build full table path
        std::string full_path = prefix.empty() ? fname : (prefix + "." + fname);
        
        ss << "\n[" << full_path << "]\n";
        serialize_struct_recursive(ss, fv, full_path);
    }
    
    // Third pass: write cell arrays of structs as array of tables [[key]]
    for (int i = 0; i < num_fields; ++i) {
        const char* fname = mxGetFieldNameByNumber(mx_struct, i);
        mxArray* fv = mxGetField(mx_struct, 0, fname);
        if (!fv || mxIsEmpty(fv) || !mxIsCell(fv)) continue;
        
        // Check if this is a cell array of structs
        mwSize num_elements = mxGetNumberOfElements(fv);
        bool is_array_of_structs = true;
        for (mwSize j = 0; j < num_elements; ++j) {
            mxArray* elem = mxGetCell(fv, j);
            if (!elem || !mxIsStruct(elem)) {
                is_array_of_structs = false;
                break;
            }
        }
        
        if (!is_array_of_structs || num_elements == 0) continue;
        
        // Write each struct as [[key]] section
        std::string full_path = prefix.empty() ? fname : (prefix + "." + fname);
        
        for (mwSize j = 0; j < num_elements; ++j) {
            mxArray* elem = mxGetCell(fv, j);
            ss << "\n[[" << full_path << "]]\n";
            serialize_struct_recursive(ss, elem, full_path);
        }
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