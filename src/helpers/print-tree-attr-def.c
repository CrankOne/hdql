#include "hdql/attr-def.h"
#include "hdql/context.h"
#include "hdql/compound.h"
#include "hdql/helpers/print-tree.h"

#include <string.h>

/*                                    ________________________________________
 * _________________________________/ Tree printing for attribute definitions
 */

typedef struct _ADTreeLikeNode {
    const struct hdql_AttrDef * ad;
    const char * label;
} _ADTreeLikeNode_t;

static size_t
_AD_nchildren(const hdql_TreeLikeNode_t node_, void * userdata) {
    ((void) userdata);
    const _ADTreeLikeNode_t * node = (const _ADTreeLikeNode_t *) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    return hdql_compound_get_nattrs(c);
}

static size_t
_AD_nchildren_recursive(const hdql_TreeLikeNode_t node_, void * userdata) {
    ((void) userdata);
    const _ADTreeLikeNode_t * node = (const _ADTreeLikeNode_t *) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    return hdql_compound_get_nattrs_recursive(c);
}

static hdql_TreeLikeNode_t
_AD_child(const hdql_TreeLikeNode_t node_, size_t nAttr, void * userdata) {
    ((void) userdata);
    const _ADTreeLikeNode_t * node = (const _ADTreeLikeNode_t *) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    const char ** attrNames = (const char **) alloca(sizeof(char *)*hdql_compound_get_nattrs(c));
    hdql_compound_get_attr_names(c, attrNames);
    struct _ADTreeLikeNode * nn = (struct _ADTreeLikeNode *) malloc(sizeof(struct _ADTreeLikeNode));
    nn->ad = hdql_compound_get_attr(c, attrNames[nAttr]);
    nn->label = attrNames[nAttr];
    return (hdql_TreeLikeNode_t) nn;
}

static hdql_TreeLikeNode_t
_AD_child_recursive(const hdql_TreeLikeNode_t node_, size_t nAttr, void * userdata) {
    ((void) userdata);
    const _ADTreeLikeNode_t * node = (const _ADTreeLikeNode_t *) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0;
    const struct hdql_Compound * c = hdql_attr_def_compound_type_info(ad);
    const char ** attrNames = (const char **) alloca(sizeof(char *)*hdql_compound_get_nattrs_recursive(c));
    hdql_compound_get_attr_names_recursive(c, attrNames);
    struct _ADTreeLikeNode * nn = (struct _ADTreeLikeNode *) malloc(sizeof(struct _ADTreeLikeNode));
    nn->ad = hdql_compound_get_attr(c, attrNames[nAttr]);
    nn->label = attrNames[nAttr];
    return (hdql_TreeLikeNode_t) nn;
}


static int
_AD_is_leaf(const hdql_TreeLikeNode_t node_, void * userdata) {
    ((void) userdata);
    const _ADTreeLikeNode_t * node = (const _ADTreeLikeNode_t *) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    if(hdql_attr_def_is_atomic(ad)) return 0x1;
    return 0x0;
}

static const char *
_AD_label(char * buf, size_t bufSize
        , const hdql_TreeLikeNode_t node_
        , void * userdata
        ) {
    hdql_Context_t ctx = (hdql_Context_t) userdata;
    const _ADTreeLikeNode_t * node = (const _ADTreeLikeNode_t *) node_;
    const struct hdql_AttrDef * ad = hdql_attr_def_top_attr(node->ad);
    size_t nUsed;
    if(node->label && strcmp(node->label, "(root)")) {
        nUsed = snprintf(buf, bufSize, "\"%s\": ", node->label);
    } else {
        nUsed = snprintf(buf, bufSize, "%s: ", node->label ? node->label : "NULL");
    }
    if(nUsed < bufSize) {
        hdql_top_attr_str(ad, buf + nUsed, bufSize - nUsed, ctx);
    }
    return buf;
}

static void
_AD_close_child(hdql_TreeLikeNode_t node_) {
    free((void *) node_);
}

/* Entry point to AD tree-like printing */
void hdql_attr_def_tree_print(const struct hdql_AttrDef * ad
        , hdql_Context_t ctx
        , size_t lineWidth
        , FILE * dest
        , int recursive
        ) {
    struct hdql_TreeLikeNodeIFace iface = {
        .nchildren = _AD_nchildren,
        .acquire_child = _AD_child,
        .is_leaf = _AD_is_leaf,
        .label = _AD_label,
        .release_child = _AD_close_child
    };
    if(recursive) {
        iface.nchildren = _AD_nchildren_recursive;
        iface.acquire_child = _AD_child_recursive;
    }
    struct _ADTreeLikeNode adRoot = {.ad = ad, .label = "(root)"};
    hdql_print_tree_like((hdql_TreeLikeNode_t) &adRoot, ctx, iface, dest, lineWidth);
}
