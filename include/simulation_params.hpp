/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : simulation_param
 * @created     : Friday Nov 27, 2020 00:56:57 CET
 * @license     : MIT
 * */

#ifndef SIMULATION_PARAM_HPP
#define SIMULATION_PARAM_HPP

#include <units/physical/si/derived/frequency.h>
#include "common.hpp"

namespace brun
{

struct simulation_params
{
    units::physical::si::time<units::physical::si::day> days_per_second;
    units::physical::si::frequency<units::physical::si::hertz> fps;
    float points_per_day;
    brun::position_scalar view_radius;
    std::string filename;
};

} // namespace brun

#endif /* SIMULATION_PARAM_HPP */

