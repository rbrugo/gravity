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
inline
auto render_cycle(
    brun::context const & ctx,
    SDLpp::renderer & renderer,
    brun::position::value_type const & max_radius, units::si::frequency<units::si::hertz> const fps
) noexcept
{
    using namespace units::si::literals;
    auto const freq = fps * 1q_s / 1q_us;
    auto const time_for_frame = std::chrono::microseconds{int((1./freq).count())}; // FIXME is this correct?
    return [&ctx, &renderer, &max_radius, time_for_frame](std::stop_token token) mutable {
        while (not token.stop_requested()) {
            auto const end = std::chrono::system_clock::now() + time_for_frame;
            display(ctx, renderer, max_radius);

            std::this_thread::sleep_until(end);
        }
    };
}

} // namespace brun

#endif /* GFX_HPP */

