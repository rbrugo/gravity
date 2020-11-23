/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : main
 * @created     : Saturday Jan 11, 2020 17:08:18 CET
 */

#include "common.hpp"                // for common utils - also includes entt, linear_algebra and units
#include "simulation.hpp"
#include "config.hpp"                // for "config" file related functions
#include "io.hpp"                    // graphics related functions
#include "cli.hpp"                   // for `parse_cli` function (uses Lyra)

#include <csignal>                   // signal handling    (std::signal)
#include <thread>                    // for multithreading (std::jthread)

#include <fmt/format.h>              // formatting         (fmt::print, fmt::format)

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

    auto ctx = brun::context{};
    ctx.reg = brun::load_data(not filename.empty() ? filename : "../planets.toml"); // Registry is loaded from file
    ctx.view_radius = view_radius;
    ctx.min_max_view_radius.second = [&ctx]() {
        auto const entities = ctx.reg.view<brun::position const>();
        auto const positions = entities
                             | std::views::transform([&](auto e) { return entities.get<brun::position const>(e); })
                             | std::views::transform([ ](auto const & p) { return brun::norm(p); })
                             ;
        return *std::ranges::max_element(positions);
    }();

    // Creates a thread dedicated to simulation
    auto worker = std::jthread{brun::simulation, std::ref(ctx), days_per_second};
    // Creates a thread dedicated to IO operations
    auto io = fps.count() > 0
              ? std::jthread{brun::render_cycle, std::ref(ctx), fps}
              : std::jthread{};

    return 0;
}

