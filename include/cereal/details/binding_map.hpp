#pragma once

#include "../macros.hpp"
#include "helpers.hpp"
#include "polymorphic_impl_fwd.hpp"
#include "static_object.hpp"
#include "util.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <typeindex>

namespace cereal
{
namespace detail
{
//! Base type for polymorphic void casting
/*! Contains functions for casting between registered base and derived types.

    This is necessary so that cereal can properly cast between polymorphic types
    even though void pointers are used, which normally have no type information.
    Runtime type information is used instead to index a compile-time made
   mapping that can perform the proper cast. In the case of multiple levels of
   inheritance, cereal will attempt to find the shortest path by using
   registered relationships to perform the cast.

    This class will be allocated as a StaticObject and only referenced by
   pointer, allowing a templated derived version of it to define strongly typed
   functions that cast between registered base and derived types. */
struct PolymorphicCaster {
    PolymorphicCaster() = default;
    PolymorphicCaster(const PolymorphicCaster &) = default;
    PolymorphicCaster &operator=(const PolymorphicCaster &) = default;
    PolymorphicCaster(PolymorphicCaster &&) CEREAL_NOEXCEPT {}
    PolymorphicCaster &operator=(PolymorphicCaster &&) CEREAL_NOEXCEPT
    {
        return *this;
    }
    virtual ~PolymorphicCaster() CEREAL_NOEXCEPT = default;

    //! Downcasts to the proper derived type
    virtual void const *downcast(void const *const ptr) const = 0;
    //! Upcast to proper base type
    virtual void *upcast(void *const ptr) const = 0;
    //! Upcast to proper base type, shared_ptr version
    virtual std::shared_ptr<void>
    upcast(std::shared_ptr<void> const &ptr) const = 0;
};

//! Holds registered mappings between base and derived types for casting
/*! This will be allocated as a StaticObject and holds a map containing
    all registered mappings between base and derived types. */
struct PolymorphicCasters {
    //! Maps from a derived type index to a set of chainable casters
    using DerivedCasterMap =
        std::unordered_map<std::type_index,
                           std::vector<PolymorphicCaster const *>>;
    //! Maps from base type index to a map from derived type index to caster
    std::unordered_map<std::type_index, DerivedCasterMap> map;

    std::multimap<std::type_index, std::type_index> reverseMap;

//! Error message used for unregistered polymorphic casts
#define UNREGISTERED_POLYMORPHIC_CAST_EXCEPTION(LoadSave)                      \
    throw cereal::Exception(                                                   \
        "Trying to " #LoadSave " a registered polymorphic type with an "       \
        "unregistered polymorphic cast.\n"                                     \
        "Could not find a path to a base class (" +                            \
        util::demangle(baseInfo.name()) +                                      \
        ") for type: " + ::cereal::util::demangledName<Derived>() +            \
        "\n"                                                                   \
        "Make sure you either serialize the base class at some point via "     \
        "cereal::base_class or cereal::virtual_base_class.\n"                  \
        "Alternatively, manually register the association with "               \
        "CEREAL_REGISTER_POLYMORPHIC_RELATION.");

    //! Checks if the mapping object that can perform the upcast or downcast
    //! exists, and returns it if so
    /*! Uses the type index from the base and derived class to find the matching
        registered caster. If no matching caster exists, the bool in the pair
       will be false and the vector reference should not be used. */
    static std::pair<bool, std::vector<PolymorphicCaster const *> const &>
    lookup_if_exists(std::type_index const &baseIndex,
                     std::type_index const &derivedIndex)
    {
        // First phase of lookup - match base type index
        auto const &baseMap =
            StaticObject<PolymorphicCasters>::getInstance().map;
        auto baseIter = baseMap.find(baseIndex);
        if (baseIter == baseMap.end()) {
            return {false, {}};
        }

        // Second phase - find a match from base to derived
        auto const &derivedMap = baseIter->second;
        auto derivedIter = derivedMap.find(derivedIndex);
        if (derivedIter == derivedMap.end()) {
            return {false, {}};
        }

        return {true, derivedIter->second};
    }

    //! Gets the mapping object that can perform the upcast or downcast
    /*! Uses the type index from the base and derived class to find the matching
        registered caster. If no matching caster exists, calls the exception
       function.

        The returned PolymorphicCaster is capable of upcasting or downcasting
       between the two types. */
    template <class F>
    inline static std::vector<PolymorphicCaster const *> const &
    lookup(std::type_index const &baseIndex,
           std::type_index const &derivedIndex, F &&exceptionFunc)
    {
        // First phase of lookup - match base type index
        auto const &baseMap =
            StaticObject<PolymorphicCasters>::getInstance().map;
        auto baseIter = baseMap.find(baseIndex);
        if (baseIter == baseMap.end()) {
            exceptionFunc();
        }

        // Second phase - find a match from base to derived
        auto const &derivedMap = baseIter->second;
        auto derivedIter = derivedMap.find(derivedIndex);
        if (derivedIter == derivedMap.end()) {
            exceptionFunc();
        }

        return derivedIter->second;
    }

    //! Performs a downcast to the derived type using a registered mapping
    template <class Derived>
    inline static const Derived *downcast(const void *dptr,
                                          std::type_info const &baseInfo)
    {
        auto const &mapping = lookup(baseInfo, typeid(Derived), [&]() {
            UNREGISTERED_POLYMORPHIC_CAST_EXCEPTION(save)
        });

        for (auto const *dmap : mapping) {
            dptr = dmap->downcast(dptr);
        }

        return static_cast<Derived const *>(dptr);
    }

    //! Performs an upcast to the registered base type using the given a derived
    //! type
    /*! The return is untyped because the final casting to the base type must
       happen in the polymorphic serialization function, where the type is known
       at compile time */
    template <class Derived>
    inline static void *upcast(Derived *const dptr,
                               std::type_info const &baseInfo)
    {
        auto const &mapping = lookup(baseInfo, typeid(Derived), [&]() {
            UNREGISTERED_POLYMORPHIC_CAST_EXCEPTION(load)
        });

        void *uptr = dptr;
        for (auto mIter = mapping.rbegin(), mEnd = mapping.rend();
             mIter != mEnd; ++mIter) {
            uptr = (*mIter)->upcast(uptr);
        }

        return uptr;
    }

    //! Upcasts for shared pointers
    template <class Derived>
    inline static std::shared_ptr<void>
    upcast(std::shared_ptr<Derived> const &dptr, std::type_info const &baseInfo)
    {
        auto const &mapping = lookup(baseInfo, typeid(Derived), [&]() {
            UNREGISTERED_POLYMORPHIC_CAST_EXCEPTION(load)
        });

        std::shared_ptr<void> uptr = dptr;
        for (auto mIter = mapping.rbegin(), mEnd = mapping.rend();
             mIter != mEnd; ++mIter) {
            uptr = (*mIter)->upcast(uptr);
        }

        return uptr;
    }

#undef UNREGISTERED_POLYMORPHIC_CAST_EXCEPTION
};

//! An empty noop deleter
template <class T>
struct EmptyDeleter {
    void operator()(T *) const {}
};

//! A structure holding a map from type_indices to output serializer functions
/*! A static object of this map should be created for each registered archive
    type, containing entries for every registered type that describe how to
    properly cast the type to its real type in polymorphic scenarios for
    shared_ptr, weak_ptr, and unique_ptr. */
template <class Archive>
struct OutputBindingMap {
    //! A serializer function
    /*! Serializer functions return nothing and take an archive as
        their first parameter (will be cast properly inside the function,
        a pointer to actual data (contents of smart_ptr's get() function)
        as their second parameter, and the type info of the owning smart_ptr
        as their final parameter */
    typedef std::function<void(void *, void const *, std::type_info const &)>
        Serializer;

    //! Struct containing the serializer functions for all pointer types
    struct Serializers {
        Serializer shared_ptr, //!< Serializer function for shared/weak pointers
            unique_ptr;        //!< Serializer function for unique pointers
    };

    //! A map of serializers for pointers of all registered types
    std::map<std::type_index, Serializers> map;
};

//! A structure holding a map from type name strings to input serializer
//! functions
/*! A static object of this map should be created for each registered archive
    type, containing entries for every registered type that describe how to
    properly cast the type to its real type in polymorphic scenarios for
    shared_ptr, weak_ptr, and unique_ptr. */
template <class Archive>
struct InputBindingMap {
    //! Shared ptr serializer function
    /*! Serializer functions return nothing and take an archive as
        their first parameter (will be cast properly inside the function,
        a shared_ptr (or unique_ptr for the unique case) of any base
        type, and the type id of said base type as the third parameter.
        Internally it will properly be loaded and cast to the correct type. */
    typedef std::function<void(void *, std::shared_ptr<void> &,
                               std::type_info const &)>
        SharedSerializer;
    //! Unique ptr serializer function
    typedef std::function<void(void *,
                               std::unique_ptr<void, EmptyDeleter<void>> &,
                               std::type_info const &)>
        UniqueSerializer;

    //! Struct containing the serializer functions for all pointer types
    struct Serializers {
        SharedSerializer
            shared_ptr; //!< Serializer function for shared/weak pointers
        UniqueSerializer
            unique_ptr; //!< Serializer function for unique pointers
    };

    //! A map of serializers for pointers of all registered types
    std::map<std::string, Serializers> map;
};
} // namespace detail

} // namespace cereal
