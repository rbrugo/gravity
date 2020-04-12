/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : gfx
 * @created     : Wednesday Apr 01, 2020 02:47:25 CEST
 * @license     : MIT
 */

#include "gfx.hpp"

#include <mutex>

#include <range/v3/algorithm.hpp>
#include <range/v3/numeric.hpp>
#include <range/v3/view.hpp>

#include <SDLpp/paint/shapes.hpp>

#include <GL/glew.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>

namespace brun
{

namespace
{
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    template <typename ...Components>
    inline
    auto ts_get(brun::context const & ctx, auto entt)
    {
        return std::shared_lock{ctx}, ctx.reg.get<Components...>(entt);
    }

    inline
    auto compute_origin(brun::context const & ctx)
        -> brun::position
    {
        auto const & follow = ctx.follow;
        auto const & reg = ctx.reg;
        auto lock = std::shared_lock{ctx};  // Lock the data (for safety reasons in multithreading)
        return absolute_position(reg, follow);
    }

    // Display every object whith a position, a color and a pixel radius
    auto display(brun::context const & ctx, SDLpp::renderer & renderer)
        -> std::pair<std::vector<SDLpp::paint::circle>, std::vector<SDLpp::paint::line>>
    {
        namespace rvw = ::ranges::views;
        auto const & registry = ctx.reg;
        auto const view_radius = [&ctx]{ return std::shared_lock{ctx}, ctx.view_radius; }();
        auto const [_a, _b, w, h] = renderer.size(); // get width and height
        auto const compute_displacement = [origin = compute_origin(ctx)](auto const & _1) { return _1 - origin; };

        // Rescale the vector such that the screen has "radius" 1.
        // FIXME this way one cannot see planets in the corner, outside of the circle
        auto const scale_coeff = 1. / view_radius * std::max(w, h) * 0.5;
        auto rescale = [scale_coeff](auto const & _1) { return scale_coeff * _1; };

        // Build a "circle" to be displayed
        auto to_circle = [&renderer, w, h](brun::position const & pos, SDLpp::color const color, float const radius)
        {
            auto const position = SDLpp::point2d(pos[0].count() + w/2, pos[1].count() + h/2);
            return SDLpp::paint::circle{renderer, position, radius, color};
        };

        // We are going to choose which objects to draw.
        // At first we will compute the displacement of every object from the origin, and rescale it so it
        //  will be in [0, 1] if it is contained into the screen, (1, +∞) otherwise.
        // Then we will discard every object whose rescaled displacement is greater than 1 and compute
        //  the graphics to display (the circle and the motion trail) for every survived object
        auto entities  = registry.view<brun::position const, SDLpp::color const, brun::px_radius const>();
        auto const k = std::max(w, h) * 0.5;
        auto circles = std::vector<SDLpp::paint::circle>(); circles.reserve(registry.size());
        auto lines   = std::vector<SDLpp::paint::line  >(); lines.reserve(registry.view<brun::trail const>().size());
        for (auto const entt : entities) {
            auto const pos = ts_get<brun::position>(ctx, entt);
            auto const displacement = compute_displacement(pos);
            auto const rescaled = rescale(displacement);
            if (brun::norm(rescaled) > k) {
                continue;
            }

            auto const [color, rad] = ts_get<SDLpp::color, brun::px_radius>(ctx, entt);
            auto const circle = to_circle(rescaled, color, rad);
            circles.push_back(circle);

            if (not registry.has<brun::trail>(entt)) {
                continue;
            }

            auto const [r, g, b, a] = color;
            auto const trail = registry.get<brun::trail>(entt);
            auto scaled_trail = trail
                              | rvw::transform(compute_displacement)
                              | rvw::transform(rescale)
                              ;
            auto const alpha = rvw::iota(1ul, trail.size() + 1)
                             | rvw::transform([s=trail.size()](auto const blending) -> uint8_t {
                                 return std::lerp(200., 1., blending * 1. / s);
                             })
                             ;
            auto to_line = [&renderer, w,h, r,g,b](auto const pts, auto const alpha) mutable {
                auto const [p0, p1] = pts;
                auto const x0 = double(p0[0] + w/2);
                auto const y0 = double(p0[1] + h/2);
                auto const x1 = double(p1[0] + w/2);
                auto const y1 = double(p1[1] + h/2);
                return SDLpp::paint::line{renderer, SDLpp::point2d{x0, y0}, {x1, y1}, {r, g, b, alpha}};
            };

            for (auto const [pts, alpha] : rvw::zip(rvw::zip(scaled_trail, rvw::tail(scaled_trail)), alpha)) {
                lines.push_back(to_line(pts, alpha));
            }
        }

        return std::pair{std::move(circles), std::move(lines)};
    }

    template <typename T, typename Variant>
    struct index;

    template <typename T, typename ...Types>
    struct index<T, std::variant<Types...>> {
        static constexpr size_t value = 0;
    };

    template <typename T, typename U, typename ...Types>
    struct index<T, std::variant<U, Types...>> {
        static constexpr size_t value = std::is_same_v<T, U>
                                        ? 0
                                        : (index<T, std::variant<Types...>>::value + 1)
                                        ;
    };

    template <typename Variant, typename T>
    constexpr auto index_v = index<T, Variant>::value;

    template <typename T>
    constexpr auto follow_idx = index_v<brun::follow_t, T>;

} // namespace

void draw_camera_settings(brun::context & ctx)
{
    ImGui::Begin("Camera settings");
    auto _ = std::shared_lock{ctx};
    auto const index = ctx.follow.index();
    static auto current_target = std::optional<entt::entity>{std::nullopt};
    auto const follow_com    = ImGui::RadioButton("Center of Mass", index == follow_idx<brun::follow::com>);
    auto const follow_nth    = ImGui::RadioButton("Nothing",        index == follow_idx<brun::follow::nothing>);
    auto const follow_target = ImGui::RadioButton("Target: ",       index == follow_idx<brun::follow::target>);
    ImGui::SameLine();
    auto const show_list     = ImGui::BeginCombo("", current_target.has_value()
                                                   ? ctx.reg.get<brun::tag>(*current_target).c_str()
                                                   : "");
    if (follow_com) {
        ctx.follow = brun::follow::com{};
    }
    else if (follow_nth) {
        ctx.follow = brun::follow::nothing{absolute_position(ctx.reg, ctx.follow)};
    }
    else if (follow_target and current_target.has_value()) {
        ctx.follow = brun::follow::target{*current_target};
    }
    if (show_list) {
        auto const selected = current_target.value_or(static_cast<entt::entity>(-1));
        auto const group = ctx.reg.group<brun::position const, brun::tag const>();
        for (auto const entt : group) {
            auto const & name = group.get<brun::tag const>(entt);
            if (ImGui::Selectable(name.c_str(), selected == entt)) {
                current_target = entt;
                if (index == follow_idx<brun::follow::target>) {
                    ctx.follow = brun::follow::target{entt};
                }
            }
            if (selected == entt) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

void draw_relative_distances(brun::context & ctx)
{
    static auto current_target = std::optional<entt::entity>{std::nullopt};
    static auto options = std::array<bool, 4>{true};
    auto _ = std::shared_lock{ctx};

    ImGui::Begin("Data");
    // Structure:
    // [combo - set target] { com, objs... } [checkbox - relative position] [checkbox - relative speed]
    // ["name"] ["relative position" (opt)] ["relative speed" (opt)]
    // [first]  [first pos (opt)] [first speed (opt)]

    auto const show_list = ImGui::BeginCombo("", current_target.has_value()
                                               ? ctx.reg.get<brun::tag>(*current_target).c_str()
                                               : "Center of mass");
    auto const group = ctx.reg.group<brun::position const, brun::velocity const, brun::tag const>();
    if (show_list) {
        if (ImGui::Selectable("Center of mass", not current_target.has_value())) {
            current_target = std::nullopt;
        }
        if (not current_target.has_value()) {
            ImGui::SetItemDefaultFocus();
        }
        auto const selected = current_target.value_or(static_cast<entt::entity>(-1));
        for (auto const entt : group) {
            auto const & tag = group.get<brun::tag const>(entt);
            if (ImGui::Selectable(tag.c_str(), selected == entt)) {
                current_target = entt;
            }
            if (selected == entt) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Checkbox("distance (norm)", std::addressof(options[0]));
    ImGui::SameLine();
    ImGui::Checkbox("position", std::addressof(options[1]));
    ImGui::SameLine();
    ImGui::Checkbox("velocity (norm)", std::addressof(options[2]));
    ImGui::SameLine();
    ImGui::Checkbox("velocity", std::addressof(options.at(3)));

    if (auto const count = ranges::count(options, true); count != 0) {
        ImGui::Columns(count + 1, nullptr, true); // # columns, boh, vertical separators
        // Header
        ImGui::Separator();
        ImGui::Text("name");
        ImGui::NextColumn();
        if (options[0]) {
            ImGui::Text("distance (norm)");
            ImGui::NextColumn();
        }
        if (options[1]) {
            ImGui::Text("position");
            ImGui::NextColumn();
        }
        if (options[2]) {
            ImGui::Text("velocity (norm)");
            ImGui::NextColumn();
        }
        if (options[3]) {
            ImGui::Text("velocity");
            ImGui::NextColumn();
        }

        auto const & [target_pos, target_vel] = current_target.has_value()
                                          ? group.get<brun::position const, brun::velocity const>(*current_target)
                                          : std::pair{
                                              center_of_mass(ctx.reg), center_of_mass<brun::velocity>(ctx.reg)
                                          };
        for (auto const entt : group) {
            auto const & tag = group.get<brun::tag const>(entt);
            auto const relative_pos = group.get<brun::position const>(entt) - target_pos;
            auto const relative_vel = group.get<brun::velocity const>(entt) - target_vel;
            ImGui::Separator();
            ImGui::Text("%s", tag.c_str());
            ImGui::NextColumn();
            if (options[0]) {
                auto const abs_pos = fmt::format("{}", norm(relative_pos));
                ImGui::Text("%s", abs_pos.c_str());
                ImGui::NextColumn();
            }
            if (options[1]) {
                auto const pos     = fmt::format("{}", relative_pos);
                ImGui::Text("%s", pos.c_str());
                ImGui::NextColumn();
            }
            if (options[2]) {
                auto const abs_vel = fmt::format("{}", norm(relative_vel));
                ImGui::Text("%s", abs_vel.c_str());
                ImGui::NextColumn();
            }
            if (options[3]) {
                auto const vel     = fmt::format("{}", relative_vel);
                ImGui::Text("%s", vel.c_str());
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();
    }

    ImGui::End();
}

void draw_graphics(brun::context & ctx, SDLpp::renderer & renderer, SDLpp::window const & window) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window.handler());
    ImGui::NewFrame();

    // Makes a test window
    {
        ImGui::Begin("Test window");
        ImGui::Text("Some text here");
        static bool button = false;
        if (ImGui::Button("Button")) { button = not button; }
        ImGui::Text("Button is pressed: %s\n", button ? "true " : "false");
        ImGui::Text("Current framerate: %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::End();
    }

    draw_camera_settings(ctx);

    draw_relative_distances(ctx);

    // Make the screen black
    auto const [circles, lines] = display(ctx, renderer);
    {
        ranges::for_each(lines,   &SDLpp::paint::line  ::display); // Draw motion trail first,
        ranges::for_each(circles, &SDLpp::paint::circle::display); //  then circles, on the "canvas"
    }
    ImGui::Render();
    glClearColor(0.00f, 0.00f, 0.00f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    renderer.present(); // Display the canvas (calls `SDL_GL_SwapWindow` inside)
}
} // namespace brun


