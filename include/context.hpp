/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : camera
 * @created     : Saturday Mar 14, 2020 00:08:05 CET
 * @license     : MIT
 * */

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <atomic>
#include <variant>
#include <shared_mutex>

#include <entt/entt.hpp>

#include "common.hpp"

namespace brun
{

/// Represent the camera policy
namespace follow
{
struct com    { brun::position offset; }; /// Follows the center of mass
struct nothing{ brun::position offset; }; /// Stay in place
struct target                             /// Follow a target
{
    constexpr target(entt::entity const tg) : id{tg} {}
    entt::entity id;
    brun::position offset;
};
} // namespace follow
using follow_t = std::variant<follow::com, follow::nothing, follow::target>;

/// Represent the status of the simulation
enum class status : int8_t
{
    starting,   // Simulation thread is preparing
    running,    // Simulation thread has started, now other thread can follow
    stopped     // All threads have to stop
};

struct context
{
    std::atomic<brun::status> status = status::starting;
    brun::position_scalar view_radius;
    entt::registry reg;
    follow_t follow;
private:
    mutable std::shared_mutex ctx_mtx;

public:
    inline void lock()     const noexcept { ctx_mtx.lock();   }
    inline bool try_lock() const noexcept { return ctx_mtx.try_lock(); }
    inline void unlock()   const noexcept { ctx_mtx.unlock(); }

    inline void lock_shared()     const noexcept { ctx_mtx.lock_shared(); }
    inline bool try_lock_shared() const noexcept { return ctx_mtx.try_lock_shared(); }
    inline void unlock_shared()   const noexcept { return ctx_mtx.unlock_shared(); }
};

} // namespace brun

#endif /* CAMERA_HPP */

