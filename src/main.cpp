/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : main
 * @created     : Saturday Jan 11, 2020 17:08:18 CET
 */

#include "common.hpp"                // for common utils - also includes entt, linear_algebra and units
#include "input.hpp"                 // for "config" file related functions
#include "gfx.hpp"                   // graphics related functions
#include "cli.hpp"                   // for `parse_cli` function (uses Lyra)
#include "keyboard.hpp"              // keyboard handling

#include <csignal>                   // signal handling    (std::signal)
#include <thread>                    // for multithreading (std::thread, std::shared_mutex)
#include <execution>                 // for parallelism    (std::execution::par_unseq)

#include <fmt/format.h>              // formatting         (fmt::print, fmt::format)
#include <fmt/chrono.h>              // compatibility with std::chrono

#include <range/v3/view/iota.hpp>    // iota function for generating ranges of numbers

namespace la = std::experimental::math;
namespace rvw = ranges::views;
using namespace units::si::literals;

namespace brun
{
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
    }
} // namespace brun

using brun::literals::operator""_Gm;
using brun::literals::operator""_kmps;
using brun::literals::operator""_Yg;

// Compute a step of simulation with a time interval of `dt` (default: 1 day)
void update(brun::context & ctx, units::si::time<units::si::day> const dt = 1.q_d)
{
    auto & reg = ctx.reg;
    //A list of objects which are movable
    auto movables = reg.group<brun::position, brun::velocity, brun::mass>();
    // A list of objects which can generate a g-field
    // NB an object doesn't need to be movable (to have a velocity) to produce a gravitational field
    // Only requirements to produce a g-field are position and mass
    // In reality, every object have a mass velocity, but in a planetary system the central star may be
    //  considered at rest (at the cost of a bit of accuracy)
    auto massives = reg.group<brun::position, brun::mass>();

    // Where updated values are stored
    struct data_node { entt::entity entity; brun::position pos; brun::velocity vel; };
    auto updated = std::vector<data_node>{}; updated.reserve(movables.size());
    auto updated_mtx = std::mutex{};
    std::for_each(std::execution::par_unseq, movables.begin(), movables.end(), [&](auto const target) {
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

        auto _ = std::lock_guard{updated_mtx};
        updated.push_back({target, r_fin, v_fin});
    });
    auto lock = std::scoped_lock{ctx};  // Lock the registry so I can write in it safely (bc multithread)
    for (auto const & [target, position, velocity] : updated) {
        reg.assign_or_replace<brun::position>(target, position);
        reg.assign_or_replace<brun::velocity>(target, velocity);
    }
}

void simulation(brun::context & ctx, units::si::time<units::si::day> const days_per_second)
{
    auto & registry = ctx.reg;
    // Some config params - some will be configurable from the config file in the future
    /// const     auto view_radius = 1.1 * std::sqrt(2) * 149.6_Gm;
    constexpr auto first_day = 1;
    constexpr auto last_day = 365;
    /// auto const fps = units::si::frequency<units::si::hertz>{60};
    constexpr auto dt = 10.q_min;        // Maximum of the simulation

    /// auto const days_per_second = 1.q_d;  // Days to compute every second - may be selected runtime
    auto const days_per_millisecond = days_per_second / 1000;

    // Computes the simulation from `first_dat` to `last_day` with a step of `dt` and a cap
    //  of `production_ratio` days each second
    auto accumulator = 24.q_h;
    // Timestep calculation for better accuracy
    // Each ms must be computed the simulation for          Δt := days_per_millisecond
    //  in small steps of size                              dt := dt
    //  so an idea is to compute                            n  := floor(Δt/dt) = floor(η)
    //  steps of simulation with duration dt each ms.
    // But Δt < dt: in those cases, η < 1 => n = 0.
    // It's better to define a new quantity dτ such that    η·dt = (n+1)·dτ
    // So we will use                                       dτ := dt·η/(n+1)
    //  and will make `n+1` steps of simulation with duration dτ each.
    auto const ratio = days_per_millisecond / dt;
    auto const n_steps = std::floor(ratio) + 1;
    auto const timestep = dt * ratio / n_steps;
    auto const total_calc_begin = std::chrono::steady_clock::now();
    fmt::print(stderr, "Δt: {}\ndt: {}\ntimestep: {}\n", days_per_millisecond, dt, timestep);

    ctx.status.store(brun::status::running);
    for (auto const day : rvw::iota(first_day, last_day)) {
        accumulator -= 24.q_h;
        brun::dump(registry, day);  // Once a day, dumps data on terminal
        if (ctx.status.load() == brun::status::stopped) {
            fmt::print(stderr, "Simulation stopped\n");
            return;
        }
        auto const day_calc_begin = std::chrono::steady_clock::now();
        do {
            auto const begin = std::chrono::steady_clock::now();
            for ([[maybe_unused]] auto _ : rvw::iota(0, n_steps)) {
                update(ctx, timestep);
                accumulator += timestep;
            }
            std::this_thread::sleep_until(begin + std::chrono::microseconds{900});

        } while (accumulator < 24.q_h);
        auto const day_calc_end = std::chrono::steady_clock::now();

        {
            auto const lock = std::scoped_lock{ctx};

            registry.view<brun::position, brun::trail>().each([](auto const & p, auto & t) {
                t.push_front(p);
                t.pop_back();
            });
        }
        auto const day_calc_time = (day_calc_end - day_calc_begin);
        auto const avg_calc_time = (day_calc_end - total_calc_begin)/(day - first_day + 1);
        fmt::print("This day calc time: {}\nAverage simulation rate: {}\n", day_calc_time, avg_calc_time);
    }
    ctx.status.store(brun::status::stopped);
    brun::dump(registry, last_day);
}

// The program entry point
int main(int argc, char const * argv[])
{
    // Takes command line arguments
    auto const & res = brun::parse_cli(argc, argv);
    if (not res) {
        if (not res.error().empty()) {
            fmt::print(stderr, "Error in parsing command line arguments: {}\n", res.error());
            std::exit(1);
        }
        std::exit(0);
    }
    auto const [days_per_second, fps, view_radius, filename] = *res;
    fmt::print("dps: {}\nfps: {}\nview radius: {}\nfilename: {}\n", days_per_second, fps, view_radius, filename);
    std::signal(SIGINT, &std::exit);

    // Init graphics
    auto mgr = SDLpp::system_manager{SDLpp::flag::init::everything};
    if (not mgr) {
        fmt::print(stderr, "Cannot init SDL: {} | {}\n",
                    std::string{SDL_GetError()}, std::string{IMG_GetError()});
        return 2;
    }

    auto window = SDLpp::window{"solar system", {1200, 900}};
    auto renderer = SDLpp::renderer{window, SDLpp::flag::renderer::accelerated};
    if (not renderer) {
        fmt::print(stderr, "Cannot create the renderer: {}\n", SDL_GetError());
        std::exit(1);
    }
    renderer.set_draw_color(SDLpp::colors::black);

    auto ctx = brun::context{};
    ctx.reg = brun::load_data(not filename.empty() ? filename : "../planets.toml"); // Registry is loaded from file
    // Creates a thread dedicated to simulation
    auto worker = std::async(std::launch::async, simulation, std::ref(ctx), days_per_second);
    // Creates a thread dedicated to keyboard input
    auto keyboard = std::async(std::launch::async, brun::keyboard_cycle, std::ref(ctx));
    // Creates a thread dedicated to graphics rendering according to `fps`
    auto graphics = std::async(std::launch::async, brun::render_cycle, std::ref(ctx), std::ref(renderer), view_radius, fps);

    worker.get();
    keyboard.get();
    graphics.get();

    return 0;
}

