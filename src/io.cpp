/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : io
 * @created     : Monday Feb 17, 2020 15:33:27 CET
 * @license     : MIT
 */

#include "io.hpp"
#include "gfx.hpp"

#include <mutex>

#include <range/v3/algorithm.hpp>
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
} // namespace detail

namespace
{
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

    auto io_events(brun::context & ctx)
    {
        constexpr auto input_delay = std::chrono::milliseconds{10};
        auto displacement      = brun::position{};
        auto delta_view_radius = brun::position_scalar{};
        for (auto const event : SDLpp::event_range) {
            ImGui_ImplSDL2_ProcessEvent(std::addressof(event.handler()));
            auto const input = SDLpp::match(event,
                 [](SDLpp::event_category::quit) { return +'q'; },
                 [](SDLpp::event_category::key_down key) {
                     /* switch (key.keysym.sym) { */
                     /* case 'q': */ /* quit = true; break; */
                     /* } */
                     /* return key.keysym.sym == 'q'; */
                     return key.keysym.sym;
                 },
                 [](auto) { return +'\0'; }
            );
            switch (input) {
            case +'q':
                ctx.status.store(brun::status::stopped);
                break;
            case SDLK_LEFT:
                displacement = displacement + brun::position{-1._Gm, 0._Gm, 0._Gm};
                break;
            case SDLK_RIGHT:
                displacement = displacement + brun::position{+1._Gm, 0._Gm, 0._Gm};
                break;
            case SDLK_UP:
                displacement = displacement + brun::position{0._Gm, -1._Gm, 0._Gm};
                break;
            case SDLK_DOWN:
                displacement = displacement + brun::position{0._Gm, +1._Gm, 0._Gm};
                break;
            case '+':
            case SDLK_KP_PLUS:
                delta_view_radius -= 10._Gm;
                break;
            case '-':
            case SDLK_KP_MINUS:
                delta_view_radius += 10._Gm;
                break;
            default:
                break;
            }
            if (std::any_of(begin(displacement), end(displacement), [](auto x) { return x != 0._Gm; })) {
                auto lock = std::scoped_lock{ctx};
                std::visit([&displacement](auto & follow) { follow.offset = follow.offset + displacement; }, ctx.follow);
            }
            if (delta_view_radius != brun::position_scalar{}) {
                auto lock = std::scoped_lock{ctx};
                ctx.view_radius += delta_view_radius;
            }
        }
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
} // namespace


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
    sdl_gl_set_attributes();

    auto const flags = SDLpp::flag::window::opengl | SDLpp::flag::window::allow_highDPI; // resizable?
    auto window = SDLpp::window{"solar system", {1200, 900}, flags};
    auto renderer = SDLpp::renderer{window, SDLpp::flag::renderer::accelerated};
    if (not renderer) {
        fmt::print(stderr, "Cannot create the renderer: {}\n", SDL_GetError());
        std::exit(1);
    }
    renderer.set_draw_color(SDLpp::colors::black);

    // Init OpenGL
    // NB: OpenGL is already initialized!
    // See:
    // https://discourse.libsdl.org/t/mixing-opengl-and-renderer/19946/19
    auto gl_context = SDL_GL_GetCurrentContext();
    SDL_GL_MakeCurrent(window.handler(), gl_context);
    SDL_GL_SetSwapInterval(1); // enable vsync

    if (glewInit() != GLEW_OK) {
        fmt::print(stderr, "Failed to load OpenGL loader!\n");
        std::exit(1); //TODO
    }

    // Init Dear ImGUI
    [[maybe_unused]] auto & io = init_imgui(window, gl_context);

    // Wait for the simulation
    while (ctx.status.load() == brun::status::starting) {
        std::this_thread::yield();
    }
    int_fast16_t count = 0;
    while (ctx.status.load() == brun::status::running) {
        auto const end = std::chrono::system_clock::now() + time_for_frame;
        io_events(ctx);
        if (++count == fps.count() / 10) {update_trail(ctx); count = 0;}
        draw_graphics(ctx, renderer, window);

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


