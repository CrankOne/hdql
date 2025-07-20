%option noyywrap reentrant 8bit yylineno
%option bison-locations
%option noinput nounput
%x SELECTOR SELECTOR_IDS

D           [0-9]
L           [a-zA-Z_\\]
UL          [A-Z_]
DTNAME      {UL}
identifier  {L}({L}|{D})*

%{
#include <stdio.h>
#include "hdql.tab.h"

#define YY_USER_ACTION                              \
    yylloc->first_line = yylloc->last_line;         \
    yylloc->first_column = yylloc->last_column;     \
    for(int i = 0; yytext[i] != '\0'; i++) {        \
        if(yytext[i] == '\n') {                     \
            yylloc->last_line++;                    \
            yylloc->last_column = 0;                \
        } else {                                    \
            yylloc->last_column++;                  \
        }                                           \
    }

%}

%%

        static char gVerbatimBuf[128];
        static char * gVerbatimCur;

[ \t\n]         ; // ignore all whitespaces
"+"             {return T_PLUS;}
"-"             {return T_MINUS;}
"*"             {return T_ASTERISK;}
"/"             {return T_SLASH;}
"%"             {return T_PERC;}
"**"            {return T_DBL_ASTERISK;}
">"             {return T_GT;}
">>"            {return T_RBSHIFT;}
"<<"            {return T_LBSHIFT;}
"<"             {return T_LT;}
">="            {return T_GTE;}
"<="            {return T_LTE;}
"=="            {return T_EQ;}
":="            {return T_WALRUS;}
"("             {return T_LBC;}
")"             {return T_RBC;}
"{"             {return T_LCRLBC;}
"}"             {return T_RCRLBC;}
"!="            {return T_NE;}
"&"             {return T_AMP;}
"&&"            {return T_DBL_AMP;}
"|"             {return T_PIPE;}
"||"            {return T_DBL_PIPE;}
"^"             {return T_CAP;}
"^^"            {return T_DBL_CAP;}
"~"             {return T_TILDE;}
"!"             {return T_EXCLMM;}
"?"             {return T_QUESTIONMM;}
":"             {return T_COLON;}
";"             {return T_SEMICOLON;}
"++"            {return T_INCREMENT;}
"--"            {return T_DECREMENT;}
"+="            {return T_PLUSE;}
"-="            {return T_MINUSE;}
">>="           {return T_RBSHIFTE;}
"<<="           {return T_LBSHIFTE;}
","             {return T_COMMA;}
"."             {return T_PERIOD;}
([[:digit:]]{-}[0])[[:digit:]]*       {   char * endptr;
                    long val = strtol( yytext, &endptr, 0 );
                    int ec = errno;
                    if( *endptr != '\0' || (ec == ERANGE && (val == LONG_MIN || val == LONG_MAX)) ) {
                        hdql_error(yylloc, ws, yyscanner, "invalid integer"
                            " literal \"%s\": %s", yytext, strerror(ec) );
                        return T_INVALID_LITERAL;
                    }
                    yylval->intStaticValue = val;
                    return T_INT_STATIC_VALUE;
                }
0[01234567]*    {   char * endptr;
                    unsigned long val = strtoul( yytext, &endptr, 8 );
                    /*TODO: currently we do not really support full range of
                     * unsigned int values... */
                    if( val >= LONG_MAX ) {
                        hdql_error(yylloc, ws, yyscanner, "invalid integer"
                            " literal \"%s\": value is too large", yytext );
                        return T_INVALID_LITERAL;
                    }
                    int ec = errno;
                    if( *endptr != '\0' || (ec == ERANGE && (val == LONG_MIN || val == LONG_MAX)) ) {
                        hdql_error(yylloc, ws, yyscanner, "invalid integer"
                            " literal \"%s\": %s", yytext, strerror(ec) );
                        return T_INVALID_LITERAL;
                    }
                    yylval->intStaticValue = val;
                    return T_INT_STATIC_VALUE;
                }
0[xX][[:xdigit:]]+  {   char * endptr;
                    unsigned long val = strtoul( yytext, &endptr, 16 );
                    /*TODO: currently we do not really support full range of
                     * unsigned int values... */
                    if( val >= LONG_MAX ) {
                        hdql_error(yylloc, ws, yyscanner, "invalid integer"
                            " literal \"%s\": value is too large", yytext );
                        return T_INVALID_LITERAL;
                    }
                    int ec = errno;
                    if( *endptr != '\0' || (ec == ERANGE && (val == LONG_MIN || val == LONG_MAX)) ) {
                        hdql_error(yylloc, ws, yyscanner, "invalid integer"
                            " literal \"%s\": %s", yytext, strerror(ec) );
                        return T_INVALID_LITERAL;
                    }
                    yylval->intStaticValue = val;
                    return T_INT_STATIC_VALUE;
                }
[+-]?(({D}*"."{D}+)|({D}+"."{D}*))([eE][-+]?{D}+)? {
                    char * endPtr = NULL;
                    double val = strtod( yytext, &endPtr );
                    int ec = errno;
                    if( *endPtr != '\0'
                     || (HUGE_VAL == val && ERANGE == ec)
                     || (DBL_MIN  <= val && ERANGE == ec)
                     ) {
                        hdql_error(yylloc, ws, yyscanner, "invalid floating point"
                            " literal \"%s\": %s", yytext, strerror(ec) );
                        return T_INVALID_LITERAL;
                    }
                    yylval->fltStaticValue = val;
                    return T_FLT_STATIC_VALUE;
                }
{identifier}    {   {   /* Lookup for name in constant values table and return
                         * a const value if found */
                        struct hdql_Constants * consts = hdql_context_get_constants(ws->context);
                        hdql_Datum_t valuePtr;
                        enum hdql_ExternValueType vt = hdql_constants_get_value(consts, yytext, &valuePtr);
                        if(hdql_kExternValFltType == vt) {
                            yylval->fltStaticValue = *((hdql_Flt_t *) valuePtr);
                            return T_FLT_STATIC_VALUE;
                        } else if(hdql_kExternValIntType == vt) {
                            yylval->fltStaticValue = *((hdql_Int_t *) valuePtr);
                            return T_INT_STATIC_VALUE;
                        }
                    }
                    yylval->strID = strdup(yytext);
                    return T_IDENTIFIER;
                }


"["             gVerbatimCur = gVerbatimBuf; BEGIN(SELECTOR);

<SELECTOR>"]"   {
                    *gVerbatimCur = '\0';
                    BEGIN(INITIAL);
                    yylval->selexpr = strdup(gVerbatimBuf);
                    return T_SELECTION_EXPRESSION;
                }

<SELECTOR>\\->  {
                    strncpy(gVerbatimCur, yytext+1, 2);
                    gVerbatimCur += 2;
                }

<SELECTOR>->    {
                    *gVerbatimCur = '\0';
                    yylval->selexpr = strdup(gVerbatimBuf);
                    BEGIN(SELECTOR_IDS);
                    gVerbatimCur = gVerbatimBuf;
                    return T_SELECTION_EXPRESSION;
                }

<SELECTOR_IDS>"]"   {
                    *gVerbatimCur = '\0';
                    BEGIN(INITIAL);
                    yylval->selexpr = strdup(gVerbatimBuf);
                    return T_SELECTION_LABEL;
                }

<SELECTOR,SELECTOR_IDS>\\]   { *gVerbatimCur++ = yytext[1]; }

<SELECTOR,SELECTOR_IDS>([^\]-]|(\\->))+ {
                    char *yptr = yytext;
                    while ( *yptr )
                        *gVerbatimCur++ = *yptr++;
                }

%%

