#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#include "hdql/helpers/fancy-print-err.h"

#define ANSI_RED     "\033[31;1m"
#define ANSI_RESET   "\033[0m"

static int appendf(char **buf, size_t *remaining, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(*buf, *remaining, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= *remaining) {
        if (*remaining > 0) {
            **buf = '\0';  // Null terminate
        }
        return -1;  // overflow
    }
    *buf += n;
    *remaining -= n;
    return 0;
}

int
hdql_fancy_error_print( char * outBuf, unsigned int outBufSize
        , const char *sExpr
        , const int info[5]
        , const char *errBuf
        , int flags ) {
    int errCode      = info[0];  // currently unused
    int colBegin     = info[2];  // 1-based
    int lineBegin    = info[1];  // 1-based
    int colEnd       = info[4];  // 1-based
    int lineEnd      = info[3];  // 1-based

    if (!outBuf || outBufSize == 0) return -1;

    char * pOut = outBuf;
    size_t rem = outBufSize;

    // Step 1: Split lines
    size_t cap = 64, nLines = 0;
    const char ** lines = malloc(cap * sizeof(char *));
    size_t * lineLens = malloc(cap * sizeof(size_t));
    const char * p = sExpr;
    while( *p != '\0' ) {
        const char *start = p;
        while (*p && *p != '\n') ++p;
        if( nLines >= cap ) {
            cap <<= 1;
            lines = realloc(lines, cap * sizeof(char *));
            lineLens = realloc(lineLens, cap * sizeof(size_t));
        }
        lines[nLines] = start;
        lineLens[nLines] = p - start;
        ++nLines;
        if (*p == '\n') ++p;
    }

    int width = 1;
    for( int t = (int) nLines; t > 0; t /= 10 ) ++width;

    int printBegin = lineBegin - 1;
    int printEnd   = lineEnd   - 1;

    if (nLines > 1 && printBegin > 0)
        if (appendf(&pOut, &rem, "...\n") != 0) goto trunc;

    for (int i = printBegin; i <= printEnd && i < (int)nLines; ++i) {
        if (nLines > 1)
            if (appendf(&pOut, &rem, "%*d | ", width, i + 1) != 0) goto trunc;
        if (appendf(&pOut, &rem, "%.*s\n", (int)lineLens[i], lines[i]) != 0) goto trunc;

        // Print marker line
        if (i == lineBegin - 1 || i == lineEnd - 1) {
            if (nLines > 1)
                if (appendf(&pOut, &rem, "%*s | ", width, "") != 0) goto trunc;

            int start = (i == lineBegin - 1) ? colBegin - 1 : 0;
            int end   = (i == lineEnd   - 1) ? colEnd   - 1 : (int)lineLens[i];

            for (int j = 0; j < start && j < (int)lineLens[i]; ++j)
                if (appendf(&pOut, &rem, " ") != 0) goto trunc;

            if ((flags & 0x1) && appendf(&pOut, &rem, ANSI_RED) != 0) goto trunc;
            for (int j = start; j < end && j < (int)lineLens[i]; ++j)
                if (appendf(&pOut, &rem, "^") != 0) goto trunc;
            if ((flags & 0x1) && appendf(&pOut, &rem, ANSI_RESET) != 0) goto trunc;

            if (i == lineBegin - 1)
                if (appendf(&pOut, &rem, " Error: %s", errBuf) != 0) goto trunc;

            if (appendf(&pOut, &rem, "\n") != 0) goto trunc;
        }
    }

    if (nLines > 1 && printEnd < (int)nLines - 1)
        if (appendf(&pOut, &rem, "...\n") != 0) goto trunc;

    free(lines);
    free(lineLens);
    return 0;  // success

trunc:
    if (outBufSize > 0)
        outBuf[outBufSize - 1] = '\0';
    free(lines);
    free(lineLens);
    return 1;  // truncated
}

