#include "hdql/helpers/query-results-table.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/errors.h"
#include "hdql/attr-def.h"
#include "hdql/query.h"
#include "hdql/types.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct hdql_QueryResultsWorkspace {
    struct hdql_Query * q;
    struct hdql_Context * ctx;
    struct hdql_iQueryResultsHandler * iqr;

    /* Keys pointer */
    hdql_CollectionKey * keys;
    /* Number of key flat views */
    size_t flatKeyViewLength;
    /* Keys flat views array */
    hdql_KeyView * kv;
};

/* Init workspace with query columns
 *
 * With given query's top attribute definition, retrieves the resulting type
 * and for every selected column forwards a call to
 * interface's `handle_attr_def()`.
 *
 * If \p columns is null, then either single column titled as NULL string will
 * be expected (when query result is scalar value) or all scalar columns of a
 * resulting compound will be obtained.
 * */
static int
_query_result_table_init_columns( const struct hdql_AttrDef * ad
        , const char ** attrs
        , struct hdql_iQueryResultsHandler * iqr
        , struct hdql_Context * ctx
        ) {
    int rc;
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
        const char ** selectedColumns = attrs ? attrs : attrNames;
        for( const char ** colNamePtr = selectedColumns
                ; *colNamePtr; ++colNamePtr ) {
            const struct hdql_AttrDef * cAD
                = hdql_compound_get_attr(c, *colNamePtr);
            rc = iqr->handle_attr_def(*colNamePtr, cAD, iqr->userdata);
            if(rc < 0) return rc;
        }
    } else {
        /* query results in scalar value */
        assert(hdql_attr_def_is_atomic(ad));
        const char * singularColName;
        if(attrs && attrs[0] && attrs[0][0] != '\0') {
            if(attrs[1] && attrs[1][0] != '\0') {
                /* This means, that more than one column name is provided,
                 * while query results in a singular value. Most probably
                 * indicates error. */
                return HDQL_ERR_GENERIC;
            }
        } else {
            singularColName = "";
        }
        rc = iqr->handle_attr_def(singularColName, ad, iqr->userdata);
        if(rc < 0) return rc;
    }
    return 0;
}

/* Allocates structure to track keys with their flattened views
 *
 * Forwards call to interface's `handle_keys()`.
 *
 * \note Entire query object is required since full keys chain allocation
 *       implies recursive traversal.
 * */
static int
_query_result_table_init_keys( struct hdql_QueryResultsWorkspace * ws ) {
    ws->keys = NULL;
    int rc = hdql_query_keys_reserve(ws->q, &ws->keys, ws->ctx);
    assert(rc == 0);
    ws->flatKeyViewLength = hdql_keys_flat_view_size(ws->keys, ws->ctx);
    ws->kv = ws->flatKeyViewLength
                      ? (hdql_KeyView *) malloc(sizeof(hdql_KeyView)*ws->flatKeyViewLength)
                      : NULL;
    hdql_keys_flat_view_update(ws->q, ws->keys, ws->kv, ws->ctx);
    assert(ws->iqr->handle_keys);
    return ws->iqr->handle_keys(ws->keys, ws->kv, ws->flatKeyViewLength, ws->iqr->userdata);
}



struct hdql_QueryResultsWorkspace *
hdql_query_results_init(
          struct hdql_Query * q
        , const char ** attrs
        , struct hdql_iQueryResultsHandler * iqr
        , struct hdql_Context * ctx
        ) {
    int rc;
    struct hdql_QueryResultsWorkspace * ws = (struct hdql_QueryResultsWorkspace *)
        hdql_context_alloc(ctx, sizeof(struct hdql_QueryResultsWorkspace));

    ws->q   = q;
    ws->ctx = ctx;
    ws->iqr = iqr;

    if(iqr->handle_attr_def) {
        const struct hdql_AttrDef * ad = hdql_query_top_attr(q);
        assert(ad);
        if(0 != (rc = _query_result_table_init_columns(ad, attrs, iqr, ctx))) {
            /* TODO forward error */
            return NULL;
        }
    }
    if(iqr->handle_keys) {
        if(0 != (rc = _query_result_table_init_keys(ws))) {
            /* TODO forward error */
            return NULL;
        }
    }
    if(iqr->finalize_schema) iqr->finalize_schema(iqr->userdata);
    return ws;
}

int
hdql_query_results_reset( struct hdql_Datum * d, struct hdql_QueryResultsWorkspace * ws ) {
    return hdql_query_reset(ws->q, d, ws->ctx);
}

int
hdql_query_results_advance( struct hdql_QueryResultsWorkspace * ws ) {
    hdql_Datum_t r;
    if(NULL == (r = hdql_query_get(ws->q, ws->keys, ws->ctx))) {
        ws->iqr->handle_record(r, ws->iqr->userdata);
        return 0;
    }
    return 1;
}

int
hdql_query_results_destroy( struct hdql_QueryResultsWorkspace * ) {
    return -1;  // TODO
}

/*
 * Prints query results as CSV
 */

struct hdql_CSVHandler;  /* fwd */

struct hdql_CSVColumnHandler {
    /** Attribute entry definition */
    const hdql_AttrDef * ad;
    /** Prints columns based on attribute definition and given datum,
     * uses CSV handler's formatting options and stream */
    void (*handler)( struct hdql_CSVHandler *
            , hdql_Datum_t
            , const hdql_AttrDef * ad
            );
    /** NULL-terminated list of column names yielded by this attribute. */
    const char ** columnNames;
};

struct hdql_CSVHandler {
    /** destination stream to print data */
    FILE * dest;
    /** context is needed to expand compound columns */
    struct hdql_Context * ctx;
    /** Array of attribute handlers, in order */
    size_t nAttrs  /**< overall attributes to handle */
         , nColumns  /**< overall columns count */
         ;
    struct hdql_CSVColumnHandler ** attributeHandlers;
};

static void
_csv_print_column(struct hdql_CSVHandler * csv
        , const char * columnString ) {
    /* TODO: column formatting options, delimiter, etc */
    fputs(columnString, csv->dest);
    fputs(",", csv->dest);
}

static int
_add_column_handler( struct hdql_CSVHandler * csv
        , struct hdql_CSVColumnHandler * ch
        ) {
    static const size_t chs = sizeof(struct hdql_CSVColumnHandler *);
    if(csv->attributeHandlers) {
        assert(csv->nAttrs != 0);
        struct hdql_CSVColumnHandler ** newArr = (struct hdql_CSVColumnHandler **)
            malloc(chs*(csv->nAttrs + 1));
        if(!newArr) return HDQL_ERR_MEMORY;
        mempcpy(newArr, csv->attributeHandlers, chs*(csv->nAttrs));
        newArr[csv->nAttrs] = ch;
        free(csv->attributeHandlers);
        csv->attributeHandlers = newArr;
    } else {
        assert(csv->nAttrs == 0);
        csv->attributeHandlers = (struct hdql_CSVColumnHandler **)
                malloc(chs);
        if(csv->attributeHandlers) return HDQL_ERR_MEMORY;
        csv->attributeHandlers[0] = ch;
    }
    ++(csv->nAttrs);
    return HDQL_ERR_CODE_OK;
}


static void _handle_collection_as_csv_entry(      struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, const hdql_AttrDef * ad);
static void _handle_scalar_compound_as_csv_entry( struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, const hdql_AttrDef * ad);


static int
_csv_results_handler_handle_attr_def(const char * columnName
        , const struct hdql_AttrDef * ad, void * csv_ ) {
    struct hdql_CSVHandler * csv = (struct hdql_CSVHandler *) csv_;
    if(hdql_attr_def_is_collection(ad)) {
        /* for collection columns we only print number of items
         * in the list */
        struct hdql_CSVColumnHandler * ch = (struct hdql_CSVColumnHandler *)
                malloc(sizeof(struct hdql_CSVColumnHandler));

        ch->ad = ad;
        ch->handler = _handle_collection_as_csv_entry;

        ch->columnNames = (const char **) malloc(sizeof(const char*)*2);
        ch->columnNames[0] = columnName;
        ch->columnNames[1] = NULL;

        _add_column_handler(csv, ch);

        ++(csv->nColumns);
    } else if(hdql_attr_def_is_compound(ad)) {
        /* Non-collection compound attr is expanded into a set of
         * attrname.sub-attrname columns */
        /* TODO: extend columns array with compound attribute handler */
        // csv->nColumns += nCompoundColumns;
    } else if(hdql_attr_def_is_atomic(ad)) {
        ++(csv->nColumns);
    }
}

/*
 * Attribute handlers
 */

/* Collection attribute yields only items count in CSV output */
static void
_handle_collection_as_csv_entry( struct hdql_CSVHandler * csv
        , hdql_Datum_t ownerDatum
        , const hdql_AttrDef * ad
        ) {
    assert(hdql_attr_def_is_collection(ad));
    assert(!hdql_attr_def_is_scalar(ad));
    const struct hdql_CollectionAttrInterface * ciface
        = hdql_attr_def_collection_iface(ad);
    assert(ciface);

    /* count items */
    size_t nItems = 0;
    hdql_It_t it = ciface->create(ownerDatum, ciface->definitionData
            , NULL, csv->ctx);
    assert(it);
    ciface->reset(it, ownerDatum, ciface->definitionData, NULL, csv->ctx);
    while(NULL != (it = ciface->advance(it))) {++nItems;}

    char buf[32];
    snprintf(buf, sizeof(buf), "%zu", nItems);
    _csv_print_column(csv, buf); 
}



/* Scalar compound attribute yields few columns recursively */
static void
_handle_scalar_compound_as_csv_entry( struct hdql_CSVHandler * csv
        , hdql_Datum_t ownerDatum
        , const hdql_AttrDef * ad
        ) {
    assert(!hdql_attr_def_is_collection(ad));
    assert(hdql_attr_def_is_scalar(ad));
    assert(hdql_attr_def_is_compound(ad));
    assert(!hdql_attr_def_is_static_value(ad));
    assert(!hdql_attr_def_is_atomic(ad));

    const struct hdql_ScalarAttrInterface * siface
        = hdql_attr_def_scalar_iface(ad);
    assert(siface);

    // TODO: need preallocated `definitionData' supplied by some reentrant item
    //_csv_print_column(csv, buf); 
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

