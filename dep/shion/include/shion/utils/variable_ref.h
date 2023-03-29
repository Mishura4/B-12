#ifndef SHION_VARIABLE_REF_H_
#define SHION_VARIABLE_REF_H_

#include <cassert>

#include "../types.h"
#include "reference.h"

namespace shion
{
  template <typename... PossibleTypes>
    requires (sizeof...(PossibleTypes) > 0)
  struct variable_ref
  {
    using possible_types = shion::type_list<PossibleTypes...>;
    using index_type = shion::uint32;

    template <size_t N>
    using type_at = typename possible_types::template at<N>;

    static constexpr size_t npos = std::numeric_limits<std::size_t>::max();

    template <typename T>
    consteval static index_type index_of() noexcept
    {
      return (static_cast<index_type>(possible_types::template type_index<T>));
    }

    template <typename T>
    consteval static bool has_alternative() noexcept
    {
      return (index_of<T>() != npos);
    }

    template <typename T>
    constexpr bool holds_type() const
    {
      return (index_of<T>() == type_index);
    }

    template <typename T>
      requires (has_alternative<T>())
    constexpr variable_ref(T &value) noexcept :
      ptr{&value},
      type_index{index_of<T>()}
    {
    }

    template <size_t N>
      requires (N < possible_types::size)
    constexpr auto get() const noexcept -> type_at<N> &
    {
      assert(type_index == N);

      return (*static_cast<type_at<N> *>(ptr));
    }

    template <typename T>
      requires (has_alternative<T>())
    constexpr auto get() const noexcept -> T &
    {
      assert(type_index == index_of<T>());

      return (*static_cast<T *>(ptr));
    }

    template <size_t N>
      requires (N < possible_types::size)
    constexpr auto at() const -> type_at<N> &
    {
      if (type_index != N)
        throw std::bad_variant_access();
      return (*static_cast<type_at<N> *>(ptr));
    }

    template <typename T>
      requires (has_alternative<T>())
    constexpr auto at() const -> T &
    {
      if (type_index != index_of<T>())
        throw std::bad_variant_access();
      return (*static_cast<T *>(ptr));
    }

    template <size_t N>
      requires (N < possible_types::size)
    constexpr auto try_get() const noexcept -> type_at<N> *
    {
      if (type_index != N)
        return (nullptr);
      return (static_cast<type_at<N> *>(ptr));
    }

    template <typename T>
      requires (has_alternative<T>())
    constexpr auto try_get() const noexcept -> T *
    {
      if (type_index != index_of<T>())
        return (nullptr);
      return (static_cast<T *>(ptr));
    }

    void *ptr;
    shion::int32 type_index;
  };

  template <typename T>
  struct variable_ref<T> : public reference<T>
  {
    using pcossible_types = shion::type_list<T>;
    using index_type = shion::int32;

    static constexpr size_t npos = std::numeric_limits<std::size_t>::max();

    using reference<T>::reference;
    using reference<T>::operator=;
    using reference<T>::get;

    template <typename U>
    consteval static index_type index_of() noexcept
    {
      if constexpr (std::is_same_v<T, U>)
        return (0);
      else
        return (npos);
    }

    template <typename U>
    consteval bool holds_type() const
    {
      return (std::is_same_v<T, U>);
    }

    template <typename U>
    consteval static bool has_alternative() noexcept
    {
      return (std::is_same_v<T, U>);
    }

    template <size_t N>
      requires (N == 0)
    constexpr auto get() const noexcept -> T &
    {
      return (reference<T>::get());
    }

    template <typename U>
      requires (has_alternative<U>())
    constexpr auto get() const noexcept -> U &
    {
      return (reference<T>::get());
    }

    template <size_t N>
      requires (N == 0)
    constexpr auto at() const noexcept -> T &
    {
      return (reference<T>::get());
    }

    template <typename U>
      requires (has_alternative<U>())
    constexpr auto at() const noexcept -> U &
    {
      return (reference<T>::get());
    }

    template <size_t N>
      requires (N == 0)
    constexpr auto try_get() const noexcept -> T *
    {
      return (&reference<T>::get());
    }

    template <typename U>
      requires (has_alternative<U>())
    constexpr auto try_get() const noexcept -> U *
    {
      return (&reference<T>::get());
    }
  };
}

#endif
