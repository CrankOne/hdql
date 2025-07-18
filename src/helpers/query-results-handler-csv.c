#include "hdql/helpers/query-results-handler-csv.h"

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/errors.h"
#include "hdql/types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct hdql_CSVHandler;  /* fwd */
struct hdql_CSVAttrHandler;  /* fwd */
struct hdql_AttrHandlerTier;  /* fwd */

/* recursive struct defining one level attribute handlers */
struct hdql_AttrHandlerTier {
    const char * suffix;  /* null for top level */
    size_t n;  /* number handlers at this level */
    struct hdql_CSVAttrHandler ** handlers;
};

/* Userdata for `hdql_iQueryResultsHandler` defining how to actually handle
 * attributes from a query result. */
struct hdql_CSVHandler {
    /** destination stream to print data */
    FILE * dest;
    /** value in a record delimiter, a null-terminated string */
    char * valueDelimiter;
    /** record delimiter, a null-terminated string */
    char * recordDelimiter;

    /** context is needed to expand compound columns */
    struct hdql_Context * ctx;

    /* Array of attribute handlers, in order */
    struct hdql_AttrHandlerTier rootHandlers;

    /* internal printer state */
    size_t nColumnsPrinted;
};

/* Handles individual attributes in query result. May expand to multiple
 * columns (e.g. for scalar compound attributes) */
struct hdql_CSVAttrHandler {
    /* Attribute entry definition */
    const struct hdql_AttrDef * ad;
    /* Prints columns based on attribute definition and given datum,
     * uses CSV handler's formatting options and stream */
    void (*value_handler)( struct hdql_CSVHandler *, hdql_Datum_t, struct hdql_CSVAttrHandler * ad);

    /* runtime short-lived state, sometimes used to dereference scalar
     * attributes (depends on iface) */
    hdql_Datum_t scSuppDynamicData;

    union {
        /* callback used to stringify atomic scalar value */
        int (*get_as_string)(const hdql_Datum_t, char * buf, size_t bufSize, hdql_Context_t);
        /* child tiers to stringify compounds */
        struct hdql_AttrHandlerTier attrHandlers;
    } payload;
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

/** utility function iterating all attribute handlers with given callback */
//static int
//_for_all_handlers_in_tier( struct hdql_AttrHandlerTier * t
//        , int (*cllb)(const char * prefix, struct hdql_CSVAttrHandler *, void *)
//        , void * userdata
//        ) {
//    int rc;
//    for(size_t i = 0; i < t->n; ++i) {
//        rc = cllb(t->suffix, t->handlers[i], userdata);
//        if(0 != rc) return rc;
//    }
//    return 0;
//}

/*
 * Utility functions of CSV record handler main structure
 */

/* utility function: actually printing the column, respecting delimiters,
 * formatting, etc. */
static void
_csv_print_column(struct hdql_CSVHandler * csv
        , const char * columnString ) {
    if(csv->nColumnsPrinted) {
        fputs(csv->valueDelimiter, csv->dest);
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

static void
_expand_scalar_compound_to_columns( const struct hdql_AttrDef * ad
        , struct hdql_AttrHandlerTier * t
        , struct hdql_Context * ctx
        );

static int
_extend_tier_with_attribute( struct hdql_AttrHandlerTier * t
            , const char * attrName
            , const struct hdql_AttrDef * ad
            , struct hdql_Context * ctx
            ) {
    /* allocate new attribute handler */
    struct hdql_CSVAttrHandler * ah = (struct hdql_CSVAttrHandler *)
                malloc(sizeof(struct hdql_CSVAttrHandler));
    ah->ad = ad;
    if(hdql_attr_def_is_collection(ad)) {
        ah->value_handler = _handle_collection_as_csv_entry;
    } else if(hdql_attr_def_is_atomic(ad)) {
        ah->value_handler = _handle_scalar_atomic_as_csv_entry;
        struct hdql_ValueTypes * valTypes = hdql_context_get_types(ctx);
        const struct hdql_ValueInterface * vi
              = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(ad));
        /* TODO: unclear whether we will get rid of this method in favor of
         *  conversions, though */
        assert(vi->get_as_string);
        ah->payload.get_as_string = vi->get_as_string;
    } else if(hdql_attr_def_is_compound(ad)) {
        ah->value_handler = _handle_scalar_compound_as_csv_entry;
        _expand_scalar_compound_to_columns(ad, &ah->payload.attrHandlers, ctx);
    }
    #ifndef DNDEBUG
    else { assert(0);}
    #endif
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
    const size_t nAttrs = hdql_compound_get_nattrs(c);
    const char ** attrNames = alloca(sizeof(const char*)*nAttrs);
    hdql_compound_get_attr_names(c, attrNames);
    for(size_t nAttr = 0; nAttr < nAttrs; ++nAttr) {
        const struct hdql_AttrDef * subAD = hdql_compound_get_attr(c, attrNames[nAttr]);
        assert(subAD);
        _extend_tier_with_attribute(t, attrNames[nAttr], subAD, ctx);
    }
}

/* part of `hdql_iQueryResultsHandler` implementation for CSV handler,
 * matches `hdql_iQueryResultsHandler::handle_attr_def()`.
 *
 * Extends set of individual attributes handlers based on compoound query
 * result attribute declaration, keeps track on the columns to print.
 * */
static int
_csv_results_handler_handle_attr_def(const char * attrName
        , const struct hdql_AttrDef * ad, void * csv_ ) {
    struct hdql_CSVHandler * csv = (struct hdql_CSVHandler *) csv_;
    return _extend_tier_with_attribute(&csv->rootHandlers, attrName, ad, csv->ctx);
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
        h->scSuppDynamicData = siface->reset(ownerDatum
                , h->scSuppDynamicData
                , siface->definitionData
                , csv->ctx
                );
    }
    assert(siface->dereference);
    return siface->dereference( ownerDatum
            , h->scSuppDynamicData
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
        _csv_print_column(csv, "N/A");  /* TODO: configurable */
    }
}

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

    hdql_Datum_t r = _get_scalar_data(csv, ownerDatum, h);

    assert(0); /* TODO: traverse recursively */
}

/*
 * Public API
 */

int
hdql_query_results_handler_csv_init( struct hdql_iQueryResultsHandler * iqr
        , FILE * stream
        , const char * valueDelimiter, const char * recordDelimiter
        ) {
    iqr->handle_attr_def = _csv_results_handler_handle_attr_def;
    // ...
    assert(0);  // TODO
    return 0;
}
