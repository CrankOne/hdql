static hdql_Datum_t
_dereference_`'TYPE`'_`'ATTR_NAME`'( hdql_Datum_t owner_
    , hdql_Datum_t /*dynData*/
    , struct hdql_CollectionKey * /*keyPtr*/
    , const hdql_Datum_t /*defData*/
    , hdql_Context_t /*context*/
    ) {
    NSTYPE * owner = reinterpret_cast<NSTYPE*>(owner_);
    if(!owner->ATTR_NAME) return NULL;
    return reinterpret_cast<hdql_Datum_t>(
        owner->ATTR_NAME.get());
}

static const struct hdql_ScalarAttrInterface _`'TYPE`'_`'ATTR_NAME`'_iface = {
    .definitionData = NULL,
    .instantiate = NULL,
    .dereference = _dereference_`'TYPE`'_`'ATTR_NAME,
    .reset = NULL,
    .destroy = NULL,
};

