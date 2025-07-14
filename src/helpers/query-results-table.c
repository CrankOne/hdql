#include "hdql/helpers/query-results-table.h"

#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/compound.h"
#include "hdql/value.h"
#include "hdql/attr-def.h"

#include <assert.h>

static int
_recursive_call_on_attrs( const struct hdql_AttrDef * def
            , const char * prefix
            , char delimiter
            , int (*func)(const char*, void *), void * userdata
            ) {
    int rc = 0;
    if(hdql_attr_def_is_compound(def)) {
        /* Compound appends column names with choosen delimiter */
        const struct hdql_Compound * compound = hdql_attr_def_compound_type_info(def);
        do {
            size_t nAttrs = hdql_compound_get_nattrs(compound);
            const char ** attrNames = (const char **) alloca(sizeof(char*) * nAttrs);
            hdql_compound_get_attr_names(compound, attrNames);
            for (size_t i = 0; i < nAttrs; ++i) {
                const char * aName = attrNames[i];
                const struct hdql_AttrDef * subDef = hdql_compound_get_attr(compound, aName);
                char buf[256];
                if(*prefix != '\0') {
                    snprintf(buf, sizeof(buf), "%s%c%s", prefix, delimiter, aName);
                } else {
                    strcpy(buf, aName);
                }
                rc = _recursive_call_on_attrs(subDef, buf, delimiter
                        , func, userdata);
                if(rc != 0) {
                    /* Should never happen */
                    fputs("Exit from recursive call"
                          " on query attributes due to previous error."
                          , stderr
                          );
                    return rc;
                }
            }
            if(hdql_compound_is_virtual(compound))
                compound = hdql_virtual_compound_get_parent(compound);
            else
                compound = NULL;
        } while(compound);
        return rc;
    }
    if(0 != (rc = func(prefix, userdata))) {
        fprintf(stderr, "Recursive callback for query attribute returned"
                " %d, exit.\n", rc);
    }
    return 0;
}

struct ColumnNamesCollectionState {
    size_t nMaxColumns;
    size_t count;
    char ** namesPtr;
};

static int
_push_column_name( const char * path
        , void * columnNamesCollectionState_
        ) {
    struct ColumnNamesCollectionState * s
        = (struct ColumnNamesCollectionState *) columnNamesCollectionState_;
    if(s->count >= s->nMaxColumns) {
        fprintf(stderr, "Can't allocate column name #%zu as new"
                " column will exceed foreseen limit per record.\n"
                , s->count );
        return -1;
    }
    s->namesPtr[(s->count)++] = strdup(path);
    s->namesPtr[ s->count   ] = NULL;
    return 0;
}

int
hdql_stream_column_names( struct hdql_Query * q
            , char ** names
            , size_t * nColumns
            , char delimiter
            ) {
    const struct hdql_AttrDef * topAttrDef = hdql_query_top_attr(q);
    assert(topAttrDef);

    struct ColumnNamesCollectionState cs;
    cs.nMaxColumns = *nColumns;
    cs.count = 0;
    cs.namesPtr = names;

    _recursive_call_on_attrs(topAttrDef, ""
            , delimiter
            , _push_column_name, &cs
            );
    return 0;
}

#if 0
/**\brief Populates table with query result using given iface
 *
 * The function does not set columns, neither key (views).
 *
 * \p query is the query in use
 * \p t is the "table" (abstract) instance to populate
 * \p columns a NULL-terminated list of ordered column names to populate
 * \p context is the query context in use
 *
 * \return -1 if columns is empty or null
 * */
int
hdql_populate_records( struct hdql_Query * q
                     , struct iQueryResultsTable * t
                     , const char ** columns
                     , hdql_CollectionKey * keys
                     , struct hdql_Context * ctx
                     , char columnDelimiter
                     //, const struct hdql_Types * valTypes
                     ) {
    if(!columns) return -1;  /*Error: no user-provided columns to dump*/
    const hdql_AttrDef * topAttrDef = hdql_query_top_attr(q);
    assert(topAttrDef);
    void * rPtr;
    while(NULL != (rPtr = hdql_query_get(q, keys, ctx))) {
        /* flatten result with recursive function */
        _populate_record( topAttrDef, ""
                    , 
                    );
        /* finalize row, if need */
        if(t->finalize_row) t->finalize_row(t->userdata);
    }

    return 0;
}
#endif

