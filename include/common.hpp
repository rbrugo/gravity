/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : common
 * @created     : Monday Feb 17, 2020 15:33:57 CET
 * @license     : MIT
 * */

#ifndef COMMON_HPP
#define COMMON_HPP

#include <iostream>
#include <deque>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <linear_algebra.hpp>

#include <entt/entt.hpp>

#include <units/math.h>
#include <units/physical/si/length.h>
#include <units/physical/si/time.h>
#include <units/physical/si/mass.h>
#include <units/physical/si/force.h>
#include <units/physical/si/frequency.h>

namespace brun
{

// Formatted table on terminal
void dump(entt::registry & reg, std::optional<int> day = std::nullopt);

} // namespace brun

namespace STD_LA
{
    // Defining a way to print a vector onto the screen
    template <typename T, std::size_t N>
    inline
    std::ostream & operator<<(std::ostream & out, STD_LA::fs_vector<T, N> const & v)
    {
        out << fmt::format("({:%.3gQ}, {:%.3gQ}, {:%.3gQ}) {:%q}", v[0], v[1], v[2], v[2]);
        return out;
    }
} // namespace STD_LA

namespace units::si
{
    // Some new SI units type definitions
    struct megametre : prefixed_unit<megametre, mega, metre> {};
    struct gigametre : prefixed_unit<gigametre, giga, metre> {};
    struct kilometre_per_second : deduced_unit<kilometre_per_second, dim_velocity, kilometre, second> {};
    struct yottagram : prefixed_unit<yottagram, yotta, gram> {};
} // namespace units

namespace brun
{
    // Some type aliases
    using position_scalar = units::si::length<units::si::gigametre>;                // Gm type
    using velocity_scalar = units::si::velocity<units::si::kilometre_per_second>;   // km/s type
    using position  = std::experimental::math::fs_vector<position_scalar, 3>;       // 3-vec of Gm
    using velocity  = std::experimental::math::fs_vector<velocity_scalar, 3>;       // 3-vec of km/s
    using mass      = units::si::mass<units::si::yottagram>;                        // Yg type
    using trail     = std::deque<position>;                                         // list of past positions
    using tag       = std::string;
    using px_radius = float;

    inline namespace literals
    {
        constexpr auto operator""_Gm(long double const value) noexcept { return position_scalar(value); }
        constexpr auto operator""_kmps(long double const value) noexcept { return velocity_scalar(value); }
        constexpr auto operator""_Yg(long double const value) noexcept { return mass(value); }
    } // namespace brun :: literals

    template <typename ET, typename OT>
    constexpr
    auto norm(std::experimental::math::vector<ET, OT> const & v) noexcept
        -> typename std::experimental::math::vector<ET,OT>::value_type
    {
        auto const sq_norm = v * v;
        if constexpr(not std::is_floating_point_v<typename std::decay<decltype(sq_norm)>::type>) {
            return units::sqrt(v * v);
        } else {
            return typename std::experimental::math::vector<ET,OT>::value_type{std::sqrt(sq_norm)};
        }
    }

    template <typename ET, typename OT>
    constexpr
    auto unit(std::experimental::math::vector<ET, OT> const & v) noexcept
    {
        return 1./norm(v) * v;
    }
} // namespace brun

#endif /* COMMON_HPP */

