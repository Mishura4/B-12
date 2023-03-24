#ifndef B12_DATASTORE_H_
#define B12_DATASTORE_H_

#include "B12.h"

#include "DataStore.h"

#include "DataStructures.h"

namespace B12
{
	struct DataStores
	{
		using GuildSettings = DataStore<GuildSettingsEntry, "guild_settings">;

		static GuildSettings guild_settings;
	};
} // namespace B12

#endif
