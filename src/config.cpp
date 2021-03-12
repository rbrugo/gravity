/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : input
 * @created     : Thursday Feb 27, 2020 13:26:24 CET
 * @license     : MIT
 */

#include <fstream>
#include <variant>
#include <random>
#include <optional>
#include <algorithm>
#include <filesystem>

#include <toml.hpp>
#ifndef GRAVITY_NO_JSON
#   include <nlohmann/json.hpp>
#endif // GRAVITY_NO_JSON

#include <tl/expected.hpp>
// #include <range/v3/action/transform.hpp>

#include <SDLpp/color.hpp>

// #include <experimental/mdspan>
// namespace STD_LA :: detail {
// using std::experimental::dynamic_extent;
// } // namespace STD_LA :: detail
#include "common.hpp"

namespace brun
{
namespace detail
{
    // Loads from file a JSON or a TOML table
    auto load_data(std::filesystem::path const & data_path)
#ifndef GRAVITY_NO_JSON
        -> std::variant<nlohmann::json, toml::table>
#else
        -> std::variant<toml::table>
#endif
    {
        if (not std::filesystem::exists(data_path)) {
            fmt::print(stderr, "Error - can't find file {}\n", data_path);
            std::exit(1);
        }
        auto file = std::ifstream{data_path};
        if (not file.is_open()) {
            fmt::print(stderr, "Error - can't open file {}\n", data_path);
            std::exit(2);
        }

        constexpr auto tolower = [](unsigned char ch) noexcept { return std::tolower(ch); };
        // auto const ext = data_path.extension().string() | ranges::actions::transform(tolower);
        auto ext = data_path.extension().string();
        std::ranges::transform(ext, ext.begin(), tolower);
        if (ext == ".json") {
#ifndef GRAVITY_NO_JSON
            auto json = nlohmann::json{};
            file >> json;
            return json;
#else
            fmt::print(stderr, "Error - json support is not enabled\n");
            std::exit(3);
#endif
        } else if (ext == ".toml") {
            return toml::parse(file);
        } else {
            fmt::print(stderr, "Error - invalid file format (json and toml files are supported)\n");
            std::exit(3);
        }

    }

    // Builds a vector from a TOML table
    template <typename Vector>
    auto build_vector(toml::node_view<const toml::node> const & node) noexcept
        -> tl::expected<std::decay_t<Vector>, std::string_view>
    {
        using vector_type = std::decay_t<Vector>;  // extract the vector type
        using unit_type = vector_type::value_type; // extract the unit type (meters, km/s, ...)
        using expected = tl::expected<vector_type, std::string_view>; // the result type for this function
        static_assert(
            std::is_convertible_v<vector_type, brun::position> or
            std::is_convertible_v<vector_type, brun::velocity>,
            "The Vector template argument must be a position or a velocity 3-vector"
        );

        // Utility to extract a number from a node
        constexpr auto get = [](toml::node const & x) noexcept {
            return x.value<double>().value() * unit_type{1};
        };
        // Error message to show if the object is neither a scalar nor a 3-vector
        constexpr auto parse_error =
            "Error while parsing {0}: "
            "invalid content ({0} must be a scalar or a vector type of scalars with size 3)";

        if (node.is_array()) {
            auto const & arr = *node.as_array();
            if (arr.size() != 1 and arr.size() != 3) {
                return expected{tl::unexpect, parse_error};
            }
            if (not arr[0].is_number()) {
                return expected{tl::unexpect, parse_error};
            }
            if (arr.size() == 1) {
                auto const value = get(arr[0]);
                auto const zero = unit_type{0.};
                if constexpr (std::is_same_v<vector_type, brun::position>) {
                    return vector_type{zero, value, zero};
                } else {
                    return vector_type{value, zero, zero};
                }
            }
            else {
                if (not arr[1].is_number() or not arr[2].is_number()) {
                    return expected{tl::unexpect, parse_error};
                }
                return vector_type{get(arr[0]), get(arr[1]), get(arr[2])};
            }
        }
        else if (node.is_number()) {
            /* auto const value = get(node); */
            auto const value = node.value<double>().value() * unit_type{1};
            auto const zero = unit_type{0.};
            if constexpr (std::is_same_v<vector_type, brun::position>) {
                return vector_type{zero, value, zero};
            } else {
                return vector_type{value, zero, zero};
            }
        }
        return expected{tl::unexpect, parse_error};
    }

#ifndef GRAVITY_NO_JSON
    // Build a registry from a JSON table
    auto build_registry(nlohmann::json const & json) //FIXME //TODO incomplete
        -> std::pair<entt::registry, float>
    {
        using brun::literals::operator""_Gm;
        using brun::literals::operator""_kmps;
        using brun::literals::operator""_Yg;
        auto registry = entt::registry{};
        for (auto const & data : json) {
            auto const entity = registry.create();
            auto const position = brun::position{0._Gm, data["distance_from_sun [e6 km]"].get<double>() * 1._Gm, 0._Gm};
            auto const velocity = brun::velocity{data["orbital_velocity [km/s]"].get<double>() * 1._kmps, 0._kmps, 0._kmps};
            auto const mass     = brun::mass{data["mass [Yg]"].get<double>() * 1._Yg};
            auto const name     = data["name"].get<std::string>();
            auto const color    = data.value("color", 0xFFFF00);
            registry.emplace<brun::tag>(entity, name);
            registry.emplace<brun::position>(entity, position);
            registry.emplace<brun::velocity>(entity, velocity);
            registry.emplace<brun::mass>(entity, mass);
            registry.emplace<SDLpp::color>(entity, SDLpp::color{
                uint8_t((color & 0xFF0000) >> 16), uint8_t((color & 0x00FF00) >> 8), uint8_t(color & 0x0000FF)
            }); //this or without default?
        }
        return {std::move(registry), 5.f};
    }
#endif // GRAVITY_NO_JSON

    // Given a table an attribute and an optional default value, returns the content of the table if it is
    //  present and match the requested type, or the default value if the table has not an entry named after attr;
    //  if no default value is given or if the entry has a value with the wrong type, an error is returned
    // Example: `expect<double>(table, "mass")` => if `table` has a "mass" entry which is convertible to
    //  double, return that. Otherwise return an error
    // Example: `expect<int32_t>(table, "color", 0xFFFF00)` => if `table` has a "color" entry which is
    //  convertible to `int32_t` (32-bit width integer), return that. Otherwise return the int 0xFFFF00
    template <typename T>
    auto expect(auto const & table, std::string_view const attr, std::optional<T> default_val = std::nullopt)
        -> tl::expected<T, std::string>
    {
        if (not table[attr]) {
            if (default_val.has_value()) {
                return *default_val;
            }
            auto error = std::string{"no attribute \""}.append(attr).append("\" found");
            return tl::expected<double, std::string>{tl::unexpect, std::move(error)};
        }
        auto const & node = table[attr];
        if constexpr (std::is_arithmetic_v<T>) {
            return *node.template value<double>();
        } else if constexpr (std::is_convertible_v<std::string const &, T>) {
            return *node.template value<std::string>();
        } else {
            auto error = std::string{"type error occurred while parsing attribute \""}
                       .append(attr).append("\"");
            return tl::expected<double, std::string>{tl::unexpect, std::move(error)};
        }
    }


    void extract_object(
        entt::registry & registry, toml::table const & table,
        tl::expected<int32_t, std::string> const & default_trail_length,
        tl::expected<float, std::string> const & default_trail_density,
        tl::expected<int32_t, std::string> const & default_color,
        tl::expected<float, std::string> const & default_px_radius,
        brun::position const base_position = brun::position{} * 0.,
        brun::velocity const base_velocity = brun::velocity{} * 0.
    )
    {
        auto const name      = table["name"].as_string()->get();
        auto const mass      = expect<double>(table, "mass");
        auto const pos_node  = table["distance"];
        auto const vel_node  = table["orbital_velocity"]; // TODO: not needed for all objects
        auto const trail_len = expect<int32_t>(table, "motion_trail_length",  *default_trail_length);
        auto const trail_den = expect<int32_t>(table, "motion_trail_density", *default_trail_density);
        auto const color     = expect<int32_t>(table, "color", *default_color);
        auto const px_radius = expect<float>(table, "px_radius", *default_px_radius);

        auto const position = build_vector<brun::position>(pos_node);
        auto const velocity = build_vector<brun::velocity>(vel_node);

        if (not mass.has_value()) {
            fmt::print(stderr, "{}\n", mass.error());
            std::exit(5);
        }
        if (not position.has_value()) {
            fmt::print(stderr, position.error(), "position");
            fmt::print("\n");
            std::exit(6);
        }
        if (not velocity.has_value()) {
            fmt::print(stderr, velocity.error(), "velocity");
            fmt::print("\n");
            std::exit(7);
        }
        if (not px_radius.has_value()) {
            fmt::print(stderr, "{}\n", px_radius.error());
            std::exit(7);
        }
        if (*px_radius < 0) {
            fmt::print(stderr, "Error - cannot use a negative value for {} px_radius\n", name);
            std::exit(8);
        }

        // Create a new entity inside the registry and register its attributes
        fmt::print("Registered object \"{}\"\n", name);
        auto const entity = registry.create();
        registry.emplace<brun::tag>(entity, name);
        registry.emplace<brun::position>(entity, position.value() + base_position);
        registry.emplace<brun::velocity>(entity, velocity.value() + base_velocity);
        registry.emplace<brun::mass>(entity, *mass * 1._Yg);
        registry.emplace<SDLpp::color>(entity, SDLpp::color{
            uint8_t((*color & 0xFF0000) >> 16), uint8_t((*color & 0x00FF00) >> 8), uint8_t(*color & 0x0000FF)
        });
        registry.emplace<brun::px_radius>(entity, *px_radius);
        if (auto const n = trail_len.value(), d = trail_den.value(); d * n > 0) {
            auto & tail = registry.emplace<brun::trail>(entity);
            tail.resize(n * d, *position + base_position);
        }

        // Planets
        if (auto const satellites_tbl = table["satellites"].as_array(); satellites_tbl) {
            fmt::print("Registering {} satellites:\n", name);
            auto const & satellites = *satellites_tbl;
            for (auto const & subnode : satellites) {
                auto const & table = *subnode.as_table();
                extract_object(
                    registry, table,
                    default_trail_length, default_trail_density, default_color, default_px_radius,
                    *position + base_position, *velocity + base_velocity
                );
            }
        }
    }

    // Build a registry from a TOML table
    auto build_registry(toml::table const & toml)
        -> std::pair<entt::registry, float>
    {
        using brun::literals::operator""_Yg;
        auto registry = entt::registry{};

        // Get configuration
        auto const default_trail_length  = expect<int32_t>(toml["config"], "motion_trail_length", 0);
        auto const default_trail_density = expect<float>(toml["config"], "motion_trail_density", 5);
        auto const default_color         = expect<int32_t>(toml["config"], "default_color", 0xFFFFFF);
        auto const default_px_radius     = expect<float>(toml["config"], "default_px_radius", 5.);
        if (not default_color.has_value()) {
            fmt::print(stderr, "{}\n", default_color.error());
            std::exit(7);
        }
        if (not default_px_radius.has_value()) {
            fmt::print(stderr, "{}\n", default_px_radius.error());
            std::exit(7);
        }
        if (*default_px_radius < 0) {
            fmt::print(stderr, "Error - cannot use a negative value for the default px_radius\n");
            std::exit(8);
        }

        // Build planets, stars and other objects listed in the config file
        auto const & planets = *toml["object"].as_array();
        for (auto const & node : planets) {
            auto const & table = *node.as_table();
            extract_object(registry, table,
                           default_trail_length, default_trail_density,
                           default_color, default_px_radius);
        }
        return std::pair{std::move(registry), default_trail_density.value()};
    }
} // namespace detail

// Loads data from the file passed as argument and build the registry
auto load_data(std::filesystem::path const & data)
    -> std::pair<entt::registry, float>
{
    return std::visit([](auto const & table) { return detail::build_registry(table); }, detail::load_data(data));
}

} // namespace brun


