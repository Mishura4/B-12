#ifndef B12_COMMAND_HANDLER_H_
#define B12_COMMAND_HANDLER_H_

#include "B12.h"
#include "Command.h"

#include "../Guild/Guild.h"

#include "Data/Lang.h"

namespace B12
{
	class CommandHandler
	{
	public:
		CommandHandler(const CommandHandler &) = delete;
		CommandHandler(CommandHandler &&) = delete;
		
		CommandHandler &operator=(const CommandHandler &) = delete;
		CommandHandler &operator=(CommandHandler &&) = delete;
		
		CommandHandler(const dpp::slashcommand_t& e);
		CommandHandler(const dpp::button_click_t& e);
	
		template <typename Callable, typename... Args>
		auto call(Callable&& callable, Args&&... args) -> CommandResponse;

		template <typename Callable, typename... Args>
		requires (invocable_r<Callable, CommandResponse, Args...> ||
			invocable_r<Callable, CommandResponse, CommandHandler *, Args...>)
		void exec(Callable &&callable, Args&&... args);

		template <string_literal Command>
		void exec(command_option_view options)
		{
			exec(&CommandHandler::command<Command>, options);
		}
		
		template <string_literal Command>
		CommandResponse command(command_option_view options);

		void sendThink(bool ephemeral = true);

		bool isInteraction() const;
	
	private:
		dpp::cluster *_cluster;
		const dpp::user *_issuer{nullptr};
		const dpp::snowflake _guild_id{0};
		const dpp::guild_member *_member_issuer{nullptr};
		const dpp::channel *_channel{nullptr};

		struct InteractionSource
		{
			enum Type
			{
				UNKNOWN = 0,
				SLASH_COMMAND,
				BUTTON_CLICK
			};
			const dpp::interaction_create_t *event;
			Type type{UNKNOWN};
			bool has_replied{false};
			
			AsyncExecutor<dpp::confirmation> thinking_executor{[this](const dpp::confirmation &)
			{
				has_replied = true;
			}};
		};

		std::variant<InteractionSource> _source;

		auto _getInteraction() -> InteractionSource &;
		void _process(CommandResponse &response);
	};
	
	template <typename Callable, typename... Args>
	auto CommandHandler::call(Callable&& callable, Args&&... args) -> CommandResponse
	{
		if constexpr (std::is_member_function_pointer_v<std::remove_cvref_t<Callable>>)
			return (std::invoke(callable, this, std::forward<Args>(args)...));
		else
			return (std::invoke(callable, std::forward<Args>(args)...));
	}

	template <typename Callable, typename... Args>
	requires (invocable_r<Callable, CommandResponse, Args...> ||
		invocable_r<Callable, CommandResponse, CommandHandler *, Args...>)
	void CommandHandler::exec(Callable&& callable, Args&&... args)
	{
		CommandResponse response = call(std::forward<Callable>(callable), std::forward<Args>(args)...);

		_process(response);
	}

}

#endif /* B12_COMMAND_HANDLER_H_ */
