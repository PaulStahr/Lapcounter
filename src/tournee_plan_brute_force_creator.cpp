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
#include "./tournee_plan_brute_force_creator.h"

brute_force_tournee_plan_creator::brute_force_tournee_plan_creator(size_t pl, size_t sl, size_t ro, size_t eq_sl): tournee_plan_creator(pl, sl, ro, eq_sl){}

void brute_force_tournee_plan_creator::create_plan(){
    if (player < slots)
        return;
    
    max_deth = 0;
    min_deth = rounds;
    checked_plans=0;

    player_on_slot = new uint8_t*[rounds];
        
    plays_on_slot = new int[player * player];
    std::fill(plays_on_slot, plays_on_slot+player * player, 0);
        
    plays_against = new int[player * player];
    std::fill(plays_against, plays_against + player * player, 0);
    
    plays_total_times = new int[player];
    std::fill(plays_total_times, plays_total_times + player, 0);

    played_against_max_times = new int[player];
    std::fill(played_against_max_times, played_against_max_times + player, 0);

    for (size_t i=0;i<slots;i++){
        plays_on_slot[i * player + slots-i-1]=1;
        plays_total_times[i]++;
        for (size_t j=0;j<i;j++){
            plays_against[i* player + j] = 1;
            if (play_against_max == 1){
                played_against_max_times[i]++;
                played_against_max_times[j]++;
            }
        }
    }

    player_on_slot[0] = possible_rounds;

    srand(time(NULL));
    uint8_t** race_table = calculate_plan(1, 0);

    delete possible_rounds;

    delete player_on_slot;
    
    delete[] plays_on_slot;
    delete[] plays_against;
    copy_plan(race_table);
}

uint8_t** brute_force_tournee_plan_creator::calculate_plan(size_t deth, size_t round_index){
    if ((++checked_plans)%30000==0){
        std::cout<<"checked_plans:"<<checked_plans<<" searched from deth:"<<min_deth<<" to:"<<max_deth<<std::endl;
        max_deth = 0;
        min_deth = rounds;
    }
    if (deth > max_deth)
        max_deth = deth;
    if (deth < min_deth)
        min_deth = deth;
    
    uint8_t** race_table_local = NULL;
                
    for(;round_index < possible_round_count;round_index++){
        uint8_t* player_on_slot_round = possible_rounds + round_index * slots;
        player_on_slot[deth] = player_on_slot_round;
        bool incorrect = false;

        for (size_t i=0;i<slots;i++){
            if ((++plays_on_slot[player_on_slot_round[i] * player + i])>max_play_on_slot)
                incorrect = true;
            if ((++plays_total_times[player_on_slot_round[i]])>max_play)
                incorrect = true;
        }
        
        if (!incorrect){
            //Analyse Wer spielt gegen Wen + Korrektheitspruefung Spielen 2 zu oft gegeneinander
            for (size_t i=1;i<slots;i++){
                int a = player_on_slot_round[i];
                for (size_t j=0;j<i;j++){
                    int b = player_on_slot_round[j];
                    int times = a>b ? ++plays_against[a* player + b] : ++plays_against[b * player + a];
                    if (times > play_against_max){
                        incorrect = true;
                    }else if (times == play_against_max){
                        if ((++played_against_max_times[a])>play_against_max_count)
                            incorrect = true;
                        if ((++played_against_max_times[b])>play_against_max_count)
                            incorrect = true;
                    }
                }
            }
            if (!incorrect){
                //Keinen Fehler gefunden
                if (deth + 1 == rounds){
                    //Korrekter Plan
                    std::cout<<"Found Correct Plan"<<std::endl;

                    race_table_local = new uint8_t*[rounds];
                    for (size_t i=0;i<rounds;i++){
                        race_table_local[i] = new uint8_t[slots];
                        std::copy(player_on_slot[i], player_on_slot[i] + slots, race_table_local[i]);
                    }
                    goto cleanup;
                }else{
                    //Rekursiber selbstaufruf, wenn alles okay, hier ist eventuell das Vergleichen von Plaenen moeglich
                    if (race_table_local == NULL)
                        race_table_local = calculate_plan(deth+1, round_index);
                    if (race_table_local != NULL)
                        goto cleanup;
                }
            }

            for (size_t i=1;i<slots;i++){
                int a = player_on_slot_round[i];
                for (size_t j=0;j<i;j++){
                    int b = player_on_slot_round[j];
                    int times = a > b ? plays_against[a * player + b]-- : plays_against[b * player + a]--;
                    if (times == play_against_max){
                        played_against_max_times[a]--;
                        played_against_max_times[b]--;
                    }
                }
            }
        }

        for (size_t i=0;i<slots;i++){
            int posr = player_on_slot_round[i];
            plays_on_slot[posr * player + i]--;
            plays_total_times[posr]--;
        }
    }
    return race_table_local;
    cleanup:
    for (size_t i=0;i<slots;i++){
        plays_on_slot[player_on_slot[deth][i] * player + i]--;
        plays_total_times[player_on_slot[deth][i]]--;
    }

    return race_table_local;
}
