#include <coroutine>
#include <optional>
#include <utility>
#include <memory>

namespace detail {
	template <typename ReturnType>
	struct co_task_promise;
}

/**
	* @brief Basic implementation of a coroutine. It can be co_awaited to make nested coroutines, waiting on a set of coroutines.
	* Meant to be used in conjunction with coroutine events (@see event_router::co_attach), or on its own, in which case the user is responsible for keeping the lifetime of the co_task object until it ends
	*/
template <typename ReturnType>
struct co_task {
	/**
		* @brief Coroutine handle type.
		* The actual promise type is meant to be opaque and inaccessible
		*/
	using handle_type = std::coroutine_handle<detail::co_task_promise<ReturnType>>;

	co_task() = default;
	co_task(const co_task &) = delete;
	co_task(co_task &&o) : handle(std::exchange(o.handle, nullptr)) {}

	~co_task()
	{
		if (handle) {
			if (handle.done())
				handle.destroy();
			else
				handle.promise().self_destruct = true;
		}
	}

	explicit co_task(handle_type handle_) : handle(handle_) {}

	co_task &operator=(const co_task &) = delete;

	co_task &operator=(co_task &&o) noexcept {
		handle = std::exchange(o.handle, nullptr);
		return (*this);
	}

	/**
	* @brief Coroutine handle
	* @see handle_type
	*/
	handle_type handle;

	/**
		* @brief First step when the object is co_await-ed. The return type signifies whether we need to suspend the caller or not
		* 
		* @return bool Whether the coroutine is done or not
		*/
	bool await_ready() {
		return handle.done();
	}

	/**
		* @brief Second step when the object is co_await-ed. We store the suspended coroutine in the promise to resume when we're done
		*/
	void await_suspend(std::coroutine_handle<> parent) {
		if (handle.promise().is_sync) {
			std::cout << "sync : resume parent" << std::endl;
			parent.resume();
		}
		else
		{
			std::cout << "not sync : new parent" << std::endl;
			handle.promise().parent = parent;
			handle.promise().is_sync = false;
		}
	}

	ReturnType await_resume();

	bool done() const noexcept {
		return handle.done();
	}
};

namespace detail {
	/**
		* @brief Base implementation of promise_type, without return type that would depend on R. Not meant to use directly.
		* 
		* @see promise
		*/
	template <typename ReturnType>
	struct co_task_promise_base {
		struct chain_final_awaiter {
			bool await_ready() noexcept {
				return (false);
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<co_task_promise<ReturnType>> handle) noexcept;

			void await_resume() noexcept {}
		};

		co_task_promise_base()
		{
			++dpp::num_coros;
			std::cout << "create " << dpp::num_coros << std::hex << " 0x" << this << std::endl;
		}

		~co_task_promise_base()
		{
			std::cout << "destroy " << dpp::num_coros << std::hex << " 0x" << this << std::endl;
			--dpp::num_coros;
		}

		std::coroutine_handle<> parent = nullptr;

		std::exception_ptr exception = nullptr;

		bool is_sync = true;

		bool self_destruct = false;

		/**
			* @brief Construct a new promise object
			*/
		//co_task_promise_base() = default;

		/**
			* @brief Function called when the coroutine is created.
			*
			* @return std::suspend_never Don't suspend, we want the start to execute immediately on the caller.
			*/
		std::suspend_never initial_suspend() noexcept {
			return {};
		}

		/**
			* @brief Function called when the coroutine reaches its last suspension point
			* 
			* @return std::suspend_never Never suspend this coroutine at the final suspend point
			*/
		chain_final_awaiter final_suspend() noexcept {
			return {};
		}

		void unhandled_exception() {
			exception = std::current_exception();
		}
	};

	/**
		* @brief Implementation of promise_base for non-void return type
		*/
	template <typename ReturnType>
	struct co_task_promise : co_task_promise_base<ReturnType> {
		std::optional<ReturnType> value = std::nullopt;
			
		/**
			* @brief Function called when the coroutine returns a value
			*/
		void return_value(ReturnType expr) {
			value = std::forward<ReturnType>(expr);
		}

		/**
			* @brief Get the return object
			*
			* @return coroutine dpp::coroutine type
			*/
		co_task<ReturnType> get_return_object() {
			return co_task{std::coroutine_handle<detail::co_task_promise<ReturnType>>::from_promise(*this)};
		}
	};

	template <>
	struct co_task_promise<void> : co_task_promise_base<void> {
		/**
			* @brief Function called when the coroutine returns a value
			*/
		void return_void() {
		}

		/**
			* @brief Get the return object
			*
			* @return coroutine dpp::coroutine type
			*/
		co_task<void> get_return_object() {
			return co_task{std::coroutine_handle<detail::co_task_promise<void>>::from_promise(*this)};
		}
	};

	template <typename R>
	std::coroutine_handle<> co_task_promise_base<R>::chain_final_awaiter::await_suspend(std::coroutine_handle<co_task_promise<R>> handle) noexcept {
		co_task_promise<R> &promise = handle.promise();
		std::coroutine_handle<> next_coroutine = promise.parent ? promise.parent : std::noop_coroutine();

		if (promise.self_destruct)
			handle.destroy();
		return next_coroutine;
	}
}

template <typename R>
R co_task<R>::await_resume() {
	if (handle.promise().exception)
		std::rethrow_exception(handle.promise().exception);
	if constexpr (!std::is_same_v<R, void>)
		return std::forward<R>(*std::exchange(handle.promise().value, std::nullopt));
}

template <typename R>
template <typename T>
bool dpp::awaitable<R>::await_suspend(std::coroutine_handle<::detail::co_task_promise<T>> handle) {
	l_guard lock{data->mutex};
	
				std::cout << "d" << std::endl;
	if (data->result.has_value())
	{
		return false; // immediately resume the coroutine as we already have the result of the api call
	}
	handle.promise().is_sync = false;
	data->coro_handle = handle;
	return true; // need to wait for the callback to resume
}

template<typename T, typename... Args>
struct std::coroutine_traits<::co_task<T>, Args...> {
	using promise_type = ::detail::co_task_promise<T>;
}; 