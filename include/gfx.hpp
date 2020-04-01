/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : gfx
 * @created     : Wednesday Apr 01, 2020 02:48:41 CEST
 * @license     : MIT
 * */

#ifndef GFX_HPP
#define GFX_HPP

#include "context.hpp"
#include <units/physical/si/frequency.h>
#include <SDLpp/texture.hpp>

namespace brun
{

void draw_graphics(brun::context const & ctx, SDLpp::renderer & renderer, SDLpp::window const & window);


} // namespace brun

#endif /* GFX_HPP */

