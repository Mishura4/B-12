#include "B12.h"

#include "Core/Bot.h"

using namespace B12;

bool B12::isLogEnabled(LogLevel level)
{
	return (Bot::isLogEnabled(level));
}

void B12::log(LogLevel level, std::string_view str)
{
	Bot::log(level, str);
}
