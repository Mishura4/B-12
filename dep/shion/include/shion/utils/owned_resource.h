#ifndef SHION_OWNED_RESOURCE_H_
#define SHION_OWNED_RESOURCE_H_

#include <concepts>
#include <type_traits>
#include <iostream>

#include "observer_ptr.h"

namespace shion::utils
{

  template <typename T, typename U>
  concept resource_releaser = requires(T t, U &u) { t(u);};

  template <typename T, auto releaser>
  requires(std::invocable<std::remove_cvref_t<decltype(releaser)>, T &>)
  struct owned_resource
  {
    owned_resource() = default;
    owned_resource(const owned_resource &) = delete;
    explicit owned_resource(T &&resource) :
      resource{std::forward<T>(resource)}
    {

    }

    owned_resource(owned_resource &&rhs) : resource{std::move(rhs.resource)}, owned{true}
    {
      rhs.owned = false;
    }

    ~owned_resource()
    {
      if (hasResource())
        releaser(resource);
    }

    T &operator*() const
    {
      return (resource);
    }

    T &operator->() const
    {
      return (resource);
    }

    T &get() const
    {
      return (resource);
    }

    owned_resource &operator=(const owned_resource &) = delete;

    owned_resource &operator=(owned_resource &&rhs)
    {
      if (hasResource())
        releaser(resource);
      if (rhs.hasResource())
      {
        rhs.owned = false;
        resource = std::move(rhs.resource);
        owned = true;
      }
      else
        owned = false;
    }

    bool hasResource() const { return (owned); }

    void release()
    {
      assert(hasResource());
      releaser(resource);
      owned = false;
    }

    T resource{};
    bool owned{false};
  };

  template <typename T, auto releaser>
  requires(std::invocable<std::remove_cvref_t<decltype(releaser)>, T *&>)
  struct owned_resource<T *, releaser>
  {
    owned_resource() = default;
    owned_resource(const owned_resource &) = delete;
    owned_resource(T *resource) :
      resource{resource}
    {

    }

    owned_resource(owned_resource &&rhs) : resource{rhs.resource} { rhs.resource = nullptr; }

    ~owned_resource()
    {
      if (hasResource())
        releaser(resource);
    }

    T &operator*() const
    {
      return (*resource);
    }

    T &operator->() const
    {
      return (*resource);
    }

    T *get() const
    {
      return (resource);
    }

    T &operator[](size_t i) const
    {
      return (resource[i]);
    }

    owned_resource &operator=(const owned_resource &) = delete;

    owned_resource &operator=(owned_resource &&rhs)
    {
      if (hasResource())
        releaser(resource);
      resource = rhs.resource;
      rhs.resource = nullptr;
      return (*this);
    }

    owned_resource &operator=(T *rhs)
    {
      if (hasResource())
        releaser(resource);
      resource = rhs;
      return (*this);
    }

    bool hasResource() const { return (resource != nullptr); }

    auto release()
    {
      assert(hasResource());
      if constexpr (requires (T *t) {{releaser(t)} -> std::same_as<void>;})
        releaser(resource);
      else
        return (releaser(resource));
    }

    T *resource{};
  };

  inline constexpr auto close_stdfile = [](std::FILE *&f){fclose(f);};

  using owned_stdfile = owned_resource <std::FILE *, close_stdfile> ;
}
#endif
