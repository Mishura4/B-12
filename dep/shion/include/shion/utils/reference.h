#ifndef SHION_REFERENCE_H_
#define SHION_REFERENCE_H_

namespace shion
{
  template <typename T>
  struct reference
  {
    constexpr reference(T &var) noexcept :
      ptr(&var)
    {

    }

    constexpr auto &get() const noexcept
    {
      assert(ptr);

      return (*ptr);
    }

    operator T &() const noexcept
    {
      return (get());
    }

    reference &operator=(const T &other) noexcept(std::is_nothrow_copy_assignable_v<T>)
      requires (!std::is_const_v<T> &&std::is_copy_assignable_v<T>)
    {
      assert(ptr);

      *ptr = other;
      return (*this);
    }

    reference &operator=(const reference<T> &other) noexcept(std::is_nothrow_copy_assignable_v<T>)
      requires (!std::is_const_v<T> &&std::is_copy_assignable_v<T>)
    {
      assert(ptr);

      *ptr = other;
      return (*this);
    }

    T *ptr;
  };
}

#endif
