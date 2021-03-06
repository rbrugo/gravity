/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : io
 * @created     : Monday Feb 17, 2020 15:33:41 CET
 * @license     : MIT
 * */

#ifndef IO_HPP
#define IO_HPP

#include <thread>
#include <shared_mutex>

#include <entt/fwd.hpp>

#include <SDLpp/texture.hpp>

#include <units/physical/si/base/time.h>
#include <units/physical/si/derived/frequency.h>

#include "common.hpp"
#include "context.hpp"
#include "simulation_params.hpp"

namespace brun
{

auto center_of_mass(entt::registry & registry) -> brun::position;
void display(brun::context const &, SDLpp::renderer &, brun::position::value_type const max_radius);

// Generates a function which must be invoked in a thread, which refresh the graphics at a certain rate
void render_cycle(brun::context & ctx, simulation_params const & params) noexcept;

} // namespace brun

#endif /* IO_HPP */

