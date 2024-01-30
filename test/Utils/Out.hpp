#pragma once

#include <iostream>
#include <string_view>

/**
 * Printer of expressions and their names.
 * Warning: works incorrectly if expressions contain commas.
 */
#define OUT(...) out_internal::out(#__VA_ARGS__, __VA_ARGS__)

namespace out_internal {
template <class... T>
void out(std::string_view names, T&&... ts)
{
	auto helper = [&names](auto&& t) {
		size_t pos = names.find(',');
		if (pos == names.npos) {
			std::cout << names << " = " << t;
			names = "";
		} else {
			std::cout << names.substr(0, pos) << " = " << t << ", ";
			names = names.substr(pos + 1);
		}
	};
	(..., helper(std::forward<T>(ts)));
	std::cout << std::endl;
}
} // namespace out_internal {
