#ifndef SHION_CONSTEVAL_MAP_H
#define SHION_CONSTEVAL_MAP_H

#include <utility>
#include <algorithm>

namespace shion
{

  template <typename T, typename U>
  struct consteval_map_entry
  {
    using key_type = T;
    using value_type = U;

    constexpr consteval_map_entry(std::tuple<T, U> &&args) :
      key{std::get<0>(args)}, value{std::get<1>(args)}
    {

    }
    constexpr consteval_map_entry(T key_, U value_) noexcept(noexcept(T(key_)) && noexcept(U(U(value_)))) :
      key{key_}, value{value_}
    {
    }

    T key;
    U value;
  };

  template <typename T>
  struct entry_helper;

  template <typename T, typename U>
  struct entry_helper<std::tuple<T, U>>
  {
    using key_type = T;
    using value_type = U;
    using entry_type = consteval_map_entry<T, U>;
  };

  template <typename... EntryTypes>
  struct consteval_map
  {
    using entries_type = std::tuple<consteval_map_entry<typename entry_helper<EntryTypes>::key_type, typename entry_helper<EntryTypes>::value_type>...>;

    consteval consteval_map(EntryTypes&&... entries_) :
      entries{typename entry_helper<EntryTypes>::entry_type(std::get<0>(entries_), std::get<1>(entries_))...}
    {
    }

    template <const consteval_map &map, auto key>
    static consteval auto &lookup()
    {
      using key_type = decltype(key);

      return (_lookup<map, 0, key>());
    }

    template <auto key>
    constexpr auto &get()
    {
      using key_type = decltype(key);

      return (_lookup_nonstatic<0, key>());
    }

    entries_type entries;

  private:
    template <const consteval_map &map, size_t N, auto key>
    static consteval auto &_lookup()
    {
      static_assert(N <= std::tuple_size_v<entries_type>, "key not found");

      if constexpr (std::is_same_v<decltype(key), decltype(std::get<N>(map.entries).key)>)
      {
        if constexpr (std::get<N>(map.entries).key == key)
          return (std::get<N>(map.entries));
        else
          return (_lookup<map, N + 1, key>());
      }
      else
        return (_lookup<map, N + 1, key>());
    }

    template <size_t N, auto key>
    consteval auto &_lookup_nonstatic()
    {
      static_assert(N <= std::tuple_size_v<entries_type>, "key not found");

      if constexpr (std::is_same_v<decltype(key), decltype(std::get<N>(entries).key)>)
      {
        if constexpr (std::get<N>(entries).key == key)
          return (std::get<N>(entries));
        else
          return (_lookup<N + 1, key>());
      }
      else
        return (_lookup<N + 1, key>());
    }
  };


  template <auto &map, auto key>
  consteval auto &map_get()
  {
    using map_type = std::remove_reference_t<decltype(map)>;

    return (map_type::template lookup<map, key>());
  }
}

#endif
