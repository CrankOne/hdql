#include "hdql/helpers/query-results-table.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/query-tree.h"
#include "hdql/attr-def.h"
#include "hdql/types.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct {
    // ...
};

/**\brief 
 *
 * A query can be of a scalar type, or a compound type from which
 * scalar attributes are requested.
 *
 * If \p columns is null, then either single column titled as "value" will be
 * expected (when query result is scalar value) or all scalar columns of a
 * resulting compound will be obtained.
 * */
int
hdql_query_result_table_init( struct hdql_AttrDef * ad
        , const char ** columns
        , struct hdql_iQueryResultsTable * iqr
        ) {
    if(hdql_attr_def_is_compound(ad)) {
        /* iterate over (selected) compound attributes and let the object
         * behind the interface know, in which order we are going to provide
         * values and of which type */
        const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
        size_t nAttrs = hdql_compound_get_nattrs(c);
        assert(nAttrs > 0);
        const char ** attrNames = (const char **) alloca(sizeof(char*)*(nAttrs+1));
        hdql_compound_get_attr_names(c, attrNames);
        attrNames[nAttrs] = NULL;
        const char ** selectedColumns = columns ? columns : attrNames;
        for( const char ** colNamePtr = selectedColumns
                ; *colNamePtr; ++colNamePtr ) {
            const struct hdql_AttrDef * cAD
                = hdql_compound_get_attr(c, *colNamePtr);
            iqr->handle_column_definition(*colNamePtr, cAD, iqr->userdata);
        }
    } else {
        /* query results in scalar value */
        assert(hdql_attr_def_is_atomic(ad));
        const char * singularColName;
        if(columns && columns[0] && columns[0][0] != '\0') {
            if(columns[1] && columns[1][0] != '\0') {
                /* This means, that more than one column name is provided,
                 * while query results in a singular value. Most probably
                 * indicates error. */
                return HDQL_ERR_GENERIC;
            }
        } else {
            singularColName = "value";
        }
        iqr->handle_column_definition(singularColName, ad, iqr->userdata);
    }
    if(iqr->columns_defined) iqr->columns_defined(iqr->userdata);
    return HDQL_ERR_CODE_OK;
}

#if 0
struct hdql_QueryTreeNode *
hdql_query_results_table_init( const char * qstr, const char ** sqstr
        , struct hdql_Compound * rootCompound
        , hdql_Context_t ctx_
        , void(*errcb)(const char *, size_t, const int *, void *), void * userdata
        ) {
    /* creat child context */
    struct hdql_Context * ctx = hdql_context_create_descendant(ctx_, 0x0);
    /* interpret root query */
    int rc = 0;
    struct hdql_Query * rootQuery; {
        char * expCpy = (char*) alloca(strlen(qstr)+1);
        assert(expCpy);
        strcpy(expCpy, qstr);
        char errBuf[256] = "";
        int errDetails[5] = {0, -1, -1, -1, -1};  // error code, first line, first column, last line, last column
        rootQuery = hdql_compile_query( qstr
                              , rootCompound
                              , ctx
                              , errBuf, sizeof(errBuf)
                              , errDetails
                              );
        rc = errDetails[0];
        if(rc) {
            errcb( "unable to interpret root query expression"
                 , SIZE_MAX, errDetails, userdata);
            return NULL;
        }
    }
    assert(rootQuery);
    /* Depending on the expression, "attribute definition" (which is
     * essentially a type) can describe either atomic value, or (possibly
     * virtual) compound. It is never collection. */
    const struct hdql_AttrDef * rootQueryResultAD = hdql_query_top_attr(rootQuery);
    assert(rootQueryResultAD);
    assert(!hdql_attr_def_is_collection(rootQueryResultAD));
    /* allocate new tree tier node */
    struct hdql_QueryTreeNode * qtn
            = (struct hdql_QueryTreeNode *)
                    hdql_context_alloc(ctx, sizeof(struct hdql_QueryTreeNode));
    /* set node's root query */
    qtn->q = rootQuery;
    /* reserve keys */
    qtn->keys = NULL;
    rc = hdql_query_keys_reserve(qtn->q, &qtn->keys, ctx);
    assert(rc == 0);
    /* set node's context */
    qtn->ctx = ctx;
    /* configure root value receiver TODO */
    //qtn->recv->handle_record_keys =
    //qtn->recv->handle_record_data =

    /* Check that for root query resulting in scalar value, sub-queries are not
     * specified. If ok, we're done */
    if(!hdql_attr_def_is_compound(rootQueryResultAD)) {
        /* root query results in a scalar value */
        if(sqstr && sqstr[0] != NULL) {
            errcb("root query expression results in scalar value, while"
                    " sub-queries are specified"
                 , SIZE_MAX, NULL, userdata );
            return NULL;
        }
        qtn->sqn = NULL;
    }
    /* count sub-queries */
    size_t nSubQs = 0;
    for(const char ** cc = sqstr; *cc; ++cc, ++nSubQs) {}
    /* allocate pointers array for nodes with sub-queries */
    qtn->sqn = (struct hdql_QueryTreeNode **) hdql_context_alloc(ctx
            , sizeof(struct hdql_QueryTreeNode *)*(nSubQs+1));
    qtn->sqn[nSubQs] = NULL;
    /* having top attribute definition for a root query proceed with
     * sub-queries (single query tree tier) */
    rc = hdql_query_interpret_subordiantes(rootQueryResultAD
            , sqstr, qtn->sqn, ctx
            , 
            );

    return qtn;
}
#endif


#if 0
typedef struct {
    void * userdata;
    int (*handle_record_keys)(struct hdql_CollectionKey *, void * userdata);
    int (*handle_record_data)(hdql_Datum_t, void * userdata);
} QueryResultsReceiver;

static int
_query_table_advance_record(
          struct hdql_Query * q
        , QueryResultsReceiver * recv
        , struct hdql_CollectionKey * keys
        , struct hdql_Context * ctx
        ) {
    int rc;
    /* get new data entry and return "no data" code, if failed */
    hdql_Datum_t r;
    if(NULL == (r = hdql_query_get(q, keys, ctx))) return HDQL_ERR_CODE_NO_DATA;

    if(keys && recv->handle_record_keys) {
        if(HDQL_ERR_CODE_OK != (rc = recv->handle_record_keys(keys, recv->userdata))) {
            return rc;
        }
    }

    /* how to proceed with the new record depends on the type of result. Few
     * options are possible:
     * - query result is a scalar atomic value. Then receiver should accept it
     *   and that's it.
     * - it is a compound type. Then receiver sohuld consider more complex
     *   logic with selective acquiziton of columns, etc.
     * We do not rely on the query introspection information here (only verify
     * it in debug builds) as this function is meant to be performant and be
     * part of more complex routine checking consistency in advance.
     * */
    return recv->handle_record_data(r, recv->userdata);
}

/*
 * Atomic value handling receiver implementation
 */

typedef struct {
    int (*handle_atomic_value)(hdql_Datum_t);
} QueryAtomicResultsReceiver;

static int
_handle_atomic_record(hdql_Datum_t datum, void * userdata) {
    QueryAtomicResultsReceiver * recv = (QueryAtomicResultsReceiver *) userdata;
    return recv->handle_atomic_value(datum);
}

/*
 * Compound value handling receiver implementation
 */

typedef struct {
    /* list of value-getting queries, in order
     * Code that composes these elements, must make sure that every query is 
     * */
    struct hdql_ValueInterface * columnsSelection;

    int (*handle_compound_value)(hdql_Datum_t);
} QueryCompoundResultsReceiver;

static int
_handle_compound_record(hdql_Datum_t datum, void * userdata) {
    QueryCompoundResultsReceiver * recv = (QueryCompoundResultsReceiver *) userdata;
    for(int * nColPtr = recv->columnsSelection; *nColPtr >= 0; ++nColPtr) {
        
    }
    //return recv->handle_compound_value(datum);
}

#endif

