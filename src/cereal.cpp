#include "cereal/details/binding_map.hpp"
#include "cereal/details/static_object.hpp"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/archives/portable_binary.hpp"
#include "cereal/archives/xml.hpp"

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

        template OutputBindingMap<PortableBinaryOutputArchive> &
        StaticObject<OutputBindingMap<PortableBinaryOutputArchive>>::createShared();

        template InputBindingMap<PortableBinaryInputArchive> &
        StaticObject<InputBindingMap<PortableBinaryInputArchive>>::createShared();

        template OutputBindingMap<XMLOutputArchive> &
        StaticObject<OutputBindingMap<XMLOutputArchive>>::createShared();

        template InputBindingMap<XMLInputArchive> &
        StaticObject<InputBindingMap<XMLInputArchive>>::createShared();
    } // namespace detail
} // namespace cereal