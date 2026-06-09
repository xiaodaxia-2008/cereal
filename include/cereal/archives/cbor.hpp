/**
 * Copyright © 2026 Zen Shawn. All rights reserved.
 *
 * @file CborArchive.hpp
 * @author Zen Shawn
 * @email xiaozisheng2008@hotmail.com
 * @date 16:50:00, June 04, 2026
 */
#ifndef CEREAL_ARCHIVES_CBOR_HPP_
#define CEREAL_ARCHIVES_CBOR_HPP_

#include "cereal/cereal.hpp"
#include "cereal/details/util.hpp"

#include <cmath>
#include <cstring>
#include <istream>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <stack>
#include <string>
#include <valarray>
#include <vector>

namespace cereal
{
namespace detail
{

// ============================================================================
// CBOR low-level constants and helper functions
// ============================================================================
namespace cbor
{
namespace major
{
inline constexpr uint8_t uint = 0;   // Unsigned integer
inline constexpr uint8_t nint = 1;   // Negative integer
inline constexpr uint8_t bstr = 2;   // Byte string
inline constexpr uint8_t tstr = 3;   // Text string
inline constexpr uint8_t array = 4;  // Array of data items
inline constexpr uint8_t map = 5;    // Map of key-value pairs
inline constexpr uint8_t tag = 6;    // Semantic tag
inline constexpr uint8_t simple = 7; // Simple value or float
} // namespace major

namespace info
{
inline constexpr uint8_t uint8_follows = 24; // 1-byte argument follows
inline constexpr uint8_t uint16_follows =
    25; // 2-byte argument follows (big-endian)
inline constexpr uint8_t uint32_follows =
    26; // 4-byte argument follows (big-endian)
inline constexpr uint8_t uint64_follows =
    27; // 8-byte argument follows (big-endian)
inline constexpr uint8_t indefinite = 31; // Indefinite length
} // namespace info

namespace simple
{
inline constexpr uint8_t false_value = 20;
inline constexpr uint8_t true_value = 21;
inline constexpr uint8_t null_value = 22;
inline constexpr uint8_t float16 = 25; // IEEE 754 half-precision (16-bit)
inline constexpr uint8_t float32 = 26; // IEEE 754 single-precision (32-bit)
inline constexpr uint8_t float64 = 27; // IEEE 754 double-precision (64-bit)
inline constexpr uint8_t break_code =
    31; // "break" stop code for indefinite containers
} // namespace simple

[[nodiscard]] constexpr uint8_t initial_byte(uint8_t major_type,
                                             uint8_t additional_info) noexcept
{
    return static_cast<uint8_t>((major_type << 5) | (additional_info & 0x1f));
}

[[nodiscard]] constexpr uint8_t get_major_type(uint8_t initial) noexcept
{
    return initial >> 5;
}

[[nodiscard]] constexpr uint8_t get_additional_info(uint8_t initial) noexcept
{
    return initial & 0x1f;
}

[[nodiscard]] inline double decode_half(uint16_t half) noexcept
{
    const int sign = (half >> 15) & 1;
    const int exp = (half >> 10) & 0x1f;
    const int mant = half & 0x3ff;

    double val;
    if (exp == 0) {
        val = std::ldexp(static_cast<double>(mant), -24);
    }
    else if (exp != 31) {
        val = std::ldexp(static_cast<double>(mant + 1024), exp - 25);
    }
    else {
        val = (mant == 0) ? std::numeric_limits<double>::infinity()
                          : std::numeric_limits<double>::quiet_NaN();
    }
    return sign ? -val : val;
}
} // namespace cbor

// Low-level buffer dumping helpers
inline void dumpByte(uint8_t byte, std::string &buffer, size_t &pos)
{
    if (pos >= buffer.size()) {
        buffer.resize(buffer.size() == 0 ? 128 : buffer.size() * 2);
    }
    buffer[pos] = static_cast<char>(byte);
    ++pos;
}

inline void dumpBe16(uint16_t val, std::string &buffer, size_t &pos)
{
    dumpByte(static_cast<uint8_t>(val >> 8), buffer, pos);
    dumpByte(static_cast<uint8_t>(val), buffer, pos);
}

inline void dumpBe32(uint32_t val, std::string &buffer, size_t &pos)
{
    dumpByte(static_cast<uint8_t>(val >> 24), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 16), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 8), buffer, pos);
    dumpByte(static_cast<uint8_t>(val), buffer, pos);
}

inline void dumpBe64(uint64_t val, std::string &buffer, size_t &pos)
{
    dumpByte(static_cast<uint8_t>(val >> 56), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 48), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 40), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 32), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 24), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 16), buffer, pos);
    dumpByte(static_cast<uint8_t>(val >> 8), buffer, pos);
    dumpByte(static_cast<uint8_t>(val), buffer, pos);
}

inline void encodeArg(uint8_t major_type, uint64_t value, std::string &buffer,
                      size_t &pos)
{
    using namespace cbor;

    if (value < 24) {
        dumpByte(initial_byte(major_type, static_cast<uint8_t>(value)), buffer,
                 pos);
    }
    else if (value <= 0xFF) {
        dumpByte(initial_byte(major_type, info::uint8_follows), buffer, pos);
        dumpByte(static_cast<uint8_t>(value), buffer, pos);
    }
    else if (value <= 0xFFFF) {
        dumpByte(initial_byte(major_type, info::uint16_follows), buffer, pos);
        dumpBe16(static_cast<uint16_t>(value), buffer, pos);
    }
    else if (value <= 0xFFFFFFFF) {
        dumpByte(initial_byte(major_type, info::uint32_follows), buffer, pos);
        dumpBe32(static_cast<uint32_t>(value), buffer, pos);
    }
    else {
        dumpByte(initial_byte(major_type, info::uint64_follows), buffer, pos);
        dumpBe64(value, buffer, pos);
    }
}

struct CborNode
{
    enum Type : uint8_t
    {
        Null,
        Bool,
        Int,
        Uint,
        Float,
        Text,
        Bytes,
        Array,
        Map
    };

    Type type = Null;

    bool b = false;
    int64_t i = 0;
    uint64_t u = 0;
    double d = 0.0;
    std::string s;
    std::vector<uint8_t> bin;

    std::vector<CborNode> arr;
    std::vector<std::pair<std::string, CborNode>> obj;

    CborNode() = default;

    explicit CborNode(bool v) : type(Bool), b(v)
    {
    }

    explicit CborNode(int64_t v) : type(Int), i(v)
    {
    }

    explicit CborNode(uint64_t v) : type(Uint), u(v)
    {
    }

    explicit CborNode(double v) : type(Float), d(v)
    {
    }

    explicit CborNode(std::string v) : type(Text), s(std::move(v))
    {
    }

    explicit CborNode(std::vector<uint8_t> v) : type(Bytes), bin(std::move(v))
    {
    }

    bool isNull() const
    {
        return type == Null;
    }
};

// Read shift-based big endian values
inline uint16_t readBe16(const uint8_t *&it, const uint8_t *end)
{
    if (it + 2 > end) {
        throw Exception("Unexpected end of CBOR data");
    }
    uint16_t val =
        (static_cast<uint16_t>(it[0]) << 8) | static_cast<uint16_t>(it[1]);
    it += 2;
    return val;
}

inline uint32_t readBe32(const uint8_t *&it, const uint8_t *end)
{
    if (it + 4 > end) {
        throw Exception("Unexpected end of CBOR data");
    }
    uint32_t val = (static_cast<uint32_t>(it[0]) << 24) |
                   (static_cast<uint32_t>(it[1]) << 16) |
                   (static_cast<uint32_t>(it[2]) << 8) |
                   static_cast<uint32_t>(it[3]);
    it += 4;
    return val;
}

inline uint64_t readBe64(const uint8_t *&it, const uint8_t *end)
{
    if (it + 8 > end) {
        throw Exception("Unexpected end of CBOR data");
    }
    uint64_t val = (static_cast<uint64_t>(it[0]) << 56) |
                   (static_cast<uint64_t>(it[1]) << 48) |
                   (static_cast<uint64_t>(it[2]) << 40) |
                   (static_cast<uint64_t>(it[3]) << 32) |
                   (static_cast<uint64_t>(it[4]) << 24) |
                   (static_cast<uint64_t>(it[5]) << 16) |
                   (static_cast<uint64_t>(it[6]) << 8) |
                   static_cast<uint64_t>(it[7]);
    it += 8;
    return val;
}

inline uint64_t decodeArg(const uint8_t *&it, const uint8_t *end,
                          uint8_t additionalInfo)
{
    using namespace cbor;

    if (additionalInfo < 24) {
        return additionalInfo;
    }

    switch (additionalInfo) {
    case info::uint8_follows: {
        if (it >= end) {
            throw Exception("Unexpected end of CBOR data");
        }
        return *it++;
    }
    case info::uint16_follows:
        return readBe16(it, end);
    case info::uint32_follows:
        return readBe32(it, end);
    case info::uint64_follows:
        return readBe64(it, end);
    default:
        throw Exception("Invalid CBOR additional info");
    }
}

CborNode parseCborNode(const uint8_t *&it, const uint8_t *end);

inline std::vector<uint8_t> parseIndefiniteBstr(const uint8_t *&it,
                                                const uint8_t *end)
{
    using namespace cbor;
    std::vector<uint8_t> result;

    while (it < end) {
        if (*it == simple::break_code) {
            ++it;
            break;
        }
        uint8_t initial = *it++;
        uint8_t major = get_major_type(initial);
        uint8_t info = get_additional_info(initial);

        if (major != major::bstr) {
            throw Exception("Expected byte string chunk in indefinite bstr");
        }

        uint64_t len = decodeArg(it, end, info);
        if (it + len > end) {
            throw Exception("Unexpected end of CBOR data");
        }
        result.insert(result.end(), it, it + len);
        it += len;
    }
    return result;
}

inline std::string parseIndefiniteTstr(const uint8_t *&it, const uint8_t *end)
{
    using namespace cbor;
    std::string result;

    while (it < end) {
        if (*it == simple::break_code) {
            ++it;
            break;
        }
        uint8_t initial = *it++;
        uint8_t major = get_major_type(initial);
        uint8_t info = get_additional_info(initial);

        if (major != major::tstr) {
            throw Exception("Expected text string chunk in indefinite tstr");
        }

        uint64_t len = decodeArg(it, end, info);
        if (it + len > end) {
            throw Exception("Unexpected end of CBOR data");
        }
        result.append(reinterpret_cast<const char *>(it), len);
        it += len;
    }
    return result;
}

inline CborNode parseCborNode(const uint8_t *&it, const uint8_t *end)
{
    using namespace cbor;

    if (it >= end) {
        throw Exception("Unexpected end of CBOR data");
    }

    uint8_t initial = *it++;
    uint8_t majorType = get_major_type(initial);
    uint8_t additionalInfo = get_additional_info(initial);

    switch (majorType) {
    case major::uint: {
        uint64_t val = decodeArg(it, end, additionalInfo);
        if (val <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            CborNode n;
            n.type = CborNode::Int;
            n.i = static_cast<int64_t>(val);
            return n;
        }
        else {
            CborNode n;
            n.type = CborNode::Uint;
            n.u = val;
            return n;
        }
    }
    case major::nint: {
        uint64_t val = decodeArg(it, end, additionalInfo);
        CborNode n;
        n.type = CborNode::Int;
        n.i = -1 - static_cast<int64_t>(val);
        return n;
    }
    case major::bstr: {
        if (additionalInfo == info::indefinite) {
            return CborNode(parseIndefiniteBstr(it, end));
        }
        uint64_t len = decodeArg(it, end, additionalInfo);
        if (it + len > end) {
            throw Exception("Unexpected end of CBOR data");
        }
        std::vector<uint8_t> bytes(it, it + len);
        it += len;
        return CborNode(std::move(bytes));
    }
    case major::tstr: {
        if (additionalInfo == info::indefinite) {
            return CborNode(parseIndefiniteTstr(it, end));
        }
        uint64_t len = decodeArg(it, end, additionalInfo);
        if (it + len > end) {
            throw Exception("Unexpected end of CBOR data");
        }
        std::string s(reinterpret_cast<const char *>(it), len);
        it += len;
        return CborNode(std::move(s));
    }
    case major::array: {
        if (additionalInfo == info::indefinite) {
            CborNode n;
            n.type = CborNode::Array;
            while (it < end) {
                if (*it == simple::break_code) {
                    ++it;
                    break;
                }
                n.arr.push_back(parseCborNode(it, end));
            }
            return n;
        }
        uint64_t count = decodeArg(it, end, additionalInfo);
        CborNode n;
        n.type = CborNode::Array;
        n.arr.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) {
            n.arr.push_back(parseCborNode(it, end));
        }
        return n;
    }
    case major::map: {
        if (additionalInfo == info::indefinite) {
            CborNode n;
            n.type = CborNode::Map;
            while (it < end) {
                if (*it == simple::break_code) {
                    ++it;
                    break;
                }
                CborNode keyNode = parseCborNode(it, end);
                if (keyNode.type != CborNode::Text) {
                    throw Exception("Expected text key in CBOR map");
                }
                CborNode valNode = parseCborNode(it, end);
                n.obj.emplace_back(std::move(keyNode.s), std::move(valNode));
            }
            return n;
        }
        uint64_t count = decodeArg(it, end, additionalInfo);
        CborNode n;
        n.type = CborNode::Map;
        n.obj.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) {
            CborNode keyNode = parseCborNode(it, end);
            if (keyNode.type != CborNode::Text) {
                throw Exception("Expected text key in CBOR map");
            }
            CborNode valNode = parseCborNode(it, end);
            n.obj.emplace_back(std::move(keyNode.s), std::move(valNode));
        }
        return n;
    }
    case major::tag: {
        uint64_t tagVal = decodeArg(it, end, additionalInfo);
        (void)tagVal;
        return parseCborNode(it, end);
    }
    case major::simple: {
        switch (additionalInfo) {
        case simple::false_value:
            return CborNode(false);
        case simple::true_value:
            return CborNode(true);
        case simple::null_value:
            return CborNode();
        case simple::float16: {
            uint16_t half = readBe16(it, end);
            CborNode n;
            n.type = CborNode::Float;
            n.d = decode_half(half);
            return n;
        }
        case simple::float32: {
            uint32_t bits = readBe32(it, end);
            float f;
            std::memcpy(&f, &bits, 4);
            CborNode n;
            n.type = CborNode::Float;
            n.d = static_cast<double>(f);
            return n;
        }
        case simple::float64: {
            uint64_t bits = readBe64(it, end);
            double d;
            std::memcpy(&d, &bits, 8);
            CborNode n;
            n.type = CborNode::Float;
            n.d = d;
            return n;
        }
        default:
            throw Exception("Unsupported CBOR simple value");
        }
    }
    default:
        throw Exception("Unknown CBOR major type");
    }
}

inline CborNode parseCbor(const std::string &buffer)
{
    const auto *it = reinterpret_cast<const uint8_t *>(buffer.data());
    const auto *end = it + buffer.size();
    return parseCborNode(it, end);
}

} // namespace detail

// ============================================================================
// CborOutputArchive
// ============================================================================
class CborOutputArchive : public OutputArchive<CborOutputArchive>
{
    enum class NodeType
    {
        StartObject,
        InObject,
        StartArray,
        InArray
    };

public:
    class Options
    {
    public:
        static Options Default()
        {
            return Options();
        }

    private:
        friend class CborOutputArchive;
    };

    CborOutputArchive(std::ostream &stream,
                      Options const & = Options::Default())
        : OutputArchive<CborOutputArchive>(this), itsStream(stream),
          itsNextName(nullptr), itsPos(0)
    {
        itsBuffer.reserve(4096);
        itsNodeStack.push(NodeType::StartObject);
        itsNameCounter.push(0);
    }

    ~CborOutputArchive() CEREAL_NOEXCEPT
    {
        using namespace detail::cbor;
        while (!itsNodeStack.empty()) {
            auto &top = itsNodeStack.top();
            switch (top) {
            case NodeType::InArray:
            case NodeType::InObject:
                detail::dumpByte(simple::break_code, itsBuffer, itsPos);
                break;
            case NodeType::StartArray:
                detail::dumpByte(initial_byte(major::array, 0), itsBuffer,
                                 itsPos);
                break;
            case NodeType::StartObject:
                detail::dumpByte(initial_byte(major::map, 0), itsBuffer,
                                 itsPos);
                break;
            }
            itsNodeStack.pop();
            if (!itsNameCounter.empty()) {
                itsNameCounter.pop();
            }
        }
        itsStream.write(itsBuffer.data(), itsBuffer.size());
    }

    void saveBinaryValue(const void *data, size_t size)
    {
        detail::encodeArg(detail::cbor::major::bstr, size, itsBuffer, itsPos);
        if (size > 0) {
            ensureCapacity(size);
            std::memcpy(&itsBuffer[itsPos], data, size);
            itsPos += size;
        }
    }

    void saveBinaryValue(const void *data, size_t size, const char *name)
    {
        if (name) {
            setNextName(name);
        }
        writeName();
        saveBinaryValue(data, size);
    }

    void startNode()
    {
        writeName();
        itsNodeStack.push(NodeType::StartObject);
        itsNameCounter.push(0);
    }

    void finishNode()
    {
        using namespace detail::cbor;

        switch (itsNodeStack.top()) {
        case NodeType::StartArray:
            detail::dumpByte(initial_byte(major::array, 0), itsBuffer, itsPos);
            break;
        case NodeType::StartObject:
            detail::dumpByte(initial_byte(major::map, 0), itsBuffer, itsPos);
            break;
        case NodeType::InArray:
        case NodeType::InObject:
            detail::dumpByte(simple::break_code, itsBuffer, itsPos);
            break;
        }

        itsNodeStack.pop();
        itsNameCounter.pop();
    }

    void setNextName(const char *name)
    {
        itsNextName = name;
    }

    void saveValue(bool b)
    {
        using namespace detail::cbor;
        detail::dumpByte(initial_byte(major::simple, b ? simple::true_value
                                                       : simple::false_value),
                         itsBuffer, itsPos);
    }

    void saveValue(int i)
    {
        saveInt(static_cast<int64_t>(i));
    }

    void saveValue(unsigned u)
    {
        saveUint(static_cast<uint64_t>(u));
    }

    void saveValue(int64_t i64)
    {
        saveInt(i64);
    }

    void saveValue(uint64_t u64)
    {
        saveUint(u64);
    }

    void saveValue(double d)
    {
        using namespace detail::cbor;
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        detail::dumpByte(initial_byte(major::simple, simple::float64),
                         itsBuffer, itsPos);
        detail::dumpBe64(bits, itsBuffer, itsPos);
    }

    void saveValue(std::string const &s)
    {
        detail::encodeArg(detail::cbor::major::tstr, s.size(), itsBuffer,
                          itsPos);
        if (!s.empty()) {
            ensureCapacity(s.size());
            std::memcpy(&itsBuffer[itsPos], s.data(), s.size());
            itsPos += s.size();
        }
    }

    template <class CharT, class Traits, class Alloc,
              typename std::enable_if<!std::is_same<CharT, char>::value, int>::type = 0>
    void saveValue(std::basic_string<CharT, Traits, Alloc> const &s)
    {
        detail::encodeArg(detail::cbor::major::bstr, s.size() * sizeof(CharT), itsBuffer, itsPos);
        if (s.size() > 0) {
            ensureCapacity(s.size() * sizeof(CharT));
            std::memcpy(&itsBuffer[itsPos], s.data(), s.size() * sizeof(CharT));
            itsPos += s.size() * sizeof(CharT);
        }
    }

    void saveValue(char const *s)
    {
        saveValue(std::string(s));
    }

    void saveValue(std::nullptr_t)
    {
        using namespace detail::cbor;
        detail::dumpByte(initial_byte(major::simple, simple::null_value),
                         itsBuffer, itsPos);
    }

    template <class T>
    inline typename std::enable_if<!std::is_same<T, int64_t>::value &&
                                       std::is_same<T, long long>::value,
                                   void>::type
    saveValue(T val)
    {
        saveInt(static_cast<int64_t>(val));
    }

    template <class T>
    inline
        typename std::enable_if<!std::is_same<T, uint64_t>::value &&
                                    std::is_same<T, unsigned long long>::value,
                                void>::type
        saveValue(T val)
    {
        saveUint(static_cast<uint64_t>(val));
    }

private:
    void saveInt(int64_t val)
    {
        using namespace detail::cbor;
        if (val >= 0) {
            detail::encodeArg(major::uint, static_cast<uint64_t>(val),
                              itsBuffer, itsPos);
        }
        else {
            detail::encodeArg(major::nint, static_cast<uint64_t>(-1 - val),
                              itsBuffer, itsPos);
        }
    }

    void saveUint(uint64_t val)
    {
        detail::encodeArg(detail::cbor::major::uint, val, itsBuffer, itsPos);
    }

    template <class T,
              traits::EnableIf<sizeof(T) == sizeof(std::int32_t),
                               std::is_signed<T>::value> = traits::sfinae>
    inline void saveLong(T l)
    {
        saveValue(static_cast<std::int32_t>(l));
    }

    template <class T,
              traits::EnableIf<sizeof(T) != sizeof(std::int32_t),
                               std::is_signed<T>::value> = traits::sfinae>
    inline void saveLong(T l)
    {
        saveValue(static_cast<std::int64_t>(l));
    }

    template <class T,
              traits::EnableIf<sizeof(T) == sizeof(std::int32_t),
                               std::is_unsigned<T>::value> = traits::sfinae>
    inline void saveLong(T lu)
    {
        saveValue(static_cast<std::uint32_t>(lu));
    }

    template <class T,
              traits::EnableIf<sizeof(T) != sizeof(std::int32_t),
                               std::is_unsigned<T>::value> = traits::sfinae>
    inline void saveLong(T lu)
    {
        saveValue(static_cast<std::uint64_t>(lu));
    }

public:
#if defined(_MSC_VER) && _MSC_VER < 1916
    void saveValue(unsigned long lu)
    {
        saveLong(lu);
    };
#else
    template <class T, traits::EnableIf<std::is_same<T, long>::value,
                                        !std::is_same<T, int>::value,
                                        !std::is_same<T, std::int64_t>::value> =
                           traits::sfinae>
    inline void saveValue(T t)
    {
        saveLong(t);
    }

    template <class T,
              traits::EnableIf<std::is_same<T, unsigned long>::value,
                               !std::is_same<T, unsigned>::value,
                               !std::is_same<T, std::uint64_t>::value> =
                  traits::sfinae>
    inline void saveValue(T t)
    {
        saveLong(t);
    }
#endif

    template <class T,
              traits::EnableIf<
                  std::is_arithmetic<T>::value, !std::is_same<T, long>::value,
                  !std::is_same<T, unsigned long>::value,
                  !std::is_same<T, std::int64_t>::value,
                  !std::is_same<T, std::uint64_t>::value,
                  !std::is_same<T, long long>::value,
                  !std::is_same<T, unsigned long long>::value,
                  (sizeof(T) >= sizeof(long double) ||
                   sizeof(T) >= sizeof(long long))> = traits::sfinae>
    inline void saveValue(T const &t)
    {
        std::stringstream ss;
        ss.precision(std::numeric_limits<long double>::max_digits10);
        ss << t;
        saveValue(ss.str());
    }

    void writeName()
    {
        using namespace detail::cbor;
        NodeType &nodeType = itsNodeStack.top();

        if (nodeType == NodeType::StartArray) {
            detail::dumpByte(initial_byte(major::array, info::indefinite),
                             itsBuffer, itsPos);
            nodeType = NodeType::InArray;
        }
        else if (nodeType == NodeType::StartObject) {
            detail::dumpByte(initial_byte(major::map, info::indefinite),
                             itsBuffer, itsPos);
            nodeType = NodeType::InObject;
        }

        if (nodeType == NodeType::InArray) {
            return;
        }

        if (itsNextName != nullptr) {
            size_t len = std::strlen(itsNextName);
            detail::encodeArg(major::tstr, len, itsBuffer, itsPos);
            if (len > 0) {
                ensureCapacity(len);
                std::memcpy(&itsBuffer[itsPos], itsNextName, len);
                itsPos += len;
            }
            itsNextName = nullptr;
        }
        else {
            std::string name =
                "value" + std::to_string(itsNameCounter.top()++) + "\0";
            size_t len = name.size();
            detail::encodeArg(major::tstr, len, itsBuffer, itsPos);
            ensureCapacity(len);
            std::memcpy(&itsBuffer[itsPos], name.data(), len);
            itsPos += len;
        }
    }

    void makeArray()
    {
        itsNodeStack.top() = NodeType::StartArray;
    }



private:
    void ensureCapacity(size_t additional)
    {
        size_t required = itsPos + additional;
        if (required > itsBuffer.size()) {
            itsBuffer.resize(required);
        }
    }

    std::ostream &itsStream;
    std::string itsBuffer;
    size_t itsPos;
    std::stack<NodeType> itsNodeStack;
    std::stack<uint32_t> itsNameCounter;
    char const *itsNextName;
};

// ============================================================================
// CborInputArchive
// ============================================================================
class CborInputArchive : public InputArchive<CborInputArchive>
{
public:
    CborInputArchive(std::istream &stream)
        : InputArchive<CborInputArchive>(this), itsNextName(nullptr), itsKeyNotFound(false)
    {
        std::string buffer(std::istreambuf_iterator<char>(stream), {});
        itsRoot = detail::parseCbor(buffer);

        if (itsRoot.type == detail::CborNode::Array) {
            itsIteratorStack.emplace_back(&itsRoot, Iterator::ArrayTag{});
        }
        else {
            itsIteratorStack.emplace_back(&itsRoot, Iterator::ObjectTag{});
        }
    }

    ~CborInputArchive() CEREAL_NOEXCEPT = default;

    //! Configures how missing keys are handled during deserialization.
    /*! The default policy is to silently ignore keys present in the
        serialized type but missing from the CBOR input (the corresponding
        member keeps its default value). Pass @c false to make a missing key
        throw a cereal::Exception instead.
        @param ignore If true (the default), silently skip missing keys */
    void setIgnoreMissingKeys(bool ignore) { m_ignoreMissingKeys = ignore; }

    //! Returns the current "ignore missing keys" policy.
    bool shouldIgnoreMissingKeys() const { return m_ignoreMissingKeys; }

    //! Returns true if the given key exists in the current CBOR map node.
    //! Used to implement ignore-missing-key behaviour for NVPs.
    bool hasName(const char *name) const
    {
        if (name && !itsIteratorStack.empty()) {
            auto const actualName = itsIteratorStack.back().name();
            if (actualName && std::strcmp(name, actualName) == 0)
                return true;
            return itsIteratorStack.back().hasName(name);
        }
        return false;
    }

    void loadBinaryValue(void *data, size_t size)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }

        auto &val = itsIteratorStack.back().value();
        if (val.type != detail::CborNode::Bytes) {
            throw Exception("Expected CBOR byte string for binary data");
        }
        if (val.bin.size() != size) {
            throw Exception("Binary data size mismatch");
        }

        std::memcpy(data, val.bin.data(), size);
        ++itsIteratorStack.back();
        itsNextName = nullptr;
    }

    void loadBinaryValue(void *data, size_t size, const char *name)
    {
        if (name) {
            itsNextName = name;
        }
        loadBinaryValue(data, size);
    }

    class Iterator
    {
    public:
        struct ObjectTag
        {
        };

        struct ArrayTag
        {
        };

        struct MissingTag
        {
        };

        Iterator() : itsParent(nullptr), itsIndex(0), itsSize(0), itsIsMissing(false)
        {
        }

        Iterator(detail::CborNode *parent, ObjectTag)
            : itsParent(parent), itsIndex(0), itsIsMissing(false)
        {
            if (parent->type == detail::CborNode::Map) {
                itsKeys.reserve(parent->obj.size());
                for (auto &[k, v] : parent->obj) {
                    itsKeys.push_back(k);
                }
                itsSize = itsKeys.size();
            }
        }

        Iterator(detail::CborNode *parent, ArrayTag)
            : itsParent(parent), itsIndex(0), itsIsMissing(false)
        {
            if (parent->type == detail::CborNode::Array) {
                itsSize = parent->arr.size();
            }
        }

        Iterator(std::nullptr_t, MissingTag)
            : itsParent(nullptr), itsIndex(0), itsSize(0), itsIsMissing(true)
        {
        }

        bool isMissing() const
        {
            return itsIsMissing;
        }

        size_t size() const
        {
            return itsSize;
        }

        Iterator &operator++()
        {
            if (itsIsMissing) {
                return *this;
            }
            ++itsIndex;
            return *this;
        }

        const detail::CborNode &value() const
        {
            if (itsIsMissing) {
                static const detail::CborNode dummyNode;
                return dummyNode;
            }
            if (itsIndex >= itsSize) {
                throw Exception("No more objects in CBOR input");
            }

            if (itsParent->type == detail::CborNode::Array) {
                return itsParent->arr[itsIndex];
            }
            else {
                return itsParent->obj[itsIndex].second;
            }
        }

        detail::CborNode &mutableValue()
        {
            if (itsIsMissing) {
                static detail::CborNode dummyNode;
                return dummyNode;
            }
            if (itsIndex >= itsSize) {
                throw Exception("No more objects in CBOR input");
            }

            if (itsParent->type == detail::CborNode::Array) {
                return itsParent->arr[itsIndex];
            }
            else {
                return itsParent->obj[itsIndex].second;
            }
        }

        const char *name() const
        {
            if (itsIsMissing) {
                return nullptr;
            }
            if (itsParent->type == detail::CborNode::Map &&
                itsIndex < itsKeys.size())
            {
                return itsKeys[itsIndex].c_str();
            }
            return nullptr;
        }

        inline bool search(const char *searchName)
        {
            if (itsIsMissing) {
                return false;
            }
            if (itsParent->type == detail::CborNode::Map) {
                const auto len = std::strlen(searchName);
                for (size_t i = 0; i < itsKeys.size(); ++i) {
                    const auto &key = itsKeys[i];
                    if (key.size() == len &&
                        std::strncmp(searchName, key.data(), len) == 0)
                    {
                        itsIndex = i;
                        return true;
                    }
                }
            }
            return false;
        }

        inline bool hasName(const char *searchName) const
        {
            if (itsIsMissing) {
                return false;
            }
            if (itsParent->type == detail::CborNode::Map) {
                const auto len = std::strlen(searchName);
                for (const auto &key : itsKeys) {
                    if (key.size() == len &&
                        std::strncmp(searchName, key.data(), len) == 0)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

    private:
        detail::CborNode *itsParent;
        std::vector<std::string> itsKeys;
        size_t itsIndex;
        size_t itsSize;
        bool itsIsMissing;
    };

    inline void search()
    {
        if (itsIteratorStack.back().isMissing()) {
            itsKeyNotFound = true;
            itsNextName = nullptr;
            return;
        }

        auto localNextName = itsNextName;
        itsNextName = nullptr;

        if (localNextName) {
            auto const actualName = itsIteratorStack.back().name();

            if (!actualName || std::strcmp(localNextName, actualName) != 0) {
                if (!itsIteratorStack.back().search(localNextName)) {
                    if (m_ignoreMissingKeys)
                        itsKeyNotFound = true;
                    else
                        throw Exception("CBOR Parsing failed - provided NVP (" +
                                        std::string(localNextName) + ") not found");
                }
            }
        }
    }

    void startNode()
    {
        search();

        if (itsKeyNotFound) {
            itsIteratorStack.emplace_back(nullptr, Iterator::MissingTag{});
            itsKeyNotFound = false;
            return;
        }

        auto &current = itsIteratorStack.back().mutableValue();
        if (current.type == detail::CborNode::Array) {
            itsIteratorStack.emplace_back(&current, Iterator::ArrayTag{});
        }
        else {
            itsIteratorStack.emplace_back(&current, Iterator::ObjectTag{});
        }
    }

    void finishNode()
    {
        bool wasMissing = itsIteratorStack.back().isMissing();
        itsIteratorStack.pop_back();
        if (!wasMissing) {
            ++itsIteratorStack.back();
        }
    }

    const char *getNodeName() const
    {
        return itsIteratorStack.back().name();
    }

    void setNextName(const char *name)
    {
        itsNextName = name;
    }

    template <class T,
              traits::EnableIf<std::is_signed<T>::value,
                               sizeof(T) < sizeof(int64_t)> = traits::sfinae>
    inline void loadValue(T &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = static_cast<T>(readNumber());
        ++itsIteratorStack.back();
    }

    template <class T,
              traits::EnableIf<std::is_unsigned<T>::value,
                               sizeof(T) < sizeof(uint64_t),
                               !std::is_same<bool, T>::value> = traits::sfinae>
    inline void loadValue(T &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = static_cast<T>(readNumber());
        ++itsIteratorStack.back();
    }

    void loadValue(bool &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        auto &v = itsIteratorStack.back().value();
        if (v.type != detail::CborNode::Bool) {
            throw Exception("Expected boolean in CBOR");
        }
        val = v.b;
        ++itsIteratorStack.back();
    }

    void loadValue(int64_t &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = readInt();
        ++itsIteratorStack.back();
    }

    void loadValue(uint64_t &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = readUint();
        ++itsIteratorStack.back();
    }

    void loadValue(float &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = static_cast<float>(readNumber());
        ++itsIteratorStack.back();
    }

    void loadValue(double &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = readNumber();
        ++itsIteratorStack.back();
    }

    void loadValue(std::string &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        auto &v = itsIteratorStack.back().value();
        if (v.type != detail::CborNode::Text) {
            throw Exception("Expected string in CBOR");
        }
        val = v.s;
        ++itsIteratorStack.back();
    }

    template <class CharT, class Traits, class Alloc,
              typename std::enable_if<!std::is_same<CharT, char>::value, int>::type = 0>
    void loadValue(std::basic_string<CharT, Traits, Alloc> &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        auto &v = itsIteratorStack.back().value();
        if (v.type != detail::CborNode::Bytes) {
            throw Exception("Expected CBOR byte string for wide string");
        }
        val.resize(v.bin.size() / sizeof(CharT));
        if (v.bin.size() > 0) {
            std::memcpy(const_cast<CharT *>(val.data()), v.bin.data(), v.bin.size());
        }
        ++itsIteratorStack.back();
    }

    void loadValue(std::nullptr_t &)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        if (!itsIteratorStack.back().value().isNull()) {
            throw Exception("Expected null value in CBOR");
        }
        ++itsIteratorStack.back();
    }

    template <class T>
    inline typename std::enable_if<!std::is_same<T, int64_t>::value &&
                                       std::is_same<T, long long>::value,
                                   void>::type
    loadValue(T &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = static_cast<T>(readInt());
        ++itsIteratorStack.back();
    }

    template <class T>
    inline
        typename std::enable_if<!std::is_same<T, uint64_t>::value &&
                                    std::is_same<T, unsigned long long>::value,
                                void>::type
        loadValue(T &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        val = static_cast<T>(readUint());
        ++itsIteratorStack.back();
    }

#ifndef _MSC_VER
private:
    template <class T>
    inline typename std::enable_if<sizeof(T) == sizeof(std::int32_t) &&
                                       std::is_signed<T>::value,
                                   void>::type
    loadLong(T &l)
    {
        loadValue(reinterpret_cast<std::int32_t &>(l));
    }

    template <class T>
    inline typename std::enable_if<sizeof(T) == sizeof(std::int64_t) &&
                                       std::is_signed<T>::value,
                                   void>::type
    loadLong(T &l)
    {
        loadValue(reinterpret_cast<std::int64_t &>(l));
    }

    template <class T>
    inline typename std::enable_if<sizeof(T) == sizeof(std::uint32_t) &&
                                       !std::is_signed<T>::value,
                                   void>::type
    loadLong(T &lu)
    {
        loadValue(reinterpret_cast<std::uint32_t &>(lu));
    }

    template <class T>
    inline typename std::enable_if<sizeof(T) == sizeof(std::uint64_t) &&
                                       !std::is_signed<T>::value,
                                   void>::type
    loadLong(T &lu)
    {
        loadValue(reinterpret_cast<std::uint64_t &>(lu));
    }

public:
    template <class T>
    inline typename std::enable_if<std::is_same<T, long>::value &&
                                       sizeof(T) >= sizeof(std::int64_t) &&
                                       !std::is_same<T, std::int64_t>::value,
                                   void>::type
    loadValue(T &t)
    {
        loadLong(t);
    }

    template <class T>
    inline typename std::enable_if<std::is_same<T, unsigned long>::value &&
                                       sizeof(T) >= sizeof(std::uint64_t) &&
                                       !std::is_same<T, std::uint64_t>::value,
                                   void>::type
    loadValue(T &t)
    {
        loadLong(t);
    }
#endif

private:
    double readNumber()
    {
        auto &v = itsIteratorStack.back().value();
        switch (v.type) {
        case detail::CborNode::Int:
            return static_cast<double>(v.i);
        case detail::CborNode::Uint:
            return static_cast<double>(v.u);
        case detail::CborNode::Float:
            return v.d;
        default:
            throw Exception("Expected numeric value in CBOR");
        }
    }

    int64_t readInt()
    {
        auto &v = itsIteratorStack.back().value();
        switch (v.type) {
        case detail::CborNode::Int:
            return v.i;
        case detail::CborNode::Uint:
            if (v.u >
                static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            {
                throw Exception("CBOR uint value out of range for int64");
            }
            return static_cast<int64_t>(v.u);
        case detail::CborNode::Float:
            return static_cast<int64_t>(v.d);
        default:
            throw Exception("Expected numeric value in CBOR");
        }
    }

    uint64_t readUint()
    {
        auto &v = itsIteratorStack.back().value();
        switch (v.type) {
        case detail::CborNode::Int:
            if (v.i < 0) {
                throw Exception("CBOR negative value out of range for uint64");
            }
            return static_cast<uint64_t>(v.i);
        case detail::CborNode::Uint:
            return v.u;
        case detail::CborNode::Float:
            return static_cast<uint64_t>(v.d);
        default:
            throw Exception("Expected numeric value in CBOR");
        }
    }

    void stringToNumber(std::string const &str, long long &val)
    {
        val = std::stoll(str);
    }

    void stringToNumber(std::string const &str, unsigned long long &val)
    {
        val = std::stoull(str);
    }

    void stringToNumber(std::string const &str, long double &val)
    {
        val = std::stold(str);
    }

public:
    template <class T,
              traits::EnableIf<
                  std::is_arithmetic<T>::value, !std::is_same<T, long>::value,
                  !std::is_same<T, unsigned long>::value,
                  !std::is_same<T, std::int64_t>::value,
                  !std::is_same<T, std::uint64_t>::value,
                  !std::is_same<T, long long>::value,
                  !std::is_same<T, unsigned long long>::value,
                  (sizeof(T) >= sizeof(long double) ||
                   sizeof(T) >= sizeof(long long))> = traits::sfinae>
    inline void loadValue(T &val)
    {
        search();
        if (itsKeyNotFound) {
            itsKeyNotFound = false;
            return;
        }
        std::string encoded;
        loadValue(encoded);
        stringToNumber(encoded, val);
    }

    void loadSize(size_type &size)
    {
        if (itsIteratorStack.back().isMissing()) {
            size = 0;
            return;
        }
        size = static_cast<size_type>(itsIteratorStack.back().size());
    }

private:
    const char *itsNextName;
    detail::CborNode itsRoot;
    std::vector<Iterator> itsIteratorStack;
    bool itsKeyNotFound;
    bool m_ignoreMissingKeys = true;   //!< If true, silently skip missing keys (default preserves prior "ignore" semantics)
};

// ============================================================================
// Prologues/Epilogues for CborArchive
// ============================================================================
template <class T>
inline void prologue(CborOutputArchive &, NameValuePair<T> const &)
{
}

template <class T>
inline void prologue(CborInputArchive &, NameValuePair<T> const &)
{
}

template <class T>
inline void epilogue(CborOutputArchive &, NameValuePair<T> const &)
{
}

template <class T>
inline void epilogue(CborInputArchive &, NameValuePair<T> const &)
{
}

template <class T>
inline void prologue(CborOutputArchive &, DeferredData<T> const &)
{
}

template <class T>
inline void prologue(CborInputArchive &, DeferredData<T> const &)
{
}

template <class T>
inline void epilogue(CborOutputArchive &, DeferredData<T> const &)
{
}

template <class T>
inline void epilogue(CborInputArchive &, DeferredData<T> const &)
{
}

template <class T>
inline void prologue(CborOutputArchive &ar, SizeTag<T> const &)
{
    ar.makeArray();
}

template <class T>
inline void prologue(CborInputArchive &, SizeTag<T> const &)
{
}

template <class T>
inline void epilogue(CborOutputArchive &, SizeTag<T> const &)
{
}

template <class T>
inline void epilogue(CborInputArchive &, SizeTag<T> const &)
{
}

template <class T,
          traits::EnableIf<!std::is_arithmetic<T>::value,
                           !traits::has_minimal_base_class_serialization<
                               T, traits::has_minimal_output_serialization,
                               CborOutputArchive>::value,
                           !traits::has_minimal_output_serialization<
                               T, CborOutputArchive>::value> = traits::sfinae>
inline void prologue(CborOutputArchive &ar, T const &)
{
    ar.startNode();
}

template <class T,
          traits::EnableIf<!std::is_arithmetic<T>::value,
                           !traits::has_minimal_base_class_serialization<
                               T, traits::has_minimal_input_serialization,
                               CborInputArchive>::value,
                           !traits::has_minimal_input_serialization<
                               T, CborInputArchive>::value> = traits::sfinae>
inline void prologue(CborInputArchive &ar, T const &)
{
    ar.startNode();
}

template <class T,
          traits::EnableIf<!std::is_arithmetic<T>::value,
                           !traits::has_minimal_base_class_serialization<
                               T, traits::has_minimal_output_serialization,
                               CborOutputArchive>::value,
                           !traits::has_minimal_output_serialization<
                               T, CborOutputArchive>::value> = traits::sfinae>
inline void epilogue(CborOutputArchive &ar, T const &)
{
    ar.finishNode();
}

template <class T,
          traits::EnableIf<!std::is_arithmetic<T>::value,
                           !traits::has_minimal_base_class_serialization<
                               T, traits::has_minimal_input_serialization,
                               CborInputArchive>::value,
                           !traits::has_minimal_input_serialization<
                               T, CborInputArchive>::value> = traits::sfinae>
inline void epilogue(CborInputArchive &ar, T const &)
{
    ar.finishNode();
}

inline void prologue(CborOutputArchive &ar, std::nullptr_t const &)
{
    ar.writeName();
}

inline void prologue(CborInputArchive &, std::nullptr_t const &)
{
}

inline void epilogue(CborOutputArchive &, std::nullptr_t const &)
{
}

inline void epilogue(CborInputArchive &, std::nullptr_t const &)
{
}

template <class T,
          traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void prologue(CborOutputArchive &ar, T const &)
{
    ar.writeName();
}

template <class T,
          traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void prologue(CborInputArchive &, T const &)
{
}

template <class T,
          traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void epilogue(CborOutputArchive &, T const &)
{
}

template <class T,
          traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void epilogue(CborInputArchive &, T const &)
{
}

template <class CharT, class Traits, class Alloc>
inline void prologue(CborOutputArchive &ar,
                     std::basic_string<CharT, Traits, Alloc> const &)
{
    ar.writeName();
}

template <class CharT, class Traits, class Alloc>
inline void prologue(CborInputArchive &,
                     std::basic_string<CharT, Traits, Alloc> const &)
{
}

template <class CharT, class Traits, class Alloc>
inline void epilogue(CborOutputArchive &,
                     std::basic_string<CharT, Traits, Alloc> const &)
{
}

template <class CharT, class Traits, class Alloc>
inline void epilogue(CborInputArchive &,
                     std::basic_string<CharT, Traits, Alloc> const &)
{
}

// ============================================================================
// Prologue / Epilogue for BinaryData
// These override the generic non-arithmetic catch-all so that BinaryData is
// treated as a leaf value (byte string), not as a map/array node.
// ============================================================================
template <class T>
inline void prologue(CborOutputArchive &ar, BinaryData<T> const &)
{
    ar.writeName();
}

template <class T>
inline void prologue(CborInputArchive &, BinaryData<T> const &)
{
}

template <class T>
inline void epilogue(CborOutputArchive &, BinaryData<T> const &)
{
}

template <class T>
inline void epilogue(CborInputArchive &, BinaryData<T> const &)
{
}


// ============================================================================
// Cereal Serialization functions
// ============================================================================
template <class T>
inline void CEREAL_SAVE_FUNCTION_NAME(CborOutputArchive &ar,
                                      NameValuePair<T> const &t)
{
    ar.setNextName(t.name);
    ar(t.value);
}

template <class T>
inline void CEREAL_LOAD_FUNCTION_NAME(CborInputArchive &ar, NameValuePair<T> &t)
{
    // Skip completely when ignoring missing keys and the key is absent -
    // leave t.value untouched.  When the user has opted into throwing on
    // missing keys, fall through and let search() raise.
    if (ar.shouldIgnoreMissingKeys() && !ar.hasName(t.name))
        return;
    ar.setNextName(t.name);
    ar(t.value);
}

inline void CEREAL_SAVE_FUNCTION_NAME(CborOutputArchive &ar, std::nullptr_t const &t)
{
    ar.saveValue(t);
}

inline void CEREAL_LOAD_FUNCTION_NAME(CborInputArchive &ar, std::nullptr_t &t)
{
    ar.loadValue(t);
}

//! Save raw binary data as a CBOR byte string.
template <class T>
inline void CEREAL_SAVE_FUNCTION_NAME(CborOutputArchive &ar, BinaryData<T> const &bd)
{
    ar.saveBinaryValue(bd.data, static_cast<size_t>(bd.size));
}

//! Load raw binary data from a CBOR byte string.
template <class T>
inline void CEREAL_LOAD_FUNCTION_NAME(CborInputArchive &ar, BinaryData<T> &bd)
{
    ar.loadBinaryValue(bd.data, static_cast<size_t>(bd.size));
}

template <class T,
          traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void CEREAL_SAVE_FUNCTION_NAME(CborOutputArchive &ar, T const &t)
{
    ar.saveValue(t);
}

template <class T,
          traits::EnableIf<std::is_arithmetic<T>::value> = traits::sfinae>
inline void CEREAL_LOAD_FUNCTION_NAME(CborInputArchive &ar, T &t)
{
    ar.loadValue(t);
}

template <class CharT, class Traits, class Alloc>
inline void
CEREAL_SAVE_FUNCTION_NAME(CborOutputArchive &ar,
                          std::basic_string<CharT, Traits, Alloc> const &str)
{
    ar.saveValue(str);
}

template <class CharT, class Traits, class Alloc>
inline void
CEREAL_LOAD_FUNCTION_NAME(CborInputArchive &ar,
                          std::basic_string<CharT, Traits, Alloc> &str)
{
    ar.loadValue(str);
}





template <class T>
inline void CEREAL_SAVE_FUNCTION_NAME(CborOutputArchive &ar, SizeTag<T> const &st)
{
    if constexpr (traits::is_output_serializable<BinaryData<T>, CborOutputArchive>::value) {
        ar.writeName();
        ar.saveValue(static_cast<uint64_t>(st.size));
    }
}

template <class T>
inline void CEREAL_LOAD_FUNCTION_NAME(CborInputArchive &ar, SizeTag<T> &st)
{
    if constexpr (traits::is_input_serializable<BinaryData<T>, CborInputArchive>::value) {
        uint64_t size;
        ar.loadValue(size);
        st.size = static_cast<size_type>(size);
    } else {
        ar.loadSize(st.size);
    }
}

} // namespace cereal

CEREAL_REGISTER_ARCHIVE(cereal::CborInputArchive)
CEREAL_REGISTER_ARCHIVE(cereal::CborOutputArchive)
CEREAL_SETUP_ARCHIVE_TRAITS(cereal::CborInputArchive, cereal::CborOutputArchive)
#endif // CEREAL_ARCHIVES_CBOR_HPP_
