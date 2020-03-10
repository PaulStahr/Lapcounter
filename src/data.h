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

#include <string>
#include <vector>
#include "serialize.h"
#include "query_performance_counter.h"


#ifndef DATA_H
#define DATA_H
class player{
    public:
    std::string first_name;
    std::string second_name;
    unsigned int races;
    unsigned int points;
    private:
};

class race_data_item{
    public:
    race_data_item();
    race_data_item(player &belong);
    std::vector<nanotime_t> round_times;
    player *belongs_to_player;
    
    nanotime_t get_absolut_last_time() const;
    nanotime_t get_relative_last_time() const;
    void pop_time();
    nanotime_t get_best_time() const;
    nanotime_t get_round_time(size_t round) const;
    size_t get_best_time_index() const;
    
    private:
};

class race_item;
namespace SERIALIZE
{
template <>
std::ostream & write_value(std::ostream & out, race_item const & value);
template <>
std::istream & read_value(std::istream & in, race_item & value);
}

class race_item{
    public:
    race_item(size_t rounds);
    std::vector<race_data_item> members;
    nanotime_t race_start_time;
    size_t member_count() const;
    
    int get_place(race_data_item const & item) const;
    int get_current_place(race_data_item const & item) const;
    void add_time (race_data_item & item, nanotime_t time);

    size_t get_round(race_data_item const &) const;
    bool has_finished(race_data_item const &) const;
    nanotime_t timediff_to_first(race_data_item const & item) const;
    size_t get_first_player(size_t round) const;
    bool all_finished() const;
//friend std::ostream & (write_value)(std::ostream & out, race_item const & value);
//friend std::istream & (read_value)(std::istream & in, race_item & value);

    size_t rounds;
    private:

};

namespace SERIALIZE
{
template <>
std::ostream & write_value(std::ostream & out, player const & value);
template <>
std::istream & read_value(std::istream & in, player & value);
template <>
std::ostream & write_value(std::ostream & out, race_data_item const & value);
template <>
std::istream & read_value(std::istream & in, race_data_item & value);
    
}


#endif
