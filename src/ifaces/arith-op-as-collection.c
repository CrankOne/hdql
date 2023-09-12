struct ArithCollection {
    bool isResultValid, isExhausted;
    hdql_QueryProduct * qp;
    hdql_Datum_t * cValues;
    hdql_Datum_t result;
    struct hdql_OperationEvaluator evaluator;
};

static hdql_It_t
_arith_collection_op_create( hdql_Datum_t root
                           , hdql_SelectionArgs_t selArgs_
                           , hdql_Context_t ctx
                           ) {
    struct ArithCollection * r = hdql_alloc(ctx, struct ArithCollection);
    struct hdql_ArithCollectionArgs * aca
        = hdql_cast(ctx, struct hdql_ArithCollectionArgs, selArgs_);
    r->isResultValid = false;
    r->isExhausted = false;
    struct hdql_Query * argQs[3] = {aca->a, aca->b, NULL};
    r->qp = hdql_query_product_create(argQs, &(r->cValues), ctx);
    r->result = hdql_create_value(aca->evaluator->returnType, ctx);
    r->evaluator = *(aca->evaluator);
    return reinterpret_cast<hdql_It_t>(r);
}

static hdql_Datum_t
_arith_collection_op_dereference( hdql_Datum_t root
                                , hdql_It_t it_
                                , hdql_Context_t ctx
                                ) {
    struct ArithCollection * it = hdql_cast(ctx, struct ArithCollection, it_);
    if(it->isResultValid) return it->result;
    int rc = it->evaluator.op(it->cValues[0], it->cValues[1], it->result);
    if(0 != rc) {
        hdql_context_err_push(ctx, rc, "Arithmetic operation on"
                " collection returned %d", rc);  // TODO: elaborate
    }
    it->isResultValid = true;
    return it->result;
}

static hdql_It_t
_arith_collection_op_advance( hdql_Datum_t root
                            , hdql_SelectionArgs_t selArgs
                            , hdql_It_t it_
                            , hdql_Context_t ctx
                            ) {
    struct ArithCollection * it = hdql_cast(ctx, struct ArithCollection, it_);
    it->isResultValid = false;
    if(!it->isExhausted)
        it->isExhausted = !hdql_query_product_advance(root, it->qp, ctx);
    return it_;
}

static void
_arith_collection_op_reset( hdql_Datum_t root
                          , hdql_SelectionArgs_t selArgs
                          , hdql_It_t it_
                          , hdql_Context_t ctx
                          ) {
    struct ArithCollection * it = hdql_cast(ctx, struct ArithCollection, it_);
    hdql_query_product_reset(root, it->qp, ctx);
    if(NULL == *(it->cValues)) {
        // product results in an empty set
        it->isExhausted = true;
        it->isResultValid = false;
    }
    _arith_collection_op_dereference(root, it_, ctx);
}

static void
_arith_collection_op_destroy( hdql_It_t it_
                            , hdql_Context_t ctx
                            ) {
    struct ArithCollection * it = hdql_cast(ctx, struct ArithCollection, it_);
    hdql_query_product_destroy(it->qp, ctx);
    for( struct hdql_Query ** q = hdql_query_product_get_query(it->qp)
       ; NULL != *q; ++q ) {
        hdql_query_destroy(*q, ctx);
    }
    hdql_context_free(ctx, reinterpret_cast<hdql_Datum_t>(it_));
}

static void
_arith_collection_op_get_key( hdql_Datum_t root
                            , hdql_It_t
                            , struct hdql_CollectionKey *
                            , hdql_Context_t ctx
                            ) {
    assert(0);
}

static int
_arith_collection_op_reserve_key( struct hdql_CollectionKey ** dest
        , hdql_SelectionArgs_t selArgs_
        , hdql_Context_t ctx
        ) {
    assert(dest);
    struct hdql_ArithCollectionArgs * aca
        = hdql_cast(ctx, struct hdql_ArithCollectionArgs, selArgs_);
    struct hdql_Query * argQs[3] = {aca->a, aca->b, NULL};
    struct hdql_CollectionKey ** keys = hdql_query_product_reserve_keys(argQs, ctx);
    (*dest) = hdql_alloc(ctx, struct hdql_CollectionKey);
    (*dest)->code = 0x0;
    (*dest)->pl.datum = reinterpret_cast<hdql_Datum_t>(keys);
    return 0;
}

static void
_arith_collection_op_free_selection(hdql_Context_t ctx, hdql_SelectionArgs_t) {
    assert(0);
    // ... TODO?
}

const struct hdql_CollectionAttrInterface hdql_gArithOpIFace = {
      .create = NULL  //_arith_collection_op_create,
    , .dereference = NULL  //_arith_collection_op_dereference,
    , .advance = NULL  //_arith_collection_op_advance,
    , .reset = NULL  //_arith_collection_op_reset,
    , .destroy = NULL  //_arith_collection_op_destroy,
    , .compile_selection = NULL
    , .free_selection = _arith_collection_op_free_selection
};
