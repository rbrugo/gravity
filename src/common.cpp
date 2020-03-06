/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : common
 * @created     : Monday Feb 17, 2020 16:01:09 CET
 * @license     : MIT
 */

#include "common.hpp"

namespace brun
{
// Dump a formatted table on the terminal
void dump(entt::registry & reg, std::optional<int> day)
{
    auto const divisor = fmt::format("|{:-^113}|\n", "");
    if (day.has_value()) {
        std::cout << divisor;
        brun::print("|{:^113}|\n", fmt::format("DAY {}", *day));
    }
    std::cout << divisor;
    brun::print("|{:<10}|{:^20}|{:^40}|{:^40}|\n", "Obj name", "mass", "position", "velocity");
    std::cout << divisor;
    auto dump_single = [](auto const & tag, auto const mass, auto const position, auto const velocity) {
        brun::print("|{:<10}|{:^20}|{:^40}|{:^40}|\n", tag, mass, position, velocity);
    };
    reg.view<brun::tag, brun::mass, brun::position, brun::velocity>().each(dump_single);
    std::cout << divisor << std::flush;
}
} // namespace brun


