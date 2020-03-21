/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : gfx
 * @created     : Monday Feb 17, 2020 15:33:41 CET
 * @license     : MIT
 * */

#ifndef GFX_HPP
#define GFX_HPP

#include <thread>
#include <shared_mutex>

#include <entt/fwd.hpp>

#include <SDLpp/system_manager.hpp>
#include <SDLpp/texture.hpp>
#include <SDLpp/window.hpp>
#include <SDLpp/event.hpp>
#include <SDLpp/paint/shapes.hpp>

#include <units/physical/si/time.h>
#include <units/physical/si/frequency.h>

#include "common.hpp"
#include "context.hpp"

#ifndef __cpp_lib_jthread
#   include <jthread.hpp>
#endif // __cpp_lib_jthread

namespace brun
{

auto center_of_mass(entt::registry & registry) -> brun::position;
void display(brun::context const &, SDLpp::renderer &, brun::position::value_type const max_radius);

// Generates a function which must be invoked in a thread, which refresh the graphics at a certain rate
void render_cycle(
    brun::context & ctx,
    SDLpp::renderer & renderer,
    brun::position::value_type const & max_radius, units::si::frequency<units::si::hertz> const fps
) noexcept;

} // namespace brun

#endif /* GFX_HPP */

