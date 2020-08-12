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

#include <random>   
#include "tournee_plan_creator.h"

class approximate_tournee_plan_creator: public tournee_plan_creator{
	public:
	approximate_tournee_plan_creator(int pl, int sl, int ro, int sl_eq);

	void create_plan();

	int get_best_rating();

	private:
	uint8_t** round_slot_to_player;
	uint16_t* player_slot_to_racecount;
	uint8_t* player_player_to_racecount;
	uint16_t* player_to_racecount;
	bool* round_to_isreplaced;

	uint8_t* round_to_replaced;
	uint8_t** round_to_replaced_with;
	size_t stack_deth;
    size_t slottypes;
 
	int rating, best_rating; //lower are better
	int relative_rating;
    std::mt19937 gen;

	void insert_round(int round, uint8_t* new_round);

	void replace(size_t round, uint8_t* new_round);
	void push_test(int round);
	void replace_last_test(uint8_t* replacement);

	void validate();
	int32_t check_combination();
	void pop_all_tests();
	bool pop_last_test();
	bool recursive_approximation(size_t deth);
	void approximate_plan();
	uint64_t swap_rating();
	void swap_round_optimization();
};
