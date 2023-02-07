#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>
#include <array>

struct options_t
{
    uint32_t    _led_stripe_white;
    uint32_t    _led_stripe_count;
    uint32_t    _led_stripe_start;
    uint32_t    _led_stripe_prestart;
    bool        _debug_overlay;
    uint32_t    _color_error;
    uint32_t    _color_background;
    uint32_t    _color_foreground;
    uint32_t    _color_selection;
    uint8_t     _max_player;
    std::array<uint8_t, 4> _input_pin;
};

std::istream & operator >> (std::istream & input, options_t & opt);

std::ostream & operator << (std::ostream & output, options_t const & opt);

#endif
