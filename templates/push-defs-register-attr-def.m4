pushdef(beginStruct, `    { /*$1*/define(TYPE, $1)
        struct hdql_Compound * c = hdql_compound_new("TYPE");')dnl
pushdef(scalarAtomic, `
            struct hdql_AtomicTypeFeatures $1_typeFeatures = {0x0, hdql_types_get_type_code(valTypes, "$2")};
            struct hdql_ScalarAttrInterface _`'TYPE`'_$1_iface = {
                .definitionData = NULL,
                .instantiate = NULL,
                .dereference = _dereference_`'TYPE`'_$1,
                .reset = NULL,
                .destroy = NULL,
            };
            struct hdql_AttrDef * ad_`'$1 = hdql_attr_def_create_atomic_scalar(
                  /* type info */  & $1_typeFeatures
                , /* attr iface */ & _`'TYPE`'_$1_iface
                , /* key type code */ 0x0
                , /* key iface */ NULL
                , context
        ); ')
pushdef(arrayAtomic, `
            struct hdql_AtomicTypeFeatures $1_typeFeatures = {0x0, hdql_types_get_type_code(valTypes, "$2")};
            struct hdql_CollectionAttrInterface _`'TYPE`'_$1_iface = {
                .create = _`'TYPE`'_$1_create,
                .dereference = _`'TYPE`'_$1_dereference,
                .advance = _`'TYPE`'_$1_advance,
                .reset = _`'TYPE`'_$1_reset,
                .destroy = _`'TYPE`'_$1_destroy,
                .compile_selection = hdql::test::_compile_simple_selection,
                .free_selection = hdql::test::_free_simple_selection
            };
            struct hdql_AttrDef * ad_`'$1 = hdql_attr_def_create_atomic_collection(
                  /* type info */  & $1_typeFeatures
                , /* attr iface */ & _`'TYPE`'_$1_iface
                , /* key type code */ size_t_TypeCode
                , /* key iface */ NULL
                , context
        ); ')
pushdef(mapCompound, `
            struct hdql_Compound * compound = hdql_compound_new("$2");
            struct hdql_CollectionAttrInterface _`'TYPE`'_$1_iface = {
                .create = _`'TYPE`'_$1_create,
                .dereference = _`'TYPE`'_$1_dereference,
                .advance = _`'TYPE`'_$1_advance,
                .reset = _`'TYPE`'_$1_reset,
                .destroy = _`'TYPE`'_$1_destroy,
                .compile_selection = hdql::test::_compile_simple_selection,
                .free_selection = hdql::test::_free_simple_selection
            };
            struct hdql_AttrDef * ad_`'$1 = hdql_attr_def_create_compound_collection(
                  /* type info (compound) */  compound
                , /* attr iface */ & _`'TYPE`'_$1_iface
                , /* key type code */ hdql_types_get_type_code(valTypes, "$3");
                , /* key iface */ NULL  // TODO!
                , context
        ); ')
pushdef(endStruct, `    } /*$1*/')dnl
