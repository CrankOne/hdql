#include "hdql/helpers/query-results-handler-csv.h"

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/errors.h"
#include "hdql/query.h"
#include "hdql/types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if 0
#   define _M_DBGMSG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#   define _M_DBGMSG(fmt, ...)
#endif

struct hdql_CSVHandler;  /* fwd */
struct hdql_CSVAttrHandler;  /* fwd */
struct hdql_AttrHandlerTier;  /* fwd */

/* recursive struct defining one level attribute handlers */
struct hdql_AttrHandlerTier {
    size_t n;  /* number handlers at this level */
    struct hdql_CSVAttrHandler ** handlers;
};

/* Handles individual attributes in query result. May expand to multiple
 * columns (e.g. for scalar compound attributes) */
struct hdql_CSVAttrHandler {
    const char * name;

    /* Attribute entry definition */
    const struct hdql_AttrDef * ad;

    /* Prints columns based on attribute definition and given datum,
     * uses CSV handler's formatting options and stream */
    void (*value_handler)( struct hdql_CSVHandler *, hdql_Datum_t, struct hdql_CSVAttrHandler * ad);

    /* runtime short-lived state, used to dereference scalar or collection
     * attributes (depends on iface) */
    union {
        hdql_Datum_t scSupp;
        hdql_It_t collectionIt;
    } dynamicData;

    union {
        /* callback used to stringify atomic scalar value */
        int (*get_as_string)(const hdql_Datum_t, char * buf, size_t bufSize, hdql_Context_t);
        /* child tiers to stringify compounds */
        struct hdql_AttrHandlerTier attrHandlers;
    } payload;
};

/* Userdata for `hdql_iQueryResultsHandler` defining how to actually handle
 * attributes from a query result. */
struct hdql_CSVHandler {
    /** destination stream to print data */
    FILE * dest;
    /** formatting assets */
    struct hdql_DSVFormatting fmt;

    /** context is needed to expand compound columns */
    struct hdql_Context * ctx;

    struct hdql_CSVAttrHandler rootObjectHandler;

    struct hdql_KeyView * flatKeyViews;
    size_t flatKeyViewLength;

    /* internal printer state */
    size_t nColumnsPrinted;
};



/*
 * Utility functions of handler tier struct
 */

/* utility function extending attribute handlers collected in tier with
 * given item */
static int
_push_back_handler( struct hdql_AttrHandlerTier * t, struct hdql_CSVAttrHandler * h ) {
    static const size_t chs = sizeof(struct hdql_CSVAttrHandler *);
    if(t->handlers) {
        assert(t->n != 0);
        struct hdql_CSVAttrHandler ** newArr = (struct hdql_CSVAttrHandler **)
            malloc(chs*(t->n + 1));
        if(!newArr) return HDQL_ERR_MEMORY;
        memcpy(newArr, t->handlers, chs*(t->n));
        newArr[t->n] = h;
        free(t->handlers);
        t->handlers = newArr;
    } else {
        assert(t->n == 0);
        t->handlers = (struct hdql_CSVAttrHandler **) malloc(chs);
        if(!t->handlers) return HDQL_ERR_MEMORY;
        t->handlers[0] = h;
    }
    ++(t->n);
    return HDQL_ERR_CODE_OK;
}

/*
 * Utility functions of CSV record handler main structure
 */

/* utility function: actually printing the column, respecting delimiters,
 * formatting, etc. */
static void
_csv_print_column(struct hdql_CSVHandler * csv
        , const char * columnString ) {
    if(csv->nColumnsPrinted) {
        fputs(csv->fmt.valueDelimiter, csv->dest);
    }
    fputs(columnString, csv->dest);
    ++(csv->nColumnsPrinted);
}


/*
 * Utility functions of CSV attribute handler
 */

/* forward definitions of attribute handlers to print attributes as values
 * within the CSV table record */
static void _handle_collection_as_csv_entry(      struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_CSVAttrHandler * ad);
static void _handle_scalar_compound_as_csv_entry( struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_CSVAttrHandler * ad);
static void _handle_scalar_atomic_as_csv_entry(   struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_CSVAttrHandler * ad);
static void _handle_scalar_atomic_value_as_csv_entry(   struct hdql_CSVHandler * csv, hdql_Datum_t ownerDatum, struct hdql_CSVAttrHandler * ad);

static void
_expand_scalar_compound_to_columns( const struct hdql_AttrDef * ad
        , struct hdql_AttrHandlerTier * t
        , struct hdql_Context * ctx
        );

static int
_assign_value_handler( const struct hdql_AttrDef * ad
        , struct hdql_CSVAttrHandler * ah
        , hdql_Context_t ctx
        ) {
    /* we dereference here top attr def for forwarding queries to use it
     * with decisions below. Note, that working with forwarding query
     * needs dedicated state management */
    const struct hdql_AttrDef * topAD = hdql_attr_def_top_attr(ad);

    if( hdql_attr_def_is_collection(topAD) ) {
        _M_DBGMSG("  top attr type is of collection type\n");
        ah->value_handler = _handle_collection_as_csv_entry;
    } else if(hdql_attr_def_is_atomic(topAD)) {
        _M_DBGMSG("  top attr type is of scalar atomic type\n");
        ah->value_handler = _handle_scalar_atomic_as_csv_entry;
        struct hdql_ValueTypes * valTypes = hdql_context_get_types(ctx);
        assert(valTypes);
        const struct hdql_ValueInterface * vi
              = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(topAD));
        /* TODO: unclear whether we will get rid of this method in favor of
         *  conversions, though */
        assert(vi->get_as_string);
        ah->payload.get_as_string = vi->get_as_string;
    } else if(hdql_attr_def_is_compound(topAD)) {
        _M_DBGMSG("  top attr type is of scalar compound type (expanding recursively)\n");
        ah->value_handler = _handle_scalar_compound_as_csv_entry;
        _expand_scalar_compound_to_columns(ad, &ah->payload.attrHandlers, ctx);
    }
    #ifndef DNDEBUG
    else {
        // EXAMPLE: `.hits{a:=.x, b:=.y, c:=.z}`
        char bf[128];
        hdql_top_attr_str(ad, bf, sizeof(bf), ctx);
        fputs("DEBUG ASSERTION FAILED: logic error: can't handle top level attr def of type `", stderr);
        fputs(bf, stderr);
        fputs("'.\n", stderr);
        assert(0);
    }
    #endif
    return 0;
}

static int
_extend_tier_with_attribute( struct hdql_AttrHandlerTier * t
            , const char * attrName
            , const struct hdql_AttrDef * ad
            , struct hdql_Context * ctx
            ) {
    assert(ctx);
    /* allocate new attribute handler */
    struct hdql_CSVAttrHandler * ah = (struct hdql_CSVAttrHandler *)
                malloc(sizeof(struct hdql_CSVAttrHandler));

    ah->name = attrName && '\0' != *attrName ? attrName : NULL;  //<- ...
    ah->value_handler = NULL;
    ah->ad = ad;
    memset(&ah->payload,     0x0, sizeof(ah->payload));
    memset(&ah->dynamicData, 0x0, sizeof(ah->dynamicData));

    _M_DBGMSG("  assigning handler to `%s'...\n", attrName ? attrName : "?");
    _assign_value_handler(ad, ah, ctx);

    /* push new initialized attribute handler */
    return _push_back_handler(t, ah);
}

/* builds a tree to recursively traverse scalar compound definitions,
 * utilizing CSV value handlers */
static void
_expand_scalar_compound_to_columns( const struct hdql_AttrDef * ad
        , struct hdql_AttrHandlerTier * t
        , struct hdql_Context * ctx
        ) {
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    const size_t nAttrs = hdql_compound_get_nattrs_recursive(c);
    const char ** attrNames = (const char **) alloca(sizeof(const char*)*nAttrs);
    hdql_compound_get_attr_names_recursive(c, attrNames);
    for(size_t nAttr = 0; nAttr < nAttrs; ++nAttr) {
        const struct hdql_AttrDef * subAD = hdql_compound_get_attr(c, attrNames[nAttr]);
        assert(subAD);
        _extend_tier_with_attribute(t, attrNames[nAttr], subAD, ctx);
    }
}

/* part of `hdql_iQueryResultsHandler` implementation for CSV handler,
 * matches `hdql_iQueryResultsHandler::handle_result_type()`.
 * */
static int
_csv_handler_set_result_type( const struct hdql_AttrDef * ad, void * csv_ ) {
    fflush(stderr);

    struct hdql_CSVHandler * csv = (struct hdql_CSVHandler *) csv_;
    assert(csv->rootObjectHandler.ad == NULL);  /* make sure it hasn't being called before */
    csv->rootObjectHandler.ad = ad;
    _M_DBGMSG("Assigning handler for root record type.\n");
    /* Special case here is when root type is a collection. Contrary to
     * in-depth iteration of the query result (assigned
     * by _assign_value_handler(), we expect query itslef to iterate over this
     * collection, so we must consider datum as a scalar item here. */
    if(hdql_attr_def_is_atomic(ad)) {
        /* get atomic value iface */
        struct hdql_ValueTypes * valTypes = hdql_context_get_types(csv->ctx);
        assert(valTypes);
        const struct hdql_ValueInterface * vi
                = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(ad));

        if(hdql_attr_def_is_scalar(ad)) {
            _M_DBGMSG("  root obj. top attr type is of scalar atomic type\n");
            csv->rootObjectHandler.value_handler = _handle_scalar_atomic_as_csv_entry;
            /* TODO: unclear whether we will get rid of this method in favor of
             *  conversions, though */
            assert(vi->get_as_string);
            csv->rootObjectHandler.payload.get_as_string = vi->get_as_string;
        } else {
            assert(hdql_attr_def_is_collection(ad));
            /* This is a special (and simplest possible) case when query
             * yields a series of scalars that this handler must not iterate
             * by itself. Just use this datum to stringify as is. */
            csv->rootObjectHandler.payload.get_as_string = vi->get_as_string;
            csv->rootObjectHandler.value_handler = _handle_scalar_atomic_value_as_csv_entry;
        }
    } else if(hdql_attr_def_is_compound(ad)) {
        _M_DBGMSG("  root obj. top attr type is of scalar compound type (expanding recursively)\n");
        csv->rootObjectHandler.value_handler = _handle_scalar_compound_as_csv_entry;
        _expand_scalar_compound_to_columns(ad, &csv->rootObjectHandler.payload.attrHandlers, csv->ctx);
    }
    #ifndef DNDEBUG
    else {
        char bf[128];
        hdql_top_attr_str(ad, bf, sizeof(bf), csv->ctx);
        fputs("DEBUG ASSERTION FAILED: logic error: can't handle top level attr def of query result"
                " yielding type `", stderr);
        fputs(bf, stderr);
        fputs("'.\n", stderr);
        assert(0);
    }
    #endif
    return 0;
}

/*
 * Attribute handlers
 */

/* Collection attribute yields only items count in CSV output */
static void
_handle_collection_as_csv_entry( struct hdql_CSVHandler * csv
        , hdql_Datum_t ownerDatum
        , struct hdql_CSVAttrHandler * h
        ) {
    assert(hdql_attr_def_is_collection(h->ad));
    assert(!hdql_attr_def_is_scalar(h->ad));
    const struct hdql_CollectionAttrInterface * ciface
        = hdql_attr_def_collection_iface(h->ad);
    assert(ciface);

    /* count items
     *
     * Note, that for non-empty collections, iterator is always not null, even
     * if it can not be further advanced. Proper way to check the end of
     * collection is to retrieve the value (dereference).
     * */
    size_t nItems = 0;
    if(h->dynamicData.collectionIt)
        h->dynamicData.collectionIt = ciface->reset( 
                  h->dynamicData.collectionIt
                , ownerDatum
                , ciface->definitionData
                , NULL
                , csv->ctx
                );
    while( h->dynamicData.collectionIt ) {  /* null is allowed for iterator (empty collections) */
        hdql_Datum_t check = ciface->dereference(h->dynamicData.collectionIt, NULL);
        if(!check) break;
        h->dynamicData.collectionIt = ciface->advance(h->dynamicData.collectionIt);
        ++nItems;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%zu", nItems);
    _csv_print_column(csv, buf); 
}

/* common part for both _handle_scalar_compound_as_csv_entry() and
 * _handle_scalar_atomic_as_csv_entry() */
static hdql_Datum_t
_get_scalar_data(struct hdql_CSVHandler * csv
        , hdql_Datum_t ownerDatum
        , struct hdql_CSVAttrHandler * h) {
    const struct hdql_ScalarAttrInterface * siface
        = hdql_attr_def_scalar_iface(h->ad);
    assert(siface);

    if(siface->reset) {
        h->dynamicData.scSupp = siface->reset(ownerDatum
                , h->dynamicData.scSupp
                , siface->definitionData
                , csv->ctx
                );
    }
    assert(siface->dereference);
    return siface->dereference( ownerDatum
            , h->dynamicData.scSupp
            , NULL
            , siface->definitionData
            , csv->ctx
            );
    /* note: we do not call destroy() here as siface->reset() has to already
     * take care of it */
}

/* Scalar atomic attribute yields single column with simple printing */
static void
_handle_scalar_atomic_as_csv_entry( struct hdql_CSVHandler * csv
        , hdql_Datum_t ownerDatum
        , struct hdql_CSVAttrHandler * h
        ) {
    assert(!hdql_attr_def_is_collection(h->ad));
    assert(hdql_attr_def_is_scalar(h->ad));
    assert(!hdql_attr_def_is_compound(h->ad));

    hdql_Datum_t r = _get_scalar_data(csv, ownerDatum, h);
    if(r) {    
        char buf[128];  /* TODO: configurable? */
        h->payload.get_as_string(r, buf, sizeof(buf), csv->ctx);
        _csv_print_column(csv, buf);
    } else {
        _csv_print_column(csv, csv->fmt.nullToken);
    }
}

static void
_handle_scalar_atomic_value_as_csv_entry( struct hdql_CSVHandler * csv
        , hdql_Datum_t valueDatum
        , struct hdql_CSVAttrHandler * h
        ) {
    char buf[128];  /* TODO: configurable? */
    h->payload.get_as_string(valueDatum, buf, sizeof(buf), csv->ctx);
    _csv_print_column(csv, buf);
}

static int _reset_dynamic_states(struct hdql_AttrHandlerTier * t, hdql_Datum_t datum, struct hdql_Context * ctx);

/* Scalar compound attribute yields few columns recursively */
static void
_handle_scalar_compound_as_csv_entry( struct hdql_CSVHandler * csv
        , hdql_Datum_t ownerDatum
        , struct hdql_CSVAttrHandler * h
        ) {
    assert(!hdql_attr_def_is_collection(h->ad));
    assert(hdql_attr_def_is_scalar(h->ad));
    assert(hdql_attr_def_is_compound(h->ad));
    assert(!hdql_attr_def_is_static_value(h->ad));
    assert(!hdql_attr_def_is_atomic(h->ad));

    /* Get item by dereferencing the owner with attribute definition */
    hdql_Datum_t r = _get_scalar_data(csv, ownerDatum, h);

    //assert(hdql_attr_def_is_compound(topAD))
    _reset_dynamic_states(&h->payload.attrHandlers
                        , r, csv->ctx);

    for(size_t i = 0; i < h->payload.attrHandlers.n; ++i) {
        struct hdql_CSVAttrHandler * ah = h->payload.attrHandlers.handlers[i];
        ah->value_handler(csv, r, ah);
    }
}

/* part of `hdql_iQueryResultsHandler` implementation for CSV handler,
 * matches `hdql_iQueryResultsHandler::handle_record()`.
 *
 * Extends set of individual attributes handlers based on compoound query
 * result attribute declaration, keeps track on the columns to print.
 * */
static int
_csv_results_handler_handle_record(hdql_Datum_t datum, void * csv_ ) {
    assert(csv_);
    struct hdql_CSVHandler * csv = (struct hdql_CSVHandler *) csv_;

    csv->nColumnsPrinted = 0;

    _M_DBGMSG("Handling new record.\n");

    if(csv->flatKeyViews && csv->flatKeyViewLength) {
        /* Print columns for keys; use <keyPrefix><N> for keys without a
         * label */
        for(size_t nFKV = 0; nFKV < csv->flatKeyViewLength; ++nFKV) {
            struct hdql_KeyView * kv = csv->flatKeyViews + nFKV;
            if(kv->interface && kv->interface->get_as_string) {
                char fkvBf[128];
                kv->interface->get_as_string(kv->keyPtr->pl.datum, fkvBf, sizeof(fkvBf), csv->ctx);
                _csv_print_column(csv, fkvBf);
            } else {
                _csv_print_column(csv, csv->fmt.nullToken);
            }
        }
    }

    assert(csv->rootObjectHandler.ad);
    if(hdql_attr_def_is_compound(csv->rootObjectHandler.ad)) {
        struct hdql_AttrHandlerTier * hsTier = &csv->rootObjectHandler.payload.attrHandlers;
        _reset_dynamic_states(hsTier, datum, csv->ctx);

        _M_DBGMSG("  record is of compound type, expecting %zu attrs\n", hsTier->n);
        for(size_t i = 0; i < hsTier->n; ++i) {
            assert(hsTier->handlers[i]->value_handler);
            _M_DBGMSG("  handling root compound's attr #%zu\n", i);
            hsTier->handlers[i]->value_handler(csv, datum
                    , hsTier->handlers[i] );
        }
    } else {
        assert(hdql_attr_def_is_atomic(csv->rootObjectHandler.ad));
        _M_DBGMSG("  record is of atomic type.\n");
        csv->rootObjectHandler.value_handler( csv
                , datum, &csv->rootObjectHandler);
    }
    fputs(csv->fmt.recordDelimiter, csv->dest);
    _M_DBGMSG("Record handled.\n");
    return 0;
}

static int
_reset_dynamic_states(struct hdql_AttrHandlerTier * t, hdql_Datum_t datum, struct hdql_Context * ctx) {
    for(size_t i = 0; i < t->n; ++i) {
        struct hdql_CSVAttrHandler * ah = t->handlers[i];

        const struct hdql_AttrDef * topAD = hdql_attr_def_top_attr(ah->ad);

        if(hdql_attr_def_is_collection(topAD)) {  /* collection */
            const struct hdql_CollectionAttrInterface * ciface
                = hdql_attr_def_collection_iface(ah->ad);
            assert(ciface);
            assert(ciface->reset);
            if((!ah->dynamicData.collectionIt) && ciface->create)
                ah->dynamicData.collectionIt = ciface->create( datum
                        , ciface->definitionData
                        , NULL  /* select all */
                        , ctx
                        );
            ah->dynamicData.collectionIt = ciface->reset( ah->dynamicData.collectionIt
                             , datum
                             , ciface->definitionData
                             , NULL /* select all */
                             , ctx
                             );
        } else {  /* scalar */
            assert(hdql_attr_def_is_scalar(topAD));
            const struct hdql_ScalarAttrInterface * siface = hdql_attr_def_scalar_iface(ah->ad);
            if(siface->instantiate) {
                if(NULL == ah->dynamicData.scSupp) {
                    ah->dynamicData.scSupp
                        = siface->instantiate(datum, siface->definitionData, ctx);
                    assert(ah->dynamicData.scSupp);
                }
                assert(siface->reset);
                ah->dynamicData.scSupp = siface->reset( datum
                        , ah->dynamicData.scSupp
                        , siface->definitionData
                        , ctx
                        );
            }
        }
    }
    return 0;
}

static void
_print_columns_list( const struct hdql_AttrHandlerTier * t
        , FILE * dest, size_t * columnsCount
        , const char * prefix
        , const char * columnDelim, const char * attrDelim, const char * collectionMarker, const char * anonColumnName
        );
/* part of `hdql_iQueryResultsHandler` implementation for CSV handler,
 * matches `hdql_iQueryResultsHandler::finalize_schema()`.
 *
 * Prints column names
 */
static int _csv_handler_finalize_schema(void * csv_) {
    assert(csv_);
    struct hdql_CSVHandler * csv = (struct hdql_CSVHandler *) csv_;
    csv->nColumnsPrinted = 0;

    if(csv->flatKeyViews && csv->flatKeyViewLength) {
        /* Print columns for keys; use <keyPrefix><N> for keys without a
         * label */
        char keyColNameBf[32];
        for(size_t nFKV = 0; nFKV < csv->flatKeyViewLength; ++nFKV) {
            struct hdql_KeyView * kv = csv->flatKeyViews + nFKV;
            if(csv->nColumnsPrinted) fputs(csv->fmt.valueDelimiter, csv->dest);
            if(kv->label) {
                fputs(kv->label, csv->dest);
            } else {
                snprintf( keyColNameBf, sizeof(keyColNameBf)
                        , csv->fmt.unlabeledKeyColumnFormat, nFKV );
                fputs(keyColNameBf, csv->dest);
            }
            ++(csv->nColumnsPrinted);
        }
    }

    if(hdql_attr_def_is_compound(csv->rootObjectHandler.ad)) {
        _print_columns_list(&csv->rootObjectHandler.payload.attrHandlers
                , csv->dest, &csv->nColumnsPrinted, NULL
                , csv->fmt.valueDelimiter, csv->fmt.attrDelimiter
                , csv->fmt.collectionLengthMarker, csv->fmt.anonymousColumnName
            );
    } else {
        if(csv->nColumnsPrinted) fputs(csv->fmt.valueDelimiter, csv->dest);
        fputs(csv->fmt.anonymousColumnName, csv->dest);
    }
    /* Preint delimiter after table header */
    fputs(csv->fmt.recordDelimiter, csv->dest);
    return 0;
}
static void
_print_columns_list( const struct hdql_AttrHandlerTier * t
        , FILE * dest
        , size_t * columnsCount
        /* formatting */
        , const char * prefix
        , const char * columnDelim, const char * attrDelim
        , const char * collectionMarker, const char * anonColumnName
        ) {
    for(size_t i = 0; i < t->n; ++i) {
        /* consider attribute definition at the tier */
        const struct hdql_CSVAttrHandler * ah = t->handlers[i];

        const struct hdql_AttrDef * topAD = hdql_attr_def_top_attr(ah->ad);

        if(hdql_attr_def_is_collection(topAD)) {
            /* print column delimiter */
            if(*columnsCount) fputs(columnDelim, dest);
            /* prefix collection column */
            fputs(collectionMarker, dest);
            if(prefix && attrDelim) {
                fputs(prefix, dest);
                fputs(attrDelim, dest);
            }
            fputs(ah->name ? ah->name : anonColumnName, dest);
            ++(*columnsCount);
        } else if(hdql_attr_def_is_atomic(topAD)) {
            /* print column delimiter */
            if(*columnsCount) fputs(columnDelim, dest);
            /* ordinary column */
            if(prefix && attrDelim) {
                fputs(prefix, dest);
                fputs(attrDelim, dest);
            }
            fputs(ah->name ? ah->name : anonColumnName, dest);
            ++(*columnsCount);
        } else if(hdql_attr_def_is_compound(topAD)) {
            /* compound -- expand recursively */
            char * nextPrefix = NULL;
            if(attrDelim) {
                size_t len = strlen(ah->name ? ah->name : anonColumnName) + strlen(attrDelim);
                if(prefix) len += strlen(prefix) + strlen(attrDelim);
                nextPrefix = (char *) malloc(len);
                if(prefix) {
                    snprintf(nextPrefix, len, "%s%s%s%s", prefix, attrDelim
                            , ah->name ? ah->name : anonColumnName, attrDelim);
                } else {
                    snprintf(nextPrefix, len, "%s%s", ah->name ? ah->name : anonColumnName, attrDelim);
                }
            }
            _print_columns_list( &ah->payload.attrHandlers
                    , dest, columnsCount, nextPrefix, columnDelim, attrDelim
                    , collectionMarker, anonColumnName);
        }
        #ifndef DNDEBUG
        else {
            assert(0);
        }
        #endif
    }
}

/*
 * Keys management
 */

/* part of `hdql_iQueryResultsHandler` implementation for CSV handler,
 * matches `hdql_iQueryResultsHandler::handle_keys()`.
 *
 * Keys themselves are given by a single linked list and uniquely identifies a
 * single record. To simplify acquizition of key values, a so-called "flat view"
 * is created in parallel of keys list, simplifying retrieval of keys once
 * the query result is advanced.
 * */
static int _csv_handler_handle_keys(struct hdql_CollectionKey * keys
            , struct hdql_KeyView * flatKeyViews
            , size_t nFlatKeys
            , void * csv_
            ) {
    assert(csv_);
    struct hdql_CSVHandler * csv = (struct hdql_CSVHandler *) csv_;
    assert(flatKeyViews);
    csv->flatKeyViews      = flatKeyViews;
    csv->flatKeyViewLength = nFlatKeys;
    return 0;
}

/*
 * Public API
 */

int
hdql_query_results_handler_csv_init( struct hdql_iQueryResultsHandler * iqr
        , FILE * stream
        , const struct hdql_DSVFormatting * fmt
        , struct hdql_Context * ctx
        ) {
    assert(ctx);
    /* set iface implementation callbacks */
    iqr->handle_result_type = _csv_handler_set_result_type;
    iqr->handle_keys        = _csv_handler_handle_keys;
    iqr->handle_record      = _csv_results_handler_handle_record;
    iqr->finalize_schema    = _csv_handler_finalize_schema;

    struct hdql_CSVHandler * csv
            = (struct hdql_CSVHandler *) malloc(sizeof(struct hdql_CSVHandler));
    csv->dest = stream;

    /* formatting options */
    #define _M_dup_or_NULL(t) \
        csv-> fmt . t = fmt-> t ? strdup(fmt-> t) : NULL
    _M_dup_or_NULL(recordDelimiter          );
    _M_dup_or_NULL(valueDelimiter           );
    _M_dup_or_NULL(attrDelimiter            );
    _M_dup_or_NULL(collectionLengthMarker   );
    _M_dup_or_NULL(anonymousColumnName      );
    _M_dup_or_NULL(nullToken                );
    _M_dup_or_NULL(unlabeledKeyColumnFormat );
    #undef _M_dup_or_NULL

    csv->ctx = ctx;
    csv->nColumnsPrinted = 0;

    memset(&csv->rootObjectHandler, 0x0, sizeof(csv->rootObjectHandler));
    fflush(stderr);

    iqr->userdata = csv;
    return 0;
}

void
hdql_query_results_handler_csv_cleanup( struct hdql_iQueryResultsHandler * iqr ) {
    // TODO
}
