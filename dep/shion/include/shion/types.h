#ifndef SHION_TYPES_H_
#define SHION_TYPES_H_

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <chrono>
#include <type_traits>

namespace shion
{
  namespace types
  {
    using int8 = std::int8_t;
    using int16 = std::int16_t;
    using int32 = std::int32_t;
    using int64 = std::int64_t;
    using uint8 = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;
    using size = std::size_t;

    using std::string_view;

    using timestamp = std::chrono::microseconds;

    using std::chrono::nanoseconds;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;
    using std::chrono::seconds;
  }

  using namespace types;

  constexpr auto noop = [](auto...) constexpr {};

  template <auto R>
  constexpr auto noop_r = [](auto...) constexpr -> decltype(R) { return (R); };

  template <typename T>
  constexpr auto make_empty = [](auto &&...) {return T{}; };

  template <typename T>
  requires(!std::is_enum_v<T>)
  constexpr T bitflag(T operand)
  {
    if (operand == 0)
      return (0);
    if (operand == static_cast<T>(-1))
      return (static_cast<T>(-1));

    T _operand{operand - 1};

    return (T{1} << _operand);
  }

  template<typename T>
  requires(std::is_enum_v<T>)
  constexpr T bitflag(std::underlying_type_t<T> operand)
  {
    if (operand == 0)
      return T{0};
    if (operand == static_cast<T>(-1))
      return (static_cast<T>(-1));

    std::underlying_type_t<T> _operand{operand - 1};

    return T{std::underlying_type_t<T>{1} << _operand};
  }

  template<typename T>
    requires(std::is_enum_v<T>)
  constexpr T &operator|=(T &lhs, const T &rhs)
  {
    lhs = T{std::to_underlying(lhs) | std::to_underlying(rhs)};
    return (lhs);
  }

  namespace literals
  {
  // work-around for https://developercommunity.visualstudio.com/t/warning-c4455-issued-when-using-standardized-liter/270349
    using namespace std::string_view_literals;
    using namespace std::chrono_literals;
  }

  using namespace literals;

  //using ssize = std::ssize_t;
}

#endif