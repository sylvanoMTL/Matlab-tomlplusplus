/*
 * toml_write_string.cpp
 * Serialize MATLAB struct to TOML string preserving MATLAB field order
 * and forcing double quotes for string scalars.
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
#include <algorithm>

// Forward declaration
std::unique_ptr<toml::node> convert_mx_to_node(const mxArray* mx);

// Helper: escape a string for placement in double quotes
static std::string escape_for_double_quotes(const std::string &s)
{
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

// Move concrete node into a temporary single-key table and stream it.
// The function expects node_ptr to own a concrete node (table/array/value<T>).
// It will release node_ptr, move the object into tmp, delete the temporary, and append the streamed text to ss.
static void append_single_key_serialized(std::ostringstream &ss, const char* key, std::unique_ptr<toml::node> &node_ptr)
{
    // If it's a table, create root and insert the table under the key to get [key] ... format
    if (auto t = node_ptr->as_table()) {
        toml::table tmp;
        toml::table* raw = static_cast<toml::table*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    // If it's an array, or a value, inserting into a table will produce key = [...]
    if (auto a = node_ptr->as_array()) {
        toml::table tmp;
        toml::array* raw = static_cast<toml::array*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    // Now handle concrete scalar values: string, int, double, bool, date/time
    if (auto s = node_ptr->as_string()) {
        // We'll not use toml++ streaming for the string because we want to force double-quotes.
        std::string v = s->get();
        // consume node
        delete static_cast<toml::value<std::string>*>(node_ptr.release());
        ss << key << " = \"" << escape_for_double_quotes(v) << "\"";
        return;
    }

    if (auto i = node_ptr->as_integer()) {
        toml::table tmp;
        auto *raw = static_cast<toml::value<int64_t>*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    if (auto f = node_ptr->as_floating_point()) {
        toml::table tmp;
        auto *raw = static_cast<toml::value<double>*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    if (auto b = node_ptr->as_boolean()) {
        toml::table tmp;
        auto *raw = static_cast<toml::value<bool>*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    if (auto d = node_ptr->as_date()) {
        toml::table tmp;
        auto *raw = static_cast<toml::value<toml::date>*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    if (auto tm = node_ptr->as_time()) {
        toml::table tmp;
        auto *raw = static_cast<toml::value<toml::time>*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    if (auto dt = node_ptr->as_date_time()) {
        toml::table tmp;
        auto *raw = static_cast<toml::value<toml::date_time>*>(node_ptr.release());
        tmp.insert_or_assign(key, std::move(*raw));
        delete raw;
        ss << tmp;
        return;
    }

    // Unknown type: just drop (safest)
    node_ptr.reset();
}

// Convert MATLAB struct to TOML string preserving input field order
toml::table convert_struct_to_table_dummy(const mxArray* mx_struct) {
    // not used; kept for compatibility if needed
    return toml::table{};
}

// Convert MATLAB cell array to TOML array (same as before)
toml::array convert_cell_to_array(const mxArray* mx_cell) {
    toml::array arr;
    mwSize num_elements = mxGetNumberOfElements(mx_cell);
    for (mwSize i = 0; i < num_elements; ++i) {
        mxArray* element = mxGetCell(mx_cell, i);
        if (element && !mxIsEmpty(element)) {
            auto node_ptr = convert_mx_to_node(element);
            if (node_ptr) {
                // move into arr
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

// Convert MATLAB types to concrete toml::node instances (unique_ptr)
std::unique_ptr<toml::node> convert_mx_to_node(const mxArray* mx) {
    if (!mx || mxIsEmpty(mx))
        return nullptr;

    if (mxIsStruct(mx)) {
        // Build a toml::table representing this struct (we will move it later)
        toml::table tbl;
        int num_fields = mxGetNumberOfFields(mx);
        // Use MATLAB field order
        std::vector<std::string> field_names;
        field_names.reserve(num_fields);
        for (int i = 0; i < num_fields; ++i)
            field_names.emplace_back(mxGetFieldNameByNumber(mx, i));
        for (const auto &name : field_names) {
            mxArray* fv = mxGetField(mx, 0, name.c_str());
            if (fv && !mxIsEmpty(fv)) {
                auto node_ptr = convert_mx_to_node(fv);
                if (node_ptr) {
                    // move concrete into tbl under name
                    if (auto t = node_ptr->as_table()) {
                        toml::table* raw = static_cast<toml::table*>(node_ptr.release());
                        tbl.insert_or_assign(name, std::move(*raw));
                        delete raw;
                    }
                    else if (auto a = node_ptr->as_array()) {
                        toml::array* raw = static_cast<toml::array*>(node_ptr.release());
                        tbl.insert_or_assign(name, std::move(*raw));
                        delete raw;
                    }
                    else if (auto s = node_ptr->as_string()) {
                        auto *raw = static_cast<toml::value<std::string>*>(node_ptr.release());
                        tbl.insert_or_assign(name, std::move(*raw));
                        delete raw;
                    }
                    else if (auto i = node_ptr->as_integer()) {
                        auto *raw = static_cast<toml::value<int64_t>*>(node_ptr.release());
                        tbl.insert_or_assign(name, std::move(*raw));
                        delete raw;
                    }
                    else if (auto f = node_ptr->as_floating_point()) {
                        auto *raw = static_cast<toml::value<double>*>(node_ptr.release());
                        tbl.insert_or_assign(name, std::move(*raw));
                        delete raw;
                    }
                    else if (auto b = node_ptr->as_boolean()) {
                        auto *raw = static_cast<toml::value<bool>*>(node_ptr.release());
                        tbl.insert_or_assign(name, std::move(*raw));
                        delete raw;
                    }
                    else {
                        node_ptr.reset();
                    }
                }
            }
        }
        return std::make_unique<toml::table>(std::move(tbl));
    }

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

// Main MEX entry
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    if (nrhs != 1) {
        mexErrMsgIdAndTxt("toml_write_string:invalidArgs", "Usage: toml_str = toml_write_string(struct)");
    }
    if (nlhs > 1) {
        mexErrMsgIdAndTxt("toml_write_string:tooManyOutputs", "Too many output arguments");
    }
    if (!mxIsStruct(prhs[0])) {
        mexErrMsgIdAndTxt("toml_write_string:invalidInput", "Input must be a MATLAB struct");
    }

    try {
        const mxArray* s = prhs[0];
        int num_fields = mxGetNumberOfFields(s);
        std::vector<std::string> field_names;
        field_names.reserve(num_fields);
        for (int i = 0; i < num_fields; ++i)
            field_names.emplace_back(mxGetFieldNameByNumber(s, i));

        std::ostringstream ss;

        bool first_field = true;
        for (const auto &fname : field_names) {
            mxArray* fv = mxGetField(s, 0, fname.c_str());
            if (!fv || mxIsEmpty(fv)) continue;

            // Add a blank line between top-level entries (but not before the first)
            if (!first_field) ss << "\n";
            first_field = false;

            auto node_ptr = convert_mx_to_node(fv);
            if (!node_ptr) continue;

            // If the node is a string scalar, force double quotes and write manually
            if (node_ptr->is_value() && node_ptr->as_string()) {
                // extract string then delete node
                auto *raw = static_cast<toml::value<std::string>*>(node_ptr.release());
                std::string v = raw->get();
                delete raw;
                ss << fname << " = \"" << escape_for_double_quotes(v) << "\"";
            } else {
                // For other types, create a temporary table with a single key and stream it
                append_single_key_serialized(ss, fname.c_str(), node_ptr);
            }
        }

        std::string toml_string = ss.str();
        plhs[0] = mxCreateString(toml_string.c_str());
    }
    catch (const std::exception &e) {
        std::string error_msg = "Error creating TOML: ";
        error_msg += e.what();
        mexErrMsgIdAndTxt("toml_write_string:error", error_msg.c_str());
    }
}
