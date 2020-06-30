/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : keyboard
 * @created     : Saturday Mar 21, 2020 01:53:53 CET
 * @license     : MIT
 * */

#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

#include <chrono>
#include <thread>
#include <mutex>
#define __cpp_lib_jthread
#ifndef __cpp_lib_jthread
#   include <jthread.hpp>
#endif // __cpp_lib_jthread

#include <SDLpp/event.hpp>

#include "common.hpp"
#include "context.hpp"

namespace brun
{

auto keyboard_cycle(brun::context & ctx)
{
    constexpr auto input_delay = std::chrono::milliseconds{10};
    while (ctx.status.load(std::memory_order::acquire) == brun::status::starting) {
        std::this_thread::yield();
    }
    while (ctx.status.load(std::memory_order::acquire) == brun::status::running) {
        auto displacement      = brun::position{};
        auto delta_view_radius = brun::position_scalar{};
        for (auto const event : SDLpp::event_range) {
            auto const input = SDLpp::match(event,
                 [](SDLpp::event_category::quit) { return +'q'; },
                 [](SDLpp::event_category::key_down key) {
                     /* switch (key.keysym.sym) { */
                     /* case 'q': */ /* quit = true; break; */
                     /* } */
                     /* return key.keysym.sym == 'q'; */
                     return key.keysym.sym;
                 },
                 [](auto) { return +'\0'; }
            );
            switch (input) {
            case +'q':
                ctx.status.store(brun::status::stopped, std::memory_order::release);
                break;
            case SDLK_LEFT:
                displacement = displacement + brun::position{+1._Gm, 0._Gm, 0._Gm};
                break;
            case SDLK_RIGHT:
                displacement = displacement + brun::position{-1._Gm, 0._Gm, 0._Gm};
                break;
            case SDLK_UP:
                displacement = displacement + brun::position{0._Gm, -1._Gm, 0._Gm};
                break;
            case SDLK_DOWN:
                displacement = displacement + brun::position{0._Gm, +1._Gm, 0._Gm};
                break;
            case '+':
            case SDLK_KP_PLUS:
                delta_view_radius -= 10._Gm;
                break;
            case '-':
            case SDLK_KP_MINUS:
                delta_view_radius += 10._Gm;
                break;
            default:
                break;
            }
            if (std::any_of(begin(displacement), end(displacement), [](auto x) { return x != 0._Gm; })) {
                auto lock = std::scoped_lock{ctx};
                std::visit([&displacement](auto & follow) { follow.offset = follow.offset + displacement; }, ctx.follow);
            }
            if (delta_view_radius != brun::position_scalar{}) {
                auto lock = std::scoped_lock{ctx};
                ctx.view_radius += delta_view_radius;
            }
        }
        std::this_thread::sleep_for(input_delay);
    }
}

} // namespace brun

#endif /* KEYBOARD_HPP */

