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

#ifndef FIREWORK_H
#define FIREWORK_H

#include <array>
#include <cstdint>
#include <vector>
#include <numeric>
#include <deque>
#include <random>
#include <limits>
#include <algorithm>
#include <iostream>
#include <stdexcept>

class color_t
{
public:
    uint8_t _r, _g, _b;
    
    color_t(uint8_t r_, uint8_t g_, uint8_t b_);
    color_t(uint32_t col);
    uint8_t & operator [](size_t c)
    {
        switch (c)
        {
            case 0: return _r;
            case 1: return _g;
            case 2: return _b;
        }
        throw std::runtime_error("Unknown index");
    }

    uint8_t const & operator [](size_t c) const
    {
        switch (c)
        {
            case 0: return _r;
            case 1: return _g;
            case 2: return _b;
        }
        throw std::runtime_error("Unknown index");
    }

};

class static_particle_t
{
    public:    
    std::array<int32_t, 2> _position;
    uint32_t _livetime;
};

class particle_t
{
public:
    std::array<int32_t, 2> _position;
    std::array<int32_t, 2> _direction;
    uint32_t _livetime;
    
    particle_t(std::array<int32_t, 2> const & position_, std::array<int32_t, 2> const & direction_, uint32_t livetime_);
    
    bool operator()(int32_t gravitation);
};

class rocket_t
{
public:
    particle_t _rocket;
    std::deque<std::array<int32_t, 2> > _trail;
    
    rocket_t(particle_t const & rocket_) : _rocket(rocket_) {}
     
    bool operator()(bool & create_explosion, int32_t gravitation);
};

class explosion_t
{
public:
    std::vector<particle_t> _particles;
    color_t _color;
    
    explosion_t(size_t num_particles_, std::array<int32_t, 2> const & position_, std::array<int32_t, 2> const & direction_, uint32_t maxlen, uint32_t timestep_, std::mt19937_64 gen);
    
    bool operator()(int32_t gravitation);
    
};

class firework_t
{
public:
    std::vector<rocket_t> _rockets;
    std::vector<explosion_t> _explosions;
    std::mt19937_64 _gen;
    std::uniform_int_distribution<int32_t> _vertical_dir;
    std::uniform_int_distribution<int32_t> _horizontal_dir;
    std::uniform_int_distribution<int32_t> _horizontal_pos;
    size_t _height;
    uint32_t _timestep;
    uint32_t _gravitation;
    
    firework_t(size_t width_, size_t height_, uint32_t timestep_);

    void create_rocket();
    void operator()();
};

#endif
