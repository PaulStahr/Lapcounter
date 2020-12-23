#include "options.h"

#include <string>
#include <algorithm>
#include <istream>
#include <ostream>

namespace opt_string
{
const char* led_stripe_white    = "led_stripe_white";
const char* led_stripe_debug    = "led_stripe_debug";
const char* led_stripe_count    = "led_stripe_count";
const char* color_error         = "color_error";
const char* color_foreground    = "color_foreground";
const char* color_background    = "color_background";
const char* color_selection     = "color_selection";
const char* input_pin_          = "input_pin_";
const char* max_player          = "max_player";
}

std::istream & operator >> (std::istream & input, options_t & opt)
{
    std::string line;
    while (getline (input,line))
    {
        auto seperator = std::find(line.begin(), line.end(), ':');
        if (seperator != line.end())
        {
            std::string name(line.begin(), seperator);
            std::string value(seperator + 1, line.end());
            if (name == opt_string::led_stripe_white)       {opt._led_stripe_white  = std::strtol(value.c_str(), nullptr, 16);}
            else if (name == opt_string::led_stripe_count)  {opt._led_stripe_count  = std::strtol(value.c_str(), nullptr, 10);}
            else if (name == opt_string::led_stripe_debug)  {opt._led_stripe_debug         = value == "true";}
            else if (name == opt_string::color_error)       {opt._color_error       = std::strtol(value.c_str(), nullptr, 16);}
            else if (name == opt_string::color_foreground)  {opt._color_foreground  = std::strtol(value.c_str(), nullptr, 16);}
            else if (name == opt_string::color_background)  {opt._color_background  = std::strtol(value.c_str(), nullptr, 16);}
            else if (name == opt_string::color_selection)   {opt._color_selection   = std::strtol(value.c_str(), nullptr, 16);}
            else if (name == opt_string::max_player)        {opt._max_player        = std::stoi(value);}
            else
            {
                for (size_t i = 0; i < opt._input_pin.size(); ++i)
                {
                    if (name == opt_string::input_pin_ + std::to_string(i))
                    {
                        opt._input_pin[i] = std::stoi(value);
                        break;
                    }
                }
            }
        }
    }
    return input;
}

std::ostream & operator << (std::ostream & out, options_t const & opt)
{
    out << std::dec;
    out << opt_string::max_player << ':' << (opt._led_stripe_debug ? "true" : "false") << std::endl;
    out << opt_string::led_stripe_count << ':' << opt._led_stripe_count << std::endl;
    out << std::hex;
    out << opt_string::led_stripe_white << ':' << opt._led_stripe_white << std::endl;
    out << opt_string::color_error      << ':' << opt._color_error      << std::endl;
    out << opt_string::color_foreground << ':' << opt._color_foreground << std::endl;
    out << opt_string::color_background << ':' << opt._color_background << std::endl;
    out << opt_string::color_selection  << ':' << opt._color_selection  << std::endl;
    out << opt_string::max_player       << ':' << opt._max_player       << std::endl;
    out << std::dec;
    for (size_t i = 0; i < opt._input_pin.size(); ++i)
    {
        out << opt_string::input_pin_ << std::to_string(i) << ':' << opt._input_pin[i] << std::endl;
    }
    return out;
}
