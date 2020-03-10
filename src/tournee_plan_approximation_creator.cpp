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
#include "./tournee_plan_approximation_creator.h"

approximate_tournee_plan_creator::approximate_tournee_plan_creator(int pl, int sl, int ro, int sl_eq) : tournee_plan_creator(pl, sl, ro, sl_eq){}

void approximate_tournee_plan_creator::create_plan(){
    if (player < slots || slots%equal_hard_slots != 0)
        return;

    rating = relative_rating = play_against_min * (player * (player - 1)) / 2;
    std::cout << player << '*' << play_against_min << std::endl;
    stack_deth = 0;

    player_on_slot = new uint8_t*[rounds];

    slottypes = slots/equal_hard_slots;
    plays_on_slot = new uint16_t[player * slottypes];
    std::fill(plays_on_slot, plays_on_slot + player * slottypes, 0);

    already_replaced = new bool[rounds];
    std::fill(already_replaced, already_replaced+rounds, false);

    replaced_round = new uint8_t[rounds];
    replaced_round_with = new uint8_t*[rounds];

    plays_against = new uint8_t[player * player];
    std::fill(plays_against, plays_against + player * player, 0);

    plays_total_times = new uint16_t[player];
    std::fill(plays_total_times, plays_total_times + player, 0);

    for (size_t i=0;i<rounds;i++)
        insert_round(i, possible_rounds + slots * (i*possible_round_count/rounds));

    srand(time(NULL));
    best_rating = rating;

    approximate_plan();

    delete[] plays_on_slot;
    delete[] plays_against;
    delete[] player_on_slot;
    delete[] already_replaced;
    delete[] plays_total_times;
    delete[] replaced_round;
    delete[] replaced_round_with;
}

int approximate_tournee_plan_creator::get_best_rating(){
    return best_rating;
}

void approximate_tournee_plan_creator::insert_round(int round, uint8_t* new_round){//Call Only if theres no round
    for (size_t i=0;i<slots;i++){
        relative_rating += (++plays_on_slot[new_round[i] * slottypes + i/equal_hard_slots])>max_play_on_slot;
        relative_rating += (++plays_total_times[new_round[i]])>max_play;
    }

    for (size_t i=1;i<slots;i++){
        uint8_t a1 = new_round[i];
        for (size_t j=0;j<i;j++){
            uint8_t b1 = new_round[j];
            relative_rating -= (++plays_against[a1 * player + b1]) <= play_against_min;
            ++plays_against[b1 * player + a1];
            /*if (a1>b1){
                relative_rating -= (++plays_against[a1 * player + b1]) <= play_against_min;
            }else{
                relative_rating -= (++plays_against[b1 * player + a1]) <= play_against_min;
            }*/
        }
    }
    rating = relative_rating;// + absolute_rating();

    player_on_slot[round] = new_round;	   
}

/*uint64_t approximate_tournee_plan_creator::absolute_rating(){
    uint64_t rating = 0;
    for (size_t i=1;i<player;i++){
        uint8_t* plays_against_player = plays_against + i * player;
        for (size_t j=0;j<i;j++)
            if (plays_against_player[j]<play_against_min)
                    rating += play_against_min - plays_against_player[j];
    }
    return rating;
}*/

void approximate_tournee_plan_creator::replace(size_t round, uint8_t* new_round){
    uint8_t* old_round = player_on_slot[round];

    for (size_t i=0, j = 0;i<slots;i++){
        j += i == (j + 1) * equal_hard_slots; //j = i / equal_hard_slots
        uint8_t a0 = old_round[i];
        uint8_t a1 = new_round[i];
        relative_rating -= (--plays_on_slot[a0 * slottypes + j])>=max_play_on_slot;
        relative_rating -= (--plays_total_times[a0])>=max_play;
        relative_rating += (++plays_on_slot[a1 * slottypes + j])>max_play_on_slot;
        relative_rating += (++plays_total_times[a1])>max_play;
    }

    for (size_t i=1;i<slots;i++){
        uint8_t a0 = old_round[i];
        uint8_t a1 = new_round[i];
        for (size_t j=0;j<i;j++){
            uint8_t b0 = old_round[j];
            uint8_t b1 = new_round[j];
            relative_rating += (plays_against[b0 * player + a0] = --plays_against[a0 * player + b0]) < play_against_min;
            relative_rating -= (plays_against[b1 * player + a1] = ++plays_against[a1 * player + b1]) <= play_against_min;
            /*if (a0>b0)
                relative_rating += (--plays_against[a0 * player + b0] < play_against_min);
            else
                relative_rating += (--plays_against[b0 * player + a0] < play_against_min);
            if (a1>b1){
                relative_rating -= (++plays_against[a1 * player + b1]) <= play_against_min;
            }else{
                relative_rating -= (++plays_against[b1 * player + a1]) <= play_against_min;
            }*/
        }
    }
    rating = relative_rating;// + absolute_rating();
    player_on_slot[round] = new_round;
}

void approximate_tournee_plan_creator::push_test(int round){
    if (already_replaced[round])
        return;
    replaced_round[stack_deth]=round;
    replaced_round_with[stack_deth]=player_on_slot[round];
    stack_deth++;
    already_replaced[round] = true;
}

void approximate_tournee_plan_creator::replace_last_test(uint8_t* replacement){
    if (stack_deth<1)
        std::cout<<"error 1"<<std::endl;
    replace(replaced_round[stack_deth-1], replacement);
}

void approximate_tournee_plan_creator::validate(){
    while (stack_deth>0)
        already_replaced[replaced_round[--stack_deth]] = false;
    best_rating = rating;
}

int32_t approximate_tournee_plan_creator::check_combination(){
    return best_rating - rating;
}

void approximate_tournee_plan_creator::pop_all_tests(){
    while (pop_last_test());
}

bool approximate_tournee_plan_creator::pop_last_test(){
    if (stack_deth < 1)
        return false;
    uint8_t round = replaced_round[--stack_deth];
    replace(round, replaced_round_with[stack_deth]);
    already_replaced[round] = false;
    return true;
}

bool approximate_tournee_plan_creator::recursive_approximation(size_t deth){
    size_t start_replacing = gen() % rounds;
    for (size_t i = 0;i < rounds&&!should_stop();i++){
        size_t replace_round = (i + start_replacing)%rounds;
        if (already_replaced[replace_round])
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
                if (stack_deth<1)
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
                copy_plan(player_on_slot);
                if (i!=0)
                    std::cout<<"Decreased deth to:"<<(i=0)<<std::endl;
            }else{
            std::cout<<"Increased deth to:"<<++i<<std::endl;		   
        }
    }
    swap_round_optimization();
    copy_plan(player_on_slot);
}

uint64_t approximate_tournee_plan_creator::swap_rating(){
    uint8_t* last_played = new uint8_t[player];
    std::fill(last_played, last_played+player, -1307674368001);
    uint64_t rating = 0;
    for (size_t i=0;i<rounds;i++){
        uint8_t* current_round = player_on_slot[i];
        for (size_t j=0;j<slots;j++){
            uint8_t player = current_round[j];
            rating += 1307674368000/(i-last_played[player]);
            last_played[player] = i;
        }
    }
    delete[] last_played;
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
                    uint8_t* tmp = player_on_slot[i];
                    player_on_slot[i] = player_on_slot[j];
                    player_on_slot[j] = player_on_slot[k];
                    player_on_slot[k] = tmp;
                    long long unsigned int new_rating = swap_rating();
                    if (best_rating <= new_rating){
                        player_on_slot[k] = player_on_slot[j];
                        player_on_slot[j] = player_on_slot[i];
                        player_on_slot[i] = tmp;
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
