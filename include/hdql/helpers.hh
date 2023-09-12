#pragma once

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <typeinfo>
#include <typeindex>
#include <cstring>

#include "hdql/errors.h"
#include "hdql/attr-def.h"
#include "hdql/compound.h"
#include "hdql/context.h"
#include "hdql/query-key.h"
#include "hdql/query.h"
#include "hdql/types.h"
#include "hdql/value.h"

/**\file
 * \brief Optional helpers for fast creation of compound definitions based on
 *        C++ structure definitions 
 * */

namespace hdql {
namespace helpers {

typedef std::unordered_map<std::type_index, hdql_Compound *> Compounds;

namespace detail {

template<typename T> struct ArithTypeNames;

template<> struct ArithTypeNames<float> { static constexpr const char * name = "float"; };
template<> struct ArithTypeNames<short> { static constexpr const char * name = "short"; };
template<> struct ArithTypeNames<int>   { static constexpr const char * name = "int"; };
template<> struct ArithTypeNames<size_t>{ static constexpr const char * name = "size_t"; };
template<> struct ArithTypeNames<uint32_t>{ static constexpr const char * name = "uint32_t"; };
// ...

template<typename T> struct is_shared_ptr : std::false_type {};
template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template<bool isCompound, bool isCollection> struct AttrDefCallback;

template<> struct AttrDefCallback<true, true> {
    //static constexpr auto create_attr_def = hdql_attr_def_create_compound_collection;
    static inline hdql_AttrDef * create_attr_def( hdql_Compound ** typeInfoPtr
                                      , hdql_CollectionAttrInterface * iface
                                      , hdql_ValueTypeCode_t keyCode
                                      , hdql_ReserveKeysListCallback_t keyCllb
                                      , hdql_Context_t ctx
                                      ) {
        return hdql_attr_def_create_compound_collection(*typeInfoPtr, iface, keyCode, keyCllb, ctx);
    }
};

template<> struct AttrDefCallback<true, false> {
    //static constexpr auto create_attr_def = hdql_attr_def_create_compound_scalar;
    static inline hdql_AttrDef * create_attr_def( hdql_Compound ** typeInfoPtr
                                      , hdql_ScalarAttrInterface * iface
                                      , hdql_ValueTypeCode_t keyCode
                                      , hdql_ReserveKeysListCallback_t keyCllb
                                      , hdql_Context_t ctx
                                      ) {
        return hdql_attr_def_create_compound_scalar(*typeInfoPtr, iface, keyCode, keyCllb, ctx);
    }
};

template<> struct AttrDefCallback<false, false> {
    static constexpr auto create_attr_def = hdql_attr_def_create_atomic_scalar;
};

template<> struct AttrDefCallback<false, true> {
    static constexpr auto create_attr_def = hdql_attr_def_create_atomic_collection;
};

//
// Pointer type association
template<typename T, typename EnableT=void> struct IndirectAccessTraits {
    static constexpr bool provides = false;
};

template<typename T>
struct IndirectAccessTraits<T*, void> {  // todo: const, volatile, const volatile
    static constexpr bool provides = true;
    typedef T ReferencedType;
    static inline T * get(T * ptr) { return ptr; }
};

#if 0
template<typename T>
struct IndirectAccessTraits<T
        , typename std::enable_if<!( std::is_standard_layout<T>::value
                                  && std::is_trivial<T>::value
                                  && !std::is_pointer<T>::value
                                   )>::type
        > {  // todo: const, volatile, const volatile
    static constexpr bool provides = true;
    typedef T ReferencedType;
    static inline T * get(T & obj) { return &obj; }
};
#endif

template<typename T>
struct IndirectAccessTraits<std::shared_ptr<T>, void> {  // todo: const, volatile, const volatile
    static constexpr bool provides = true;
    typedef T ReferencedType;
    static inline T * get(std::shared_ptr<T> & ptr) { return ptr ? ptr.get() : nullptr; }
};

//                                                       _____________________
// ____________________________________________________/ STL collection traits
template<typename T> struct STLContainerTraits {
    static constexpr bool isOfType = false;
};

//
// Map of instances (TODO: not tested)
template< typename KeyT
        , typename ValueT
        >
struct STLContainerTraits< std::unordered_map<KeyT, ValueT> > {
    static constexpr bool isOfType = true;
    typedef std::unordered_map<KeyT, ValueT> Type;
    typedef typename Type::iterator Iterator;
    typedef typename IndirectAccessTraits<ValueT>::ReferencedType ValueType;
    typedef KeyT Key;
    
    static ValueType * get(Iterator it) {
        return IndirectAccessTraits<ValueT>::get(it->second);
    }

    static void get_key( const Type &
                       , Iterator it
                       , hdql_CollectionKey & key
                       ) {
        const auto v = it->first;
        assert(key.pl.datum);
        memcpy(key.pl.datum, &v, sizeof(Key));
    }
};

//
// Map of pointers to instances
template< typename KeyT
        , typename ValueT
        >
struct STLContainerTraits< std::unordered_map<KeyT, std::shared_ptr<ValueT>> > {
    static constexpr bool isOfType = true;
    typedef std::unordered_map<KeyT, std::shared_ptr<ValueT>> Type;
    typedef typename Type::iterator Iterator;
    typedef ValueT ValueType;
    typedef KeyT Key;

    static ValueType * get(Iterator it) {
        return IndirectAccessTraits<std::shared_ptr<ValueT>>::get(it->second);
    }

    static void get_key( const Type &
                       , Iterator it
                       , hdql_CollectionKey & key
                       ) {
        const auto v = it->first;
        assert(key.pl.datum);
        memcpy(key.pl.datum, &v, sizeof(KeyT));
    }
};

//
// Vector of instances (TODO: not tested)
template< typename ValueT >
struct STLContainerTraits< std::vector<ValueT> > {
    static constexpr bool isOfType = true;
    typedef std::vector<ValueT> Type;
    typedef typename Type::iterator Iterator;
    typedef typename IndirectAccessTraits<ValueT>::ReferencedType ValueType;
    typedef size_t Key;
    
    static ValueType * get(Iterator it) {
        return IndirectAccessTraits<ValueT>::get(*it);
    }

    static void get_key( Type & container
                       , Iterator it
                       , hdql_CollectionKey & key
                       ) {
        auto dist_ = std::distance(container.begin(), it);
        assert(dist_ >= 0);
        size_t dist = static_cast<size_t>(dist_);
        memcpy(key.pl.datum, &dist, sizeof(size_t));
    }
};

//
// Vector of pointers
template< typename ValueT >
struct STLContainerTraits< std::vector<std::shared_ptr<ValueT>> > {
    static constexpr bool isOfType = true;
    typedef std::vector<std::shared_ptr<ValueT>> Type;
    typedef typename Type::iterator Iterator;
    typedef ValueT ValueType;
    typedef size_t Key;
    
    static ValueType * get(Iterator it) {
        return IndirectAccessTraits<std::shared_ptr<ValueT>>::get(*it);
    }

    static void get_key( Type & container
                       , Iterator it
                       , hdql_CollectionKey & key
                       ) {
        auto dist_ = std::distance(container.begin(), it);
        assert(dist_ >= 0);
        size_t dist = static_cast<size_t>(dist_);
        memcpy(key.pl.datum, &dist, sizeof(size_t));
    }
};

}  // namespace ::hdql::helpers::detail

template<auto T, typename EnableT=void> struct IFace;

template< typename T
        , typename EnableT=void
        > struct TypeInfoMixin;

template< typename T>
struct TypeInfoMixin<T, typename std::enable_if<std::is_arithmetic<T>::value>::type> {
    static constexpr bool isCompound = false;
    static hdql_AtomicTypeFeatures
    type_info(const hdql_ValueTypes * valTypes, Compounds & compounds) {
        auto r = hdql_AtomicTypeFeatures {
              .isReadOnly = 0x0
            , .arithTypeCode = hdql_types_get_type_code(valTypes, detail::ArithTypeNames<T>::name )
        };
        if(0x0 == r.arithTypeCode) {
            char bf[32], errbf[64];
            snprintf(errbf, sizeof(errbf), "Failed to obtain type code for"
                    " C/C++ type `%s'", hdql_cxx_demangle(typeid(T).name(), bf, sizeof(bf)) );
            throw std::runtime_error(errbf);
        }
        return r;
    }
};  // type info mixin for arithmetic types

template< typename T>
struct TypeInfoMixin<T, typename std::enable_if< std::is_standard_layout<T>::value
                                              && (!detail::is_shared_ptr<T>::value)
                                              && !std::is_arithmetic<T>::value>::type> {
    static constexpr bool isCompound = true;
    static hdql_Compound *
    type_info(const hdql_ValueTypes * valTypes, Compounds & compounds) {
        assert(valTypes);
        auto it = compounds.find(typeid(T));
        assert(compounds.end() != it);
        return it->second;
    }
};  // type info mixin for compounds

template< typename T>
struct TypeInfoMixin<T, typename std::enable_if<detail::IndirectAccessTraits<T>::provides>::type> {
    static constexpr bool isCompound = true;
    static hdql_Compound *
    type_info(const hdql_ValueTypes * valTypes, Compounds & compounds) {
        auto it = compounds.find(typeid(typename detail::IndirectAccessTraits<T>::ReferencedType));
        if(compounds.end() == it) {
            char errbf[256], bf[128];
            snprintf( errbf, sizeof(errbf)
                    , "No compound of C++ type `%s' had been defined yet"
                    , hdql_cxx_demangle(typeid(typename detail::IndirectAccessTraits<T>::ReferencedType).name()
                        , bf, sizeof(bf)) );
            throw std::runtime_error(errbf);
        }
        return it->second;
    }
};  // type info mixin for compounds

/// Implements arithmetic scalar attribute access interface
template<typename OwnerT, typename AttrT, AttrT OwnerT::*ptr>
struct IFace<ptr, typename std::enable_if<std::is_arithmetic<AttrT>::value, void>::type>
        : public TypeInfoMixin<AttrT> {
    static constexpr bool isCollection = false;

    static hdql_Datum_t
    dereference( hdql_Datum_t root  // owning object
               , hdql_Datum_t dynData  // allocated with `instantiate()`
               , struct hdql_CollectionKey * // may be NULL
               , const hdql_Datum_t  // may be NULL
               , hdql_Context_t
               ) {
        return reinterpret_cast<hdql_Datum_t>(&(reinterpret_cast<OwnerT *>(root)->*ptr));
    }

    static hdql_ScalarAttrInterface iface() {
        return hdql_ScalarAttrInterface{
                  .definitionData = NULL
                , .instantiate = NULL
                , .dereference = dereference
                , .reset = NULL
                , .destroy = NULL
            };
    }

    static constexpr auto create_attr_def = detail::AttrDefCallback< false, false >::create_attr_def;
};  // scalar atomic attribute

/// Implements scalar compound access interface
template<typename OwnerT, typename AttrT, AttrT OwnerT::*ptr>
struct IFace<ptr, typename std::enable_if<detail::IndirectAccessTraits<AttrT>::provides, void>::type>
        : public TypeInfoMixin<AttrT> {
    static constexpr bool isCollection = false;

    static hdql_Datum_t
    dereference( hdql_Datum_t root  // owning object
               , hdql_Datum_t dynData  // allocated with `instantiate()`
               , struct hdql_CollectionKey * // may be NULL
               , const hdql_Datum_t  // may be NULL
               , hdql_Context_t
               ) {
        auto p = detail::IndirectAccessTraits<AttrT>::get(reinterpret_cast<OwnerT *>(root)->*ptr);
        return reinterpret_cast<hdql_Datum_t>(p);
    }

    static hdql_ScalarAttrInterface iface() {
        return hdql_ScalarAttrInterface{
                  .definitionData = NULL
                , .instantiate = NULL
                , .dereference = dereference
                , .reset = NULL
                , .destroy = NULL
            };
    }

    static constexpr auto create_attr_def = detail::AttrDefCallback<true, false>::create_attr_def;
};  // scalar compound attribute

//
// Implements arithmetic 1D array attribute access interface
//
// Simple C/C++ 1D array of instances
template<typename OwnerT, typename AttrT, AttrT OwnerT::*ptr>
struct IFace< ptr, typename std::enable_if<
              std::is_array<AttrT>::value && std::is_arithmetic<typename std::remove_extent<AttrT>::type>::value
            , void>::type>
        : public TypeInfoMixin<typename std::remove_extent<AttrT>::type> {
    static constexpr bool isCollection = true;

    typedef typename std::remove_extent<AttrT>::type ElementType;
    typedef size_t Key;
    struct Iterator {
        OwnerT * owner;
        size_t cIndex;
    };

    static hdql_It_t
    create( hdql_Datum_t owner
          , const hdql_Datum_t defData
          , hdql_SelectionArgs_t
          , hdql_Context_t context
          ) {
        Iterator * it = reinterpret_cast<Iterator *>(hdql_context_alloc(context, sizeof(Iterator)));
        it->owner = reinterpret_cast<OwnerT*>(owner);
        return reinterpret_cast<hdql_It_t>(it);
    }

    static hdql_Datum_t
    dereference( hdql_It_t it_
               , struct hdql_CollectionKey * key
               ) {
        Iterator * it = reinterpret_cast<Iterator*>(it_);
        if(it->cIndex == std::extent<AttrT>::value) return NULL;
        if(key) {
            assert(key->pl.datum);
            *reinterpret_cast<size_t *>(key->pl.datum) = it->cIndex;
        }
        return reinterpret_cast<hdql_Datum_t>((it->owner->*ptr) + it->cIndex);
    }

    static hdql_It_t
    advance( hdql_It_t it_ ) {
        Iterator * it = reinterpret_cast<Iterator*>(it_);
        if(it->cIndex == std::extent<AttrT>::value) return it_;
        ++(it->cIndex);
        return it_;
    }

    static hdql_It_t
    reset( hdql_It_t it_
         , hdql_Datum_t newOwner
         , const hdql_Datum_t defData
         , hdql_SelectionArgs_t
         , hdql_Context_t ) {
        Iterator * it = reinterpret_cast<Iterator*>(it_);
        it->owner = reinterpret_cast<OwnerT *>(newOwner);
        it->cIndex = 0;
        return it_;
    }

    static void
    destroy( hdql_It_t it_
           , hdql_Context_t context ) {
        hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(it_));
    }

    static hdql_SelectionArgs_t
    compile_selection( const char * expt
                     , const hdql_Datum_t definitionData
                     , hdql_Context_t
                     ) {
        throw std::runtime_error("TODO");  // TODO
    }

    static void
    free_selection( const hdql_Datum_t definitionData
                  , hdql_SelectionArgs_t sArgs
                  , hdql_Context_t context ) {
        throw std::runtime_error("TODO");
    }

    static hdql_CollectionAttrInterface iface() {
        return hdql_CollectionAttrInterface{
                  .definitionData = NULL
                , .create = create
                , .dereference = dereference
                , .advance = advance
                , .reset = reset
                , .destroy = destroy
                , .compile_selection = compile_selection
                , .free_selection = free_selection
            };
    }

    static constexpr auto create_attr_def
        = detail::AttrDefCallback< TypeInfoMixin<typename std::remove_extent<AttrT>::type>::isCompound
                         , true>::create_attr_def;
};  // scalar atomic attribute

//
// STL collection
//
// Interfaces for collections based on STL containers ([un]ordered maps,
// vectors, etc) are instantiated automatically using this specialization.
// Collection must be one of the types foreseen by `STLContainerTraits<>`
// template as it defines element access.
template<typename OwnerT, typename AttrT, AttrT OwnerT::*ptr>
struct IFace<ptr, typename std::enable_if<
              detail::STLContainerTraits<AttrT>::isOfType
            , void>::type>
        : public TypeInfoMixin<typename detail::STLContainerTraits<AttrT>::ValueType> {
    static constexpr bool isCollection = true;
    typedef typename detail::STLContainerTraits<AttrT>::Key Key;

    struct Iterator {
        OwnerT * owner;
        hdql_SelectionArgs_t selection;  // TODO: must be of templated type
        typename detail::STLContainerTraits<AttrT>::Iterator it;
    };

    static hdql_It_t
    create( hdql_Datum_t owner
          , const hdql_Datum_t defData
          , hdql_SelectionArgs_t selection
          , hdql_Context_t context
          ) {
        Iterator * it = reinterpret_cast<Iterator *>(hdql_context_alloc(context, sizeof(Iterator)));
        it->owner = reinterpret_cast<OwnerT*>(owner);

        //it->selection = reinterpret_cast<SelectionArgs *>(selection);  // TODO
        it->selection = selection;

        return reinterpret_cast<hdql_It_t>(it);
    }

    static hdql_Datum_t
    dereference( hdql_It_t it_
               , struct hdql_CollectionKey * key
               ) {
        Iterator * it = reinterpret_cast<Iterator*>(it_);
        if((it->owner->*ptr).end() == it->it) return NULL;
        if(key) {
            assert(0x0 != key->code);
            detail::STLContainerTraits<AttrT>::get_key(*it->owner.*ptr, it->it, *key);
        }
        return reinterpret_cast<hdql_Datum_t>(
                detail::STLContainerTraits<AttrT>::get(it->it));
    }

    static hdql_It_t
    advance( hdql_It_t it_ ) {
        Iterator * it = reinterpret_cast<Iterator*>(it_);
        if(it->it == (it->owner->*ptr).end()) return it_;
        ++(it->it);
        return it_;
    }

    static hdql_It_t
    reset( hdql_It_t it_
         , hdql_Datum_t newOwner
         , const hdql_Datum_t defData
         , hdql_SelectionArgs_t
         , hdql_Context_t ) {
        Iterator * it = reinterpret_cast<Iterator*>(it_);
        it->owner = reinterpret_cast<OwnerT *>(newOwner);
        it->it = (it->owner->*ptr).begin();
        return it_;
    }

    static void
    destroy( hdql_It_t it_
           , hdql_Context_t context ) {
        hdql_context_free(context, reinterpret_cast<hdql_Datum_t>(it_));
    }

    static hdql_SelectionArgs_t
    compile_selection( const char * expt
                     , const hdql_Datum_t definitionData
                     , hdql_Context_t
                     ) {
        throw std::runtime_error("TODO");  // TODO
    }

    static void
    free_selection( const hdql_Datum_t definitionData
                  , hdql_SelectionArgs_t sArgs
                  , hdql_Context_t context ) {
        throw std::runtime_error("TODO");
    }

    static hdql_CollectionAttrInterface iface() {
        return hdql_CollectionAttrInterface{
                  .definitionData = NULL
                , .create = create
                , .dereference = dereference
                , .advance = advance
                , .reset = reset
                , .destroy = destroy
                , .compile_selection = compile_selection
                , .free_selection = free_selection
            };
    }

    static constexpr auto create_attr_def
        = detail::AttrDefCallback< TypeInfoMixin<typename detail::STLContainerTraits<AttrT>::ValueType>::isCompound
                         , true>::create_attr_def;
};  // STL container collection

// ...
class CompoundTypes : public Compounds {
    template<typename CompoundT>
    class AttributeInsertionProxy {
    private:
        hdql_Compound & _compound;
        CompoundTypes & _compounds;
        hdql_Context & _context;
        //hdql_ValueTypes & _valTypes;
    protected:
        AttributeInsertionProxy( hdql_Compound & compound_
                               , CompoundTypes & compounds_
                               , hdql_Context & context
                               //, hdql_ValueTypes & valTypes_
                               )
            : _compound(compound_)
            , _compounds(compounds_)
            , _context(context)
            {}
    public:
        template<auto ptr>
        typename std::enable_if<helpers::IFace<ptr>::isCollection, AttributeInsertionProxy<CompoundT>>::type &
        attr(const char * name) {
            hdql_ValueTypes * vts = hdql_context_get_types(&_context);
            assert(vts);
            auto typeInfo = helpers::IFace<ptr>::type_info(vts, _compounds);
            auto iface = helpers::IFace<ptr>::iface();
            hdql_ValueTypeCode_t keyTypeCode
                = hdql_types_get_type_code(vts
                        , detail::ArithTypeNames<typename helpers::IFace<ptr>::Key>::name );
            if(0x0 == keyTypeCode) {
                throw std::runtime_error("Unknown key type.");  // TODO: elaborate
            }
            hdql_AttrDef * ad = helpers::IFace<ptr>::create_attr_def(
                          &typeInfo
                        , &iface
                        , keyTypeCode
                        , nullptr
                        , &_context
                    );
            int rc = hdql_compound_add_attr( &_compound
                                  , name
                                  , ad
                    );
            if(HDQL_ERR_CODE_OK == rc) return *this;
            char errbf[128];
            snprintf(errbf, sizeof(errbf), "Failed to add attribute \"%s\": %d"
                    , name, rc);
            throw std::runtime_error(errbf);
        }

        template<auto ptr>
        typename std::enable_if<!helpers::IFace<ptr>::isCollection, AttributeInsertionProxy<CompoundT>>::type &
        attr(const char * name) {
            hdql_ValueTypes * vts = hdql_context_get_types(&_context);
            assert(vts);
            auto typeInfo = helpers::IFace<ptr>::type_info(vts, _compounds);
            auto iface = helpers::IFace<ptr>::iface();
            hdql_AttrDef * ad = helpers::IFace<ptr>::create_attr_def(
                          &typeInfo
                        , &iface
                        , 0x0
                        , nullptr
                        , &_context
                    );
            int rc = hdql_compound_add_attr( &_compound
                                  , name
                                  , ad
                    );
            if(HDQL_ERR_CODE_OK == rc) return *this;
            char errbf[128];
            snprintf(errbf, sizeof(errbf), "Failed to add attribute \"%s\": %d"
                    , name, rc);
            throw std::runtime_error(errbf);
        }

        template<auto ptr>
        typename std::enable_if<helpers::IFace<ptr>::isCollection, AttributeInsertionProxy<CompoundT>>::type &
        attr(const char * name, hdql_ReserveKeysListCallback_t keyListReserveCallback) {
            hdql_ValueTypes * vts = hdql_context_get_types(&_context);
            assert(vts);
            auto typeInfo = helpers::IFace<ptr>::type_info(vts, _compounds);
            auto iface = helpers::IFace<ptr>::iface();
            hdql_AttrDef * ad = helpers::IFace<ptr>::create_attr_def(
                          &typeInfo
                        , &iface
                        , 0x0
                        , keyListReserveCallback
                        , &_context
                    );
            int rc = hdql_compound_add_attr( &_compound
                                  , name
                                  , ad
                    );
            if(HDQL_ERR_CODE_OK == rc) return *this;
            char errbf[128];
            snprintf(errbf, sizeof(errbf), "Failed to add attribute \"%s\": %d", rc);
            throw std::runtime_error(errbf);
        }

        CompoundTypes & end_compound() {
            return _compounds;
        }

        friend class CompoundTypes;
    };
private:
    hdql_Context_t _contextPtr;
public:
    CompoundTypes(hdql_Context_t context) : _contextPtr(context) {}

    template<typename T> AttributeInsertionProxy<T>
    new_compound(const char * name) {
        hdql_Compound * newCompound = hdql_compound_new(name, _contextPtr);
        assert(newCompound);
        this->emplace(typeid(T), newCompound);
        return AttributeInsertionProxy<T>(*newCompound, *this, *_contextPtr);
    }
};

}  // ::hdql::helpers
}  // namespace hdql

