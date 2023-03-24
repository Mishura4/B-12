#ifndef B12_USER_H_
#define B12_USER_H_

#include "B12.h"

namespace B12
{
	class User
	{
	public:
		explicit User(dpp::snowflake _id);

	private:
		dpp::snowflake _id;
	};
} // namespace B12

#endif
