#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/compound.h"
#include "hdql/helpers/print-tree.h"
#include "hdql/types.h"

#include "hdql/internal-api.h"

#include <string.h>
#include <assert.h>

void
hdql_print_value_shallow( const struct hdql_Datum * r
        , const struct hdql_AttrDef * topAttrDef
        , struct hdql_Context * ctx
        , char * strBuf, size_t bufSize
        ) {
    int nUsed;
#define _M_append_strbuf(...) { \
        nUsed = snprintf( strBuf, bufSize, __VA_ARGS__ ); \
        if(nUsed < 0 || ((size_t) nUsed) >= bufSize) return; \
        strBuf += nUsed; \
        bufSize -= nUsed; \
    }

    if(!r) {
        char buf[128];
        hdql_top_attr_str(topAttrDef, buf, sizeof(buf), ctx);
        _M_append_strbuf("NULL: %s", buf);
        return;
    }

    struct hdql_ValueTypes * valTypes = hdql_context_get_types(ctx);
    if(hdql_attr_def_is_static_const_value(topAttrDef)) {
        /* static value, for instance `2', `2 + 3', etc */
        _M_append_strbuf( "static value %p of type %d"
              ,  hdql_attr_def_get_static_value(topAttrDef)
              , (int) hdql_attr_def_get_atomic_value_type_code(topAttrDef)
              );

        if(hdql_attr_def_get_atomic_value_type_code(topAttrDef)) {
            const struct hdql_ValueInterface * vi
                = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(topAttrDef));
            if(vi && vi->get_as_string) {
                char bf[32];
                vi->get_as_string((hdql_Datum_t) r, bf, sizeof(bf), ctx);
                _M_append_strbuf( "value=\"%s\"", bf );
            } else {
                _M_append_strbuf(" (no value interface)");
            }
        }
    } else if(hdql__attr_def_is_fwd_query(topAttrDef)) {
        /* forwarding query */
        _M_append_strbuf("forwarding query %p", hdql__attr_def_fwd_query(topAttrDef));
    } else if(!hdql_attr_def_is_atomic(topAttrDef)) {  // compound instance(s) of certain type
        assert(hdql_attr_def_is_compound(topAttrDef));
        const struct hdql_Compound * ct = hdql_attr_def_compound_type_info(topAttrDef);
        if(!hdql_compound_is_virtual(ct)) {
            _M_append_strbuf( "compound %s instance %p of type `%s': "
                  , hdql_attr_def_is_collection(topAttrDef) ? "collection" : "scalar"
                  , r
                  , hdql_compound_get_name(hdql_attr_def_compound_type_info(topAttrDef))
                  );
            /*print_compound_instance_1st_tier(r, topAttrDef);*/
        } else {
            char buf[128];
            hdql_compound_get_full_type_str(ct, buf, sizeof(buf));
            const char * tp = hdql_attr_def_is_collection(topAttrDef) ? "collection" : "scalar";
            _M_append_strbuf( "compound %s instance %p of type `%s': ", tp, r, buf);
            /*print_compound_instance_1st_tier(r, topAttrDef);*/
        }
    } else {  // atomic item
        if(!hdql_attr_def_is_collection(topAttrDef)) {  // scalar compound attribute
            // xxx, example: `.eventID', `.hits.x'
            const struct hdql_ValueInterface * vi = hdql_types_get_type(valTypes
                    , hdql_attr_def_get_atomic_value_type_code(topAttrDef) );
            if(!vi) {
                _M_append_strbuf("scalar instance %p of unknown type", r);
            } else if(!vi->get_as_string) {
                _M_append_strbuf("scalar instance %p of type <%s> (no to-string method)"
                        , r, vi->name );
            } else {
                char valueBf[32];
                vi->get_as_string((hdql_Datum_t) r, valueBf, sizeof(valueBf), ctx);
                _M_append_strbuf("scalar instance %p of type <%s> with value \"%s\""
                        , r, vi->name, valueBf );
            }
        } else {
            /* item from collection of atomic scalars (1d list or array),
             * for example: `.hits.rawData.samples' */
            #if 1
            const struct hdql_ValueInterface * vi
                = hdql_types_get_type(valTypes, hdql_attr_def_get_atomic_value_type_code(topAttrDef));
            if(vi && vi->get_as_string) {
                char bf[32];
                vi->get_as_string((hdql_Datum_t) r, bf, sizeof(bf), ctx);
                _M_append_strbuf("value=\"%s\"", bf );
            } else {
                _M_append_strbuf(" (no value interface)");
            }
            #else
            // Query resulted in weird item
            printf("??? (attr.def. provided, %s, %s, %s %s)"
                  , topAttrDef->staticValueFlags ? "stat.val." : "not a static value"
                  , topAttrDef->isSubQuery ? "subquery" : "not a subquery"
                  , topAttrDef->isAtomic ? "atomic" : "compound"
                  , topAttrDef->isCollection ? "collection" : "scalar"
                  );
            #endif
        }
    }
#undef _M_append_strbuf
}

/*                            _________________________________________________
 * _________________________/ Tree printing for attribute definitions and data
 */

typedef const struct _ADDTreeLikeNode {
    const struct hdql_AttrDef * ad;
    const char * label;
    hdql_Datum_t datum;
} * _ADDTreeLikeNode_t;

static size_t
_ADD_nchildren(const hdql_TreeLikeNode_t node_, void * userdata) {
    ((void) userdata);
    _ADDTreeLikeNode_t node = (_ADDTreeLikeNode_t) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    return hdql_compound_get_nattrs(c);
}

static size_t
_ADD_nchildren_recursive(const hdql_TreeLikeNode_t node_, void * userdata) {
    ((void) userdata);
    _ADDTreeLikeNode_t node = (_ADDTreeLikeNode_t) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    return hdql_compound_get_nattrs_recursive(c);
}

static void
_ADD_resolve_datum_noeval(struct _ADDTreeLikeNode * nn
        , hdql_Datum_t owner
        , hdql_Context_t context
        ) {
    if( owner
     && hdql_attr_def_is_scalar(nn->ad)
     && (!hdql__attr_def_is_fwd_query(nn->ad))
     ) {
        /* for simple, non-forwarding scalar values we try to obtain the values */
        const struct hdql_ScalarAttrInterface * iface
            = hdql_attr_def_scalar_iface(nn->ad);
        if(iface && !iface->instantiate) {
            #if 1
            assert(false);
            #else
            nn->datum = iface->dereference(owner
                    , NULL  /* no dyn. data (no instantiate) */
                    , iface->definitionData
                    , context
                    );
            #endif
        } else {
            /* failed to get the interface, or it has a complex lifecycle */
            nn->datum = NULL;
        }
    } else {
        /* parent compound value is not set (usually must be), or it is a
         * collection -- omit getting recursive retrieval */
        nn->datum = NULL;
    }
}

static hdql_TreeLikeNode_t
_ADD_child(const hdql_TreeLikeNode_t node_, size_t nAttr, void * userdata) {
    ((void) userdata);
    _ADDTreeLikeNode_t node = (_ADDTreeLikeNode_t) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    const char ** attrNames = (const char **) alloca(sizeof(char *)*hdql_compound_get_nattrs(c));
    hdql_compound_get_attr_names(c, attrNames);
    struct _ADDTreeLikeNode * nn = (struct _ADDTreeLikeNode *) malloc(sizeof(struct _ADDTreeLikeNode));
    nn->ad = hdql_compound_get_attr(c, attrNames[nAttr]);
    nn->label = attrNames[nAttr];
    _ADD_resolve_datum_noeval(nn, node->datum, userdata);  /* TODO: consider simple eval version? */
    /* otherwise, retrieve child datum (it's a scalar and owner is not null) */
    return (hdql_TreeLikeNode_t) nn;
}

static hdql_TreeLikeNode_t
_ADD_child_recursive(const hdql_TreeLikeNode_t node_, size_t nAttr, void * userdata) {
    ((void) userdata);
    _ADDTreeLikeNode_t node = (_ADDTreeLikeNode_t) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    const char ** attrNames = (const char **) alloca(sizeof(char *)*hdql_compound_get_nattrs_recursive(c));
    hdql_compound_get_attr_names_recursive(c, attrNames);
    struct _ADDTreeLikeNode * nn = (struct _ADDTreeLikeNode *) malloc(sizeof(struct _ADDTreeLikeNode));
    nn->ad = hdql_compound_get_attr(c, attrNames[nAttr]);
    nn->label = attrNames[nAttr];
    _ADD_resolve_datum_noeval(nn, node->datum, userdata);  /* TODO: consider simple eval version? */
    return (hdql_TreeLikeNode_t) nn;
}


static int
_ADD_is_leaf(hdql_TreeLikeNode_t node_, void * userdata) {
    ((void) userdata);
    _ADDTreeLikeNode_t node = (_ADDTreeLikeNode_t) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0x1;
    return 0x0;
}

static const char *
_ADD_label(char * buf, size_t bufSize
        , const hdql_TreeLikeNode_t node_
        , void * userdata
        ) {
    _ADDTreeLikeNode_t node = (_ADDTreeLikeNode_t) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    int nUsed;
    if(node->label && strcmp(node->label, "(root)")) {
        nUsed = snprintf(buf, bufSize, "\"%s\": ", node->label);
    } else {
        nUsed = snprintf(buf, bufSize, "%s: ", node->label ? node->label : "NULL");
    }
    if(nUsed > 0 && nUsed < bufSize) {
        hdql_print_value_shallow(node->datum, ad
                , (hdql_Context_t) userdata
                , buf + nUsed, bufSize - nUsed );
    }
    return buf;
}

static void
_ADD_close_child(hdql_TreeLikeNode_t node_) {
    free((void *) node_);
}

/* Entry point to AD tree-like printing */
void
hdql_attr_def_tree_data_print(const struct hdql_AttrDef * ad
        , const struct hdql_Datum * datum
        , hdql_Context_t ctx
        , size_t lineWidth
        , FILE * dest
        , int recursive
        ) {
    struct hdql_TreeLikeNodeIFace iface = {
        .nchildren = _ADD_nchildren,
        .acquire_child = _ADD_child,
        .is_leaf = _ADD_is_leaf,
        .label = _ADD_label,
        .release_child = _ADD_close_child
    };
    if(recursive) {
        iface.nchildren = _ADD_nchildren_recursive;
        iface.acquire_child = _ADD_child_recursive;
    }
    struct _ADDTreeLikeNode addRoot = {.ad = ad, .label = "(root)"
                , .datum = (hdql_Datum_t) datum};
    hdql_print_tree_like((hdql_TreeLikeNode_t) &addRoot, ctx, iface, dest, lineWidth);
}
