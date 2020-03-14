/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : camera
 * @created     : Saturday Mar 14, 2020 00:08:05 CET
 * @license     : MIT
 * */

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <variant>
#include <shared_mutex>

#include <entt/entt.hpp>

#include "common.hpp"

namespace brun
{

namespace follow
{
struct com{};
struct nothing{ brun::position last; };
struct target {
    target(entt::entity const tg) : id{tg} {}
    entt::entity id;
    brun::position offset;
};
} // namespace follow
using follow_t = std::variant<follow::com, follow::nothing, follow::target>;

struct context
{
    entt::registry reg;
    follow_t follow;
private:
    mutable std::shared_mutex reg_mtx;

    inline void lock()     const noexcept { reg_mtx.lock();   }
    inline bool try_lock() const noexcept { return reg_mtx.try_lock(); }
    inline void unlock()   const noexcept { reg_mtx.unlock(); }

    inline void lock_shared()     const noexcept { reg_mtx.lock_shared(); }
    inline bool try_lock_shared() const noexcept { return reg_mtx.try_lock_shared(); }
    inline void unlock_shared()   const noexcept { return reg_mtx.unlock_shared(); }
};

} // namespace brun

#endif /* CAMERA_HPP */

