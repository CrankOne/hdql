#ifndef H_HDQL_UNION_H
#define H_HDQL_UNION_H 1

struct hdql_Context;  /* fwd */

/**\brief Union type definition
 *
 * Built from individual attributes definitions forming a list of options this
 * union can have. Access interface can be of scalar or collection type.
 * */
struct hdql_Union;

struct hdql_Union * hdql_union_new(const char * name, struct hdql_Context * ctx);

#endif
