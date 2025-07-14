#include "hdql/helpers/queryResultsTable.hh"

namespace hdql {
namespace helpers {
namespace aux {

void
flatten_compound_to_row( const hdql_Compound * compound
                       , const hdql_Datum * datum
                       , hdql_Context * ctx
                       , iQueryResultsTable & sink
                       , const std::vector<std::string> & orderedNames
                       , char delimiter
                       , const std::string & prefix
                        ) {
    size_t nattrs = hdql_compound_get_nattrs(compound);
    std::vector<const char *> attrNames(nattrs);
    hdql_compound_get_attr_names(compound, attrNames.data());

    std::unordered_map<std::string, const hdql_AttrDef *> attrDefs;
    for (const char * name : attrNames) {
        const hdql_AttrDef * ad = hdql_compound_get_attr(compound, name);
        std::string fullName = prefix.empty() ? name : (prefix + delimiter + name);
        attrDefs[fullName] = ad;
    }

    for (const std::string & name : orderedNames) {
        auto it = attrDefs.find(name);
        if (it == attrDefs.end()) {
            sink.add_value(std::string{});  // missing field
            continue;
        }

        const hdql_AttrDef * ad = it->second;
        const hdql_ValueInterface * vi = hdql_attr_def_get_interface(ad);
        const void * valuePtr = vi->get_ptr(datum, name.c_str());

        if (!valuePtr) {
            sink.add_value(std::string{});
            continue;
        }

        if (hdql_attr_def_is_compound(ad)) {
            throw std::runtime_error("Nested compound types not supported in flatten step (pre-flatten required)");
        } else if (hdql_attr_def_is_atomic(ad)) {
            switch (hdql_attr_def_get_atomic_value_type_code(ad)) {
                case 0x01:  // int
                    sink.add_value(*reinterpret_cast<const int *>(valuePtr));
                    break;
                case 0x02:  // float
                    sink.add_value(*reinterpret_cast<const float *>(valuePtr));
                    break;
                case 0x03:  // string
                    sink.add_value(std::string(reinterpret_cast<const char *>(valuePtr)));
                    break;
                default: {
                    std::ostringstream oss;
                    vi->print(valuePtr, oss);
                    sink.add_value(oss.str());
                    break;
                }
            }
        } else {
            sink.add_value(std::string{"<?>"});  // unknown type
        }
    }
}
}  // namespace ::hdql::helkpers::aux

}  // namespace ::hdql::helpers
}  // namespace hdql

