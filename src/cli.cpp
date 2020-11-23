/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : cli
 * @created     : Saturday Mar 21, 2020 01:30:20 CET
 * @license     : MIT
 */

#include "cli.hpp"

auto brun::parse_cli(int argc, char const * argv[])
    -> tl::expected<simulation_params, std::string>
{
    bool show_help = false;
    int days_per_second = 1;
    int fps = 60;
    double view_radius = 1.1 * std::sqrt(2) * 149.6;//11403.3;
    std::string filename;

    auto cli = lyra::help(show_help)
             | lyra::arg(filename, "dataset path")("path to the dataset")
             | lyra::opt(days_per_second, "days per second")["-d"]["--dps"]["--days-per-second"]
                        ("How many days must be simulated each second")
             | lyra::opt(fps, "framerate")["-f"]["--fps"]["--framerate"]
                        ("Graphics framerate -- 0 to disable graphics")
             | lyra::opt(view_radius, "view radius")["-r"]["--radius"]
                        ("Default view radius")
             ;
    auto const result = cli.parse({argc, argv});
    if (not result) {
        return tl::expected<simulation_params, std::string>{tl::unexpect, result.errorMessage()};
    }
    if (show_help) {
        fmt::print("{}\n", cli);
        return tl::expected<simulation_params, std::string>{tl::unexpect, ""};
    }
    auto params = simulation_params{
        units::physical::si::time<units::physical::si::day>{days_per_second},
        units::physical::si::frequency<units::physical::si::hertz>{fps},
        brun::position_scalar{view_radius},
        std::move(filename)
    };
    return params;//return tl::expected<simulation_params, std::string>
}

