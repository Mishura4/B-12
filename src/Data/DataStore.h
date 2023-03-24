#ifndef B12_DATA_STORE_H_
#define B12_DATA_STORE_H_

#include "B12.h"

#include <shion/traits/type_list.h>
#include <shion/utils/owned_resource.h>
#include <shion/utils/string_literal.h>

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_set>

#include "B12.h"

#include "Database.h"
#include "DatabaseStatement.h"

#include "DataStructures.h"

/*
 * TODO: CLEANUP THIS FILE
 * this file is the first iteration of a succesful but incomplete attempt at serializing generic data structures into a database
 * a bit too spaghetti for my taste, it needs a rewrite
 * it's also missing using arbitrary key fields, right now it relies on the first field to be the primary key with a type of dpp::snowflake
 */

extern "C"
{
	struct sqlite3_stmt;

	int sqlite3_finalize(sqlite3_stmt* pStmt);
}

namespace B12
{
	using namespace shion::literals;

	namespace _
	{
		template <typename T>
		// workaround for Visual Studio struggling with name resolution in parameter packs
		constexpr inline auto field_key = T::key;

		template <typename T>
		struct data_store_field_type_helper_s
		{
			static consteval auto getType()
			{
				using string_types = shion::type_list<std::string, std::string_view>;

				constexpr bool msvc_fix_is_text = string_types::has_type<T>;

				if constexpr (std::integral<T> || std::is_same_v<T, dpp::snowflake>)
				{
					return ("INTEGER"_sl);
				}
				else if constexpr (std::floating_point<T>)
				{
					return ("REAL"_sl);
				}
				else if constexpr (msvc_fix_is_text)
				{
					return ("TEXT"_sl);
				}
				else if constexpr (std::is_array_v<T>)
				{
					if constexpr (
						std::is_same_v<char, std::decay_t<T>> ||
						std::is_same_v<char, std::remove_pointer_t<T>>)
					{
						return ("TEXT"_sl);
					}
					else if constexpr (
						std::is_same_v<std::byte, std::decay_t<T>> ||
						std::is_same_v<std::byte, std::remove_pointer_t<T>>)
					{
						return ("BLOB"_sl);
					}
				}
				else if constexpr (std::is_same_v<T, bool>)
				{
					return ("NUMERIC"_sl);
				}
				else
				{
					static_assert(!std::is_same_v<T, T>, "type is unimplemented");
				}
			}
		};

		template <int flag>
		constexpr inline auto sql_constraint_for_field_attr = ""_sl;

		template <>
		constexpr inline auto sql_constraint_for_field_attr<FieldAttributeFlags::PRIMARY_KEY> =
			""_sl; // " PRIMARY KEY"_sl;

		template <int flags>
		struct data_store_attr_helper_s
		{
			static consteval auto getConstraint()
			{
				return (shion::literal_concat<
					sql_constraint_for_field_attr<flags & FieldAttributeFlags::PRIMARY_KEY>,
					sql_constraint_for_field_attr<flags & FieldAttributeFlags::NOT_NULL>,
					sql_constraint_for_field_attr<flags & FieldAttributeFlags::UNIQUE>>());
			}
		};

		template <typename T>
		struct data_store_field_helper_s
		{
			static constexpr auto name    = T::key;
			static constexpr auto storage =
				data_store_field_type_helper_s<typename T::value_type>::getType();
			static constexpr auto constraints =
				data_store_attr_helper_s<T::FIELD_ATTRIBUTES>::getConstraint();
		};
	} // namespace _

	template <typename T>
	struct registry_edit_helper;

	template <typename... Fields>
	struct registry_edit_helper<shion::registry<Fields...>>
	{
		using type = shion::registry<typename Fields::editor...>;

		constexpr static type get(shion::registry<Fields...>& registry)
		{
			constexpr auto                get_field_value_ref = []<typename Field>(
				shion::registry<Fields...>& registry0
			) -> auto&
			{
				return (registry0.template get<Field::key>());
			};
			return {
				typename Fields::editor{get_field_value_ref.template operator()<Fields>(registry)}...
			};
		}
	};

	template <typename T, shion::string_literal Name>
	class DataStore
	{
	public:
		using entry_type = T;
		using edit_entry = typename registry_edit_helper<entry_type>::type;
		constexpr static auto name = Name;

		// the interface to change data within an entry
		// writes on destroy
		struct Entry : public edit_entry
		{
			using edit_entry::get;

			~Entry()
			{
				_data_store.save(*this);
			}

		protected:
			friend class DataStore<T, Name>;

			Entry(DataStore& dataStore, entry_type& entry, bool is_new) :
				edit_entry(registry_edit_helper<entry_type>::get(entry)),
				_data_store(dataStore),
				_is_new(is_new) {}

			Entry()             = delete;
			Entry(const Entry&) = delete;
			Entry(Entry&&)      = delete;

			DataStore& _data_store;
			bool       _is_new{false};

			// populates a DatabaseStatement with an update query
			// returns true on success, false on error
			// stmt is unchanged at the end if no fields need updating
			bool fillUpdateStatement(DatabaseStatement& stmt) const
			{
				std::stringstream queryStream;
				constexpr auto    primaryKey = edit_entry::key_list::template at<0>;
				constexpr auto    idxSeq = std::make_index_sequence<edit_entry::field_type_list::size>();
				UpdateHelper      helper{*this};

				queryStream << std::string_view(shion::literal_concat<"UPDATE ", Name, " SET">());
				helper.fillFieldRelationalList(queryStream, idxSeq);
				if (!helper.num_edited_fields) // no fields to update
					return (true);
				queryStream << std::string_view(
					shion::literal_concat<"\nWHERE ", primaryKey, " = ?">()
				);

				stmt = _data_store._database->prepare(queryStream.str());
				if (!stmt.hasResource())
					return (false);
				if (!helper.bindParameters(stmt, idxSeq))
					return (false);
				if (!stmt.bind(this->template get<primaryKey>()))
					return (false);
				return (true);
			}

			bool fillInsertStatement(DatabaseStatement& stmt) const
			{
				constexpr auto query = _generateInsertQuery();

				stmt = _data_store._database->prepare(query);

				if (!stmt.resource)
					return (false);
				if (!_bindAll(stmt, std::make_index_sequence<edit_entry::field_type_list::size>()))
					return (false);
				return (true);
			}

		private:
			struct UpdateHelper
			{
				template <size_t N>
				bool bindParameters(DatabaseStatement& statement) const
				{
					constexpr auto key   = edit_entry::key_list::template at<N>;
					const auto&    field = entry.template get<key>();

					if (!field.edited)
						return {true};
					return {statement.bind(field.value)};
				}

				template <size_t... N>
				bool bindParameters(DatabaseStatement& statement, std::index_sequence<N...>) const
				{
					if (!(true && ... && bindParameters<N>(statement)))
						return (false);
					return (true);
				}

				template <size_t N>
				constexpr void fillFieldRelational(std::stringstream& oss)
				{
					constexpr auto key = edit_entry::key_list::template at<N>;

					if (!entry.template get<key>().edited)
						return;

					constexpr auto line = shion::literal_concat<"\n\t", _getFieldName<N>(), " = ?">();

					num_edited_fields++;
					if (oss.view().size())
						oss << ", ";
					oss << std::string_view(line);
				}

				template <size_t... N>
				constexpr void fillFieldRelationalList(std::stringstream& oss, std::index_sequence<N...>)
				{
					std::stringstream ss;

					(fillFieldRelational<N>(ss), ...);
					oss << ss.view();
				}

				const Entry& entry;
				int          num_edited_fields{0};
			};

			template <size_t N>
			bool _bindAll(DatabaseStatement& statement) const
			{
				constexpr auto key   = edit_entry::key_list::template at<N>;
				const auto&    entry = this->template get<key>();

				return {statement.bind(entry.value)};
			}

			template <size_t... N>
			bool _bindAll(DatabaseStatement& statement, std::index_sequence<N...>) const
			{
				return {(true && ... && _bindAll<N>(statement))};
			}
		};

		Entry get(dpp::snowflake id)
		{
			std::optional<T>& entry = _fetch(id);

			if (entry.has_value())
				return {*this, entry.value(), false};
			entry                                     = T();
			entry.value().template get<"snowflake">() = id;
			return {*this, entry.value(), true};
		};

		const std::optional<T> &operator[](dpp::snowflake id)
		{
			return (_fetch(id));
		}

		bool save(const Entry& data);

		void setDatabase(Database& db)
		{
			_database = &db;
			_database->exec(_generateCreateTableQuery());
		}

		bool loadAll()
		{
			DatabaseStatement stmt = _database->prepare(_generateSelectQuery());

			return (stmt.exec(_loadCallback));
		}

	private:
		struct GeneralQueryHelper
		{
			template <size_t N, bool condition = true>
			static consteval auto _getFieldName()
			{
				if constexpr (condition)
					return (T::key_list::template at<N>);
				else
					return (shion::string_literal(""));
			}

			template <size_t N>
			static consteval auto _getFieldNameWithComma()
			{
				constexpr auto field_name = T::key_list::template at<N>;

				if constexpr (N == 0)
					return (field_name);
				else
					return (shion::literal_concat<", ", field_name>());
			}

			template <size_t... N>
			static consteval auto _getFieldList(std::index_sequence<N...>)
			{
				if constexpr (sizeof...(N) == 1)
					return (_getFieldName<T, 0>());
				else
				{
					return (shion::literal_concat<_getFieldNameWithComma<N>()...>());
				}
			}

			template <size_t N>
			static consteval auto _getFieldPlaceholder()
			{
				if constexpr (N == 0)
					return (string_literal{"?"});
				else
					return (shion::literal_concat<", ", string_literal{"?"}>());
			}

			template <size_t... N>
			static consteval auto _getFieldPlaceholders(std::index_sequence<N...>)
			{
				if constexpr (sizeof...(N) == 1)
					return (string_literal{"?"});
				else
					return (shion::literal_concat<_getFieldPlaceholder<N>()...>());
			}

			template <size_t N, bool condition = true, bool comma = true>
			static consteval auto _getFieldRelational()
			{
				constexpr auto line = shion::literal_concat<"\n\t", _getFieldName<N>(), " = ?">();

				if constexpr (!condition)
					return (shion::string_literal(""));
				else if constexpr (N != 0 && comma)
					return (shion::literal_concat<",", line>());
				else
					return (line);
			}

			template <size_t... N>
			static consteval auto _getFieldRelationalList(std::index_sequence<N...>)
			{
				return (shion::literal_concat<_getFieldRelational<N>()...>());
			}

			template <size_t N>
			constexpr static auto _attributes = T::field_type_list::template at<N>::FIELD_ATTRIBUTES;

			template <size_t... N>
			static consteval auto _getWhereClause(std::index_sequence<N...>)
			{
				constexpr int num_keys =
					(0 + ... + (_attributes<N> & FieldAttributeFlags::PRIMARY_KEY ? 1 : 0));

				static_assert(num_keys == 1);
				return (shion::literal_concat<
					"\nWHERE",
					shion::literal_concat<(_getFieldRelational<
						N,
						_attributes<N> & FieldAttributeFlags::PRIMARY_KEY,
						false>())...>()>());
			}
		};

		constexpr static auto _generateUpdateQuery()
		{
			constexpr auto idxSeq = std::make_index_sequence<T::key_list::size>();

			return (shion::literal_concat<
				"UPDATE ",
				Name,
				" SET ",
				GeneralQueryHelper::_getFieldRelationalList(idxSeq),
				GeneralQueryHelper::_getWhereClause(idxSeq)>());
		}

		constexpr static auto _generateSelectQuery()
		{
			constexpr auto idxSeq = std::make_index_sequence<T::key_list::size>();

			return (
				shion::
				literal_concat<"SELECT ", GeneralQueryHelper::_getFieldList(idxSeq), " FROM ", Name>());
		}

		consteval static auto _generateInsertQuery()
		{
			constexpr auto idxSeq = std::make_index_sequence<T::key_list::size>();

			return (shion::literal_concat<
				"INSERT INTO ",
				Name,
				"\n\t(",
				GeneralQueryHelper::_getFieldList(idxSeq),
				")\nVALUES (",
				GeneralQueryHelper::_getFieldPlaceholders(idxSeq),
				");">());
		}

		std::optional<T> &_fetch(dpp::snowflake id)
		{
			return (_data[id]);
		}

		template <size_t N, bool condition = true>
		static consteval auto _getFieldName()
		{
			if constexpr (condition)
				return (T::key_list::template at<N>);
			else
				return (shion::string_literal(""));
		}

		template <size_t N>
		static consteval auto _getCreateTableQuery()
		{
			using helper = _::data_store_field_helper_s<typename T::field_type_list::template at<N>>;

			constexpr auto line = shion::
				literal_concat<"\n\t", _getFieldName<N>(), " ", helper::storage, helper::constraints>();

			if constexpr (N == 0)
				return (line);
			else
				return (shion::literal_concat<",", line>());
		}

		template <size_t... N>
		static consteval auto _getConstraintsLine()
		{
			constexpr int num_keys =
			(0 + ... +
				(T::field_type_list::template at<N>::FIELD_ATTRIBUTES & FieldAttributeFlags::PRIMARY_KEY
					 ? 1
					 : 0));

			if constexpr (num_keys == 0)
				return (""_sl);
			else
			{
				return (shion::literal_concat<
					",\n\tPRIMARY KEY (",
					shion::literal_concat<(_getFieldName<
						N,
						T::field_type_list::template at<N>::FIELD_ATTRIBUTES &
						FieldAttributeFlags::PRIMARY_KEY>())...>(),
					")">());
			}
		}

		template <size_t... N>
		static consteval auto _getCreateTableQuery(std::index_sequence<N...>)
		{
			return (
				shion::literal_concat<_getCreateTableQuery<N>()..., _getConstraintsLine<N...>(), "\n">());
		}

		consteval static auto _generateCreateTableQuery()
		{
			constexpr auto idxSeq = std::make_index_sequence<T::key_list::size>();

			return (shion::literal_concat<
				"CREATE TABLE IF NOT EXISTS ",
				Name,
				" (",
				_getCreateTableQuery(idxSeq),
				");">());
		}

		template <size_t N>
		static void _loadField(DatabaseStatement& stmt, T& entry)
		{
			constexpr auto key = T::key_list::template at<N>;
			using type = typename T::value_type_list::template at<N>;

			if constexpr (std::integral<type> || std::same_as<type, dpp::snowflake>)
			{
				entry.template get<key>() = static_cast<type>(stmt.fetchInt64(N));
			}
			else if constexpr (std::floating_point<type>)
			{
				entry.template get<key>() = static_cast<type>(stmt.fetchDouble(N));
			}
			else if constexpr (std::assignable_from<type, std::string>)
			{
				entry.template get<key>() = static_cast<type>(stmt.fetchText(N));
			}
			else
			{
				static_assert(!std::same_as<type, type>, "unsupported type");
			}
		}

		template <size_t... Ns>
		static void _loadFields(DatabaseStatement& stmt, T& entry, std::index_sequence<Ns...>)
		{
			(_loadField<Ns>(stmt, entry), ...);
		}

		shion::utils::observer_ptr<Database>                 _database{nullptr};
		std::unordered_map<dpp::snowflake, std::optional<T>> _data;

		std::function<bool(DatabaseStatement&)> _loadCallback = [this](DatabaseStatement& s)
		{
			dpp::snowflake    id{static_cast<uint64>(s.fetchInt64(0))};
			std::optional<T>& entry = _data[id];

			entry = GuildSettingsEntry{};
			_loadFields(s, entry.value(), std::make_index_sequence<T::key_list::size>());
			return (true);
		};
	};

	template <typename T, shion::string_literal Name>
	bool DataStore<T, Name>::save(const Entry& entry)
	{
		if (!_database)
			return (false);

		DatabaseStatement statement;

		if (entry._is_new)
		{
			if (!entry.fillInsertStatement(statement))
				return (false);
		}
		else
		{
			if (!entry.fillUpdateStatement(statement))
				return (false);
		}
		if (!statement.hasResource())
			return (true);
		return (statement.exec());
	}
}

#endif
