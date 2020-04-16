/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : common
 * @created     : Monday Feb 17, 2020 16:01:09 CET
 * @license     : MIT
 */

#include "common.hpp"
#include <units/format.h>

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

// Build the rotation matrix
auto build_rotation_matrix(brun::rotation_info const rotation)
    -> brun::rotation_matrix
{
    auto const x_rad = rotation.x_axis * 2 * M_PI / 255;
    auto const z_rad = rotation.z_axis * 2 * M_PI / 255;

    auto const z_rotation = brun::rotation_matrix{
        {  std::cos(z_rad), std::sin(z_rad), 0. },
        { -std::sin(z_rad), std::cos(z_rad), 0. },
        {               0.,             0.,  1. }
    };

    auto const x_rotation = brun::rotation_matrix{
            {1.,  0.,              0.},
            {0.,  std::cos(x_rad), std::sin(x_rad)},
            {0., -std::sin(x_rad), std::cos(x_rad)}
    };

    return x_rotation * z_rotation;
}

auto build_reversed_rotation_matrix(brun::rotation_info const rotation)
    -> brun::rotation_matrix
{
    auto const x_rad = -rotation.x_axis * 2 * M_PI / 255;
    auto const z_rad = -rotation.z_axis * 2 * M_PI / 255;

    auto const z_rotation = brun::rotation_matrix{
        {  std::cos(z_rad), std::sin(z_rad), 0. },
        { -std::sin(z_rad), std::cos(z_rad), 0. },
        {               0.,             0.,  1. }
    };

    auto const x_rotation = brun::rotation_matrix{
            {1.,  0.,              0.},
            {0.,  std::cos(x_rad), std::sin(x_rad)},
            {0., -std::sin(x_rad), std::cos(x_rad)}
    };

    return z_rotation * x_rotation;
}

} // namespace brun


