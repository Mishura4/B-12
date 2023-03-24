#include "B12.h"

#include "Database.h"

#include "Core/Bot.h"

extern "C"
{
	#include <sqlite3.h>
}

using namespace B12;

bool Database::open(std::filesystem::path path)
{
	sqlite3* ptr;

	_name = stdfs::relative(path).string();
	if (int ret = sqlite3_open_v2(
			path.string().c_str(),
			&ptr,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
			nullptr
		);
		ret != SQLITE_OK)
	{
		B12::log(B12::LogLevel::ERROR, "{} : could not open database", _name);
		B12::log(B12::LogLevel::ERROR, "  sqlite error: {}", sqlite3_errstr(ret));
		ret = sqlite3_open_v2(
			path.string().c_str(),
			&ptr,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_MEMORY | SQLITE_OPEN_FULLMUTEX,
			nullptr
		);
		if (ret != SQLITE_OK)
		{
			B12::log(B12::LogLevel::ERROR, "{} : could not open database in-memory", _name);
			B12::log(B12::LogLevel::ERROR, "\tsqlite error: {}", sqlite3_errstr(ret));
			return (false);
		}
		_name = fmt::format("(virtual) {}", _name);
		B12::log(B12::LogLevel::ERROR, "  opened database as in-memory instead");
	}
	_database = ptr;
	return (true);
}

bool Database::exec(const std::string& query)
{
	char* error{nullptr};

	B12::log(B12::LogLevel::TRACE, "Executing SQL query (exec):\n{}\n", query);
	if (int ret = sqlite3_exec(_database.get(), query.c_str(), nullptr, nullptr, &error);
		error || ret != SQLITE_OK)
	{
		B12::log(
			B12::LogLevel::ERROR,
			"{}: query execution failed :\n"
			"{}\n"
			"Message: {}",
			_name,
			query,
			(error ? error : sqlite3_errstr(ret))
		);
		if (error)
			sqlite3_free(error);
		return (false);
	}
	return (true);
}

bool Database::_query(const std::string& query, sqlite_callback callback, void* userdata)
{
	char* error{nullptr};

	B12::log(B12::LogLevel::TRACE, "Executing SQL query (query):\n{}\n", query);
	if (int ret = sqlite3_exec(_database.get(), query.c_str(), callback, userdata, &error);
		error || ret != SQLITE_OK)
	{
		B12::log(
			B12::LogLevel::ERROR,
			"{}: query execution failed :\n"
			"{}\n"
			"Message: {}",
			_name,
			query,
			(error ? error : sqlite3_errstr(ret))
		);
		if (error)
			sqlite3_free(error);
		return (false);
	}
	return (true);
}

DatabaseStatement Database::prepare(std::string_view query)
{
	sqlite3_stmt* ret;
	const char*   tail;

	if (int err = sqlite3_prepare_v3(
			_database.get(),
			query.data(),
			static_cast<int>(query.size()),
			SQLITE_PREPARE_NO_VTAB,
			&ret,
			&tail
		);
		err < 0)
	{
		B12::log(
			B12::LogLevel::ERROR,
			"{}: query preparation failed :\n"
			"{}\n"
			"Message: {}",
			_name,
			query,
			sqlite3_errstr(err)
		);
		return {nullptr};
	}
	return {ret};
}
