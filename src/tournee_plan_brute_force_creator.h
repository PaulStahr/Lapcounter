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

#include "./tournee_plan_creator.h"

#ifndef TOURNEE_PLAN_BRUTE_FORCE_CREATOR
#define TOURNEE_PLAN_BRUTE_FORCE_CREATOR

class brute_force_tournee_plan_creator: public tournee_plan_creator{
	public:
	brute_force_tournee_plan_creator(size_t pl, size_t sl, size_t ro, size_t eq_sl);

	void create_plan();
	private:
	uint8_t** round_slot_to_player;
	uint8_t* player_slot_to_racecount;
    uint8_t* player_player_to_racecount;
	uint8_t* possible_rounds;
	uint8_t* played_against_max_times;
	uint8_t* player_to_racecount;

	size_t max_deth;
	size_t min_deth;
	uint64_t checked_plans;

	uint8_t** calculate_plan(size_t deth, size_t round_index);
};

#endif
