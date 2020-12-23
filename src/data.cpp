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

#include "data.h"

#include <string>
#include <vector>
#include <limits>
#include <algorithm>

namespace SERIALIZE{
std::ostream & write_value(std::ostream & out, player_t const & value)
{
    write_value(out, value.first_name);
    write_value(out, value.second_name);
    write_value(out, value.races);
    write_value(out, value.points);
    return out;
}

std::istream & read_value(std::istream & in, player_t & value)
{
    read_value(in, value.first_name);
    read_value(in, value.second_name);
    read_value(in, value.races);
    read_value(in, value.points);
    return in;
}

std::ostream & write_value(std::ostream & out, race_data_item const & value)
{
    write_value(out, value.belongs_to_player);
    write_value(out, value.round_times);
    return out;
}

std::istream & read_value(std::istream & in, race_data_item & value)
{
    read_value(in, value.belongs_to_player);
    read_value(in, value.round_times);
    return in;
}

/*std::ostream & write_value(std::ostream & out, race_item const & value)
{
    write_value(out, value.members);
    write_value(out, value.race_start_time);
    write_value(out, value.race_state);
    write_value(out, value.rounds);
    return out;
}

std::istream & read_value(std::istream & in, race_item & value)
{
    read_value(in, value.members);
    read_value(in, value.race_start_time);
    read_value(in, value.race_state);
    read_value(in, value.rounds);
    return in;
}*/
}

race_data_item::race_data_item(player_t &belong) : belongs_to_player(&belong){}
    
race_data_item::race_data_item() : belongs_to_player(nullptr){}
    
nanotime_t race_data_item::get_absolut_last_time() const{
    return round_times.empty() ? std::numeric_limits<nanotime_t>::max() : round_times.back();
}
    
nanotime_t race_data_item::get_relative_last_time() const{
    return round_times.size() < 2 ? std::numeric_limits<nanotime_t>::max() : (round_times.back() - round_times[round_times.size() - 2]);
}

void race_item::add_time (race_data_item & item, nanotime_t time){
    if (!has_finished(item))
        item.round_times.push_back(time);
}

void race_data_item::pop_time(){
    if (!round_times.empty())
        round_times.pop_back();
}
    
nanotime_t race_data_item::get_best_time() const{
    if (round_times.size()<2)
        return std::numeric_limits<nanotime_t>::max();
    nanotime_t best = std::numeric_limits<nanotime_t>::max();
    for (size_t i=1;i<round_times.size();i++){
        if (round_times[i]-round_times[i-1]<best)
            best = round_times[i]-round_times[i-1];
    }
    return best;
}
    
size_t race_item::get_round(race_data_item const & item) const{
    return std::min(item.round_times.size(), rounds);
}

nanotime_t race_data_item::get_round_time(size_t round) const
{
    return round_times[round] - round_times[round - 1];
}

size_t race_data_item::get_best_time_index() const{
    if (round_times.size()<2)
        return std::numeric_limits<size_t>::max();
    nanotime_t best = std::numeric_limits<nanotime_t>::max();
    size_t index = 0;
    for (size_t i=1;i<round_times.size();i++){
        if (round_times[i]-round_times[i-1]<best){
            index = i;
            best = round_times[i]-round_times[i-1];
        }
    }
    return index;
}

bool race_item::has_finished(race_data_item const & item) const{return item.round_times.size() > rounds;}

race_item::race_item(size_t rounds_): rounds(rounds_){}

size_t race_item::member_count() const{
    return members.size();
}

int race_item::get_place(race_data_item const & item) const{
    if (!(has_finished(item)))
        return -1;
    int place = 1;
    for (size_t i=0;i<member_count();i++){
        race_data_item const& tmp = members[i];
        if (&tmp == &item)
            continue;
        if (has_finished(tmp) && tmp.get_absolut_last_time()<item.get_absolut_last_time())
            ++place;
    }
    return place;
}

size_t race_item::get_first_player(size_t round) const
{
    nanotime_t min = std::numeric_limits<nanotime_t>::max();
    size_t player = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < member_count(); ++i)
    {
        race_data_item const & tmp = members[i];
        if (tmp.round_times.size() > round && tmp.round_times[round] < min)
        {
            min = tmp.round_times[round];
            player = i;
        }
    } 
    return player;
}

bool race_item::all_finished() const{return std::all_of(members.begin(), members.begin() + member_count(), [this](race_data_item const & member){return this->has_finished(member);});}

nanotime_t race_item::timediff_to_first(race_data_item const & item) const{
    size_t index = item.round_times.size() - 1;
    size_t player = get_first_player(index);
    if (player == std::numeric_limits<size_t>::max())
    {
        return std::numeric_limits<nanotime_t>::max();
    }
    return item.round_times[index] - members[player].round_times[index];
}

int race_item::get_current_place(race_data_item const & item) const{
    int place = 1;
    size_t itemround = item.round_times.size();
    for (race_data_item const& tmp : members){
        if (&tmp == &item)
            continue;
        size_t tmpround = tmp.round_times.size();
        if (tmpround > itemround || (tmpround == itemround && tmp.get_absolut_last_time() < item.get_absolut_last_time()))
            place++;
    }
    return place;
}
