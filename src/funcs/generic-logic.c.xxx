/**\file
 * \brief Implementation of `any()`, `all()`, `none()` functions
 *
 * This file provides implementation for `any()`, `all()`, `none()` functions
 * that essentially are the concatenation of boolean results.
 *
 * `any()`, `all()`, `none()` recieve arbitrary number of arguments, scalars,
 * collections, etc with to-bool conversion operation defined for every item.
 * On call it iterates over all the arguments and collection entries evaluating
 * queries to boolean values till:
 *
 *  - for `any()` first `true` result is met (in which case it returns `true`,
 *    or all the sequences are depleted (and the `false` is returned). Returned
 *    value is of boolean argument.
 *  - for `all()` first `false` result is met (in which case it returns `false`)
 *    or all the sequences are depleted (and the `true` is returned). Returned
 *    value is of boolean argument.
 *  - for `none()` first `true` result is met (in which case it returns `false`)
 *    or all the sequences are depleted (and the `true` is returned). Returned
 *    value is of boolean argument.
 *
 * Result is not annotated with a key and is a plain boolean value.
 * */

#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

#include <stdio.h>
#include <assert.h>

struct GenericFunctionOnSetInstArgs {
    size_t nArg;
    hdql_Datum_t * argValues;
    hdql_TypeConverter * converters;
    // ...
};

struct hdql_AttrDef *
hdql_generic_function_try_construct__set_logic(
          struct hdql_Query ** args
        , void * userdata
        , char * failureBuffer, size_t failureBufferSize
        , hdql_Context_t context
        ) {
    /* count arguments */
    size_t nArg = 0;
    for(struct hdql_Query ** q = args; NULL != *q; ++q, ++nArg) {}
    if(0 == nArg) {
        snprintf( failureBuffer, failureBufferSize
                , "Function is not defined for empty arguments list." );
        return NULL;
    }
    /* allocate function instantiation arguments object containing type
     * conversions, preallocated values, etc */
    struct GenericFunctionOnSetInstArgs * gfArgs
            = hdql_alloc(context, struct GenericFunctionOnSetInstArgs);
    /* get boolean value type code to figure out converters */
    struct hdql_ValueTypes * vts = hdql_context_get_types(context);
    assert(vts);
    hdql_ValueTypeCode_t boolTypeCode = hdql_types_get_type_code(vts, "hdql_Bool_t");
    assert(0x0 != boolTypeCode);
    /* fill instantiation args with the converters/intermediate values, etc */
    for(struct hdql_Query ** q = args; NULL != *q; ++q, ++nArg) {
        const struct hdql_AttrDef * ad = hdql_query_top_attr(*q);
        /* if query returns a compound, decline function instantiation */
        if(hdql_attr_def_is_compound(ad)) {
            /* TODO: perhaps, user-defined compound-to-bool operation can be
             * added in future. This code should be revised than. */
            const struct hdql_Compound * compound = hdql_attr_def_compound_type_info(ad);
            if(hdql_compound_is_virtual(compound)) {
                snprintf( failureBuffer, failureBufferSize
                    , "Argument %lu is of virtual compound type based on <%s>"
                    , nArg + 1
                    , hdql_compound_get_name(compound) );
                return NULL;
            } else {
                snprintf( failureBuffer, failureBufferSize
                    , "Argument %lu is of compound type <%s>"
                    , nArg + 1
                    , hdql_compound_get_name(compound) );
                return NULL;
            }
        }
        hdql_ValueTypeCode_t tc = hdql_attr_def_get_atomic_value_type_code(ad);
        if(0x0 == tc) {
            /* this is somewhat unexpected situation, when query result is not
             * a compound, but atomic value type code is not set. Yet it can
             * be legal in future changes of the API */
            snprintf( failureBuffer, failureBufferSize
                    , "Argument %lu doesn not produce yield to atomic value of"
                      " well-defined type"
                    , nArg + 1 );
            return NULL;
        }
        /* If argument is of boolean value, skip converter creation */
    }
    // ...
    assert(0);  // TODO
}

