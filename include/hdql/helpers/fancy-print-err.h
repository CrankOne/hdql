#ifndef H_HDQL_FANCY_PRINT_ERR_H
#define H_HDQL_FANCY_PRINT_ERR_H 1

#ifdef __cplusplus
extern "C" {
#endif

int
hdql_fancy_error_print( char * outBuf, unsigned int outBufSize
    , const char *sExpr
    , const int info[5]
    , const char *errBuf
    , int flags );

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
