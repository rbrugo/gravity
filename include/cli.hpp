/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : cli
 * @created     : Friday Mar 20, 2020 21:05:52 CET
 * @license     : MIT
 * */

#ifndef CLI_HPP
#define CLI_HPP

#include <tl/expected.hpp>
#include <lyra/lyra.hpp>

#include "common.hpp"

namespace brun
{

struct simulation_params
{
    units::si::time<units::si::day> days_per_second;
    units::si::frequency<units::si::hertz> fps;
    brun::position_scalar view_radius;
    std::string filename;
};

auto parse_cli(int argc, char const * argv[]) -> tl::expected<simulation_params, std::string>;


} // namespace brun

#endif /* CLI_HPP */

