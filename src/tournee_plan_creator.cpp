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
#include "tournee_plan_creator.h"
#include <boost/thread.hpp>

tournee_plan_creator::tournee_plan_creator(size_t pl, size_t sl, size_t ro, size_t equal_slots){
    equal_hard_slots = equal_slots;
    rounds = ro;
    player = pl;
    slots = sl;
    possible_round_count = 1;
    for (size_t i=0;i<slots;i++)
        possible_round_count *= (player - i);
    max_play = (rounds * slots + player-1) / player;
    max_play_on_slot = (max_play * equal_hard_slots + slots - 1) / slots;
    play_against_total = slots * rounds * (slots-1) / player;
    play_against_min = (rounds * slots * (slots-1)) / (player * (player - 1));
    play_against_max = (rounds * slots * (slots-1) + (player * (player - 1)) - 1) / (player * (player - 1));
    play_against_max_count = play_against_total - play_against_min * max_play;

    possible_rounds = new uint8_t[possible_round_count * slots];
    
    for (size_t i=0;i<slots;i++)
        possible_rounds[i]=slots-i-1;
    
    //Erstelle alle moeglichen Runden
    for (size_t i=1;i<possible_round_count;i++){
        uint8_t* current_round = possible_rounds + i * slots;
        std::copy (possible_rounds + (i-1) * slots, possible_rounds + i * slots, current_round);
        add_one:
        current_round[0]++;
        for (size_t j=1;j<slots && current_round[j-1]>=static_cast<int>(player);j++){
            current_round[j-1] = 0;
            current_round[j] += 1;
        }
        //Test for correctness and go back at error
        for (size_t j=1;j<slots;j++)
            for (size_t k=0;k<j;k++)
                if (current_round[j] == current_round[k])
                    goto add_one;
    }
    plan = new tournee_plan(player, slots, rounds, equal_hard_slots);

    running = false;
    solved = false;
    std::cout<<"play_against_max:"<<play_against_max<<" play_against_max_count:"<<play_against_max_count<<" plays_max_on_slot:"<<max_play_on_slot<<std::endl;
}

//virtual void create_plan(){}

void tournee_plan_creator::print_plan(int** race_table){
    for (size_t i=0;i<rounds;i++){
        std::cout<<"Runde "<<i<<':';
        for (size_t j=0;j<slots;j++)
            std::cout<<race_table[i][j]<<' ';
        std::cout<<std::endl;
    }
}

tournee_plan_creator::~tournee_plan_creator() {
    delete[] possible_rounds;
}

void tournee_plan_creator::start_creation(){
    stop = false;
    boost::thread thrd(function, this);
}

void tournee_plan_creator::stop_creation(){
    stop = true;
}

tournee_plan* tournee_plan_creator::get_plan(){
    return plan;
}

bool tournee_plan_creator::is_running(){
    return running;
}

bool tournee_plan_creator::is_solved(){
    return solved;
}

bool tournee_plan_creator::should_stop(){
    return stop;
}

void tournee_plan_creator::copy_plan(uint8_t** pl){
    for (size_t i=0;i<rounds;i++)
        std::copy (pl[i], pl[i] + slots, plan->race_table.begin() + i * slots);
}
