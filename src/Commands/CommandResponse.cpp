#include "CommandResponse.h"

#include "Data/Lang.h"

using namespace B12;

namespace
{
	constexpr auto success_prefix = shion::literal_concat(
		shion::string_literal<lang::SUCCESS_EMOJI.size()>{lang::SUCCESS_EMOJI.data()}, " "
	);
	constexpr auto confirm_prefix = shion::literal_concat("\xE2\x9A\xA0\xEF\xB8\x8F", " ");
	constexpr auto error_prefix   = shion::literal_concat(
		shion::string_literal<lang::ERROR_EMOJI.size()>{lang::ERROR_EMOJI.data()},
		" "
	);
}

void CommandResponse::format()
{
	if (std::holds_alternative<Thinking>(type))
		return;
	constexpr auto get_type_prefix = [](uint64 index) constexpr -> std::string_view
	{
		switch (index)
		{
			case PossibleTypes::type_index<Success>:
			case PossibleTypes::type_index<SuccessEdit>:
				break;

			case PossibleTypes::type_index<SuccessAction>:
				return (success_prefix);

			case PossibleTypes::type_index<Confirm>:
				return (confirm_prefix);

			case PossibleTypes::type_index<ConfigError>:
			case PossibleTypes::type_index<APIError>:
			case PossibleTypes::type_index<InternalError>:
			case PossibleTypes::type_index<UsageError>:
				return (error_prefix);

			default:
				break;
		}
		return {};
	};
	content.message.content = fmt::format(
		"{}{}{}{}",
		get_type_prefix(type.index()),
		content.message.content,
		content.warnings.empty() ? "\n"sv : ""sv,
		fmt::join(content.warnings, "\n"sv)
	);
}
