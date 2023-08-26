#include "hdql/errors.h"

const char *
hdql_err_str(hdql_Err_t code) {
    switch(code) {
        case HDQL_ERR_CODE_OK:
            return "not an error, ok return code";
        case HDQL_ERR_GENERIC:
            return "unspecified common error";
        case HDQL_ERR_MEMORY:
            return "memory allocation error";

        case HDQL_ERR_TRANSLATION_FAILURE:
            return "general parser/lexer error";
        case HDQL_ERR_UNKNOWN_ATTRIBUTE:
            return "identifier is not known";
        case HDQL_ERR_ATTRIBUTE:
            return "querying atomic value";
        case HDQL_BAD_QUERY_EXPRESSION:
            return "failed to create or append query object";
        case HDQL_ERR_COMPOUNDS_STACK_DEPTH:
            return "compounds stack depth exceed";
        case HDQL_ERR_ATTR_FILTER:
            return "filter can not be applied to previous query result of type";
        case HDQL_ERR_FILTER_RET_TYPE:
            return "filter query result type can not be evaluated to bool";

        case HDQL_ERR_OPERATION_NOT_SUPPORTED:
            return "operation is not supported";
        case HDQL_ERR_ARITH_OPERATION:
            return "arithmetic operation error";
        case HDQL_ARITH_ERR_ZERO_DIVISION:
            return "division by zero";
        case HDQL_ARITH_ERR_NEGATIVE_SHIFT:
            return "negative bit shift";
        case HDQL_ERR_FUNC_MAX_NARG:
            return "maximum number of function arguments exceed";
        case HDQL_ERR_FUNC_ARG_COLLISION:
            return "argument #N already has been set";
        case HDQL_ERR_FUNC_REDEFINITION:
            return "function has been already defined";

        default:
            return "(unidentified error)";
    }
}

#if 0
//  12 |  ... .foo{bar := {{ ...
//                 30-31- ^^
//  Error: unexpected {, expected identifier.

// Rules:
//
//  + if problematic line is 1st (single) and this is the only line (first and
//    last lines are the same and it is the 1st line), do not print line number
//  - otherwise, line numbers must be printed, unless `hdql_kNoLineNumbers`
//    flag is given
// 
//  + if same line is bigger than 80 chars it will be truncated
//  - if problematic piece is bigger than 40 characters, no truncation applied
//  - if problematic line is not the same, no truncation applied
//

int
hdql_fancy_error( const char * programText
                , const int * positions
                , char * outbuf
                , unsigned int flags
                ) {
    bool truncateLine = false;
    /* positions: first line, column; last line, column */
    const int firstLine   = positions[0]
            , firstColumn = positions[1]
            , lastLine    = positions[2]
            , lastColumn  = positions[3]
            ;
    /* locate piece of text to be printed */
    int nLine = 0, nChar = 0;
    size_t lineOffsetStart = 0
         , lineOffsetEnd = 0
         ;
    for(const char * c = programText; '\0' != *c; ++c) {
        if('\n' == *c) {
            ++nLine;
            nChar = 0;
            continue;
        }
        ++nChar;
        //if(nLine == firstLine && firstColumn == nChar) {
        //}
    }
    bool doPrintLine = true;
    //if(firstLine == lastLine && ) ...
}
#endif

