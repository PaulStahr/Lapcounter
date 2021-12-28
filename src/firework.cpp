/*
Copyright (c) 2019 Paul Stahr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "firework.h"

color_t::color_t::color_t(uint8_t r_, uint8_t g_, uint8_t b_) : _r(r_), _g(g_), _b(b_){}

color_t::color_t::color_t(uint32_t col) : _r(col), _g(col >> 8), _b(col >> 16){}

particle_t::particle_t(std::array<int32_t, 2> const & position_, std::array<int32_t, 2> const & direction_, uint32_t livetime_) : _position(position_), _direction(direction_), _livetime(livetime_){}


bool particle_t::operator()(int32_t gravitation)
{
    if (_livetime != 0)
    {
        std::transform(_direction.begin(), _direction.end(), _position.begin(), _position.begin(), std::plus<int32_t>());
        _direction[1] -= gravitation;
        //std::cout << '(' << (_position[0] >> 16) << ',' << (_position[1] >> 16) << ')' << '(' << _direction[0] << ',' << _direction[1] << ')' << std::endl;
        --_livetime;
        return true;
    }
    return false;
}

bool rocket_t::operator()(bool & create_explosion, int32_t gravitation)
{
    if (_rocket._direction[1] > 0)
    {
        _rocket(gravitation);
        create_explosion = _rocket._direction[1] <= 0;
        _trail.push_back(_rocket._position);
        if (_trail.size() > 10)
        {
            _trail.pop_front();
        }
    }
    else
    {
        create_explosion = false;
        if (!_trail.empty())
        {
            _trail.pop_front();
        }
    }
    return !_trail.empty();
}

explosion_t::explosion_t(size_t num_particles_, std::array<int32_t, 2> const & position_, std::array<int32_t, 2> const & direction_, uint32_t maxlen, uint32_t timestep, std::mt19937_64 gen) : _color(gen())
{
    _particles.reserve(num_particles_);
    for (size_t i = 0; i < num_particles_; ++i)
    {
        double arc = (gen() % 1024) * M_PI * 2 / 1024;
        uint32_t len = gen() % (maxlen * timestep);// + 100000;
        size_t livetime = (gen() % 1000 + 1000) / timestep;
        _particles.emplace_back(position_, std::array<int32_t, 2>({direction_[0] + int32_t(len*sin(arc)), direction_[1] + int32_t(len*cos(arc))}), livetime);
    }
}

bool explosion_t::operator()(int32_t gravitation)
{
    auto write = _particles.begin();
    for (auto read = _particles.begin(); read != _particles.end(); ++read)
    {
        if ((*read)(gravitation))
        {
            *write = *read;
            ++write;
        }
    }
    _particles.erase(write, _particles.end()); 
    return !_particles.empty();
}

firework_t::firework_t(size_t width_, size_t height_, uint32_t timestep_) : _vertical_dir(0, height_ >> 7), _horizontal_dir(0, 0x10000), _horizontal_pos(0, width_), _height(height_),  _timestep(timestep_),_gravitation(timestep_ * 20){}

void firework_t::create_rocket()
{
    _rockets.emplace_back(particle_t(
        std::array<int32_t, 2>({_horizontal_pos(_gen), 0}),
        std::array<int32_t, 2>({_horizontal_dir(_gen), _vertical_dir(_gen)}), std::numeric_limits<int32_t>::max()));
}

void firework_t::operator()()
{
    {
        auto write = _rockets.begin();
        for (auto read = _rockets.begin(); read != _rockets.end(); ++read)
        {
            bool create_explosion;
            if ((*read)(create_explosion, _gravitation))
            {
                *write = *read;
                ++write;
            }
            if (create_explosion)
            {
                _explosions.emplace_back(100, read->_rocket._position, read->_rocket._direction, _gen() % 10000, _timestep, _gen);
            }
        }
        _rockets.erase(write, _rockets.end());
    }
    //str2.erase(std::remove_if(_rockets.begin(), _rockets.end(),[](unsigned char x){return std::isspace(x);}),
    auto write = _explosions.begin();
    for (auto read = _explosions.begin(); read != _explosions.end(); ++read)
    {
        if ((*read)(_gravitation))
        {
            *write = *read;
            ++write;
        }
    }
    _explosions.erase(write, _explosions.end());
}
