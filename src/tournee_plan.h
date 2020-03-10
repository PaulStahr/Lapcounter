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

#ifndef tournee_plan_h
#define tournee_plan_h
#include <iostream>
#include <vector>

class tournee_plan{
	public:

	tournee_plan(size_t pl, size_t sl, size_t ro, size_t eq_sl, int* rt);
    tournee_plan(size_t pl, size_t sl, size_t ro, size_t eq_sl);
	tournee_plan(size_t pl, size_t sl, size_t ro, size_t eq_sl, int** rt);
	void print();
	void print_html(std::ostream &out);
	size_t get_player();
	size_t get_slots();
	size_t get_rounds();
	size_t get_equal_slots();
	size_t get_player_at(size_t round, size_t slot);
	std::vector<size_t> race_table;
	private:
	size_t player;
	size_t slots;
	size_t rounds;
	size_t equal_slots;
};
#endif
