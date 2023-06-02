#ifndef B12_API_CACHE_H_
#define B12_API_CACHE_H_

#include <shion/shion.h>

#include "B12.h"
#include "API.h"

#include "CachedResource.h"

namespace B12
{
	template <typename Endpoint>
	struct ResourceCache;

	template <template<typename, bool> typename EndpointT, shion::basic_string_literal Name, typename ID, bool CanUseName>
	  requires (std::constructible_from<size_t, ID> && sizeof(ID) <= 2)
	struct ResourceCache<EndpointT<APIResource<Name, ID>, CanUseName>>
	{
		using Endpoint = EndpointT<APIResource<Name, ID>, CanUseName>;
		using Resource = APIResource<Name, ID>;
		
		using resource_type = std::shared_ptr<Resource>;
		using future_type = std::shared_future<resource_type>;

		ResourceCache(size_t max_size) :
			_max_entries(static_cast<ID>(std::min(max_size, size_t{std::numeric_limits<ID>::max()}))),
			_storage(max_size)
		{
			if constexpr (CanUseName)
				_resolver.reserve(max_size);
		}

		ResourceCache(dpp::cluster *cluster) :
			ResourceCache{_fetchCount(cluster)}
		{
		}
		
		struct CachedResource
		{
				resource_type resource;
				future_type   future;
				app_time      last_touched;
				file_time     time_retrieved;
		};

		struct ResourceAccessor
		{
			auto get() -> const std::shared_ptr<Resource> &
			{
				if (resource)
					return (resource);
				if (future.valid())
				{
					resource = future.get();
					return (resource);
				}
				return (resource);
			}

			operator bool() const
			{
				return (static_cast<bool>(resource));
			}

			const Resource *operator->()
			{
				return (&get());
			}

			~ResourceAccessor()
			{
				const auto &resource = get();

				if (resource)
					cache._touch(resource->id);
				
			}
			
			ResourceCache&		 cache;
			resource_type			 resource;
			future_type				 future;
		};
		
		using Storage = std::vector<CachedResource>;
		
		bool isKeyValid(ID value) const
		{
			if (!(value < _max_entries))
				return (false);
			return (true);
		}
		
		auto request(dpp::cluster *cluster, ID id) -> ResourceAccessor
		{
			if (!isKeyValid(id)) // for now, until i come up with a good solution
				return {*this, nullptr, {}};
			
			std::scoped_lock lock{_mutex};
			auto fstime_now = std::chrono::file_clock::now();
			CachedResource *entry = _fetch(id);

			if (entry)
			{
				if (entry->resource && fstime_now - entry->time_retrieved < std::chrono::weeks{1})
					return (make_accessor(*entry));
				if (entry->future.valid())
					return (make_accessor(*entry));
			}

			// otherwise, first try to get saved files
			namespace fs = std::filesystem;
			
			fs::path cache_path = shion::literal_concat(Endpoint::API_t::NAME, "/", Name).data;
			std::error_code err;

			cache_path /= fmt::format("{}.json", id);
			cache_path = cache_path.lexically_normal();
			if (auto time = fs::last_write_time(cache_path, err);
					err == std::error_code{} && fstime_now - time < std::chrono::weeks{1})
			{
				if (std::ifstream fs{cache_path}; fs.good())
				{
					entry->resource = std::make_shared<Resource>(Resource{.resource = json::parse(fs)});
					entry->time_retrieved = fstime_now;
					return (make_accessor(*entry));
				}
			}
			auto promise = new std::promise<std::shared_ptr<Resource>>{};
			entry->future = promise->get_future().share();
			cluster->request(Endpoint::url(id), dpp::m_get, OnRecv{std::move(promise), id, this});
			return (make_accessor(*entry));
		}

	private:
		struct OnRecv
		{
			std::promise<std::shared_ptr<Resource>> *p;
			std::optional<ID> id;
			ResourceCache *self;

			void operator()(const dpp::http_request_completion_t &result)
			{
				std::scoped_lock lock{self->_mutex};
				
				if (result.error || result.status >= 300)
				{
					static_assert(!std::is_const_v<decltype(p)>);
					p->set_value(nullptr);
					delete p;
					return;
				}
				auto json = json::parse(result.body);
				
				if (!id.has_value())
					id = resource_id<Resource>(json);

				std::shared_ptr<Resource> ptr = std::make_shared<Resource>(Resource{.resource = std::move(json), .id = id.value_or(std::numeric_limits<ID>::max())});
				
				if (id.has_value())
				{
					std::error_code err;
					std::filesystem::path file_path{Endpoint::resource(*id)};
				
					if (auto parent_path = file_path.parent_path();
							create_directories(parent_path, err) || err == std::error_code{})
					{
						std::ofstream fs{file_path, std::ios::out | std::ios::trunc};

						if (fs.good())
							fs << json.dump();
						else
							B12::log(LogLevel::ERROR, "Failed to save API resource {}:{} to disk", Endpoint::PATH.data, id.value_or(0));
					}
					else
						B12::log(LogLevel::ERROR, "Failed to create directories for path {}", parent_path.string());
				}
				
				if (id.has_value())
					self->_retrieve(*id) = CachedResource{ptr, {}, std::chrono::steady_clock::now(), std::chrono::file_clock::now()};
				p->set_value(ptr);
				delete p;
			}
		};

		auto _fetchCount(dpp::cluster *cluster) -> ID
		{

			auto future = Endpoint::retrieve_json(cluster);
			auto value = future.get();
			if (std::holds_alternative<json>(value))
			{
				const json &json_value = std::get<json>(value);
				
				if (auto it = json_value.find("count"); it != json_value.end() && it->is_number_integer())
				{
					size_t size = it->get<size_t>();
					
					B12::log(LogLevel::TRACE, "Loaded count for API {} : {} entries", Endpoint::PATH, size);
					if (size > std::numeric_limits<ID>::max())
						log(LogLevel::ERROR, "Fetched count for API {} is larger than this cache is set to hold", size);
					return (static_cast<ID>(size));
				}
				B12::log(LogLevel::TRACE, "Could not find count for API {}", Endpoint::PATH);
			}
			else
				B12::log(LogLevel::TRACE, "Error while fetching count for API {}", Endpoint::PATH);
			return (0);
		}
		
		void _touch(ID id)
		{
			std::scoped_lock lock{_mutex};
			CachedResource *entry = _fetch(id);

			if (!entry)
			{
				B12::log(LogLevel::ERROR, "Could not find entry {}:{} for update", Endpoint::PATH.data, id);
				return;
			}
			entry->last_touched = std::chrono::steady_clock::now();
		}

		void _save(json json)
		{
			
		}

		template <typename... Ts>
		auto make_accessor(CachedResource &resource) -> ResourceAccessor
		{
			resource.last_touched = std::chrono::steady_clock::now();
			return {*this, resource.resource, resource.future};
		}
		
		auto _fetch(ID id) -> CachedResource *
		{
			if (id < _storage.size())
				return (&_storage[id]);
			return (nullptr);
		}

		auto _retrieve(ID id) -> CachedResource &
		{
			if (id < _storage.size())
				return (_storage[id]);
			return (_extras[id]);
		}
		
		ID _max_entries = std::numeric_limits<ID>::max();
		Storage _storage;
		std::unordered_map<ID, CachedResource> _extras;
		
		using NameResolver = std::conditional_t<CanUseName, std::unordered_map<std::string, ID>, empty_t>;
		
		NameResolver _resolver;
		std::mutex _mutex;
	};
	
	template <API_type T, template <typename...> typename... Resources>
	struct APICache
	{
		using API = T;
	};
}

#endif
