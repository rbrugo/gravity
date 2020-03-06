/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : main
 * @created     : Saturday Jan 11, 2020 17:08:18 CET
 */

#include "input.hpp"                 // for "config" file related functions
#include "common.hpp"                // for common utils
#include "gfx.hpp"                   // graphics related functions

#include <csignal>                   // signal handling
#include <thread>                    // for multithreading
#include <iostream>                  // terminal input-output

#include <fmt/format.h>              // formatting functions

#include <linear_algebra.hpp>        // vectors
#include <entt/entt.hpp>             // entity component system - where the data is stored

#include <range/v3/view/iota.hpp>    // iota function for generating ranges of numbers

namespace la = std::experimental::math;
namespace rvw = ranges::views;
using namespace units::si::literals;

namespace brun
{
    inline namespace constants
    {
        constexpr auto c = 299792458q_mps;
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
    }
} // namespace brun

using brun::literals::operator""_Gm;
using brun::literals::operator""_kmps;
using brun::literals::operator""_Yg;

// Compute a step of simulation with a time interval of `dt` (default: 1 day)
void update(std::shared_mutex & mtx, entt::registry & reg, units::si::time<units::si::day> const dt = 1.q_d)
{
    // Where updated values are stored
    auto updated = std::map<entt::entity, std::pair<brun::position, brun::velocity>>{};

    //A list of objects which are movable
    auto movables = reg.view<brun::position, brun::velocity, brun::mass>();
    // A list of objects which can generate a g-field
    // NB an object doesn't need to be movable (to have a velocity) to produce a gravitational field
    // Only requirements to produce a g-field are position and mass
    // In reality, every object have a mass velocity, but in a planetary system the central star may be
    //  considered at rest (at the cost of a bit of accuracy)
    auto massives = reg.view<brun::position, brun::mass>();
    for (auto const target : movables) { // Iterate each object and compute his acceleration
        // Function needed to compute acceleration on a target object, given his current position
        auto compute_acceleration = [&massives, &reg](auto const target, auto const & position) mutable {
            using mass_on_sq_dist = la::fs_vector<decltype(1._Yg/(1._Gm*1._Gm)), 3>;
            constexpr auto G = brun::constants::G<brun::position, brun::mass>;

            // Computes the sum of the fields in the target position
            auto accumulator = mass_on_sq_dist{};
            for (auto const other : massives) {
                if (other == target) {
                    continue;
                }
                auto const distance  = position - reg.get<brun::position>(other);
                auto const unit      = brun::unit(distance);
                auto const mass      = reg.get<brun::mass>(other);
                auto const res       = (mass / (distance * distance)) * brun::unit(distance);
                accumulator = accumulator + res;
            }
            return - G * accumulator;
        };

        // Euler-Richardson algorithm
        auto const & [r0, v0] = reg.get<brun::position, brun::velocity>(target);
        auto const   a0 = compute_acceleration(target, r0);

        // step 1
        auto const v_mid = v0 + 0.5 * a0 * dt;
        auto const r_mid = r0 + 0.5 * v0 * dt;
        // step 2
        auto const a_mid = compute_acceleration(target, r_mid);
        // step 3
        auto const r_fin = r0 + v_mid * dt;
        auto const v_fin = v0 + a_mid * dt;

        updated.insert_or_assign(target, std::make_pair(r_fin, v_fin));
    }
    auto lock = std::shared_lock{mtx};  // Lock the registry so I can write in it safely (bc multithread)
    for (auto const & [target, vectors] : updated) {
        auto const & [position, velocity] = vectors;
        reg.replace<brun::position>(target, position);
        reg.replace<brun::velocity>(target, velocity);
    }
}

// The program entry point
int main(int argc, char * argv[])
{
    // Take the config file from command line (or the default one)
    auto const filename = argc > 1 ? std::string{argv[1]} : std::string{"../planets.toml"};
    std::signal(SIGINT, &std::exit);

    // Init graphics
    auto mgr = SDLpp::system_manager{SDLpp::flag::init::everything};
    if (not mgr) {
        brun::print(std::cerr, "Cannot init SDL: {} | {}\n",
                    std::string{SDL_GetError()}, std::string{IMG_GetError()});
        return 2;
    }

    auto window = SDLpp::window{"solar system", {1200, 900}};
    auto renderer = SDLpp::renderer{window, SDLpp::flag::renderer::accelerated};
    if (not renderer) {
        brun::print(std::cerr, "Cannot create the renderer: {}\n", SDL_GetError());
        std::exit(1);
    }
    renderer.set_draw_color(SDLpp::colors::black);

    // Some config params - some will be configurable from the config file in the future
    const     auto view_radius = 1.1 * std::sqrt(2) * 149.6_Gm;
    constexpr auto first_day = 1;
    constexpr auto last_day = 365;
    constexpr auto fps = units::si::frequency<units::si::hertz>{60};
    constexpr auto dt = 1q_min;
    auto const production_ratio = 2.q_d/1q_s;     // May be selected runtime

    auto registry = brun::load_data(filename); // Registry is loaded from file

    auto mtx = std::shared_mutex{};
    // Creates a thread dedicated to graphics rendering according to `fps`
    auto worker = std::jthread{brun::render_cycle(mtx, renderer, registry, view_radius, fps)};
    // Computes the simulation from `first_dat` to `last_day` with a step of `dt` and a cap
    //  of `production_ratio` days each second
    for (auto const day : rvw::iota(first_day, last_day)) {
        brun::dump(registry, day);  // Once a day, dumps data on terminal
        auto accumulator = decltype(dt){0};
        do {
            const auto begin = std::chrono::system_clock::now();
            auto sub_accumulator = decltype(dt){0};
            do {
                update(mtx, registry, dt);
                sub_accumulator += dt;
            } while (sub_accumulator < production_ratio * 1.q_s / 1000);
            accumulator += sub_accumulator;
            std::this_thread::sleep_until(begin + std::chrono::milliseconds{1});
        } while (accumulator < 24.q_h);
        auto const lock = std::scoped_lock{mtx};
        /* auto const idx = day - first_day; */
        registry.view<brun::position, brun::trail>().each([](auto const & p, auto & t) {
            t.push_front(p);
            t.pop_back();
            /* auto const i = idx % t.size(); */
            /* t[i] = p; */
        });
    }
    brun::dump(registry, last_day);
    [[maybe_unused]] auto const stopped = worker.request_stop();

    return 0;
}

