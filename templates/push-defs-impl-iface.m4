pushdef(beginStruct, `/*
 * $1 implem
 */
pushdef(TYPE, $1)dnl
pushdef(NSTYPE, `hdql::test::TYPE')dnl
')dnl
dnl                         * * *   * * *   * * *
pushdef(scalarAtomic, `
pushdef(ATTR_NAME, $1)dnl
pushdef(ATTR_TYPE, $2)dnl
include(`templates/implems/scalarAtomic.cc.m4')
popdef(`ATTR_TYPE')dnl
popdef(`ATTR_NAME')dnl
')
dnl                         * * *   * * *   * * *
pushdef(arrayAtomic, `
pushdef(ATTR_NAME, $1)dnl
pushdef(ATTR_TYPE, $2)dnl
pushdef(ARRAY_SIZE, $3)dnl
include(`templates/implems/arrayAtomic.cc.m4')
popdef(`ARRAY_SIZE')dnl
popdef(`ATTR_TYPE')dnl
popdef(`ATTR_NAME')dnl
')dnl arrayAtomic
dnl                         * * *   * * *   * * *
pushdef(scalarCompound,  `
pushdef(ATTR_NAME, $1)dnl
pushdef(ATTR_TYPE, $2)dnl
include(`templates/implems/scalarCompound.cc.m4')
popdef(`ATTR_TYPE')dnl
popdef(`ATTR_NAME')dnl
')dnl scalarCompound
dnl                         * * *   * * *   * * *
define(mapCompound, `
pushdef(ATTR_NAME, $1)dnl
pushdef(ATTR_TYPE, $2)dnl
pushdef(KEY_TYPE, $3)dnl
include(`templates/implems/mapCompound.cc.m4')
popdef(`KEY_TYPE')dnl
popdef(`ATTR_TYPE')dnl
popdef(`ATTR_NAME')dnl
')dnl mapCompound
dnl                         * * *   * * *   * * *
pushdef(ATTR_NAME, $1)dnl
pushdef(ATTR_TYPE, $2)dnl
pushdef(arrayCompound, `')dnl
popdef(`ATTR_TYPE')dnl
popdef(`ATTR_NAME')dnl
dnl                         * * *   * * *   * * *
pushdef(endStruct, `
popdef(`TYPE')dnl
popdef(`NSTYPE')dnl
/* (end of TYPE iface) */ popdef(`TYPE')')dnl
