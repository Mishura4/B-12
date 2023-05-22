#ifndef B12_API_H_
#define B12_API_H_

#include "B12.h"

#include <concepts>

#include "CachedResource.h"

namespace shion
{
	template <typename CharT, size_t N>
	constexpr bool ends_with(shion::basic_string_literal<CharT, N> literal, auto token)
	requires (std::equality_comparable_with<decltype(token), CharT>)
	{
		return (literal.data[literal.size - 1] == token);
	}
}

namespace B12
{
	template <string_literal APIName, string_literal URL_>
		requires (!shion::ends_with(APIName, '/') && !shion::ends_with(APIName, '\\'))
	struct API
	{
		static constexpr inline auto NAME = APIName;
		static constexpr inline auto URL = URL_;

		template <typename T, bool CanUseName>
		struct Endpoint;
		
		template <string_literal Name, typename ID, bool CanUseName>
			requires (!shion::ends_with(Name, '/') && !shion::ends_with(Name, '\\'))
		struct Endpoint<APIResource<Name, ID>, CanUseName>
		{
			static constexpr inline auto NAME = Name;
			static constexpr inline auto PATH = shion::literal_concat(API::NAME, "/", NAME);
			using API = API;

			using Resource = APIResource<Name, ID>;
			
			static constexpr inline auto BASE_URL = 
				[]() consteval
				{
					if constexpr (shion::ends_with(URL_, '/'))
						return (shion::literal_concat(URL_, Name));
					else
						return (shion::literal_concat(URL_, "/", Name));
				}();
			
			static auto retrieve_json(dpp::cluster *cluster) -> std::future<std::variant<json, dpp::error_info>>
			{
				namespace fs = std::filesystem;
			
				fs::path cache_path = shion::literal_concat(API::NAME, "/", Name, ".json").data;
				cache_path = cache_path.lexically_normal();
				return (retrieve_json(cluster, url(), cache_path));
			}

			static auto retrieve_json(
				dpp::cluster* cluster,
				ID            key
			) -> std::future<std::variant<json, dpp::error_info>>
			{
				namespace fs = std::filesystem;
			
				fs::path cache_path = resource(key);
				cache_path = cache_path.lexically_normal();
				return (retrieve_json(cluster, url(key), cache_path));
			}
			
			static auto retrieve_json(
				dpp::cluster*    cluster,
				std::string_view key
			) -> std::future<std::variant<json, dpp::error_info>>
			requires (CanUseName)
			{
				namespace fs = std::filesystem;
			
				fs::path cache_path = resource(key);
				cache_path = cache_path.lexically_normal();
				return (retrieve_json(cluster, url(key), cache_path));
			}

			static auto retrieve_json(
				dpp::cluster*         cluster,
				std::string_view      url,
				std::filesystem::path file_path
			) -> std::future<std::variant<json, dpp::error_info>>
			{
				auto p = std::make_unique<std::promise<std::variant<json, dpp::error_info>>>();
				std::error_code err;
				auto fstime_now = std::chrono::file_clock::now();
			
				if (auto time = last_write_time(file_path, err);
						err == std::error_code{} && fstime_now - time < std::chrono::weeks{1})
				{
					if (std::ifstream fs{file_path}; fs.good())
					{
						B12::log(
							LogLevel::TRACE,
							"Resource {} found on disk, from {} days ago",
							file_path.string(),
							std::chrono::floor<std::chrono::days>(fstime_now - time).count()
						);
						
						p->set_value({json::parse(fs)});
						return (p->get_future());
					}
				}
				struct OnRecv
				{
					using resource_type = std::variant<json, dpp::error_info>;
					using promise_type = std::promise<resource_type>;
							
					std::filesystem::path file_path;
					promise_type *p;

					void operator()(const dpp::http_request_completion_t &result)
					{
						auto promise = std::unique_ptr<promise_type>{p};
						
						if (result.error)
						{
							promise->set_value(resource_type{std::in_place_type<dpp::error_info>, result.error, std::string{}});
							return;
						}
						if (result.status >= 300)
						{
							promise->set_value(resource_type{std::in_place_type<dpp::error_info>, result.status, result.body});
							return;
						}
						std::error_code err;

						if (auto parent_path = file_path.parent_path();
								create_directories(parent_path, err) || err == std::error_code{})
						{
							std::ofstream fs{file_path, std::ios::out | std::ios::trunc};

							if (fs.good())
								fs << result.body;
							else
								B12::log(LogLevel::ERROR, "Failed to save API resource {}", file_path.string());
						}
						else
							B12::log(LogLevel::ERROR, "Failed to create directories for path {}", parent_path.string());
						promise->set_value(nlohmann::json::parse(result.body));
					}
				};
				auto future = p->get_future();
				
				cluster->request(std::string{url}, dpp::m_get, OnRecv{file_path, p.release()});
				return {std::move(future)};
			}
			
			static std::string url(ID identifier)
			{
				if constexpr (!shion::ends_with(BASE_URL, '/'))
					return (fmt::format("{}/{}", BASE_URL.data, identifier));
				else
					return (fmt::format("{}{}", BASE_URL.data, identifier));
			}
			
			static std::string url(std::string_view identifier)
				requires(CanUseName)
			{
				return (fmt::format("{}/{}", BASE_URL.data, identifier));
			}
			
			static std::string url()
			{
				return {BASE_URL.data};
			}
			
			static std::string resource(ID identifier)
			{
				return (fmt::format("{}/{}", Name.data, identifier));
			}
			
			static std::string resource(std::string_view identifier)
				requires (CanUseName)
			{
				return (fmt::format("{}/{}", Name.data, identifier));
			}
		};
	};

	template <typename T>
	concept API_type = requires {T::NAME; T::URL;} && std::is_base_of_v<API<T::NAME, T::URL>, T>;
}

#endif
