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
#include <type_traits>
#include <utility>

namespace cereal
{
namespace pfr_detail
{

//! Detect whether a non-member `serialize(Archive&, T&)` exists in T's namespace
//! (i.e. a user-provided ADL serialize) WITHOUT picking up cereal's own
//! PFR-namespaced `serialize`.  We perform the lookup from this dedicated
//! namespace via ADL on T only; cereal::serialize is invisible here.
template <class Archive, class T, class = void>
struct has_non_member_serialize : std::false_type {};

template <class Archive, class T>
struct has_non_member_serialize<Archive, T,
    std::void_t<decltype(serialize(std::declval<Archive&>(), std::declval<T&>()))>>
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
              !pfr_detail::has_non_member_serialize<Archive, T>::value
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
