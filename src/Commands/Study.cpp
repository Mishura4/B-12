#include "B12.h"

#include "Core/Bot.h"

#include "Command.h"
#include "Data/DataStores.h"
#include "Data/Lang.h"
#include "Study.h"

using namespace B12;

namespace
{
	//constexpr auto STUDY_EMOJI = "\xF0\x9F\x93\x96"sv;

	/*void add_study_button(dpp::message& m)
	{
		m.add_component(
			dpp::component{}.add_component(
				dpp::component{}.set_emoji(std::string(STUDY_EMOJI)).set_id(STUDY_COMMAND_BUTTON_ID)
			)
		);
	}*/
}

template <>
void Bot::command<"study">(
	const dpp::interaction_create_t& e,
	const dpp::interaction&          interaction,
	command_option_view              /*options*/
)
{
	Guild*       guild = Bot::fetchGuild(interaction.guild_id);
	dpp::message reply;

	if (interaction.type == dpp::it_component_button)
		reply.set_flags(dpp::m_ephemeral);
	if (!guild->studyRole() || !guild->studyChannel())
	{
		reply.content = lang::DEFAULT.ERROR_STUDY_BAD_SETTINGS;
		e.reply(reply);
		return;
	}
	B12::AsyncExecutor<dpp::confirmation> replyExecutor([&](const dpp::confirmation&) {});
	const dpp::role&                       studyRole = guild->studyRole().value();
	const dpp::guild_member& issuer = interaction.member;

	replyExecutor(&dpp::interaction_create_t::thinking, &e, true);
	if (std::ranges::find(issuer.roles, studyRole.id) != issuer.roles.end())
	{
		B12::AsyncExecutor<dpp::confirmation> roleExecutor(
			[&](const dpp::confirmation&)
			{
				reply.content = lang::DEFAULT.COMMAND_STUDY_REMOVED;
			},
			[&](const dpp::error_info& error)
			{
				reply = fmt::format("error: {}", error.message);
			}
		);

		roleExecutor(
			&dpp::cluster::guild_member_remove_role,
			&Bot::bot(),
			guild->dppGuild().id,
			issuer.user_id,
			studyRole.id
		);

		roleExecutor.wait();
		replyExecutor.wait();
		e.edit_original_response(reply);
	}
	else
	{
		B12::AsyncExecutor<dpp::confirmation> roleExecutor(
			[&](const dpp::confirmation&)
			{
				reply.content = lang::DEFAULT.COMMAND_STUDY_ADDED.format(*guild->studyChannel());
			},
			[&](const dpp::error_info& error)
			{
				reply = fmt::format("error: {}", error.message);
			}
		);

		roleExecutor(
			&dpp::cluster::guild_member_add_role,
			&Bot::bot(),
			guild->dppGuild().id,
			issuer.user_id,
			studyRole.id
		);

		roleExecutor.wait();
		replyExecutor.wait();
		e.edit_original_response(reply);
		if (interaction.type == dpp::it_application_command)
		{
			auto& command = std::get<dpp::command_interaction>(e.command.data);

			Bot::bot().message_create(
				dpp::message{
					interaction.channel_id,
					fmt::format(
						"{} was sent to the {} realm. <a:nodyesnod:1078439588021944350>",
						issuer.get_mention(),
						command.get_mention()
					)
				}.set_reference(interaction.id)
			);
		}
	}
}

namespace
{
	dpp::message get_study_info_message(Guild* guild)
	{
		const auto& curRole    = guild->studyRole();
		const auto& curChannel = guild->studyChannel();

		return (dpp::message(
			fmt::format(
				"Welcome to the `/study` installation wizard! "
				"<:catthumbsup:1066427078284681267>\n\nCurrent "
				"study role is {}\nCurrent study channel is {}\n\nTo change these settings, please use "
				"`/server study settings` with command parameters.",
				(curRole ? curRole->get_mention() : "(none)"),
				(curChannel ? curChannel->get_mention() : "(none)")
			)
		));
	}

	struct StudySettingsHelper
	{
		Guild*                           guild;
		const dpp::interaction_create_t* event;
		std::optional<dpp::role>         newRole{std::nullopt};
		std::optional<dpp::channel>      newChannel{std::nullopt};
		std::string                      responseNotes;

		void handle(command_option_view options)
		{
			event->thinking(false);
			for (const auto& opt : options)
			{
				if (!handle_opt(opt))
					return;
			}
			success();
		}

		void success()
		{
			std::stringstream ss;

			ss << "Successfully set ";
			if (newRole.has_value())
			{
				ss << fmt::format("study role to {}", newRole.value());
				guild->studyRole(&newRole.value());
				if (newChannel.has_value())
					ss << " and ";
			}
			if (newChannel.has_value())
			{
				guild->studyChannel(&newChannel.value());
				ss << fmt::format("study channel to {}", newChannel.value());
			}
			ss << '!';
			if (!responseNotes.empty())
			{
				ss << responseNotes;
			}
			if (!guild->saveSettings())
			{
				ss << "\n\xE2\x9A\xA0**There was an issue saving the settings, "
					"they will be lost after next bot restart**\xE2\x9A\xA0";
			}
			event->edit_original_response(ss.str());
		}

		bool set_role(dpp::snowflake role_id)
		{
			try
			{
				dpp::role_map role_map =
					Bot::bot().roles_get_sync(
						guild->settings().get<"snowflake">()
					); // TODO: give access to id from guild

				auto it = role_map.find(role_id);
				if (it == role_map.end())
				{
					event->edit_original_response(lang::DEFAULT.ERROR_ROLE_NOT_FOUND.format(role_id));
					return (false);
				}
				newRole = it->second;
				guild->studyRole(&newRole.value());
			}
			catch (const dpp::rest_exception&)
			{
				event->edit_original_response(lang::DEFAULT.ERROR_ROLE_FETCH_FAILED.format(role_id));
				return (false);
			}
			return (true);
		}

		bool set_channel(dpp::snowflake channel_id)
		{
			newChannel = guild->findChannel(channel_id);
			if (!newChannel)
			{
				event->edit_original_response(lang::DEFAULT.ERROR_CHANNEL_NOT_FOUND.format(channel_id));
				return (false);
			}
			dpp::permission myPermissions     = guild->getPermissions(guild->b12User(), *newChannel);
			dpp::permission permissionsNeeded = dpp::p_send_messages | dpp::p_use_application_commands;

			if (!myPermissions.has(permissionsNeeded))
			{
				event->edit_original_response(
					lang::DEFAULT.PERMISSION_BOT_MISSING.format(
						newChannel.value(),
						permissionsNeeded ^ (myPermissions & permissionsNeeded)
					)
				);
				return (false);
			}
			return (post_study_message());
		}

		bool post_study_message()
		{
			dpp::message studyMessage{lang::DEFAULT.STUDY_CHANNEL_WELCOME};
			auto         messageCallback = [&](const dpp::message& m)
			{
				studyMessage = m;
			};
			B12::AsyncExecutor<dpp::message> executor(messageCallback);

			studyMessage.set_guild_id(newChannel->guild_id);
			studyMessage.set_channel_id(newChannel->id);
			studyMessage.add_component(
				dpp::component().add_component(
					dpp::component{}.set_emoji("\xF0\x9F\x93\x96").set_id(STUDY_COMMAND_BUTTON_ID)
				)
			);
			executor(&dpp::cluster::message_create, &Bot::bot(), studyMessage);
			executor.wait();
			if (!studyMessage.sent)
			{
				event->edit_original_response({lang::DEFAULT.ERROR_GENERIC_COMMAND_FAILURE});
				return (false);
			}
			return (true);
		}

		bool handle_opt(const dpp::command_data_option& opt)
		{
			if (std::holds_alternative<dpp::snowflake>(opt.value))
			{
				dpp::snowflake id = std::get<dpp::snowflake>(opt.value);

				if (opt.type == dpp::co_role)
				{
					if (!set_role(id))
						return (false);
				}
				else if (opt.type == dpp::co_channel)
				{
					if (!set_channel(id))
						return (false);
				}
			}
			return (true);
		}
	};
} // namespace

template <>
void Bot::command<"settings server study">(
	const dpp::interaction_create_t& e,
	const dpp::interaction&          interaction,
	command_option_view              options
)
{
	if (options.empty())
	{
		Guild* guild = Bot::fetchGuild(interaction.guild_id);

		e.reply(get_study_info_message(guild));
		return;
	}
	StudySettingsHelper helper{.guild = Bot::fetchGuild(interaction.guild_id), .event = &e};

	helper.handle(options);
}

namespace
{
	template <typename T>
	constexpr auto to_array_actually(auto&&... args)
	{
		return (std::array<T, sizeof...(args)>{T{args}...});
	}

	constexpr auto handlers = to_array_actually<bool (*)(const dpp::message&, StudySetupMCP&)>(
		[](const dpp::message& msg, StudySetupMCP& mcp)
		{
			dpp::message reply{"step 2!"};

			reply.set_channel_id(msg.channel_id);
			reply.set_guild_id(msg.guild_id);
			reply.set_reference(msg.id, msg.guild_id, msg.channel_id);
			Bot::bot().message_create(reply, mcp.callback);
			return (true);
		},
		[](const dpp::message& msg, StudySetupMCP& mcp)
		{
			dpp::message reply{"step 3 hehe"};

			reply.set_channel_id(msg.channel_id);
			reply.set_guild_id(msg.guild_id);
			reply.set_reference(msg.id, msg.guild_id, msg.channel_id);
			Bot::bot().message_create(reply, mcp.callback);
			return (true);
		}
	);
} // namespace

bool StudySetupMCP::step(const dpp::message& msg)
{
	if (handlers[step_index](msg, *this))
	{
		++step_index;
		return (true);
	}
	return (false);
}

uint32_t StudySetupMCP::nbSteps() const
{
	return (std::tuple_size_v<decltype(handlers)>);
}
