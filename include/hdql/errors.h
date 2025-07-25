#ifndef H_HDQL_ERROR_CODES_H
#define H_HDQL_ERROR_CODES_H

#include "hdql/types.h"
#define HDQL_M_for_each_err_code(m, ...) \
    m( OK,               0,    "ok" ) \
    m( GENERIC,         -1,    "generic error" ) \
    /* ... */

/* General codes */
#define HDQL_ERR_CODE_OK                    0   /* not an error, ok return code */
#define HDQL_ERR_GENERIC                   -1   /* unspecified common error */
#define HDQL_ERR_MEMORY                    -2   /* memory allocation error */
#define HDQL_ERR_BAD_ARGUMENT              -3   /* bad argument or argument combination */
#define HDQL_ERR_CONTEXT_INCOMPLETE        -4   /* context not initialized properly */
#define HDQL_ERR_NAME_COLLISION            -5   /* an entry with such name already exists */
/* Parser/lexer errors */
#define HDQL_ERR_TRANSLATION_FAILURE      -11   /* general parser/lexer error */
#define HDQL_ERR_UNKNOWN_ATTRIBUTE        -12   /* identifier is not known */
#define HDQL_ERR_ATTRIBUTE                -13   /* querying atomic value */
#define HDQL_BAD_QUERY_EXPRESSION         -14   /* failed to create or append query object */
#define HDQL_ERR_COMPOUNDS_STACK_DEPTH    -15   /* compounds stack depth exceed */
#define HDQL_ERR_ATTR_FILTER              -16   /* filter can not be applied to result query of type */
#define HDQL_ERR_FILTER_RET_TYPE          -17   /* filter query result type can not be evaluated to bool */
#define HDQL_ERR_BAD_QUERY_STATE          -18   /* query instance is on unforeseen state */
/* Arithmetic operations (definition or evaluation) errors */
#define HDQL_ERR_OPERATION_NOT_SUPPORTED  -21   /* operation is not supported */
#define HDQL_ERR_ARITH_OPERATION          -22   /* arithmetic operation error */
#define HDQL_ARITH_ERR_ZERO_DIVISION      -23   /* division by zero */
#define HDQL_ARITH_ERR_NEGATIVE_SHIFT     -24   /* negative bit shift */
#define HDQL_ERR_FUNC_UNKNOWN             -25   /* unknown function identifier */
#define HDQL_ERR_FUNC_CANT_INSTANTIATE    -26   /* could not instantiate function candidate for given arguments */
#define HDQL_ERR_EMPTY_SET                -27   /* operation result is an empty set */
#define HDQL_ERR_CONVERSION               -31   /* value conversion failed */
//#define HDQL_ERR_FUNC_ARG_COLLISION       -26   /* argument #N already has been set */
//#define HDQL_ERR_FUNC_REDEFINITION        -27   /* function has been already defined */
/* Interface-specific errors */
#define HDQL_ERR_INTERFACE_ERROR         -101  /* failed to reserve key */

#ifdef __cplusplus
extern "C"
#endif
const char * hdql_err_str(hdql_Err_t);

#endif

