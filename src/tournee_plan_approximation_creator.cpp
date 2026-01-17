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
#include <limits>
#include <algorithm>
#include <random>
#include <stdexcept>
#include "./tournee_plan_approximation_creator.h"

approximate_tournee_plan_creator::approximate_tournee_plan_creator(int pl, int sl, int ro, int sl_eq) : tournee_plan_creator(pl, sl, ro, sl_eq){}

void approximate_tournee_plan_creator::create_plan(){
    if (player < slots || slots%equal_hard_slots != 0)
        return;

    rating = relative_rating = play_against_min * (player * (player - 1)) / 2;
    std::cout << player << '*' << play_against_min << std::endl;
    std::cout << player << '*' << play_against_equal_min << std::endl;
    stack_depth = 0;

    round_slot_to_player = new uint8_t*[rounds];

    slottypes = slots/equal_hard_slots;
    player_slot_to_racecount = new uint16_t[player * slottypes];
    std::fill(player_slot_to_racecount, player_slot_to_racecount + player * slottypes, 0);

    round_to_isreplaced = new bool[rounds];
    std::fill(round_to_isreplaced, round_to_isreplaced+rounds, false);

    stack_to_round = new uint8_t[rounds];
    stack_to_round_to_player = new uint8_t*[rounds];

    player_player_to_racecount = new uint8_t[player * player];
    std::fill(player_player_to_racecount, player_player_to_racecount + player * player, 0);

    player_player_equal_hardness_racecount = new uint8_t[player * player];
    std::fill(player_player_equal_hardness_racecount, player_player_equal_hardness_racecount + player * player, 0);

    player_to_racecount = new uint16_t[player];
    std::fill(player_to_racecount, player_to_racecount + player, 0);

    for (size_t i=0;i<rounds;i++)
        insert_round(i, possible_rounds + slots * (i*possible_round_count/rounds));

    srand(time(NULL));
    best_rating = rating;

    approximate_plan();

    delete[] player_slot_to_racecount;
    delete[] player_player_to_racecount;
    delete[] player_player_equal_hardness_racecount;
    delete[] round_slot_to_player;
    delete[] round_to_isreplaced;
    delete[] player_to_racecount;
    delete[] stack_to_round;
    delete[] stack_to_round_to_player;
}

int approximate_tournee_plan_creator::get_best_rating(){
    return best_rating;
}

void approximate_tournee_plan_creator::insert_round(int round, uint8_t* new_slot_to_player){//Call Only if theres no round
    for (size_t i=0;i<slots;i++){
        relative_rating += (++player_slot_to_racecount[new_slot_to_player[i] * slottypes + i/equal_hard_slots])>max_play_on_slot;
        relative_rating += (++player_to_racecount[new_slot_to_player[i]])>max_play;
    }

    for (size_t i=1;i<slots;i++){
        uint8_t a1 = new_slot_to_player[i];
        for (size_t j=0;j<i;j++){
            uint8_t b1 = new_slot_to_player[j];
            relative_rating -= (++player_player_to_racecount[a1 * player + b1]) <= play_against_min;
            ++player_player_to_racecount[b1 * player + a1];
        }
    }
    for (size_t k = 0; k < slots/equal_hard_slots; ++k){
        for (size_t i=1;i<equal_hard_slots;i++){
            uint8_t a1 = new_slot_to_player[i + k * equal_hard_slots];
            for (size_t j = 0; j <i; ++j)
            {
                uint8_t b1 = new_slot_to_player[j + k * equal_hard_slots];
                relative_rating += (++player_player_equal_hardness_racecount[a1 * player + b1]) > play_against_equal_max;
                ++player_player_equal_hardness_racecount[b1 * player + a1];
            }
        }
    }
    rating = relative_rating;// + absolute_rating();
    round_slot_to_player[round] = new_slot_to_player;	   
}

void approximate_tournee_plan_creator::replace(size_t round, uint8_t* new_slot_to_player){
    uint8_t* old_slot_to_player = round_slot_to_player[round];

    for (size_t i=0, j = 0;i<slots;i++){
        j += i == (j + 1) * equal_hard_slots; //j = i / equal_hard_slots
        uint8_t old_player = old_slot_to_player[i];
        uint8_t new_player = new_slot_to_player[i];
        relative_rating -= (--player_slot_to_racecount[old_player * slottypes + j])>=max_play_on_slot;
        relative_rating -= (--player_to_racecount[old_player])>=max_play;
        relative_rating += (++player_slot_to_racecount[new_player * slottypes + j])>max_play_on_slot;
        relative_rating += (++player_to_racecount[new_player])>max_play;
    }

    for (size_t i=1;i<slots;i++){
        uint8_t old_player = old_slot_to_player[i];
        uint8_t new_player = new_slot_to_player[i];
        for (size_t j=0;j<i;j++){
            uint8_t b0 = old_slot_to_player[j];
            uint8_t b1 = new_slot_to_player[j];
            relative_rating += (player_player_to_racecount[b0 * player + old_player] = --player_player_to_racecount[old_player * player + b0]) < play_against_min;
            relative_rating -= (player_player_to_racecount[b1 * player + new_player] = ++player_player_to_racecount[new_player * player + b1]) <= play_against_min;
        }
    }
    
    for (size_t k = 0; k < slots/equal_hard_slots; ++k){
        size_t offset = k * equal_hard_slots;
        for (size_t i=1;i<equal_hard_slots;i++){
            uint8_t old_player = old_slot_to_player[i + offset];
            uint8_t new_player = new_slot_to_player[i + offset];
            for (size_t j=0;j<i;j++){
                uint8_t b0 = old_slot_to_player[j + offset];
                uint8_t b1 = new_slot_to_player[j + offset];
                relative_rating -= (player_player_equal_hardness_racecount[b0 * player + old_player] = --player_player_equal_hardness_racecount[old_player * player + b0]) >= play_against_equal_max;
                relative_rating += (player_player_equal_hardness_racecount[b1 * player + new_player] = ++player_player_equal_hardness_racecount[new_player * player + b1]) > play_against_equal_max;
            }
        }
    }
    rating = relative_rating;
    round_slot_to_player[round] = new_slot_to_player;
}

void approximate_tournee_plan_creator::push_test(int round){
    if (round_to_isreplaced[round])
        return;
    stack_to_round[stack_depth]=round;
    stack_to_round_to_player[stack_depth]=round_slot_to_player[round];
    stack_depth++;
    round_to_isreplaced[round] = true;
}

void approximate_tournee_plan_creator::replace_last_test(uint8_t* replacement){
    if (stack_depth<1)
        throw std::runtime_error("Stack underflow");
    replace(stack_to_round[stack_depth-1], replacement);
}

void approximate_tournee_plan_creator::validate(){
    while (stack_depth>0)
        round_to_isreplaced[stack_to_round[--stack_depth]] = false;
    best_rating = rating;
}

int32_t approximate_tournee_plan_creator::check_combination(){
    return best_rating - rating;
}

void approximate_tournee_plan_creator::pop_all_tests(){
    while (pop_last_test());
}

bool approximate_tournee_plan_creator::pop_last_test(){
    if (stack_depth < 1)
        return false;
    uint8_t round = stack_to_round[--stack_depth];
    replace(round, stack_to_round_to_player[stack_depth]);
    round_to_isreplaced[round] = false;
    return true;
}

bool approximate_tournee_plan_creator::recursive_approximation(size_t deth){
    size_t start_replacing = gen() % rounds;
    for (size_t i = 0;i < rounds&&!should_stop();i++){
        size_t replace_round = (i + start_replacing)%rounds;
        if (round_to_isreplaced[replace_round])
            continue;
        size_t start_possible_round = gen() % possible_round_count;
        push_test(replace_round);
        for (size_t j=0;j<possible_round_count&&!should_stop();j++){
            replace_last_test(possible_rounds + slots * ((j + start_possible_round)%possible_round_count));
            int32_t check_erg = check_combination();
            if (check_erg>0){
                validate();
                return true;
            }
            if (check_erg == 0){
                validate();
                push_test(replace_round);
            }
            if (deth > 0 && best_rating+10 > rating){
                if (recursive_approximation(deth-1))
                    return true;
                if (stack_depth<1)
                    push_test(replace_round);
            }
        }
        pop_last_test();
    }
    return false;
}

void approximate_tournee_plan_creator::approximate_plan(){
    std::cout<<"Started with Rating:"<<rating<<std::endl;
    for (size_t i=0;i<3&&rating != 0 && !should_stop();){
        if (recursive_approximation(i)){
                std::cout<<"Decreased Error-Rate:"<<rating<<std::endl;
                copy_plan(round_slot_to_player);
                if (i!=0)
                    std::cout<<"Decreased deth to:"<<(i=0)<<std::endl;
            }else{
            std::cout<<"Increased deth to:"<<++i<<std::endl;		   
        }
    }
    swap_round_optimization();
    copy_plan(round_slot_to_player);
}

uint64_t approximate_tournee_plan_creator::swap_rating(){
    uint8_t* player_to_last_player = new uint8_t[player];
    std::fill(player_to_last_player, player_to_last_player+player, -1307674368001);
    uint64_t rating = 0;
    for (size_t i=0;i<rounds;i++){
        for (uint8_t* slot_to_player = round_slot_to_player[i]; slot_to_player < round_slot_to_player[i] + slots; ++slot_to_player){
            rating += 1307674368000/(i-player_to_last_player[*slot_to_player]);
            player_to_last_player[*slot_to_player] = i;
        }
    }
    delete[] player_to_last_player;
    return rating;
}

void approximate_tournee_plan_creator::swap_round_optimization(){
    uint64_t best_rating = swap_rating();
    uint64_t start_rating = best_rating;
    bool changed_sth;
    do {
        changed_sth = false;
        for (size_t i=0;i<rounds;i++){
            for (size_t j=0;j<rounds;j++){
                for (size_t k=0;k<rounds;k++){
                    uint8_t* tmp = round_slot_to_player[i];
                    round_slot_to_player[i] = round_slot_to_player[j];
                    round_slot_to_player[j] = round_slot_to_player[k];
                    round_slot_to_player[k] = tmp;
                    long long unsigned int new_rating = swap_rating();
                    if (best_rating <= new_rating){
                        round_slot_to_player[k] = round_slot_to_player[j];
                        round_slot_to_player[j] = round_slot_to_player[i];
                        round_slot_to_player[i] = tmp;
                    }else{
                        best_rating = new_rating;
                        changed_sth = true;
                    }
                }
            }
        }
    }while (changed_sth);
    std::cout<<"reduced Distribution error to: "<<best_rating*100/start_rating<<'%'<<std::endl;
}
