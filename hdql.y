%code requires {

#include "hdql/types.h"
#include "hdql/query.h"
#include "hdql/function.h"
#include "hdql/errors.h"

struct hdql_FuncArgList {
    struct hdql_Query * thisArgument;
    struct hdql_FuncArgList * nextArgument;
};

typedef struct Workspace {
    struct {
        struct hdql_Compound * compoundPtr;
        char * newAttributeName;
        hdql_AttrIdx_t vLastIdx;
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
static hdql_Datum_t trivial_dereference( hdql_Datum_t d
    , hdql_Datum_t
    , struct hdql_CollectionKey *
    , const hdql_Datum_t
    , hdql_Context_t
    ) { return d; }
struct TypedFilterQueryCache {
    struct hdql_Query * q;
    const struct hdql_ValueInterface * vi;
};
static hdql_Datum_t filtered_dereference_scalar(hdql_Datum_t d, hdql_Context_t, hdql_Datum_t);
static void free_filter_scalar_dereference_supp_data(hdql_Datum_t, hdql_Context_t);
static hdql_Datum_t filtered_dereference_collection(hdql_Datum_t d, hdql_Context_t, hdql_Datum_t);
static void free_filter_collection_dereference_supp_data(hdql_Datum_t, hdql_Context_t);
static int
_resolve_query_top_as_compound( struct hdql_Query * q
                              , char * identifier
                              , YYLTYPE * yyloc
                              , struct Workspace * ws
                              , const struct hdql_AttributeDefinition ** r
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

#define is_atomic(attr)     (attr->attrFlags & hdql_kAttrIsAtomic)
#define is_collection(attr) (attr->attrFlags & hdql_kAttrIsCollection)
#define is_static(attr)     (attr->attrFlags & hdql_kAttrIsStaticValue)
#define is_subquery(attr)   (attr->attrFlags & hdql_kAttrIsSubquery)

#define is_compound(attr)   (!is_atomic(attr))
#define is_scalar(attr)     (!is_collection(attr))
#define is_dynamic(attr)    (!is_static(attr))
#define is_direct_query(attr) (!is_subquery(attr))

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

int _push_cmpd(struct Workspace *, struct hdql_Compound * cmpd);
int _pop_cmpd(struct Workspace *);
struct hdql_Compound * hdql_parser_top_compound(struct Workspace *);

}

%union {
    char * strID;
    char * selexpr;
    hdql_Int_t intStaticValue;
    hdql_Flt_t fltStaticValue;
    struct { struct hdql_Compound * compoundPtr; size_t lastIdx; struct hdql_Query * filter; } vCompound;
    struct hdql_Query * queryPtr;
    struct hdql_FuncArgList * funcArgsList;
    // ... function
}

%token T_PLUS "+" T_MINUS "-" T_ASTERISK "*" T_SLASH "/" T_PERC "%" T_RBSHIFT ">>" T_LBSHIFT "<<"
%token T_DBL_ASTERISK "**" T_TILDE "~"
%token T_DBL_CAP "^^" T_DBL_AMP "&&" T_DBL_PIPE "||"
%token T_LBC "(" T_RBC ")" T_LSQBC "[" T_RSQBC "]" T_LCRLBC "{" T_RCRLBC "}"
%token T_EXCLMM "!" T_QUESTIONMM "?" T_COLON ":"
%token T_GT ">" T_GTE ">=" T_LT "<" T_LTE "<=" T_EQ "==" T_NE "!=" T_WALRUS ":="
%token T_AMP "&" T_PIPE "|" T_CAP "^"
%token T_INCREMENT "++" T_DECREMENT "--"
%token T_PLUSE "+=" T_MINUSE "-=" T_RBSHIFTE ">>=" T_LBSHIFTE "<<="
%token T_SEMICOLON T_COMMA "," T_PERIOD "."

%token T_INVALID_LITERAL "invalid literal"
%token<strID> T_IDENTIFIER "identifier"
%token<selexpr> T_SELECTION_EXPRESSION "selection expression"
%token<intStaticValue> T_INT_STATIC_VALUE "integer value"
%token<fltStaticValue> T_FLT_STATIC_VALUE "floating point value"

%type<queryPtr> queryExpr aOp aQExpr;
%type<vCompound> vCompoundDef scopedDefs;
%type<funcArgsList> argsList;
//%type<cmpOpFunc> cmpOp;
//%type<cmpExpr> ftCompExpr;
//%type<selectionBinOp> selExpr;

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
%left T_RSQBC T_LSQBC
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
                struct hdql_AttributeDefinition * attrDef
                        = hdql_context_local_attribute_create(ws->context);
                assert(attrDef->attrFlags == 0x0);
                attrDef->attrFlags |= hdql_kAttrIsAtomic | hdql_kAttrIsStaticValue;
                attrDef->interface.scalar.dereference = trivial_dereference;
                attrDef->typeInfo.staticValue.typeCode = hdql_types_get_type_code(
                        hdql_context_get_types(ws->context), "hdql_Flt_t");
                assert(attrDef->typeInfo.staticValue.typeCode);
                attrDef->typeInfo.staticValue.datum = hdql_context_alloc(ws->context, sizeof(hdql_Flt_t));
                if( NULL == attrDef->typeInfo.staticValue.datum ) {
                    hdql_error(&yyloc, ws, yyscanner
                            , "Failed to allocate memory for floating point constant value");
                    return HDQL_ERR_MEMORY;
                }
                *((hdql_Flt_t *) attrDef->typeInfo.staticValue.datum) = $1;
                $$ = hdql_query_create(attrDef, NULL, ws->context);
                // todo: ^^^ check query creation result
                assert($$);
            }
            | T_INT_STATIC_VALUE
            {
                struct hdql_AttributeDefinition * attrDef
                        = hdql_context_local_attribute_create(ws->context);
                assert(attrDef->attrFlags == 0x0);
                attrDef->attrFlags |= hdql_kAttrIsAtomic | hdql_kAttrIsStaticValue;
                attrDef->interface.scalar.dereference = trivial_dereference;
                attrDef->typeInfo.staticValue.typeCode = hdql_types_get_type_code(
                        hdql_context_get_types(ws->context), "hdql_Int_t");
                assert(attrDef->typeInfo.staticValue.typeCode);
                attrDef->typeInfo.staticValue.datum = hdql_context_alloc(ws->context, sizeof(hdql_Int_t));
                if( NULL == attrDef->typeInfo.staticValue.datum ) {
                    hdql_error(&yyloc, ws, yyscanner
                            , "Failed to allocate memory for integer constant value");
                }
                *((hdql_Int_t *) attrDef->typeInfo.staticValue.datum) = $1;
                $$ = hdql_query_create(attrDef, NULL, ws->context);
                // todo: ^^^ check query creation result
                assert($$);
            }
            | queryExpr { $$ = $1; }
            ;

  queryExpr : T_PERIOD T_IDENTIFIER
            {
                const struct hdql_AttributeDefinition * attrDef = hdql_compound_get_attr(
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
            | T_PERIOD T_IDENTIFIER T_SELECTION_EXPRESSION
            {   
                const struct hdql_AttributeDefinition * attrDef = hdql_compound_get_attr(
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
                    free($3);
                    return HDQL_ERR_UNKNOWN_ATTRIBUTE;
                }
                if(is_scalar(attrDef)) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute \"%s\" of `%s' is not a collection (can't"
                          " apply selection \"%s\")"
                        , $2, hdql_compound_get_name(hdql_parser_top_compound(ws)), $3);
                    free($2);
                    free($3);
                    return HDQL_ERR_UNKNOWN_ATTRIBUTE;
                }
                char qcErrBuf[HDQL_SELEXPR_ERROR_BUFFER_SIZE];
                if(!attrDef->interface.collection.compile_selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute \"%s\" of `%s' does not support selection"
                          " (can't compile selection expression \"%s\")"
                        , $2, hdql_compound_get_name(hdql_parser_top_compound(ws)), $3);
                    free($2);
                    free($3);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                hdql_SelectionArgs_t selection = attrDef->interface.collection.compile_selection($3
                            , attrDef, ws->context, qcErrBuf, sizeof(qcErrBuf) );
                if(NULL == selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "failed to translate selection expression \"%s\" of"
                        " collection attribute %s::%s: %s"
                        , $3, hdql_compound_get_name(hdql_parser_top_compound(ws)), $2, qcErrBuf);
                    free($2);
                    free($3);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                free($2);
                free($3);
                $$ = hdql_query_create( attrDef, selection, ws->context);
                assert($$);
            }
            | queryExpr T_PERIOD T_IDENTIFIER
            {
                const struct hdql_AttributeDefinition * attrDef;
                int rc = _resolve_query_top_as_compound($1, $3, &yyloc, ws, &attrDef);
                if(0 != rc) { free($3); return rc; }
                struct hdql_Query * cq = hdql_query_create(attrDef, NULL, ws->context);
                assert(cq);
                free($3);
                $$ = hdql_query_append($1, cq);
            }
            | queryExpr T_PERIOD T_IDENTIFIER T_SELECTION_EXPRESSION
            {
                const struct hdql_AttributeDefinition * attrDef;
                int rc = _resolve_query_top_as_compound($1, $3, &yyloc, ws, &attrDef);
                if(0 != rc) { free($3); free($4); return rc; }

                if(is_scalar(attrDef)) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute `%s::%s' is not a collection (can't"
                          " apply selection expression \"%s\")"
                        , hdql_compound_get_name(attrDef->typeInfo.compound)
                        , $3, $4 );
                    free($3);
                    free($4);
                    return HDQL_ERR_ATTRIBUTE;
                }

                char qcErrBuf[HDQL_SELEXPR_ERROR_BUFFER_SIZE];
                if(!attrDef->interface.collection.compile_selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "attribute \"%s\" of `%s' does not support selection"
                          " (can't compile selection expression \"%s\")"
                        , $3, hdql_compound_get_name(attrDef->typeInfo.compound), $4);
                    free($3);
                    free($4);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                hdql_SelectionArgs_t selection = attrDef->interface.collection.compile_selection($4
                            , attrDef, ws->context, qcErrBuf, sizeof(qcErrBuf) );
                if(NULL == selection) {
                    hdql_error(&yyloc, ws, yyscanner
                        , "failed to translate selection expression \"%s\" of"
                        " collection attribute %s::%s: %s"
                        , $4, hdql_compound_get_name(attrDef->typeInfo.compound), $3, qcErrBuf);
                    free($3);
                    free($4);
                    return HDQL_BAD_QUERY_EXPRESSION;
                }
                free($3);
                free($4);

                struct hdql_Query * cq = hdql_query_create(attrDef, selection, ws->context);
                assert(cq);
                $$ = hdql_query_append($1, cq);
            }
            | queryExpr T_LCRLBC {
                    const struct hdql_AttributeDefinition * topAttrDef 
                            = hdql_query_top_attr($1);
                    if(is_atomic(topAttrDef)) {
                        // todo: is it so?
                        hdql_error(&yyloc, ws, yyscanner
                                  , "Compound scope operator `{}' can not be"
                                    " applied to atomic value." );
                        return HDQL_BAD_QUERY_EXPRESSION;
                    }
                    int rc = _push_cmpd(ws, topAttrDef->typeInfo.compound);
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
                struct hdql_AttributeDefinition newAttrDef;
                hdql_attribute_definition_init(&newAttrDef);
                newAttrDef.interface.collection = gSubQueryInterface;
                newAttrDef.typeInfo.subQuery = $3;
                /* (!) sub-query results always considered as collection */
                newAttrDef.attrFlags = hdql_kAttrIsCollection | hdql_kAttrIsSubquery;
                if(hdql_query_top_attr($3)->attrFlags & hdql_kAttrIsAtomic) {
                    newAttrDef.attrFlags |= hdql_kAttrIsAtomic;
                }
                /* Add new attribute to virtual compound */
                int rc = hdql_compound_add_attr(vCompound, $1, 0, &newAttrDef);
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
                $$.lastIdx = 1;
            }
            | vCompoundDef T_COMMA {
                ws->compoundStack[ws->compoundStackTop].compoundPtr = $1.compoundPtr;
            } T_IDENTIFIER T_WALRUS aQExpr {
                assert(hdql_compound_is_virtual($1.compoundPtr));
                assert($1.lastIdx > 0);
                /* define new attribute of a virtual compound,
                 * named as T_IDENTIFIER with query as a result.
                 * New (virtual) attribute definition inherits what is
                 * provided by query's top attribute */
                struct hdql_AttributeDefinition newAttrDef;
                hdql_attribute_definition_init(&newAttrDef);
                newAttrDef.interface.collection = gSubQueryInterface;
                newAttrDef.typeInfo.subQuery = $6;
                /* (!) sub-query results always considered as collection */
                newAttrDef.attrFlags = hdql_kAttrIsCollection | hdql_kAttrIsSubquery;
                if(is_atomic(hdql_query_top_attr($6))) {
                    newAttrDef.attrFlags |= hdql_kAttrIsAtomic;
                }
                /* Add new attribute to virtual compound */
                int rc = hdql_compound_add_attr($1.compoundPtr, $4, $$.lastIdx, &newAttrDef);
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
                $$.lastIdx = $1.lastIdx + 1;
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
_push_cmpd(struct Workspace * ws, struct hdql_Compound * cmpd) {
    if(ws->compoundStackTop + 1 == HDQL_COMPOUNDS_STACK_MAX_DEPTH) {
        return HDQL_ERR_COMPOUNDS_STACK_DEPTH;
    }
    //++(ws->compoundStackTop); // xxx?
    ws->compoundStack[++(ws->compoundStackTop)].compoundPtr = cmpd /*topAttrDef->typeInfo.compound*/;
    ws->compoundStack[   ws->compoundStackTop ].newAttributeName = NULL;
    ws->compoundStack[   ws->compoundStackTop ].vLastIdx = 0;
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

struct hdql_Compound *
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
                              , const struct hdql_AttributeDefinition ** r
                              ) {
    const struct hdql_AttributeDefinition * topAttrDef;
    do {
        topAttrDef = hdql_query_top_attr(q);
        if(is_direct_query(topAttrDef)) break;
        q = topAttrDef->typeInfo.subQuery;
        assert(q);
    } while(is_subquery(topAttrDef));
    if(is_atomic(topAttrDef)) {
        const struct hdql_ValueInterface * vi
            = hdql_types_get_type(hdql_context_get_types(ws->context)
                        , topAttrDef->typeInfo.atomic.arithTypeCode);
        hdql_error( yyloc, ws, NULL
                  , "query result does not provide"
                    " attributes (can't retrieve attribute \"%s\""
                    " of the %s type)"
                  , identifier, vi ? vi->name : "NULL" );
        *r = NULL;
        return HDQL_ERR_ATTRIBUTE;
    }
    assert(is_dynamic(topAttrDef));  // TODO
    *r = hdql_compound_get_attr(topAttrDef->typeInfo.compound, identifier);
    if(NULL == *r) {
        hdql_error(yyloc, ws, NULL
            , "type `%s' has no attribute \"%s\""
            , hdql_compound_get_name(topAttrDef->typeInfo.compound), identifier);
        return HDQL_ERR_ATTRIBUTE;
    }
    return 0;  // ok
}

static struct hdql_Query *
_new_virtual_compound_query( YYLTYPE * yylloc
                           , Workspace_t ws
                           , yyscan_t yyscanner
                           , struct hdql_Compound * compoundPtr
                           , struct hdql_Query * filterQuery
                           ) {
    /* create "attribute definition" yielding virtual compound
     * instance based on the sub-query */
    struct hdql_AttributeDefinition * vCompoundAttrDef
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
        const struct hdql_AttributeDefinition * fQResultTypeAttrDef
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
                if(!hdql_compound_is_virtual(fQResultTypeAttrDef->typeInfo.compound)) {
                    hdql_error(yylloc, ws, yyscanner
                                , "Filter query result is of compound type `%s'"
                                  " (logic or arithmetic type expected)"
                                , hdql_compound_get_name(fQResultTypeAttrDef->typeInfo.compound));
                } else {
                    hdql_error(yylloc, ws, yyscanner
                                , "Filter query result is of virtual compound type"
                                  " based on type `%s'"
                                  " (logic or arithmetic type expected)"
                                , hdql_compound_get_name(hdql_virtual_compound_get_parent(fQResultTypeAttrDef->typeInfo.compound)));
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
                , fQResultTypeAttrDef->typeInfo.atomic.arithTypeCode
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
}

#if 1
static hdql_Datum_t
filtered_dereference_scalar( hdql_Datum_t d
                           , hdql_Context_t ctx
                           , hdql_Datum_t filterQPtr
                           ) {
    assert(filterQPtr);
    // `filtered dereference`-specific arguments
    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) filterQPtr);
    assert(tfqp->q);
    // retrieve logic result using filtering query
    hdql_Datum_t r = hdql_query_get(tfqp->q, d, NULL, ctx);
    // no result means filter failure
    if(NULL == r) return NULL;
    assert(tfqp->vi);
    assert(tfqp->vi->get_as_logic);
    // filter must provide scalar of atomic type which has to-logic interface
    // method implemented
    hdql_Bool_t passes = tfqp->vi->get_as_logic(r);

    hdql_query_reset(tfqp->q, d, ctx);
    return passes ? d : r;
}

static void
free_filter_scalar_dereference_supp_data(hdql_Datum_t suppData, hdql_Context_t ctx) {
    if(!suppData) return;
    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) suppData);
    hdql_query_destroy(tfqp->q, ctx);
    hdql_context_free(ctx, suppData);
}

static hdql_Datum_t
filtered_dereference_collection( hdql_Datum_t d
                               , hdql_Context_t ctx
                               , hdql_Datum_t filterQPtr
                               ) {
    assert(filterQPtr);
    // `filtered dereference`-specific arguments
    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) filterQPtr);
    assert(tfqp->q);
    assert(tfqp->vi);
    assert(tfqp->vi->get_as_logic);
    // filter must provide collection of atomic type instances which has
    // to-logic interface method implemented
    hdql_Bool_t passes = true;
    
    hdql_Datum_t ir;
    do {
        ir = hdql_query_get(tfqp->q, d, NULL, ctx);
        if(ir)
            passes &= tfqp->vi->get_as_logic(ir);
    } while(ir && passes);

    hdql_query_reset(tfqp->q, d, ctx);
    return passes ? d : NULL;
}

static void
free_filter_collection_dereference_supp_data(hdql_Datum_t suppData, hdql_Context_t ctx) {
    if(!suppData) return;
    struct TypedFilterQueryCache * tfqp = ((struct TypedFilterQueryCache *) suppData);
    hdql_query_destroy(tfqp->q, ctx);
    hdql_context_free(ctx, suppData);
}
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

    const struct hdql_AttributeDefinition
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
    const struct hdql_AttributeDefinition
        * attrA = hdql_query_top_attr(a),
        * attrB = b ? hdql_query_top_attr(b) : NULL;
    const struct hdql_ValueTypes * types
        = hdql_context_get_types(ws->context);
    assert(types);
    /* these assertions must be fullfilled by `hdql_query_top_attr()` */
    assert(is_direct_query(attrA));
    assert(attrB == NULL || is_direct_query(attrB));
    /* in current version arithmetic operations on compounds are prohibited */
    if(is_compound(attrA)) {
        hdql_error(yyloc, ws, NULL
                , "%soperand of %s operator is of compound type `%s')"
                  " (can't use compound instance in arithmetics)"
                , opDescription
                , b ? "first " : ""
                , hdql_compound_get_name(attrB->typeInfo.compound));
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;
    }
    if(attrB && is_compound(attrB)) {
        hdql_error(yyloc, ws, NULL
                , "second operand of %s operator is of compound type `%s'"
                  " (can't use compound instance in arithmetics)"
                , opDescription
                , hdql_compound_get_name(attrB->typeInfo.compound));
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;
    }
    /* Figure out query types, this time not just top attribute definition,
     * but query result as a whole. If all queries in a chain returns  */
    bool attrAIsFullScalar = hdql_query_is_fully_scalar(a)
       , attrBIsFullScalar = b ? hdql_query_is_fully_scalar(b) : true;
    /* in current version arithmetic operations on both collection operands
     * are prohibited */
    if((!attrAIsFullScalar) && (!attrBIsFullScalar)) {
        hdql_error(yyloc, ws, NULL
                , "both operands of %s arithmetic operator are collections"
                , opDescription );
        return HDQL_ERR_OPERATION_NOT_SUPPORTED;
    }

    /* obtain the arithmetic operator implementation based on type code(s) for
     * operand(s) */
    hdql_ValueTypeCode_t codeA, codeB;
    if(is_static(attrA)) {
        codeA = attrA->typeInfo.staticValue.typeCode;
    } else {
        assert(attrA);
        codeA = attrA->typeInfo.atomic.arithTypeCode;
    }
    
    if(attrB) {
        if(is_static(attrB)) {
            codeB = attrB->typeInfo.staticValue.typeCode;
        } else {
            assert(attrB);
            codeB = attrB->typeInfo.atomic.arithTypeCode;
        }
    } else {
        codeB = 0x0;
    }

    const struct hdql_OperationEvaluator * evaluator
            = hdql_op_get(hdql_context_get_operations(ws->context), codeA, opCode, codeB );
    if(NULL == evaluator) {
        if(codeB) {
            hdql_error( yyloc, ws, NULL
                      , "%s operator is not defined for operands of types %s and %s"
                      , opDescription
                      , hdql_types_get_type(types, codeA)->name
                      , hdql_types_get_type(types, codeB)->name
                      );
            return HDQL_ERR_OPERATION_NOT_SUPPORTED;
        } else {
            hdql_error( yyloc, ws, NULL
                      , opDescription
                      , "%s operator is not defined for operand of type %s"
                      , hdql_types_get_type(types, codeA)->name
                      );
            return HDQL_ERR_OPERATION_NOT_SUPPORTED;
        }
    }

    /* check, if query can be calculated statically (i.e., consists only
     * from constant values. In this case we calculate the value prior to
     * execution to save CPU.
     * In such a case, we create new trivial query calculated from static
     * values, free operands and return the new one */
    if( is_static(attrA)
     && ((!attrB) || is_static(attrB))) {
        const hdql_Datum_t valueA = hdql_query_get(a, NULL, NULL, ws->context)
                         , valueB = b ? hdql_query_get(b, NULL, NULL, ws->context) : NULL
                         ;
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
        struct hdql_AttributeDefinition * attrDef
                        = hdql_context_local_attribute_create(ws->context);
        attrDef->attrFlags = hdql_kAttrIsAtomic | hdql_kAttrIsStaticValue;
        attrDef->interface.scalar.dereference = trivial_dereference;
        attrDef->typeInfo.staticValue.typeCode = evaluator->returnType;
        attrDef->typeInfo.staticValue.datum = result;
        *r = hdql_query_create(attrDef, NULL, ws->context);
        assert(*r);
        /* destroy sub-queries */
        hdql_query_destroy(a, ws->context);
        if(b) {
            hdql_query_destroy(b, ws->context);
        }
        /* TODO (?): destroy owned attribute definitions */
        return 0;
    }

    /* otherwise, create operation node */
    assert(is_atomic(attrA) && ((!attrB) || is_atomic(attrB)));
    struct hdql_AttributeDefinition * opAttrDef
            = hdql_context_local_attribute_create(ws->context);

    /* arithmetic operations are defined for atomics only */
    opAttrDef->attrFlags = hdql_kAttrIsAtomic;

    opAttrDef->typeInfo.atomic.isReadOnly = 0x1;  /* result is RO */
    opAttrDef->typeInfo.atomic.arithTypeCode = evaluator->returnType;  /* result type defined by arith op */

    #if 0
    if(attrAIsFullScalar && attrBIsFullScalar) {
        /* arithmetic operation on scalar values */
        hdql_Datum_t scalarOp = hdql_scalar_arith_op_create(a, b, evaluator, ws->context);

        assert(0);  // TODO
        opAttrDef->interface.scalar.suppData = (hdql_Datum_t) scalarOp;
        opAttrDef->interface.scalar.free_supp_data = hdql_scalar_arith_op_free;
        opAttrDef->interface.scalar.dereference = hdql_scalar_arith_op_dereference;

        *r = hdql_query_create(opAttrDef, NULL, ws->context);
    } else {
        /* new node must be of a collection type as one of the operands
         * is a collection */
        opAttrDef->isCollection = 0x1;
        opAttrDef->interface.collection = hdql_gArithOpIFace;
        /* special marker denoting that key is produced by argument queries */
        opAttrDef->interface.collection.keyTypeCode = 0x0;
        /* instance get freed at query destruction */
        struct hdql_ArithCollectionArgs * arithCollectionArgs
                = hdql_alloc(ws->context, struct hdql_ArithCollectionArgs);
        arithCollectionArgs->a = a;
        arithCollectionArgs->b = b;
        arithCollectionArgs->evaluator = evaluator;

        *r = hdql_query_create(opAttrDef, ((hdql_SelectionArgs_t) arithCollectionArgs), ws->context);
    }
    assert(*r);
    return 0;
    #else
    assert(0);
    return -1;
    #endif
}

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

    #if 1
    assert(0);
    #else
    struct hdql_Func * fDef = NULL;
    int rc = hdql_func_create(funcName, argsArray, &fDef);
    if(fDef == NULL && rc == 0) {  /* special state for name lookup failure */
        free(argsArray);
        hdql_error(yyloc, ws, yyscanner, "Function \"%s\" is not defined", funcName);
        return NULL;
    }
    if( rc < 0 ) {
        // TODO: list argument types here to provide more explainatory
        // information on failure
        hdql_error(yyloc, ws, yyscanner, "Function \"%s\" can not be applied,"
            " return code %d", funcName, rc);
        free(argsArray);
        return NULL;
    }
    /* free temporary array of argument queries */
    free(argsArray);
    #endif

    assert(0);  // TODO create query around a function

    return NULL;  // TODO
}

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
    ws.errMsgSize = errBufLength;
    /* make user-provided context available for subsequent calls */
    ws.context = ctx;
    /* set data type lookup table */
    ws.compoundStack[0].compoundPtr = rootCompound;
    ws.compoundStack[0].newAttributeName = NULL;
    ws.compoundStack[0].vLastIdx = 0;
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

