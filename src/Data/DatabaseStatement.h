#ifndef B12_DATABASE_STATEMENT_H_
#define B12_DATABASE_STATEMENT_H_

#include "B12.h"

#include <codecvt>
#include <concepts>
#include <locale>
#include <variant>
#include <span>

#include <shion/utils/owned_resource.h>

#include <sqlite3.h>

extern "C"
{
	struct sqlite3_stmt;
}

namespace B12
{
	namespace _
	{
		inline constexpr auto free_statement = [](sqlite3_stmt*& ptr)
		{
			if (int err = sqlite3_finalize(ptr); err != 0)
				B12::log(B12::LogLevel::ERROR, "could not free sql statement: {}", sqlite3_errstr(err));
		};

		using statement_resource = shion::utils::owned_resource<sqlite3_stmt*, free_statement>;
	} // namespace _

	struct DatabaseStatement : public _::statement_resource
	{
		using base = _::statement_resource;

		template <typename T>
		bool bind(T&& value)
		{
			return {bind(value, ++index)};
		}

		template <typename T>
		bool bind(T&& value, int param_index)
		{
			if (int ret = _bind(value, param_index); ret != 0)
			{
				B12::log(
					B12::LogLevel::ERROR,
					"failed to bind parameter {} of type {} :\n"
					"Message: {}",
					param_index,
					typeid(T).name(),
					sqlite3_errstr(ret)
				);
				return {false};
			}
			return {true};
		}

		template <typename T>
			requires(requires(T t, DatabaseStatement& stmt)
			{
				{
					std::invoke(t, stmt)
				} -> std::convertible_to<bool>;
			})
		bool exec(T&& callback)
		{
			int ret;

			while ((ret = sqlite3_step(base::resource)) == SQLITE_ROW)
			{
				if (!callback(*this))
					return (false);
			}
			if (ret == SQLITE_DONE)
				return (true);
			B12::log(
				B12::LogLevel::ERROR,
				"failed to execute statement :\n"
				"Message: {}",
				sqlite3_errstr(ret)
			);
			return (false);
		}

		bool exec()
		{
			return (exec(
				[](DatabaseStatement&)
				{
					return (true);
				}
			));
		}

		using variant =
		std::variant<std::monostate, std::span<const std::byte>, std::string, int64, double>;

		std::span<const std::byte> fetchBytes(int column)
		{
			auto* value = sqlite3_column_value(base::resource, column);

			assert(value);
			return (_fetchBytes(value));
		}

		auto fetchBytes()
		{
			return (fetchBytes(index++));
		}

		std::string fetchText(int column)
		{
			auto* value = sqlite3_column_value(base::resource, column);

			assert(value);
			return (_fetchText(value));
		}

		auto fetchText()
		{
			return (fetchText(index++));
		}

		double fetchDouble(int column)
		{
			auto* value = sqlite3_column_value(base::resource, column);

			assert(value);
			return (_fetchDouble(value));
		}

		auto fetchDouble()
		{
			return (fetchDouble(index++));
		}

		int fetchInt(int column)
		{
			auto* value = sqlite3_column_value(base::resource, column);

			assert(value);
			return (_fetchInt(value));
		}

		auto fetchInt()
		{
			return (fetchInt(index++));
		}

		int64 fetchInt64(int column)
		{
			auto* value = sqlite3_column_value(base::resource, column);

			assert(value);
			return (_fetchInt64(value));
		}

		auto fetchInt64()
		{
			return (fetchInt64(index++));
		}

		variant fetchValue(int column)
		{
			auto* value = sqlite3_column_value(base::resource, column);

			assert(value);
			switch (auto datatype = sqlite3_value_type(value))
			{
				case SQLITE_INTEGER:
					return {_fetchInt64(value)};

				case SQLITE_FLOAT:
					return {_fetchDouble(value)};

				case SQLITE_BLOB:
					return {_fetchBytes(value)};

				case SQLITE_TEXT:
					return {_fetchText(value)};

				case SQLITE_NULL:
					return {};

				default:
					B12::log(B12::LogLevel::BASIC, "Datatype {} not found", datatype);
					return {};
			}
		}

		auto fetchValue()
		{
			return (fetchValue(index++));
		}

		using base::base;
		using base::operator=;

		int index{0};

	private:
		std::span<const std::byte> _fetchBytes(sqlite3_value* value) const
		{
			auto* bytes = static_cast<const std::byte*>(sqlite3_value_blob(value));
			int   size  = sqlite3_value_bytes(value);

			assert(bytes != nullptr && size > 0);
			return {bytes, static_cast<size_t>(size)};
		}

		std::string _fetchText(sqlite3_value* value) const
		{
			auto* text = sqlite3_value_text(value);
			int   size = sqlite3_value_bytes(value);

			assert(text != nullptr && size > 0);
			return {text, text + size};
		}

		double _fetchDouble(sqlite3_value* value) const
		{
			return (sqlite3_value_double(value));
		}

		int _fetchInt(sqlite3_value* value) const
		{
			return (sqlite3_value_int(value));
		}

		int64 _fetchInt64(sqlite3_value* value) const
		{
			return (sqlite3_value_int64(value));
		}

		int _bind(std::nullptr_t, int param_index)
		{
			return (sqlite3_bind_null(base::resource, param_index));
		}

		int _bind(dpp::snowflake id, int param_index)
		{
			return (_bind(static_cast<int64>(id), param_index));
		}

		template <typename T>
			requires(std::integral<std::remove_cvref_t<T>>)
		int _bind(T&& value, int param_index)
		{
			if constexpr (sizeof(T) >= sizeof(int64))
				return (sqlite3_bind_int64(base::resource, param_index, std::forward<T>(value)));
			else
				return (sqlite3_bind_int(base::resource, param_index, std::forward<T>(value)));
		}

		template <typename T>
			requires(std::floating_point<std::remove_cvref_t<T>>)
		int _bind(T&& value, int param_index)
		{
			return (sqlite3_bind_double(base::resource, param_index, std::forward<T>(value)));
		}

		template <size_t N>
		int _bind(const std::span<std::byte, N>& data, int param_index)
		{
			return (sqlite3_bind_blob64(
				base::resource,
				param_index,
				data.data(),
				data.size(),
				SQLITE_STATIC
			));
		}

		int _bind(std::string_view str, int param_index)
		{
			return (sqlite3_bind_text64(
				base::resource,
				param_index,
				str.data(),
				str.size(),
				SQLITE_STATIC,
				SQLITE_UTF8
			));
		}

		int _bind(std::wstring_view str, int param_index)
		{
			std::wstring_convert<std::codecvt_utf16<wchar_t>> convertor;
			auto                                              converted = convertor.to_bytes(str.data());

			return (sqlite3_bind_text64(
				base::resource,
				param_index,
				converted.data(),
				str.size(),
				SQLITE_STATIC,
				SQLITE_UTF16
			));
		}
	};
} // namespace B12

#endif
