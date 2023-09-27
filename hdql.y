%code requires {

#include "hdql/types.h"
#include "hdql/query.h"
#include "hdql/function.h"
#include "hdql/errors.h"
#include "hdql/attr-def.h"
#include "hdql/internal-ifaces.h"

struct hdql_FuncArgList {
    struct hdql_Query * thisArgument;
    struct hdql_FuncArgList * nextArgument;
};

struct hdql_AnnotatedSelection {
    char * selectionExpression
       , * identifiers
       ;
};

typedef struct Workspace {
    struct {
        const struct hdql_Compound * compoundPtr;
        char * newAttributeName;
    } compoundStack[HDQL_COMPOUNDS_STACK_MAX_DEPTH];
    unsigned char compoundStackTop;
    struct hdql_Context * context;
    struct hdql_Query * query;
    /* shortcuts */
    //struct hdql_Operations * operations;
    /* ... */
    char * errMsg;
    unsigned int errMsgSize;
    unsigned int errPos[4]; /* first column, first line, last column, last line */
} * Workspace_t;

typedef void *yyscan_t;  /* circumvent circular dep: YACC/BISON does not know this type */
}

%define api.pure full
%define parse.error verbose
%locations

%lex-param {Workspace_t ws}
%lex-param {yyscan_t yyscanner}

%parse-param {Workspace_t ws}
%parse-param {yyscan_t yyscanner}

%code {
static int
_resolve_query_top_as_compound( struct hdql_Query * q
                              , char * identifier
                              , YYLTYPE * yyloc
                              , struct Workspace * ws
                              , const struct hdql_AttrDef ** r
                              );
static struct hdql_Query *
_new_virtual_compound_query( YYLTYPE * yylloc
                           , Workspace_t ws
                           , yyscan_t yyscanner
                           , struct hdql_Compound * compoundPtr
                           , struct hdql_Query * filterPtr
                           );
static struct hdql_Query *
_new_function( YYLTYPE * yyloc, struct Workspace * ws, yyscan_t yyscanner
             , const char * funcName
             , struct hdql_FuncArgList * argsList
             );
static int
_operation( struct hdql_Query * a
          , hdql_OperationCode_t opCode
          , struct hdql_Query * b
          , YYLTYPE * yyloc
          , struct Workspace * ws
          , yyscan_t yyscanner
          , const char * opDescription
          , struct hdql_Query ** r
          );
#define M_OP(op1, opName, op2, opDescr, R) \
    int rc = _operation(op1, hdql_k ## opName, op2, &yyloc, ws, NULL, opDescr, &(R)); \
    if(0 != rc) return rc;

#define is_atomic(attr)     hdql_attr_def_is_atomic(attr)
#define is_collection(attr) hdql_attr_def_is_collection(attr)
#define is_static(attr)     hdql_attr_def_is_static_value(attr)
#define is_subquery(attr)   hdql_attr_def_is_fwd_query(attr)

#define is_compound(attr)       hdql_attr_def_is_compound(attr)
#define is_scalar(attr)         hdql_attr_def_is_scalar(attr)
#define is_dynamic(attr)        (!hdql_attr_def_is_static_value(attr))
#define is_direct_query(attr)   hdql_attr_def_is_direct_query(attr)

}

%code provides {

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <assert.h>
# include <limits.h>
# include <math.h>
# include <float.h>
# include <stdarg.h>

#define YY_DECL \
    int hdql_lex( YYSTYPE* yylval_param \
             , YYLTYPE* yylloc_param \
             , Workspace_t ws \
             , yyscan_t yyscanner )
YY_DECL;

void
hdql_error( YYLTYPE * yylloc
          , Workspace_t ws
          , yyscan_t yyscanner
          , const char * fmt
          , ...
          );

int _push_cmpd(struct Workspace *, const struct hdql_Compound * cmpd);
int _pop_cmpd(struct Workspace *);
const struct hdql_Compound * hdql_parser_top_compound(struct Workspace *);

}

%union {
    char * strID;
    char * selexpr;
    hdql_Int_t intStaticValue;
    hdql_Flt_t fltStaticValue;
    struct { struct hdql_Compound * compoundPtr; struct hdql_Query * filter; } vCompound;
    struct hdql_Query * queryPtr;
    struct hdql_FuncArgList * funcArgsList;
    struct hdql_AnnotatedSelection annotatedSelection;
    // ... function
}

%token T_PLUS "+" T_MINUS "-" T_ASTERISK "*" T_SLASH "/" T_PERC "%" T_RBSHIFT ">>" T_LBSHIFT "<<"
%token T_DBL_ASTERISK "**" T_TILDE "~"
%token T_DBL_CAP "^^" T_DBL_AMP "&&" T_DBL_PIPE "||"
%token T_LBC "(" T_RBC ")" T_LCRLBC "{" T_RCRLBC "}"
%token T_EXCLMM "!" T_QUESTIONMM "?" T_COLON ":"
%token T_GT ">" T_GTE ">=" T_LT "<" T_LTE "<=" T_EQ "==" T_NE "!=" T_WALRUS ":="
%token T_AMP "&" T_PIPE "|" T_CAP "^"
%token T_INCREMENT "++" T_DECREMENT "--"
%token T_PLUSE "+=" T_MINUSE "-=" T_RBSHIFTE ">>=" T_LBSHIFTE "<<="
%token T_SEMICOLON T_COMMA "," T_PERIOD "."

%token T_INVALID_LITERAL "invalid literal"
%token<strID> T_IDENTIFIER "identifier"
%token<selexpr> T_SELECTION_EXPRESSION "selection expression"
%token<selexpr> T_SELECTION_IDS "selection identifier(s)"
%token<intStaticValue> T_INT_STATIC_VALUE "integer value"
%token<fltStaticValue> T_FLT_STATIC_VALUE "floating point value"

%type<queryPtr> queryExpr aOp aQExpr;
%type<vCompound> vCompoundDef scopedDefs;
%type<funcArgsList> argsList;
%type<annotatedSelection> selection;

// > Operations with lowest precedence are listed first, and those with
// > highest precedence are listed last. Operations with equal precedence
// > are listed on the same line
//%nonassoc T_LT T_LTE T_GT T_GTE
%right T_RBSHIFTE T_LBSHIFTE
%right T_QUESTIONMM
%left T_DBL_PIPE
%left T_DBL_AMP
%left T_DBL_CAP
%left T_PIPE
%left T_CAP
%left T_AMP
%left T_LT T_LTE T_GT T_GTE
%left T_RBSHIFT T_LBSHIFT
%left T_PLUS T_MINUS
%left T_ASTERISK T_SLASH T_PERC
%right T_TILDE T_EXCLMM
%right UNARYMINUS
%left T_LBC T_RBC
%left T_COMMA
%nonassoc T_WALRUS
%left T_PERIOD
%left T_LCRLBC T_RCRLBC /* highest priority */

%start toplev

%%

     toplev : error
            { /*ws->root = NULL;*/ return HDQL_ERR_TRANSLATION_FAILURE; }
            //| queryExpr { ws->query = $1; }
            | aQExpr { ws->query = $1; }
            ;

     aQExpr : aOp  { $$ = $1; }
            | aQExpr T_PLUS aQExpr
            { M_OP( $1, OpSum,      $3, "sum binary", $$ ); }
            | aQExpr T_MINUS aQExpr
            { M_OP( $1, OpMinus,    $3, "subtraction binary", $$ ); }
            | aQExpr T_ASTERISK aQExpr
            { M_OP( $1, OpProduct,  $3, "product binary", $$ ); }
            | aQExpr T_SLASH aQExpr
            { M_OP( $1, OpDivide,   $3, "division binary", $$ ); }
            | aQExpr T_PERC aQExpr
            { M_OP( $1, OpModulo,   $3, "modulo binary", $$ ); }
            | aQExpr T_RBSHIFT aQExpr
            { M_OP( $1, OpRBShift,  $3, "right bit shift binary", $$ ); }
            | aQExpr T_LBSHIFT aQExpr
            { M_OP( $1, OpLBShift,  $3, "left bit shift binary", $$ ); }
            | aQExpr T_AMP aQExpr
            { M_OP( $1, OpBAnd,     $3, "bitwise AND binary", $$ ); }
            | aQExpr T_CAP aQExpr
            { M_OP( $1, OpBXOr,     $3, "bitwise XOR binary", $$ ); }
            | aQExpr T_PIPE aQExpr
            { M_OP( $1, OpBOr,      $3, "bitwise OR binary", $$ ); }
            | aQExpr T_DBL_PIPE aQExpr
            { M_OP( $1, OpLOr,      $3, "logic OR", $$ ); }
            | aQExpr T_DBL_AMP aQExpr
            { M_OP( $1, OpLAnd,      $3, "logic AND", $$ ); }
            | aQExpr T_DBL_CAP aQExpr
            { M_OP( $1, OpLXOr,      $3, "logic XOR", $$ ); }
            | aQExpr T_LT aQExpr
            { M_OP( $1, OpLT,       $3, "less-than binary", $$ ); }
            | aQExpr T_LTE aQExpr
            { M_OP( $1, OpLTE,      $3, "less-than-or-equal binary", $$ ); }
            | aQExpr T_GT aQExpr
            { M_OP( $1, OpGT,       $3, "greater-than binary", $$ ); }
            | aQExpr T_GTE aQExpr
            { M_OP( $1, OpGTE,      $3, "greater-than-or-equal binary", $$ ); }
            | T_EXCLMM aQExpr
            { M_OP( $2, UOpNot,   NULL, "logic negate unary", $$ ); }
            | T_TILDE aQExpr
            { M_OP( $2, UOpBNot,  NULL, "bitwise inversion unary", $$ ); }
            | UNARYMINUS aQExpr
            { M_OP( $2, UOpMinus, NULL, "arithmetic negate unary", $$ ); }
            | T_LBC aQExpr T_RBC
            { $$ = $2; }
            | T_IDENTIFIER T_LBC argsList T_RBC
            {
                $$ = _new_function(&yyloc, ws, yyscanner, $1, $3);
                free($1);
                if(NULL == $$) return HDQL_ERR_TRANSLATION_FAILURE;
            }
            ;

   argsList : aQExpr
            { 
                $$ = (struct hdql_FuncArgList*)
                        malloc(sizeof(struct hdql_FuncArgList));
                $$->thisArgument = $1;
                $$->nextArgument = NULL;
            }
            | argsList T_COMMA aQExpr
            {
                $$->nextArgument = (struct hdql_FuncArgList*)
                        malloc(sizeof(struct hdql_FuncArgList));
                $$->thisArgument = $3;
                $$->nextArgument = $1;
            }
            ;

        aOp : T_FLT_STATIC_VALUE
            {
                hdql_ValueTypeCode_t vtCode = hdql_types_get_type_code(
                        hdql_context_get_types(ws->context), "hdql_Flt_t");
                assert(0x0 != vtCode);

                hdql_Datum_t valueCopy = hdql_context_alloc(ws->context, sizeof(hdql_Flt_t));
                if( NULL == valueCopy ) {
                    hdql_error(&yyloc, ws, yyscanner
                            , "Failed to allocate memory for floating point constant value");
                    return HDQL_ERR_MEMORY;
                }
                *((hdql_Flt_t *) valueCopy) = $1;
                struct hdql_AttrDef * attrDef
                        = hdql_attr_def_create_static_atomic_scalar_value(vtCode, valueCopy, ws->context);
                hdql_attr_def_set_stray(attrDef);
                $$ = hdql_query_create(attrDef, NULL, ws->context);
                assert($$);
                assert(vtCode == hdql_attr_def_get_atomic_value_type_code(hdql_query_top_attr($$)));
            }
            | T_INT_STATIC_VALUE
            {
                hdql_ValueTypeCode_t vtCode = hdql_types_get_type_code(
                        hdql_context_get_types(ws->context), "hdql_Int_t");
                assert(0x0 != vtCode);

                hdql_Datum_t valueCopy = hdql_context_alloc(ws->context, sizeof(hdql_Int_t));
                if( NULL == valueCopy ) {
                    hdql_error(&yyloc, ws, yyscanner
                            , "Failed to allocate memory for integer constant value");
                    return HDQL_ERR_MEMORY;
                }
                *((hdql_Int_t *) valueCopy) = $1;
                struct hdql_AttrDef * attrDef
                        = hdql_attr_def_create_static_atomic_scalar_value(vtCode, valueCopy, ws->context);
                hdql_attr_def_set_stray(attrDef);
                $$ = hdql_query_create(attrDef, NULL, ws->context);
                assert($$);
                assert(vtCode == hdql_attr_def_get_atomic_value_type_code(hdql_query_top_attr($$)));
            }
            | queryExpr { $$ = $1; }
            ;

  selection : T_SELECTION_EXPRESSION
            { $$.selectionExpression = $1; $$.identifiers = NULL; }
            | T_SELECTION_EXPRESSION T_SELECTION_IDS
            { $$.selectionExpression = $1; $$.identifiers = $2; }
            | T_SELECTION_IDS
            { $$.selectionExpression = NULL; $$.identifiers = $1; }
            ;

  queryExpr : T_PERIOD T_IDENTIFIER
            {
                const struct hdql_AttrDef * attrDef = hdql_compound_get_attr(
                    hdql_parser_top_compound(ws), $2 );
                if(NULL == attrDef) {
                    const struct hdql_Compound * topCompound = hdql_parser_top_compound(ws);
                    if(!hdql_compound_is_virtual(topCompound)) {
                        hdql_error(&yyloc, ws, yyscanner
                            , "type `%s' has no attribute \"%s\""
                            , hdql_compound_get_name(topCompound), $2);
                    } else {
                        hdql_error(&yyloc, ws, yyscanner
                            , "virtual compound based on type `%s' has no attribute \"%s\""
                            , hdql_compound_get_name(hdql_virtual_compound_get_parent(topCompound)), $2);
                    }
                    free($2);
                    return HDQL_ERR_UNKNOWN_ATTRIBUTE;
                }
                $$ = hdql_query_create( attrDef, NULL, ws->context);
                free($2);
                assert($$);
            }
            | T_PERIOD T_IDENTIFIER selection
            {   
                const struct hdql_AttrDef * attrDef = hdql_compound_get_attr(
                    hdql_parser_top_compound(ws), $2 );
                if(NULL == attrDef) {
                    const struct hdql_Compound * topCompound = hdql_parser_top_compound(ws);
                    if(!hdql_compound_is_virtual(topCompound)) {
                        hdql_error(&yyloc, ws, yyscanner
                            , "type `%s' has no attribute \"%s\""
                            , hdql_compound_get_name(topCompound), $2);
                    } else {
                        hdql_error(&yyloc, ws, yyscanner
                            , "virtual compound based on type `%s' has no attribute \"%s\""
                            , hdql_compound_get_name(hdql_virtual_compound_get_parent(topCompound)), $2);
                    }
                    free($2);
                    if($3.selectionExpression) free($3.selectionExpression);
                    if($3.identifiers)         free($3.identifiers);
                    return HDQL_ERR_UNKNOWN_ATTRIBUTE;
                }
                if(is_scalar(attrDef)) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute \"%s\" of `%s' is not a collection (can't"
                          " apply selection \"%s\")"
                        , $2, hdql_compound_get_name(hdql_parser_top_compound(ws)), $3);
                    free($2);
                    if($3.selectionExpression) free($3.selectionExpression);
                    if($3.identifiers)         free($3.identifiers);
                    return HDQL_ERR_UNKNOWN_ATTRIBUTE;
                }
                const struct hdql_CollectionAttrInterface * iface
                        = hdql_attr_def_collection_iface(attrDef);
                if(!iface->compile_selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute \"%s\" of `%s' does not support selection"
                          " (can't compile selection expression \"%s\")"
                        , $2, hdql_compound_get_name(hdql_parser_top_compound(ws)), $3);
                    free($2);
                    if($3.selectionExpression) free($3.selectionExpression);
                    if($3.identifiers)         free($3.identifiers);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                hdql_SelectionArgs_t selection = iface->compile_selection($3.selectionExpression
                            , iface->definitionData, ws->context );
                if(NULL == selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "failed to translate selection expression \"%s\" of"
                        " collection attribute %s::%s."
                        , $3, hdql_compound_get_name(hdql_parser_top_compound(ws)), $2);
                    free($2);
                    if($3.selectionExpression) free($3.selectionExpression);
                    if($3.identifiers)         free($3.identifiers);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                free($2);
                if($3.selectionExpression) free($3.selectionExpression);
                if($3.identifiers)         free($3.identifiers);
                $$ = hdql_query_create( attrDef, selection, ws->context);
                assert($$);
            }
            | queryExpr T_PERIOD T_IDENTIFIER
            {
                const struct hdql_AttrDef * attrDef;
                int rc = _resolve_query_top_as_compound($1, $3, &yyloc, ws, &attrDef);
                if(0 != rc) { free($3); return rc; }
                struct hdql_Query * cq = hdql_query_create(attrDef, NULL, ws->context);
                assert(cq);
                free($3);
                $$ = hdql_query_append($1, cq);
            }
            | queryExpr T_PERIOD T_IDENTIFIER selection
            {
                const struct hdql_AttrDef * attrDef;
                int rc = _resolve_query_top_as_compound($1, $3, &yyloc, ws, &attrDef);
                if(0 != rc) {
                    free($3);
                    if($4.selectionExpression) free($4.selectionExpression);
                    if($4.identifiers)         free($4.identifiers);
                    return rc;
                }

                if(is_scalar(attrDef)) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute `%s::%s' is not a collection (can't"
                          " apply selection expression \"%s\")"
                        , hdql_compound_get_name(hdql_attr_def_compound_type_info(attrDef))
                        , $3, $4 );
                    free($3);
                    if($4.selectionExpression) free($4.selectionExpression);
                    if($4.identifiers)         free($4.identifiers);
                    return HDQL_ERR_ATTRIBUTE;
                }

                const struct hdql_CollectionAttrInterface * iface
                        = hdql_attr_def_collection_iface(attrDef);
                if(!iface->compile_selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute \"%s\" of `%s' does not support selection"
                          " (can't compile selection expression \"%s\")"
                        , $3, hdql_compound_get_name(hdql_attr_def_compound_type_info(attrDef)), $4);
                    free($3);
                    if($4.selectionExpression) free($4.selectionExpression);
                    if($4.identifiers)         free($4.identifiers);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                hdql_SelectionArgs_t selection = iface->compile_selection($4.selectionExpression
                            , iface->definitionData, ws->context);
                if(NULL == selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "failed to translate selection expression \"%s\" of"
                        " collection attribute %s::%s."
                        , $4, hdql_compound_get_name(hdql_attr_def_compound_type_info(attrDef)), $3);
                    free($3);
                    if($4.selectionExpression) free($4.selectionExpression);
                    if($4.identifiers)         free($4.identifiers);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                free($3);
                if($4.selectionExpression) free($4.selectionExpression);
                if($4.identifiers)         free($4.identifiers);

                struct hdql_Query * cq = hdql_query_create(attrDef, selection, ws->context);
                assert(cq);
                $$ = hdql_query_append($1, cq);
            }
            | queryExpr T_LCRLBC {
                    const struct hdql_AttrDef * topAttrDef 
                            = hdql_query_top_attr($1);
                    if(is_atomic(topAttrDef)) {
                        // todo: is it so?
                        hdql_error(&yyloc, ws, yyscanner
                                  , "Compound scope operator `{}' can not be"
                                    " applied to atomic value." );
                        return HDQL_BAD_QUERY_EXPRESSION;
                    }
                    int rc = _push_cmpd(ws, hdql_attr_def_compound_type_info(topAttrDef));
                    //int rc = hdql_parser_push(&yyloc, ws, $1);
                    if(0 != rc) return rc;
                } scopedDefs T_RCRLBC {
                    int rc;
                    struct hdql_Query * scopeQuery = _new_virtual_compound_query(
                            &yyloc, ws, yyscanner,
                            $4.compoundPtr, $4.filter);
                    if(NULL == scopeQuery) return HDQL_BAD_QUERY_EXPRESSION;

                    rc = _pop_cmpd(ws);
                    if(0 != rc) return rc;
                    /* append virtual compound query to the query before scope
                     * operator */
                    $$ = hdql_query_append($1, scopeQuery);
                    assert($$ == $1);
                }
            | T_LCRLBC scopedDefs T_RCRLBC {
                    struct hdql_Query * scopeQuery = _new_virtual_compound_query(
                            &yyloc, ws, yyscanner,
                            $2.compoundPtr, $2.filter);
                    if(NULL == scopeQuery) return HDQL_BAD_QUERY_EXPRESSION;
                    /* append virtual compound query to the query before scope
                     * operator */
                    $$ = scopeQuery;
                }
            ;

scopedDefs : vCompoundDef {
                $$.filter = NULL;
           }
           | vCompoundDef T_COLON {
                _push_cmpd(ws, $1.compoundPtr); 
           } aQExpr {
                $$.filter = $4;
                _pop_cmpd(ws);
           }
           | T_COLON aQExpr {
                $$.compoundPtr
                        = hdql_virtual_compound_new(hdql_parser_top_compound(ws), ws->context);
                hdql_context_add_virtual_compound(ws->context, $$.compoundPtr);
                assert(hdql_compound_is_virtual($$.compoundPtr));
                $$.filter = $2;
           }
           ;

vCompoundDef : T_IDENTIFIER T_WALRUS aQExpr {
                /* _substitute_ compound with new virtual one */
                struct hdql_Compound * vCompound
                            = hdql_virtual_compound_new(hdql_parser_top_compound(ws), ws->context);
                hdql_context_add_virtual_compound(ws->context, vCompound);
                assert(hdql_compound_is_virtual(vCompound));
                /* define new attribute of a virtual compound,
                 * named as T_IDENTIFIER with query as a result.
                 * New (virtual) attribute definition inherits what is
                 * provided by query's top attribute */
                struct hdql_AttrDef * newAttrDef
                    = hdql_attr_def_create_fwd_query($3, ws->context);
                /* Add new attribute to virtual compound */
                int rc = hdql_compound_add_attr(vCompound, $1, newAttrDef);
                if(0 != rc) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "failed to define attribute \"%s\" for virtual"
                          " compound type based on `%s'; error code %d"
                        , $1
                        , hdql_compound_get_name(hdql_virtual_compound_get_parent(vCompound))
                        , rc );
                    free($1);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                free($1);
                $$.compoundPtr = vCompound;
            }
            | vCompoundDef T_COMMA {
                ws->compoundStack[ws->compoundStackTop].compoundPtr = $1.compoundPtr;
            } T_IDENTIFIER T_WALRUS aQExpr {
                assert(hdql_compound_is_virtual($1.compoundPtr));
                /* define new attribute of a virtual compound,
                 * named as T_IDENTIFIER with query as a result.
                 * New (virtual) attribute definition inherits what is
                 * provided by query's top attribute */
                struct hdql_AttrDef * newAttrDef
                    = hdql_attr_def_create_fwd_query($6, ws->context);
                /* Add new attribute to virtual compound */
                int rc = hdql_compound_add_attr($1.compoundPtr, $4, newAttrDef);
                if(0 != rc) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "failed to define attribute \"%s\" for virtual"
                          " compound type based on `%s'; error code %d"
                        , $4
                        , hdql_compound_get_name(hdql_virtual_compound_get_parent($1.compoundPtr))
                        , rc );
                    free($4);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                free($4);
                $$.compoundPtr = $1.compoundPtr;
            }
            ;

%%

#include "lex.yy.h"

// ...

/*
 * Parser error function
 * NOTE: `yyerror` is a macro that expands to `hdql_error`
 */
void
hdql_error( YYLTYPE * yylloc
          , Workspace_t ws
          , yyscan_t yyscanner
          , const char * fmt
          , ...
          ) {
    if( ws->errMsgSize && '\0' != ws->errMsg[0] ) {
        fprintf( stderr
               , "New HDQL error message replacing previous,"
                 " at %d,%d - %d,%d: \"%s\".\n"
               , yylloc->first_line, yylloc->first_column
               , yylloc->last_line, yylloc->last_column
               , ws->errMsg );
    }
    if( ws->errMsgSize ) {
        va_list arglist;
        va_start(arglist, fmt);
        vsnprintf(ws->errMsg, ws->errMsgSize, fmt, arglist);
        va_end(arglist);

        ws->errPos[0] = yylloc->first_line;
        ws->errPos[1] = yylloc->first_column;
        ws->errPos[2] = yylloc->last_line;
        ws->errPos[3] = yylloc->last_column;
    } else {
        char s[512];

        va_list arglist;
        va_start(arglist, fmt);
        vsnprintf(s, sizeof(s), fmt, arglist);
        va_end(arglist);

        fprintf( stderr, "HDQL error at %d,%d - %d,%d: %s"
               , yylloc->first_line, yylloc->first_column
               , yylloc->last_line, yylloc->last_column
               , s );
    }

}

int
_push_cmpd(struct Workspace * ws, const struct hdql_Compound * cmpd) {
    if(ws->compoundStackTop + 1 == HDQL_COMPOUNDS_STACK_MAX_DEPTH) {
        return HDQL_ERR_COMPOUNDS_STACK_DEPTH;
    }
    //++(ws->compoundStackTop); // xxx?
    ws->compoundStack[++(ws->compoundStackTop)].compoundPtr = cmpd /*topAttrDef->typeInfo.compound*/;
    ws->compoundStack[   ws->compoundStackTop ].newAttributeName = NULL;
    return 0;
}

int
_pop_cmpd(struct Workspace * ws) {
    if(ws->compoundStackTop == 0) {
        return HDQL_ERR_COMPOUNDS_STACK_DEPTH;
    }
    --(ws->compoundStackTop);
    return 0;
}

const struct hdql_Compound *
hdql_parser_top_compound(struct Workspace * ws) {
    assert(ws->compoundStack[ws->compoundStackTop].compoundPtr);
    return ws->compoundStack[ws->compoundStackTop].compoundPtr;
}

/* An utility function: resolves query list till the topmost compound,
 * correctly accounting for recursive queries */
static int
_resolve_query_top_as_compound( struct hdql_Query * q
                              , char * identifier
                              , YYLTYPE * yyloc
                              , struct Workspace * ws
                              , const struct hdql_AttrDef ** r
                              ) {
    const struct hdql_AttrDef * topAttrDef;
    do {
        topAttrDef = hdql_query_top_attr(q);
        if(is_direct_query(topAttrDef)) break;
        q = hdql_attr_def_fwd_query(topAttrDef);  //topAttrDef->typeInfo.subQuery;
        assert(q);
    } while(is_subquery(topAttrDef));
    if(is_atomic(topAttrDef)) {
        const struct hdql_ValueInterface * vi
            = hdql_types_get_type(hdql_context_get_types(ws->context)
                        , hdql_attr_def_get_atomic_value_type_code(topAttrDef)
                        );
        hdql_error( yyloc, ws, NULL
                  , "query result does not provide"
                    " attributes (can't retrieve attribute \"%s\""
                    " of the %s type)"
                  , identifier, vi ? vi->name : "NULL" );
        *r = NULL;
        return HDQL_ERR_ATTRIBUTE;
    }
    assert(is_dynamic(topAttrDef));  // TODO
    const struct hdql_Compound * compound = hdql_attr_def_compound_type_info(topAttrDef);
    *r = hdql_compound_get_attr(compound, identifier);
    if(NULL == *r) {
        hdql_error(yyloc, ws, NULL
            , "type `%s' has no attribute \"%s\""
            , hdql_compound_get_name(compound), identifier);
        return HDQL_ERR_ATTRIBUTE;
    }
    return 0;  // ok
}  // _resolve_query_top_as_compound()

static hdql_Datum_t _trivial_dereference( hdql_Datum_t d
    , hdql_Datum_t dd
    , struct hdql_CollectionKey * key
    , const hdql_Datum_t defData
    , hdql_Context_t ctx
    ) { return d; }

/* This function gets called upon finalizing scope operator (after `}'
 * in `{...}' and produces filtering or trivial query node that should return
 * a virtual compound instance. Virtual compound itself is built and defined
 * up to the moment this function gets invoked, we just have to pack it into
 * a query node. Not-so-trivial case arises if filtering expression was given
 * in the scope operator. Example:
 *
 *      a.b{c := .d*2, e := .f/2}.e
 *  
 * up to the closing `}' virtual compound has been composed already, we shall
 * provide the parser with query node attributed to this v-compound in order
 * it will be capable to resolve `.e'. Note, this query is always a scalar
 * instance (there is no way in HDQL currently to "split" single instance into
 * a collection of items within the "scope operator"). This scalar is optional
 * as filtering expression can result of the instance to be declined. */
static struct hdql_Query *
_new_virtual_compound_query( YYLTYPE * yylloc
                           , Workspace_t ws
                           , yyscan_t yyscanner
                           , struct hdql_Compound * vCompoundPtr
                           , struct hdql_Query * filterQuery
                           ) {
    #if 1
    struct hdql_ScalarAttrInterface iface;
    if(NULL == filterQuery) {
        bzero(&iface, sizeof(iface));
        iface.dereference = _trivial_dereference;
    } else {
        iface = _hdql_gFilteredCompoundIFace;
        iface.definitionData = (hdql_Datum_t) filterQuery;
    }
    struct hdql_AttrDef * vCompoundAttrDef = hdql_attr_def_create_compound_scalar(
                  vCompoundPtr  /* ... compound ptr */
                , &iface  /* ......... scalar iface ptr */
                , 0x0  /* ............ key type code */
                , NULL  /* ........... key copy callback */
                , ws->context  /* .... context */
                );
    hdql_attr_def_set_stray(vCompoundAttrDef);
    return hdql_query_create(vCompoundAttrDef, NULL, ws->context);
    #else
    /* create "attribute definition" yielding virtual compound
     * instance based on the sub-query */
    struct hdql_AttrDef * vCompoundAttrDef
        = hdql_context_local_attribute_create(ws->context);
    /* - it is of compound type, is scalar (single virtual compound instance is
     *   created based on single parent compound) */
    assert(vCompoundAttrDef->attrFlags == 0x0);  // xxx
    vCompoundAttrDef->typeInfo.compound = compoundPtr;
    if(!filterQuery) {
        /* virtual compound is not filtered, use trivial dereference */
        vCompoundAttrDef->interface.scalar.dereference = trivial_dereference;
    } else {
        /* virtual compound is filtered, allocate supplementary data keeping
         * filtering query and cache its return type */
        struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *)
                hdql_context_alloc(ws->context, sizeof(struct TypedFilterQueryCache)));
        tfqp->q = filterQuery;
        const struct hdql_AttrDef * fQResultTypeAttrDef
            = hdql_query_top_attr(filterQuery);
        
        /* assure filter expression results in proper types */ {
            /* - Filter can not be applied to static value (trivial
             * filtering condition seem to have no sense in the language) */
            if(is_static(fQResultTypeAttrDef)) {
                hdql_error(yylloc, ws, yyscanner, "Filter query result is of"
                    " static value, this (trivial) case is not yet implemented");
                // TODO: support it, ppl might want it for readability or external
                //  parametrisation
                return NULL;
            }
            /* - Filter can not be applied to atomic attribute (no properties) */
            if(is_compound(fQResultTypeAttrDef)) {
                struct hdql_Compound * compound
                        = hdql_attr_def_compound_type_info(fQResultTypeAttrDef);
                if(!hdql_compound_is_virtual(compound)) {
                    hdql_error(yylloc, ws, yyscanner
                                , "Filter query result is of compound type `%s'"
                                  " (logic or arithmetic type expected)"
                                , hdql_compound_get_name(compound));
                } else {
                    hdql_error(yylloc, ws, yyscanner
                                , "Filter query result is of virtual compound type"
                                  " based on type `%s'"
                                  " (logic or arithmetic type expected)"
                                , hdql_compound_get_name(hdql_virtual_compound_get_parent(compound)));
                }
                return NULL;
            }
            /* - forwardign query
             * TODO: either support it, or generate more explainatory error
             *       currently, I can not imagine a construction where it might be
             *       possible */
            if(is_subquery(fQResultTypeAttrDef)) {
                hdql_error(yylloc, ws, yyscanner, "Filter query result provides"
                    " forwarding query (sub-query), which is unforseen by current"
                    " language specification. Please, provide a feedback to"
                    " developer(s) if you see this message.");
                return NULL;
            }
        }  /* otherise, virtual compound is scalar compound or collection of compounds */

        /* check filtering query return type */
        tfqp->vi = hdql_types_get_type( hdql_context_get_types(ws->context)
                , hdql_attr_def_get_atomic_value_type_code(fQResultTypeAttrDef)
                );
        assert(tfqp->vi);

        assert(0);  // TODO

        if(is_collection(fQResultTypeAttrDef)) {
            vCompoundAttrDef->interface.collection.definitionData = ((hdql_Datum_t) tfqp);
            // force to-bool evaluating function here that can
            // accept collections of a single element here
            vCompoundAttrDef->interface.scalar.dereference = filtered_dereference_collection;
            //vCompoundAttrDef->interface.scalar.free_supp_data = free_filter_collection_dereference_supp_data;
        } else {
            //vCompoundAttrDef->interface.scalar.definitionData = ((hdql_Datum_t) tfqp);
            vCompoundAttrDef->interface.scalar.dereference = filtered_dereference_scalar;
            //vCompoundAttrDef->interface.scalar.free_supp_data = free_filter_scalar_dereference_supp_data;
        }
    }
    return hdql_query_create(vCompoundAttrDef, NULL, ws->context);
    #endif
}

#if 1
//static hdql_Datum_t
//filtered_dereference_scalar( hdql_Datum_t d
//                           , hdql_Context_t ctx
//                           , hdql_Datum_t filterQPtr
//                           ) {
//    assert(filterQPtr);
//    // `filtered dereference`-specific arguments
//    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) filterQPtr);
//    assert(tfqp->q);
//    // retrieve logic result using filtering query
//    hdql_query_reset(tfqp->q, d, ctx);  // TODO: check rc
//    hdql_Datum_t r = hdql_query_get(tfqp->q, NULL, ctx);
//    // no result means filter failure
//    if(NULL == r) return NULL;
//    assert(tfqp->vi);
//    assert(tfqp->vi->get_as_logic);
//    // filter must provide scalar of atomic type which has to-logic interface
//    // method implemented
//    hdql_Bool_t passes = tfqp->vi->get_as_logic(r);
//
//    hdql_query_reset(tfqp->q, d, ctx);
//    return passes ? d : r;
//}
//
//static void
//free_filter_scalar_dereference_supp_data(hdql_Datum_t suppData, hdql_Context_t ctx) {
//    if(!suppData) return;
//    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) suppData);
//    hdql_query_destroy(tfqp->q, ctx);
//    hdql_context_free(ctx, suppData);
//}
//
//static hdql_Datum_t
//filtered_dereference_collection( hdql_Datum_t d
//                               , hdql_Context_t ctx
//                               , hdql_Datum_t filterQPtr
//                               ) {
//    assert(filterQPtr);
//    // `filtered dereference`-specific arguments
//    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) filterQPtr);
//    assert(tfqp->q);
//    assert(tfqp->vi);
//    assert(tfqp->vi->get_as_logic);
//    // filter must provide collection of atomic type instances which has
//    // to-logic interface method implemented
//    hdql_Bool_t passes = true;
//    
//    hdql_Datum_t ir;
//    hdql_query_reset(tfqp->q, d, ctx);  // TODO: check rc
//    do {
//        ir = hdql_query_get(tfqp->q, NULL, ctx);
//        if(ir)
//            passes &= tfqp->vi->get_as_logic(ir);
//    } while(ir && passes);
//
//    hdql_query_reset(tfqp->q, d, ctx);
//    return passes ? d : NULL;
//}
//
//static void
//free_filter_collection_dereference_supp_data(hdql_Datum_t suppData, hdql_Context_t ctx) {
//    if(!suppData) return;
//    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) suppData);
//    hdql_query_destroy(tfqp->q, ctx);
//    hdql_context_free(ctx, suppData);
//}
#else
static int
_bind_filter( struct hdql_Query * target
            , struct hdql_Query * filter
            , YYLTYPE * yyloc
            , struct Workspace * ws
            , yyscan_t yyscanner
            ) {
    assert(target);
    assert(filter);
    struct hdql_ValueTypes * vt = hdql_context_get_types(ws->context);
    assert(vt);

    const struct hdql_AttrDef
              * tTopAttrDef = hdql_query_top_attr(target)
            , * fTopAttrDef = hdql_query_top_attr(filter)
            ;
    assert(NULL != tTopAttrDef);

    assert(!tTopAttrDef->isSubQuery);
    assert(tTopAttrDef->isCollection);

    /* Applying filter expression to atomic value is nonsense */
    if(tTopAttrDef->isAtomic) {
        const struct hdql_ValueInterface * tqvi
                = hdql_types_get_type(vt, tTopAttrDef->typeInfo.atomic.arithTypeCode);
        hdql_error( yyloc, ws, yyscanner
                  , "filter can not be applied to an atomic type (query result"
                    " is of type %s)", tqvi ? tqvi->name : "(no type name)" );
        return HDQL_ERR_ATTR_FILTER;
    }

    assert(NULL != fTopAttrDef);

    assert(!fTopAttrDef->staticValueFlags);
    assert(!fTopAttrDef->isSubQuery);
    //^^^ TODO: apparently, valid cases, but one need to rewrite code below

    /* filter query has to result in a scalar, atomic value */
    if(!fTopAttrDef->isAtomic) {
        const struct hdql_ValueInterface * fqvi
                = hdql_types_get_type(vt, fTopAttrDef->typeInfo.atomic.arithTypeCode);
        const char * fCompoundName = hdql_compound_get_name(fTopAttrDef->typeInfo.compound);
        hdql_error( yyloc, ws, yyscanner
                  , "filter query result type is of type %s -- can't be evaluated to bool"
                  ,  fCompoundName );
        return HDQL_ERR_ATTR_FILTER;
    }
    //hdql_ValueInterface * vi = hdql_types_get_type(vt, tTopAttrDef->isCollection);
    return hdql_query_bind_filter(target, filter, ws->context);
}
#endif

static int
_operation( struct hdql_Query * a
          , hdql_OperationCode_t opCode
          , struct hdql_Query * b
          , YYLTYPE * yyloc
          , struct Workspace * ws
          , yyscan_t yyscanner
          , const char * opDescription
          , struct hdql_Query ** r
          ) {
    assert(ws);
    assert(ws->context);
    const struct hdql_AttrDef
        * attrA =     hdql_query_top_attr(a),
        * attrB = b ? hdql_query_top_attr(b) : NULL;
    const struct hdql_ValueTypes * types
        = hdql_context_get_types(ws->context);
    assert(types);
    /* these assertions must be fullfilled by `hdql_query_top_attr()` */
    assert(                 is_direct_query(attrA));
    assert(attrB == NULL || is_direct_query(attrB));
    /* arithmetic operations on compounds are prohibited (at least in current
     * HDQL specifications */
    if(is_compound(attrA)) {
        hdql_error(yyloc, ws, NULL
                , "%soperand of %s operator is of compound type `%s')"
                  " (can't use compound instance in arithmetics)"
                , opDescription
                , b ? "first " : ""
                , hdql_compound_get_name(hdql_attr_def_compound_type_info(attrA)));
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;
    }
    if(attrB && is_compound(attrB)) {
        hdql_error(yyloc, ws, NULL
                , "second operand of %s operator is of compound type `%s'"
                  " (can't use compound instance in arithmetics)"
                , opDescription
                , hdql_compound_get_name(hdql_attr_def_compound_type_info(attrB)));
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;
    }
    /* Figure out query types, this time not just top attribute definition,
     * but query result as a whole. If all queries in a chain returns  */
    bool attrAIsFullScalar =     hdql_query_is_fully_scalar(a)
       , attrBIsFullScalar = b ? hdql_query_is_fully_scalar(b) : true;
    /* in current version arithmetic operations on both collection operands
     * are prohibited */
    if((!attrAIsFullScalar) && (!attrBIsFullScalar)) {
        hdql_error(yyloc, ws, NULL
                , "can't apply %s arithmetic operator to arguments which are both collections"
                , opDescription );
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;
    }

    /* obtain the arithmetic operator implementation based on type code(s) for
     * operand(s) */
    hdql_ValueTypeCode_t codeA =         hdql_attr_def_get_atomic_value_type_code(attrA)
                       , codeB = attrB ? hdql_attr_def_get_atomic_value_type_code(attrB) : 0x0
                       ;
    const struct hdql_OperationEvaluator * evaluator
            = hdql_op_get(hdql_context_get_operations(ws->context), codeA, opCode, codeB );
    if(NULL == evaluator) {
        if(attrA) {
            const struct hdql_ValueInterface * viA = hdql_types_get_type(types, codeA)
                                           , * viB = hdql_types_get_type(types, codeB)
                                     ;
            hdql_error( yyloc, ws, NULL
                      , "%s operator is not defined for operands of types %s and %s"
                      , opDescription
                      , viA ? viA->name : "(null type)"
                      , viB ? viB->name : "(null type)"
                      );
            return HDQL_ERR_OPERATION_NOT_SUPPORTED;
        } else {
            const struct hdql_ValueInterface * viA = hdql_types_get_type(types, codeA);
            hdql_error( yyloc, ws, NULL
                      , "%s operator is not defined for operand of type %s"
                      , opDescription
                      , viA ? viA->name : "(null type)"
                      );
            return HDQL_ERR_OPERATION_NOT_SUPPORTED;
        }
    }

    /* check, if query can be calculated statically (i.e., consists only
     * from constant values. In this case we calculate the value prior to
     * execution to save CPU... */
    if( is_static(attrA)
     && ((!attrB) || is_static(attrB))) {
        /* ...in such a case, we create new trivial query calculated from static
         * values, free operands and return the new one. Both nodes do not
         * need an owner object, so we do not rely on query resolution
         * mechanics here, by directly retrieving values from attr defs and
         * making an op */
        #if 0
        const hdql_Datum_t valueA =     hdql_query_get(a, NULL, ws->context)
                         , valueB = b ? hdql_query_get(b, NULL, ws->context) : NULL
                         ;
        #else
        const hdql_Datum_t valueA =     hdql_attr_def_get_static_value(attrA)
                         , valueB = b ? hdql_attr_def_get_static_value(attrB) : NULL
                         ;
        #endif
        assert(valueA);
        assert((!b) || NULL != valueB);
        hdql_Datum_t result = hdql_create_value(evaluator->returnType, ws->context);
        
        char errBf[128];
        int rc = evaluator->op(valueA, valueB, result);
        //int rc = hdql_op_eval( valueA, evaluator, valueB, result);

        if(0 != rc) {
            hdql_error( yyloc, ws, NULL, "%s %s", opDescription, errBf );
            hdql_destroy_value(evaluator->returnType, result, ws->context);
            return HDQL_ERR_ARITH_OPERATION;
        }

        /* new value calculated, create the result */
        struct hdql_AttrDef * resultAD
            = hdql_attr_def_create_static_atomic_scalar_value(evaluator->returnType, result, ws->context);
        hdql_attr_def_set_stray(resultAD);
        *r = hdql_query_create(resultAD, NULL, ws->context);
        assert(*r);
        /* destroy sub-queries */
        hdql_query_destroy(a, ws->context);
        if(b) hdql_query_destroy(b, ws->context);
        /* TODO (?): destroy owned attribute definitions */
        return 0;
    }
    /* ...otherwise, create operation node */
    struct hdql_AtomicTypeFeatures typeInfo;
    typeInfo.isReadOnly = 0x1; /* result is RO */
    typeInfo.arithTypeCode = evaluator->returnType;  /* result type defined by arith op */
    struct hdql_AttrDef * rAD;
    struct hdql_ArithOpDefData * defData = (struct hdql_ArithOpDefData *) hdql_context_alloc(
            ws->context, sizeof(struct hdql_ArithOpDefData));
    assert(defData);
    defData->args[0] = a;
    defData->args[1] = b;
    defData->evaluator = evaluator;
    if(attrAIsFullScalar && attrBIsFullScalar) {
        /* operation results in scalar */
        struct hdql_ScalarAttrInterface scalarIFace = _hdql_gScalarArithOpIFace;
        scalarIFace.definitionData = (hdql_Datum_t) defData;
        rAD = hdql_attr_def_create_atomic_scalar(
                &typeInfo, &scalarIFace, 0x0, NULL, ws->context );
    } else {
        /* operation results in collection */
        struct hdql_CollectionAttrInterface collectionIFace = _hdql_gCollectionArithOpIFace;
        collectionIFace.definitionData = (hdql_Datum_t) defData;
        rAD = hdql_attr_def_create_atomic_collection(
                  &typeInfo, &collectionIFace, 0x0
                , hdql_reserve_arith_op_collection_key, ws->context );
    }
    hdql_attr_def_set_stray(rAD);
    *r = hdql_query_create(rAD, NULL, ws->context);
    return 0;
}  /* _operation() */

static struct hdql_Query *
_new_function( YYLTYPE * yyloc, struct Workspace * ws, yyscan_t yyscanner
             , const char * funcName
             , struct hdql_FuncArgList * argsList
             ) {
    /* get the total number of arguments for overloaded functions */
    size_t nArgs = 0;
    for(struct hdql_FuncArgList * cArg = argsList; ; cArg = cArg->nextArgument ) {
        ++nArgs;
        if(NULL == cArg->nextArgument) break;
    }
    /* build plain temporary array of arguments */
    struct hdql_Query ** argsArray = malloc(sizeof(struct hdql_Query *)*(nArgs + 1));
    size_t nArg = 0;
    for(struct hdql_FuncArgList * cArg = argsList; NULL != cArg; ) {
        argsArray[nArg++] = cArg->thisArgument;
        struct hdql_FuncArgList * toFree = cArg;
        cArg = cArg->nextArgument;
        free(toFree);
    }
    argsArray[nArgs] = NULL;
    /* Retrieve functions dictionary and try to to instantiate function object */
    struct hdql_Functions * fDict = hdql_context_get_functions(ws->context);
    assert(fDict);
    struct hdql_AttrDef * fAD;
    int rc = hdql_functions_resolve(fDict, funcName
            , argsArray, &fAD, ws->context);
    free(argsArray);  /* free tmp args array */
    /* Functions construction may fail, in this case we should provide the
     * user with explainatory information on which arguments were lookup
     * performed */
    if( HDQL_ERR_CODE_OK != rc ) {
        //assert(0);  // TODO: dump arg queries description
        //hdql_query_dump();
        hdql_error( yyloc, ws, NULL
                  , "Failed to instantiate function object %s(...): %s"
                  , funcName, hdql_err_str(rc)
                  );
        return NULL;
    }
    assert(fAD);
    hdql_attr_def_set_stray(fAD);
    /* Otherwise, create a new query object wrapping "function attribute
     * definition */
    return hdql_query_create(fAD, NULL, ws->context);
}  /* _new_function() */

/* 
 * Main parser function
 */
struct hdql_Query *
hdql_compile_query( const char * strexpr
                  , struct hdql_Compound * rootCompound
                  , struct hdql_Context * ctx
                  , char * errBuf
                  , unsigned int errBufLength
                  , int * errDetails
                  ) {
    struct Workspace ws;
    /* inti buffer for error message */
    ws.errMsg     = errBuf;
    if(ws.errMsg) *ws.errMsg  = '\0';
    ws.errMsgSize = errBufLength;
    /* make user-provided context available for subsequent calls */
    ws.context = ctx;
    /* set data type lookup table */
    ws.compoundStack[0].compoundPtr = rootCompound;
    ws.compoundStack[0].newAttributeName = NULL;
    ws.compoundStackTop = 0;
    /* init result */
    ws.query = NULL;

    /* do lex scanning */
    void * scannerPtr;
    yylex_init(&scannerPtr);
    struct yy_buffer_state * buffer = yy_scan_string(strexpr, scannerPtr);
    /* parse scanned */
    int rc = yyparse(&ws, scannerPtr);
    /* copy error details on failure */
    if(0 != (errDetails[0] = rc)) {
        errDetails[1] = ws.errPos[0];       errDetails[2] = ws.errPos[1];
        errDetails[3] = ws.errPos[2];       errDetails[4] = ws.errPos[3];
    }
    /* free scanner buffer, destroy scanner */
    yy_delete_buffer(buffer, scannerPtr);
    yylex_destroy(scannerPtr);

    return ws.query;
}

