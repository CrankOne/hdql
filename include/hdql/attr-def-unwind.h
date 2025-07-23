#ifndef H_HDQL_ATTR_DEF_UNWIND_H
#define H_HDQL_ATTR_DEF_UNWIND_H 1

struct hdql_AttrDef;  /* fwd */

typedef struct hdql_AttrDefStack {
    const char * name;
    const struct hdql_AttrDef * ad;
    const struct hdql_AttrDefStack * prev;
} * hdql_AttrDefStack_t;

struct hdql_iAttrDefVisitor {
    void * userdata;
    /** Should return 0 if ok, non-zero status considered as error */
    int (*atomic_ad)(const hdql_AttrDefStack_t, void * userdata);

    /** Must return 0 to unwind compound, 1 to omit, other codes considered
     * as error. Compounds ignored when set to null. */
    int (*push_compound_ad)(const hdql_AttrDefStack_t, void * userdata);
    /** Denotes end of compound attribute iterations. Can be null.
     * Will be called in case of errors. Called only if `push_compound_ad()`
     * returned 0. */
    void (*pop_compound_ad)(const hdql_AttrDefStack_t, void * userdata);
};

int hdql_attr_def_unwind(const struct hdql_AttrDef * ad, struct hdql_iAttrDefVisitor * );

#endif  /* H_HDQL_ATTR_DEF_UNWIND_H */

