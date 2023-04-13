#ifndef SHION_VISITOR_H_
#define SHION_VISITOR_H_

namespace shion::utils
{
	template <typename... Ts>
	struct visitor : Ts...
	{
		using Ts::operator()...;
	};
	
	template <typename... Ts>
	visitor(Ts...) -> visitor<Ts...>;
}

#endif /* SHION_VISITOR_H_ */