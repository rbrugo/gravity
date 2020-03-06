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

// Computes the system's center of mass
auto center_of_mass(entt::registry & registry)
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
    auto objects = registry.view<brun::position, brun::mass>();
    auto const [wpos, total_mass] = ::ranges::accumulate(objects, pair{}, pairwise_sum, pos);
    return 1./total_mass * wpos;
}

// Display every object whith a position, a color and a pixel radius
void display(
    std::shared_mutex & mtx, SDLpp::renderer & renderer, entt::registry & registry,
    brun::position::value_type const view_radius)
{
    namespace rvw = ::ranges::views;
    auto const [_a, _b, w, h] = renderer.size(); // get width and height
    auto const com = center_of_mass(registry);
    auto compute_displacement = [&com](auto const & _1) { return _1 - com; }; // compute displacement from the com

    // Rescale the vector such that the screen has "radius" 1.
    // FIXME this way one cannot see planets in the corner, outside of the circle
    auto const scale_coeff = 1. / view_radius * std::max(w, h) * 0.5;
    auto rescale = [scale_coeff](auto const & _1) { return scale_coeff * _1; };

    // Build a "circle" to be displayed
    auto to_circle = [&renderer, w, h](brun::position const & pos, SDLpp::color const color, float const radius) {
        auto const position = SDLpp::point2d(pos[0].count() + w/2, pos[1].count() + h/2);
        return SDLpp::paint::circle{renderer, position, radius, color};
    };

    /**
    auto transform_first = [](auto && f) { // Utility that transform only the first component of a triple
        return rvw::transform([f=std::forward<decltype(f)>(f)](auto && t) mutable {
            auto && [first, second, third] = t;
            return std::tuple{
                f(std::forward<decltype(first)>(first)),
                std::forward<decltype(second)>(second),
                std::forward<decltype(third)>(third)
            };
        });
    };

    auto filter_first = [](auto && f) { // Utility that apply a filter to the first memeber of a tuple
        return rvw::filter([f=std::forward<decltype(f)>(f)](auto const & t) mutable {
            auto const & first = std::get<0>(t);
            return f(first);
        });
    }; */

    // Create a list of triples with (position, color, px_radius); transform the first member in the displacement
    //  relative to the COM, rescale it to fit in the screen if the magnitude is less than the max visual radius,
    //  throw away objects which are too far away, then transform the triples into graphic circles
    auto entities  = registry.view<brun::position, SDLpp::color, brun::px_radius>();

    /**
    auto positions = entities
                   | rvw::transform([&registry](auto const entt) { return registry.get<brun::position>(entt); });
    auto colors    = entities
                   | rvw::transform([&registry](auto const entt) { return registry.get<SDLpp::color>(entt); });
    auto radii     = entities
                   | rvw::transform([&registry](auto const entt) { return registry.get<brun::px_radius>(entt); });

    auto circles = rvw::zip(positions, colors, radii)
                  | transform_first(compute_displacement)
                  | transform_first(rescale)
                  | filter_first([k=std::max(w,h)*0.5](auto const & _1) { return brun::norm(_1) < k; })
                  | rvw::transform([&to_circle](auto const & t) {
                      auto const & [pos, col, rad] = t; return to_circle(pos, col, rad);
                  })
                  ;*/

    auto const k = std::max(w, h) * 0.5;
    auto circles = std::vector<SDLpp::paint::circle>(); circles.reserve(registry.size());
    auto lines   = std::vector<SDLpp::paint::line>();   lines.reserve(registry.view<brun::trail>().size());
    for (auto const entt : entities) {
        auto const pos = registry.get<brun::position>(entt);
        auto const displacement = compute_displacement(pos);
        auto const rescaled = rescale(pos);
        if (brun::norm(rescaled) > k) {
            continue;
        }

        auto const [color, rad] = registry.get<SDLpp::color, brun::px_radius>(entt);
        /* auto const x */
        auto const circle = to_circle(rescaled, color, rad);
        circles.push_back(circle);

        if (not registry.has<brun::trail>(entt)) {
            continue;
        }

        auto const [r, g, b, a] = color;
        auto const trail = registry.get<brun::trail>(entt);
        auto scaled_trail = trail
                          /* | rvw::unique */
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
        /* auto trail_blended = rvw::zip_with( */
            /* rvw::zip(scaled_trail, rvw::tail(scaled_trail)), alpha, to_line */
        /* ); */
        for (auto const [pts, alpha] : rvw::zip(rvw::zip(scaled_trail, rvw::tail(scaled_trail)), alpha)) {
            lines.push_back(to_line(pts, alpha));
        }

        /* ranges::actions::push_back(lines, trail_blended | rvw::move); */
    }


    // TODO FIXME
    // elimina il blending della scia e media il colore con il nero
    // altrimenti reimplementare velocemente bresenham
#warning LEGGI QUI RIGA 154 DI GFX.CPP
    auto line_draw = [&renderer](auto const line) {
        /* renderer.set_draw_color(SDLpp::colors::black); */
        /* auto const bg = SDLpp::paint::line{ */
            /* renderer, line.position(), line.position(), line.thickness(), SDLpp::colors::black */
        /* }; */
        /* bg.display(); */
        line.display();
    };
    // Make the screen black
    renderer.blend_mode(SDLpp::flag::blend_mode::none);
    renderer.set_draw_color(SDLpp::colors::black);
    renderer.clear();
    {
        auto lock = std::scoped_lock{mtx};  // Lock the data (for safety reasons in multithreading)
        ranges::for_each(lines,   line_draw);//&SDLpp::paint::line  ::display);
        ranges::for_each(circles, &SDLpp::paint::circle::display); // Draw circles on the "canvas"
    }
    renderer.present(); // Display the canvas
}

} // namespace brun


