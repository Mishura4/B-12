#ifndef B12_DATA_BASE_H_
#define B12_DATA_BASE_H_

#include "B12.h"

#include <filesystem>
#include <span>
#include <shion/utils/observer_ptr.h>
#include <shion/utils/owned_resource.h>

#include "DatabaseStatement.h"

extern "C"
{
	struct sqlite3;

	int sqlite3_close(sqlite3*);
}

namespace B12
{
	template <typename T>
	concept query_callback_type =
		requires(T&& t, std::span<char*> data, std::span<char*> columns)
		{
			{ std::invoke<int>(t, data, columns) };
		};

	namespace _
	{
		inline constexpr auto close_database = [](sqlite3*& ptr)
		{
			sqlite3_close(ptr);
		};
	}

	template <typename T, shion::string_literal Name>
	class DataStore;

	class Database
	{
	public:
		bool open(std::filesystem::path path);
		bool exec(const std::string& query);

		template <query_callback_type T>
		bool query(const std::string& query, T&& callback)
		{
			auto _callback = [](void* user_data, int nb_col, char** data, char** columns) -> int
			{
				auto fun = static_cast<std::remove_reference_t<T>*>(user_data);

				if ((*fun)(std::span(data, nb_col), std::span(columns, nb_col)))
					return (0);
				return (1);
			};

			return (_query(query, _callback, static_cast<void*>(&callback)));
		}

		DatabaseStatement prepare(std::string_view query);

	private:
		using sqlite_callback = int (*)(void*, int, char**, char**);

		bool _query(const std::string& query, sqlite_callback callback, void* user_data);

		shion::utils::owned_resource<sqlite3*, _::close_database> _database;
		std::string                                               _name;
	};
} // namespace B12

#endif
