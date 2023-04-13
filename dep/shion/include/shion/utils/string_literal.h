#ifndef SHION_STRING_LITERAL_H
#define SHION_STRING_LITERAL_H

#include <utility>
#include <algorithm>

namespace shion
{
  template <typename CharT, size_t N>
  struct string_literal
  {
    constexpr inline static size_t size{N};

  	template <typename T>
  	requires (std::same_as<const CharT *, T>)
  	consteval string_literal(T str)
  	{
  		std::ranges::copy_n(str, N, data);
  		data[N] = 0;
  	}

    template <size_t... Is>
    constexpr string_literal(const CharT *str, std::index_sequence<Is...>) :
      data{str[Is]...}
    {
    }

    constexpr string_literal(const CharT(&arr)[N + 1]) :
      string_literal{arr, std::make_index_sequence<N + 1>()}
    {
    }

    template <typename CharT2, size_t N2>
    constexpr bool operator==(string_literal<CharT2, N2> other) const
    {
      if constexpr (N2 != N)
        return (false);
      else
      {
        for (size_t i = 0; i < N; ++i)
        {
          if (data[i] != other.data[i])
            return (false);
        }
        return (true);
      }
    }

    template <typename CharT2, size_t N2>
    constexpr bool operator<=>(string_literal<CharT2, N2> other) const
    {
      if constexpr (N2 != N)
        return (false);
      else
        return (std::ranges::lexicographical_compare(data, other.data));
    }

    constexpr operator std::string_view() const
    {
      return {data, N};
    }

    constexpr operator std::string() const
    {
      return {data, N};
    }

    char data[N + 1];
  };

  template <typename CharT, std::size_t N>
  string_literal(const CharT(&)[N]) -> string_literal<CharT, N - 1>;

  template <typename T>
  struct is_string_literal_s
  {
    constexpr inline static bool value = false;
  };

  template <template <size_t> typename T, size_t N>
  struct is_string_literal_s<T<N>>
  {
    constexpr inline static bool value = true;
  };

  namespace _
  {
    template <string_literal lhs, string_literal rhs, string_literal ...more>
    struct literal_concat
    {
      consteval static auto insert(auto &src, size_t size, auto &where)
      {
        return (std::copy_n(src, size, where));
      }

      consteval static auto concat()
      {
        constexpr size_t size1 = decltype(lhs)::size;
        constexpr size_t size2 = decltype(rhs)::size;
        constexpr size_t packSize = {(0 + ... + decltype(more)::size)};
        char data[size1 + size2 + packSize + 1];
        auto it{std::begin(data)};

        it = insert(lhs.data, size1, it);
        it = insert(rhs.data, size2, it);
        ((it = insert(more.data, decltype(more)::size, it)), ...);
        data[size1 + size2 + packSize] = 0;
        return (string_literal{data});
      }
    };
  }

  template <string_literal lhs, string_literal rhs, string_literal ...more>
  constexpr auto literal_concat = []() consteval
  {
    return (_::literal_concat<lhs, rhs, more...>::concat());
  };

  namespace literals
  {
    template<string_literal Input>
    consteval auto operator""_sl()
    {
      return Input;
    }
  }
}

#endif
