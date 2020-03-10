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

#include "tournee_plan.h"

tournee_plan::tournee_plan(size_t pl, size_t sl, size_t ro, size_t eq_sl, int* rt) : race_table(rt, rt + ro * sl), player (pl), slots(sl), rounds(ro), equal_slots(eq_sl)
{
}

tournee_plan::tournee_plan(size_t pl, size_t sl, size_t ro, size_t eq_sl) : race_table(ro * sl), player (pl), slots(sl), rounds(ro), equal_slots(eq_sl)
{}


tournee_plan::tournee_plan(size_t pl, size_t sl, size_t ro, size_t eq_sl, int** rt) : race_table(ro * sl), player (pl), slots(sl), rounds(ro), equal_slots(eq_sl)
{
    for (size_t round = 0; round < ro; ++round)
    {
        std::copy(rt[round], rt[round] + slots, race_table.begin() + round * slots);
    }
}

void tournee_plan::print(){
    for (size_t i=0;i<rounds;i++){
        std::cout<<"Runde "<<i<<':';
        for (size_t j=0;j<slots;j++){
            std::cout<<race_table[i * slots + j]<<' ';
        }
        std::cout<<std::endl;
    }
}

void tournee_plan::print_html(std::ostream &out){
    out<<"<html>"<<std::endl;
    out<<"Spieler:"<<get_player()<<"<br />"<<std::endl;
    out<<"Slots:"<<get_slots()<<"<br />"<<std::endl;
    out<<"Rennen:"<<get_rounds()<<"<br />"<<std::endl;
    out<<"Gleichschwere Bahnen:"<<get_equal_slots()<<"<br />"<<std::endl;
    out<<"<table border = \"1\">"<<std::endl;
    for (size_t i=0;i<rounds;i++){
        out<<"<tr>"<<std::endl;
        out<<"<td>"<<std::endl;
        out<<"Runde "<<i;
        out<<"</td>"<<std::endl;
        for (size_t j=0;j<slots;j++){
            out<<"<td>"<<std::endl;
            out<<race_table[i * slots + j];
            out<<"</td>"<<std::endl;
        }
        out<<"</tr>"<<std::endl;
    }
    out<<"</table>"<<std::endl;
    out<<"</html>"<<std::endl;
}

size_t tournee_plan::get_player(){
    return player;
}

size_t tournee_plan::get_slots(){
    return slots;
}

size_t tournee_plan::get_rounds(){
    return rounds;
}

size_t tournee_plan::get_equal_slots(){
    return equal_slots;
}

size_t tournee_plan::get_player_at(size_t round, size_t slot){
    return race_table[round * slots + slot];	
}
