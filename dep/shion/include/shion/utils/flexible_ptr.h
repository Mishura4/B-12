#ifndef SHION_FLEXIBLE_PTR_H_
#define SHION_FLEXIBLE_PTR_H_

#include <memory>
#include <variant>
#include <concepts>

namespace shion {

template <typename T, typename Deleter = std::default_delete<T>>
class flexible_ptr_base {
public:
	using pointer = std::decay_t<T>;

private:
	using owning_ptr = std::unique_ptr<T, Deleter>;
	using observer_ptr = std::decay_t<T>;
	using null_ptr = std::nullptr_t;
	using variant_ptr = std::variant<null_ptr, observer_ptr, owning_ptr>;

	struct getter {
		pointer operator()(null_ptr) const noexcept {
			return (nullptr);
		}

		pointer operator()(observer_ptr p) const noexcept {
			return (p);
		}

		pointer operator()(const owning_ptr &p) const noexcept {
			return (p.get());
		}
	};

public:
	/**
		* @brief Default constructor, initializes pointer to nullptr.
		*/
	flexible_ptr_base() noexcept : flexible_ptr_base{nullptr} {}

	/**
		* @brief Nullptr constructor.
		*/
	flexible_ptr_base(std::nullptr_t) noexcept : ptr{std::in_place_type<std::nullptr_t>, nullptr} {}

	/**
		* @brief Observer constructor. After using this constructor, the pointer points to p and is non-owning
		*
		* @param p Pointer representing the object to point to
		*/
	flexible_ptr_base(T* p) noexcept : ptr{std::in_place_type<observer_ptr>, p} {}

	/**
		* @brief Moved unique ptr constructor. Grabs ownership of the object pointed to
		*
		* @param p Owning pointer to grab ownership from
		*/
	flexible_ptr_base(std::unique_ptr<T, Deleter>&& p) noexcept : ptr{std::in_place_type<owning_ptr>, std::move(p)} {}

	/**
		* @brief This object is non-copyable.
		*/
	flexible_ptr_base(flexible_ptr_base const&) = delete;

	/**
		* @brief Move constructor, moves the pointer to this object, including ownership status.
		*/
	flexible_ptr_base(flexible_ptr_base&&) = default;

	/**
		* @brief This object is non-copyable.
		*/
	flexible_ptr_base& operator=(flexible_ptr_base const&) = delete;

	/**
		* @brief Move asignment operator, moves the pointer to this object, including ownership status.
		*/
	flexible_ptr_base& operator=(flexible_ptr_base&&) = default;

	/**
		* @brief Nullptr assignment operator, equivalent to reset()
		*/
	flexible_ptr_base& operator=(std::nullptr_t) noexcept {
		reset();
		return *this;
	}

	/**
		* @brief Release the object pointed to.
		*
		* - If owning, set to non-owning, keep the pointer and return it
		* - If non-owning, equivalent to reset() and returns nullptr
		* - If null, do nothing and return nullptr.
		*/
	pointer release() noexcept {
		if (std::holds_alternative<owning_ptr>(ptr)) {
			T* p = std::get<owning_ptr>(ptr).release();
			point(p);
			return (p);
		} else if (std::holds_alternative<observer_ptr>(ptr)) {
			reset();
			return (nullptr);
		} /* else nullptr_t */
		return (nullptr);
	}

	/**
		* @brief Set to nullptr.
		*/
	void reset() noexcept {
		ptr.emplace<std::nullptr_t>(nullptr);
	}

	/**
		* @brief Set the pointer to p in a non-owning fashion.
		*/
	template <typename U>
	requires (std::convertible_to<U, observer_ptr>)
	void point(U p) noexcept {
		ptr.emplace<observer_ptr>(p);
	}

	/**
		* @brief Grab ownership the resource represented by p.
		*/
	template <typename U>
	requires (std::convertible_to<U, owning_ptr>)
	void grab(U&& p) noexcept {
		ptr.emplace<owning_ptr>(std::forward<U>(p));
	}

	/**
		* @brief Get the underlying pointer value.
		*/
	pointer get() const {
		return (std::visit(getter{}, ptr));
	}

	/**
		* @brief Access the object pointed to.
		*
		* @warn The behavior is undefined if get() == nullptr.
		*/
	pointer operator->() const noexcept {
		return (get());
	}

	T& operator*() & {
		return (*get());
	}

	T&& operator*() && {
		return (static_cast<T&&>(*get()));
	}

	T const& operator*() const& {
		return (*get());
	}

	T const&& operator*() const&& {
		return (static_cast<T const&&>(*get()));
	}

	bool operator<=>(T* p) const noexcept {
		return (get() <=> p);
	}

	bool operator==(T* p) const noexcept {
		return (get() == p);
	}

	bool is_owning() const noexcept {
		return (std::holds_alternative<owning_ptr>(ptr));
	}

	bool is_null() const noexcept {
		return (get() == nullptr);
	}

	explicit operator bool() const noexcept {
		return (is_null());
	}

private:
	variant_ptr ptr = {std::in_place_type<std::nullptr_t>, nullptr};
};

template <typename T>
flexible_ptr_base(T*) -> flexible_ptr_base<T*>;

template <typename T, typename Deleter>
flexible_ptr_base(std::unique_ptr<T, Deleter>) -> flexible_ptr_base<T, Deleter>;

template <typename T, typename Deleter = std::default_delete<T>>
using flexible_ptr = flexible_ptr_base<T, Deleter>;

}

#endif