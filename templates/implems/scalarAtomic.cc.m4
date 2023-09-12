static hdql_Datum_t
_dereference_`'TYPE`'_`'ATTR_NAME`'( hdql_Datum_t owner
    , hdql_Datum_t /*dynData*/
    , struct hdql_CollectionKey * /*keyPtr*/
    , const hdql_Datum_t /*defData*/
    , hdql_Context_t /*context*/
    ) {
    return reinterpret_cast<hdql_Datum_t>(
        &(reinterpret_cast<NSTYPE*>(owner)->ATTR_NAME));
}

