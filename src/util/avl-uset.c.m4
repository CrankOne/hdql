define(_M_AVL, hdql_uset)dnl
define(_M_AVLNode, AVLNode_uset)dnl
dnl
dnl define(_M_isMap, false)dnl
dnl
define(_M_keyDynAlloc, `none')
dnl
define(_M_keyType, `unsigned long')dnl
define(_M_key_cmp, `($1 < $2 ? -1 : ($1 > $2 ? 1 : 0))')
dnl
include(src/util/avl.m4)

