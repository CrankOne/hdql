#include "hdql/helpers/print-tree.h"

#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>

struct TreeLikePrintSettings {
    struct hdql_TreeLikeNodeIFace inode;
    char * buf;
    size_t bufSize;
    FILE * outf;
    void * userdata;
};

static void
_hdql_print_tree_impl( struct TreeLikePrintSettings * S
              , const hdql_TreeLikeNode_t node
              , const char *prefix, int isLast, int isRoot) {
    size_t n_children = S->inode.nchildren(node, S->userdata);
    int isLeaf = S->inode.is_leaf(node, S->userdata);
    S->inode.label(S->buf, S->bufSize, node, S->userdata);
    if(isRoot) {
        fprintf(S->outf, "-%c %s\n", isLeaf ? '-' : '+', S->buf);
    } else {
        fprintf(S->outf, "%s%c-%c %s\n",
               prefix,
               isLast ? '`' : '|',
               isLeaf ? '-' : '+',
               S->buf);
    }
    char nextPrefix[128];
    for(size_t i = 0; i < n_children; ++i) {
        hdql_TreeLikeNode_t child = S->inode.acquire_child(node, i, S->userdata);
        int childIsLast = (i + 1 == n_children);
        if(isRoot) {
            snprintf(nextPrefix, sizeof(nextPrefix), " ");
        } else {
            snprintf(nextPrefix, sizeof(nextPrefix),
                     "%s%s",
                     prefix,
                     isLast ? "  " : "| ");
        }

        _hdql_print_tree_impl(S, child, nextPrefix, childIsLast, 0);
        if(S->inode.release_child)
            S->inode.release_child(child);
    }
}

void
hdql_print_tree_like( const hdql_TreeLikeNode_t node, void * userdata
              , struct hdql_TreeLikeNodeIFace iface
              , FILE * outFile, size_t lineWidth) {
    char * buf = (char*) malloc(lineWidth + 1);
    struct TreeLikePrintSettings S = {
        .inode = iface,
        .buf = buf,
        .bufSize = lineWidth + 1,
        .outf = outFile,
        .userdata = userdata
    };
    S.inode = iface;
    _hdql_print_tree_impl(&S, node, "", 1, 1);
    free(buf);
}

