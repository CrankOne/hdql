#pragma once

#include <string>
#include <list>
#include <variant>

#include "hdql/helpers/query.hh"

namespace hdql {
namespace helpers {

/**\brief Sink abstraction for tabular query result 
 * */
struct iQueryResultsTable {
    /// Value type codes
    enum ColumnDataTypeCode : char {
        kUnknown = 0,
        kBool = 1,
        kInt = 2,
        kFloat = 3,
        kString = 4
    };
    using ColumnDef = std::pair<ColumnDataTypeCode, std::string>;
    using Value     = std::variant<int, float, std::string>;
    virtual ~iQueryResultsTable() = default;
    /// Sets header of the table as list of column names and expected data types
    virtual void columns(const std::list<ColumnDef> &) = 0;
    /// Adds value to the current row
    virtual void add_value(const Value &) = 0;
    /// Finalizes current row
    virtual void finalize_row() = 0;
    /// Flush or finalize stream (optional)
    virtual void finalize() {}
};  // class iQueryResultsTable

namespace aux {
/// Utility function to flatten compound query result
void flatten_compound_to_row( const hdql_Compound * compound,
                                  const hdql_Datum * datum,
                                  hdql_Context * ctx,
                                  iQueryResultsTable & sink,
                                  const std::vector<std::string> & orderedNames,
                                  char delimiter,
                                  const std::string & prefix = "");
}  // namespace ::hdql::helpers::aux

template<typename RootT>
void dump_query_results( Query & q
                       , RootT & root
                       , iQueryResultsTable & sink
                       , const std::vector<std::string> & columnsOrder = {}
                       , char recurseDelimiter = '.'
                       ) {
    if (!q.is_compound()) {
        throw std::runtime_error("Only compound query results are currently supported");
    }

    std::vector<std::string> keyNames = q.key_names();
    std::vector<std::string> valueNames = q.names(recurseDelimiter);

    std::vector<std::string> allColumns = keyNames;
    if (!columnsOrder.empty())
        allColumns.insert(allColumns.end(), columnsOrder.begin(), columnsOrder.end());
    else
        allColumns.insert(allColumns.end(), valueNames.begin(), valueNames.end());
    // columnar output schema
    std::list<iQueryResultsTable::ColumnDef> schema;
    for (const auto & name : allColumns) {
        schema.emplace_back(iQueryResultsTable::kString, name);  // use kString as fallback
    }

    sink.columns(schema);

    GenericQueryCursor cursor = q.generic_cursor_on(root);
    hdql_Context * ctx = q._ownContext;
    const hdql_Compound * compoundType = hdql_attr_def_compound_type_info(q._topAttrDef);
    const hdql_CollectionKey * keys = q.keys();

    std::vector<hdql_KeyView> keyViews(keyNames.size());
    hdql_keys_flat_view_update(q._query, keys, keyViews.data(), ctx);

    while (cursor) {
        // Insert key values
        for (const hdql_KeyView & kv : keyViews) {
            std::ostringstream oss;
            kv.interface->print(kv.keyPtr, oss);
            sink.add_value(oss.str());
        }

        // Insert compound result
        const hdql_Datum * d = reinterpret_cast<const hdql_Datum *>(q._r);
        aux::flatten_compound_to_row(compoundType, d, ctx, sink,
                                columnsOrder.empty() ? valueNames : columnsOrder,
                                recurseDelimiter);

        sink.finalize_row();
        ++cursor;
    }

    sink.finalize();
}

}  // namespace ::hdql::helpers
}  // namespace hdql

