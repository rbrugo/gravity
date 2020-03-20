/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : gfx
 * @created     : Monday Feb 17, 2020 15:33:27 CET
 * @license     : MIT
 */

#include "gfx.hpp"

#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/numeric.hpp>
#include <range/v3/view.hpp>

namespace brun
{

namespace detail
{
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
} // namespace detail

// Computes the system's center of mass
auto center_of_mass(entt::registry const & registry)
    -> brun::position
{
    using weighted_position = decltype(brun::position{} * brun::mass{});
    auto pairwise_sum = []<class T, class U>(std::pair<T, U> a, std::pair<T, U> const & b) noexcept {
        a.first = std::move(a.first) + b.first;
        a.second = std::move(a.second) + b.second;
        return a;
    };
    using pair = std::pair<weighted_position, brun::mass>;

    auto const pos = [&registry](entt::entity const entt) mutable {
        auto const [pos, mass] = registry.get<brun::position, brun::mass>(entt);
        return pair{pos * mass, mass};
    };
    auto objects = registry.view<brun::position const, brun::mass const>();
    auto const [wpos, total_mass] = ::ranges::accumulate(objects, pair{}, pairwise_sum, pos);
    return 1./total_mass * wpos;
}

auto compute_origin(brun::context const & ctx)
    -> brun::position
{
    auto const & follow = ctx.follow;
    auto const & reg = ctx.reg;
    return std::visit(detail::overloaded{
        [](follow::nothing stay) {
            return stay.last;
        },
        [&reg](follow::com com) {
            return center_of_mass(reg) + com.offset;
        },
        [&reg](follow::target target) {
            return reg.get<brun::position>(target.id) + target.offset;
        }
    }, follow);
}

// Display every object whith a position, a color and a pixel radius
void display(
    brun::context const & ctx, SDLpp::renderer & renderer, brun::position::value_type const view_radius
    )
{
    namespace rvw = ::ranges::views;
    auto const & registry = ctx.reg;
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
        auto const pos = registry.get<brun::position>(entt);
        auto const displacement = compute_displacement(pos);
        auto const rescaled = rescale(displacement);
        if (brun::norm(rescaled) > k) {
            continue;
        }

        auto const [color, rad] = registry.get<SDLpp::color, brun::px_radius>(entt);
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

    // Make the screen black
    renderer.blend_mode(SDLpp::flag::blend_mode::none);
    renderer.set_draw_color(SDLpp::colors::black);
    renderer.clear();
    {
        auto lock = std::scoped_lock{ctx.reg_mtx};  // Lock the data (for safety reasons in multithreading)
        ranges::for_each(lines,   &SDLpp::paint::line  ::display); // Draw motion trail first,
        ranges::for_each(circles, &SDLpp::paint::circle::display); //  then circles, on the "canvas"
    }
    renderer.present(); // Display the canvas
}

} // namespace brun


