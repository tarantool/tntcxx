#include "Dec.h"

template <class ...P>
struct Plan {
    template <class ...Q>
    constexpr Plan<P..., Q...> operator+ (Plan<Q...>) const { return {}; }
};

template <int64_t ...I, size_t ...J, class ...T>
constexpr auto collect_plan_h(Path<I...>, Path<J...>, T&... t);

template<int64_t ...I, class ...T>
constexpr auto expand_plan(T&... t)
{
    constexpr auto res = Plan<Path<I...>>{};
    constexpr auto p = Path<I...>{};
    auto& r = resolve_path(p, t...);
    using R = std::remove_reference_t<decltype(r)>;
    if constexpr (has_children(detectFamily<R>())) {
	using U = unwrap_t<R>;
	if constexpr (tnt::is_tuplish_v<U>) {
	    auto e = tnt::tuple_iseq<U>{};
	    return res + collect_plan_h(p, e, t...);
	} else {
	    return res + Plan<Path<I..., -1>>{};
	}
    } else {
	return res;
    }
}

template <int64_t ...I, size_t ...J, class ...T>
constexpr auto collect_plan_h(Path<I...>, tnt::iseq<J...>, T&... t)
{
    return (expand_plan<I..., J>(t...) + ...);
}

template <class ...T>
constexpr auto collect_plan(T&... t)
{
    return collect_plan_h(Path<>{}, tnt::make_iseq<sizeof ...(T)>{}, t...);
}