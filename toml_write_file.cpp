/*
 * toml_write_file.cpp  — Safe & working version for toml++ v3
 *
 * Compile with:
 *   mex -R2018a CXXFLAGS="$CXXFLAGS -std=c++17" -I/path/to/tomlplusplus/include toml_write_file.cpp
 *
 * Usage in MATLAB:
 *   data.name = 'Alice';
 *   data.age = 25;
 *   data.server.host = 'localhost';
 *   data.server.ports = [8080, 8081, 8082];
 *   toml_write_file(data, 'config.toml');
 */

#include "mex.h"
#include <toml++/toml.h>
#include <string>
#include <memory>
#include <cmath>
#include <climits>
#include <fstream>

// Forward declaration
std::unique_ptr<toml::node> convert_mx_to_node(const mxArray* mx);

// Helper: move-and-delete raw pointer into table
template <typename Concrete>
void insert_concrete_into_table(toml::table &tbl, const char* key, std::unique_ptr<toml::node> &node_ptr)
{
    // take ownership of the concrete pointer
    Concrete* raw = static_cast<Concrete*>(node_ptr.release());
    // move into the table (uses Concrete's move constructor)
    tbl.insert(key, std::move(*raw));
    // delete the moved-from temporary object
    delete raw;
}

// Convert MATLAB struct to TOML table (preserve MATLAB field order)
toml::table convert_struct_to_table(const mxArray* mx_struct)
{
    toml::table tbl;
    int num_fields = mxGetNumberOfFields(mx_struct);

    for (int i = 0; i < num_fields; ++i)
    {
        const char* field_name = mxGetFieldNameByNumber(mx_struct, i);
        mxArray* field_value = mxGetFieldByNumber(mx_struct, 0, i);
        if (!field_value || mxIsEmpty(field_value))
            continue;

        auto node_ptr = convert_mx_to_node(field_value);
        if (!node_ptr) continue;

        // Identify concrete type and transfer ownership safely
        if (auto t = node_ptr->as_table())
        {
            insert_concrete_into_table<toml::table>(tbl, field_name, node_ptr);
        }
        else if (auto a = node_ptr->as_array())
        {
            insert_concrete_into_table<toml::array>(tbl, field_name, node_ptr);
        }
        else if (auto s = node_ptr->as_string())
        {
            insert_concrete_into_table<toml::value<std::string>>(tbl, field_name, node_ptr);
        }
        else if (auto i = node_ptr->as_integer())
        {
            insert_concrete_into_table<toml::value<int64_t>>(tbl, field_name, node_ptr);
        }
        else if (auto f = node_ptr->as_floating_point())
        {
            insert_concrete_into_table<toml::value<double>>(tbl, field_name, node_ptr);
        }
        else if (auto b = node_ptr->as_boolean())
        {
            insert_concrete_into_table<toml::value<bool>>(tbl, field_name, node_ptr);
        }
        else if (auto d = node_ptr->as_date())
        {
            insert_concrete_into_table<toml::value<toml::date>>(tbl, field_name, node_ptr);
        }
        else if (auto t = node_ptr->as_time())
        {
            insert_concrete_into_table<toml::value<toml::time>>(tbl, field_name, node_ptr);
        }
        else if (auto dt = node_ptr->as_date_time())
        {
            insert_concrete_into_table<toml::value<toml::date_time>>(tbl, field_name, node_ptr);
        }
        else
        {
            // Unknown / not supported concrete node type: skip
            // node_ptr will be destroyed automatically if not released
        }
    }

    return tbl;
}

// Convert MATLAB cell array to TOML array
toml::array convert_cell_to_array(const mxArray* mx_cell)
{
    toml::array arr;
    mwSize num_elements = mxGetNumberOfElements(mx_cell);
    for (mwSize i = 0; i < num_elements; ++i)
    {
        mxArray* element = mxGetCell(mx_cell, i);
        if (!element || mxIsEmpty(element)) continue;

        auto node_ptr = convert_mx_to_node(element);
        if (!node_ptr) continue;

        // Move concrete node into array similarly to table logic
        if (auto t = node_ptr->as_table())
        {
            toml::table* raw = static_cast<toml::table*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto a = node_ptr->as_array())
        {
            toml::array* raw = static_cast<toml::array*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto s = node_ptr->as_string())
        {
            auto *raw = static_cast<toml::value<std::string>*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto i = node_ptr->as_integer())
        {
            auto *raw = static_cast<toml::value<int64_t>*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto f = node_ptr->as_floating_point())
        {
            auto *raw = static_cast<toml::value<double>*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto b = node_ptr->as_boolean())
        {
            auto *raw = static_cast<toml::value<bool>*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto d = node_ptr->as_date())
        {
            auto *raw = static_cast<toml::value<toml::date>*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto t = node_ptr->as_time())
        {
            auto *raw = static_cast<toml::value<toml::time>*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else if (auto dt = node_ptr->as_date_time())
        {
            auto *raw = static_cast<toml::value<toml::date_time>*>(node_ptr.release());
            arr.push_back(std::move(*raw));
            delete raw;
        }
        else
        {
            // skip unsupported
        }
    }
    return arr;
}

// Convert MATLAB numeric array to TOML array
toml::array convert_numeric_array_to_toml(const mxArray* mx)
{
    toml::array arr;
    double* data = mxGetPr(mx);
    mwSize num_elements = mxGetNumberOfElements(mx);
    for (mwSize i = 0; i < num_elements; ++i)
    {
        double val = data[i];
        if (val == std::floor(val) && val >= INT64_MIN && val <= INT64_MAX)
            arr.push_back(static_cast<int64_t>(val));
        else
            arr.push_back(val);
    }
    return arr;
}

// Convert MATLAB type to concrete TOML node (unique_ptr to concrete object)
std::unique_ptr<toml::node> convert_mx_to_node(const mxArray* mx)
{
    if (!mx || mxIsEmpty(mx))
        return nullptr;

    if (mxIsStruct(mx))
        return std::make_unique<toml::table>(convert_struct_to_table(mx));

    if (mxIsCell(mx))
        return std::make_unique<toml::array>(convert_cell_to_array(mx));

    if (mxIsChar(mx))
    {
        char* str = mxArrayToString(mx);
        std::string result(str ? str : "");
        if (str) mxFree(str);
        return std::make_unique<toml::value<std::string>>(result);
    }

    if (mxIsLogical(mx))
    {
        mwSize num = mxGetNumberOfElements(mx);
        if (num == 1)
        {
            bool v = mxGetLogicals(mx)[0] != 0;
            return std::make_unique<toml::value<bool>>(v);
        }
        else
        {
            toml::array arr;
            mxLogical* data = mxGetLogicals(mx);
            for (mwSize i = 0; i < num; ++i)
                arr.push_back(data[i] != 0);
            return std::make_unique<toml::array>(std::move(arr));
        }
    }

    if (mxIsDouble(mx) || mxIsSingle(mx))
    {
        mwSize num_elements = mxGetNumberOfElements(mx);
        if (num_elements == 1)
        {
            double val = mxGetScalar(mx);
            if (val == std::floor(val) && val >= INT64_MIN && val <= INT64_MAX)
                return std::make_unique<toml::value<int64_t>>(static_cast<int64_t>(val));
            else
                return std::make_unique<toml::value<double>>(val);
        }
        else
        {
            return std::make_unique<toml::array>(convert_numeric_array_to_toml(mx));
        }
    }

    return nullptr;
}

// MEX entry point
void mexFunction(int nlhs, mxArray*[], int nrhs, const mxArray* prhs[])
{
    if (nrhs != 2)
        mexErrMsgIdAndTxt("toml_write_file:invalidArgs",
                          "Usage: toml_write_file(struct, filename)");

    if (!mxIsStruct(prhs[0]))
        mexErrMsgIdAndTxt("toml_write_file:invalidInput", "First input must be a struct");

    if (!mxIsChar(prhs[1]))
        mexErrMsgIdAndTxt("toml_write_file:invalidInput", "Second input must be a filename string");

    char* filename = mxArrayToString(prhs[1]);
    if (!filename)
        mexErrMsgIdAndTxt("toml_write_file:memoryError", "Could not allocate memory for filename");

    try
    {
        toml::table tbl = convert_struct_to_table(prhs[0]);

        std::ofstream ofs(filename);
        if (!ofs)
        {
            mxFree(filename);
            mexErrMsgIdAndTxt("toml_write_file:error", "Could not open file for writing");
        }

        ofs << tbl;
        ofs.close();
    }
    catch (const std::exception& e)
    {
        mxFree(filename);
        std::string error_msg = "Error writing TOML file: ";
        error_msg += e.what();
        mexErrMsgIdAndTxt("toml_write_file:error", error_msg.c_str());
    }

    mxFree(filename);
}
