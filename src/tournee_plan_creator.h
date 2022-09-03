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

#include "./tournee_plan.h"

#ifndef tournee_plan_creator_h
#define tournee_plan_creator_h

class tournee_plan_creator{
	public:
        tournee_plan_creator(size_t pl, size_t sl, size_t ro, size_t equal_slots);
	//virtual void create_plan(){}
	void print_plan(int** race_table);
	~tournee_plan_creator();
	virtual void create_plan() =0;
	virtual int get_best_rating() =0;
	void start_creation();
	void stop_creation();
	tournee_plan* get_plan();

	bool is_running();
	bool is_solved();
	protected:
	size_t equal_hard_slots;
	size_t rounds;
	size_t player;
	size_t slots;
	int play_against_max;
	int play_against_min;
    int play_against_equal_min;
    int play_against_equal_max;
	int play_against_max_count;
	int max_play_on_slot;
	int max_play;
	int play_against_total;
	size_t possible_round_count;
	uint8_t* possible_rounds;

	bool should_stop();
	void copy_plan(uint8_t** plan);
	private:
	bool stop;
	bool running;
	bool solved;
	tournee_plan* plan;

	static void function(tournee_plan_creator* creator)
    {
        if (creator->running)
        return;
        creator->running = true;
        creator->create_plan();
        creator->running = false;
        creator->solved = true;
    }
};
#endif
