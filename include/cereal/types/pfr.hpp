/*! \file pfr.hpp
    \brief Automatic serialization for aggregate types via Boost.PFR
    \ingroup PFR */
/*
  Copyright (c) 2024, Zen Shawn
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the copyright holder nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef CEREAL_TYPES_PFR_HPP_
#define CEREAL_TYPES_PFR_HPP_

#ifdef CEREAL_USE_BOOST_PFR

#include "cereal/cereal.hpp"

#include <boost/pfr.hpp>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace cereal
{
namespace pfr_detail
{

//! All five non-member cereal dispatch names, with and without a version
//! argument.  Each is detected via ADL from this dedicated namespace so that
//! cereal's own PFR `serialize` overload (defined in namespace cereal) is
//! invisible to the lookup.  This is necessary to avoid both:
//!   1. PFR silently shadowing a user-provided free function
//!   2. Infinite recursion when PFR's SFINAE itself consults a trait that
//!      also looks up the same name (cereal's traits::has_non_member_* look
//!      up the same names from cereal::detail, which does see cereal::*).

template <class Archive, class T, class = void>
struct has_non_member_serialize : std::false_type {};
template <class Archive, class T>
struct has_non_member_serialize<Archive, T,
    std::void_t<decltype(serialize(std::declval<Archive&>(), std::declval<T&>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_versioned_serialize : std::false_type {};
template <class Archive, class T>
struct has_non_member_versioned_serialize<Archive, T,
    std::void_t<decltype(serialize(std::declval<Archive&>(), std::declval<T&>(), std::declval<std::uint32_t>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_save : std::false_type {};
template <class Archive, class T>
struct has_non_member_save<Archive, T,
    std::void_t<decltype(save(std::declval<Archive&>(), std::declval<T const&>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_versioned_save : std::false_type {};
template <class Archive, class T>
struct has_non_member_versioned_save<Archive, T,
    std::void_t<decltype(save(std::declval<Archive&>(), std::declval<T const&>(), std::declval<std::uint32_t>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_load : std::false_type {};
template <class Archive, class T>
struct has_non_member_load<Archive, T,
    std::void_t<decltype(load(std::declval<Archive&>(), std::declval<T&>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_versioned_load : std::false_type {};
template <class Archive, class T>
struct has_non_member_versioned_load<Archive, T,
    std::void_t<decltype(load(std::declval<Archive&>(), std::declval<T&>(), std::declval<std::uint32_t>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_save_minimal : std::false_type {};
template <class Archive, class T>
struct has_non_member_save_minimal<Archive, T,
    std::void_t<decltype(save_minimal(std::declval<Archive const&>(), std::declval<T const&>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_versioned_save_minimal : std::false_type {};
template <class Archive, class T>
struct has_non_member_versioned_save_minimal<Archive, T,
    std::void_t<decltype(save_minimal(std::declval<Archive const&>(), std::declval<T const&>(), std::declval<std::uint32_t>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_load_minimal : std::false_type {};
template <class Archive, class T>
struct has_non_member_load_minimal<Archive, T,
    std::void_t<decltype(load_minimal(std::declval<Archive const&>(), std::declval<T&>(), std::declval<int>()))>>
    : std::true_type {};

template <class Archive, class T, class = void>
struct has_non_member_versioned_load_minimal : std::false_type {};
template <class Archive, class T>
struct has_non_member_versioned_load_minimal<Archive, T,
    std::void_t<decltype(load_minimal(std::declval<Archive const&>(), std::declval<T&>(), std::declval<int>(), std::declval<std::uint32_t>()))>>
    : std::true_type {};

} // namespace pfr_detail

//! Serialization for aggregate types using Boost.PFR
/*! This provides automatic serialization for any aggregate type (a struct
    with only public data members and no user-declared constructors, etc.)
    by using Boost.PFR to iterate over all fields with their names.

    Field names are preserved via cereal::make_nvp, so human-readable archives
    (JSON, XML) will include field names, while binary archives will elide them.

    This serialize function is only enabled when:
    - The type is an aggregate (std::is_aggregate_v)
    - The type is not a fundamental/arithmetic type
    - Boost.PFR can iterate its fields (verified via expression SFINAE)
    - The type does NOT already have a member serialize, save, or load function
    - The type does NOT already have a non-member (ADL) serialize function --
      this prevents PFR from silently shadowing a user-defined free serialize
    - The type does NOT already have a non-member (ADL) save/load or
      save_minimal/load_minimal (versioned or not) -- these would otherwise
      be silently shadowed by PFR's combined serialize.  All ten non-member
      detectors live in pfr_detail (rather than reusing cereal::traits) so
      the lookups are not polluted by PFR's own `serialize` overload, and so
      they don't trigger downstream trait instantiations that themselves
      depend on `serialize` and could recurse.

    Usage:
    @code{.cpp}
    struct MyPoint { float x, y, z; };
    // No need to write serialize manually -- PFR handles it automatically
    // JSON output: {"x": 1.0, "y": 2.0, "z": 3.0}
    cereal::JSONOutputArchive ar(os);
    ar(myPoint);
    @endcode

    @note This feature is enabled via the CMake option CEREAL_USE_BOOST_PFR.
          The boost::pfr headers must be available.
    @ingroup PFR */
template <class Archive, class T,
          traits::EnableIf<
              std::is_aggregate_v<T>,
              !std::is_fundamental_v<T>,
              !traits::has_member_serialize<T, Archive>::value,
              !traits::has_member_save<T, Archive>::value,
              !traits::has_member_load<T, Archive>::value,
              !pfr_detail::has_non_member_serialize<Archive, T>::value,
              !pfr_detail::has_non_member_save<Archive, T>::value,
              !pfr_detail::has_non_member_load<Archive, T>::value,
              !pfr_detail::has_non_member_save_minimal<Archive, T>::value,
              !pfr_detail::has_non_member_load_minimal<Archive, T>::value,
              !pfr_detail::has_non_member_versioned_serialize<Archive, T>::value,
              !pfr_detail::has_non_member_versioned_save<Archive, T>::value,
              !pfr_detail::has_non_member_versioned_load<Archive, T>::value,
              !pfr_detail::has_non_member_versioned_save_minimal<Archive, T>::value,
              !pfr_detail::has_non_member_versioned_load_minimal<Archive, T>::value
          > = traits::sfinae>
auto CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar, T& t)
    -> decltype(
        // Expression SFINAE: verify boost::pfr can iterate this type
        boost::pfr::for_each_field(t, [](auto&) {}),
        void()
    )
{
    [&]<std::size_t... Is>(std::index_sequence<Is...>)
    {
        (ar(cereal::make_nvp<Archive>(
            boost::pfr::get_name<Is, T>().data(),
            boost::pfr::get<Is>(t)
        )), ...);
    }(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
}

} // namespace cereal

#endif // CEREAL_USE_BOOST_PFR

#endif // CEREAL_TYPES_PFR_HPP_
