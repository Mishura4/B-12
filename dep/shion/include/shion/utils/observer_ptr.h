#ifndef SHION_OBSERVER_PTR_H_
#define SHION_OBSERVER_PTR_H_

#include <type_traits>

namespace shion::utils
{
  template <typename T>
  struct observer_ptr
  {
      static_assert(!std::is_reference_v<T>, "cannot create a pointer-to-reference");

      operator T *() const { return (raw_ptr); }

      T &operator*() const { return (*raw_ptr); }

      T *operator->() const { return (raw_ptr); }

      T *raw_ptr;

      observer_ptr &operator=(const observer_ptr &) = default;

      observer_ptr &operator=(T *ptr)
      {
        raw_ptr = ptr;
        return (*this);
      }
  };

  template <typename T>
  struct observer_ptr<const T>
  {
      static_assert(!std::is_reference_v<T>, "cannot create a pointer-to-reference");

      operator const T *() const { return (raw_ptr); }

      const T &operator*() const { return (*raw_ptr); }

      const T *operator->() const { return (raw_ptr); }

      observer_ptr &operator=(const observer_ptr &) = default;

      observer_ptr &operator=(const observer_ptr<T> &mutable_ptr)
      {
        raw_ptr = mutable_ptr.raw_ptr;
        return (*this);
      }

      observer_ptr &operator=(const T *ptr)
      {
        raw_ptr = ptr;
        return (*this);
      }

      const T *raw_ptr;
  };
}  // namespace shion::utils

#endif
