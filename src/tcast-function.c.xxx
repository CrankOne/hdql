
#include "hdql/context.h"
#include "hdql/compound.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <assert.h>

/**\brief Interface for generic functions */
struct hdql_GenericFuncIFace {
    /**\brief Type code for result value*/
    hdql_ValueTypeCode_t resultTypeCode;
    /**\brief May allocate state for the generic function */
    hdql_Datum_t (*init_state)(hdql_Datum_t, hdql_Query **, hdql_Context_t);
    /**\brief Evaluates function on data
     *
     * Should return zero if item is not the last one, positive number if
     * no more results available and one of the (negative) `HDQL_ERR_...`
     * codes on error. */
    int (*eval)(hdql_Datum_t data, hdql_Query **, hdql_Datum_t state, hdql_Context_t);
    /**\brief Called to re-set state */
    int (*reset)();
    /**\brief Called on cleanup */
    int (*destroy)();
};

struct hdql_FunctionCtrArgs {
    union {
        struct hdql_GenericFuncIFace * genericFunction;
        // ...
    } pl;
    hdql_Query ** argQueries;
};

/*
 * Collection interface to generic function
 */

typedef struct GenericFunctionWrapper {
    bool isResultValid;
    bool isExhausted;
    const struct hdql_GenericFuncIFace * iface;
    hdql_Datum_t state;
    hdql_Datum_t result;
    hdql_Query ** argQueries;
} GenericFunctionWrapper_t;

static hdql_It_t
_generic_function_create_state(
          hdql_Datum_t dataObject
        , hdql_SelectionArgs_t fdef_
        , hdql_Context_t ctx
        ) {
    assert(fdef_);
    struct GenericFunctionWrapper * fWrapper
        = hdql_alloc(ctx, GenericFunctionWrapper_t);
    struct hdql_FunctionCtrArgs * fCtrArgs = (struct hdql_FunctionCtrArgs *) fdef_;
    fWrapper->iface = fCtrArgs->pl.genericFunction;
    /* Initialize state */
    fWrapper->state = fWrapper->iface->init_state(dataObject, fCtrArgs->argQueries, ctx);
    /* Allocate result pointer */
    assert(fWrapper->iface->resultTypeCode);
    fWrapper->result = hdql_allocate(fWrapper->iface->resultTypeCode, ctx);
    /* invalidate current result */
    fWrapper->isResultValid = false;
    /* mark as not exhausted yet */
    fWrapper->isExhausted = false;
    /* set argument queries pointer */
    fWrapper->argQueries = fCtrArgs->argQueries;
    return ((hdql_It_t) fWrapper);
}

static hdql_Datum_t
_generic_function_dereference( hdql_Datum_t root
        , hdql_It_t fWrapper_, hdql_Context_t ctx) {
    struct GenericFunctionWrapper * fWrapper
        = (struct GenericFunctionWrapper *) fWrapper_;
    if(!fWrapper->isResultValid) {
        int rc = fWrapper->iface->eval(root, fWrapper->argQueries, fWrapper->result, ctx);
        if(!!(fWrapper->isExhausted = (rc == 0)))
            fWrapper->isResultValid = true;
        assert(0 >= rc);  // TODO: handle function evaluation error
    }
    if(fWrapper->isResultValid) return fWrapper->result;
    return NULL;
}

static hdql_It_t
_generic_function_advance( hdql_Datum_t root
                         , hdql_SelectionArgs_t
                         , hdql_It_t fWrapper_
                         , hdql_Context_t
                         ) {

}


#if 0
typedef hdql_Datum_t (*hdql_DataCastFunction)(hdql_Datum_t, hdql_Context_t);

struct hdql_Func {
private:
    static const unsigned int kIsExhausted;
    static const unsigned int kIsResultValid;
    static const unsigned int kHasState;

    unsigned int _funcTypeFlags;
    union {
        // ...
    } pl;

    hdql_Query ** _argQueries;
    hdql_Datum_t _result;

    hdql_DataCastFunction ** _castArg;  // not used by generic function
    hdql_CollectionKey ** _argKeys;  // not used by generic function
    hdql_Datum_t * _argValues;  // not used by generic function
protected:
    void _set_exhausted() { _funcTypeFlags |= kIsExhausted; }
    void _set_result_valid() { _funcTypeFlags |= kIsResultValid; }
public:
    bool is_exhausted() const { return _funcTypeFlags & kIsExhausted; }
    bool is_result_valid() const { return _funcTypeFlags & kIsResultValid; }
    bool has_state() const { return _funcTypeFlags & kHasState; }
};
#endif

