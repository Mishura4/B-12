#ifndef B12_DATASTRUCTURES_H_
#define B12_DATASTRUCTURES_H_

#include "B12.h"

namespace B12
{
	namespace _
	{
		struct FieldAttributeFlags
		{
			enum Values
			{
				NONE        = shion::bitflag(0),
				PRIMARY_KEY = shion::bitflag(1),
				UNIQUE      = shion::bitflag(2),
				NOT_NULL    = shion::bitflag(3)
			};
		};
	}

	using FieldAttributeFlags = _::FieldAttributeFlags::Values;

	template <typename ValueType>
	struct ValueEditWrapper // wrapper class to mark the value as edited, useful for UPDATE queries
	{
		template <typename T>
			requires(requires(ValueType& val, std::add_const_t<T> t) { val = t; })
		ValueEditWrapper &             operator=(const T& other)
		{
			value  = other;
			edited = true;
			return (*this);
		}

		template <typename T>
			requires(requires(ValueType& val, std::add_rvalue_reference_t<T> t) { val = t; })
		ValueEditWrapper &             operator=(T&& other)
		{
			value  = std::move(other);
			edited = true;
			return (*this);
		}

		operator const ValueType&() const &
		{
			return (value);
		}

		operator ValueType&&() &&
		{
			return (std::move(value));
		}

		ValueType& value;
		bool       edited{false};
	};

	template <auto Key, typename ValueType, FieldAttributeFlags Attributes>
	struct DataField : public shion::_::registry_field<Key, ValueType>
	{
		using base = shion::_::registry_field<Key, ValueType>;

		using editor = shion::_::registry_field<Key, ValueEditWrapper<ValueType>>;

		using base::base;
		using base::operator=;

		constexpr static int FIELD_ATTRIBUTES = Attributes;
	};

	template <
		shion::string_literal Key,
		typename T,
		FieldAttributeFlags Attributes = FieldAttributeFlags::NONE>
	constexpr auto data_field() -> DataField<Key, T, Attributes>
	{
		return {};
	}

	template <shion::string_literal Key, typename T, FieldAttributeFlags Attributes, typename... Args>
		requires(sizeof...(Args) > 0)
	constexpr auto data_field(Args&&... args) -> DataField<Key, T, Attributes>
	{
		return DataField<Key, T, Attributes>(std::forward<Args>(args)...);
	}

	template <
		shion::_::intellisense_literal_fix auto Key,
		typename T,
		FieldAttributeFlags Attributes = FieldAttributeFlags::NONE>
	constexpr auto data_field() -> DataField<Key, T, Attributes>
	{
		return {};
	}

	template <
		shion::_::intellisense_literal_fix auto Key,
		typename T,
		FieldAttributeFlags Attributes,
		typename... Args>
	constexpr auto data_field(Args&&... args) -> DataField<Key, T, Attributes>
	{
		return DataField<Key, T, Attributes>(std::forward<Args>(args)...);
	}

	using GuildSettingsEntry = decltype(shion::registry(
		data_field<"snowflake", dpp::snowflake, FieldAttributeFlags::PRIMARY_KEY>(),
		data_field<"study_channel", dpp::snowflake>(),
		data_field<"study_react_message", dpp::snowflake>(),
		data_field<"study_role", dpp::snowflake>()
	));
} // namespace MyNamespace

#endif
