/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : input
 * @created     : Thursday Feb 27, 2020 13:42:31 CET
 * @license     : MIT
 * */

#ifndef INPUT_HPP
#define INPUT_HPP

#include <filesystem>
#include <entt/entt.hpp>

namespace brun
{

auto load_data(std::filesystem::path const & data) -> entt::registry;

} // namespace brun

#endif /* INPUT_HPP */

