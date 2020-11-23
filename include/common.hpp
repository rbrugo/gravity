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

#include <range/v3/numeric/accumulate.hpp>

namespace la = STD_LA;

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

template <typename T, typename N>
struct fmt::formatter<la::vector<T, N>>
{
    char specifier = '\0';
    std::uint8_t precision = -1;
    std::uint8_t align_width = -1;
    char align_spec = '\0';

    constexpr auto parse_int(format_parse_context::iterator it, std::uint8_t & property)
    {
        property = 0;
        while (*it >= '0' and *it <= '9') {
            property = property * 10 - '0' + *it;
            ++it;
        }
        return it;
    }

    constexpr auto parse(format_parse_context & ctx)
    {
        auto it = ctx.begin(), end = ctx.end();
        while (it != end and *it != '}') {
            switch (*it) {
            case '^':
            case '<':
            case '>':
                align_spec = *it;
                it = parse_int(std::next(it), align_width);
                break;
            case '.':
                it = parse_int(std::next(it), precision);
                break;
            case 'e':
            case 'E':
            case 'g':
            case 'G':
                specifier = *it;
                break;
            default:
                throw fmt::format_error{"invalid specifier"};
            }
        }
        return it;
    }

    template <typename FormatContext>
    auto format(la::vector<T, N> const & v, FormatContext & ctx)
    {
        auto quantity = fmt::format(
            "{}:%{}{}Q{}", '{',
            precision != static_cast<uint8_t>(-1) ? fmt::format(".{}", precision) : "",
            specifier != '\0' ? fmt::format("{}", specifier) : "",
        '}');
        // "{:%.3gQ}"
        auto format_string = fmt::format("({0} {0} {0}) {1}", std::move(quantity), "{:%q}");
        // "({:%.3gQ} {:%.3gQ} {:%.3gQ}) {:%q}"
        auto formatted     = fmt::format(format_string, v[0], v[1], v[2], v[2]);
        // "(12.3 13 13) m"

        auto align_string = fmt::format("{}:{}{}", '{',
            align_spec != '\0' ? fmt::format("{}{}", align_spec, align_width) : "",
        '}');
        // "{:^3}"

        return format_to(ctx.out(), align_string, formatted);
        // fmt::format("{:^3}", "(12.3 13 13) m")
    }
};

namespace units::physical::si
{
    // Some new SI units type definitions
    /* struct megametre : prefixed_unit<megametre, mega, metre> {}; */
    /* struct gigametre : prefixed_unit<gigametre, giga, metre> {}; */
    struct kilometre_per_second : deduced_unit<kilometre_per_second, dim_speed, kilometre, second> {};
    /* struct yottagram : prefixed_unit<yottagram, yotta, gram> {}; */
} // namespace units::si

namespace brun
{
    // Some type aliases
    using position_scalar = units::physical::si::length<units::physical::si::gigametre>;      // Gm type
    using velocity_scalar = units::physical::si::speed<units::physical::si::kilometre_per_second>;   // km/s type
    using position  = la::fs_vector<position_scalar, 3>;       // 3-vec of Gm
    using velocity  = la::fs_vector<velocity_scalar, 3>;       // 3-vec of km/s
    using mass      = units::physical::si::mass<units::physical::si::yottagram>;                        // Yg type
    using trail     = std::deque<position>;                                         // list of past positions
    using tag       = std::string;
    using px_radius = float;
    using rotation_matrix = la::fs_matrix<brun::position_scalar::rep, 3, 3>;

    struct rotation_info
    {
        constexpr rotation_info() = default;
        constexpr rotation_info(uint8_t a, uint8_t b) : z_axis{a}, x_axis{b} {};
        uint8_t z_axis = 0;
        uint8_t x_axis = 0;
        inline auto operator-() const { return rotation_info(256 - z_axis, 256 - x_axis) ; };
    };

    inline namespace literals
    {
        constexpr auto operator""_Gm(long double const value) noexcept { return position_scalar(value); }
        constexpr auto operator""_kmps(long double const value) noexcept { return velocity_scalar(value); }
        constexpr auto operator""_Yg(long double const value) noexcept { return mass(value); }
    } // namespace brun :: literals

    template <typename ET, typename OT>
    constexpr
    auto norm(la::vector<ET, OT> const & v) noexcept
        -> typename la::vector<ET,OT>::value_type
    {
        auto const sq_norm = v * v;
        if constexpr(not std::is_floating_point_v<typename std::decay<decltype(sq_norm)>::type>) {
            return units::sqrt(sq_norm);
        } else {
            return typename la::vector<ET,OT>::value_type{std::sqrt(sq_norm)};
        }
    }

    template <typename ET, typename OT>
    constexpr
    auto unit(la::vector<ET, OT> const & v) noexcept
    {
        return 1./norm(v) * v;
    }

    // Computes the system's center of mass
    template <typename Component = brun::position>
    auto center_of_mass(entt::registry const & registry)
        -> Component
    {
        using weighted_position = decltype(Component{} * brun::mass{});
        auto pairwise_sum = []<class T, class U>(std::pair<T, U> a, std::pair<T, U> const & b) noexcept {
            a.first = std::move(a.first) + b.first;
            a.second = std::move(a.second) + b.second;
            return a;
        };
        using pair = std::pair<weighted_position, brun::mass>;

        auto const pos = [&registry](entt::entity const entt) mutable {
            auto const [pos, mass] = registry.get<Component, brun::mass>(entt);
            return pair{pos * mass, mass};
        };
        auto objects = registry.view<Component const, brun::mass const>();
        auto const [wpos, total_mass] = ::ranges::accumulate(objects, pair{}, pairwise_sum, pos);
        return 1./total_mass * wpos;
    }

    auto build_rotation_matrix(rotation_info const info) -> brun::rotation_matrix;
    auto build_reversed_rotation_matrix(rotation_info const info) -> brun::rotation_matrix;

} // namespace brun

#endif /* COMMON_HPP */

