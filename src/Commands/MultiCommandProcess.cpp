#include "B12.h"

#include "Core/Bot.h"
#include "MultiCommandProcess.h"

using namespace B12;

using MCP = MultiCommandProcess;

MultiCommandProcess::MultiCommandProcess(dpp::message master_) :
	master(std::make_unique<dpp::message>(master_)),
	callback{
		[this](const dpp::confirmation_callback_t& event)
		{
			callbackFn(event);
		}
	} {}

MultiCommandProcess::MultiCommandProcess(MultiCommandProcess&& rval) noexcept :
	master{std::move(rval.master)},
	expiration{rval.expiration},
	step_index{rval.step_index},
	callback{
		[this](const dpp::confirmation_callback_t& event)
		{
			callbackFn(event);
		}
	} {}

void MCP::callbackFn(const dpp::confirmation_callback_t& event)
{
	if (std::holds_alternative<dpp::message>(event.value))
	{
		const auto&    myMessage = std::get<dpp::message>(event.value);
		dpp::snowflake old_id    = (master ? master->id : dpp::snowflake_t<dpp::message>{0});

		master     = std::make_unique<dpp::message>(myMessage);
		expiration = Bot::lastUpdate() + std::chrono::seconds();
		processing = false;
		if (old_id)
			Bot::MCPUpdate(old_id);
	}
}

MCP &MCP::operator=(MCP&& rhs) noexcept
{
	master     = std::move(rhs.master);
	expiration = rhs.expiration;
	step_index = rhs.step_index;
	return (*this);
}
