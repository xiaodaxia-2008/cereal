#include "cereal/details/binding_map.hpp"
#include "cereal/details/static_object.hpp"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"

namespace cereal
{
namespace detail
{

template <class T>
T &StaticObject<T>::createShared()
{
    static T t;
    return t;
}

template PolymorphicCasters &StaticObject<PolymorphicCasters>::createShared();

template Versions &StaticObject<Versions>::createShared();

template OutputBindingMap<BinaryOutputArchive> &
StaticObject<OutputBindingMap<BinaryOutputArchive>>::createShared();

template InputBindingMap<BinaryInputArchive> &
StaticObject<InputBindingMap<BinaryInputArchive>>::createShared();

template OutputBindingMap<JSONOutputArchive> &
StaticObject<OutputBindingMap<JSONOutputArchive>>::createShared();

template InputBindingMap<JSONInputArchive> &
StaticObject<InputBindingMap<JSONInputArchive>>::createShared();
} // namespace detail
} // namespace cereal