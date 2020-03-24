/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : simulation
 * @created     : Tuesday Mar 24, 2020 23:51:51 CET
 * @license     : MIT
 * */

#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <units/physical/si/length.h>
#include <units/physical/si/mass.h>
#include <units/physical/si/force.h>
#include <units/physical/si/time.h>

namespace brun
{

class context;

inline namespace constants
{
    template <
        typename Length = units::si::length<units::si::metre>,
        typename Mass   = units::si::mass<units::si::kilogram>,
        typename Force  = units::si::force<units::si::newton>
    >
    using G_type = decltype(Force{} * Length{} * Length{} / (Mass{} * Mass{}));
    template <
        typename Length = units::si::length<units::si::metre>,
        typename Mass   = units::si::mass<units::si::kilogram>,
        typename Force  = units::si::force<units::si::newton>
    > constexpr inline
    auto G = G_type<Length, Mass, Force>{G_type<>{6.67e-11}};
} // namespace constants

void simulation(brun::context & ctx, units::si::time<units::si::day> const days_per_second);

} // namespace brun

#endif /* SIMULATION_HPP */

