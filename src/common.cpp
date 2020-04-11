/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : common
 * @created     : Monday Feb 17, 2020 16:01:09 CET
 * @license     : MIT
 */

#include "common.hpp"
#include <units/format.h>
#include <range/v3/numeric/accumulate.hpp>

namespace brun
{
// Dump a formatted table on the terminal
void dump(entt::registry & reg, std::optional<int> day)
{
    auto const divisor = fmt::format("|{:-^113}|\n", "");

    if (day.has_value()) {
        fmt::print(divisor);
        fmt::print("|{:^113}|\n", fmt::format("DAY {}", *day));
    }
    fmt::print(divisor);
    fmt::print("|{:<10}|{:^14}|{:^43}|{:^43}|\n", "Obj name", "mass", "position", "velocity");
    fmt::print(divisor);
    auto dump_single = [](auto const & tag, auto const mass, auto const position, auto const velocity) {
        fmt::print("|{:<10}|{:^14%.3gQ %q}|{:^43}|{:^43}|\n", tag, mass, position, velocity);
    };
    reg.view<brun::tag, brun::mass, brun::position, brun::velocity>().each(dump_single);
    fmt::print(divisor);
}

// Computes the system's center of mass
auto center_of_mass(entt::registry const & registry)
    -> brun::position
{
    using weighted_position = decltype(brun::position{} * brun::mass{});
    auto pairwise_sum = []<class T, class U>(std::pair<T, U> a, std::pair<T, U> const & b) noexcept {
        a.first = std::move(a.first) + b.first;
        a.second = std::move(a.second) + b.second;
        return a;
    };
    using pair = std::pair<weighted_position, brun::mass>;

    auto const pos = [&registry](entt::entity const entt) mutable {
        auto const [pos, mass] = registry.get<brun::position, brun::mass>(entt);
        return pair{pos * mass, mass};
    };
    auto objects = registry.view<brun::position const, brun::mass const>();
    auto const [wpos, total_mass] = ::ranges::accumulate(objects, pair{}, pairwise_sum, pos);
    return 1./total_mass * wpos;
}

} // namespace brun


