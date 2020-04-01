/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : gfx
 * @created     : Monday Feb 17, 2020 15:33:27 CET
 * @license     : MIT
 */

#include "gfx.hpp"

#include <mutex>

#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/numeric.hpp>
#include <range/v3/view.hpp>

#include <SDLpp/system_manager.hpp>
#include <SDLpp/texture.hpp>
#include <SDLpp/window.hpp>
#include <SDLpp/event.hpp>
#include <SDLpp/paint/shapes.hpp>

#include <GL/glew.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>

#include <fmt/color.h>

namespace brun
{

namespace detail
{
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    // Wraps an object and a function to launch at the end of his lifetime
    template <typename T, typename F>
    class raii_wrapper
    {
        using wrapped_type = std::decay_t<T>;
        using destructor_type = std::decay_t<F>;
        wrapped_type    _obj;
        destructor_type _at_exit;

    public:
        template <typename U = wrapped_type, typename G = destructor_type>
        constexpr raii_wrapper(U && u, G && g) : _obj{std::forward<U&&>(u)}, _at_exit{std::forward<G&&>(g)} {;}
        inline ~raii_wrapper() {
            if constexpr (std::is_invocable_v<destructor_type, wrapped_type>) {
                _at_exit(_obj);
            } else {
                _at_exit();
            }
        }

        constexpr auto handler()       noexcept -> T       & { return _obj; }
        constexpr auto handler() const noexcept -> T const & { return _obj; }
    };
    template <typename T, typename U>
    raii_wrapper(T && t, U && u) -> raii_wrapper<T, U>;

    // ...set gl attributes
    void sdl_gl_set_attributes()
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        // Window with graphics context
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    }


    auto init_imgui(SDLpp::window & window, auto & gl_context)
        -> decltype((ImGui::GetIO()))
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        constexpr auto glsl_version = "#version 430";
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window.handler(), gl_context);
        ImGui_ImplOpenGL3_Init(glsl_version);
        return ImGui::GetIO();
    }

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
    auto lock = std::shared_lock{ctx};  // Lock the data (for safety reasons in multithreading)
    return std::visit(detail::overloaded{
        [](follow::nothing stay) {
            return stay.offset;
        },
        [&reg](follow::com com) {
            return center_of_mass(reg) + com.offset;
        },
        [&reg](follow::target target) {
            return reg.get<brun::position>(target.id) + target.offset;
        }
    }, follow);
}

template <typename ...Components>
inline
auto ts_get(brun::context const & ctx, auto entt)
{
    return std::shared_lock{ctx}, ctx.reg.get<Components...>(entt);
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

void update_trail(brun::context & ctx)
{
    auto & registry = ctx.reg;
    auto const lock = std::scoped_lock{ctx};

    registry.view<brun::position, brun::trail>().each([](auto const & p, auto & t) {
        t.push_front(p);
        t.pop_back();
    });
}

void render_cycle(
    brun::context & ctx,
    units::si::frequency<units::si::hertz> const fps
) noexcept
{
    using namespace units::si::literals;
    auto const freq = fps * 1q_s / 1q_us;
    auto const time_for_frame = std::chrono::microseconds{int((1./freq).count())}; // FIXME is this correct?


    // Init SDL graphics
    auto mgr = SDLpp::system_manager{SDLpp::flag::init::everything};
    if (not mgr) {
        fmt::print(stderr, "Cannot init SDL: {} | {}\n",
                    std::string{SDL_GetError()}, std::string{IMG_GetError()});
        return;
    }
    detail::sdl_gl_set_attributes();

    auto const flags = SDLpp::flag::window::opengl | SDLpp::flag::window::allow_highDPI; // resizable?
    auto window = SDLpp::window{"solar system", {1200, 900}, flags};
    auto renderer = SDLpp::renderer{window, SDLpp::flag::renderer::accelerated};
    if (not renderer) {
        fmt::print(stderr, "Cannot create the renderer: {}\n", SDL_GetError());
        std::exit(1);
    }
    renderer.set_draw_color(SDLpp::colors::black);

    /* int minor, major; */
    /* SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major); */
    /* SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor); */
    /* fmt::print(stderr, "Version {}.{}\n", major, minor); */
    /* fmt::print(stderr, "Version {}\n", glGetString(GL_VERSION)); */

    // Init OpenGL
    // NB: OpenGL is already initialized!
    // See:
    // https://discourse.libsdl.org/t/mixing-opengl-and-renderer/19946/19
    //
    // auto gl_context = detail::raii_wrapper{SDL_GL_CreateContext(window.handler()), SDL_GL_DeleteContext};
    auto gl_context = SDL_GL_GetCurrentContext();
    SDL_GL_MakeCurrent(window.handler(), gl_context);
    SDL_GL_SetSwapInterval(1); // enable vsync

    if (glewInit() != GLEW_OK) {
        fmt::print(stderr, "Failed to load OpenGL loader!\n");
        std::exit(1); //TODO
    }

    // Init Dear ImGUI
    [[maybe_unused]] auto & io = detail::init_imgui(window, gl_context);
    /* auto const imgui_clear_color = ImVec4{0.45f, 0.55f, 0.60f, 1.00f}; */
    auto const imgui_clear_color = ImVec4{0.f, 0.f, 0.f, 1.00f};

    // Wait for the simulation
    while (ctx.status.load() == brun::status::starting) {
        std::this_thread::yield();
    }
    int_fast32_t count = 0;
    while (ctx.status.load() == brun::status::running) {
        auto const end = std::chrono::system_clock::now() + time_for_frame;
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
            ImGui::End();
        }

        // Make the screen black
        if (++count == fps.count() / 10) {update_trail(ctx); count = 0;}
        auto const [circles, lines] = display(ctx, renderer);
        {
            ranges::for_each(lines,   &SDLpp::paint::line  ::display); // Draw motion trail first,
            ranges::for_each(circles, &SDLpp::paint::circle::display); //  then circles, on the "canvas"
        }
        ImGui::Render();
        glClearColor(imgui_clear_color.x, imgui_clear_color.y, imgui_clear_color.z, imgui_clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        renderer.present(); // Display the canvas (calls `SDL_GL_SwapWindow` inside)

        // framerate
        std::this_thread::sleep_until(end);
    }

    // CleanUp
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    fmt::print(stderr, "GFX Finished\n");
}

} // namespace brun


