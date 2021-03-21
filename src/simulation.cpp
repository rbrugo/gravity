/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : simulation
 * @created     : Tuesday Mar 24, 2020 23:51:51 CET
 * @license     : MIT
 */

#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>                 // std::for_each, std::views::iota
#include <execution>                 // for parallelism    (std::execution::par_unseq)

#include <fmt/format.h>              // formatting         (fmt::print, fmt::format)
#include <fmt/chrono.h>              // compatibility with std::chrono

#include "context.hpp"
#include "simulation.hpp"

namespace brun
{

using units::physical::si::operator""_q_d;
using units::physical::si::operator""_q_h;
using units::physical::si::operator""_q_min;
using brun::literals::operator""_Gm;
using brun::literals::operator""_kmps;
using brun::literals::operator""_Yg;

// Compute a step of simulation with a time interval of `dt` (default: 1 day)
void update(brun::context & ctx, units::physical::si::time<units::physical::si::day> const dt = 1._q_d)
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
    std::for_each(std::execution::par_unseq, movables.begin(), movables.end(), [&](auto const target) noexcept
    {
        // Function needed to compute acceleration on a target object, given his current position
        auto compute_acceleration = [&massives, &reg, target](auto const & position) mutable noexcept
        {
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
                // Workaround for ambiguous overload resolution
                auto const res       = (mass / (distance * distance)) * brun::unit(distance);
                /* auto const res       = brun::unit(distance) * mass / (distance * distance); */
                /* auto const res       = la::operator*( mass / (distance * distance), brun::unit(distance)); */
                accumulator = accumulator + res;
            }
            return - G * accumulator;
        };

        // Euler-Richardson algorithm
        auto const & [r0, v0] = reg.get<brun::position, brun::velocity>(target);
        auto const    a0 = compute_acceleration(r0);

        // step 1
        auto const v_mid = v0 + 0.5 * a0 * dt;
        auto const r_mid = r0 + 0.5 * v0 * dt;
        // step 2
        auto const a_mid = compute_acceleration(r_mid);
        // step 3
        auto const r_fin = r0 + v_mid * dt;
        auto const v_fin = v0 + a_mid * dt;

        auto _ = std::lock_guard{updated_mtx};
        updated.push_back({target, r_fin, v_fin});
    });
    auto lock = std::scoped_lock{ctx};  // Lock the registry so I can write in it safely (bc multithread)
    for (auto const & [target, position, velocity] : updated) {
        reg.emplace_or_replace<brun::position>(target, position);
        reg.emplace_or_replace<brun::velocity>(target, velocity);
    }
}

void simulation(brun::context & ctx, units::physical::si::time<units::physical::si::day> const days_per_second)
{
    auto & registry = ctx.reg;
    // Some config params - some will be configurable from the config file in the future
    /// const     auto view_radius = 1.1 * std::sqrt(2) * 149.6_Gm;
    constexpr auto first_day = 1;
    constexpr auto last_day = 365;// * 10;
    /// auto const fps = units::si::frequency<units::si::hertz>{60};
    constexpr auto dt = 10._q_min;        // Maximum of the simulation

    /// auto const days_per_second = 1.q_d;  // Days to compute every second - may be selected runtime
    auto const days_per_millisecond = days_per_second / 1000;

    // Computes the simulation from `first_dat` to `last_day` with a step of `dt` and a cap
    //  of `production_ratio` days each second
    auto accumulator = 24._q_h;
    // Timestep calculation for better accuracy
    // Each ms must be computed the simulation for          Δt := days_per_millisecond
    //  in small steps of size                              dt := dt
    //  so an idea is to compute                            n  := floor(Δt/dt) = floor(η)
    //  steps of simulation with duration dt each ms.
    // But Δt < dt: in those cases, η < 1 => n = 0.
    // It's better to define a new quantity dτ such that    η·dt = (n+1)·dτ
    // So we will use                                       dτ := dt·η/(n+1)
    //  and will make `n+1` steps of simulation with duration dτ each.
    auto const ratio = static_cast<decltype(dt)>(days_per_millisecond) / dt;
    auto const n_steps = std::floor(ratio.count()) + 1;
    auto const timestep = dt * ratio / n_steps;
    fmt::print(stderr, "Δt: {}\n", days_per_millisecond);   // Δt
    fmt::print(stderr, "dt: {}\n", dt);                     // dt
    fmt::print(stderr, "η:  {}\n", ratio);                  // η
    fmt::print(stderr, "timestep: {}\n", timestep);         // dτ
    fmt::print(stderr, "n_steps: {}\n", n_steps);           // n + 1

    ctx.status.store(brun::status::running, std::memory_order::release);
    for (auto const day : std::views::iota(first_day, last_day)) {
        accumulator -= 24._q_h;
        brun::dump(registry, day);  // Once a day, dumps data on terminal
        do {
            if (ctx.status.load(std::memory_order::acquire) == brun::status::stopped) {
                fmt::print(stderr, "Simulation stopped\n");
                return;
            }
            for ([[maybe_unused]] auto _ : std::views::iota(0, n_steps)) {
                auto const begin = std::chrono::steady_clock::now();
                update(ctx, timestep);
                accumulator = accumulator + timestep;
                // Sign, `sleep` is not precise enough
                std::this_thread::sleep_until(begin + std::chrono::microseconds{990} / (n_steps));
            }
        } while (accumulator < 24._q_h);
    }
    ctx.status.store(brun::status::stopped, std::memory_order::release);
    brun::dump(registry, last_day);
}

} // namespace brun


